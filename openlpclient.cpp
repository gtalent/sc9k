/*
 * Copyright 2021 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValueRef>
#include <QNetworkReply>

#include "openlpclient.hpp"

OpenLPClient::OpenLPClient(QObject *parent): QObject(parent) {
	poll();
	m_pollTimer.start(500);
	connect(&m_pollTimer, &QTimer::timeout, this, &OpenLPClient::poll);
	connect(m_nam, &QNetworkAccessManager::finished, this, &OpenLPClient::handleGeneralResponse);
	connect(m_songListNam, &QNetworkAccessManager::finished, this, &OpenLPClient::handleSongListResponse);
	connect(m_slideListNam, &QNetworkAccessManager::finished, this, &OpenLPClient::handleSlideListResponse);
	connect(m_pollingNam, &QNetworkAccessManager::finished, this, &OpenLPClient::handlePollResponse);
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

void OpenLPClient::get(QString urlExt) {
	QUrl url(QString(BaseUrl) + urlExt);
	QNetworkRequest rqst(url);
	m_nam->get(rqst);
}

void OpenLPClient::requestSongList() {
	QUrl url(QString(BaseUrl) + "/api/service/list?_=1626628079579");
	QNetworkRequest rqst(url);
	m_songListNam->get(rqst);
}

void OpenLPClient::requestSlideList() {
	QUrl url(QString(BaseUrl) + "/api/controller/live/text?_=1626628079579");
	QNetworkRequest rqst(url);
	m_slideListNam->get(rqst);
}

void OpenLPClient::poll() {
	QUrl url(QString(BaseUrl) + "/api/poll?_=1626628079579");
	QNetworkRequest rqst(url);
	m_pollingNam->get(rqst);
}

void OpenLPClient::handleGeneralResponse(QNetworkReply *reply) {
	if (reply->error()) {
		qDebug() << reply->request().url() << ":" << reply->errorString();
	}
}

void OpenLPClient::handlePollResponse(QNetworkReply *reply) {
	if (reply->error()) {
		qDebug() << reply->errorString();
		emit pollFailed();
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
	}
	emit pollUpdate(m_songNameMap[songId], slide);
}

void OpenLPClient::handleSongListResponse(QNetworkReply *reply) {
	if (reply->error()) {
		qDebug() << reply->errorString();
	}
	auto data = reply->readAll();
	if (data.isEmpty()) {
		return;
	}
	QStringList songList;
	auto doc = QJsonDocument::fromJson(data);
	auto items = doc.object()["results"].toObject()["items"].toArray();
	m_songNameMap.clear();
	for (const auto &item : items) {
		auto song = item.toObject();
		auto name = song["title"].toString();
		auto id = song["id"].toString();
		m_songNameMap[id] = name;
		songList.push_back(name);
	}
	emit songListUpdate(songList);
}

void OpenLPClient::handleSlideListResponse(QNetworkReply *reply) {
	if (reply->error()) {
		qDebug() << reply->errorString();
	}
	auto data = reply->readAll();
	if (data.isEmpty()) {
		return;
	}
	QStringList slideList;
	QStringList tagList;
	auto doc = QJsonDocument::fromJson(data);
	auto items = doc.object()["results"].toObject()["slides"].toArray();
	for (const auto &item : items) {
		auto slide = item.toObject();
		auto text = slide["text"].toString();
		auto tag = slide["tag"].toString();
		slideList.push_back(text);
		tagList.push_back(tag);
	}
	emit slideListUpdate(tagList, slideList);
}
