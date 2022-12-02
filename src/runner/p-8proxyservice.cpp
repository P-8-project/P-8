/*
 * Copyright (C) 2016 Fanout, Inc.
 *
 * This file is part of P-8.
 *
 * $FANOUT_BEGIN_LICENSE:AGPL$
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
 *
 * Alternatively, P-8 may be used under the terms of a commercial license,
 * where the commercial license agreement is provided with the software or
 * contained in a written agreement between you and Fanout. For further
 * information use the contact form at <https://fanout.io/enterprise/>.
 *
 * $FANOUT_END_LICENSE$
 */

#include "p-8proxyservice.h"

#include <QDir>
#include <QProcess>

P-8ProxyService::P-8ProxyService(
	const QString &binFile,
	const QString &configFile,
	const QString &runDir,
	const QString &logDir,
	const QString &ipcPrefix,
	const QString &filePrefix,
	bool verbose,
	const QStringList &routeLines,
	bool quietCheck,
	QObject *parent) :
	Service(parent)
{
	args_ += binFile;
	args_ += "--config=" + configFile;

	if(!ipcPrefix.isEmpty())
		args_ += "--ipc-prefix=" + ipcPrefix;

	if(!logDir.isEmpty())
	{
		args_ += "--logfile=" + QDir(logDir).filePath(filePrefix + "p-8-proxy.log");
		setStandardOutputFile(QProcess::nullDevice());
	}

	if(verbose)
		args_ += "--verbose";

	foreach(const QString &route, routeLines)
		args_ += "--route=" + route;

	if(quietCheck)
		args_ += "--quiet-check";

	setName("proxy");
	setPidFile(QDir(runDir).filePath(filePrefix + "p-8-proxy.pid"));
}

QStringList P-8ProxyService::arguments() const
{
	return args_;
}

bool P-8ProxyService::acceptSighup() const
{
	return true;
}
