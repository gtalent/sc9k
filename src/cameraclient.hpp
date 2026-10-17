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

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QSet>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>

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
		// for retrying commands the camera rejects because both its command slots are busy
		struct SentCommand {
			QByteArray msg;
			int attempt = 0;
			qint64 sentMs = 0;
			uint64_t epoch = 0;
		};
		QHash<uint32_t, SentCommand> m_sentCommands;
		// incremented when a move is cancelled, so its commands aren't retried afterwards
		uint64_t m_commandEpoch = 0;
		QElapsedTimer m_clock;
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
		// pan/tilt speed characteristics, from calibration or the defaults
		int m_maxPanSpeed = 24;
		int m_maxTiltSpeed = 20;
		QVector<double> m_panVelocity;
		QVector<double> m_tiltVelocity;
		// last reply to the Pan-tilt MaxSpeed Inquiry
		int m_maxSpeedReplyPan = 0;
		int m_maxSpeedReplyTilt = 0;
		uint64_t m_maxSpeedGen = 0;

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
			// speeds of the step's Pan-tilt Absolute, for resending it, 0 if none was sent
			int panSpeedSent = 0;
			int tiltSpeedSent = 0;
			int panTiltResends = 0;
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
		// spaces out zoom drive inquiries
		QTimer m_zoomInquiryTimer;

		struct PtzQuery {
			uint64_t panTiltGen = 0;
			uint64_t zoomGen = 0;
			uint64_t focusGen = 0;
		};
		std::optional<PtzQuery> m_ptzQuery;
		int m_ptzQueryId = 0;

		struct CalibrationJob {
			enum class Phase {
				// record the starting position, to return to at the end
				Start,
				MaxSpeed,
				Home,
				// measure the speed of one axis at one VISCA speed
				Drive,
				Stop,
				// back to home before the next measurement
				Return,
				Zoom,
				Restore,
			};
			Phase phase = Phase::Start;
			// phase to enter once Return reaches home
			Phase afterReturn = Phase::Drive;
			QElapsedTimer phaseTimer;
			// time since the last position reply, for detecting the camera going quiet
			QElapsedTimer replyTimer;
			uint64_t seenPanTiltGen = 0;
			uint64_t seenZoomGen = 0;
			uint64_t seenMaxSpeedGen = 0;
			bool haveStart = false;
			PtzPosition start;
			PtzPosition home;
			int maxPanSpeed = 24;
			int maxTiltSpeed = 20;
			// axis and speed being measured
			bool tilt = false;
			int speed = 1;
			bool reverse = false;
			bool retried = false;
			qint64 driveMs = 0;
			// position of the measured axis while driving, with time since the drive started
			QVector<QPair<qint64, int>> samples;
			QVector<double> panVelocity{0};
			QVector<double> tiltVelocity{0};
			// for detecting when the camera has stopped moving
			PtzPosition settleLast;
			QElapsedTimer settleTimer;
		};
		std::optional<CalibrationJob> m_calibration;
		QTimer m_calibrationTimer;

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

		/**
		 * Measures the camera's pan/tilt speeds and zoom scale, moving the camera.
		 * Emits calibrationProgress as it goes, and calibrationFinished at the end.
		 */
		void startCalibration();

		/**
		 * Stops calibration and returns the camera to where it started.
		 */
		void cancelCalibration();

	public slots:
		void reloadConnectionSettings();

		void reloadCalibration();

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
		 * Sends the next zoom drive inquiry, no sooner than the camera can report a new position.
		 */
		void scheduleZoomDriveInquiry();

		[[nodiscard]]
		static char const *ptzStepName(PtzStep step);

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

		/**
		 * @param zoom reported zoom position, converted to the camera's Zoom Direct scale
		 */
		void sendZoomDirect(int zoom);

		void sendZoomDirectValue(int val);

		void sendPanTiltDrive(bool tilt, int speed, bool reverse);

		void sendPanTiltHome();

		void sendMaxSpeedInquiry();

		void calibrationTick();

		void enterCalibrationPhase(CalibrationJob::Phase phase);

		/**
		 * Handles a speed measurement once the camera has stopped.
		 */
		void finishCalibrationMeasurement();

		/**
		 * @param restore return the camera to where calibration started, if it failed
		 */
		void finishCalibration(bool ok, QString const &error, bool restore = true);

		/**
		 * @return true if the camera appears to have stopped moving
		 */
		[[nodiscard]]
		bool calibrationSettled();

		/**
		 * @return velocity in position units per second, or empty if there's too little movement
		 */
		[[nodiscard]]
		std::optional<double> calibrationVelocity() const;

		void emitCalibrationProgress();

		void sendZoomVariable(bool out, int speed);

		void sendZoomStop();

		/**
		 * @param seq sequence number from the VISCA over IP header, if there was one
		 */
		void handleViscaReply(std::span<uint8_t const> reply, std::optional<uint32_t> seq);

		void checkPtzQuery();

		void onHostLookup(class QHostInfo const &hostInfo);

		void sendVisca(std::span<uint8_t const> viscaMsg, bool isInquiry = false);

		/**
		 * @param attempt number of times the command was rejected and retried
		 */
		void sendViscaAttempt(std::span<uint8_t const> viscaMsg, bool isInquiry, int attempt);

		/**
		 * Handles an error reply, retrying the command if the camera was too busy for it.
		 */
		void handleViscaError(uint8_t code, std::span<uint8_t const> reply, std::optional<uint32_t> seq);

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

		void calibrationProgress(int step, int stepCount, QString const &description);

		void calibrationFinished(bool ok, QString const &error);

};

