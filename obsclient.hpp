#pragma once

#include <QNetworkAccessManager>
#include <QObject>

class OBSClient : public QObject
{
	Q_OBJECT
   private:
	   static constexpr auto BaseUrl = "http://127.0.0.1:9302";
		QNetworkAccessManager *m_nam = new QNetworkAccessManager(this);

   public:
		explicit OBSClient(QObject *parent = nullptr);

   public slots:
		void setScene(QString scene);

		void showSlides();

		void hideSlides();

		void setSlidesVisible(int state);

   private:
		void get(QString url);

   signals:

};

