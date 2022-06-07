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

#include "p-8handlerservice.h"

#include <QDir>

P-8HandlerService::P-8HandlerService(
	const QString &binFile,
	const QString &configFile,
	const QString &logDir,
	bool verbose,
	QObject *parent) :
	Service(parent)
{
	args_ += binFile;
	args_ += "--config=" + configFile;

	if(!logDir.isEmpty())
		args_ += "--logfile=" + QDir(logDir).filePath("p-8-handler.log");

	if(verbose)
		args_ += "--verbose";
}

QString P-8HandlerService::name() const
{
	return "handler";
}

QStringList P-8HandlerService::arguments() const
{
	return args_;
}

bool P-8HandlerService::acceptSighup() const
{
	return true;
}
