/*
 * Copyright 2021 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "obsclient.hpp"

OBSClient::OBSClient(QObject *parent): QObject(parent) {
	m_pollTimer.start(1000);
	connect(&m_pollTimer, &QTimer::timeout, this, &OBSClient::poll);
	connect(m_pollingNam, &QNetworkAccessManager::finished, this, &OBSClient::handlePollResponse);
}

void OBSClient::setScene(QString scene) {
	get(QString("/Scene?name=%1").arg(scene));
}

void OBSClient::showSlides() {
	setScene("MusicScene");
}

void OBSClient::hideSlides() {
	setScene("SpeakerScene");
}

void OBSClient::setSlidesVisible(int state) {
	if (state) {
		setScene("MusicScene");
	} else {
		setScene("SpeakerScene");
	}
}

void OBSClient::get(QString urlExt) {
	QUrl url(QString(BaseUrl) + urlExt);
	QNetworkRequest rqst(url);
	auto reply = m_nam->get(rqst);
	connect(reply, &QIODevice::readyRead, reply, &QObject::deleteLater);
}

void OBSClient::poll() {
	QUrl url(QString(BaseUrl) + "/ping");
	QNetworkRequest rqst(url);
	m_pollingNam->get(rqst);
}

void OBSClient::handlePollResponse(QNetworkReply *reply) {
	reply->deleteLater();
	if (reply->error()) {
		qDebug() << "OBSClient error response:" << reply->errorString();
		emit pollFailed();
		return;
	}
	emit pollUpdate();
}

