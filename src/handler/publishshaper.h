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

#ifndef PUBLISHSHAPER_H
#define PUBLISHSHAPER_H

#include <QObject>

class PublishFormat;

class PublishShaper : public QObject
{
	Q_OBJECT

public:
	PublishShaper(QObject *parent = 0);
	~PublishShaper();

	void setRate(int messagesPerSecond);
	void setHwm(int hwm);

	bool addMessage(QObject *target, const PublishFormat &format, const QString &route = QString(), const QList<QByteArray> &exposeHeaders = QList<QByteArray>());

signals:
	void send(QObject *target, const PublishFormat &format, const QList<QByteArray> &exposeHeaders);

private:
	class Private;
	friend class Private;
	Private *d;
};

#endif
