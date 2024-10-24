/*
 * Copyright 2021 - 2024 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValueRef>
#include <QNetworkReply>
#include <QSettings>

#include "settingsdata.hpp"
#include "openlpclient.hpp"

OpenLPClient::OpenLPClient(QObject *parent): QObject(parent) {
	setBaseUrl();
	poll();
	m_pollTimer.start(250);
	connect(&m_pollTimer, &QTimer::timeout, this, &OpenLPClient::poll);
	connect(m_nam, &QNetworkAccessManager::finished, this, &OpenLPClient::handleGeneralResponse);
	connect(m_songListNam, &QNetworkAccessManager::finished, this, &OpenLPClient::handleSongListResponse);
	connect(m_slideListNam, &QNetworkAccessManager::finished, this, &OpenLPClient::handleSlideListResponse);
	connect(m_pollingNam, &QNetworkAccessManager::finished, this, &OpenLPClient::handlePollResponse);
}

QString OpenLPClient::getNextSong() {
	auto const currentSong = m_songNameMap[m_currentSongId];
	auto const songIdx = m_songList.indexOf(currentSong) + 1;
	if (songIdx < m_songList.size()) {
		return m_songList[songIdx];
	}
	return "";
}

void OpenLPClient::nextSlide() {
	post("/api/v2/controller/progress", R"({"action":"next"})");
}

void OpenLPClient::prevSlide() {
	post("/api/v2/controller/progress", R"({"action":"previous"})");
}

void OpenLPClient::nextSong() {
	post("/api/v2/service/progress", R"({"action":"next"})");
}

void OpenLPClient::prevSong() {
	post("/api/v2/service/progress", R"({"action":"previous"})");
}

void OpenLPClient::blankScreen() {
	get("/api/display/hide");
}

void OpenLPClient::showSlides() {
	get("/api/display/show");
}

void OpenLPClient::setSlidesVisible(bool value) {
	if (value) {
		showSlides();
	} else {
		blankScreen();
	}
}

void OpenLPClient::changeSong(int it) {
	auto n = QString::number(it);
	auto url = "/api/service/set?data=%7B%22request%22%3A+%7B%22id%22%3A+" + n + "%7D%7D&_=1627181837297";
	get(url);
}

void OpenLPClient::changeSlide(int slide) {
	auto n = QString::number(slide);
	auto url = R"(/api/controller/live/set?data={"request"%3A+{"id"%3A)" + n + "}}&_=1626628079579)";
	get(url);
}

void OpenLPClient::setBaseUrl() {
	auto const [host, port] = getOpenLPConnectionData();
	m_baseUrl = QString("http://%1:%2").arg(host, QString::number(port));
}

void OpenLPClient::get(QString const&urlExt) {
	QUrl url(m_baseUrl + urlExt);
	QNetworkRequest rqst(url);
	m_nam->get(rqst);
}

void OpenLPClient::post(QString const&url, QString const&data) {
	QNetworkRequest rqst(QUrl(m_baseUrl + url));
	rqst.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	m_nam->post(rqst, data.toUtf8());
}

void OpenLPClient::requestSongList() {
	QUrl url(m_baseUrl + "/api/service/list?_=1626628079579");
	QNetworkRequest rqst(url);
	m_songListNam->get(rqst);
}

void OpenLPClient::requestSlideList() {
	QUrl url(m_baseUrl + "/api/controller/live/text?_=1626628079579");
	QNetworkRequest rqst(url);
	m_slideListNam->get(rqst);
}

void OpenLPClient::poll() {
	QUrl url(m_baseUrl + "/api/poll?_=1626628079579");
	QNetworkRequest rqst(url);
	m_pollingNam->get(rqst);
}

void OpenLPClient::handleGeneralResponse(QNetworkReply *reply) {
	reply->deleteLater();
	if (reply->error()) {
		qDebug() << "OpenLPClient error response:" << reply->request().url() << ":" << reply->errorString();
	}
}

void OpenLPClient::handlePollResponse(QNetworkReply *reply) {
	reply->deleteLater();
	if (reply->error()) {
		qDebug() << "OpenLPClient error response:" << reply->errorString();
		emit pollFailed();
		m_currentServiceId = -1;
		m_currentSongId = "";
		m_songNameMap.clear();
		return;
	}
	auto data = reply->readAll();
	if (data.isEmpty()) {
		return;
	}
	auto doc = QJsonDocument::fromJson(data);
	auto obj = doc.object()["results"].toObject();
	auto songId = obj["item"].toString();
	auto slide = obj["slide"].toInt();
	auto service = obj["service"].toInt();
	if (service != m_currentServiceId) {
		requestSongList();
		m_currentServiceId = service;
	}
	if (m_currentSongId != songId) {
		requestSlideList();
		m_currentSongId = songId;
		emit songChanged(songId);
	}
	emit pollUpdate(m_songNameMap[songId], slide);
}

void OpenLPClient::handleSongListResponse(QNetworkReply *reply) {
	reply->deleteLater();
	if (reply->error()) {
		qDebug() << "OpenLPClient error response:" << reply->errorString();
	}
	auto data = reply->readAll();
	if (data.isEmpty()) {
		return;
	}
	auto doc = QJsonDocument::fromJson(data);
	auto items = doc.object()["results"].toObject()["items"].toArray();
	m_songNameMap.clear();
	m_songList.clear();
	for (auto const &item : items) {
		auto song = item.toObject();
		auto name = song["title"].toString();
		auto id = song["id"].toString();
		m_songNameMap[id] = name;
		m_songList.push_back(name);
	}
	emit songListUpdate(m_songList);
}

void OpenLPClient::handleSlideListResponse(QNetworkReply *reply) {
	reply->deleteLater();
	if (reply->error()) {
		qDebug() << "OpenLPClient error response:" << reply->errorString();
	}
	auto data = reply->readAll();
	if (data.isEmpty()) {
		return;
	}
	QStringList slideList;
	QStringList tagList;
	auto doc = QJsonDocument::fromJson(data);
	auto items = doc.object()["results"].toObject()["slides"].toArray();
	for (auto const &item : items) {
		auto slide = item.toObject();
		auto text = slide["text"].toString();
		auto tag = slide["tag"].toString();
		slideList.push_back(text);
		tagList.push_back(tag);
	}
	emit slideListUpdate(tagList, slideList);
}
