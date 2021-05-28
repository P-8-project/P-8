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

#ifndef ZHTTPMANAGER_H
#define ZHTTPMANAGER_H

#include <QObject>

class ZhttpRequestPacket;
class ZhttpResponsePacket;
class ZhttpRequest;

class ZhttpManager : public QObject
{
	Q_OBJECT

public:
	ZhttpManager(QObject *parent = 0);
	~ZhttpManager();

	QByteArray instanceId() const;
	void setInstanceId(const QByteArray &id);

	bool setClientOutSpecs(const QStringList &specs);
	bool setClientOutStreamSpecs(const QStringList &specs);
	bool setClientInSpecs(const QStringList &specs);

	bool setServerInSpecs(const QStringList &specs);
	bool setServerInStreamSpecs(const QStringList &specs);
	bool setServerOutSpecs(const QStringList &specs);

	ZhttpRequest *createRequest();
	ZhttpRequest *takeNext();

signals:
	void incomingRequestReady();

private:
	class Private;
	friend class Private;
	Private *d;

	friend class ZhttpRequest;
	void link(ZhttpRequest *req);
	void unlink(ZhttpRequest *req);
	bool canWriteImmediately() const;
	void write(const ZhttpRequestPacket &packet);
	void write(const ZhttpRequestPacket &packet, const QByteArray &instanceAddress);
};

#endif
