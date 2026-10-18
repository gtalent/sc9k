/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "settingsdata.hpp"
#include "openlpclient.hpp"

OpenLPClient::OpenLPClient(QObject *parent): QObject(parent) {
	connect(&m_ws, &QWebSocket::connected, this, &OpenLPClient::onWsConnected);
	connect(&m_ws, &QWebSocket::disconnected, this, &OpenLPClient::onWsDisconnected);
	// OpenLP sends its JSON as binary frames
	connect(&m_ws, &QWebSocket::binaryMessageReceived, this, &OpenLPClient::onWsMessage);
	connect(&m_ws, &QWebSocket::textMessageReceived, this, [this](QString const &message) {
		onWsMessage(message.toUtf8());
	});
	connect(&m_ws, &QWebSocket::pong, this, [this] {
		m_pongReceived = true;
	});
	// OpenLP only pushes on changes, so ping to detect a dead connection
	connect(&m_pingTimer, &QTimer::timeout, this, [this] {
		if (!m_pongReceived) {
			m_ws.abort();
			return;
		}
		m_pongReceived = false;
		m_ws.ping();
	});
	connect(&m_reconnectTimer, &QTimer::timeout, this, &OpenLPClient::connectToOpenLP);
	m_pingTimer.setInterval(5000);
	m_reconnectTimer.setInterval(2000);
	m_reconnectTimer.start();
	reloadConnectionSettings();
}

QString OpenLPClient::getNextSong() {
	auto const songIdx = m_songIdList.indexOf(m_currentSongId) + 1;
	if (songIdx > 0 && songIdx < m_songList.size()) {
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
	post("/api/v2/core/display", R"({"display":"hide"})");
}

void OpenLPClient::showSlides() {
	post("/api/v2/core/display", R"({"display":"show"})");
}

void OpenLPClient::setSlidesVisible(bool const value) {
	if (value) {
		showSlides();
	} else {
		blankScreen();
	}
}

void OpenLPClient::changeSong(int const it) {
	// OpenLP treats an index of 0 as a missing ID, so select by the item's UUID
	if (it < 0 || it >= m_songIdList.size()) {
		return;
	}
	post("/api/v2/service/show", QString(R"({"id":"%1"})").arg(m_songIdList[it]));
}

void OpenLPClient::changeSlide(int const slide) {
	post("/api/v2/controller/show", QString(R"({"id":%1})").arg(slide));
}

void OpenLPClient::reloadConnectionSettings() {
	auto const [host, port] = getOpenLPConnectionData();
	if (m_host == host && m_httpPort == port) {
		return;
	}
	m_host = host;
	m_httpPort = port;
	m_baseUrl = QString("http://%1:%2").arg(host, QString::number(port));
	m_ws.abort();
	connectToOpenLP();
}

void OpenLPClient::post(QString const &urlExt, QString const &data) {
	QNetworkRequest rqst(QUrl(m_baseUrl + urlExt));
	rqst.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	auto const reply = m_nam->post(rqst, data.toUtf8());
	connect(reply, &QNetworkReply::finished, this, [reply] {
		reply->deleteLater();
		if (reply->error()) {
			qDebug() << "OpenLPClient error response:" << reply->request().url() << ":" << reply->errorString();
		}
	});
}

void OpenLPClient::requestSongList() {
	auto const reply = m_nam->get(QNetworkRequest(QUrl(m_baseUrl + "/api/v2/service/items")));
	connect(reply, &QNetworkReply::finished, this, [this, reply] {
		reply->deleteLater();
		if (reply->error()) {
			qDebug() << "OpenLPClient error response:" << reply->errorString();
			return;
		}
		QStringList songList;
		QStringList songIdList;
		for (auto const &item : QJsonDocument::fromJson(reply->readAll()).array()) {
			auto const song = item.toObject();
			songList.push_back(song["title"].toString());
			songIdList.push_back(song["id"].toString());
		}
		m_songIdList = std::move(songIdList);
		if (m_songList != songList) {
			m_songList = std::move(songList);
			emit songListUpdate(m_songList);
			emit songChanged(m_currentSongId);
		}
		emitUpdate();
	});
}

void OpenLPClient::requestSlideList() {
	auto const reply = m_nam->get(QNetworkRequest(QUrl(m_baseUrl + "/api/v2/controller/live-items")));
	connect(reply, &QNetworkReply::finished, this, [this, reply] {
		reply->deleteLater();
		if (reply->error()) {
			qDebug() << "OpenLPClient error response:" << reply->errorString();
			return;
		}
		auto const item = QJsonDocument::fromJson(reply->readAll()).object();
		// a newer request is in flight if the live item has changed since this one was sent
		if (item["id"].toString() != m_currentSongId) {
			return;
		}
		QStringList slideList;
		QStringList tagList;
		for (auto const &s : item["slides"].toArray()) {
			auto const slide = s.toObject();
			auto text = slide["text"].toString();
			if (text.isEmpty()) {
				text = slide["title"].toString();
			}
			slideList.push_back(std::move(text));
			tagList.push_back(slide["tag"].toVariant().toString());
		}
		if (m_currentSlideList != slideList || m_currentTagList != tagList) {
			m_currentSlideList = std::move(slideList);
			m_currentTagList = std::move(tagList);
			emit slideListUpdate(m_currentTagList, m_currentSlideList);
		}
		emitUpdate();
	});
}

void OpenLPClient::resetState() {
	m_currentServiceId = -1;
	m_currentSongId = "";
	m_currentSlide = 0;
	m_currentSlideList.clear();
	m_currentTagList.clear();
	m_songList.clear();
	m_songIdList.clear();
}

void OpenLPClient::emitUpdate() {
	// identify the song by position, as titles are not unique
	emit pollUpdate(static_cast<int>(m_songIdList.indexOf(m_currentSongId)), m_currentSlide);
}

void OpenLPClient::connectToOpenLP() {
	if (m_ws.state() != QAbstractSocket::UnconnectedState) {
		return;
	}
	// ask OpenLP which port its WebSocket server is on before connecting
	QNetworkRequest rqst(QUrl(m_baseUrl + "/api/v2/core/system"));
	rqst.setTransferTimeout(m_reconnectTimer.interval());
	auto const reply = m_nam->get(rqst);
	connect(reply, &QNetworkReply::finished, this, [this, reply, baseUrl = m_baseUrl] {
		reply->deleteLater();
		if (reply->error() || baseUrl != m_baseUrl || m_ws.state() != QAbstractSocket::UnconnectedState) {
			return;
		}
		auto const system = QJsonDocument::fromJson(reply->readAll()).object();
		auto const wsPort = system["websocket_port"].toInt();
		if (wsPort > 0) {
			m_wsPort = static_cast<uint16_t>(wsPort);
		}
		m_ws.open(QUrl(QString("ws://%1:%2").arg(m_host, QString::number(m_wsPort))));
	});
}

void OpenLPClient::onWsConnected() {
	m_connected = true;
	m_reconnectTimer.stop();
	m_pongReceived = true;
	m_pingTimer.start();
}

void OpenLPClient::onWsDisconnected() {
	m_pingTimer.stop();
	if (m_connected) {
		m_connected = false;
		resetState();
		emit pollFailed();
	}
	if (!m_reconnectTimer.isActive()) {
		m_reconnectTimer.start();
	}
}

void OpenLPClient::onWsMessage(QByteArray const &message) {
	// OpenLP sends the full state on connect and again whenever the live slide or the service changes:
	// {"results": {"service": int, "item": uuid, "slide": int, "blank": bool, ...}}
	auto const state = QJsonDocument::fromJson(message).object()["results"].toObject();
	if (state.isEmpty()) {
		return;
	}
	auto const service = state["service"].toInt();
	auto const songId = state["item"].toVariant().toString();
	m_currentSlide = state["slide"].toInt();
	auto const serviceChanged = service != m_currentServiceId;
	auto const itemChanged = songId != m_currentSongId;
	if (serviceChanged) {
		// service ID increments on any service modification, including edits to the live item
		m_currentServiceId = service;
		requestSongList();
	}
	if (itemChanged) {
		m_currentSongId = songId;
		emit songChanged(songId);
	}
	if (serviceChanged || itemChanged) {
		requestSlideList();
	}
	emitUpdate();
}
