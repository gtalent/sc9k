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

class CameraClient: public QObject {
	Q_OBJECT
	private:
		QString m_baseUrl;
		QNetworkAccessManager *const m_nam = new QNetworkAccessManager(this);
		QNetworkAccessManager *const m_pollingNam = new QNetworkAccessManager(this);
		QTimer m_pollTimer;

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

		void get(QString const&url);

		void post(QString const&url);

		void poll();

		void handlePollResponse(QNetworkReply *reply);

	signals:
		void pollUpdate();

		void pollFailed();

};

