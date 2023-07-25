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

void CameraClient::setPreset(int preset) {
	if (preset > 0 && preset < MaxCameraPresets) {
		get(QString("/cgi-bin/ptzctrl.cgi?ptzcmd&poscall&%1").arg(preset));
		--preset;
		auto const vc = getVideoConfig()[preset];
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

/*
void CameraClient::getVideoConfig() {
	QUrl url(QString(m_baseUrl) + "/cgi-bin/param.cgi?get_image_conf");
	QNetworkRequest rqst(url);
	auto const reply = m_nam->get(rqst);
	connect(reply, &QIODevice::readyRead, this, [this, reply] {processVideoConfig(reply);});
}
*/

void CameraClient::setBaseUrl() {
	auto const [host, port] = getCameraConnectionData();
	m_baseUrl = QString("http://%1:%2").arg(host, QString::number(port));
}

/*
void CameraClient::processVideoConfig(QNetworkReply *reply) {
	auto constexpr brightStart = std::string_view("bright = ");
	auto constexpr saturationStart = std::string_view("saturation = ");
	auto constexpr contrastStart = std::string_view("contrast = ");
	auto constexpr sharpnessStart = std::string_view("sharpness = ");
	auto constexpr hueStart = std::string_view("hue = ");
	while (!reply->atEnd()) {
		auto const line = reply->readLine();
		if (line.startsWith(brightStart)) {
			m_videoConfig.brightness = QString(line.mid(brightStart.size())).toInt();
		} else if (line.startsWith(saturationStart)) {
			m_videoConfig.saturation = QString(line.mid(saturationStart.size())).toInt();
		} else if (line.startsWith(contrastStart)) {
			m_videoConfig.contrast = QString(line.mid(contrastStart.size())).toInt();
		} else if (line.startsWith(sharpnessStart)) {
			m_videoConfig.sharpness = QString(line.mid(sharpnessStart.size())).toInt();
		} else if (line.startsWith(hueStart)) {
			m_videoConfig.hue = QString(line.mid(hueStart.size())).toInt();
		}
	}
	qDebug() << 
		"brightness:" << m_videoConfig.brightness <<
		"saturation:" << m_videoConfig.brightness <<
		"contrast:" << m_videoConfig.brightness <<
		"sharpness:" << m_videoConfig.brightness << 
		"hue:" << m_videoConfig.hue;
}
*/

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
