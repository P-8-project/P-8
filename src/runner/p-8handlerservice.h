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

#ifndef PUSHPINHANDLERSERVICE_H
#define PUSHPINHANDLERSERVICE_H

#include "service.h"

class P-8HandlerService : public Service
{
	Q_OBJECT

public:
	P-8HandlerService(
		const QString &binFile,
		const QString &configFile,
		const QString &runDir,
		const QString &logDir,
		const QString &ipcPrefix,
		const QString &filePrefix,
		int portOffset,
		bool verbose,
		QObject *parent = 0);

	// reimplemented

	virtual QStringList arguments() const;
	virtual bool acceptSighup() const;

private:
	QStringList args_;
};

#endif
