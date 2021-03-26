/*
 * Copyright (C) 2012 Fan Out Networks, Inc.
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

#include "inspectmanager.h"

#include <stdio.h>
#include <assert.h>
#include <QPointer>
#include "qzmqsocket.h"
#include "packet/tnetstring.h"
#include "packet/inspectrequestpacket.h"
#include "packet/inspectresponsepacket.h"
#include "log.h"
#include "inspectrequest.h"

class InspectManager::Private : public QObject
{
	Q_OBJECT

public:
	InspectManager *q;
	QString req_spec;
	QZmq::Socket *req_sock;
	QHash<QByteArray, InspectRequest*> reqsById;

	Private(InspectManager *_q) :
		QObject(_q),
		q(_q),
		req_sock(0)
	{
	}

	bool setup()
	{
		delete req_sock;

		req_sock = new QZmq::Socket(QZmq::Socket::Dealer, this);
		connect(req_sock, SIGNAL(readyRead()), SLOT(req_readyRead()));
		connect(req_sock, SIGNAL(messagesWritten(int)), SLOT(req_messagesWritten(int)));
		if(!req_sock->bind(req_spec))
		{
			delete req_sock;
			req_sock = 0;
			return false;
		}

		return true;
	}

public slots:
	void req_readyRead()
	{
		QPointer<QObject> self = this;

		while(req_sock->canRead())
		{
			QList<QByteArray> msg = req_sock->read();
			if(msg.count() != 2 || !msg[0].isEmpty())
			{
				log_warning("1");
				// TODO: log warning, invalid
				continue;
			}

			QVariant data = TnetString::toVariant(msg[1]);
			if(data.isNull())
			{
				log_warning("2");
				// TODO: log warning, invalid
				continue;
			}

			InspectResponsePacket p;
			if(!p.fromVariant(data))
			{
				log_warning("3");
				// TODO: log warning, invalid
				continue;
			}

			InspectRequest *req = reqsById.value(p.id);
			if(!req)
			{
				log_warning("4");
				// TODO: log warning, unknown request id
				continue;
			}

			req->handle(p);

			if(!self)
				return;
		}
	}

	void req_messagesWritten(int count)
	{
		Q_UNUSED(count);
	}
};

InspectManager::InspectManager(QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
}

InspectManager::~InspectManager()
{
	delete d;
}

bool InspectManager::setSpec(const QString &spec)
{
	d->req_spec = spec;
	return d->setup();
}

InspectRequest *InspectManager::createRequest()
{
	InspectRequest *req = new InspectRequest;
	req->setup(this);
	d->reqsById.insert(req->id(), req);
	return req;
}

void InspectManager::write(const InspectRequestPacket &packet)
{
	assert(d->req_sock);

	QList<QByteArray> msg;
	msg += QByteArray();
	msg += TnetString::fromVariant(packet.toVariant());
	d->req_sock->write(msg);
}

void InspectManager::unlink(InspectRequest *req)
{
	d->reqsById.remove(req->id());
}

#include "inspectmanager.moc"
