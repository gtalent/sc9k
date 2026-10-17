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
#include <QSet>
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
		int m_focusPos = 0;
		uint64_t m_focusGen = 0;
		// Focus and zoom replies are the same size, so focus replies are told apart by
		// sequence number. Pan-tilt replies don't echo it reliably, but their size is unique.
		QSet<uint32_t> m_focusInquirySeqs;
		// pan is 4 nibbles on most cameras, 5 on some, detected from inquiry replies
		size_t m_panNibbles = 4;
		// reported zoom position / Zoom Direct value, learned from the camera
		double m_zoomDirectScale = 1.0;

		enum class PtzStep {
			Locate,
			Zoom,
			PanTilt,
			// set manual focus once the zoom has settled
			Focus,
		};
		struct PtzAxisMotion {
			// time since last move command, movement may not have started before MinSettleMs
			QElapsedTimer moveTimer;
			// time since the position last changed
			QElapsedTimer changeTimer;
			PtzPosition last;
		};
		struct PtzJob {
			PtzPosition target;
			// manual focus position, autofocus if empty
			std::optional<int> focus;
			int panTiltSpeed = 1;
			int zoomSpeed = 0;
			std::array<PtzStep, 4> steps{PtzStep::Locate, PtzStep::Zoom, PtzStep::PanTilt, PtzStep::Focus};
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
			// zoom drive timing, which can continue into the next step
			QElapsedTimer jobTimer;
			// times out the step
			QElapsedTimer stepTimer;
			// position when the step started, for measuring its progress
			PtzPosition stepStart;
			uint64_t seenPanTiltGen = 0;
			uint64_t seenZoomGen = 0;
			// zoom and pan/tilt can move at the same time, so track them separately
			PtzAxisMotion zoomMotion;
			PtzAxisMotion panTiltMotion;
		};
		std::optional<PtzJob> m_ptzJob;
		QTimer m_ptzTimer;

		struct PtzQuery {
			uint64_t panTiltGen = 0;
			uint64_t zoomGen = 0;
			uint64_t focusGen = 0;
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
		 * Requests the current pan, tilt, zoom, and focus position.
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

		void startPtzJob(PtzPosition const &target, std::optional<int> focus);

		void cancelPtzJob();

		void ptzTick();

		void startPtzStep();

		void advancePtzStep();

		/**
		 * Ends the job, unless a zoom drive from an earlier step is still running.
		 */
		void finishPtzJob();

		[[nodiscard]]
		bool hasNextPtzMove() const;

		[[nodiscard]]
		bool zoomStepDone();

		/**
		 * @return true if the zoom is close enough to its target to start the next move
		 */
		[[nodiscard]]
		bool zoomNearlyDone() const;

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
		 * @return true if pan/tilt is close enough to its target to start the next move
		 */
		[[nodiscard]]
		bool panTiltNearlyDone() const;

		[[nodiscard]]
		bool zoomStable();

		[[nodiscard]]
		bool panTiltStable();

		/**
		 * Records whether the axis moved since the last sample.
		 * @return true if the axis appears to have stopped moving
		 */
		[[nodiscard]]
		bool axisStable(PtzAxisMotion &motion, bool changed) const;

		void sendPanTiltInquiry();

		void sendZoomInquiry();

		void sendFocusInquiry();

		void sendFocusAuto();

		void sendFocusManual();

		void sendFocusDirect(int focus);

		void sendPanTiltAbsolute(PtzPosition const &pos, int panSpeed, int tiltSpeed);

		void sendPanTiltStop();

		void sendZoomDirect(int zoom);

		void sendZoomVariable(bool out, int speed);

		void sendZoomStop();

		/**
		 * @param seq sequence number from the VISCA over IP header, if there was one
		 */
		void handleViscaReply(std::span<uint8_t const> reply, std::optional<uint32_t> seq);

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

		/**
		 * @param focus -1 if the camera didn't report it
		 */
		void ptzPositionReceived(int pan, int tilt, int zoom, int focus);

		void ptzPositionQueryFailed();

};

