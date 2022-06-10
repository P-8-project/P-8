/*
 * Copyright (C) 2012-2016 Fanout, Inc.
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

#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include "config.h"

Settings::Settings(const QString &fileName) :
	include_(0),
	portOffset_(0)
{
	main_ = new QSettings(fileName, QSettings::IniFormat);

	libdir_ = valueRaw("global/libdir").toString();
	if(libdir_.isEmpty())
	{
		if(QFile::exists("src/p-8/p-8.pro"))
		{
			// running in tree
			libdir_ = QFileInfo("src/p-8").absoluteFilePath();
		}
		else
		{
			// use compiled value
			libdir_ = LIBDIR;
		}
	}

	rundir_ = valueRaw("global/rundir").toString();
	if(rundir_.isEmpty())
	{
		// fallback to runner section (deprecated)
		rundir_ = valueRaw("runner/rundir").toString();
	}

	ipcPrefix_ = valueRaw("global/ipc_prefix", "p-8-").toString();
	portOffset_ = valueRaw("global/port_offset", 0).toInt();

	QString includeFile = valueRaw("global/include").toString();

	// if include is exactly "internal.conf", rewrite relative to libdir
	// TODO: remove this hack at next major version
	if(includeFile == "internal.conf")
		includeFile = "{libdir}/internal.conf";

	includeFile = resolveVars(includeFile);

	if(!includeFile.isEmpty())
	{
		// if include is a relative path, then use it relative to the config file location
		QFileInfo fi(includeFile);
		if(fi.isRelative())
			includeFile = QFileInfo(QFileInfo(fileName).absoluteDir(), includeFile).filePath();

		include_ = new QSettings(includeFile, QSettings::IniFormat);
	}
}

Settings::~Settings()
{
	delete include_;
	delete main_;
}

QString Settings::resolveVars(const QString &in) const
{
	QString out = in;
	out.replace("{libdir}", libdir_);
	out.replace("{rundir}", rundir_);
	out.replace("{ipc_prefix}", ipcPrefix_);

	// adjust tcp ports
	int at = 0;
	while(true)
	{
		at = out.indexOf("tcp://", at);
		if(at == -1)
			break;

		at = out.indexOf(':', at + 6);
		if(at == -1)
			break;

		int start = at + 1;
		for(at = start; at < out.length(); ++at)
		{
			if(!out[at].isDigit())
				break;
		}

		bool ok;
		int x = out.mid(start, at - start).toInt(&ok);
		if(!ok)
			break;

		x += portOffset_;

		out.replace(start, at, QString::number(x));
	}

	return out;
}

QVariant Settings::valueRaw(const QString &key, const QVariant &defaultValue) const
{
	if(include_)
	{
		if(main_->contains(key))
			return main_->value(key);
		else
			return include_->value(key, defaultValue);
	}
	else
		return main_->value(key, defaultValue);
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const
{
	QVariant v = valueRaw(key, defaultValue);
	if(v.isValid())
	{
		if(v.type() == QVariant::String)
		{
			v = resolveVars(v.toString());
		}
		else if(v.type() == QVariant::StringList)
		{
			QStringList oldList = v.toStringList();
			QStringList newList;
			foreach(QString s, oldList)
				newList += resolveVars(s);
			v = newList;
		}
	}

	return v;
}

int Settings::adjustedPort(const QString &key, int defaultValue) const
{
	int x = value(key, QVariant(defaultValue)).toInt();
	if(x > 0)
		x += portOffset_;
	return x;
}

void Settings::setIpcPrefix(const QString &s)
{
	ipcPrefix_ = s;
}

void Settings::setPortOffset(int x)
{
	portOffset_ = x;
}
