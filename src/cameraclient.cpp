/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QNetworkReply>
#include <QSettings>
#include <string_view>

#include "settingsdata.hpp"
#include "cameraclient.hpp"

CameraClient::CameraClient(QObject *parent): QObject(parent) {
	setBaseUrl();
	m_pollTimer.start(1000);
	connect(&m_pollTimer, &QTimer::timeout, this, &CameraClient::poll);
	connect(m_pollingNam, &QNetworkAccessManager::finished, this, &CameraClient::handlePollResponse);
}

void CameraClient::setPresetVC(int preset, VideoConfig const&vc) {
	if (preset > 0 && preset < MaxCameraPresets) {
		get(QString("/cgi-bin/ptzctrl.cgi?ptzcmd&poscall&%1").arg(preset));
		setBrightness(vc.brightness);
		setSaturation(vc.saturation);
		setContrast(vc.contrast);
		setSharpness(vc.sharpness);
		setHue(vc.hue);
	}
}

void CameraClient::setPreset(int preset) {
	if (preset > 0 && preset < MaxCameraPresets) {
		get(QString("/cgi-bin/ptzctrl.cgi?ptzcmd&poscall&%1").arg(preset));
		auto const vc = getVideoConfig()[preset - 1];
		setBrightness(vc.brightness);
		setSaturation(vc.saturation);
		setContrast(vc.contrast);
		setSharpness(vc.sharpness);
		setHue(vc.hue);
	}
}

void CameraClient::setBrightness(int val) {
	if (val > -1) {
		get(QString("/cgi-bin/ptzctrl.cgi?post_image_value&bright&%1").arg(val));
	}
}

void CameraClient::setSaturation(int val) {
	if (val > -1) {
		get(QString("/cgi-bin/ptzctrl.cgi?post_image_value&saturation&%1").arg(val));
	}
}

void CameraClient::setContrast(int val) {
	if (val > -1) {
		get(QString("/cgi-bin/ptzctrl.cgi?post_image_value&contrast&%1").arg(val));
	}
}

void CameraClient::setSharpness(int val) {
	if (val > -1) {
		get(QString("/cgi-bin/ptzctrl.cgi?post_image_value&sharpness&%1").arg(val));
	}
}

void CameraClient::setHue(int val) {
	if (val > -1) {
		get(QString("/cgi-bin/ptzctrl.cgi?post_image_value&hue&%1").arg(val));
	}
}

void CameraClient::setBaseUrl() {
	auto const [host, port] = getCameraConnectionData();
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
