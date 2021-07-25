#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

class OpenLPClient: public QObject {
	Q_OBJECT
   private:
	   struct Song {
			QString name;
			QString id;
		};
	   static constexpr auto BaseUrl = "http://127.0.0.1:4316";
		QNetworkAccessManager *m_nam = new QNetworkAccessManager(this);
		QNetworkAccessManager *m_pollingNam = new QNetworkAccessManager(this);
		QNetworkAccessManager *m_songListNam = new QNetworkAccessManager(this);
		QNetworkAccessManager *m_slideListNam = new QNetworkAccessManager(this);
		QTimer m_pollTimer;
		QHash<QString, QString> m_songNameMap;
		int m_currentServiceId = -1;
		QString m_currentSongId;

   public:
		explicit OpenLPClient(QObject *parent = nullptr);

   public slots:
		void nextSlide();

		void prevSlide();

		void nextSong();

		void prevSong();

		void blankScreen();

		void showSlides();

		void changeSong(int it);

		void changeSlide(int slide);

   private:
		void get(QString url);

		void requestSongList();

		void requestSlideList();

   private slots:
		void poll();

		void handleGeneralResponse(QNetworkReply *reply);

		void handlePollResponse(QNetworkReply *reply);

		void handleSongListResponse(QNetworkReply *reply);

		void handleSlideListResponse(QNetworkReply *reply);

   signals:
		void pollUpdate(QString songId, int slideNum);

		void songListUpdate(QStringList);

		void slideListUpdate(QStringList, QStringList);

};

