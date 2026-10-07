/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

#include "consts.hpp"

class OpenLPClient: public QObject {
	Q_OBJECT

	private:
		QString m_host;
		uint16_t m_httpPort = 4316;
		uint16_t m_wsPort = 4317;
		QString m_baseUrl;

		QNetworkAccessManager *m_nam = new QNetworkAccessManager(this);
		QNetworkAccessManager *m_pollingNam = new QNetworkAccessManager(this);
		QNetworkAccessManager *m_songListNam = new QNetworkAccessManager(this);
		QNetworkAccessManager *m_slideListNam = new QNetworkAccessManager(this);
		QTimer m_pollTimer;

		QTcpSocket *m_wsSocket = nullptr;
		QByteArray m_wsReadBuffer;
		QByteArray m_secWebSocketKey;
		bool m_wsConnected = false;

		QHash<QString, QString> m_songNameMap;
		QStringList m_songList;
		int m_currentServiceId = -1;
		QString m_currentSongId;
		int m_currentSlide = 0;

	public:
		explicit OpenLPClient(QObject *parent = nullptr);

		[[nodiscard]]
		QString getNextSong();

	public slots:
		void nextSlide();

		void prevSlide();

		void nextSong();

		void prevSong();

		void blankScreen();

		void showSlides();

		void setSlidesVisible(bool value);

		void changeSong(int it);

		void changeSlide(int slide);

		void setBaseUrl();

	private:
		void get(QString const&url);

		void post(QString const&url, QString const&data);

		void requestSongList();

		void requestSlideList();

		void connectWebSocket();

		void sendWebSocketHandshake();

		void processWebSocketHandshake();

		void processWebSocketFrames();

		void handleWebSocketMessage(QString const&message);

		void sendWebSocketPong(QByteArray const&payload);

		void handleStateJson(QJsonObject const&results);

	private slots:
		void poll();

		void onWsConnected();

		void onWsDisconnected();

		void onWsReadyRead();

		void onWsErrorOccurred(QAbstractSocket::SocketError socketError);

		void handleGeneralResponse(QNetworkReply *reply);

		void handlePollResponse(QNetworkReply *reply);

		void handleSongListResponse(QNetworkReply *reply);

		void handleSlideListResponse(QNetworkReply *reply);

	signals:
		void pollUpdate(QString songId, int slideNum);

		void pollFailed();

		void songListUpdate(QStringList);

		void slideListUpdate(QStringList, QStringList);

		void songChanged(QString);
};

