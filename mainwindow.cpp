#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "slideview.hpp"
#include "mainwindow.hpp"

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent) {
	move(0, 0);
	setFixedSize(590, 555);
	setWindowTitle(tr("Slide Controller 9000"));
	const auto mainWidget = new QWidget(this);
	const auto rootLyt = new QVBoxLayout;
	const auto controlsLayout = new QGridLayout;
	const auto slideView = new SlideView(this);
	setCentralWidget(mainWidget);
	mainWidget->setLayout(rootLyt);
	rootLyt->addWidget(slideView);
	rootLyt->addLayout(controlsLayout);
	// setup slide controls
	const auto btnPrevSong = new QPushButton(tr("Previous Song (Left)"), this);
	const auto btnPrevSlide = new QPushButton(tr("Previous Slide (Up)"), this);
	const auto btnNextSlide = new QPushButton(tr("Next Slide (Down)"), this);
	const auto btnNextSong = new QPushButton(tr("Next Song (Right))"), this);
	const auto btnBlankSlides = new QPushButton(tr("Blank Slides (,)"), this);
	const auto btnShowSlides = new QPushButton(tr("Show Slides (.)"), this);
	controlsLayout->addWidget(btnPrevSlide, 0, 0);
	controlsLayout->addWidget(btnNextSlide, 0, 1);
	controlsLayout->addWidget(btnPrevSong, 1, 0);
	controlsLayout->addWidget(btnNextSong, 1, 1);
	controlsLayout->addWidget(btnBlankSlides, 2, 0);
	controlsLayout->addWidget(btnShowSlides, 2, 1);
	btnNextSong->setShortcut(Qt::Key_Right);
	btnPrevSong->setShortcut(Qt::Key_Left);
	btnNextSlide->setShortcut(Qt::Key_Down);
	btnPrevSlide->setShortcut(Qt::Key_Up);
	btnBlankSlides->setShortcut(Qt::Key_Comma);
	btnShowSlides->setShortcut(Qt::Key_Period);
	btnBlankSlides->setToolTip(tr("Also hides slides in OBS"));
	connect(btnNextSlide, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::nextSlide);
	connect(btnPrevSlide, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::prevSlide);
	connect(btnNextSong, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::nextSong);
	connect(btnPrevSong, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::prevSong);
	connect(btnBlankSlides, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::blankScreen);
	connect(btnBlankSlides, &QPushButton::clicked, &m_obsClient, &OBSClient::hideSlides);
	connect(btnShowSlides, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::showSlides);
	connect(&m_openlpClient, &OpenLPClient::pollUpdate, slideView, &SlideView::pollUpdate);
	connect(&m_openlpClient, &OpenLPClient::songListUpdate, slideView, &SlideView::songListUpdate);
	connect(&m_openlpClient, &OpenLPClient::slideListUpdate, slideView, &SlideView::slideListUpdate);
	connect(slideView, &SlideView::songChanged, &m_openlpClient, &OpenLPClient::changeSong);
	connect(slideView, &SlideView::slideChanged, &m_openlpClient, &OpenLPClient::changeSlide);
	// setup scene selector
	const auto btnObsHideSlides = new QPushButton(tr("Hide Slides in OBS (;)"), mainWidget);
	const auto btnObsShowSlides = new QPushButton(tr("Show Slides in OBS (')"), mainWidget);
	controlsLayout->addWidget(btnObsHideSlides, 3, 0);
	controlsLayout->addWidget(btnObsShowSlides, 3, 1);
	btnObsHideSlides->setShortcut(Qt::Key_Semicolon);
	btnObsShowSlides->setShortcut(Qt::Key_Apostrophe);
	btnObsShowSlides->setToolTip(tr("Also shows slides in OpenLP"));
	connect(btnObsHideSlides, &QPushButton::clicked, &m_obsClient, &OBSClient::hideSlides);
	connect(btnObsShowSlides, &QPushButton::clicked, &m_obsClient, &OBSClient::showSlides);
	connect(btnObsShowSlides, &QPushButton::clicked, &m_openlpClient, &OpenLPClient::showSlides);
}

MainWindow::~MainWindow() {
}
