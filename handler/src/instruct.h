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

#ifndef INSTRUCT_H
#define INSTRUCT_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QList>
#include <QHash>
#include "packet/httpresponsedata.h"

class Instruct
{
public:
	enum HoldMode
	{
		NoHold,
		ResponseHold,
		StreamHold
	};

	class Channel
	{
	public:
		QString name;
		QString prevId;
		QStringList filters;
	};

	HoldMode holdMode;
	QList<Channel> channels;
	int timeout;
	QList<QByteArray> exposeHeaders;
	QByteArray keepAliveData;
	int keepAliveTimeout;
	QHash<QString, QString> meta;
	HttpResponseData response;

	Instruct() :
		holdMode(NoHold),
		timeout(-1),
		keepAliveTimeout(-1)
	{
	}

	static Instruct fromResponse(const HttpResponseData &response, bool *ok = 0, QString *errorMessage = 0);
};

#endif
