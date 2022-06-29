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

#include <QtTest/QtTest>
#include "log.h"
#include "routesfile.h"

class RoutesFileTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		//log_setOutputLevel(LOG_LEVEL_WARNING);
	}

	void cleanupTestCase()
	{
	}

	void lineTests()
	{
		QList<RoutesFile::RouteSection> r;
		bool ok;

		r = RoutesFile::parseLine("apple", &ok);
		QVERIFY(ok);
		QCOMPARE(r.count(), 1);
		QCOMPARE(r[0].value, QString("apple"));
		QVERIFY(r[0].props.isEmpty());

		r = RoutesFile::parseLine("apple banana", &ok);
		QVERIFY(ok);
		QCOMPARE(r.count(), 2);
		QCOMPARE(r[0].value, QString("apple"));
		QVERIFY(r[0].props.isEmpty());
		QCOMPARE(r[1].value, QString("banana"));
		QVERIFY(r[1].props.isEmpty());

		r = RoutesFile::parseLine("  apple   banana  # comment", &ok);
		QVERIFY(ok);
		QCOMPARE(r.count(), 2);
		QCOMPARE(r[0].value, QString("apple"));
		QVERIFY(r[0].props.isEmpty());
		QCOMPARE(r[1].value, QString("banana"));
		QVERIFY(r[1].props.isEmpty());

		r = RoutesFile::parseLine("apple,organic,type=gala,from=\"washington, \\\"usa\\\"\"", &ok);
		QVERIFY(ok);
		QCOMPARE(r.count(), 1);
		QCOMPARE(r[0].value, QString("apple"));
		QCOMPARE(r[0].props.count(), 3);
		QVERIFY(r[0].props.contains("organic"));
		QVERIFY(r[0].props.value("organic").isEmpty());
		QCOMPARE(r[0].props.value("type"), QString("gala"));
		QCOMPARE(r[0].props.value("from"), QString("washington, \"usa\""));

		r = RoutesFile::parseLine("apple,organic banana cherry,type=bing", &ok);
		QVERIFY(ok);
		QCOMPARE(r.count(), 3);
		QCOMPARE(r[0].value, QString("apple"));
		QCOMPARE(r[0].props.count(), 1);
		QVERIFY(r[0].props.contains("organic"));
		QVERIFY(r[0].props.value("organic").isEmpty());
		QCOMPARE(r[1].value, QString("banana"));
		QVERIFY(r[1].props.isEmpty());
		QCOMPARE(r[2].value, QString("cherry"));
		QCOMPARE(r[2].props.value("type"), QString("bing"));

		// unterminated quote
		r = RoutesFile::parseLine("apple,organic,type=\"gala", &ok);
		QVERIFY(!ok);

		// empty value
		r = RoutesFile::parseLine(",organic", &ok);
		QVERIFY(!ok);

		// empty prop name
		r = RoutesFile::parseLine("apple,organic,", &ok);
		QVERIFY(!ok);

		// empty prop name
		r = RoutesFile::parseLine("apple,organic,=gala", &ok);
		QVERIFY(!ok);
	}
};

QTEST_MAIN(RoutesFileTest)
#include "routesfiletest.moc"
