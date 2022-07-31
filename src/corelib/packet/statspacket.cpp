/*
 * Copyright (C) 2014-2016 Fanout, Inc.
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

#include "statspacket.h"

#include <assert.h>

QVariant StatsPacket::toVariant() const
{
	QVariantHash obj;

	if(!from.isEmpty())
		obj["from"] = from;

	if(!route.isEmpty())
		obj["route"] = route;

	if(type == Activity)
	{
		int x = count;
		if(x < 0)
			x = 0;
		obj["count"] = x;
	}
	else if(type == Message)
	{
		obj["channel"] = channel;

		if(!itemId.isNull())
			obj["item-id"] = itemId;

		int x = count;
		if(x < 0)
			x = 0;
		obj["count"] = x;

		if(blocks >= 0)
			obj["blocks"] = blocks;

		obj["transport"] = transport;
	}
	else if(type == Connected || type == Disconnected)
	{
		obj["id"] = connectionId;

		if(type == Connected)
		{
			if(connectionType == WebSocket)
				obj["type"] = QByteArray("ws");
			else // Http
				obj["type"] = QByteArray("http");

			obj["peer-address"] = peerAddress.toString().toUtf8();

			if(ssl)
				obj["ssl"] = true;

			obj["ttl"] = ttl;
		}
		else // Disconnected
		{
			obj["unavailable"] = true;
		}
	}
	else if(type == Subscribed || type == Unsubscribed)
	{
		obj["mode"] = mode;
		obj["channel"] = channel;

		if(type == Subscribed)
		{
			obj["ttl"] = ttl;
		}
		else // Unsubscribed
		{
			obj["unavailable"] = true;
		}
	}
	else // Report
	{
		if(connectionsMax != -1)
			obj["connections"] = connectionsMax;
		if(connectionsMinutes != -1)
			obj["minutes"] = connectionsMinutes;
		if(messagesReceived != -1)
			obj["received"] = messagesReceived;
		if(messagesSent != -1)
			obj["sent"] = messagesSent;
		if(httpResponseMessagesSent != -1)
			obj["http-response-sent"] = httpResponseMessagesSent;
	}

	return obj;
}

bool StatsPacket::fromVariant(const QByteArray &_type, const QVariant &in)
{
	if(in.type() != QVariant::Hash)
		return false;

	QVariantHash obj = in.toHash();

	if(obj.contains("from"))
	{
		if(obj["from"].type() != QVariant::ByteArray)
			return false;

		from = obj["from"].toByteArray();
	}

	if(obj.contains("route"))
	{
		if(obj["route"].type() != QVariant::ByteArray)
			return false;

		route = obj["route"].toByteArray();
	}

	if(_type == "activity")
	{
		type = Activity;

		if(!obj.contains("count") || !obj["count"].canConvert(QVariant::Int))
			return false;

		count = obj["count"].toInt();
		if(count < 0)
			return false;
	}
	else if(_type == "message")
	{
		type = Message;

		if(!obj.contains("channel") || obj["channel"].type() != QVariant::ByteArray)
			return false;

		channel = obj["channel"].toByteArray();

		if(obj.contains("item-id"))
		{
			if(obj["item-id"].type() != QVariant::ByteArray)
				return false;

			itemId = obj["item-id"].toByteArray();
		}

		if(!obj.contains("count") || !obj["count"].canConvert(QVariant::Int))
			return false;

		count = obj["count"].toInt();
		if(count < 0)
			return false;

		if(obj.contains("blocks"))
		{
			if(!obj["blocks"].canConvert(QVariant::Int))
				return false;

			blocks = obj["blocks"].toInt();
		}

		if(!obj.contains("transport") || obj["transport"].type() != QVariant::ByteArray)
			return false;

		transport = obj["transport"].toByteArray();
	}
	else if(_type == "conn")
	{
		if(!obj.contains("id") || obj["id"].type() != QVariant::ByteArray)
			return false;

		connectionId = obj["id"].toByteArray();

		type = Connected;
		if(obj.contains("unavailable"))
		{
			if(obj["unavailable"].type() != QVariant::Bool)
				return false;

			if(obj["unavailable"].toBool())
				type = Disconnected;
		}

		if(type == Connected)
		{
			if(!obj.contains("type") || obj["type"].type() != QVariant::ByteArray)
				return false;

			QByteArray typeStr = obj["type"].toByteArray();
			if(typeStr == "ws")
				connectionType = WebSocket;
			else if(typeStr == "http")
				connectionType = Http;
			else
				return false;

			if(obj.contains("peer-address"))
			{
				if(obj["peer-address"].type() != QVariant::ByteArray)
					return false;

				QByteArray peerAddressStr = obj["peer-address"].toByteArray();
				if(!peerAddress.setAddress(QString::fromUtf8(peerAddressStr)))
					return false;
			}

			if(obj.contains("ssl"))
			{
				if(obj["ssl"].type() != QVariant::Bool)
					return false;

				ssl = obj["ssl"].toBool();
			}

			if(!obj.contains("ttl") || !obj["ttl"].canConvert(QVariant::Int))
				return false;

			ttl = obj["ttl"].toInt();
			if(ttl < 0)
				return false;
		}
	}
	else if(_type == "sub")
	{
		if(!obj.contains("mode") || obj["mode"].type() != QVariant::ByteArray)
			return false;

		mode = obj["mode"].toByteArray();

		if(!obj.contains("channel") || obj["channel"].type() != QVariant::ByteArray)
			return false;

		channel = obj["channel"].toByteArray();

		type = Subscribed;
		if(obj.contains("unavailable"))
		{
			if(obj["unavailable"].type() != QVariant::Bool)
				return false;

			if(obj["unavailable"].toBool())
				type = Unsubscribed;
		}

		if(type == Subscribed)
		{
			if(!obj.contains("ttl") || !obj["ttl"].canConvert(QVariant::Int))
				return false;

			ttl = obj["ttl"].toInt();
			if(ttl < 0)
				return false;
		}
	}
	else if(_type == "report")
	{
		if(obj.contains("connections"))
		{
			if(!obj["connections"].canConvert(QVariant::Int))
				return false;

			connectionsMax = obj["connections"].toInt();
		}

		if(obj.contains("minutes"))
		{
			if(!obj["minutes"].canConvert(QVariant::Int))
				return false;

			connectionsMinutes = obj["minutes"].toInt();
		}

		if(obj.contains("received"))
		{
			if(!obj["received"].canConvert(QVariant::Int))
				return false;

			messagesReceived = obj["received"].toInt();
		}

		if(obj.contains("sent"))
		{
			if(!obj["sent"].canConvert(QVariant::Int))
				return false;

			messagesSent = obj["sent"].toInt();
		}

		if(obj.contains("http-response-sent"))
		{
			if(!obj["http-response-sent"].canConvert(QVariant::Int))
				return false;

			httpResponseMessagesSent = obj["http-response-sent"].toInt();
		}
	}
	else
		return false;

	return true;
}
