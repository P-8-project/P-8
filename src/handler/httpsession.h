/*
 * Copyright (C) 2016 Fanout, Inc.
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

#ifndef HTTPSESSION_H
#define HTTPSESSION_H

#include <QObject>
#include "packet/httprequestdata.h"
#include "packet/httpresponsedata.h"
#include "zhttprequest.h"
#include "instruct.h"

class QTimer;
class ZhttpManager;
class StatsManager;

class HttpSession : public QObject
{
	Q_OBJECT

public:
	class AcceptData
	{
	public:
		QHostAddress peerAddress;
		bool debug;
		bool autoCrossOrigin;
		QByteArray jsonpCallback;
		bool jsonpExtendedResponse;
		HttpRequestData requestData;
		QString route;
		QString channelPrefix;
		QByteArray sigIss;
		QByteArray sigKey;
		bool trusted;
		bool responseSent;
		QString sid;

		AcceptData() :
			trusted(false),
			responseSent(false)
		{
		}
	};

	HttpSession(ZhttpRequest *req, const HttpSession::AcceptData &adata, const Instruct &instruct, ZhttpManager *outZhttp, StatsManager *stats, QObject *parent = 0);
	~HttpSession();

	Instruct::HoldMode holdMode() const;
	ZhttpRequest::Rid rid() const;
	QUrl requestUri() const;
	QString route() const;
	QString sid() const;
	QHash<QString, Instruct::Channel> channels() const;
	QHash<QString, QString> meta() const;

	void start();
	void respond(int code, const QByteArray &reason, const HttpHeaders &headers, const QByteArray &body, const QList<QByteArray> &exposeHeaders);
	void respond(int code, const QByteArray &reason, const HttpHeaders &headers, const QVariantList &bodyPatch, const QList<QByteArray> &exposeHeaders);
	void stream(const QByteArray &content);
	void close();

signals:
	void subscribe(const QString &channel);
	void unsubscribe(const QString &channel);
	void finished();

private:
	class Private;
	friend class Private;
	Private *d;
};

#endif
