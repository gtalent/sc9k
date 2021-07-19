#pragma once

#include <QNetworkAccessManager>
#include <QObject>

class OpenLPClient: public QObject {
	Q_OBJECT
   private:
	   static constexpr auto BaseUrl = "http://127.0.0.1:4316";
		QNetworkAccessManager *m_nam = new QNetworkAccessManager(this);

   public:
		explicit OpenLPClient(QObject *parent = nullptr);

   public slots:
		void nextSlide();

		void prevSlide();

		void nextSong();

		void prevSong();

		void blankScreen();

		void showSlides();

   private:
		void get(QString url);

   signals:

};

