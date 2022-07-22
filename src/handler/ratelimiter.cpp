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

#include "ratelimiter.h"

#include <QList>
#include <QMap>
#include <QPointer>
#include <QTimer>

#define MIN_BATCH_INTERVAL 25

class RateLimiter::Private : public QObject
{
	Q_OBJECT

public:
	int rate;
	int hwm;
	bool batchWaitEnabled;
	QMap<QString, QList<Action*> > buckets;
	QString lastKey;
	QTimer *timer;
	bool firstPass;
	int batchInterval;
	int batchSize;
	bool lastBatchEmpty;

	Private(QObject *_q) :
		QObject(_q),
		rate(-1),
		hwm(-1),
		batchWaitEnabled(false),
		batchInterval(-1),
		batchSize(-1),
		lastBatchEmpty(false)
	{
		timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, &Private::timeout);
	}

	~Private()
	{
		timer->disconnect(this);
		timer->setParent(0);
		timer->deleteLater();

		QMapIterator<QString, QList<Action*> > it(buckets);
		while(it.hasNext())
		{
			it.next();
			QList<Action*> bucket = it.value();

			qDeleteAll(bucket);
		}
	}

	void setRate(int actionsPerSecond)
	{
		if(actionsPerSecond > 0)
		{
			rate = actionsPerSecond;

			if(rate >= 1000 / MIN_BATCH_INTERVAL)
			{
				batchInterval = MIN_BATCH_INTERVAL;
				batchSize = (rate * batchInterval + 999) / 1000;
			}
			else
			{
				batchInterval = 1000 / rate;
				batchSize = 1;
			}
		}
		else
		{
			rate = -1;
			batchInterval = -1;
			batchSize = -1;
		}

		setup();
	}

	bool addAction(const QString &key, Action *action)
	{
		QList<Action*> &bucket = buckets[key];
		if(hwm > 0 && bucket.count() >= hwm)
			return false;

		bucket += action;

		setup();
		return true;
	}

private:
	void setup()
	{
		if(rate > 0)
		{
			if(!buckets.isEmpty() || !lastBatchEmpty)
			{
				if(timer->isActive())
				{
					// after the first pass, switch to batch interval
					if(!firstPass)
						timer->setInterval(batchInterval);
				}
				else
				{
					// process first batch
					firstPass = true;

					if(batchWaitEnabled)
					{
						// if wait enabled, collect for awhile before processing
						timer->start(batchInterval);
					}
					else
					{
						// if wait not enabled, process immediately
						timer->start(0);
					}
				}
			}
			else
			{
				if(lastBatchEmpty)
				{
					// if we processed nothing on this pass, stop timer
					lastBatchEmpty = false;
					timer->stop();
				}
			}
		}
		else
		{
			if(!buckets.isEmpty())
			{
				if(timer->isActive())
				{
					// ensure we're on fastest interval
					timer->setInterval(0);
				}
				else
				{
					// process first batch right away
					firstPass = true;
					timer->start(0);
				}
			}
			else
			{
				timer->stop();
			}
		}
	}

	// return false if self destroyed
	bool processBatch()
	{
		if(buckets.isEmpty())
		{
			lastBatchEmpty = true;
			return true;
		}

		lastBatchEmpty = false;

		QMap<QString, QList<Action*> >::iterator it;

		if(!lastKey.isNull())
		{
			it = buckets.find(lastKey);

			if(it == buckets.end())
				it = buckets.begin();
		}
		else
		{
			it = buckets.begin();
		}

		QPointer<QObject> self = this;

		int processed = 0;
		while((batchSize < 1 || processed < batchSize) && it != buckets.end())
		{
			QList<Action*> &bucket = it.value();

			QString key = it.key();
			Action *action = bucket.takeFirst();

			if(bucket.isEmpty())
			{
				lastKey = key;
				it = buckets.erase(it);
			}
			else
			{
				++it;
				if(it == buckets.end())
					it = buckets.begin();
			}

			bool ret = action->execute();
			delete action;

			if(!self)
				return false;

			if(ret)
				++processed;
		}

		if(it != buckets.end())
			lastKey = it.key();

		return true;
	}

private slots:
	void timeout()
	{
		if(!processBatch())
			return;

		firstPass = false;

		setup();
	}
};

RateLimiter::RateLimiter(QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
}

RateLimiter::~RateLimiter()
{
	delete d;
}

void RateLimiter::setRate(int actionsPerSecond)
{
	d->setRate(actionsPerSecond);
}

void RateLimiter::setHwm(int hwm)
{
	d->hwm = hwm;
}

void RateLimiter::setBatchWaitEnabled(bool on)
{
	d->batchWaitEnabled = on;
}

bool RateLimiter::addAction(const QString &key, Action *action)
{
	return d->addAction(key, action);
}

RateLimiter::Action *RateLimiter::lastAction(const QString &key) const
{
	if(d->buckets.contains(key))
	{
		const QList<Action*> &bucket = d->buckets[key];
		if(!bucket.isEmpty())
			return bucket.last();
	}

	return 0;
}

#include "ratelimiter.moc"
