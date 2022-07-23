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

#include "httpsessionupdatemanager.h"

#include <QTimer>
#include <QUrl>
#include "httpsession.h"

class HttpSessionUpdateManager::Private : public QObject
{
	Q_OBJECT

public:
	class Bucket
	{
	public:
		QPair<int, QUrl> key;
		QSet<HttpSession*> sessions;
		QTimer *timer;
	};

	HttpSessionUpdateManager *q;
	QHash<QPair<int, QUrl>, Bucket*> buckets;
	QHash<QTimer*, Bucket*> bucketsByTimer;
	QHash<HttpSession*, Bucket*> bucketsBySession;

	Private(HttpSessionUpdateManager *_q) :
		QObject(_q),
		q(_q)
	{
	}

	~Private()
	{
		QHashIterator<QPair<int, QUrl>, Bucket*> it(buckets);
		while(it.hasNext())
		{
			it.next();
			Bucket *bucket = it.value();

			bucket->timer->disconnect(this);
			bucket->timer->setParent(0);
			bucket->timer->deleteLater();
			delete bucket;
		}
	}

	void removeBucket(Bucket *bucket)
	{
		foreach(HttpSession *hs, bucket->sessions)
			bucketsBySession.remove(hs);

		bucketsByTimer.remove(bucket->timer);
		buckets.remove(bucket->key);

		bucket->timer->disconnect(this);
		bucket->timer->setParent(0);
		bucket->timer->deleteLater();
		delete bucket;
	}

	void registerSession(HttpSession *hs, int timeout, const QUrl &uri)
	{
		if(bucketsBySession.contains(hs))
			return;

		QUrl tmp = uri;
		tmp.setQuery(QString()); // remove the query part
		QPair<int, QUrl> key(timeout, tmp);

		Bucket *bucket = buckets.value(key);
		if(bucket)
		{
			bucket->sessions += hs;
			bucketsBySession[hs] = bucket;
			return;
		}

		bucket = new Bucket;
		bucket->key = key;
		bucket->sessions += hs;
		bucket->timer = new QTimer(this);
		connect(bucket->timer, &QTimer::timeout, this, &Private::timer_timeout);

		buckets[key] = bucket;
		bucketsByTimer[bucket->timer] = bucket;
		bucketsBySession[hs] = bucket;

		bucket->timer->start(timeout * 1000);
	}

	void unregisterSession(HttpSession *hs)
	{
		Bucket *bucket = bucketsBySession.value(hs);
		if(!bucket)
			return;

		bucket->sessions.remove(hs);
		bucketsBySession.remove(hs);

		if(bucket->sessions.isEmpty())
			removeBucket(bucket);
	}

private slots:
	void timer_timeout()
	{
		QTimer *timer = (QTimer *)sender();
		Bucket *bucket = bucketsByTimer.value(timer);
		if(!bucket)
			return;

		QSet<HttpSession*> sessions = bucket->sessions;
		removeBucket(bucket);

		foreach(HttpSession *hs, sessions)
			hs->update();
	}
};

HttpSessionUpdateManager::HttpSessionUpdateManager(QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
}

HttpSessionUpdateManager::~HttpSessionUpdateManager()
{
	delete d;
}

void HttpSessionUpdateManager::registerSession(HttpSession *hs, int timeout, const QUrl &uri)
{
	d->registerSession(hs, timeout, uri);
}

void HttpSessionUpdateManager::unregisterSession(HttpSession *hs)
{
	d->unregisterSession(hs);
}

#include "httpsessionupdatemanager.moc"
