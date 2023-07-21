/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QApplication>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>

#include "settingsdialog.hpp"
#include "slideview.hpp"
#include "mainwindow.hpp"

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent) {
	move(0, 0);
	setFixedSize(610, 555);
	setWindowTitle(tr("Slide Controller 9000"));
	setupMenu();
	const auto mainWidget = new QWidget(this);
	m_rootLyt = new QVBoxLayout;
	const auto controlsLayout = new QGridLayout;
	m_slideView = new SlideView(this);
	setCentralWidget(mainWidget);
	mainWidget->setLayout(m_rootLyt);
	m_rootLyt->addWidget(m_slideView);
	m_rootLyt->addLayout(controlsLayout);
	// setup slide controls
	const auto btnPrevSong = new QPushButton(tr("Previous Song"), this);
	const auto btnPrevSlide = new QPushButton(tr("Previous Slide"), this);
	const auto btnNextSlide = new QPushButton(tr("Next Slide"), this);
	const auto btnNextSong = new QPushButton(tr("Next Song"), this);
	btnPrevSong->setToolTip(tr("Change to previous song (left arrow key)"));
	btnPrevSlide->setToolTip(tr("Change to previous slide (up arrow key)"));
	btnNextSong->setToolTip(tr("Change to next song (right arrow key)"));
	btnNextSlide->setToolTip(tr("Change to next slide (down arrow key)"));
	controlsLayout->addWidget(btnPrevSlide, 0, 1);
	controlsLayout->addWidget(btnNextSlide, 0, 2);
	controlsLayout->addWidget(btnPrevSong, 0, 0);
	controlsLayout->addWidget(btnNextSong, 0, 3);
	controlsLayout->setSpacing(2);
	btnNextSong->setShortcut(Qt::Key_Right);
	btnPrevSong->setShortcut(Qt::Key_Left);
	btnNextSlide->setShortcut(Qt::Key_Down);
	btnPrevSlide->setShortcut(Qt::Key_Up);
	btnNextSong->setFixedWidth(135);
	btnPrevSong->setFixedWidth(135);
	btnNextSlide->setFixedWidth(135);
	btnPrevSlide->setFixedWidth(135);
	setupViewControls(m_rootLyt);
	connect(btnNextSlide, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::nextSlide);
	connect(btnPrevSlide, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::prevSlide);
	connect(btnNextSong, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::nextSong);
	connect(btnPrevSong, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::prevSong);
	connect(&m_openlpClient, &OpenLPClient::pollUpdate, m_slideView, &SlideView::pollUpdate);
	connect(&m_openlpClient, &OpenLPClient::songListUpdate, m_slideView, &SlideView::songListUpdate);
	connect(&m_openlpClient, &OpenLPClient::slideListUpdate, m_slideView, &SlideView::slideListUpdate);
	connect(&m_openlpClient, &OpenLPClient::pollFailed, m_slideView, &SlideView::reset);
	connect(m_slideView, &SlideView::songChanged, &m_openlpClient, &OpenLPClient::changeSong);
	connect(m_slideView, &SlideView::slideChanged, &m_openlpClient, &OpenLPClient::changeSlide);
	// setup status bar
	setStatusBar(new QStatusBar(this));
	connect(&m_cameraClient, &CameraClient::pollUpdate, this, &MainWindow::cameraConnectionInit);
	connect(&m_openlpClient, &OpenLPClient::songChanged, this, &MainWindow::refreshStatusBar);
	connect(&m_openlpClient, &OpenLPClient::pollUpdate, this, &MainWindow::openLpConnectionInit);
	connect(&m_obsClient, &OBSClient::pollUpdate, this, &MainWindow::obsConnectionInit);
	refreshStatusBar();
	connect(statusBar(), &QStatusBar::messageChanged, this, [this](QStringView const&msg) {
		if (msg.empty()) {
			refreshStatusBar();
		}
	});
}

void MainWindow::setupMenu() {
	// file menu
	{
		auto const menu = menuBar()->addMenu(tr("&File"));
		auto const settingsAct = new QAction(tr("&Settings"), this);
		auto const quitAct = new QAction(tr("E&xit"), this);
		settingsAct->setShortcuts(QKeySequence::Preferences);
		connect(settingsAct, &QAction::triggered, this, &MainWindow::openSettings);
		quitAct->setShortcuts(QKeySequence::Quit);
		quitAct->setStatusTip(tr("Exit application"));
		connect(quitAct, &QAction::triggered, &QApplication::quit);
		menu->addAction(settingsAct);
		menu->addAction(quitAct);
	}
	// slides menu
	{
		auto const menu = menuBar()->addMenu(tr("&Slides"));
		auto const hideSlidesAct = new QAction(tr("&Hide Slides"), this);
		hideSlidesAct->setShortcut(Qt::CTRL | Qt::Key_1);
		connect(hideSlidesAct, &QAction::triggered, &m_openlpClient, &OpenLPClient::blankScreen);
		connect(hideSlidesAct, &QAction::triggered, &m_obsClient, &OBSClient::hideSlides);
		menu->addAction(hideSlidesAct);
		auto const showSlidesInOpenLpAct = new QAction(tr("Show in &OpenLP Only"), this);
		showSlidesInOpenLpAct->setShortcut(Qt::CTRL | Qt::Key_2);
		connect(showSlidesInOpenLpAct, &QAction::triggered, &m_openlpClient, &OpenLPClient::showSlides);
		connect(showSlidesInOpenLpAct, &QAction::triggered, &m_obsClient, &OBSClient::hideSlides);
		menu->addAction(showSlidesInOpenLpAct);
		auto const showSlidesAct = new QAction(tr("&Show Slides"), this);
		showSlidesAct->setShortcut(Qt::CTRL | Qt::Key_3);
		connect(showSlidesAct, &QAction::triggered, &m_obsClient, &OBSClient::showSlides);
		connect(showSlidesAct, &QAction::triggered, &m_openlpClient, &OpenLPClient::showSlides);
		menu->addAction(showSlidesAct);
	}
	// camera preset menu
	{
		auto const menu = menuBar()->addMenu(tr("&Camera Preset"));
		for (auto i = 0; i < MaxCameraPresets; ++i) {
			auto const cameraPresetAct = new QAction(tr("Camera Preset &%1").arg(i + 1), this);
			cameraPresetAct->setShortcut(Qt::ALT | static_cast<Qt::Key>(Qt::Key_1 + i));
			connect(cameraPresetAct, &QAction::triggered, &m_cameraClient, [this, i] {
				m_cameraClient.setPreset(i + 1);
			});
			menu->addAction(cameraPresetAct);
		}
	}
	// help menu
	{
		auto const menu = menuBar()->addMenu(tr("&Help"));
		auto const aboutAct = new QAction(tr("&About"), this);
		connect(aboutAct, &QAction::triggered, &m_cameraClient, [this] {
			QMessageBox about(this);
			about.setText(tr(
R"(Slide Controller 9000 - 1.0-beta
Build date: %1

Copyright 2021 - 2023 Gary Talent (gary@drinkingtea.net)
Slide Controller 9000 is released under the MPL 2.0
Built on Qt library under LGPL 2.0)").arg(__DATE__));
			about.exec();
		});
		menu->addAction(aboutAct);
	}
}

void MainWindow::setupDefaultViewControls(QGridLayout *viewCtlLyt) {
	auto const mainWidget = viewCtlLyt->parentWidget();
	auto const btnHideSlides = new QPushButton(tr("1. Hide"), mainWidget);
	auto const btnOpenLpShowSlides = new QPushButton(tr("2. Show in OpenLP Only"), mainWidget);
	auto const btnShowSlides = new QPushButton(tr("3. Show"), mainWidget);
	viewCtlLyt->addWidget(btnHideSlides, 0, 0);
	viewCtlLyt->addWidget(btnOpenLpShowSlides, 0, 1);
	viewCtlLyt->addWidget(btnShowSlides, 0, 2);
	btnHideSlides->setShortcut(Qt::Key_1);
	btnOpenLpShowSlides->setShortcut(Qt::Key_2);
	btnHideSlides->setToolTip(tr("Also hides slides in OBS"));
	btnShowSlides->setShortcut(Qt::Key_3);
	connect(btnHideSlides, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::blankScreen);
	connect(btnHideSlides, &QPushButton::clicked, &m_obsClient, &OBSClient::hideSlides);
	connect(btnOpenLpShowSlides, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::showSlides);
	connect(btnOpenLpShowSlides, &QPushButton::clicked, &m_obsClient, &OBSClient::hideSlides);
	connect(btnShowSlides, &QPushButton::clicked, &m_obsClient, &OBSClient::showSlides);
	connect(btnShowSlides, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::showSlides);
}

void MainWindow::setupCustomViewControls(QVector<View> const&views, QGridLayout *viewCtlLyt) {
	constexpr auto columns = 3;
	auto const parent = viewCtlLyt->parentWidget();
	for (auto i = 0; auto const&view : views) {
		auto const x = i % columns;
		auto const y = i / columns;
		auto const name = QString("%1. %2").arg(i + 1).arg(view.name);
		auto const btn = new QPushButton(name, parent);
		btn->setShortcut(Qt::Key_1 + i);
		viewCtlLyt->addWidget(btn, y, x);
		auto const slides = view.slides;
		auto const obsSlides = view.obsSlides;
		auto const cameraPreset = view.cameraPreset;
		connect(btn, &QPushButton::clicked, this, [this, slides, obsSlides, cameraPreset] {
			m_cameraClient.setPreset(cameraPreset);
			m_openlpClient.setSlidesVisible(slides);
			m_obsClient.setSlidesVisible(obsSlides);
		});
		++i;
	}
}

void MainWindow::setupViewControls(QVBoxLayout *rootLyt) {
	auto views = getViews();
	if (!m_viewControlsParent) {
		m_viewControlsParent = new QWidget(rootLyt->parentWidget());
		m_viewControlsParentLyt = new QHBoxLayout(m_viewControlsParent);
		m_viewControlsParentLyt->setContentsMargins(0, 0, 0, 0);
		rootLyt->addWidget(m_viewControlsParent);
	}
	delete m_viewControls;
	m_viewControls = new QWidget(m_viewControlsParent);
	m_viewControlsParentLyt->addWidget(m_viewControls);
	auto const viewCtlLyt = new QGridLayout(m_viewControls);
	viewCtlLyt->setSpacing(5);
	if (views.empty()) {
		views.emplace_back(View{
				.name = tr("Hide"),
				.slides = false,
				.obsSlides = false,
		});
		views.emplace_back(View{
				.name = tr("Hide in OBS"),
				.slides = false,
				.obsSlides = false,
		});
		views.emplace_back(View{
			.name = tr("Show"),
			.slides = false,
			.obsSlides = false,
		});
	}
	if (views.empty()) {
		setupDefaultViewControls(viewCtlLyt);
	} else {
		setupCustomViewControls(views, viewCtlLyt);
	}
}

void MainWindow::openSettings() {
	SettingsDialog d(this);
	auto const result = d.exec();
	if (result == QDialog::Accepted) {
		m_cameraClient.setBaseUrl();
		m_obsClient.setBaseUrl();
		m_openlpClient.setBaseUrl();
		setupViewControls(m_rootLyt);
	}
}

void MainWindow::cameraConnectionInit() {
	disconnect(&m_cameraClient, &CameraClient::pollUpdate, this, &MainWindow::cameraConnectionInit);
	connect(&m_cameraClient, &CameraClient::pollFailed, this, &MainWindow::cameraConnectionLost);
	m_cameraConnected = true;
	refreshStatusBar();
}

void MainWindow::cameraConnectionLost() {
	disconnect(&m_cameraClient, &CameraClient::pollFailed, this, &MainWindow::cameraConnectionLost);
	connect(&m_cameraClient, &CameraClient::pollUpdate, this, &MainWindow::cameraConnectionInit);
	m_cameraConnected = false;
	refreshStatusBar();
}

void MainWindow::openLpConnectionInit() {
	disconnect(&m_openlpClient, &OpenLPClient::pollUpdate, this, &MainWindow::openLpConnectionInit);
	connect(&m_openlpClient, &OpenLPClient::pollFailed, this, &MainWindow::openLpConnectionLost);
	m_openLpConnected = true;
	refreshStatusBar();
}

void MainWindow::openLpConnectionLost() {
	disconnect(&m_openlpClient, &OpenLPClient::pollFailed, this, &MainWindow::openLpConnectionLost);
	connect(&m_openlpClient, &OpenLPClient::pollUpdate, this, &MainWindow::openLpConnectionInit);
	m_openLpConnected = false;
	refreshStatusBar();
}

void MainWindow::obsConnectionInit() {
	disconnect(&m_obsClient, &OBSClient::pollUpdate, this, &MainWindow::obsConnectionInit);
	connect(&m_obsClient, &OBSClient::pollFailed, this, &MainWindow::obsConnectionLost);
	m_obsConnected = true;
	refreshStatusBar();
}

void MainWindow::obsConnectionLost() {
	disconnect(&m_obsClient, &OBSClient::pollFailed, this, &MainWindow::obsConnectionLost);
	connect(&m_obsClient, &OBSClient::pollUpdate, this, &MainWindow::obsConnectionInit);
	m_obsConnected = false;
	refreshStatusBar();
}

void MainWindow::refreshStatusBar() {
	auto const cameraStatus = m_cameraConnected ? tr("Camera: Connected") : tr("Camera: Not Connected");
	auto const openLpStatus = m_openLpConnected ? tr("OpenLP: Connected") : tr("OpenLP: Not Connected");
	auto const obsStatus = m_obsConnected ? tr("OBS: Connected") : tr("OBS: Not Connected");
	auto const nextSong = m_openlpClient.getNextSong();
	auto const nextSongTxt = m_openLpConnected ? " | Next Song: " + nextSong : "";
	statusBar()->showMessage(cameraStatus + " | " + openLpStatus + " | " + obsStatus + nextSongTxt);
}
