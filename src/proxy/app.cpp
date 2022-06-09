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

#include "app.h"

#include <assert.h>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include "processquit.h"
#include "log.h"
#include "settings.h"
#include "xffrule.h"
#include "engine.h"
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

static QByteArray parse_key(const QString &in)
{
	if(in.startsWith("base64:"))
		return QByteArray::fromBase64(in.mid(7).toUtf8());
	else
		return in.toUtf8();
}

static XffRule parse_xffRule(const QStringList &in)
{
	XffRule out;
	foreach(const QString &s, in)
	{
		if(s.startsWith("truncate:"))
		{
			bool ok;
			int x = s.mid(9).toInt(&ok);
			if(!ok)
				return out;

			out.truncate = x;
		}
		else if(s == "append")
			out.append = true;
	}
	return out;
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
	QString ipcPrefix;
	QStringList routeLines;

	ArgsData() :
		logLevel(-1)
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
	const QCommandLineOption ipcPrefixOption("ipc-prefix", "Override ipc_prefix config option.", "prefix");
	parser->addOption(ipcPrefixOption);
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

	if(parser->isSet(ipcPrefixOption))
		args->ipcPrefix = parser->value(ipcPrefixOption);

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
	Engine *engine;

	Private(App *_q) :
		QObject(_q),
		q(_q),
		engine(0)
	{
		connect(ProcessQuit::instance(), SIGNAL(quit()), SLOT(doQuit()));
		connect(ProcessQuit::instance(), SIGNAL(hup()), SLOT(reload()));
	}

	void start()
	{
		QCoreApplication::setApplicationName("p-8-proxy");
		QCoreApplication::setApplicationVersion(VERSION);

		QCommandLineParser parser;
		parser.setApplicationDescription("P-8 proxy component.");

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

		if(args.logLevel != -1)
			log_setOutputLevel(args.logLevel);
		else
			log_setOutputLevel(LOG_LEVEL_INFO);

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

		QString configFile = args.configFile;
		if(configFile.isEmpty())
			configFile = QDir(CONFIGDIR).filePath("p-8.conf");

		// QSettings doesn't inform us if the config file doesn't exist, so do that ourselves
		{
			QFile file(configFile);
			if(!file.open(QIODevice::ReadOnly))
			{
				log_error("failed to open %s, and --config not passed", qPrintable(configFile));
				emit q->quit();
				return;
			}
		}

		Settings settings(configFile);

		if(!args.ipcPrefix.isEmpty())
			settings.setIpcPrefix(args.ipcPrefix);

		QStringList m2a_in_specs = settings.value("proxy/m2a_in_specs").toStringList();
		trimlist(&m2a_in_specs);
		QStringList m2a_in_stream_specs = settings.value("proxy/m2a_in_stream_specs").toStringList();
		trimlist(&m2a_in_stream_specs);
		QStringList m2a_out_specs = settings.value("proxy/m2a_out_specs").toStringList();
		trimlist(&m2a_out_specs);
		QStringList zurl_out_specs = settings.value("proxy/zurl_out_specs").toStringList();
		trimlist(&zurl_out_specs);
		QStringList zurl_out_stream_specs = settings.value("proxy/zurl_out_stream_specs").toStringList();
		trimlist(&zurl_out_stream_specs);
		QStringList zurl_in_specs = settings.value("proxy/zurl_in_specs").toStringList();
		trimlist(&zurl_in_specs);
		QString handler_inspect_spec = settings.value("proxy/handler_inspect_spec").toString();
		QString handler_accept_spec = settings.value("proxy/handler_accept_spec").toString();
		QString handler_retry_in_spec = settings.value("proxy/handler_retry_in_spec").toString();
		QString handler_ws_control_in_spec = settings.value("proxy/handler_ws_control_in_spec").toString();
		QString handler_ws_control_out_spec = settings.value("proxy/handler_ws_control_out_spec").toString();
		QString stats_spec = settings.value("proxy/stats_spec").toString();
		QString command_spec = settings.value("proxy/command_spec").toString();
		bool ok;
		int ipcFileMode = settings.value("proxy/ipc_file_mode", -1).toString().toInt(&ok, 8);
		int maxWorkers = settings.value("proxy/max_open_requests", -1).toInt();
		QString routesFile = settings.value("proxy/routesfile").toString();
		bool autoCrossOrigin = settings.value("proxy/auto_cross_origin").toBool();
		bool acceptXForwardedProtocol = settings.value("proxy/accept_x_forwarded_protocol").toBool();
		bool useXForwardedProtocol = settings.value("proxy/set_x_forwarded_protocol").toBool();
		XffRule xffRule = parse_xffRule(settings.value("proxy/x_forwarded_for").toStringList());
		XffRule xffTrustedRule = parse_xffRule(settings.value("proxy/x_forwarded_for_trusted").toStringList());
		QStringList origHeadersNeedMarkStr = settings.value("proxy/orig_headers_need_mark").toStringList();
		trimlist(&origHeadersNeedMarkStr);
		QByteArray sigKey = parse_key(settings.value("proxy/sig_key").toString());
		QByteArray upstreamKey = parse_key(settings.value("proxy/upstream_key").toString());
		QString sockJsUrl = settings.value("proxy/sockjs_url").toString();
		bool updatesCheck = settings.value("proxy/updates_check", true).toBool();
		QString organizationName = settings.value("proxy/organization_name").toString();

		QList<QByteArray> origHeadersNeedMark;
		foreach(const QString &s, origHeadersNeedMarkStr)
			origHeadersNeedMark += s.toUtf8();

		// if routesfile is a relative path, then use it relative to the config file location
		QFileInfo fi(routesFile);
		if(fi.isRelative())
			routesFile = QFileInfo(QFileInfo(configFile).absoluteDir(), routesFile).filePath();

		if(m2a_in_specs.isEmpty() || m2a_in_stream_specs.isEmpty() || m2a_out_specs.isEmpty() || zurl_out_specs.isEmpty() || zurl_out_stream_specs.isEmpty() || zurl_in_specs.isEmpty())
		{
			log_error("must set m2a_in_specs, m2a_in_stream_specs, m2a_out_specs, zurl_out_specs, zurl_out_stream_specs, and zurl_in_specs");
			emit q->quit();
			return;
		}

		Engine::Configuration config;
		config.appVersion = VERSION;
		config.clientId = "p-8-proxy_" + QByteArray::number(QCoreApplication::applicationPid());
		config.serverInSpecs = m2a_in_specs;
		config.serverInStreamSpecs = m2a_in_stream_specs;
		config.serverOutSpecs = m2a_out_specs;
		config.clientOutSpecs = zurl_out_specs;
		config.clientOutStreamSpecs = zurl_out_stream_specs;
		config.clientInSpecs = zurl_in_specs;
		config.inspectSpec = handler_inspect_spec;
		config.acceptSpec = handler_accept_spec;
		config.retryInSpec = handler_retry_in_spec;
		config.wsControlInSpec = handler_ws_control_in_spec;
		config.wsControlOutSpec = handler_ws_control_out_spec;
		config.statsSpec = stats_spec;
		config.commandSpec = command_spec;
		config.ipcFileMode = ipcFileMode;
		config.maxWorkers = maxWorkers;
		if(!args.routeLines.isEmpty())
			config.routeLines = args.routeLines;
		else
			config.routesFile = routesFile;
		config.autoCrossOrigin = autoCrossOrigin;
		config.acceptXForwardedProtocol = acceptXForwardedProtocol;
		config.useXForwardedProtocol = useXForwardedProtocol;
		config.xffUntrustedRule = xffRule;
		config.xffTrustedRule = xffTrustedRule;
		config.origHeadersNeedMark = origHeadersNeedMark;
		config.sigIss = "p-8";
		config.sigKey = sigKey;
		config.upstreamKey = upstreamKey;
		config.sockJsUrl = sockJsUrl;
		config.updatesCheck = updatesCheck;
		config.organizationName = organizationName;

		engine = new Engine(this);
		if(!engine->start(config))
		{
			emit q->quit();
			return;
		}

		log_info("started");
	}

private slots:
	void reload()
	{
		log_info("reloading");
		log_rotate();
		engine->reload();
	}

	void doQuit()
	{
		log_info("stopping...");

		// remove the handler, so if we get another signal then we crash out
		ProcessQuit::cleanup();

		delete engine;
		engine = 0;

		log_info("stopped");
		emit q->quit();
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
