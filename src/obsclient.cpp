/*
 * Copyright 2021 - 2024 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

#include "settingsdata.hpp"
#include "obsclient.hpp"

OBSClient::OBSClient(QObject *parent): QObject(parent), m_socket(new QTcpSocket(this)) {
	connect(m_socket, &QTcpSocket::connected, this, &OBSClient::onConnected);
	connect(m_socket, &QTcpSocket::disconnected, this, &OBSClient::onDisconnected);
	connect(m_socket, &QTcpSocket::readyRead, this, &OBSClient::onReadyRead);
	connect(m_socket, &QTcpSocket::errorOccurred, this, &OBSClient::onErrorOccurred);
	setBaseUrl();
	m_pollTimer.start(1000);
	connect(&m_pollTimer, &QTimer::timeout, this, &OBSClient::poll);
}

void OBSClient::setScene(QString const &scene) {
	QJsonObject requestData;
	requestData["sceneName"] = scene;

	QJsonObject requestMsg;
	requestMsg["op"] = 6; // Request
	QJsonObject d;
	d["requestType"] = "SetCurrentProgramScene";
	d["requestId"] = QString::number(++m_requestId);
	d["requestData"] = requestData;
	requestMsg["d"] = d;

	sendTextFrame(QString::fromUtf8(QJsonDocument(requestMsg).toJson(QJsonDocument::Compact)));
}

void OBSClient::showSlides() {
	setScene(SceneSlides);
}

void OBSClient::hideSlides() {
	setScene(SceneNoSlides);
}

void OBSClient::setSlidesVisible(bool const state) {
	if (state) {
		setScene(SceneSlides);
	} else {
		setScene(SceneNoSlides);
	}
}

void OBSClient::setBaseUrl() {
	auto const [host, port, password] = getOBSConnectionData();
	bool const changed = (m_host != host || m_port != port || m_password != password);
	m_host = host;
	m_port = port;
	m_password = password;
	if (changed && m_socket->state() != QAbstractSocket::UnconnectedState) {
		m_socket->abort();
		m_state = State::Disconnected;
		m_readBuffer.clear();
	}
	if (m_state == State::Disconnected) {
		connectToHost();
	}
}

void OBSClient::connectToHost() {
	if (m_socket->state() == QAbstractSocket::ConnectedState ||
	    m_socket->state() == QAbstractSocket::ConnectingState) {
		return;
	}
	m_state = State::Connecting;
	m_readBuffer.clear();
	m_socket->connectToHost(m_host, m_port);
}

void OBSClient::poll() {
	if (m_state == State::Disconnected || m_socket->state() == QAbstractSocket::UnconnectedState) {
		connectToHost();
	} else if (m_state == State::Connected) {
		emit pollUpdate();
	}
}

void OBSClient::onConnected() {
	m_state = State::Handshaking;
	sendHandshake();
}

void OBSClient::sendHandshake() {
	QByteArray randomBytes(16, Qt::Uninitialized);
	for (qsizetype i = 0; i < 16; ++i) {
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
	).arg(m_host).arg(m_port).arg(QString::fromLatin1(m_secWebSocketKey));
	m_socket->write(request.toUtf8());
}

void OBSClient::onDisconnected() {
	m_state = State::Disconnected;
	m_readBuffer.clear();
	emit pollFailed();
}

void OBSClient::onErrorOccurred(QAbstractSocket::SocketError) {
	if (m_state != State::Disconnected) {
		m_state = State::Disconnected;
		m_readBuffer.clear();
		emit pollFailed();
	}
}

void OBSClient::onReadyRead() {
	m_readBuffer.append(m_socket->readAll());

	if (m_state == State::Handshaking) {
		processHandshake();
	}
	if (m_state == State::Identifying || m_state == State::Connected) {
		processFrames();
	}
}

void OBSClient::processHandshake() {
	qsizetype const headerEnd = m_readBuffer.indexOf("\r\n\r\n");
	if (headerEnd == -1) {
		return;
	}
	QByteArray const headers = m_readBuffer.left(headerEnd);
	m_readBuffer.remove(0, headerEnd + 4);
	if (!headers.startsWith("HTTP/1.1 101") && !headers.startsWith("HTTP/1.0 101")) {
		qDebug() << "OBSClient WebSocket handshake failed:" << headers;
		m_socket->abort();
		m_state = State::Disconnected;
		emit pollFailed();
		return;
	}
	m_state = State::Identifying;
}

void OBSClient::processFrames() {
	while (!m_readBuffer.isEmpty()) {
		if (m_readBuffer.size() < 2) {
			return;
		}

		auto const b0 = static_cast<quint8>(m_readBuffer[0]);
		auto const b1 = static_cast<quint8>(m_readBuffer[1]);

		int const opcode = (b0 & 0x0F);
		bool const masked = (b1 & 0x80) != 0;
		quint64 payloadLen = (b1 & 0x7F);

		qsizetype headerSize = 2;
		if (payloadLen == 126) {
			if (m_readBuffer.size() < headerSize + 2) {
				return;
			}
			payloadLen = (static_cast<quint64>(static_cast<quint8>(m_readBuffer[2])) << 8) |
			              static_cast<quint64>(static_cast<quint8>(m_readBuffer[3]));
			headerSize += 2;
		} else if (payloadLen == 127) {
			if (m_readBuffer.size() < headerSize + 8) {
				return;
			}
			payloadLen = 0;
			for (qsizetype i = 0; i < 8; ++i) {
				payloadLen = (payloadLen << 8) | static_cast<quint64>(static_cast<quint8>(m_readBuffer[headerSize + i]));
			}
			headerSize += 8;
		}

		QByteArray maskKey;
		if (masked) {
			if (m_readBuffer.size() < headerSize + 4) {
				return;
			}
			maskKey = m_readBuffer.mid(headerSize, 4);
			headerSize += 4;
		}

		if (static_cast<quint64>(m_readBuffer.size()) < static_cast<quint64>(headerSize) + payloadLen) {
			return;
		}

		QByteArray payload = m_readBuffer.mid(headerSize, static_cast<qsizetype>(payloadLen));
		m_readBuffer.remove(0, headerSize + static_cast<qsizetype>(payloadLen));

		if (masked && maskKey.size() == 4) {
			for (qsizetype i = 0; i < payload.size(); ++i) {
				payload[i] = static_cast<char>(payload[i] ^ maskKey[i % 4]);
			}
		}

		if (opcode == 0x1) { // Text frame
			handleMessage(QString::fromUtf8(payload));
		} else if (opcode == 0x8) { // Close frame
			m_socket->disconnectFromHost();
			m_state = State::Disconnected;
			emit pollFailed();
			return;
		} else if (opcode == 0x9) { // Ping frame
			sendPong(payload);
		}
	}
}

void OBSClient::sendPong(QByteArray const &payload) {
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
		maskedPayload[i] = static_cast<char>(maskedPayload[i] ^ maskBytes[i % 4]);
	}
	frame.append(maskedPayload);

	m_socket->write(frame);
}

void OBSClient::sendTextFrame(QString const &text) {
	if (m_socket->state() != QAbstractSocket::ConnectedState) {
		return;
	}

	QByteArray const payload = text.toUtf8();
	QByteArray frame;
	frame.append(static_cast<char>(0x81)); // FIN=1, Opcode=0x1 (Text)

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
		maskedPayload[i] = static_cast<char>(maskedPayload[i] ^ maskBytes[i % 4]);
	}
	frame.append(maskedPayload);

	m_socket->write(frame);
}

void OBSClient::handleMessage(QString const &message) {
	QJsonParseError err;
	auto const doc = QJsonDocument::fromJson(message.toUtf8(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		return;
	}

	auto const root = doc.object();
	int const op = root["op"].toInt();
	auto const d = root["d"].toObject();

	if (op == 0) { // Hello
		QJsonObject identifyData;
		identifyData["rpcVersion"] = 1;
		identifyData["eventSubscriptions"] = 0;

		if (d.contains("authentication")) {
			auto const auth = d["authentication"].toObject();
			QString const challenge = auth["challenge"].toString();
			QString const salt = auth["salt"].toString();

			QByteArray const passSalt = (m_password + salt).toUtf8();
			QByteArray const secret = QCryptographicHash::hash(passSalt, QCryptographicHash::Sha256).toBase64();
			QByteArray const authInput = secret + challenge.toUtf8();
			QString const authResponse = QString::fromUtf8(QCryptographicHash::hash(authInput, QCryptographicHash::Sha256).toBase64());

			identifyData["authentication"] = authResponse;
		}

		QJsonObject identifyMsg;
		identifyMsg["op"] = 1; // Identify
		identifyMsg["d"] = identifyData;

		sendTextFrame(QString::fromUtf8(QJsonDocument(identifyMsg).toJson(QJsonDocument::Compact)));
	} else if (op == 2) { // Identified
		m_state = State::Connected;
		emit pollUpdate();
	} else if (op == 7) { // RequestResponse
		auto const requestStatus = d["requestStatus"].toObject();
		if (!requestStatus["result"].toBool()) {
			qDebug() << "OBSClient request failed:" << requestStatus["comment"].toString();
		}
	}
}

