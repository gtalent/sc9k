/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>

#include <QMainWindow>

#include "cameraclient.hpp"
#include "obsclient.hpp"
#include "openlpclient.hpp"
#include "settingsdata.hpp"

class MainWindow: public QMainWindow {
	Q_OBJECT

	private:
		CameraClient m_cameraClient;
		OBSClient m_obsClient;
		OpenLPClient m_openlpClient;
		class SlideView *m_slideView = nullptr;
		bool m_cameraConnected = false;
		bool m_openLpConnected = false;
		bool m_obsConnected = false;
		class QVBoxLayout *m_rootLyt = nullptr;
		class QHBoxLayout *m_viewControlsParentLyt = nullptr;
		class QWidget *m_viewControlsParent = nullptr;
		class QWidget *m_viewControls = nullptr;

	public:
		explicit MainWindow(QWidget *parent = nullptr);
		~MainWindow() override = default;

	private:
		void setupMenu();

		void setupDefaultViewControls(class QGridLayout *rootLyt);

		void setupCustomViewControls(QVector<View> const&views, class QGridLayout *rootLyt);

		void setupViewControls(class QVBoxLayout *rootLyt);

		void openSettings();

		void cameraConnectionInit();

		void cameraConnectionLost();

		void openLpConnectionInit();

		void openLpConnectionLost();

		void obsConnectionInit();

		void obsConnectionLost();

		void refreshStatusBar();

};
