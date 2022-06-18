/*
 * Copyright (C) 2012-2016 Fanout, Inc.
 *
 * This file is part of P-8.
 *
 * P-8 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * P-8 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ZHTTPREQUEST_H
#define ZHTTPREQUEST_H

#include <QVariant>
#include "httprequest.h"

class ZhttpRequestPacket;
class ZhttpResponsePacket;
class ZhttpManager;

class ZhttpRequest : public HttpRequest
{
	Q_OBJECT

public:
	// pair of sender + request id
	typedef QPair<QByteArray, QByteArray> Rid;

	class ServerState
	{
	public:
		Rid rid;
		QHostAddress peerAddress;
		QString requestMethod;
		QUrl requestUri;
		HttpHeaders requestHeaders;
		QByteArray requestBody;
		int responseCode;
		int inSeq;
		int outSeq;
		int outCredits;
		QVariant userData;

		ServerState() :
			responseCode(-1),
			inSeq(-1),
			outSeq(-1),
			outCredits(-1)
		{
		}
	};

	~ZhttpRequest();

	Rid rid() const;
	void setIsTls(bool on); // updates scheme
	void setSendBodyAfterAcknowledgement(bool on); // only works in push/sub mode

	// for server requests only
	void pause();
	void resume();
	ServerState serverState() const;

	// reimplemented

	virtual QHostAddress peerAddress() const;

	virtual void setConnectHost(const QString &host);
	virtual void setConnectPort(int port);
	virtual void setIgnorePolicies(bool on);
	virtual void setTrustConnectHost(bool on);
	virtual void setIgnoreTlsErrors(bool on);

	virtual void start(const QString &method, const QUrl &uri, const HttpHeaders &headers);
	virtual void beginResponse(int code, const QByteArray &reason, const HttpHeaders &headers);

	virtual void writeBody(const QByteArray &body);

	virtual void endBody();

	virtual int bytesAvailable() const;
	virtual int writeBytesAvailable() const;
	virtual bool isFinished() const;
	virtual bool isInputFinished() const;
	virtual bool isOutputFinished() const;
	virtual bool isErrored() const;
	virtual ErrorCondition errorCondition() const;

	virtual QString requestMethod() const;
	virtual QUrl requestUri() const;
	virtual HttpHeaders requestHeaders() const;

	virtual int responseCode() const;
	virtual QByteArray responseReason() const;
	virtual HttpHeaders responseHeaders() const;

	virtual QByteArray readBody(int size = -1);

private:
	class Private;
	friend class Private;
	Private *d;

	friend class ZhttpManager;
	ZhttpRequest(QObject *parent = 0);
	void setupClient(ZhttpManager *manager, bool req);
	bool setupServer(ZhttpManager *manager, const ZhttpRequestPacket &packet);
	void setupServer(ZhttpManager *manager, const ServerState &state);
	void startServer();
	bool isServer() const;
	void handle(const ZhttpRequestPacket &packet);
	void handle(const ZhttpResponsePacket &packet);
};

#endif
