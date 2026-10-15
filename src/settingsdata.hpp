/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>
#include <QString>
#include <QVector>

struct CameraPreset {
	int brightness = 6;
	int saturation = 4;
	int contrast = 8;
	int sharpness = 3;
	int hue = 7;
	// when false, the camera's own stored preset is recalled instead
	bool usePtz = false;
	int pan = 0;
	int tilt = 0;
	int zoom = 0;
};

void setCameraPresets(class QSettings &settings, QVector<CameraPreset> const &presets);

void setCameraPresets(QVector<CameraPreset> const &presets);

QVector<CameraPreset> getCameraPresets(class QSettings &settings);

QVector<CameraPreset> getCameraPresets();

struct PtzSpeed {
	// VISCA pan/tilt speed, 1-24
	int panTilt = 12;
	// 0 uses the camera's default zoom speed, 1-8 uses variable speed zoom
	int zoom = 0;
};

void setPtzSpeed(class QSettings &settings, PtzSpeed const &speed);

[[nodiscard]]
PtzSpeed getPtzSpeed(class QSettings &settings);

[[nodiscard]]
PtzSpeed getPtzSpeed();

struct ConnectionData {
	QString host;
	uint16_t port = 0;
};

struct OBSConnectionData {
	QString host;
	uint16_t port = 0;
	QString password;
};

void setCameraConnectionData(class QSettings &settings, ConnectionData const &cd);

void setOpenLPConnectionData(class QSettings &settings, ConnectionData const &cd);

void setOBSConnectionData(class QSettings &settings, OBSConnectionData const &cd);

void setCameraConnectionData(ConnectionData const &cd);

void setOpenLPConnectionData(ConnectionData const &cd);

void setOBSConnectionData(OBSConnectionData const &cd);

[[nodiscard]]
ConnectionData getCameraConnectionData(class QSettings &settings);

[[nodiscard]]
ConnectionData getOpenLPConnectionData(class QSettings &settings);

[[nodiscard]]
OBSConnectionData getOBSConnectionData(class QSettings &settings);

[[nodiscard]]
ConnectionData getCameraConnectionData();

[[nodiscard]]
ConnectionData getOpenLPConnectionData();

[[nodiscard]]
OBSConnectionData getOBSConnectionData();


struct View {
	QString name;
	bool slides = false;
	bool obsSlides = false;
	int cameraPreset = -1;
};

void setViews(class QSettings &settings, QVector<View> const &views);

void setViews(QVector<View> const &views);

[[nodiscard]]
QVector<View> getViews(class QSettings &settings);

[[nodiscard]]
QVector<View> getViews();
