/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <concepts>

#include <QHostInfo>
#include <QLoggingCategory>

#include "cameraclient.hpp"
#include "consts.hpp"
#include "settingsdata.hpp"

// Commands, move steps, and camera errors. Debug output is off by default,
// enable it with QT_LOGGING_RULES="sc9k.camera.debug=true".
Q_LOGGING_CATEGORY(lcCamera, "sc9k.camera", QtWarningMsg)
// Every reply, including positions while moving, which is a lot of output.
// Enable with QT_LOGGING_RULES="sc9k.camera.replies.debug=true".
Q_LOGGING_CATEGORY(lcCameraReplies, "sc9k.camera.replies", QtWarningMsg)

constexpr auto PtzTickMs = 150;
// The camera only updates its reported zoom every ~40 ms, so asking faster is wasted.
// On a fast network, asking as soon as each reply arrives would flood the camera.
constexpr auto ZoomDriveInquiryMinMs = 25;
// a command rejected because the camera is busy is retried until it gets through or the move changes
constexpr auto CommandRetryMs = 200;
constexpr auto MaxCommandRetries = 50;
// sent commands are kept this long in case the camera rejects them
constexpr auto SentCommandTtlMs = 3000;
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
// start the next move once the current one has this fraction of its distance left
constexpr auto PtzOverlap = 0.5;
// below this, rounding makes the Zoom Direct scale too imprecise to learn from
constexpr auto MinZoomCalibrationVal = 1000;
// camera lands exactly on pan/tilt and Zoom Direct targets, allow a little for other cameras
constexpr auto PanTiltTolerance = 2;
constexpr auto ZoomTolerance = 4;
constexpr auto PtzQueryTimeoutMs = 2000;
// The camera silently ignores a Pan-tilt Absolute sent while a variable speed zoom is running,
// so a pan/tilt move that never starts is resent, which also covers the rest of a zoom drive.
constexpr auto MaxPanTiltResends = 10;

constexpr auto CalibrationTickMs = 30;
// camera stopped replying
constexpr auto CalibrationReplyTimeoutMs = 2000;
// cameras without the max speed inquiry don't answer it
constexpr auto CalibrationMaxSpeedTimeoutMs = 1000;
constexpr auto CalibrationPhaseTimeoutMs = 20000;
// position unchanged for this long means the camera has stopped
constexpr auto CalibrationSettleMs = 500;
// camera may not have started moving yet, so don't treat it as stopped until this has passed
constexpr auto CalibrationMinSettleMs = 300;
// return home before the next measurement once this far from it, to stay clear of the limits
constexpr auto CalibrationPanDrift = 500;
constexpr auto CalibrationTiltDrift = 150;
// least movement to measure a velocity from
constexpr auto CalibrationMinSamples = 8;
constexpr auto CalibrationMinSampleMs = 250;
// Zoom Direct value used to measure the zoom scale, mid range on most cameras
constexpr auto CalibrationZoomVal = 8000;

[[nodiscard]]
static constexpr int readNibbles(std::span<uint8_t const> const src) {
	int out = 0;
	for (auto const n : src) {
		out = (out << 4) | (n & 0x0F);
	}
	return out;
}

/**
 * @return fraction of the distance from start to target still left to travel
 */
[[nodiscard]]
static constexpr double remainingFraction(int const start, int const pos, int const target) {
	auto const total = std::abs(target - start);
	if (total == 0) {
		return 0;
	}
	return static_cast<double>(std::abs(target - pos)) / total;
}

/**
 * @return the speed whose velocity is closest to the given velocity, at least 1
 */
[[nodiscard]]
static constexpr int speedForVelocity(std::span<double const> const velocities, double const velocity) {
	size_t best = 1;
	for (size_t i = 2; i < velocities.size(); ++i) {
		if (std::abs(velocities[i] - velocity) < std::abs(velocities[best] - velocity)) {
			best = i;
		}
	}
	return static_cast<int>(best);
}

template<std::unsigned_integral T>
[[nodiscard]]
static constexpr std::optional<T> readBigEndian(std::span<uint8_t const, sizeof(T)> const src) {
	if (src.size() < sizeof(T)) {
		return std::nullopt;
	}
	T out = 0;
	for (auto const b : src) {
		out = static_cast<T>((out << 8) | b);
	}
	return out;
}

[[nodiscard]]
static constexpr int signExtend(int const val, size_t const bits) {
	auto const signBit = 1 << (bits - 1);
	return (val & signBit) ? val - (1 << bits) : val;
}

/**
 * @return how long to drive an axis for a speed measurement, long enough to measure
 * slow speeds and short enough to keep fast ones within range
 */
[[nodiscard]]
static constexpr qint64 calibrationDriveMs(bool const tilt, int const speed) {
	// tilt range is much smaller than pan range
	if (tilt) {
		return speed <= 4 ? 1200 : speed <= 12 ? 700 : 500;
	}
	return speed <= 4 ? 1600 : speed <= 12 ? 1000 : 700;
}

/**
 * Measurements are noisy where the speed curve flattens out, but a faster speed is never slower.
 */
static void makeNonDecreasing(QVector<double> &vals) {
	for (qsizetype i = 1; i < vals.size(); ++i) {
		vals[i] = std::max(vals[i], vals[i - 1]);
	}
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
	connect(&m_calibrationTimer, &QTimer::timeout, this, &CameraClient::calibrationTick);
	m_zoomInquiryTimer.setSingleShot(true);
	connect(&m_zoomInquiryTimer, &QTimer::timeout, this, [this] {
		if (m_ptzJob && m_ptzJob->zoomDriving) {
			sendZoomDriveInquiry();
		}
	});
	m_clock.start();
	m_zoomDirectScale = getZoomDirectScale();
	reloadCalibration();
}

void CameraClient::applyPreset(int const preset, CameraPreset const &cp) {
	if (preset > 0 && preset <= MaxCameraPresets) {
		if (m_calibration) {
			// live use takes priority, and the preset moves the camera anyway
			finishCalibration(false, tr("Calibration was interrupted by a camera preset."), false);
		}
		if (cp.usePtz) {
			startPtzJob(
				{.pan = cp.pan, .tilt = cp.tilt, .zoom = cp.zoom},
				cp.manualFocus ? std::optional{cp.focus} : std::nullopt);
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
	if (m_calibration) {
		finishCalibration(false, tr("Calibration was interrupted by powering off the camera."), false);
	}
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
	m_ptzQuery = PtzQuery{.panTiltGen = m_panTiltGen, .zoomGen = m_zoomGen, .focusGen = m_focusGen};
	auto const id = ++m_ptzQueryId;
	sendPanTiltInquiry();
	sendZoomInquiry();
	sendFocusInquiry();
	QTimer::singleShot(PtzQueryTimeoutMs, this, [this, id] {
		if (!m_ptzQuery || id != m_ptzQueryId) {
			return;
		}
		auto const query = *m_ptzQuery;
		m_ptzQuery.reset();
		if (query.panTiltGen != m_panTiltGen && query.zoomGen != m_zoomGen) {
			// focus isn't essential, report the position without it
			emit ptzPositionReceived(m_ptzPos.pan, m_ptzPos.tilt, m_ptzPos.zoom, -1);
		} else {
			emit ptzPositionQueryFailed();
		}
	});
}

void CameraClient::startCalibration() {
	if (m_calibration) {
		return;
	}
	if (!m_connected) {
		emit calibrationFinished(false, tr("Camera is not connected."));
		return;
	}
	cancelPtzJob();
	auto &cal = m_calibration.emplace();
	cal.replyTimer.start();
	enterCalibrationPhase(CalibrationJob::Phase::Start);
	m_calibrationTimer.start(CalibrationTickMs);
}

void CameraClient::cancelCalibration() {
	if (m_calibration) {
		finishCalibration(false, tr("Calibration was cancelled."));
	}
}

void CameraClient::reloadCalibration() {
	auto const cal = getCameraCalibration();
	m_maxPanSpeed = cal.maxPanSpeed;
	m_maxTiltSpeed = cal.maxTiltSpeed;
	m_panVelocity = cal.panVelocity;
	m_tiltVelocity = cal.tiltVelocity;
}

void CameraClient::reloadConnectionSettings() {
	cancelPtzJob();
	if (m_calibration) {
		finishCalibration(false, tr("Calibration was interrupted by a change of connection settings."), false);
	}
	auto const [host, port] = getCameraConnectionData();
	m_host = host;
	m_port = port;
	m_sequenceNumber = 1;
	// sequence numbers restart, so pending ones could be mistaken for new ones
	m_focusInquirySeqs.clear();
	m_sentCommands.clear();
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

void CameraClient::startPtzJob(PtzPosition const &target, std::optional<int> const focus) {
	cancelPtzJob();
	auto const speed = getPtzSpeed();
	auto &job = m_ptzJob.emplace();
	job.target = target;
	job.focus = focus;
	if (!focus) {
		// the previous preset may have left the camera in manual focus
		sendFocusAuto();
	}
	job.panTiltSpeed = speed.panTilt;
	job.zoomSpeed = speed.zoom;
	job.jobTimer.start();
	qCDebug(lcCamera).nospace() << "PTZ job start: target pan " << target.pan << ", tilt " << target.tilt
		<< ", zoom " << target.zoom << ", focus " << (focus ? QString::number(*focus) : QStringLiteral("auto"));
	startPtzStep();
	m_ptzTimer.start(PtzTickMs);
}

void CameraClient::cancelPtzJob() {
	if (!m_ptzJob) {
		return;
	}
	qCDebug(lcCamera) << "PTZ job cancelled";
	// the job's commands shouldn't be retried once it's gone
	++m_commandEpoch;
	// stop any movement in progress so the next job doesn't start mid move,
	// moves overlap, so both axes may be moving
	if (m_ptzJob->stepIdx > 0) {
		sendZoomStop();
		sendPanTiltStop();
	}
	m_ptzJob.reset();
	m_ptzTimer.stop();
	m_zoomInquiryTimer.stop();
}

void CameraClient::ptzTick() {
	if (!m_ptzJob) {
		m_ptzTimer.stop();
		return;
	}
	auto &job = *m_ptzJob;
	if (job.zoomDriving && job.jobTimer.elapsed() - job.zoomInquiryMs >= 2 * PtzTickMs) {
		// reply was probably lost, restart the inquiry chain
		sendZoomDriveInquiry();
	}
	if (job.stepIdx >= job.steps.size()) {
		// all moves started, waiting on a zoom drive that overlapped into the last step
		if (job.stepTimer.elapsed() > StepTimeoutMs) {
			finishZoomDrive();
		}
		finishPtzJob();
		return;
	}
	auto const step = job.steps[job.stepIdx];
	switch (step) {
		case PtzStep::Locate:
			if (m_panTiltGen != job.seenPanTiltGen && m_zoomGen != job.seenZoomGen) {
				// zoom out before panning, zoom in after
				if (job.target.zoom >= m_ptzPos.zoom) {
					job.steps = {PtzStep::Locate, PtzStep::PanTilt, PtzStep::Zoom, PtzStep::Focus};
				}
				advancePtzStep();
				return;
			}
			if (job.stepTimer.elapsed() > LocateTimeoutMs) {
				qCWarning(lcCamera) << "PTZ locate timed out, camera did not report its position";
				// position unknown, so the order can't be chosen, just move everything at once,
				// manual focus would be thrown off by zoom tracking, so it is skipped
				sendZoomDirect(job.target.zoom);
				sendPanTiltAbsolute(job.target, job.panTiltSpeed, job.panTiltSpeed);
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
				if (zoomNearlyDone()) {
					advancePtzStep();
					return;
				}
			}
			break;
		case PtzStep::PanTilt:
			if (m_panTiltGen != job.seenPanTiltGen) {
				job.seenPanTiltGen = m_panTiltGen;
				if (panTiltStepDone() || panTiltNearlyDone()) {
					advancePtzStep();
					return;
				}
			}
			break;
		case PtzStep::Focus:
			if (!job.focus) {
				advancePtzStep();
				return;
			}
			// the camera adjusts focus as it zooms, so focus can only be set once the zoom settles
			if (!job.zoomDriving && m_zoomGen != job.seenZoomGen) {
				job.seenZoomGen = m_zoomGen;
				if (zoomStepDone()) {
					sendFocusManual();
					sendFocusDirect(*job.focus);
					advancePtzStep();
					return;
				}
			}
			break;
	}
	if (job.stepTimer.elapsed() > StepTimeoutMs) {
		if (job.zoomDriving) {
			finishZoomDrive();
		}
		if (step == PtzStep::Focus && job.focus) {
			sendFocusManual();
			sendFocusDirect(*job.focus);
		}
		advancePtzStep();
		return;
	}
	switch (step) {
		case PtzStep::Locate:
			break;
		case PtzStep::Zoom:
		case PtzStep::Focus:
			if (!job.zoomDriving) {
				sendZoomInquiry();
			}
			break;
		case PtzStep::PanTilt:
			sendPanTiltInquiry();
			break;
	}
}

void CameraClient::startPtzStep() {
	auto &job = *m_ptzJob;
	job.stepTimer.start();
	// require fresh replies, sent after this step began
	job.seenPanTiltGen = m_panTiltGen;
	job.seenZoomGen = m_zoomGen;
	job.stepStart = m_ptzPos;
	qCDebug(lcCamera).nospace() << "PTZ step " << ptzStepName(job.steps[job.stepIdx])
		<< " from pan " << m_ptzPos.pan << ", tilt " << m_ptzPos.tilt << ", zoom " << m_ptzPos.zoom;
	auto const resetMotion = [this](PtzAxisMotion &motion) {
		motion.moveTimer.start();
		motion.changeTimer.start();
		motion.last = m_ptzPos;
	};
	switch (job.steps[job.stepIdx]) {
		case PtzStep::Locate:
			sendPanTiltInquiry();
			sendZoomInquiry();
			break;
		case PtzStep::Zoom:
			resetMotion(job.zoomMotion);
			job.zoomDirectSent = -1;
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
			resetMotion(job.panTiltMotion);
			job.panSpeedSent = 0;
			job.tiltSpeedSent = 0;
			job.panTiltResends = 0;
			if (auto const panDist = std::abs(job.target.pan - m_ptzPos.pan),
			               tiltDist = std::abs(job.target.tilt - m_ptzPos.tilt);
			    panDist > PanTiltTolerance || tiltDist > PanTiltTolerance) {
				// the longer axis moves at the configured speed and the shorter one slower,
				// so they arrive together and the camera moves in a straight line
				auto const panLonger = panDist >= tiltDist;
				auto const speed = std::clamp(job.panTiltSpeed, 1, panLonger ? m_maxPanSpeed : m_maxTiltSpeed);
				auto const velocity = panLonger ? m_panVelocity[speed] : m_tiltVelocity[speed];
				auto const ratio = static_cast<double>(std::min(panDist, tiltDist)) / std::max(panDist, tiltDist);
				job.panSpeedSent = panLonger ? speed : speedForVelocity(m_panVelocity, velocity * ratio);
				job.tiltSpeedSent = panLonger ? speedForVelocity(m_tiltVelocity, velocity * ratio) : speed;
				sendPanTiltAbsolute(job.target, job.panSpeedSent, job.tiltSpeedSent);
			}
			break;
		case PtzStep::Focus:
			break;
	}
}

void CameraClient::advancePtzStep() {
	auto &job = *m_ptzJob;
	++job.stepIdx;
	if (job.stepIdx >= job.steps.size()) {
		finishPtzJob();
		return;
	}
	startPtzStep();
}

void CameraClient::finishPtzJob() {
	if (m_ptzJob->zoomDriving) {
		return;
	}
	qCDebug(lcCamera).nospace() << "PTZ job done at pan " << m_ptzPos.pan << ", tilt " << m_ptzPos.tilt
		<< ", zoom " << m_ptzPos.zoom;
	m_ptzJob.reset();
	m_ptzTimer.stop();
}

bool CameraClient::hasNextPtzMove() const {
	auto const next = m_ptzJob->stepIdx + 1;
	// focus waits for the zoom to settle anyway, so only pan/tilt and zoom overlap
	return next < m_ptzJob->steps.size() && m_ptzJob->steps[next] != PtzStep::Focus;
}

bool CameraClient::zoomStepDone() {
	auto const &job = *m_ptzJob;
	auto const stable = zoomStable();
	if (job.zoomDirectSent >= 0 && job.zoomMotion.moveTimer.elapsed() < ZoomDirectStartMs) {
		// position may be from before the Zoom Direct took effect
		return false;
	}
	return std::abs(m_ptzPos.zoom - job.target.zoom) <= ZoomTolerance || stable;
}

bool CameraClient::zoomNearlyDone() const {
	auto const &job = *m_ptzJob;
	return hasNextPtzMove() &&
		remainingFraction(job.stepStart.zoom, m_ptzPos.zoom, job.target.zoom) <= PtzOverlap;
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
	auto const now = job.jobTimer.elapsed();
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
	if (remaining - lead <= ZoomTolerance || zoomStable()) {
		finishZoomDrive();
		return;
	}
	auto const inZoomStep = job.stepIdx < job.steps.size() && job.steps[job.stepIdx] == PtzStep::Zoom;
	if (inZoomStep && zoomNearlyDone()) {
		// start the next move, the drive continues alongside it
		advancePtzStep();
	}
	scheduleZoomDriveInquiry();
}

void CameraClient::finishZoomDrive() {
	auto &job = *m_ptzJob;
	qCDebug(lcCamera) << "PTZ zoom drive finished at" << m_ptzPos.zoom << "target" << job.target.zoom;
	m_zoomInquiryTimer.stop();
	sendZoomStop();
	// correct any remaining error, the zoom step completes once the zoom settles
	sendZoomDirect(job.target.zoom);
	job.zoomDriving = false;
	job.zoomMotion.moveTimer.start();
	job.zoomMotion.changeTimer.start();
	// a Pan-tilt Absolute sent during the drive was likely ignored, resend it now the drive
	// has stopped rather than waiting for the pan/tilt step to notice
	auto const inPanTiltStep = job.stepIdx < job.steps.size() && job.steps[job.stepIdx] == PtzStep::PanTilt;
	if (inPanTiltStep && job.panSpeedSent > 0 &&
	    std::abs(m_ptzPos.pan - job.stepStart.pan) <= PanTiltTolerance &&
	    std::abs(m_ptzPos.tilt - job.stepStart.tilt) <= PanTiltTolerance) {
		sendPanTiltAbsolute(job.target, job.panSpeedSent, job.tiltSpeedSent);
		job.panTiltMotion.moveTimer.start();
		job.panTiltMotion.changeTimer.start();
	}
}

void CameraClient::sendZoomDriveInquiry() {
	m_ptzJob->zoomInquiryMs = m_ptzJob->jobTimer.elapsed();
	sendZoomInquiry();
}

void CameraClient::scheduleZoomDriveInquiry() {
	if (m_zoomInquiryTimer.isActive()) {
		return;
	}
	auto const sinceLast = m_ptzJob->jobTimer.elapsed() - m_ptzJob->zoomInquiryMs;
	if (sinceLast >= ZoomDriveInquiryMinMs) {
		sendZoomDriveInquiry();
	} else {
		m_zoomInquiryTimer.start(static_cast<int>(ZoomDriveInquiryMinMs - sinceLast));
	}
}

char const *CameraClient::ptzStepName(PtzStep const step) {
	switch (step) {
		case PtzStep::Locate:
			return "Locate";
		case PtzStep::Zoom:
			return "Zoom";
		case PtzStep::PanTilt:
			return "PanTilt";
		case PtzStep::Focus:
			return "Focus";
	}
	return "Unknown";
}

bool CameraClient::panTiltStepDone() {
	auto &job = *m_ptzJob;
	auto const stable = panTiltStable();
	auto const reached =
		std::abs(m_ptzPos.pan - job.target.pan) <= PanTiltTolerance &&
		std::abs(m_ptzPos.tilt - job.target.tilt) <= PanTiltTolerance;
	if (stable && !reached && job.panSpeedSent > 0 && job.panTiltResends < MaxPanTiltResends) {
		auto const moved =
			std::abs(m_ptzPos.pan - job.stepStart.pan) > PanTiltTolerance ||
			std::abs(m_ptzPos.tilt - job.stepStart.tilt) > PanTiltTolerance;
		if (!moved) {
			++job.panTiltResends;
			qCWarning(lcCamera) << "camera did not start the pan/tilt move, resending, attempt" << job.panTiltResends;
			sendPanTiltAbsolute(job.target, job.panSpeedSent, job.tiltSpeedSent);
			job.panTiltMotion.moveTimer.start();
			job.panTiltMotion.changeTimer.start();
			return false;
		}
	}
	return reached || stable;
}

bool CameraClient::panTiltNearlyDone() const {
	auto const &job = *m_ptzJob;
	// pan and tilt move together, so go by whichever has further to go
	auto const remaining = std::max(
		remainingFraction(job.stepStart.pan, m_ptzPos.pan, job.target.pan),
		remainingFraction(job.stepStart.tilt, m_ptzPos.tilt, job.target.tilt));
	return hasNextPtzMove() && remaining <= PtzOverlap;
}

bool CameraClient::zoomStable() {
	auto &motion = m_ptzJob->zoomMotion;
	return axisStable(motion, m_ptzPos.zoom != motion.last.zoom);
}

bool CameraClient::panTiltStable() {
	auto &motion = m_ptzJob->panTiltMotion;
	return axisStable(motion, m_ptzPos.pan != motion.last.pan || m_ptzPos.tilt != motion.last.tilt);
}

bool CameraClient::axisStable(PtzAxisMotion &motion, bool const changed) const {
	if (changed) {
		motion.last = m_ptzPos;
		motion.changeTimer.start();
	}
	return motion.changeTimer.elapsed() >= StableMs && motion.moveTimer.elapsed() >= MinSettleMs;
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

void CameraClient::sendFocusInquiry() {
	// unanswered inquiries would otherwise accumulate
	if (m_focusInquirySeqs.size() > 16) {
		m_focusInquirySeqs.clear();
	}
	m_focusInquirySeqs.insert(m_sequenceNumber);
	// VISCA Focus Position Inquiry: 81 09 04 48 FF
	constexpr std::array<uint8_t, 5> inq{0x81, 0x09, 0x04, 0x48, 0xFF};
	sendVisca(inq, true);
}

void CameraClient::sendFocusAuto() {
	// VISCA Focus Auto: 81 01 04 38 02 FF
	constexpr std::array<uint8_t, 6> cmd{0x81, 0x01, 0x04, 0x38, 0x02, 0xFF};
	sendVisca(cmd);
}

void CameraClient::sendFocusManual() {
	// VISCA Focus Manual: 81 01 04 38 03 FF
	constexpr std::array<uint8_t, 6> cmd{0x81, 0x01, 0x04, 0x38, 0x03, 0xFF};
	sendVisca(cmd);
}

void CameraClient::sendFocusDirect(int const focus) {
	// VISCA Focus Direct: 81 01 04 48 0p 0q 0r 0s FF
	std::array<uint8_t, 9> cmd{0x81, 0x01, 0x04, 0x48};
	auto i = writeNibbles(cmd, 4, std::clamp(focus, 0, 0xFFFF), 4);
	cmd[i] = 0xFF;
	sendVisca(cmd);
}

void CameraClient::sendPanTiltAbsolute(PtzPosition const &pos, int const panSpeed, int const tiltSpeed) {
	// VISCA Pan-tilt Absolute: 81 01 06 02 VV WW 0Y 0Y 0Y 0Y [0Y] 0Z 0Z 0Z 0Z FF
	std::array<uint8_t, 16> cmd{
		0x81, 0x01, 0x06, 0x02,
		static_cast<uint8_t>(std::clamp(panSpeed, 1, m_maxPanSpeed)),
		static_cast<uint8_t>(std::clamp(tiltSpeed, 1, m_maxTiltSpeed)),
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
	sendZoomDirectValue(val);
}

void CameraClient::sendZoomDirectValue(int const val) {
	// VISCA Zoom Direct: 81 01 04 47 0p 0q 0r 0s FF
	std::array<uint8_t, 9> cmd{0x81, 0x01, 0x04, 0x47};
	auto i = writeNibbles(cmd, 4, std::clamp(val, 0, 0xFFFF), 4);
	cmd[i] = 0xFF;
	sendVisca(cmd);
}

void CameraClient::sendPanTiltDrive(bool const tilt, int const speed, bool const reverse) {
	// VISCA Pan-tilt Drive: 81 01 06 01 VV WW 0p 0q FF
	// p: 1 left, 2 right, 3 stop; q: 1 up, 2 down, 3 stop
	auto const dir = static_cast<uint8_t>(reverse ? 0x02 : 0x01);
	auto const v = static_cast<uint8_t>(std::clamp(speed, 1, 0x7F));
	std::array<uint8_t, 9> const cmd{
		0x81, 0x01, 0x06, 0x01,
		tilt ? uint8_t{1} : v,
		tilt ? v : uint8_t{1},
		tilt ? uint8_t{3} : dir,
		tilt ? dir : uint8_t{3},
		0xFF
	};
	sendVisca(cmd);
}

void CameraClient::sendPanTiltHome() {
	// VISCA Pan-tilt Home: 81 01 06 04 FF
	constexpr std::array<uint8_t, 5> cmd{0x81, 0x01, 0x06, 0x04, 0xFF};
	sendVisca(cmd);
}

void CameraClient::sendMaxSpeedInquiry() {
	// VISCA Pan-tilt MaxSpeed Inquiry: 81 09 06 11 FF
	constexpr std::array<uint8_t, 5> inq{0x81, 0x09, 0x06, 0x11, 0xFF};
	sendVisca(inq, true);
}

void CameraClient::calibrationTick() {
	if (!m_calibration) {
		m_calibrationTimer.stop();
		return;
	}
	using Phase = CalibrationJob::Phase;
	auto &cal = *m_calibration;
	if (cal.replyTimer.elapsed() > CalibrationReplyTimeoutMs) {
		finishCalibration(false, tr("Camera stopped responding."), false);
		return;
	}
	if (cal.phaseTimer.elapsed() > CalibrationPhaseTimeoutMs) {
		finishCalibration(false, tr("Camera did not finish moving."));
		return;
	}
	switch (cal.phase) {
		case Phase::Start:
			if (m_panTiltGen != cal.seenPanTiltGen && m_zoomGen != cal.seenZoomGen) {
				cal.start = m_ptzPos;
				cal.haveStart = true;
				enterCalibrationPhase(Phase::MaxSpeed);
				return;
			}
			sendPanTiltInquiry();
			sendZoomInquiry();
			return;
		case Phase::MaxSpeed:
			if (m_maxSpeedGen != cal.seenMaxSpeedGen) {
				// VISCA speeds are single bytes, anything else is a bad reply
				if (m_maxSpeedReplyPan > 0 && m_maxSpeedReplyPan < 0x80 &&
				    m_maxSpeedReplyTilt > 0 && m_maxSpeedReplyTilt < 0x80) {
					cal.maxPanSpeed = m_maxSpeedReplyPan;
					cal.maxTiltSpeed = m_maxSpeedReplyTilt;
				}
			} else if (cal.phaseTimer.elapsed() < CalibrationMaxSpeedTimeoutMs) {
				return;
			}
			// otherwise unsupported, keep the standard VISCA maximums
			enterCalibrationPhase(Phase::Home);
			return;
		case Phase::Home:
			sendPanTiltInquiry();
			if (calibrationSettled()) {
				cal.home = m_ptzPos;
				enterCalibrationPhase(Phase::Drive);
			}
			return;
		case Phase::Drive:
			sendPanTiltInquiry();
			if (cal.phaseTimer.elapsed() >= cal.driveMs) {
				enterCalibrationPhase(Phase::Stop);
			}
			return;
		case Phase::Stop:
			sendPanTiltInquiry();
			if (calibrationSettled()) {
				finishCalibrationMeasurement();
			}
			return;
		case Phase::Return:
			sendPanTiltInquiry();
			if (calibrationSettled()) {
				enterCalibrationPhase(cal.afterReturn);
			}
			return;
		case Phase::Zoom:
			sendZoomInquiry();
			if (cal.phaseTimer.elapsed() >= ZoomDirectStartMs && calibrationSettled()) {
				// a scale far off 1 more likely means the zoom was interrupted, keep the learned one
				if (auto const scale = static_cast<double>(m_ptzPos.zoom) / CalibrationZoomVal;
				    scale >= 0.5 && scale <= 2) {
					m_zoomDirectScale = scale;
					setZoomDirectScale(scale);
				}
				enterCalibrationPhase(Phase::Restore);
			}
			return;
		case Phase::Restore:
			sendPanTiltInquiry();
			sendZoomInquiry();
			if (calibrationSettled()) {
				finishCalibration(true, {});
			}
			return;
	}
}

void CameraClient::enterCalibrationPhase(CalibrationJob::Phase const phase) {
	using Phase = CalibrationJob::Phase;
	auto &cal = *m_calibration;
	cal.phase = phase;
	cal.phaseTimer.start();
	cal.settleLast = m_ptzPos;
	cal.settleTimer.start();
	cal.seenPanTiltGen = m_panTiltGen;
	cal.seenZoomGen = m_zoomGen;
	switch (phase) {
		case Phase::Start:
			sendPanTiltInquiry();
			sendZoomInquiry();
			break;
		case Phase::MaxSpeed:
			cal.seenMaxSpeedGen = m_maxSpeedGen;
			sendMaxSpeedInquiry();
			break;
		case Phase::Home:
			sendPanTiltHome();
			break;
		case Phase::Drive:
			cal.samples.clear();
			// a retry is after hitting a limit, so stay closer to home
			cal.driveMs = calibrationDriveMs(cal.tilt, cal.speed) / (cal.retried ? 2 : 1);
			sendPanTiltDrive(cal.tilt, cal.speed, cal.reverse);
			break;
		case Phase::Stop:
			sendPanTiltStop();
			break;
		case Phase::Return:
			sendPanTiltAbsolute(cal.home, cal.maxPanSpeed, cal.maxTiltSpeed);
			break;
		case Phase::Zoom:
			sendZoomDirectValue(CalibrationZoomVal);
			break;
		case Phase::Restore:
			sendPanTiltAbsolute(cal.start, cal.maxPanSpeed, cal.maxTiltSpeed);
			sendZoomDirect(cal.start.zoom);
			break;
	}
	emitCalibrationProgress();
}

void CameraClient::finishCalibrationMeasurement() {
	using Phase = CalibrationJob::Phase;
	auto &cal = *m_calibration;
	auto const velocity = calibrationVelocity();
	if (!velocity) {
		if (!cal.retried) {
			cal.retried = true;
			cal.afterReturn = Phase::Drive;
			enterCalibrationPhase(Phase::Return);
			return;
		}
		finishCalibration(false, tr("Could not measure %1 speed %2.")
			.arg(cal.tilt ? tr("tilt") : tr("pan")).arg(cal.speed));
		return;
	}
	(cal.tilt ? cal.tiltVelocity : cal.panVelocity).append(*velocity);
	cal.retried = false;
	cal.reverse = !cal.reverse;
	++cal.speed;
	if (cal.speed > (cal.tilt ? cal.maxTiltSpeed : cal.maxPanSpeed)) {
		if (cal.tilt) {
			cal.afterReturn = Phase::Zoom;
		} else {
			cal.tilt = true;
			cal.speed = 1;
			cal.reverse = false;
			cal.afterReturn = Phase::Drive;
		}
		enterCalibrationPhase(Phase::Return);
		return;
	}
	// alternating directions keeps the camera near home, but each move is longer than the last
	auto const drifted = cal.tilt ?
		std::abs(m_ptzPos.tilt - cal.home.tilt) > CalibrationTiltDrift :
		std::abs(m_ptzPos.pan - cal.home.pan) > CalibrationPanDrift;
	if (drifted) {
		cal.afterReturn = Phase::Drive;
		enterCalibrationPhase(Phase::Return);
	} else {
		enterCalibrationPhase(Phase::Drive);
	}
}

void CameraClient::finishCalibration(bool const ok, QString const &error, bool const restore) {
	qCDebug(lcCamera) << "calibration finished:" << (ok ? "ok" : qPrintable(error));
	// calibration moves shouldn't be retried once it's over
	++m_commandEpoch;
	auto cal = std::move(*m_calibration);
	m_calibration.reset();
	m_calibrationTimer.stop();
	if (ok) {
		makeNonDecreasing(cal.panVelocity);
		makeNonDecreasing(cal.tiltVelocity);
		setCameraCalibration({
			.maxPanSpeed = cal.maxPanSpeed,
			.maxTiltSpeed = cal.maxTiltSpeed,
			.panVelocity = cal.panVelocity,
			.tiltVelocity = cal.tiltVelocity,
			.calibratedAt = QDateTime::currentDateTime(),
		});
		reloadCalibration();
	} else {
		sendPanTiltStop();
		if (restore && cal.haveStart) {
			sendPanTiltAbsolute(cal.start, m_maxPanSpeed, m_maxTiltSpeed);
			sendZoomDirect(cal.start.zoom);
		}
	}
	emit calibrationFinished(ok, error);
}

bool CameraClient::calibrationSettled() {
	auto &cal = *m_calibration;
	if (m_ptzPos != cal.settleLast) {
		cal.settleLast = m_ptzPos;
		cal.settleTimer.start();
	}
	return cal.settleTimer.elapsed() >= CalibrationSettleMs &&
		cal.phaseTimer.elapsed() >= CalibrationMinSettleMs;
}

std::optional<double> CameraClient::calibrationVelocity() const {
	auto const &samples = m_calibration->samples;
	// ignore samples after the axis stopped changing, it hit a limit
	qsizetype end = 0;
	for (qsizetype i = 1; i < samples.size(); ++i) {
		if (samples[i].second != samples[i - 1].second) {
			end = i;
		}
	}
	auto const count = end + 1;
	if (count < CalibrationMinSamples || samples[end].first - samples[0].first < CalibrationMinSampleMs) {
		return std::nullopt;
	}
	// middle half leaves out accelerating and decelerating
	auto const &a = samples[count / 4];
	auto const &b = samples[(3 * count) / 4];
	if (b.first <= a.first) {
		return std::nullopt;
	}
	auto const velocity = std::abs(b.second - a.second) * 1000.0 / static_cast<double>(b.first - a.first);
	return velocity > 0 ? std::optional{velocity} : std::nullopt;
}

void CameraClient::emitCalibrationProgress() {
	using Phase = CalibrationJob::Phase;
	auto const &cal = *m_calibration;
	auto const stepCount = cal.maxPanSpeed + cal.maxTiltSpeed + 3;
	switch (cal.phase) {
		case Phase::Start:
		case Phase::MaxSpeed:
		case Phase::Home:
			emit calibrationProgress(0, stepCount, tr("Preparing..."));
			break;
		case Phase::Drive:
		case Phase::Stop:
		case Phase::Return:
			if (cal.afterReturn == Phase::Zoom && cal.phase == Phase::Return) {
				emit calibrationProgress(stepCount - 2, stepCount, tr("Measuring zoom..."));
			} else if (cal.tilt) {
				emit calibrationProgress(
					1 + cal.maxPanSpeed + cal.speed - 1, stepCount,
					tr("Measuring tilt speed %1 of %2...").arg(cal.speed).arg(cal.maxTiltSpeed));
			} else {
				emit calibrationProgress(
					cal.speed, stepCount,
					tr("Measuring pan speed %1 of %2...").arg(cal.speed).arg(cal.maxPanSpeed));
			}
			break;
		case Phase::Zoom:
			emit calibrationProgress(stepCount - 2, stepCount, tr("Measuring zoom..."));
			break;
		case Phase::Restore:
			emit calibrationProgress(stepCount - 1, stepCount, tr("Returning to the starting position..."));
			break;
	}
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
	sendViscaAttempt(viscaMsg, isInquiry, 0);
}

void CameraClient::sendViscaAttempt(std::span<uint8_t const> const viscaMsg, bool const isInquiry, int const attempt) {
	if (m_addr.isNull() || m_port == 0) {
		return;
	}
	auto const seq = m_sequenceNumber;
	auto const packet = createViscaPacket(viscaMsg, isInquiry);
	if (packet.size == 0) {
		return;
	}
	m_socket->writeDatagram(reinterpret_cast<char const*>(packet.data.data()), static_cast<qint64>(packet.size), m_addr, m_port);
	QByteArray const msg(reinterpret_cast<char const*>(viscaMsg.data()), static_cast<qsizetype>(viscaMsg.size()));
	if (isInquiry) {
		qCDebug(lcCameraReplies).nospace() << "inquiry seq " << seq << ": " << msg.toHex(' ');
		return;
	}
	qCDebug(lcCamera).nospace() << "command seq " << seq << ": " << msg.toHex(' ');
	auto const now = m_clock.elapsed();
	m_sentCommands.removeIf([now](auto const &it) {
		return now - it.value().sentMs > SentCommandTtlMs;
	});
	m_sentCommands.insert(seq, {.msg = msg, .attempt = attempt, .sentMs = now, .epoch = m_commandEpoch});
}

void CameraClient::handleViscaError(uint8_t const code, std::span<uint8_t const> const reply, std::optional<uint32_t> const seq) {
	auto const name = [code] {
		switch (code) {
			case 0x01: return "message length error";
			case 0x02: return "syntax error";
			case 0x03: return "command buffer full";
			case 0x04: return "command cancelled";
			case 0x05: return "no socket";
			case 0x41: return "command not executable";
			default: return "unknown error";
		}
	}();
	auto const sent = seq ? m_sentCommands.take(*seq) : SentCommand{};
	qCWarning(lcCamera).nospace() << "camera " << name << " for seq "
		<< (seq ? QString::number(*seq) : QStringLiteral("?")) << ": " << sent.msg.toHex(' ')
		<< " (reply " << QByteArray(reinterpret_cast<char const*>(reply.data()), static_cast<qsizetype>(reply.size())).toHex(' ') << ")";
	// only a busy camera is worth retrying, other errors would just fail again
	if (code != 0x03 || sent.msg.isEmpty() || sent.attempt >= MaxCommandRetries) {
		return;
	}
	QTimer::singleShot(CommandRetryMs, this, [this, sent] {
		if (sent.epoch != m_commandEpoch) {
			// the move this belonged to was cancelled
			return;
		}
		auto const bytes = std::span{reinterpret_cast<uint8_t const*>(sent.msg.constData()), static_cast<size_t>(sent.msg.size())};
		qCDebug(lcCamera) << "retrying command, attempt" << sent.attempt + 1;
		sendViscaAttempt(bytes, false, sent.attempt + 1);
	});
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
		std::optional<uint32_t> seq;
		// strip the VISCA over IP header if present, raw VISCA replies start with 0x90
		if (msg.size() >= 8 && msg[0] == 0x01) {
			// payload type 01 11 is a VISCA reply, others carry no VISCA payload
			if (msg[1] != 0x11) {
				continue;
			}
			auto const len = static_cast<size_t>((msg[2] << 8) | msg[3]);
			seq = readBigEndian<uint32_t>(msg.subspan<4, sizeof(uint32_t)>());
			msg = msg.subspan(8, std::min(len, msg.size() - 8));
		}
		handleViscaReply(msg, seq);
	}
}

void CameraClient::handleViscaReply(std::span<uint8_t const> const reply, std::optional<uint32_t> const seq) {
	if (lcCameraReplies().isDebugEnabled()) {
		qCDebug(lcCameraReplies).nospace() << "reply seq " << (seq ? QString::number(*seq) : QStringLiteral("?")) << ": "
			<< QByteArray(reinterpret_cast<char const*>(reply.data()), static_cast<qsizetype>(reply.size())).toHex(' ');
	}
	if (reply.size() < 3 || reply.back() != 0xFF) {
		return;
	}
	// errors: y0 6z ee FF
	if ((reply[1] & 0xF0) == 0x60) {
		handleViscaError(reply[2], reply, seq);
		return;
	}
	// inquiry replies: y0 50 [data] FF, command acknowledgements and completions carry no data
	if ((reply[1] & 0xF0) != 0x50) {
		return;
	}
	auto const data = reply.subspan(2, reply.size() - 3);
	if (data.size() == 2) {
		// Pan-tilt MaxSpeed: whole bytes rather than nibbles, and the only 2 byte reply
		m_maxSpeedReplyPan = data[0];
		m_maxSpeedReplyTilt = data[1];
		++m_maxSpeedGen;
		return;
	}
	if (std::ranges::any_of(data, [](uint8_t const b) { return b > 0x0F; })) {
		return;
	}
	// Replies don't say which inquiry they answer, so identify them by size,
	// and for zoom and focus, which are the same size, by sequence number.
	// The only 1 byte reply is power, since focus mode is never inquired.
	switch (data.size()) {
		case 8: // pan-tilt, 4 nibble pan
		case 9: { // pan-tilt, 5 nibble pan
			m_panNibbles = data.size() - 4;
			auto const panBits = m_panNibbles * 4;
			m_ptzPos.pan = signExtend(readNibbles(data.first(m_panNibbles)), panBits);
			m_ptzPos.tilt = signExtend(readNibbles(data.subspan(m_panNibbles)), 16);
			++m_panTiltGen;
			if (m_calibration && m_calibration->phase == CalibrationJob::Phase::Drive) {
				m_calibration->samples.append({
					m_calibration->phaseTimer.elapsed(),
					m_calibration->tilt ? m_ptzPos.tilt : m_ptzPos.pan,
				});
			}
			break;
		}
		case 4: // zoom or focus
			if (seq && m_focusInquirySeqs.remove(*seq)) {
				m_focusPos = readNibbles(data);
				++m_focusGen;
				break;
			}
			m_ptzPos.zoom = readNibbles(data);
			++m_zoomGen;
			// react immediately rather than on the next tick, a stale position overshoots the target
			if (m_ptzJob && m_ptzJob->zoomDriving) {
				updateZoomDrive();
			}
			break;
		default:
			return;
	}
	if (m_calibration) {
		m_calibration->replyTimer.start();
	}
	checkPtzQuery();
}

void CameraClient::checkPtzQuery() {
	if (m_ptzQuery &&
	    m_ptzQuery->panTiltGen != m_panTiltGen &&
	    m_ptzQuery->zoomGen != m_zoomGen &&
	    m_ptzQuery->focusGen != m_focusGen) {
		m_ptzQuery.reset();
		emit ptzPositionReceived(m_ptzPos.pan, m_ptzPos.tilt, m_ptzPos.zoom, m_focusPos);
	}
}

void CameraClient::onSocketError(QAbstractSocket::SocketError) {
	if (m_connected) {
		m_connected = false;
		emit pollFailed();
	}
}
