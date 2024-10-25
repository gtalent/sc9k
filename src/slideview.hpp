/*
 * Copyright 2021 - 2024 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#pragma once

#include <QWidget>

class SlideView: public QWidget {
	Q_OBJECT
	private:
		class QTableWidget *m_slideTable = nullptr;
		class QListWidget *m_songSelector = nullptr;
		QString m_currentSong;
		int m_currentSlide = -1;

	public:
		explicit SlideView(QWidget *parent = nullptr);

		[[nodiscard]]
		QString getNextSong() const;

	public slots:
		void pollUpdate(const QString& songId, int slideNum);

		void songListUpdate(QStringList const&songList);

		void slideListUpdate(QStringList tagList, QStringList const&songList);

		void reset();

	private slots:
		void changeSong(int song);

	signals:
		void songChanged(int);

		void slideChanged(int);
};

