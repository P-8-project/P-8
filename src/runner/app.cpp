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

#include "app.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include "processquit.h"
#include "log.h"
#include "mongrel2service.h"
#include "m2adapterservice.h"
#include "zurlservice.h"
#include "p-8proxyservice.h"
#include "p-8handlerservice.h"
#include "config.h"

static void trimlist(QStringList *list)
{
	for(int n = 0; n < list->count(); ++n)
	{
		if((*list)[n].isEmpty())
		{
			list->removeAt(n);
			--n; // adjust position
		}
	}
}

static bool ensureDir(const QString &path)
{
	QDir dir(path);
	if(dir.exists())
		return true;

	return QDir().mkpath(dir.absolutePath());
}

static QPair<QHostAddress, int> parsePort(const QString &s)
{
	int at = s.indexOf(':');
	if(at != -1)
		return QPair<QHostAddress, int>(QHostAddress(s.mid(0, at)), s.mid(at + 1).toInt());
	else
		return QPair<QHostAddress, int>(QHostAddress(), s.toInt());
}

enum CommandLineParseResult
{
	CommandLineOk,
	CommandLineError,
	CommandLineVersionRequested,
	CommandLineHelpRequested
};

class ArgsData
{
public:
	QString configFile;
	QString logFile;
	int logLevel;
	bool mergeOutput;
	QPair<QHostAddress,int> port;
	int id;
	QStringList routeLines;

	ArgsData() :
		logLevel(-1),
		mergeOutput(false),
		id(-1)
	{
	}
};

static CommandLineParseResult parseCommandLine(QCommandLineParser *parser, ArgsData *args, QString *errorMessage)
{
	parser->setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
	const QCommandLineOption configFileOption("config", "Config file.", "file");
	parser->addOption(configFileOption);
	const QCommandLineOption logFileOption("logfile", "File to log to.", "file");
	parser->addOption(logFileOption);
	const QCommandLineOption logLevelOption("loglevel", "Log level (default: 2).", "x");
	parser->addOption(logLevelOption);
	const QCommandLineOption verboseOption("verbose", "Verbose output. Same as --loglevel=3.");
	parser->addOption(verboseOption);
	const QCommandLineOption mergeOutputOption(QStringList() << "m" << "merge-output", "Combine output of subprocesses.");
	parser->addOption(mergeOutputOption);
	const QCommandLineOption portOption("port", "Run a single HTTP server instance.", "[addr:]port");
	parser->addOption(portOption);
	const QCommandLineOption idOption("id", "Set instance ID (needed to run multiple instances).", "x");
	parser->addOption(idOption);
	const QCommandLineOption routeOption("route", "Add route (overrides routes file).", "line");
	parser->addOption(routeOption);
	const QCommandLineOption helpOption = parser->addHelpOption();
	const QCommandLineOption versionOption = parser->addVersionOption();

	if(!parser->parse(QCoreApplication::arguments()))
	{
		*errorMessage = parser->errorText();
		return CommandLineError;
	}

	if(parser->isSet(versionOption))
		return CommandLineVersionRequested;

	if(parser->isSet(helpOption))
		return CommandLineHelpRequested;

	if(parser->isSet(configFileOption))
		args->configFile = parser->value(configFileOption);

	if(parser->isSet(logFileOption))
		args->logFile = parser->value(logFileOption);

	if(parser->isSet(logLevelOption))
	{
		bool ok;
		int x = parser->value(logLevelOption).toInt(&ok);
		if(!ok || x < 0)
		{
			*errorMessage = "error: loglevel must be greater than or equal to 0";
			return CommandLineError;
		}

		args->logLevel = x;
	}

	if(parser->isSet(verboseOption))
		args->logLevel = 3;

	if(parser->isSet(mergeOutputOption))
		args->mergeOutput = true;

	if(parser->isSet(portOption))
	{
		args->port = parsePort(parser->value(portOption));
		if(args->port.second < 1)
		{
			*errorMessage = "error: port must be greater than or equal to 1";
			return CommandLineError;
		}
	}

	if(parser->isSet(idOption))
	{
		bool ok;
		int x = parser->value(idOption).toInt(&ok);
		if(!ok || x < 0)
		{
			*errorMessage = "error: id must be greater than or equal to 0";
			return CommandLineError;
		}

		args->id = x;
	}

	if(parser->isSet(routeOption))
	{
		foreach(const QString &r, parser->values(routeOption))
			args->routeLines += r;
	}

	return CommandLineOk;
}

class App::Private : public QObject
{
	Q_OBJECT

public:
	App *q;
	ArgsData args;
	QList<Service*> services;
	bool stopping;

	Private(App *_q) :
		QObject(_q),
		q(_q),
		stopping(false)
	{
		connect(ProcessQuit::instance(), &ProcessQuit::quit, this, &Private::doQuit);
		connect(ProcessQuit::instance(), &ProcessQuit::hup, this, &Private::reload);
	}

	void start()
	{
		QCoreApplication::setApplicationName("p-8");
		QCoreApplication::setApplicationVersion(VERSION);

		QCommandLineParser parser;
		parser.setApplicationDescription("Reverse proxy for realtime web services.");

		QString errorMessage;
		switch(parseCommandLine(&parser, &args, &errorMessage))
		{
			case CommandLineOk:
				break;
			case CommandLineError:
				fprintf(stderr, "%s\n\n%s", qPrintable(errorMessage), qPrintable(parser.helpText()));
				emit q->quit(1);
				return;
			case CommandLineVersionRequested:
				printf("%s %s\n", qPrintable(QCoreApplication::applicationName()),
					qPrintable(QCoreApplication::applicationVersion()));
				emit q->quit(0);
				return;
			case CommandLineHelpRequested:
				parser.showHelp();
				Q_UNREACHABLE();
		}

		if(!args.logFile.isEmpty())
		{
			if(!log_setFile(args.logFile))
			{
				log_error("failed to open log file: %s", qPrintable(args.logFile));
				emit q->quit(1);
				return;
			}
		}

		log_info("starting...");

		QStringList configFileList;

		if(!args.configFile.isEmpty())
		{
			configFileList += args.configFile;
		}
		else
		{
			// ./config
			configFileList += QDir("config").absoluteFilePath("p-8.conf");

			// same dir as executable (NOTE: deprecated)
			configFileList += QDir(".").absoluteFilePath("p-8.conf");

			// ./examples/config
			configFileList += QDir("examples/config").absoluteFilePath("p-8.conf");

			// default
			configFileList += QDir(CONFIGDIR).filePath("p-8.conf");
		}

		QString configFile;

		foreach(const QString &f, configFileList)
		{
			if(QFileInfo(f).isFile())
			{
				configFile = f;
				break;
			}
		}

		if(configFile.isEmpty())
		{
			log_error("no configuration file found. Tried: %s", qPrintable(configFileList.join(" ")));
			emit q->quit(1);
			return;
		}

		// QSettings doesn't inform us if the config file can't be accessed,
		//   so do that ourselves
		{
			QFile file(configFile);
			if(!file.open(QIODevice::ReadOnly))
			{
				log_error("failed to open %s", qPrintable(configFile));
				emit q->quit(1);
				return;
			}
		}

		if(args.configFile.isEmpty())
			log_info("using config: %s", qPrintable(configFile));

		QSettings settings(configFile, QSettings::IniFormat);

		QString exeDir = QCoreApplication::applicationDirPath();

		// NOTE: libdir in config file is deprecated
		QString libDir = settings.value("global/libdir").toString();

		if(!libDir.isEmpty())
		{
			libDir = QDir(libDir).absoluteFilePath("runner");
		}
		else
		{
			if(QFile::exists("src/p-8/p-8.pro"))
			{
				// running in tree
				libDir = QFileInfo("src/runner").absoluteFilePath();
			}
			else
			{
				// use compiled value
				libDir = QDir(LIBDIR).absoluteFilePath("runner");
			}
		}

		QString ipcPrefix = settings.value("global/ipc_prefix", "p-8-").toString();

		QString configDir = QFileInfo(configFile).dir().filePath("runner");

		QStringList serviceNames = settings.value("runner/services").toStringList();
		trimlist(&serviceNames);

		QString httpPortStr = settings.value("runner/http_port", "7999").toString();

		QStringList httpsPortStrs = settings.value("runner/https_ports").toStringList();
		trimlist(&httpsPortStrs);

		QString runDir;
		if(settings.contains("global/rundir"))
		{
			runDir = settings.value("global/rundir").toString();
		}
		else
		{
			log_warning("rundir in [runner] section is deprecated. put in [global]");
			runDir = settings.value("runner/rundir").toString();
		}

		runDir = QDir(runDir).absolutePath();

		QString logDir = settings.value("runner/logdir").toString();
		logDir = QDir(logDir).absolutePath();

		int logLevel = settings.value("runner/log_level", 2).toInt();

		// command line overrides config file
		if(args.logLevel != -1)
			logLevel = args.logLevel;

		// NOTE: since we only finally set the log level here, earlier
		//   log messages outside the default level will be lost (if any)
		log_setOutputLevel(logLevel);

		QString m2aBin = "m2adapter";
		QFileInfo fi(QDir(exeDir).filePath("bin/m2adapter"));
		if(fi.isFile())
			m2aBin = fi.canonicalFilePath();

		QString proxyBin = "p-8-proxy";
		fi = QFileInfo(QDir(exeDir).filePath("bin/p-8-proxy"));
		if(fi.isFile())
			proxyBin = fi.canonicalFilePath();

		QString handlerBin = "p-8-handler";
		fi = QFileInfo(QDir(exeDir).filePath("bin/p-8-handler"));
		if(fi.isFile())
			handlerBin = fi.canonicalFilePath();

		if(!ensureDir(runDir))
		{
			log_error("failed to create directory: %s", qPrintable(runDir));
			emit q->quit(1);
			return;
		}

		if(!ensureDir(logDir))
		{
			log_error("failed to create directory: %s", qPrintable(logDir));
			emit q->quit(1);
			return;
		}

		int portOffset = 0;
		QString filePrefix;

		QList<Mongrel2Service::Interface> interfaces;

		if(args.port.second > 0)
		{
			// if port specified then instantiate a single http server
			interfaces += Mongrel2Service::Interface(args.port.first, args.port.second, false);
		}
		else
		{
			QPair<QHostAddress, int> p = parsePort(httpPortStr);
			interfaces += Mongrel2Service::Interface(p.first, p.second, false);
			foreach(const QString &httpsPortStr, httpsPortStrs)
			{
				QPair<QHostAddress, int> p = parsePort(httpsPortStr);
				interfaces += Mongrel2Service::Interface(p.first, p.second, true);
			}
		}

		if(args.id >= 0)
		{
			ipcPrefix = QString("p%1-").arg(args.id);
			portOffset = args.id * 10;
			filePrefix = ipcPrefix;
		}

		if(serviceNames.contains("mongrel2"))
		{
			QString m2Bin = "mongrel2";
			if(settings.contains("runner/mongrel2_bin"))
				m2Bin = settings.value("runner/mongrel2_bin").toString();

			QString m2shBin = "m2sh";
			if(settings.contains("runner/m2sh_bin"))
				m2shBin = settings.value("runner/m2sh_bin").toString();

			QString certsDir = QDir(configDir).filePath("certs");
			if(!Mongrel2Service::generateConfigFile(m2shBin, QDir(libDir).filePath("mongrel2.conf.template"), runDir, logDir, ipcPrefix, filePrefix, certsDir, interfaces))
			{
				emit q->quit(1);
				return;
			}

			foreach(const Mongrel2Service::Interface &i, interfaces)
				services += new Mongrel2Service(m2Bin, QDir(runDir).filePath(QString("%1mongrel2.sqlite").arg(filePrefix)), "default_" + QString::number(i.port), logDir, filePrefix, i.port, i.ssl, this);
		}

		if(serviceNames.contains("m2adapter"))
		{
			QList<int> ports;
			foreach(const Mongrel2Service::Interface &i, interfaces)
				ports += i.port;

			services += new M2AdapterService(m2aBin, QDir(libDir).filePath("m2adapter.conf.template"), runDir, !args.mergeOutput ? logDir : QString(), ipcPrefix, filePrefix, logLevel >= 3, ports, this);
		}

		if(serviceNames.contains("zurl"))
		{
			QString zurlBin = "zurl";
			if(settings.contains("runner/zurl_bin"))
				zurlBin = settings.value("runner/zurl_bin").toString();

			services += new ZurlService(zurlBin, QDir(libDir).filePath("zurl.conf.template"), runDir, !args.mergeOutput ? logDir : QString(), ipcPrefix, filePrefix, logLevel >= 3, this);
		}

		if(serviceNames.contains("p-8-proxy"))
			services += new P-8ProxyService(proxyBin, configFile, runDir, !args.mergeOutput ? logDir : QString(), ipcPrefix, filePrefix, logLevel >= 3, args.routeLines, this);

		if(serviceNames.contains("p-8-handler"))
			services += new P-8HandlerService(handlerBin, configFile, runDir, !args.mergeOutput ? logDir : QString(), ipcPrefix, filePrefix, portOffset, logLevel >= 3, this);

		foreach(Service *s, services)
		{
			connect(s, &Service::started, this, &Private::service_started);
			connect(s, &Service::stopped, this, &Private::service_stopped);
			connect(s, &Service::logLine, this, &Private::service_logLine);
			connect(s, &Service::error, this, &Private::service_error);

			if(!args.mergeOutput || qobject_cast<Mongrel2Service*>(s))
				log_info("starting %s", qPrintable(s->name()));

			s->start();
		}
	}

private:
	QString tryInsertPrefix(const QString &line, const QString &prefix)
	{
		if(line.startsWith('['))
		{
			// find third space and insert the service name
			int at = 0;
			for(int n = 0; n < 3 && at != -1; ++n)
			{
				at = line.indexOf(' ', at);
				if(at != -1)
					++at;
			}

			if(at != -1)
			{
				QString out = line;
				out.insert(at, prefix);
				return out;
			}
		}

		return line;
	}

	void stopAll()
	{
		foreach(Service *s, services)
		{
			if(!args.mergeOutput || qobject_cast<Mongrel2Service*>(s))
				log_info("stopping %s", qPrintable(s->name()));

			s->stop();
		}
	}

	void checkStopped()
	{
		if(services.isEmpty())
		{
			log_info("stopped");
			emit q->quit(0);
		}
	}

private slots:
	void service_started()
	{
		bool allStarted = true;
		foreach(Service *s, services)
		{
			if(!s->isStarted())
			{
				allStarted = false;
				break;
			}
		}

		if(allStarted)
			log_info("started");
	}

	void service_stopped()
	{
		Service *s = (Service *)sender();

		services.removeAll(s);
		delete s;

		checkStopped();
	}

	void service_logLine(const QString &line)
	{
		Service *s = (Service *)sender();

		QString out = tryInsertPrefix(line, '[' + s->name() + "] ");
		log_raw(qPrintable(out));
	}

	void service_error(const QString &error)
	{
		Service *s = (Service *)sender();

		log_error("%s: %s", qPrintable(s->name()), qPrintable(error));

		services.removeAll(s);
		delete s;

		if(stopping)
		{
			checkStopped();
		}
		else
		{
			// shutdown if we receive an unexpected error from any service
			stopping = true;
			stopAll();
		}
	}

	void reload()
	{
		log_info("reloading");
		log_rotate();

		foreach(Service *s, services)
		{
			if(s->acceptSighup())
				s->sendSighup();
		}
	}

	void doQuit()
	{
		if(!stopping)
		{
			stopping = true;
			ProcessQuit::reset(); // allow user to quit again

			log_info("stopping...");
			stopAll();
		}
		else
		{
			qDeleteAll(services);
			ProcessQuit::cleanup();

			// if we were already stopping, then exit immediately
			emit q->quit(0);
		}
	}
};

App::App(QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
}

App::~App()
{
	delete d;
}

void App::start()
{
	d->start();
}

#include "app.moc"
