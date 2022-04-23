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

#include "responselastids.h"

#include <assert.h>

ResponseLastIds::ResponseLastIds(int maxCapacity) :
	maxCapacity_(maxCapacity)
{
}

void ResponseLastIds::set(const QString &channel, const QString &id)
{
	QDateTime now = QDateTime::currentDateTime();

	if(table_.contains(channel))
	{
		Item &i = table_[channel];
		recentlyUsed_.remove(TimeStringPair(i.time, channel));
		i.id = id;
		i.time = now;
		recentlyUsed_.insert(TimeStringPair(i.time, channel), i);
	}
	else
	{
		while(!table_.isEmpty() && table_.count() >= maxCapacity_)
		{
			// remove oldest
			QMutableMapIterator<TimeStringPair, Item> it(recentlyUsed_);
			assert(it.hasNext());
			it.next();
			QString channel = it.value().channel;
			it.remove();
			table_.remove(channel);
		}

		Item i;
		i.channel = channel;
		i.id = id;
		i.time = now;
		table_.insert(channel, i);
		recentlyUsed_.insert(TimeStringPair(i.time, channel), i);
	}
}

void ResponseLastIds::remove(const QString &channel)
{
	if(table_.contains(channel))
	{
		Item &i = table_[channel];
		recentlyUsed_.remove(TimeStringPair(i.time, channel));
		table_.remove(channel);
	}
}

QString ResponseLastIds::value(const QString &channel)
{
	return table_.value(channel).id;
}
