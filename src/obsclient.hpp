/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

class OBSClient: public QObject {
	Q_OBJECT
	private:
		enum class State {
			Disconnected,
			Connecting,
			Handshaking,
			Identifying,
			Connected
		};

		static constexpr auto SceneSlides = "SlidesScene";
		static constexpr auto SceneNoSlides = "NoSlidesScene";

		QString m_host;
		uint16_t m_port = 4455;
		QString m_password;

		QTcpSocket *m_socket = nullptr;
		QTimer m_pollTimer;
		State m_state = State::Disconnected;
		QByteArray m_readBuffer;
		QByteArray m_secWebSocketKey;
		quint64 m_requestId = 0;

	public:
		explicit OBSClient(QObject *parent = nullptr);

	public slots:
		void setScene(QString const &scene);
		void showSlides();
		void hideSlides();
		void setSlidesVisible(bool state);
		void setBaseUrl();

	private slots:
		void onConnected();
		void onDisconnected();
		void onReadyRead();
		void onErrorOccurred(QAbstractSocket::SocketError socketError);
		void poll();

	private:
		void connectToHost();
		void sendHandshake();
		void processHandshake();
		void processFrames();
		void handleMessage(QString const &message);
		void sendTextFrame(QString const &text);
		void sendPong(QByteArray const &payload);

	signals:
		void pollUpdate();
		void pollFailed();
};

