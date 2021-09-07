/*
 * Copyright (C) 2014 Fanout, Inc.
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

#ifndef ZRPCMANAGER_H
#define ZRPCMANAGER_H

#include <QObject>

class ZrpcRequest;
class ZrpcResponsePacket;

class ZrpcManager : public QObject
{
	Q_OBJECT

public:
	ZrpcManager(QObject *parent = 0);
	~ZrpcManager();

	bool setInSpec(const QString &spec);

	ZrpcRequest *takeNext();

signals:
	void requestReady();

private:
	class Private;
	Private *d;

	friend class ZrpcRequest;
	void write(const ZrpcResponsePacket &packet);
};

#endif
