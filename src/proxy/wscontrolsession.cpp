/*
 * Copyright (C) 2014-2017 Fanout, Inc.
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

#include "wscontrolsession.h"

#include <assert.h>
#include <QTimer>
#include <QDateTime>
#include <QUrl>
#include "wscontrolmanager.h"

#define SESSION_TTL 60
#define REQUEST_TIMEOUT 8000

class WsControlSession::Private : public QObject
{
	Q_OBJECT

public:
	WsControlSession *q;
	WsControlManager *manager;
	int nextReqId;
	QHash<int, qint64> pendingRequests;
	QTimer *requestTimer;
	QByteArray cid;
	QByteArray route;
	QByteArray channelPrefix;
	QUrl uri;

	Private(WsControlSession *_q) :
		QObject(_q),
		q(_q),
		manager(0),
		nextReqId(0)
	{
		requestTimer = new QTimer(this);
		requestTimer->setSingleShot(true);
		connect(requestTimer, &QTimer::timeout, this, &Private::requestTimer_timeout);
	}

	~Private()
	{
		cleanup();

		requestTimer->setParent(0);
		requestTimer->disconnect(this);
		requestTimer->deleteLater();
	}

	void cleanup()
	{
		if(manager)
		{
			manager->unregisterKeepAlive(q);

			WsControlPacket::Item i;
			i.type = WsControlPacket::Item::Gone;
			write(i);

			manager->unlink(cid);
			manager = 0;
		}
	}

	void start()
	{
		manager->registerKeepAlive(q);

		WsControlPacket::Item i;
		i.type = WsControlPacket::Item::Here;
		i.route = route;
		i.channelPrefix = channelPrefix;
		i.uri = uri;
		i.ttl = SESSION_TTL;
		write(i);
	}

	void setupRequestTimer()
	{
		if(!pendingRequests.isEmpty())
		{
			// find next expiring request
			qint64 lowestTime = -1;
			QHashIterator<int, qint64> it(pendingRequests);
			while(it.hasNext())
			{
				it.next();
				qint64 time = it.value();

				if(lowestTime == -1 || time < lowestTime)
					lowestTime = time;
			}

			int until = int(lowestTime - QDateTime::currentMSecsSinceEpoch());

			requestTimer->start(qMax(until, 0));
		}
		else
		{
			requestTimer->stop();
		}
	}

	void sendGripMessage(const QByteArray &message)
	{
		int reqId = nextReqId++;

		WsControlPacket::Item i;
		i.type = WsControlPacket::Item::Grip;
		i.requestId = QByteArray::number(reqId);
		i.message = message;

		pendingRequests[reqId] = QDateTime::currentMSecsSinceEpoch() + REQUEST_TIMEOUT;
		setupRequestTimer();

		write(i);
	}

	void sendNeedKeepAlive()
	{
		WsControlPacket::Item i;
		i.type = WsControlPacket::Item::NeedKeepAlive;
		write(i);
	}

	void write(const WsControlPacket::Item &item)
	{
		WsControlPacket::Item out = item;
		out.cid = cid;
		manager->write(out);
	}

	void handle(const WsControlPacket::Item &item)
	{
		if(item.type != WsControlPacket::Item::Ack && !item.requestId.isEmpty())
		{
			// ack receipt
			WsControlPacket::Item i;
			i.type = WsControlPacket::Item::Ack;
			i.requestId = item.requestId;
			write(i);
		}

		if(item.type == WsControlPacket::Item::Send)
		{
			WebSocket::Frame::Type type;
			if(item.contentType == "binary")
				type = WebSocket::Frame::Binary;
			else if(item.contentType == "ping")
				type = WebSocket::Frame::Ping;
			else if(item.contentType == "pong")
				type = WebSocket::Frame::Pong;
			else
				type = WebSocket::Frame::Text;

			emit q->sendEventReceived(type, item.message);
		}
		else if(item.type == WsControlPacket::Item::KeepAliveSetup)
		{
			if(item.timeout > 0)
				emit q->keepAliveSetupEventReceived(true, item.timeout);
			else
				emit q->keepAliveSetupEventReceived(false);
		}
		else if(item.type == WsControlPacket::Item::Close)
		{
			emit q->closeEventReceived(item.code);
		}
		else if(item.type == WsControlPacket::Item::Detach)
		{
			emit q->detachEventReceived();
		}
		else if(item.type == WsControlPacket::Item::Cancel)
		{
			emit q->cancelEventReceived();
		}
		else if(item.type == WsControlPacket::Item::Ack)
		{
			int reqId = item.requestId.toInt();
			pendingRequests.remove(reqId);
			setupRequestTimer();
		}
	}

private slots:
	void requestTimer_timeout()
	{
		// on error, destroy any other pending requests
		pendingRequests.clear();
		setupRequestTimer();

		emit q->error();
	}
};

WsControlSession::WsControlSession(QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
}

WsControlSession::~WsControlSession()
{
	delete d;
}

QByteArray WsControlSession::cid() const
{
	return d->cid;
}

void WsControlSession::start(const QByteArray &routeId, const QByteArray &channelPrefix, const QUrl &uri)
{
	d->route = routeId;
	d->channelPrefix = channelPrefix;
	d->uri = uri;
	d->start();
}

void WsControlSession::sendGripMessage(const QByteArray &message)
{
	d->sendGripMessage(message);
}

void WsControlSession::sendNeedKeepAlive()
{
	d->sendNeedKeepAlive();
}

void WsControlSession::setup(WsControlManager *manager, const QByteArray &cid)
{
	d->manager = manager;
	d->cid = cid;
	d->manager->link(this, d->cid);
}

void WsControlSession::handle(const WsControlPacket::Item &item)
{
	assert(d->manager);

	d->handle(item);
}

#include "wscontrolsession.moc"
