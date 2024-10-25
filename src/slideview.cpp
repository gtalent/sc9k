/*
 * Copyright 2021 - 2024 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QTableWidget>

#include "slideview.hpp"

SlideView::SlideView(QWidget *parent): QWidget(parent) {
	auto lyt = new QHBoxLayout(this);
	m_songSelector = new QListWidget(this);
	m_slideTable = new QTableWidget(this);
	auto header = m_slideTable->horizontalHeader();
	header->setVisible(false);
	header->setStretchLastSection(true);
	m_slideTable->setSelectionBehavior(QTableWidget::SelectionBehavior::SelectRows);
	m_slideTable->setSelectionMode(QTableWidget::SelectionMode::SingleSelection);
	m_slideTable->setColumnCount(1);
	m_slideTable->verticalHeader()->setDefaultSectionSize(300);
#ifndef _WIN32
	m_slideTable->setAlternatingRowColors(true);
#endif
	lyt->addWidget(m_slideTable);
	lyt->addWidget(m_songSelector);
	connect(m_slideTable, &QTableWidget::currentCellChanged, this, &SlideView::slideChanged);
}

QString SlideView::getNextSong() const {
	auto const cnt = m_songSelector->count();
	auto const idx = m_songSelector->currentRow() + 1;
	if (idx < cnt) {
		return m_songSelector->currentItem()->text();
	}
	return "";
}

void SlideView::pollUpdate(QString const&songName, int slide) {
	auto const songItems = m_songSelector->findItems(songName, Qt::MatchFixedString);
	if (songItems.empty()) {
		return;
	}
	auto songItem = songItems.first();
	if (songItem != m_songSelector->currentItem()) {
		m_currentSong = songName;
		m_songSelector->setCurrentItem(songItem);
	}
	if (slide != m_currentSlide) {
		m_currentSlide = slide;
		m_slideTable->setCurrentCell(slide, 0);
	}
}

void SlideView::changeSong(int song) {
	if (song < 0) {
		return;
	}
	auto const songItem = m_songSelector->item(song);
	if (songItem && songItem->text() != m_currentSong) {
		emit songChanged(song);
	}
}

void SlideView::slideListUpdate(QStringList tagList, QStringList const&slideList) {
	for (auto &tag : tagList) {
		tag = tag.split("").join("\n");
	}
	m_currentSlide = 0;
	m_slideTable->setRowCount(static_cast<int>(slideList.size()));
	for (int i = 0; i < slideList.size(); ++i) {
		auto const& txt = slideList[i];
		auto item = new QTableWidgetItem(txt);
		item->setFlags(item->flags() & ~Qt::ItemIsEditable);
		m_slideTable->setItem(i, 0, item);
	}
	m_slideTable->setVerticalHeaderLabels(tagList);
	m_slideTable->resizeRowsToContents();
}

void SlideView::reset() {
	m_songSelector->clear();
	m_slideTable->setRowCount(0);
	m_currentSong = "";
	m_currentSlide = -1;
}

void SlideView::songListUpdate(QStringList const&songList) {
	// Is this replacing an existing song list or is it the initial song list?
	// We want to reset the song to 0 upon replacement,
	// but leave it alone upon initialization.
	auto isReplacement = m_songSelector->count() > 0;
	disconnect(m_songSelector, &QListWidget::currentRowChanged, this, &SlideView::changeSong);
	m_songSelector->clear();
	m_songSelector->addItems(songList);
	if (isReplacement) {
		changeSong(0);
	}
	connect(m_songSelector, &QListWidget::currentRowChanged, this, &SlideView::changeSong);
}
