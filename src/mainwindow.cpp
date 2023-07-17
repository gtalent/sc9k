/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QHBoxLayout>
#include <QPushButton>
#include <QStatusBar>

#include "slideview.hpp"
#include "mainwindow.hpp"

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent) {
	move(0, 0);
	setFixedSize(590, 555);
	setWindowTitle(tr("Slide Controller 9000"));
	const auto mainWidget = new QWidget(this);
	const auto rootLyt = new QVBoxLayout;
	const auto controlsLayout = new QGridLayout;
	m_slideView = new SlideView(this);
	setCentralWidget(mainWidget);
	mainWidget->setLayout(rootLyt);
	rootLyt->addWidget(m_slideView);
	rootLyt->addLayout(controlsLayout);
	// setup slide controls
	const auto showHideLyt = new QHBoxLayout;
	rootLyt->addLayout(showHideLyt);
	const auto btnPrevSong = new QPushButton(tr("Previous Song (Left)"), this);
	const auto btnPrevSlide = new QPushButton(tr("Previous Slide (Up)"), this);
	const auto btnNextSlide = new QPushButton(tr("Next Slide (Down)"), this);
	const auto btnNextSong = new QPushButton(tr("Next Song (Right)"), this);
	const auto btnHideSlides = new QPushButton(tr("Hide (1)"), this);
	const auto btnOpenLpShowSlides = new QPushButton(tr("Show in OpenLP Only (2)"), this);
	const auto btnShowSlides = new QPushButton(tr("Show (3)"), mainWidget);
	controlsLayout->addWidget(btnPrevSlide, 0, 1);
	controlsLayout->addWidget(btnNextSlide, 0, 2);
	controlsLayout->addWidget(btnPrevSong, 0, 0);
	controlsLayout->addWidget(btnNextSong, 0, 3);
	showHideLyt->addWidget(btnHideSlides);
	showHideLyt->addWidget(btnOpenLpShowSlides);
	showHideLyt->addWidget(btnShowSlides);
	btnNextSong->setShortcut(Qt::Key_Right);
	btnPrevSong->setShortcut(Qt::Key_Left);
	btnNextSlide->setShortcut(Qt::Key_Down);
	btnPrevSlide->setShortcut(Qt::Key_Up);
	btnHideSlides->setShortcut(Qt::Key_1);
	btnOpenLpShowSlides->setShortcut(Qt::Key_2);
	btnHideSlides->setToolTip(tr("Also hides slides in OBS"));
	btnShowSlides->setShortcut(Qt::Key_3);
	connect(btnNextSlide, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::nextSlide);
	connect(btnPrevSlide, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::prevSlide);
	connect(btnNextSong, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::nextSong);
	connect(btnPrevSong, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::prevSong);
	connect(btnHideSlides, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::blankScreen);
	connect(btnHideSlides, &QPushButton::clicked, &m_obsClient, &OBSClient::hideSlides);
	connect(btnOpenLpShowSlides, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::showSlides);
	connect(btnShowSlides, &QPushButton::clicked, &m_obsClient, &OBSClient::showSlides);
	connect(btnShowSlides, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::showSlides);
	connect(&m_openlpClient, &OpenLPClient::pollUpdate, m_slideView, &SlideView::pollUpdate);
	connect(&m_openlpClient, &OpenLPClient::songListUpdate, m_slideView, &SlideView::songListUpdate);
	connect(&m_openlpClient, &OpenLPClient::slideListUpdate, m_slideView, &SlideView::slideListUpdate);
	connect(&m_openlpClient, &OpenLPClient::pollFailed, m_slideView, &SlideView::reset);
	connect(m_slideView, &SlideView::songChanged, &m_openlpClient, &OpenLPClient::changeSong);
	connect(m_slideView, &SlideView::slideChanged, &m_openlpClient, &OpenLPClient::changeSlide);
	// setup scene selector
	connect(btnOpenLpShowSlides, &QPushButton::clicked, &m_obsClient, &OBSClient::hideSlides);
	// setup status bar
	setStatusBar(new QStatusBar(this));
	connect(&m_openlpClient, &OpenLPClient::songChanged, this, &MainWindow::refreshStatusBar);
	connect(&m_openlpClient, &OpenLPClient::pollUpdate, this, &MainWindow::openLpConnectionInit);
	connect(&m_obsClient, &OBSClient::pollUpdate, this, &MainWindow::obsConnectionInit);
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
	const auto openLpStatus = m_openLpConnected ? tr("OpenLP: Connected") : tr("OpenLP: Not Connected");
	const auto obsStatus = m_obsConnected ? tr("OBS: Connected") : tr("OBS: Not Connected");
	const auto nextSong = m_openlpClient.getNextSong();
	const auto nextSongTxt = m_openLpConnected ? " | Next Song: " + nextSong : "";
	statusBar()->showMessage(openLpStatus + " | " + obsStatus + nextSongTxt);
}
