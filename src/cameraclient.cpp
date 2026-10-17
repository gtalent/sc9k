/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QHostInfo>

#include "cameraclient.hpp"
#include "consts.hpp"
#include "settingsdata.hpp"

constexpr auto PtzTickMs = 150;
// how long to wait for the starting position before moving everything at once
constexpr auto LocateTimeoutMs = 1500;
constexpr auto StepTimeoutMs = 20000;
// camera may not have started moving yet, so don't treat it as stopped until this has passed
constexpr auto MinSettleMs = 600;
// position unchanged for this long means the camera has stopped
constexpr auto StableMs = 450;
// camera only updates its reported zoom every ~40 ms, so measure velocity over a longer window
constexpr auto ZoomVelocityWindowMs = 200;
// stop the zoom early by how far it travels in this time,
// covers the reported position being stale and the lens coasting after Zoom Stop
constexpr auto ZoomStopLeadMs = 150;
// give a Zoom Direct time to start moving before accepting the position as final
constexpr auto ZoomDirectStartMs = 250;
// below this, rounding makes the Zoom Direct scale too imprecise to learn from
constexpr auto MinZoomCalibrationVal = 1000;
constexpr auto PanTiltTolerance = 4;
constexpr auto ZoomTolerance = 16;
constexpr auto PtzQueryTimeoutMs = 2000;

[[nodiscard]]
static constexpr int readNibbles(std::span<uint8_t const> const src) {
	int out = 0;
	for (auto const n : src) {
		out = (out << 4) | (n & 0x0F);
	}
	return out;
}

[[nodiscard]]
static constexpr int signExtend(int const val, size_t const bits) {
	auto const signBit = 1 << (bits - 1);
	return (val & signBit) ? val - (1 << bits) : val;
}

template<size_t Sz>
static constexpr size_t writeNibbles(std::array<uint8_t, Sz> &dst, size_t i, int const val, size_t const count) {
	auto const u = static_cast<uint32_t>(val);
	for (auto n = count; n > 0; --n) {
		dst[i++] = static_cast<uint8_t>((u >> ((n - 1) * 4)) & 0x0F);
	}
	return i;
}

CameraClient::CameraClient(QObject *parent): QObject(parent), m_socket(new QUdpSocket(this)) {
	// some cameras send replies to the VISCA port rather than the sender's port,
	// so listen on it if possible to receive poll responses
	if (!m_socket->bind(QHostAddress::AnyIPv4, 52381)) {
		m_socket->bind(QHostAddress::AnyIPv4, 0);
	}
	connect(m_socket, &QUdpSocket::readyRead, this, &CameraClient::onReadyRead);
	connect(m_socket, &QUdpSocket::errorOccurred, this, &CameraClient::onSocketError);
	reloadConnectionSettings();
	poll();
	m_pollTimer.start(1000);
	connect(&m_pollTimer, &QTimer::timeout, this, &CameraClient::poll);
	connect(&m_ptzTimer, &QTimer::timeout, this, &CameraClient::ptzTick);
	m_zoomDirectScale = getZoomDirectScale();
}

void CameraClient::applyPreset(int const preset, CameraPreset const &cp) {
	if (preset > 0 && preset <= MaxCameraPresets) {
		if (cp.usePtz) {
			startPtzJob({.pan = cp.pan, .tilt = cp.tilt, .zoom = cp.zoom});
		} else {
			cancelPtzJob();
			// VISCA Memory Recall: 81 01 04 3F 02 pp FF
			std::array<uint8_t, 7> const cmd{
				0x81, 0x01, 0x04, 0x3F, 0x02,
				static_cast<uint8_t>(preset),
				0xFF
			};
			sendVisca(cmd);
		}
		setBrightness(cp.brightness);
		setSaturation(cp.saturation);
		setContrast(cp.contrast);
		setSharpness(cp.sharpness);
		setHue(cp.hue);
	}
}

void CameraClient::setPreset(int const preset) {
	auto const presets = getCameraPresets();
	if (preset > 0 && preset - 1 < presets.size()) {
		applyPreset(preset, presets[preset - 1]);
	}
}

void CameraClient::powerOn() {
	// VISCA Power On: 81 01 04 00 02 FF
	constexpr std::array<uint8_t, 6> cmd{0x81, 0x01, 0x04, 0x00, 0x02, 0xFF};
	sendVisca(cmd);
}

void CameraClient::powerOff() {
	cancelPtzJob();
	// VISCA Power Off / Standby: 81 01 04 00 03 FF
	constexpr std::array<uint8_t, 6> cmd{0x81, 0x01, 0x04, 0x00, 0x03, 0xFF};
	sendVisca(cmd);
}

void CameraClient::reboot() {
	// VISCA has no reboot command, so power cycle instead
	powerOff();
	// give the camera time to finish entering standby before powering back on
	QTimer::singleShot(5000, this, &CameraClient::powerOn);
	m_connected = false;
	m_missedPolls = 3;
	emit pollFailed();
}

bool CameraClient::connected() const noexcept {
	return m_connected;
}

void CameraClient::queryPtzPosition() {
	m_ptzQuery = PtzQuery{.panTiltGen = m_panTiltGen, .zoomGen = m_zoomGen};
	auto const id = ++m_ptzQueryId;
	sendPanTiltInquiry();
	sendZoomInquiry();
	QTimer::singleShot(PtzQueryTimeoutMs, this, [this, id] {
		if (m_ptzQuery && id == m_ptzQueryId) {
			m_ptzQuery.reset();
			emit ptzPositionQueryFailed();
		}
	});
}

void CameraClient::reloadConnectionSettings() {
	cancelPtzJob();
	auto const [host, port] = getCameraConnectionData();
	m_host = host;
	m_port = port;
	m_sequenceNumber = 1;
	m_missedPolls = 0;
	m_addr.clear();
	if (m_lookupId != -1) {
		QHostInfo::abortHostLookup(m_lookupId);
		m_lookupId = -1;
	}
	if (m_host.isEmpty() || m_addr.setAddress(m_host)) {
		return;
	}
	m_lookupId = QHostInfo::lookupHost(m_host, this, &CameraClient::onHostLookup);
}

void CameraClient::onHostLookup(QHostInfo const &hostInfo) {
	if (hostInfo.lookupId() != m_lookupId) {
		return;
	}
	m_lookupId = -1;
	// socket is bound to IPv4, so only an IPv4 address is usable
	for (auto const &addr : hostInfo.addresses()) {
		if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
			m_addr = addr;
			return;
		}
	}
}

void CameraClient::setBrightness(int const val) {
	// VISCA Brightness Direct: 81 01 04 4D 00 00 0p 0q FF
	sendDirect(0x4D, val);
}

void CameraClient::setSaturation(int const val) {
	// VISCA Color Gain Direct: 81 01 04 49 00 00 0p 0q FF
	sendDirect(0x49, val);
}

void CameraClient::setContrast(int const val) {
	// VISCA Contrast Direct: 81 01 04 A2 00 00 0p 0q FF
	sendDirect(0xA2, val);
}

void CameraClient::setSharpness(int const val) {
	// VISCA Aperture Direct: 81 01 04 42 00 00 0p 0q FF
	sendDirect(0x42, val);
}

void CameraClient::setHue(int const val) {
	// VISCA Color Hue Direct: 81 01 04 4F 00 00 0p 0q FF
	sendDirect(0x4F, val);
}

void CameraClient::sendDirect(uint8_t const opcode, int const val) {
	if (val < 0) {
		return;
	}
	auto const v = std::min(val, 0xFF);
	std::array<uint8_t, 9> const cmd{
		0x81, 0x01, 0x04, opcode, 0x00, 0x00,
		static_cast<uint8_t>((v >> 4) & 0x0F),
		static_cast<uint8_t>(v & 0x0F),
		0xFF
	};
	sendVisca(cmd);
}

void CameraClient::startPtzJob(PtzPosition const &target) {
	cancelPtzJob();
	auto const speed = getPtzSpeed();
	auto &job = m_ptzJob.emplace();
	job.target = target;
	job.panTiltSpeed = speed.panTilt;
	job.zoomSpeed = speed.zoom;
	startPtzStep();
	m_ptzTimer.start(PtzTickMs);
}

void CameraClient::cancelPtzJob() {
	if (!m_ptzJob) {
		return;
	}
	// stop any movement in progress so the next job doesn't start mid move
	switch (m_ptzJob->steps[m_ptzJob->stepIdx]) {
		case PtzStep::Locate:
			break;
		case PtzStep::Zoom:
			sendZoomStop();
			break;
		case PtzStep::PanTilt:
			sendPanTiltStop();
			break;
	}
	m_ptzJob.reset();
	m_ptzTimer.stop();
}

void CameraClient::ptzTick() {
	if (!m_ptzJob) {
		m_ptzTimer.stop();
		return;
	}
	auto &job = *m_ptzJob;
	auto const step = job.steps[job.stepIdx];
	switch (step) {
		case PtzStep::Locate:
			if (m_panTiltGen != job.seenPanTiltGen && m_zoomGen != job.seenZoomGen) {
				// zoom out before panning, zoom in after
				if (job.target.zoom >= m_ptzPos.zoom) {
					job.steps = {PtzStep::Locate, PtzStep::PanTilt, PtzStep::Zoom};
				}
				advancePtzStep();
				return;
			}
			if (job.stepTimer.elapsed() > LocateTimeoutMs) {
				// position unknown, so the order can't be chosen, just move everything at once
				sendZoomDirect(job.target.zoom);
				sendPanTiltAbsolute(job.target, job.panTiltSpeed);
				m_ptzJob.reset();
				m_ptzTimer.stop();
				return;
			}
			sendPanTiltInquiry();
			sendZoomInquiry();
			return;
		case PtzStep::Zoom:
			// while driving, replies are handled as they arrive in updateZoomDrive
			if (!job.zoomDriving && m_zoomGen != job.seenZoomGen) {
				job.seenZoomGen = m_zoomGen;
				if (zoomStepDone()) {
					calibrateZoomDirect();
					advancePtzStep();
					return;
				}
			}
			break;
		case PtzStep::PanTilt:
			if (m_panTiltGen != job.seenPanTiltGen) {
				job.seenPanTiltGen = m_panTiltGen;
				if (panTiltStepDone()) {
					advancePtzStep();
					return;
				}
			}
			break;
	}
	if (job.stepTimer.elapsed() > StepTimeoutMs) {
		if (step == PtzStep::Zoom && job.zoomDriving) {
			finishZoomDrive();
		}
		advancePtzStep();
		return;
	}
	if (step == PtzStep::Zoom) {
		if (!job.zoomDriving) {
			sendZoomInquiry();
		} else if (job.stepTimer.elapsed() - job.zoomInquiryMs >= 2 * PtzTickMs) {
			// reply was probably lost, restart the inquiry chain
			sendZoomDriveInquiry();
		}
	} else {
		sendPanTiltInquiry();
	}
}

void CameraClient::startPtzStep() {
	auto &job = *m_ptzJob;
	job.stepTimer.start();
	job.moveTimer.start();
	// require fresh replies, sent after this step began
	job.seenPanTiltGen = m_panTiltGen;
	job.seenZoomGen = m_zoomGen;
	job.lastSample = m_ptzPos;
	job.lastChangeTimer.start();
	job.zoomDriving = false;
	job.zoomDirectSent = -1;
	switch (job.steps[job.stepIdx]) {
		case PtzStep::Locate:
			sendPanTiltInquiry();
			sendZoomInquiry();
			break;
		case PtzStep::Zoom:
			if (job.zoomSpeed > 0 && std::abs(job.target.zoom - m_ptzPos.zoom) > ZoomTolerance) {
				// drive at the configured speed, then finish with Zoom Direct once the target is reached
				job.zoomOut = job.target.zoom < m_ptzPos.zoom;
				job.zoomDriving = true;
				job.zoomAnchorMs = -1;
				job.zoomVelocity = 0;
				sendZoomVariable(job.zoomOut, job.zoomSpeed);
				sendZoomDriveInquiry();
			} else if (std::abs(job.target.zoom - m_ptzPos.zoom) > ZoomTolerance) {
				sendZoomDirect(job.target.zoom);
			}
			// already in position otherwise, and even a tiny move can make the camera refocus
			break;
		case PtzStep::PanTilt:
			if (std::abs(job.target.pan - m_ptzPos.pan) > PanTiltTolerance ||
			    std::abs(job.target.tilt - m_ptzPos.tilt) > PanTiltTolerance) {
				sendPanTiltAbsolute(job.target, job.panTiltSpeed);
			}
			break;
	}
}

void CameraClient::advancePtzStep() {
	auto &job = *m_ptzJob;
	++job.stepIdx;
	if (job.stepIdx >= job.steps.size()) {
		m_ptzJob.reset();
		m_ptzTimer.stop();
		return;
	}
	startPtzStep();
}

bool CameraClient::zoomStepDone() {
	auto const &job = *m_ptzJob;
	auto const stable = ptzStable();
	if (job.zoomDirectSent >= 0 && job.moveTimer.elapsed() < ZoomDirectStartMs) {
		// position may be from before the Zoom Direct took effect
		return false;
	}
	return std::abs(m_ptzPos.zoom - job.target.zoom) <= ZoomTolerance || stable;
}

void CameraClient::calibrateZoomDirect() {
	auto const sent = m_ptzJob->zoomDirectSent;
	if (sent < MinZoomCalibrationVal) {
		return;
	}
	auto const scale = static_cast<double>(m_ptzPos.zoom) / sent;
	// anything far off 1 is more likely the zoom being interrupted than a real scale difference
	if (scale < 0.5 || scale > 2 || std::abs(scale - m_zoomDirectScale) < 0.001) {
		return;
	}
	m_zoomDirectScale = scale;
	setZoomDirectScale(scale);
}

void CameraClient::updateZoomDrive() {
	auto &job = *m_ptzJob;
	job.seenZoomGen = m_zoomGen;
	auto const now = job.stepTimer.elapsed();
	auto const zoom = m_ptzPos.zoom;
	// distance left in the direction of travel, negative once past the target
	auto const remaining = job.zoomOut ? zoom - job.target.zoom : job.target.zoom - zoom;
	if (job.zoomAnchorMs < 0) {
		job.zoomAnchorMs = now;
		job.zoomAnchorVal = zoom;
	} else if (now - job.zoomAnchorMs >= ZoomVelocityWindowMs) {
		auto const moved = job.zoomOut ? job.zoomAnchorVal - zoom : zoom - job.zoomAnchorVal;
		job.zoomVelocity = std::max(0.0, static_cast<double>(moved) / static_cast<double>(now - job.zoomAnchorMs));
		job.zoomAnchorMs = now;
		job.zoomAnchorVal = zoom;
	}
	auto const lead = job.zoomVelocity * ZoomStopLeadMs;
	if (remaining - lead <= ZoomTolerance || ptzStable()) {
		finishZoomDrive();
		return;
	}
	sendZoomDriveInquiry();
}

void CameraClient::finishZoomDrive() {
	auto &job = *m_ptzJob;
	sendZoomStop();
	// correct any remaining error, the step completes once the zoom settles
	sendZoomDirect(job.target.zoom);
	job.zoomDriving = false;
	job.moveTimer.start();
	job.lastChangeTimer.start();
}

void CameraClient::sendZoomDriveInquiry() {
	m_ptzJob->zoomInquiryMs = m_ptzJob->stepTimer.elapsed();
	sendZoomInquiry();
}

bool CameraClient::panTiltStepDone() {
	auto const &job = *m_ptzJob;
	auto const stable = ptzStable();
	auto const reached =
		std::abs(m_ptzPos.pan - job.target.pan) <= PanTiltTolerance &&
		std::abs(m_ptzPos.tilt - job.target.tilt) <= PanTiltTolerance;
	return reached || stable;
}

bool CameraClient::ptzStable() {
	auto &job = *m_ptzJob;
	if (m_ptzPos != job.lastSample) {
		job.lastSample = m_ptzPos;
		job.lastChangeTimer.start();
	}
	return job.lastChangeTimer.elapsed() >= StableMs && job.moveTimer.elapsed() >= MinSettleMs;
}

void CameraClient::sendPanTiltInquiry() {
	// VISCA Pan-tilt Position Inquiry: 81 09 06 12 FF
	constexpr std::array<uint8_t, 5> inq{0x81, 0x09, 0x06, 0x12, 0xFF};
	sendVisca(inq, true);
}

void CameraClient::sendZoomInquiry() {
	// VISCA Zoom Position Inquiry: 81 09 04 47 FF
	constexpr std::array<uint8_t, 5> inq{0x81, 0x09, 0x04, 0x47, 0xFF};
	sendVisca(inq, true);
}

void CameraClient::sendPanTiltAbsolute(PtzPosition const &pos, int const speed) {
	// VISCA Pan-tilt Absolute: 81 01 06 02 VV WW 0Y 0Y 0Y 0Y [0Y] 0Z 0Z 0Z 0Z FF
	std::array<uint8_t, 16> cmd{
		0x81, 0x01, 0x06, 0x02,
		static_cast<uint8_t>(std::clamp(speed, 0x01, 0x18)),
		static_cast<uint8_t>(std::clamp(speed, 0x01, 0x14)),
	};
	auto i = writeNibbles(cmd, 6, pos.pan, m_panNibbles);
	i = writeNibbles(cmd, i, pos.tilt, 4);
	cmd[i++] = 0xFF;
	sendVisca(std::span{cmd}.first(i));
}

void CameraClient::sendPanTiltStop() {
	// VISCA Pan-tilt Stop: 81 01 06 01 VV WW 03 03 FF
	constexpr std::array<uint8_t, 9> cmd{0x81, 0x01, 0x06, 0x01, 0x01, 0x01, 0x03, 0x03, 0xFF};
	sendVisca(cmd);
}

void CameraClient::sendZoomDirect(int const zoom) {
	// zoom is a reported position, convert it to the camera's Zoom Direct scale
	auto const val = std::clamp(static_cast<int>(std::lround(zoom / m_zoomDirectScale)), 0, 0xFFFF);
	if (m_ptzJob) {
		m_ptzJob->zoomDirectSent = val;
	}
	// VISCA Zoom Direct: 81 01 04 47 0p 0q 0r 0s FF
	std::array<uint8_t, 9> cmd{0x81, 0x01, 0x04, 0x47};
	auto i = writeNibbles(cmd, 4, val, 4);
	cmd[i] = 0xFF;
	sendVisca(cmd);
}

void CameraClient::sendZoomVariable(bool const out, int const speed) {
	// VISCA Zoom Tele (Variable): 81 01 04 07 2p FF, Wide (Variable): 81 01 04 07 3p FF
	auto const p = static_cast<uint8_t>(std::clamp(speed - 1, 0, 7));
	std::array<uint8_t, 6> const cmd{
		0x81, 0x01, 0x04, 0x07,
		static_cast<uint8_t>((out ? 0x30 : 0x20) | p),
		0xFF
	};
	sendVisca(cmd);
}

void CameraClient::sendZoomStop() {
	// VISCA Zoom Stop: 81 01 04 07 00 FF
	constexpr std::array<uint8_t, 6> cmd{0x81, 0x01, 0x04, 0x07, 0x00, 0xFF};
	sendVisca(cmd);
}

ViscaMsg CameraClient::createViscaPacket(std::span<uint8_t const> const viscaMsg, bool const isInquiry) {
	ViscaMsg packet;
	if (8 + viscaMsg.size() > MaxMsgSz) {
		return packet;
	}
	// Payload type: 0x01 0x00 for command, 0x01 0x10 for inquiry
	packet.data[0] = 0x01;
	packet.data[1] = isInquiry ? 0x10 : 0x00;
	// Payload length (16-bit big-endian)
	auto const len = static_cast<uint16_t>(viscaMsg.size());
	packet.data[2] = static_cast<uint8_t>((len >> 8) & 0xFF);
	packet.data[3] = static_cast<uint8_t>(len & 0xFF);
	// Sequence number (32-bit big-endian)
	auto const seq = m_sequenceNumber++;
	packet.data[4] = static_cast<uint8_t>((seq >> 24) & 0xFF);
	packet.data[5] = static_cast<uint8_t>((seq >> 16) & 0xFF);
	packet.data[6] = static_cast<uint8_t>((seq >> 8) & 0xFF);
	packet.data[7] = static_cast<uint8_t>(seq & 0xFF);
	// Payload
	std::ranges::copy(viscaMsg, packet.data.begin() + 8);
	packet.size = 8 + viscaMsg.size();
	return packet;
}

void CameraClient::sendVisca(std::span<uint8_t const> const viscaMsg, bool const isInquiry) {
	if (m_addr.isNull() || m_port == 0) {
		return;
	}
	auto const packet = createViscaPacket(viscaMsg, isInquiry);
	if (packet.size > 0) {
		m_socket->writeDatagram(reinterpret_cast<char const*>(packet.data.data()), static_cast<qint64>(packet.size), m_addr, m_port);
	}
}

void CameraClient::poll() {
	if (m_missedPolls < 10) {
		++m_missedPolls;
	}
	if (m_missedPolls >= 3) {
		if (m_connected) {
			m_connected = false;
			emit pollFailed();
		}
	}
	// VISCA Power Inquiry: 81 09 04 00 FF
	constexpr std::array<uint8_t, 5> inq{0x81, 0x09, 0x04, 0x00, 0xFF};
	sendVisca(inq, true);
}

void CameraClient::onReadyRead() {
	while (m_socket->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(static_cast<qsizetype>(m_socket->pendingDatagramSize()));
		QHostAddress sender;
		quint16 senderPort = 0;
		m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
		if (datagram.isEmpty() || !sender.isEqual(m_addr)) {
			continue;
		}
		m_missedPolls = 0;
		m_connected = true;
		emit pollUpdate();
		std::span<uint8_t const> msg{
			reinterpret_cast<uint8_t const*>(datagram.constData()),
			static_cast<size_t>(datagram.size())};
		// strip the VISCA over IP header if present, raw VISCA replies start with 0x90
		if (msg.size() >= 8 && msg[0] == 0x01) {
			// payload type 01 11 is a VISCA reply, others carry no VISCA payload
			if (msg[1] != 0x11) {
				continue;
			}
			auto const len = static_cast<size_t>((msg[2] << 8) | msg[3]);
			msg = msg.subspan(8, std::min(len, msg.size() - 8));
		}
		handleViscaReply(msg);
	}
}

void CameraClient::handleViscaReply(std::span<uint8_t const> const reply) {
	// inquiry replies: y0 50 [data] FF
	if (reply.size() < 3 || reply.back() != 0xFF || (reply[1] & 0xF0) != 0x50) {
		return;
	}
	auto const data = reply.subspan(2, reply.size() - 3);
	if (std::ranges::any_of(data, [](uint8_t const b) { return b > 0x0F; })) {
		return;
	}
	// Replies don't say which inquiry they answer, so identify them by size.
	// This is unambiguous only because power, pan-tilt, and zoom are the only inquiries sent.
	switch (data.size()) {
		case 8: // pan-tilt, 4 nibble pan
		case 9: { // pan-tilt, 5 nibble pan
			m_panNibbles = data.size() - 4;
			auto const panBits = m_panNibbles * 4;
			m_ptzPos.pan = signExtend(readNibbles(data.first(m_panNibbles)), panBits);
			m_ptzPos.tilt = signExtend(readNibbles(data.subspan(m_panNibbles)), 16);
			++m_panTiltGen;
			break;
		}
		case 4: // zoom
			m_ptzPos.zoom = readNibbles(data);
			++m_zoomGen;
			// react immediately rather than on the next tick, a stale position overshoots the target
			if (m_ptzJob && m_ptzJob->steps[m_ptzJob->stepIdx] == PtzStep::Zoom && m_ptzJob->zoomDriving) {
				updateZoomDrive();
			}
			break;
		default:
			return;
	}
	checkPtzQuery();
}

void CameraClient::checkPtzQuery() {
	if (m_ptzQuery && m_ptzQuery->panTiltGen != m_panTiltGen && m_ptzQuery->zoomGen != m_zoomGen) {
		m_ptzQuery.reset();
		emit ptzPositionReceived(m_ptzPos.pan, m_ptzPos.tilt, m_ptzPos.zoom);
	}
}

void CameraClient::onSocketError(QAbstractSocket::SocketError) {
	if (m_connected) {
		m_connected = false;
		emit pollFailed();
	}
}
