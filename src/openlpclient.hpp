/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWebSocket>

#include "consts.hpp"

class OpenLPClient: public QObject {
	Q_OBJECT

	private:
		QString m_host;
		uint16_t m_httpPort = 4316;
		uint16_t m_wsPort = 4317;
		QString m_baseUrl;

		QNetworkAccessManager *m_nam = new QNetworkAccessManager(this);
		QWebSocket m_ws;
		QTimer m_reconnectTimer;
		QTimer m_pingTimer;
		bool m_connected = false;
		bool m_pongReceived = false;

		QStringList m_songList;
		// service item IDs, in the same order as m_songList
		QStringList m_songIdList;
		QStringList m_currentSlideList;
		QStringList m_currentTagList;
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

		void reloadConnectionSettings();

	private:
		void post(QString const &urlExt, QString const &data);

		void requestSongList();

		void requestSlideList();

		void resetState();

		void emitUpdate();

	private slots:
		void connectToOpenLP();

		void onWsConnected();

		void onWsDisconnected();

		void onWsMessage(QByteArray const &message);

	signals:
		void pollUpdate(int songIdx, int slideNum);

		void pollFailed();

		void songListUpdate(QStringList);

		void slideListUpdate(QStringList, QStringList);

		void songChanged(QString);
};
