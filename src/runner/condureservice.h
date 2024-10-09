/*
 * Copyright (C) 2020-2023 Fanout, Inc.
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

#ifndef CONDURESERVICE_H
#define CONDURESERVICE_H

#include "service.h"
#include "listenport.h"

class CondureService : public Service
{
	Q_OBJECT

public:
	CondureService(
		const QString &name,
		const QString &binFile,
		const QString &runDir,
		const QString &logDir,
		const QString &ipcPrefix,
		const QString &filePrefix,
		int logLevel,
		const QString &certsDir,
		int clientBufferSize,
		int maxconn,
		bool allowCompression,
		const QList<ListenPort> &ports,
		QObject *parent = 0);

	static bool hasClientMode(const QString &binFile);

	// reimplemented

	virtual QStringList arguments() const;

private:
	QStringList args_;
};

#endif
