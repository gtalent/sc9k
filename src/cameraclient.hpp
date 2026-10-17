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
#include <optional>
#include <span>

#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUdpSocket>

constexpr size_t MaxMsgSz = 32;

struct ViscaMsg {
	std::array<uint8_t, MaxMsgSz> data{};
	size_t size{};
};

struct PtzPosition {
	int pan{};
	int tilt{};
	int zoom{};
	bool operator==(PtzPosition const&) const = default;
};

class CameraClient: public QObject {
	Q_OBJECT
	private:
		QString m_host;
		QHostAddress m_addr;
		int m_lookupId = -1;
		uint16_t m_port = 52381;
		QUdpSocket *const m_socket = nullptr;
		QTimer m_pollTimer;
		uint32_t m_sequenceNumber = 1;
		int m_missedPolls = 0;
		bool m_connected = false;
		// last reported position, generation counters increment on each reply
		PtzPosition m_ptzPos;
		uint64_t m_panTiltGen = 0;
		uint64_t m_zoomGen = 0;
		// pan is 4 nibbles on most cameras, 5 on some, detected from inquiry replies
		size_t m_panNibbles = 4;
		// reported zoom position / Zoom Direct value, learned from the camera
		double m_zoomDirectScale = 1.0;

		enum class PtzStep {
			Locate,
			Zoom,
			PanTilt,
		};
		struct PtzJob {
			PtzPosition target;
			int panTiltSpeed = 1;
			int zoomSpeed = 0;
			std::array<PtzStep, 3> steps{PtzStep::Locate, PtzStep::Zoom, PtzStep::PanTilt};
			size_t stepIdx = 0;
			bool zoomDriving = false;
			bool zoomOut = false;
			// start of the current zoom velocity measurement window
			qint64 zoomAnchorMs = -1;
			int zoomAnchorVal = 0;
			// zoom units per ms toward the target
			double zoomVelocity = 0;
			qint64 zoomInquiryMs = 0;
			// value sent with the step's last Zoom Direct, -1 if none
			int zoomDirectSent = -1;
			// times out the step
			QElapsedTimer stepTimer;
			// time since last move command, movement may not have started before MinSettleMs
			QElapsedTimer moveTimer;
			uint64_t seenPanTiltGen = 0;
			uint64_t seenZoomGen = 0;
			PtzPosition lastSample;
			// time since the position last changed
			QElapsedTimer lastChangeTimer;
		};
		std::optional<PtzJob> m_ptzJob;
		QTimer m_ptzTimer;

		struct PtzQuery {
			uint64_t panTiltGen = 0;
			uint64_t zoomGen = 0;
		};
		std::optional<PtzQuery> m_ptzQuery;
		int m_ptzQueryId = 0;

	public:
		explicit CameraClient(QObject *parent = nullptr);

		void applyPreset(int preset, struct CameraPreset const &cp);

		void setPreset(int preset);

		void powerOn();

		void powerOff();

		void reboot();

		[[nodiscard]]
		bool connected() const noexcept;

		/**
		 * Requests the current pan, tilt, and zoom position.
		 * Emits ptzPositionReceived or ptzPositionQueryFailed.
		 */
		void queryPtzPosition();

	public slots:
		void reloadConnectionSettings();

	private:
		void setBrightness(int val);

		void setSaturation(int val);

		void setContrast(int val);

		void setSharpness(int val);

		void setHue(int val);

		void sendDirect(uint8_t opcode, int val);

		void startPtzJob(PtzPosition const &target);

		void cancelPtzJob();

		void ptzTick();

		void startPtzStep();

		void advancePtzStep();

		[[nodiscard]]
		bool zoomStepDone();

		/**
		 * Handles a zoom reply while driving the zoom at variable speed.
		 */
		void updateZoomDrive();

		void finishZoomDrive();

		void sendZoomDriveInquiry();

		/**
		 * Updates the Zoom Direct scale from where the camera settled after a Zoom Direct.
		 */
		void calibrateZoomDirect();

		[[nodiscard]]
		bool panTiltStepDone();

		/**
		 * Records the current position as a sample.
		 * @return true if the camera appears to have stopped moving
		 */
		[[nodiscard]]
		bool ptzStable();

		void sendPanTiltInquiry();

		void sendZoomInquiry();

		void sendPanTiltAbsolute(PtzPosition const &pos, int speed);

		void sendPanTiltStop();

		void sendZoomDirect(int zoom);

		void sendZoomVariable(bool out, int speed);

		void sendZoomStop();

		void handleViscaReply(std::span<uint8_t const> reply);

		void checkPtzQuery();

		void onHostLookup(class QHostInfo const &hostInfo);

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

		void ptzPositionReceived(int pan, int tilt, int zoom);

		void ptzPositionQueryFailed();

};

