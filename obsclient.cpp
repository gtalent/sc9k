#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "obsclient.hpp"

OBSClient::OBSClient(QObject *parent): QObject(parent) {
	m_pollTimer.start(1000);
	connect(&m_pollTimer, &QTimer::timeout, this, &OBSClient::poll);
	connect(m_pollingNam, &QNetworkAccessManager::finished, this, &OBSClient::handlePollResponse);
}

void OBSClient::setScene(QString scene) {
	get(QString("/Scene?name=%1").arg(scene));
}

void OBSClient::showSlides() {
	setScene("MusicScene");
}

void OBSClient::hideSlides() {
	setScene("SpeakerScene");
}

void OBSClient::setSlidesVisible(int state) {
	if (state) {
		setScene("MusicScene");
	} else {
		setScene("SpeakerScene");
	}
}

void OBSClient::get(QString urlExt) {
	QUrl url(QString(BaseUrl) + urlExt);
	QNetworkRequest rqst(url);
	m_nam->get(rqst);
}

void OBSClient::poll() {
	QUrl url(QString(BaseUrl) + "/api/poll?_=1626628079579");
	QNetworkRequest rqst(url);
	m_pollingNam->get(rqst);
}

void OBSClient::handlePollResponse(QNetworkReply *reply) {
	if (reply->error()) {
		qDebug() << reply->errorString();
		emit pollFailed();
		return;
	}
	emit pollUpdate();
}

