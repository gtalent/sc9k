/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <QString>
#include <QVector>

struct ConnectionData {
	QString host;
	uint16_t port = 0;
};

void setCameraConnectionData(class QSettings &settings, ConnectionData const&cd);

void setOpenLPConnectionData(class QSettings &settings, ConnectionData const&cd);

void setOBSConnectionData(class QSettings &settings, ConnectionData const&cd);

[[nodiscard]]
ConnectionData getCameraConnectionData(class QSettings &settings);

[[nodiscard]]
ConnectionData getOpenLPConnectionData(class QSettings &settings);

[[nodiscard]]
ConnectionData getOBSConnectionData(class QSettings &settings);

[[nodiscard]]
ConnectionData getCameraConnectionData();

[[nodiscard]]
ConnectionData getOpenLPConnectionData();

[[nodiscard]]
ConnectionData getOBSConnectionData();


struct View {
	QString name;
	bool slides = false;
	bool obsSlides = false;
	int cameraPreset = -1;
};

void setViews(class QSettings &settings, QVector<View> const&views);

void setViews(QVector<View> const&views);

[[nodiscard]]
QVector<View> getViews(class QSettings &settings);

[[nodiscard]]
QVector<View> getViews();