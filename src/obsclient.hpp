/*
 * Copyright 2021 gary@drinkingtea.net
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
	   const QString BaseUrl = QString("http://") + SlideHost + ":9302";
		QNetworkAccessManager *m_nam = new QNetworkAccessManager(this);
		QNetworkAccessManager *m_pollingNam = new QNetworkAccessManager(this);
		QTimer m_pollTimer;

   public:
		explicit OBSClient(QObject *parent = nullptr);

   public slots:
		void setScene(QString scene);

		void showSlides();

		void hideSlides();

		void setSlidesVisible(int state);

   private:
		void get(QString url);

		void poll();

		void handlePollResponse(QNetworkReply *reply);

	signals:
		void pollUpdate();

		void pollFailed();

};

