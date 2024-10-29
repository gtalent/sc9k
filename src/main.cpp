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
	QApplication a(argc, argv);
	a.setStyle("fusion");
	QApplication::setApplicationName(QObject::tr("Slide Controller 9000"));
	MainWindow w;
	w.show();
	return QApplication::exec();
}
