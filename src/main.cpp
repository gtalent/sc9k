/*
 * Copyright 2021 - 2024 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QApplication>
#include <QSettings>

#include "mainwindow.hpp"

int main(int argc, char *argv[]) {
	QSettings::setDefaultFormat(QSettings::Format::IniFormat);
#ifdef _WIN32
	QApplication::setStyle("Fusion");
#endif
	QApplication a(argc, argv);
	QApplication::setOrganizationName("DrinkingTea");
	QApplication::setApplicationName("Slide Controller 9000");
#ifdef __linux
	QIcon icon;
	icon.addFile(":/icons/hicolor/16x16/apps/net.drinkingtea.sc9k.png");
	icon.addFile(":/icons/hicolor/32x32/apps/net.drinkingtea.sc9k.png");
	icon.addFile(":/icons/hicolor/64x64/apps/net.drinkingtea.sc9k.png");
	icon.addFile(":/icons/hicolor/128x128/apps/net.drinkingtea.sc9k.png");
	icon.addFile(":/icons/hicolor/256x256/apps/net.drinkingtea.sc9k.png");
	icon.addFile(":/icons/hicolor/512x512/apps/net.drinkingtea.sc9k.png");
	QApplication::setWindowIcon(icon);
#endif
	MainWindow w;
	w.show();
	return QApplication::exec();
}
