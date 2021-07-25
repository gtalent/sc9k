#pragma once

#include <QWidget>

class SlideView : public QWidget
{
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

   private slots:
		void changeSong(int song);

   signals:
		void songChanged(int);

		void slideChanged(int);
};

