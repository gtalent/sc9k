/*
 * Copyright 2021 - 2024 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

#include "settingsdata.hpp"
#include "obsclient.hpp"

OBSClient::OBSClient(QObject *parent): QObject(parent) {
	setBaseUrl();
	m_pollTimer.start(1000);
	connect(&m_pollTimer, &QTimer::timeout, this, &OBSClient::poll);
	connect(m_pollingNam, &QNetworkAccessManager::finished, this, &OBSClient::handlePollResponse);
}

void OBSClient::setScene(QString const&scene) {
	get(QString("/Scene?name=%1").arg(scene));
}

void OBSClient::showSlides() {
	setScene(SceneSlides);
}

void OBSClient::hideSlides() {
	setScene(SceneNoSlides);
}

void OBSClient::setSlidesVisible(bool state) {
	if (state) {
		setScene(SceneSlides);
	} else {
		setScene(SceneNoSlides);
	}
}

void OBSClient::setBaseUrl() {
	auto const [host, port] = getOBSConnectionData();
	m_baseUrl = QString("http://%1:%2").arg(host, QString::number(port));
}

void OBSClient::get(QString const&urlExt) {
	QUrl url(QString(m_baseUrl) + urlExt);
	QNetworkRequest rqst(url);
	auto reply = m_nam->get(rqst);
	connect(reply, &QIODevice::readyRead, reply, &QObject::deleteLater);
}

void OBSClient::poll() {
	QUrl url(QString(m_baseUrl) + "/ping");
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

