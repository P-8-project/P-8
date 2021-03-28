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

#ifndef M2RESPONSE_H
#define M2RESPONSE_H

#include <QObject>
#include "packet/httpheaders.h"
#include "m2request.h"

class M2Manager;

class M2Response : public QObject
{
	Q_OBJECT

public:
	~M2Response();

	void start(int code, const QByteArray &status, const HttpHeaders &headers);
	void write(const QByteArray &body);
	void close();

signals:
	void bytesWritten(int count);
	void finished();

private:
	class Private;
	friend class Private;
	Private *d;

	friend class M2Manager;
	M2Response(QObject *parent = 0);
	void handle(M2Manager *manager, const M2Request::Rid &rid);
};

#endif
