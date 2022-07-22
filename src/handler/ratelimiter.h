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

#ifndef RATELIMITER_H
#define RATELIMITER_H

#include <QObject>

class RateLimiter : public QObject
{
	Q_OBJECT

public:
	class Action
	{
	public:
		virtual ~Action() {}

		virtual bool execute() = 0;
	};

	RateLimiter(QObject *parent = 0);
	~RateLimiter();

	void setRate(int actionsPerSecond);
	void setHwm(int hwm);
	void setBatchWaitEnabled(bool on);

	bool addAction(const QString &key, Action *action);
	Action *lastAction(const QString &key) const;

private:
	class Private;
	Private *d;
};

#endif
