#include <QComboBox>
#include <QDebug>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

#include "slideview.hpp"

SlideView::SlideView(QWidget *parent): QWidget(parent) {
	auto lyt = new QVBoxLayout(this);
	m_songSelector = new QComboBox(this);
	m_slideTable = new QTableWidget(this);
	auto header = m_slideTable->horizontalHeader();
	header->setVisible(false);
	header->setStretchLastSection(true);
	m_slideTable->setSelectionBehavior(QTableWidget::SelectionBehavior::SelectRows);
	m_slideTable->setSelectionMode(QTableWidget::SelectionMode::SingleSelection);
	m_slideTable->setColumnCount(1);
	connect(header, &QHeaderView::sectionResized, m_slideTable, &QTableWidget::resizeRowsToContents);
	m_slideTable->resizeRowsToContents();
	m_slideTable->verticalHeader()->setDefaultSectionSize(300);
	lyt->addWidget(m_songSelector);
	lyt->addWidget(m_slideTable);
	connect(m_slideTable, &QTableWidget::currentCellChanged, this, &SlideView::slideChanged);
}

void SlideView::pollUpdate(QString songName, int slide) {
	if (songName != m_currentSong) {
		m_currentSong = songName;
		m_songSelector->setCurrentText(songName);
	}
	if (slide != m_currentSlide) {
		m_currentSlide = slide;
		m_slideTable->setCurrentCell(slide, 0);
	}
}

void SlideView::changeSong(int song) {
	if (m_songSelector->currentText() != m_currentSong) {
		emit songChanged(song);
	}
}

void SlideView::slideListUpdate(QStringList tagList, QStringList slideList) {
	m_currentSlide = -1;
	m_slideTable->setRowCount(slideList.size());
	for (int i = 0; i < slideList.size(); ++i) {
		auto txt = slideList[i];
		auto item = new QTableWidgetItem(txt);
		item->setFlags(item->flags() & ~Qt::ItemIsEditable);
		m_slideTable->setItem(i, 0, item);
	}
	m_slideTable->setVerticalHeaderLabels(tagList);
}

void SlideView::songListUpdate(QStringList songList) {
	disconnect(m_songSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSong(int)));
	m_songSelector->clear();
	m_songSelector->addItems(songList);
	changeSong(0);
	connect(m_songSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSong(int)));
}
