#include "obsclient.hpp"

OBSClient::OBSClient(QObject *parent): QObject(parent) {
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
