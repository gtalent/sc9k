/*
 * Copyright 2021 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
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
#ifndef _WIN32
	m_slideTable->setAlternatingRowColors(true);
#endif
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
	m_currentSlide = 0;
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
	// Is this replacing an existing song list or is it the initial song list?
	// We want to reset the song to 0 upon replacement,
	// but leave it alone upon initialization.
	auto isReplacement = m_songSelector->count() > 0;
	disconnect(m_songSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSong(int)));
	m_songSelector->clear();
	m_songSelector->addItems(songList);
	if (isReplacement) {
		changeSong(0);
	}
	connect(m_songSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSong(int)));
}
