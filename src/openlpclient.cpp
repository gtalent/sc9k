/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
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
#include <QRandomGenerator>
#include <QSettings>

#include "settingsdata.hpp"
#include "openlpclient.hpp"

OpenLPClient::OpenLPClient(QObject *parent): QObject(parent), m_wsSocket(new QTcpSocket(this)) {
	connect(m_wsSocket, &QTcpSocket::connected, this, &OpenLPClient::onWsConnected);
	connect(m_wsSocket, &QTcpSocket::disconnected, this, &OpenLPClient::onWsDisconnected);
	connect(m_wsSocket, &QTcpSocket::readyRead, this, &OpenLPClient::onWsReadyRead);
	connect(m_wsSocket, &QTcpSocket::errorOccurred, this, &OpenLPClient::onWsErrorOccurred);

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
	post("/api/v2/service/show", QString(R"({"id":%1})").arg(it));
}

void OpenLPClient::changeSlide(int const slide) {
	post("/api/v2/controller/show", QString(R"({"id":%1})").arg(slide));
}

void OpenLPClient::setBaseUrl() {
	auto const [host, port] = getOpenLPConnectionData();
	bool const changed = (m_host != host || m_httpPort != port);
	m_host = host;
	m_httpPort = port;
	m_baseUrl = QString("http://%1:%2").arg(host, QString::number(port));

	if (changed && m_wsSocket->state() != QAbstractSocket::UnconnectedState) {
		m_wsSocket->abort();
		m_wsConnected = false;
		m_wsReadBuffer.clear();
	}
	if (!m_wsConnected && m_wsSocket->state() == QAbstractSocket::UnconnectedState) {
		connectWebSocket();
	}
}

void OpenLPClient::get(QString const &urlExt) {
	QUrl url(m_baseUrl + urlExt);
	QNetworkRequest rqst(url);
	m_nam->get(rqst);
}

void OpenLPClient::post(QString const &url, QString const &data) {
	QNetworkRequest rqst(QUrl(m_baseUrl + url));
	rqst.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	m_nam->post(rqst, data.toUtf8());
}

void OpenLPClient::requestSongList() {
	QUrl url(m_baseUrl + "/api/v2/service/items");
	QNetworkRequest rqst(url);
	m_songListNam->get(rqst);
}

void OpenLPClient::requestSlideList() {
	QUrl url(m_baseUrl + "/api/v2/controller/live-items");
	QNetworkRequest rqst(url);
	m_slideListNam->get(rqst);
}

void OpenLPClient::connectWebSocket() {
	if (m_wsSocket->state() == QAbstractSocket::ConnectedState ||
	    m_wsSocket->state() == QAbstractSocket::ConnectingState) {
		return;
	}
	m_wsConnected = false;
	m_wsReadBuffer.clear();
	m_wsSocket->connectToHost(m_host, m_wsPort);
}

void OpenLPClient::poll() {
	if (!m_wsConnected && m_wsSocket->state() == QAbstractSocket::UnconnectedState) {
		connectWebSocket();
	}
	QUrl url(m_baseUrl + "/api/v2/core/system");
	QNetworkRequest rqst(url);
	m_pollingNam->get(rqst);
}

void OpenLPClient::onWsConnected() {
	sendWebSocketHandshake();
}

void OpenLPClient::sendWebSocketHandshake() {
	QByteArray randomBytes(16, Qt::Uninitialized);
	for (int i = 0; i < 16; ++i) {
		randomBytes[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
	}
	m_secWebSocketKey = randomBytes.toBase64();

	QString const request = QString(
		"GET / HTTP/1.1\r\n"
		"Host: %1:%2\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: %3\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n"
	).arg(m_host).arg(m_wsPort).arg(QString::fromLatin1(m_secWebSocketKey));

	m_wsSocket->write(request.toUtf8());
}

void OpenLPClient::onWsDisconnected() {
	m_wsConnected = false;
	m_wsReadBuffer.clear();
}

void OpenLPClient::onWsErrorOccurred(QAbstractSocket::SocketError) {
	m_wsConnected = false;
	m_wsReadBuffer.clear();
}

void OpenLPClient::onWsReadyRead() {
	m_wsReadBuffer.append(m_wsSocket->readAll());

	if (!m_wsConnected) {
		processWebSocketHandshake();
	}
	if (m_wsConnected) {
		processWebSocketFrames();
	}
}

void OpenLPClient::processWebSocketHandshake() {
	qsizetype const headerEnd = m_wsReadBuffer.indexOf("\r\n\r\n");
	if (headerEnd == -1) {
		return;
	}

	QByteArray const headers = m_wsReadBuffer.left(headerEnd);
	m_wsReadBuffer.remove(0, headerEnd + 4);

	if (!headers.startsWith("HTTP/1.1 101") && !headers.startsWith("HTTP/1.0 101")) {
		m_wsSocket->abort();
		m_wsConnected = false;
		return;
	}

	m_wsConnected = true;
	processWebSocketFrames();
}

void OpenLPClient::processWebSocketFrames() {
	while (!m_wsReadBuffer.isEmpty()) {
		if (m_wsReadBuffer.size() < 2) {
			return;
		}

		auto const b0 = static_cast<quint8>(m_wsReadBuffer[0]);
		auto const b1 = static_cast<quint8>(m_wsReadBuffer[1]);

		int const opcode = (b0 & 0x0F);
		bool const masked = (b1 & 0x80) != 0;
		quint64 payloadLen = (b1 & 0x7F);

		qsizetype headerSize = 2;
		if (payloadLen == 126) {
			if (m_wsReadBuffer.size() < headerSize + 2) {
				return;
			}
			payloadLen = (static_cast<quint64>(static_cast<quint8>(m_wsReadBuffer[2])) << 8) |
			              static_cast<quint64>(static_cast<quint8>(m_wsReadBuffer[3]));
			headerSize += 2;
		} else if (payloadLen == 127) {
			if (m_wsReadBuffer.size() < headerSize + 8) {
				return;
			}
			payloadLen = 0;
			for (qsizetype i = 0; i < 8; ++i) {
				payloadLen = (payloadLen << 8) | static_cast<quint64>(static_cast<quint8>(m_wsReadBuffer[headerSize + i]));
			}
			headerSize += 8;
		}

		QByteArray maskKey;
		if (masked) {
			if (m_wsReadBuffer.size() < headerSize + 4) {
				return;
			}
			maskKey = m_wsReadBuffer.mid(headerSize, 4);
			headerSize += 4;
		}

		if (static_cast<quint64>(m_wsReadBuffer.size()) < static_cast<quint64>(headerSize) + payloadLen) {
			return;
		}

		QByteArray payload = m_wsReadBuffer.mid(headerSize, static_cast<qsizetype>(payloadLen));
		m_wsReadBuffer.remove(0, headerSize + static_cast<qsizetype>(payloadLen));

		if (masked && maskKey.size() == 4) {
			for (qsizetype i = 0; i < payload.size(); ++i) {
				payload[i] = payload[i] ^ maskKey[i % 4];
			}
		}

		if (opcode == 0x1) { // Text frame
			handleWebSocketMessage(QString::fromUtf8(payload));
		} else if (opcode == 0x8) { // Close frame
			m_wsSocket->disconnectFromHost();
			m_wsConnected = false;
			return;
		} else if (opcode == 0x9) { // Ping frame
			sendWebSocketPong(payload);
		}
	}
}

void OpenLPClient::sendWebSocketPong(QByteArray const &payload) {
	if (m_wsSocket->state() != QAbstractSocket::ConnectedState) {
		return;
	}
	QByteArray frame;
	frame.append(static_cast<char>(0x8A)); // FIN=1, Opcode=0xA (Pong)

	auto const len = payload.size();
	quint8 const maskBit = 0x80;

	if (len < 126) {
		frame.append(static_cast<char>(maskBit | static_cast<quint8>(len)));
	} else if (len <= 0xFFFF) {
		frame.append(static_cast<char>(maskBit | 126));
		frame.append(static_cast<char>((len >> 8) & 0xFF));
		frame.append(static_cast<char>(len & 0xFF));
	} else {
		frame.append(static_cast<char>(maskBit | 127));
		for (int i = 7; i >= 0; --i) {
			frame.append(static_cast<char>((static_cast<quint64>(len) >> (i * 8)) & 0xFF));
		}
	}

	quint32 const mask = QRandomGenerator::global()->generate();
	char const maskBytes[4] = {
		static_cast<char>((mask >> 24) & 0xFF),
		static_cast<char>((mask >> 16) & 0xFF),
		static_cast<char>((mask >> 8) & 0xFF),
		static_cast<char>(mask & 0xFF)
	};
	frame.append(maskBytes, 4);

	QByteArray maskedPayload = payload;
	for (qsizetype i = 0; i < maskedPayload.size(); ++i) {
		maskedPayload[i] = maskedPayload[i] ^ maskBytes[i % 4];
	}
	frame.append(maskedPayload);

	m_wsSocket->write(frame);
}

void OpenLPClient::handleWebSocketMessage(QString const &message) {
	QJsonParseError err;
	auto const doc = QJsonDocument::fromJson(message.toUtf8(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		return;
	}
	handleStateJson(doc.object());
}

void OpenLPClient::handleStateJson(QJsonObject const &obj) {
	QJsonObject results;
	if (obj.contains("results")) {
		results = obj["results"].toObject();
	} else {
		results = obj;
	}
	if (results.isEmpty()) {
		return;
	}
	auto songId = results["item"].toVariant().toString();
	auto slide = results["slide"].toInt();
	auto service = results["service"].toInt();

	m_currentSlide = slide;

	if (service != m_currentServiceId) {
		requestSongList();
		m_currentServiceId = service;
	}
	if (m_currentSongId != songId) {
		requestSlideList();
		m_currentSongId = songId;
		emit songChanged(songId);
	}
	emit pollUpdate(m_songNameMap.value(songId), slide);
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
		if (!m_wsConnected) {
			qDebug() << "OpenLPClient error response:" << reply->errorString();
			emit pollFailed();
			m_currentServiceId = -1;
			m_currentSongId = "";
			m_songNameMap.clear();
		}
		return;
	}
	auto data = reply->readAll();
	if (data.isEmpty()) {
		return;
	}
	auto doc = QJsonDocument::fromJson(data);
	if (doc.isObject()) {
		auto rootObj = doc.object();
		if (rootObj.contains("websocket_port")) {
			auto newWsPort = static_cast<uint16_t>(rootObj["websocket_port"].toInt());
			if (newWsPort != 0 && newWsPort != m_wsPort) {
				m_wsPort = newWsPort;
				if (m_wsSocket->state() != QAbstractSocket::UnconnectedState) {
					m_wsSocket->abort();
					m_wsConnected = false;
				}
				connectWebSocket();
			}
			if (!m_wsConnected && m_currentSongId.isEmpty()) {
				requestSlideList();
				requestSongList();
			}
		} else if (rootObj.contains("results")) {
			handleStateJson(rootObj);
		}
	}
}

void OpenLPClient::handleSongListResponse(QNetworkReply *reply) {
	reply->deleteLater();
	if (reply->error()) {
		qDebug() << "OpenLPClient error response:" << reply->errorString();
		return;
	}
	auto data = reply->readAll();
	if (data.isEmpty()) {
		return;
	}
	auto doc = QJsonDocument::fromJson(data);
	QJsonArray items;
	if (doc.isArray()) {
		items = doc.array();
	} else if (doc.isObject()) {
		auto rootObj = doc.object();
		if (rootObj.contains("results")) {
			items = rootObj["results"].toObject()["items"].toArray();
		} else if (rootObj.contains("items")) {
			items = rootObj["items"].toArray();
		}
	}
	m_songNameMap.clear();
	m_songList.clear();
	for (auto const &item : items) {
		auto song = item.toObject();
		auto name = song["title"].toString();
		auto id = song["id"].toVariant().toString();
		m_songNameMap[id] = name;
		m_songList.push_back(name);
	}
	emit songListUpdate(m_songList);
	if (!m_currentSongId.isEmpty() && m_songNameMap.contains(m_currentSongId)) {
		emit pollUpdate(m_songNameMap[m_currentSongId], m_currentSlide);
	}
}

void OpenLPClient::handleSlideListResponse(QNetworkReply *reply) {
	reply->deleteLater();
	if (reply->error()) {
		qDebug() << "OpenLPClient error response:" << reply->errorString();
		return;
	}
	auto data = reply->readAll();
	if (data.isEmpty()) {
		return;
	}
	QStringList slideList;
	QStringList tagList;
	auto doc = QJsonDocument::fromJson(data);
	QJsonArray items;
	int selectedSlide = -1;
	QString itemId;

	if (doc.isObject()) {
		auto rootObj = doc.object();
		if (rootObj.contains("id")) {
			itemId = rootObj["id"].toVariant().toString();
		}
		if (rootObj.contains("slides")) {
			items = rootObj["slides"].toArray();
		} else if (rootObj.contains("results")) {
			auto resultsObj = rootObj["results"].toObject();
			if (resultsObj.contains("slides")) {
				items = resultsObj["slides"].toArray();
			}
		}
	} else if (doc.isArray()) {
		items = doc.array();
	}

	for (int i = 0; i < items.size(); ++i) {
		auto const slide = items[i].toObject();
		if (slide["selected"].toBool()) {
			selectedSlide = i;
		}
		auto text = slide["text"].toString();
		if (text.isEmpty() && slide.contains("title")) {
			text = slide["title"].toString();
		}
		auto tag = slide["tag"].toVariant().toString();
		slideList.push_back(std::move(text));
		tagList.push_back(std::move(tag));
	}

	if (!itemId.isEmpty() && m_currentSongId != itemId) {
		m_currentSongId = itemId;
		emit songChanged(itemId);
	}
	if (selectedSlide >= 0) {
		m_currentSlide = selectedSlide;
	}

	emit slideListUpdate(tagList, slideList);

	if (!m_currentSongId.isEmpty() && m_songNameMap.contains(m_currentSongId)) {
		emit pollUpdate(m_songNameMap[m_currentSongId], m_currentSlide);
	}
}
