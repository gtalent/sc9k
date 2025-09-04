/*
 * Copyright 2021 - 2024 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

#include "consts.hpp"

class OBSClient: public QObject {
	Q_OBJECT
	private:
		static constexpr auto SceneSlides = "MusicScene";
		static constexpr auto SceneNoSlides = "SpeakerScene";
		QString m_baseUrl;
		QNetworkAccessManager *m_nam = new QNetworkAccessManager(this);
		QNetworkAccessManager *m_pollingNam = new QNetworkAccessManager(this);
		QTimer m_pollTimer;

	public:
		explicit OBSClient(QObject *parent = nullptr);

	public slots:
		void setScene(QString const&scene);

		void showSlides();

		void hideSlides();

		void setSlidesVisible(bool state);

		void setBaseUrl();

	private:
		void get(QString const&url);

		void poll();

		void handlePollResponse(QNetworkReply *reply);

	signals:
		void pollUpdate();

		void pollFailed();

};

