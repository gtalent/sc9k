/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QSettings>

#include "consts.hpp"
#include "settingsdata.hpp"

void setCameraPresets(QSettings &settings, QVector<CameraPreset> const &presets) {
	settings.beginGroup("Camera");
	// key predates PTZ support, keep it to preserve existing settings
	settings.beginWriteArray("VideoImageConfig");
	for (auto i = 0; auto const &p : presets) {
		settings.setArrayIndex(i);
		settings.setValue("brightness", p.brightness);
		settings.setValue("saturation", p.saturation);
		settings.setValue("contrast", p.contrast);
		settings.setValue("sharpness", p.sharpness);
		settings.setValue("hue", p.hue);
		settings.setValue("usePtz", p.usePtz);
		settings.setValue("pan", p.pan);
		settings.setValue("tilt", p.tilt);
		settings.setValue("zoom", p.zoom);
		settings.setValue("manualFocus", p.manualFocus);
		settings.setValue("focus", p.focus);
		++i;
	}
	settings.endArray();
	settings.endGroup();
}

void setCameraPresets(QVector<CameraPreset> const &presets) {
	QSettings s;
	setCameraPresets(s, presets);
}

QVector<CameraPreset> getCameraPresets(QSettings &settings) {
	QVector<CameraPreset> presets(MaxCameraPresets);
	settings.beginGroup("Camera");
	auto const size = std::min(settings.beginReadArray("VideoImageConfig"), MaxCameraPresets);
	for (auto i = 0; i < size; ++i) {
		settings.setArrayIndex(i);
		presets[i] = {
			.brightness = settings.value("brightness").toInt(),
			.saturation = settings.value("saturation").toInt(),
			.contrast = settings.value("contrast").toInt(),
			.sharpness = settings.value("sharpness").toInt(),
			.hue = settings.value("hue").toInt(),
			.usePtz = settings.value("usePtz", false).toBool(),
			.pan = settings.value("pan", 0).toInt(),
			.tilt = settings.value("tilt", 0).toInt(),
			.zoom = settings.value("zoom", 0).toInt(),
			.manualFocus = settings.value("manualFocus", false).toBool(),
			.focus = settings.value("focus", 0).toInt(),
		};
	}
	settings.endArray();
	settings.endGroup();
	return presets;
}

QVector<CameraPreset> getCameraPresets() {
	QSettings s;
	return getCameraPresets(s);
}

void setPtzSpeed(QSettings &settings, PtzSpeed const &speed) {
	settings.beginGroup("Camera");
	settings.setValue("PanTiltSpeed", speed.panTilt);
	settings.setValue("ZoomSpeed", speed.zoom);
	settings.endGroup();
}

PtzSpeed getPtzSpeed(QSettings &settings) {
	PtzSpeed const defaults;
	PtzSpeed out;
	settings.beginGroup("Camera");
	out.panTilt = settings.value("PanTiltSpeed", defaults.panTilt).toInt();
	out.zoom = settings.value("ZoomSpeed", defaults.zoom).toInt();
	settings.endGroup();
	return out;
}

PtzSpeed getPtzSpeed() {
	QSettings s;
	return getPtzSpeed(s);
}

void setZoomDirectScale(double const scale) {
	QSettings s;
	s.setValue("Camera/ZoomDirectScale", scale);
}

double getZoomDirectScale() {
	QSettings s;
	return s.value("Camera/ZoomDirectScale", 1.0).toDouble();
}

[[nodiscard]]
static QStringList toStringList(QVector<double> const &vals) {
	QStringList out;
	for (auto const v : vals) {
		out.append(QString::number(v));
	}
	return out;
}

[[nodiscard]]
static QVector<double> toDoubles(QStringList const &vals) {
	QVector<double> out;
	for (auto const &v : vals) {
		out.append(v.toDouble());
	}
	return out;
}

void setCameraCalibration(CameraCalibration const &cal) {
	QSettings s;
	s.beginGroup("Camera");
	s.setValue("MaxPanSpeed", cal.maxPanSpeed);
	s.setValue("MaxTiltSpeed", cal.maxTiltSpeed);
	s.setValue("PanVelocity", toStringList(cal.panVelocity));
	s.setValue("TiltVelocity", toStringList(cal.tiltVelocity));
	s.setValue("CalibratedAt", cal.calibratedAt);
	s.endGroup();
}

CameraCalibration getCameraCalibration() {
	QSettings s;
	s.beginGroup("Camera");
	CameraCalibration const cal{
		.maxPanSpeed = s.value("MaxPanSpeed").toInt(),
		.maxTiltSpeed = s.value("MaxTiltSpeed").toInt(),
		.panVelocity = toDoubles(s.value("PanVelocity").toStringList()),
		.tiltVelocity = toDoubles(s.value("TiltVelocity").toStringList()),
		.calibratedAt = s.value("CalibratedAt").toDateTime(),
	};
	s.endGroup();
	// tables are indexed by speed, so they must cover every speed up to the max
	auto const valid =
		cal.calibratedAt.isValid() &&
		cal.maxPanSpeed > 0 && cal.panVelocity.size() == cal.maxPanSpeed + 1 &&
		cal.maxTiltSpeed > 0 && cal.tiltVelocity.size() == cal.maxTiltSpeed + 1;
	return valid ? cal : CameraCalibration{};
}

void clearCameraCalibration() {
	QSettings s;
	s.beginGroup("Camera");
	for (auto const key : {"MaxPanSpeed", "MaxTiltSpeed", "PanVelocity", "TiltVelocity", "CalibratedAt"}) {
		s.remove(key);
	}
	s.endGroup();
}

void setCameraConnectionData(QSettings &settings, ConnectionData const &cd) {
	settings.beginGroup("CameraClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

void setOpenLPConnectionData(QSettings &settings, ConnectionData const &cd) {
	settings.beginGroup("OpenLPClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

void setOBSConnectionData(QSettings &settings, OBSConnectionData const &cd) {
	settings.beginGroup("OBSClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.setValue("Password", cd.password);
	settings.endGroup();
}

ConnectionData getCameraConnectionData(QSettings &settings) {
	ConnectionData out;
	settings.beginGroup("CameraClient");
	out.host = settings.value("Host", "192.168.100.88").toString();
	out.port = static_cast<uint16_t>(settings.value("Port", 52381).toInt());
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

OBSConnectionData getOBSConnectionData(QSettings &settings) {
	OBSConnectionData out;
	settings.beginGroup("OBSClient");
	out.host = settings.value("Host", "127.0.0.1").toString();
	out.port = static_cast<uint16_t>(settings.value("Port", 4455).toInt());
	out.password = settings.value("Password", "").toString();
	settings.endGroup();
	return out;
}

void setCameraConnectionData(ConnectionData const &cd) {
	QSettings settings;
	settings.beginGroup("CameraClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

void setOpenLPConnectionData(ConnectionData const &cd) {
	QSettings settings;
	settings.beginGroup("OpenLPClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.endGroup();
}

void setOBSConnectionData(OBSConnectionData const &cd) {
	QSettings settings;
	settings.beginGroup("OBSClient");
	settings.setValue("Host", cd.host);
	settings.setValue("Port", cd.port);
	settings.setValue("Password", cd.password);
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

OBSConnectionData getOBSConnectionData() {
	QSettings s;
	return getOBSConnectionData(s);
}


void setViews(QSettings &settings, QVector<View> const &views) {
	settings.beginGroup("Views");
	settings.beginWriteArray("Views");
	for (auto i = 0; auto const &view : views) {
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

void setViews(QVector<View> const &views) {
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
