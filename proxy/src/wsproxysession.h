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

#ifndef WSPROXYSESSION_H
#define WSPROXYSESSION_H

#include <QObject>
#include "zwebsocket.h"

class ZRoutes;
class WsControlManager;
class StatsManager;
class DomainMap;
class XffRule;

class WsProxySession : public QObject
{
	Q_OBJECT

public:
	WsProxySession(ZRoutes *zroutes, DomainMap *domainMap, StatsManager *stats = 0, WsControlManager *wsControlManager = 0, QObject *parent = 0);
	~WsProxySession();

	QByteArray routeId() const;
	ZWebSocket::Rid rid() const;

	void setDefaultSigKey(const QByteArray &iss, const QByteArray &key);
	void setDefaultUpstreamKey(const QByteArray &key);
	void setUseXForwardedProtocol(bool enabled);
	void setXffRules(const XffRule &untrusted, const XffRule &trusted);
	void setOrigHeadersNeedMark(const QList<QByteArray> &names);

	// takes ownership
	void start(ZWebSocket *sock);

signals:
	void finishedByPassthrough();

private:
	class Private;
	friend class Private;
	Private *d;
};

#endif
