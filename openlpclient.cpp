

#include "openlpclient.hpp"

OpenLPClient::OpenLPClient(QObject *parent): QObject(parent) {

}

void OpenLPClient::nextSlide() {
	get("/api/controller/live/next");
}

void OpenLPClient::prevSlide() {
	get("/api/controller/live/previous");
}

void OpenLPClient::nextSong() {
	get("/api/service/next");
}

void OpenLPClient::prevSong() {
	get("/api/service/previous");
}

void OpenLPClient::blankScreen() {
	get("/api/display/hide");
}

void OpenLPClient::showSlides() {
	get("/api/display/show");
}

void OpenLPClient::get(QString urlExt) {
	QUrl url(QString(BaseUrl) + urlExt);
	QNetworkRequest rqst(url);
	m_nam->get(rqst);
}
