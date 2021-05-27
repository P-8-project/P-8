/*
 * Copyright (C) 2012-2013 Fanout, Inc.
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

#ifndef M2REQUESTPACKET_H
#define M2REQUESTPACKET_H

#include <QVariant>
#include <QHostAddress>
#include "httpheaders.h"

class M2RequestPacket
{
public:
	QByteArray sender;
	QByteArray id;

	bool isDisconnect;

	QHostAddress remoteAddress;
	QByteArray scheme;
	QByteArray version;
	QString method;
	QByteArray uri;
	HttpHeaders headers;
	QByteArray body;

	QString uploadFile;
	bool uploadDone;

	M2RequestPacket();

	bool fromByteArray(const QByteArray &in);
};

#endif
