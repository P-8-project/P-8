/*
 * Copyright (C) 2015-2017 Fanout, Inc.
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

#ifndef ENGINE_H
#define ENGINE_H

#include <QObject>
#include <QStringList>
#include <QHostAddress>

class Engine : public QObject
{
	Q_OBJECT

public:
	class Configuration
	{
	public:
		QString appVersion;
		QByteArray instanceId;
		QStringList serverInStreamSpecs;
		QStringList serverOutSpecs;
		QStringList clientOutSpecs;
		QStringList clientOutStreamSpecs;
		QStringList clientInSpecs;
		QString inspectSpec;
		QString acceptSpec;
		QString retryOutSpec;
		QString wsControlInSpec;
		QString wsControlOutSpec;
		QString statsSpec;
		QString commandSpec;
		QString stateSpec;
		QString proxyStatsSpec;
		QString proxyCommandSpec;
		QString pushInSpec;
		QString pushInSubSpec;
		QHostAddress pushInHttpAddr;
		int pushInHttpPort;
		int ipcFileMode;
		bool shareAll;
		int messageRate;
		int messageHwm;
		int messageBlockSize;
		int idCacheTtl;
		int connectionSubscriptionMax;
		int subscriptionLinger;
		int statsConnectionTtl;
		int statsSubscriptionTtl;
		int statsReportInterval;

		Configuration() :
			pushInHttpPort(-1),
			ipcFileMode(-1),
			shareAll(false),
			messageRate(-1),
			messageHwm(-1),
			messageBlockSize(-1),
			idCacheTtl(-1),
			connectionSubscriptionMax(-1),
			subscriptionLinger(-1),
			statsConnectionTtl(-1),
			statsSubscriptionTtl(-1),
			statsReportInterval(-1)
		{
		}
	};

	Engine(QObject *parent = 0);
	~Engine();

	bool start(const Configuration &config);
	void reload();

private:
	class Private;
	Private *d;
};

#endif
