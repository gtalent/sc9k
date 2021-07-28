/*
 * Copyright 2021 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>

#include <QMainWindow>

#include "obsclient.hpp"
#include "openlpclient.hpp"

class MainWindow: public QMainWindow {
	Q_OBJECT

   private:
	   OBSClient m_obsClient;
		OpenLPClient m_openlpClient;
		bool m_openLpConnected = false;
		bool m_obsConnected = false;

   public:
		explicit MainWindow(QWidget *parent = nullptr);
		~MainWindow() override = default;

   private slots:
		void openLpConnectionInit();

		void openLpConnectionLost();

		void obsConnectionInit();

		void obsConnectionLost();

		void refreshStatusBar();

};
