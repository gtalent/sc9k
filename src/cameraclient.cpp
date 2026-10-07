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

void CameraClient::setPresetVC(int preset, VideoConfig const&vc) {
	if (preset > 0 && preset <= MaxCameraPresets) {
		// VISCA Memory Recall: 81 01 04 3F 02 pp FF
		QByteArray cmd;
		cmd.append(static_cast<char>(0x81));
		cmd.append(static_cast<char>(0x01));
		cmd.append(static_cast<char>(0x04));
		cmd.append(static_cast<char>(0x3F));
		cmd.append(static_cast<char>(0x02));
		cmd.append(static_cast<char>(preset - 1));
		cmd.append(static_cast<char>(0xFF));
		sendVisca(cmd);

		setBrightness(vc.brightness);
		setSaturation(vc.saturation);
		setContrast(vc.contrast);
		setSharpness(vc.sharpness);
		setHue(vc.hue);
	}
}

void CameraClient::setPreset(int preset) {
	if (preset > 0 && preset <= MaxCameraPresets) {
		// VISCA Memory Recall: 81 01 04 3F 02 pp FF
		QByteArray cmd;
		cmd.append(static_cast<char>(0x81));
		cmd.append(static_cast<char>(0x01));
		cmd.append(static_cast<char>(0x04));
		cmd.append(static_cast<char>(0x3F));
		cmd.append(static_cast<char>(0x02));
		cmd.append(static_cast<char>(preset - 1));
		cmd.append(static_cast<char>(0xFF));
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
	QByteArray cmd;
	cmd.append(static_cast<char>(0x81));
	cmd.append(static_cast<char>(0x01));
	cmd.append(static_cast<char>(0x04));
	cmd.append(static_cast<char>(0x00));
	cmd.append(static_cast<char>(0x03));
	cmd.append(static_cast<char>(0xFF));
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

void CameraClient::setBrightness(int val) {
	if (val > -1) {
		// VISCA Brightness Direct: 81 01 04 4D 00 00 0p 0q FF
		QByteArray cmd;
		cmd.append(static_cast<char>(0x81));
		cmd.append(static_cast<char>(0x01));
		cmd.append(static_cast<char>(0x04));
		cmd.append(static_cast<char>(0x4D));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>((val >> 4) & 0x0F));
		cmd.append(static_cast<char>(val & 0x0F));
		cmd.append(static_cast<char>(0xFF));
		sendVisca(cmd);
	}
}

void CameraClient::setSaturation(int val) {
	if (val > -1) {
		// VISCA Color Gain Direct: 81 01 04 49 00 00 0p 0q FF
		QByteArray cmd;
		cmd.append(static_cast<char>(0x81));
		cmd.append(static_cast<char>(0x01));
		cmd.append(static_cast<char>(0x04));
		cmd.append(static_cast<char>(0x49));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>((val >> 4) & 0x0F));
		cmd.append(static_cast<char>(val & 0x0F));
		cmd.append(static_cast<char>(0xFF));
		sendVisca(cmd);
	}
}

void CameraClient::setContrast(int val) {
	if (val > -1) {
		// VISCA Contrast Direct: 81 01 04 A2 00 00 0p 0q FF
		QByteArray cmd;
		cmd.append(static_cast<char>(0x81));
		cmd.append(static_cast<char>(0x01));
		cmd.append(static_cast<char>(0x04));
		cmd.append(static_cast<char>(0xA2));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>((val >> 4) & 0x0F));
		cmd.append(static_cast<char>(val & 0x0F));
		cmd.append(static_cast<char>(0xFF));
		sendVisca(cmd);
	}
}

void CameraClient::setSharpness(int val) {
	if (val > -1) {
		// VISCA Aperture Direct: 81 01 04 42 00 00 0p 0q FF
		QByteArray cmd;
		cmd.append(static_cast<char>(0x81));
		cmd.append(static_cast<char>(0x01));
		cmd.append(static_cast<char>(0x04));
		cmd.append(static_cast<char>(0x42));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>((val >> 4) & 0x0F));
		cmd.append(static_cast<char>(val & 0x0F));
		cmd.append(static_cast<char>(0xFF));
		sendVisca(cmd);
	}
}

void CameraClient::setHue(int val) {
	if (val > -1) {
		// VISCA Color Hue Direct: 81 01 04 4F 00 00 0p 0q FF
		QByteArray cmd;
		cmd.append(static_cast<char>(0x81));
		cmd.append(static_cast<char>(0x01));
		cmd.append(static_cast<char>(0x04));
		cmd.append(static_cast<char>(0x4F));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>(0x00));
		cmd.append(static_cast<char>((val >> 4) & 0x0F));
		cmd.append(static_cast<char>(val & 0x0F));
		cmd.append(static_cast<char>(0xFF));
		sendVisca(cmd);
	}
}

QByteArray CameraClient::createViscaPacket(QByteArray const&viscaMsg, bool isInquiry) {
	QByteArray packet;
	packet.reserve(8 + viscaMsg.size());

	// Payload type: 0x01 0x00 for command, 0x01 0x10 for inquiry
	if (isInquiry) {
		packet.append(static_cast<char>(0x01));
		packet.append(static_cast<char>(0x10));
	} else {
		packet.append(static_cast<char>(0x01));
		packet.append(static_cast<char>(0x00));
	}

	// Payload length (16-bit big-endian)
	auto const len = static_cast<quint16>(viscaMsg.size());
	packet.append(static_cast<char>((len >> 8) & 0xFF));
	packet.append(static_cast<char>(len & 0xFF));

	// Sequence number (32-bit big-endian)
	auto const seq = m_sequenceNumber++;
	packet.append(static_cast<char>((seq >> 24) & 0xFF));
	packet.append(static_cast<char>((seq >> 16) & 0xFF));
	packet.append(static_cast<char>((seq >> 8) & 0xFF));
	packet.append(static_cast<char>(seq & 0xFF));

	// Payload
	packet.append(viscaMsg);
	return packet;
}

void CameraClient::sendVisca(QByteArray const&viscaMsg, bool isInquiry) {
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
	m_socket->writeDatagram(packet, addr, m_port);
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
	QByteArray inq;
	inq.append(static_cast<char>(0x81));
	inq.append(static_cast<char>(0x09));
	inq.append(static_cast<char>(0x04));
	inq.append(static_cast<char>(0x00));
	inq.append(static_cast<char>(0xFF));
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
