/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUdpSocket>

constexpr size_t MaxMsgSz = 32;

struct ViscaMsg {
	std::array<uint8_t, MaxMsgSz> data{};
	size_t size{};
};

class CameraClient: public QObject {
	Q_OBJECT
	private:
		QString m_host;
		uint16_t m_port = 52381;
		QUdpSocket *const m_socket = nullptr;
		QTimer m_pollTimer;
		uint32_t m_sequenceNumber = 1;
		int m_missedPolls = 0;
		bool m_connected = false;

	public:
		explicit CameraClient(QObject *parent = nullptr);

		void setPresetVC(int preset, struct VideoConfig const&vc);

		void setPreset(int preset);

		void reboot();

	public slots:
		void setBaseUrl();

	private:
		void setBrightness(int val);

		void setSaturation(int val);

		void setContrast(int val);

		void setSharpness(int val);

		void setHue(int val);

		void sendVisca(std::span<uint8_t const> viscaMsg, bool isInquiry = false);

		[[nodiscard]]
		ViscaMsg createViscaPacket(std::span<uint8_t const> viscaMsg, bool isInquiry = false);

		void poll();

	private slots:
		void onReadyRead();

		void onSocketError(QAbstractSocket::SocketError socketError);

	signals:
		void pollUpdate();

		void pollFailed();

};

