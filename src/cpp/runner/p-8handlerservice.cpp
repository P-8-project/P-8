/*
 * Copyright (C) 2016 Fanout, Inc.
 *
 * This file is part of P-8.
 *
 * $FANOUT_BEGIN_LICENSE:APACHE2$
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * $FANOUT_END_LICENSE$
 */

#include "p-8handlerservice.h"

#include <QDir>
#include <QProcess>

P-8HandlerService::P-8HandlerService(
	const QString &binFile,
	const QString &configFile,
	const QString &runDir,
	const QString &logDir,
	const QString &ipcPrefix,
	const QString &filePrefix,
	int portOffset,
	int logLevel)
{
	args_ += binFile;
	args_ += "--config=" + configFile;

	if(!ipcPrefix.isEmpty())
		args_ += "--ipc-prefix=" + ipcPrefix;

	if(portOffset > 0)
		args_ += "--port-offset=" + QString::number(portOffset);

	if(!logDir.isEmpty())
	{
		args_ += "--logfile=" + QDir(logDir).filePath(filePrefix + "p-8-handler.log");
		setStandardOutputFile(QProcess::nullDevice());
	}

	if(logLevel >= 0)
		args_ += "--loglevel=" + QString::number(logLevel);

	setName("handler");
	setPidFile(QDir(runDir).filePath(filePrefix + "p-8-handler.pid"));
}

QStringList P-8HandlerService::arguments() const
{
	return args_;
}

bool P-8HandlerService::acceptSighup() const
{
	return true;
}
