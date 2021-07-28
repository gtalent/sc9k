/*
 * Copyright 2021 gary@drinkingtea.net
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
		class QComboBox *m_songSelector = nullptr;
		QString m_currentSong;
		int m_currentSlide = -1;
   public:
		explicit SlideView(QWidget *parent = nullptr);

   public slots:
		void pollUpdate(QString songId, int slideNum);

		void songListUpdate(QStringList songList);

		void slideListUpdate(QStringList tagList, QStringList songList);

		void reset();

   private slots:
		void changeSong(int song);

   signals:
		void songChanged(int);

		void slideChanged(int);
};

