/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QSettings>

#include "consts.hpp"
#include "settingsdata.hpp"

void setVideoConfig(QSettings &settings, QVector<VideoConfig> const&vcList) {
	settings.beginGroup("Camera");
	settings.beginWriteArray("VideoImageConfig");
	for (auto i = 0; auto const&vc : vcList) {
		settings.setArrayIndex(i);
		settings.setValue("brightness", vc.brightness);
		settings.setValue("saturation", vc.saturation);
		settings.setValue("contrast", vc.contrast);
		settings.setValue("sharpness", vc.sharpness);
		settings.setValue("hue", vc.hue);
		++i;
	}
	settings.endArray();
	settings.endGroup();
}

void setVideoConfig(QVector<VideoConfig> const&vcList) {
	QSettings s;
	setVideoConfig(s, vcList);
}

QVector<VideoConfig> getVideoConfig(QSettings &settings) {
	QVector<VideoConfig> vc(MaxCameraPresets);
	settings.beginGroup("Camera");
	auto const size = std::min(settings.beginReadArray("VideoImageConfig"), MaxCameraPresets);
	for (auto i = 0; i < size; ++i) {
		settings.setArrayIndex(i);
		vc[i] = {
			.brightness = settings.value("brightness").toInt(),
			.saturation = settings.value("saturation").toInt(),
			.contrast = settings.value("contrast").toInt(),
			.sharpness = settings.value("sharpness").toInt(),
			.hue = settings.value("hue").toInt(),
		};
	}
	settings.endArray();
	settings.endGroup();
	return vc;
}

QVector<VideoConfig> getVideoConfig() {
	QSettings s;
	return getVideoConfig(s);
}

void setCameraConnectionData(QSettings &settings, ConnectionData const&cd) {
	settings.beginGroup("CameraClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

void setOpenLPConnectionData(QSettings &settings, ConnectionData const&cd) {
	settings.beginGroup("OpenLPClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

void setOBSConnectionData(QSettings &settings, ConnectionData const&cd) {
	settings.beginGroup("OBSClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

ConnectionData getCameraConnectionData(QSettings &settings) {
	ConnectionData out;
	settings.beginGroup("CameraClient");
	out.host = settings.value("Host", "192.168.100.88").toString();
	out.port = static_cast<uint16_t>(settings.value("Port", 80).toInt());
	settings.endGroup();
	return out;
}

ConnectionData getOpenLPConnectionData(QSettings &settings) {
	ConnectionData out;
	settings.beginGroup("OpenLPClient");
	out.host = settings.value("Host", "127.0.0.1").toString();
	out.port = static_cast<uint16_t>(settings.value("Port", 4316).toInt());
	settings.endGroup();
	return out;
}

ConnectionData getOBSConnectionData(QSettings &settings) {
	ConnectionData out;
	settings.beginGroup("OBSClient");
	out.host = settings.value("Host", "127.0.0.1").toString();
	out.port = static_cast<uint16_t>(settings.value("Port", 9302).toInt());
	settings.endGroup();
	return out;
}

void setCameraConnectionData(ConnectionData const&cd) {
	QSettings settings;
	settings.beginGroup("CameraClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

void setOpenLPConnectionData(ConnectionData const&cd) {
	QSettings settings;
	settings.beginGroup("OpenLPClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

void setOBSConnectionData(ConnectionData const&cd) {
	QSettings settings;
	settings.beginGroup("OBSClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

ConnectionData getCameraConnectionData() {
	QSettings s;
	return getCameraConnectionData(s);
}

ConnectionData getOpenLPConnectionData() {
	QSettings s;
	return getOpenLPConnectionData(s);
}

ConnectionData getOBSConnectionData() {
	QSettings s;
	return getOBSConnectionData(s);
}


void setViews(QSettings &settings, QVector<View> const&views) {
	settings.beginGroup("Views");
	settings.beginWriteArray("Views");
	for (auto i = 0; auto const&view : views) {
		settings.setArrayIndex(i);
		settings.setValue("Name", view.name);
		settings.setValue("Slides", view.slides);
		settings.setValue("ObsSlides", view.obsSlides);
		settings.setValue("Preset", view.cameraPreset);
		++i;
	}
	settings.endArray();
	settings.endGroup();
}

void setViews(QVector<View> const&views) {
	QSettings s;
	return setViews(s, views);
}

QVector<View> getViews(QSettings &settings) {
	QVector<View> out;
	settings.beginGroup("Views");
	auto const size = settings.beginReadArray("Views");
	for (auto i = 0; i < size; ++i) {
		settings.setArrayIndex(i);
		out.emplace_back(View{
			.name = settings.value("Name").toString(),
			.slides = settings.value("Slides").toBool(),
			.obsSlides = settings.value("ObsSlides").toBool(),
			.cameraPreset = settings.value("Preset").toInt(),
		});
	}
	settings.endArray();
	settings.endGroup();
	return out;
}

QVector<View> getViews() {
	QSettings s;
	return getViews(s);
}
