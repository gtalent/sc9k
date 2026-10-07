/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QHostAddress>
#include <QHostInfo>
#include <QSettings>

#include "consts.hpp"
#include "settingsdata.hpp"
#include "cameraclient.hpp"

CameraClient::CameraClient(QObject *parent): QObject(parent), m_socket(new QUdpSocket(this)) {
	m_socket->bind(QHostAddress::AnyIPv4, 0);
	connect(m_socket, &QUdpSocket::readyRead, this, &CameraClient::onReadyRead);
	connect(m_socket, &QUdpSocket::errorOccurred, this, &CameraClient::onSocketError);
	setBaseUrl();
	poll();
	m_pollTimer.start(1000);
	connect(&m_pollTimer, &QTimer::timeout, this, &CameraClient::poll);
}

void CameraClient::setPresetVC(int const preset, VideoConfig const&vc) {
	if (preset > 0 && preset <= MaxCameraPresets) {
		// VISCA Memory Recall: 81 01 04 3F 02 pp FF
		std::array<uint8_t, 7> const cmd{
			0x81, 0x01, 0x04, 0x3F, 0x02,
			static_cast<uint8_t>(preset - 1),
			0xFF
		};
		sendVisca(cmd);
		setBrightness(vc.brightness);
		setSaturation(vc.saturation);
		setContrast(vc.contrast);
		setSharpness(vc.sharpness);
		setHue(vc.hue);
	}
}

void CameraClient::setPreset(int const preset) {
	if (preset > 0 && preset <= MaxCameraPresets) {
		// VISCA Memory Recall: 81 01 04 3F 02 pp FF
		std::array<uint8_t, 7> const cmd{
			0x81, 0x01, 0x04, 0x3F, 0x02,
			static_cast<uint8_t>(preset - 1),
			0xFF
		};
		sendVisca(cmd);
		auto const vcList = getVideoConfig();
		if (preset - 1 < vcList.size()) {
			auto const vc = vcList[preset - 1];
			setBrightness(vc.brightness);
			setSaturation(vc.saturation);
			setContrast(vc.contrast);
			setSharpness(vc.sharpness);
			setHue(vc.hue);
		}
	}
}

void CameraClient::reboot() {
	// VISCA Power Off / Standby: 81 01 04 00 03 FF
	constexpr std::array<uint8_t, 6> cmd{0x81, 0x01, 0x04, 0x00, 0x03, 0xFF};
	sendVisca(cmd);
	m_connected = false;
	m_missedPolls = 3;
	emit pollFailed();
}

void CameraClient::setBaseUrl() {
	auto const [host, port] = getCameraConnectionData();
	m_host = host;
	m_port = port;
	m_sequenceNumber = 1;
	m_missedPolls = 0;
}

void CameraClient::setBrightness(int const val) {
	if (val > -1) {
		// VISCA Brightness Direct: 81 01 04 4D 00 00 0p 0q FF
		std::array<uint8_t, 9> const cmd{
			0x81, 0x01, 0x04, 0x4D, 0x00, 0x00,
			static_cast<uint8_t>((val >> 4) & 0x0F),
			static_cast<uint8_t>(val & 0x0F),
			0xFF
		};
		sendVisca(cmd);
	}
}

void CameraClient::setSaturation(int const val) {
	if (val > -1) {
		// VISCA Color Gain Direct: 81 01 04 49 00 00 0p 0q FF
		std::array<uint8_t, 9> const cmd{
			0x81, 0x01, 0x04, 0x49, 0x00, 0x00,
			static_cast<uint8_t>((val >> 4) & 0x0F),
			static_cast<uint8_t>(val & 0x0F),
			0xFF
		};
		sendVisca(cmd);
	}
}

void CameraClient::setContrast(int const val) {
	if (val > -1) {
		// VISCA Contrast Direct: 81 01 04 A2 00 00 0p 0q FF
		std::array<uint8_t, 9> const cmd{
			0x81, 0x01, 0x04, 0xA2, 0x00, 0x00,
			static_cast<uint8_t>((val >> 4) & 0x0F),
			static_cast<uint8_t>(val & 0x0F),
			0xFF
		};
		sendVisca(cmd);
	}
}

void CameraClient::setSharpness(int const val) {
	if (val > -1) {
		// VISCA Aperture Direct: 81 01 04 42 00 00 0p 0q FF
		std::array<uint8_t, 9> const cmd{
			0x81, 0x01, 0x04, 0x42, 0x00, 0x00,
			static_cast<uint8_t>((val >> 4) & 0x0F),
			static_cast<uint8_t>(val & 0x0F),
			0xFF
		};
		sendVisca(cmd);
	}
}

void CameraClient::setHue(int const val) {
	if (val > -1) {
		// VISCA Color Hue Direct: 81 01 04 4F 00 00 0p 0q FF
		std::array<uint8_t, 9> const cmd{
			0x81, 0x01, 0x04, 0x4F, 0x00, 0x00,
			static_cast<uint8_t>((val >> 4) & 0x0F),
			static_cast<uint8_t>(val & 0x0F),
			0xFF
		};
		sendVisca(cmd);
	}
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
	if (m_host.isEmpty() || m_port == 0) {
		return;
	}
	QHostAddress addr;
	if (!addr.setAddress(m_host)) {
		auto const hostInfo = QHostInfo::fromName(m_host);
		if (!hostInfo.addresses().isEmpty()) {
			addr = hostInfo.addresses().first();
		} else {
			return;
		}
	}
	auto const packet = createViscaPacket(viscaMsg, isInquiry);
	if (packet.size > 0) {
		m_socket->writeDatagram(reinterpret_cast<char const*>(packet.data.data()), static_cast<qint64>(packet.size), addr, m_port);
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
		if (datagram.isEmpty()) {
			continue;
		}
		m_missedPolls = 0;
		if (!m_connected) {
			m_connected = true;
		}
		emit pollUpdate();
	}
}

void CameraClient::onSocketError(QAbstractSocket::SocketError) {
	if (m_connected) {
		m_connected = false;
		emit pollFailed();
	}
}
