/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QNetworkReply>
#include <QSettings>

#include "settingsdata.hpp"
#include "cameraclient.hpp"

CameraClient::CameraClient(QObject *parent): QObject(parent) {
	setBaseUrl();
	m_pollTimer.start(1000);
	connect(&m_pollTimer, &QTimer::timeout, this, &CameraClient::poll);
	connect(m_pollingNam, &QNetworkAccessManager::finished, this, &CameraClient::handlePollResponse);
}

void CameraClient::setPreset(int preset) {
	if (preset > -1) {
		get(QString("/cgi-bin/ptzctrl.cgi?ptzcmd&poscall&%1").arg(preset));
	}
}

void CameraClient::setBaseUrl() {
	const auto [host, port] = getCameraConnectionData();
	m_baseUrl = QString("http://%1:%2").arg(host, QString::number(port));
}

void CameraClient::get(QString const&urlExt) {
	QUrl url(QString(m_baseUrl) + urlExt);
	QNetworkRequest rqst(url);
	auto reply = m_nam->get(rqst);
	connect(reply, &QIODevice::readyRead, reply, &QObject::deleteLater);
}

void CameraClient::poll() {
	QUrl url(QString(m_baseUrl) + "/cgi-bin/param.cgi?get_device_conf");
	QNetworkRequest rqst(url);
	m_pollingNam->get(rqst);
}

void CameraClient::handlePollResponse(QNetworkReply *reply) {
	reply->deleteLater();
	if (reply->error()) {
		qDebug() << "CameraClient error response:" << reply->errorString();
		emit pollFailed();
		return;
	}
	emit pollUpdate();
}
