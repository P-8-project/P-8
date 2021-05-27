/*
 * Copyright (C) 2013 Fanout, Inc.
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
#include <QPair>
#include <QHash>
#include "qzmqsocket.h"
#include "qzmqvalve.h"
#include "processquit.h"
#include "packet/tnetstring.h"
#include "packet/m2requestpacket.h"
#include "packet/m2responsepacket.h"
#include "packet/zurlrequestpacket.h"
#include "packet/zurlresponsepacket.h"
#include "log.h"

#define VERSION "1.0.0"

#define DEFAULT_HWM 1000
#define ZHTTP_IDEAL_CREDITS 200000

static bool validateHost(const QByteArray &in)
{
	for(int n = 0; n < in.count(); ++n)
	{
		if(in[n] == '/')
			return false;
	}

	return true;
}

class App::Private : public QObject
{
	Q_OBJECT

public:
	typedef QPair<QByteArray, QByteArray> Rid;

	class Session
	{
	public:
		// we use the same id on both sides of the adapter
		//   m2 provides a unique request id for its instance
		//   zhttp expects a request id unique to our instance to reply to
		QByteArray id;

		// m2 stuff
		QByteArray httpVersion;
		bool persistent;
		bool noChunked;
		bool respondKeepAlive;
		bool respondClose;
		bool chunked;
		int offset;
		int written;

		// zhttp stuff
		QByteArray replyAddress;
		int outSeq;
		int inSeq;
		int credits;

		Session() :
			persistent(false),
			noChunked(false),
			respondKeepAlive(false),
			respondClose(false),
			chunked(false),
			offset(0),
			written(0),
			outSeq(0),
			inSeq(0),
			credits(0)
		{
		}
	};

	App *q;
	QZmq::Socket *m2_in_sock;
	QZmq::Socket *m2_out_sock;
	QZmq::Socket *m2_control_sock;
	QZmq::Socket *zhttp_in_sock;
	QZmq::Socket *zhttp_out_sock;
	QZmq::Socket *zhttp_out_stream_sock;
	QZmq::Valve *m2_in_valve;
	QZmq::Valve *zhttp_in_valve;
	QHash<QByteArray, Session*> sessionsById;
	QByteArray m2_out_ident;

	Private(App *_q) :
		QObject(_q),
		q(_q),
		m2_in_sock(0),
		m2_out_sock(0),
		m2_control_sock(0),
		zhttp_in_sock(0),
		zhttp_out_sock(0),
		zhttp_out_stream_sock(0),
		m2_in_valve(0)
	{
		connect(ProcessQuit::instance(), SIGNAL(quit()), SLOT(doQuit()));
	}

	~Private()
	{
	}

	void start()
	{
		log_startClock();

		QStringList args = QCoreApplication::instance()->arguments();
		args.removeFirst();

		// options
		QHash<QString, QString> options;
		for(int n = 0; n < args.count(); ++n)
		{
			if(args[n] == "--")
			{
				break;
			}
			else if(args[n].startsWith("--"))
			{
				QString opt = args[n].mid(2);
				QString var, val;

				int at = opt.indexOf("=");
				if(at != -1)
				{
					var = opt.mid(0, at);
					val = opt.mid(at + 1);
				}
				else
					var = opt;

				options[var] = val;

				args.removeAt(n);
				--n; // adjust position
			}
		}

		if(options.contains("version"))
		{
			printf("m2adapter %s\n", VERSION);
			emit q->quit();
			return;
		}

		log_info("starting...");

		if(options.contains("verbose"))
			log_setOutputLevel(LOG_LEVEL_DEBUG);
		else
			log_setOutputLevel(LOG_LEVEL_INFO);

		QString configFile = options["config"];
		if(configFile.isEmpty())
			configFile = "/etc/m2adapter.conf";

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

		QSettings settings(configFile, QSettings::IniFormat);

		QString m2_in_spec = settings.value("m2_in_spec").toString();
		QString m2_out_spec = settings.value("m2_out_spec").toString();
		m2_out_ident = settings.value("m2_out_ident").toString().toUtf8();
		QString m2_control_spec = settings.value("m2_control_spec").toString();
		bool zhttp_connect = settings.value("zhttp_connect").toBool();
		QString zhttp_in_spec = settings.value("zhttp_in_spec").toString();
		QString zhttp_out_spec = settings.value("zhttp_out_spec").toString();
		QString zhttp_out_stream_spec = settings.value("zhttp_out_stream_spec").toString();

		m2_in_sock = new QZmq::Socket(QZmq::Socket::Pull, this);
		m2_in_sock->setHwm(DEFAULT_HWM);
		m2_in_sock->connectToAddress(m2_in_spec);

		m2_out_sock = new QZmq::Socket(QZmq::Socket::Pub, this);
		m2_out_sock->setHwm(DEFAULT_HWM);
		m2_out_sock->setWriteQueueEnabled(false);
		m2_out_sock->connectToAddress(m2_out_spec);

		m2_in_valve = new QZmq::Valve(m2_in_sock, this);
		connect(m2_in_valve, SIGNAL(readyRead(const QList<QByteArray> &)), SLOT(m2_in_readyRead(const QList<QByteArray> &)));

		zhttp_in_sock = new QZmq::Socket(QZmq::Socket::Sub, this);
		zhttp_in_sock->setHwm(DEFAULT_HWM);
		zhttp_in_sock->subscribe(m2_out_ident + ' ');
		if(zhttp_connect)
		{
			zhttp_in_sock->connectToAddress(zhttp_in_spec);
		}
		else
		{
			if(!zhttp_in_sock->bind(zhttp_in_spec))
			{
				log_error("unable to bind to zhttp_in_spec: %s", qPrintable(zhttp_in_spec));
				emit q->quit();
				return;
			}
		}

		zhttp_in_valve = new QZmq::Valve(zhttp_in_sock, this);
		connect(zhttp_in_valve, SIGNAL(readyRead(const QList<QByteArray> &)), SLOT(zhttp_in_readyRead(const QList<QByteArray> &)));

		zhttp_out_sock = new QZmq::Socket(QZmq::Socket::Push, this);
		zhttp_out_sock->setHwm(DEFAULT_HWM);
		if(zhttp_connect)
		{
			zhttp_out_sock->connectToAddress(zhttp_out_spec);
		}
		else
		{
			if(!zhttp_out_sock->bind(zhttp_out_spec))
			{
				log_error("unable to bind to zhttp_out_spec: %s", qPrintable(zhttp_out_spec));
				emit q->quit();
				return;
			}
		}

		zhttp_out_stream_sock = new QZmq::Socket(QZmq::Socket::Router, this);
		zhttp_out_stream_sock->setHwm(DEFAULT_HWM);
		if(zhttp_connect)
		{
			zhttp_out_stream_sock->connectToAddress(zhttp_out_stream_spec);
		}
		else
		{
			if(!zhttp_out_stream_sock->bind(zhttp_out_stream_spec))
			{
				log_error("unable to bind to zhttp_out_stream_spec: %s", qPrintable(zhttp_out_stream_spec));
				emit q->quit();
				return;
			}
		}

		m2_in_valve->open();
		zhttp_in_valve->open();

		log_info("started");
	}

	void m2_out_write(const M2ResponsePacket &packet)
	{
		QByteArray buf = packet.toByteArray();
		//log_debug("writing: [%s]", buf.data());
		m2_out_sock->write(QList<QByteArray>() << buf);
	}

	void zhttp_out_write(const ZurlRequestPacket &packet)
	{
		QByteArray buf = TnetString::fromVariant(packet.toVariant());

		log_debug("zhttp: OUT %s", buf.data());

		zhttp_out_sock->write(QList<QByteArray>() << buf);
	}

	void zhttp_out_write(const ZurlRequestPacket &packet, const QByteArray &instanceAddress)
	{
		QByteArray buf = TnetString::fromVariant(packet.toVariant());

		log_debug("zhttp: OUT %s", buf.data());

		QList<QByteArray> message;
		message += instanceAddress;
		message += QByteArray();
		message += buf;
		zhttp_out_stream_sock->write(message);
	}

private slots:
	void m2_in_readyRead(const QList<QByteArray> &message)
	{
		if(message.count() != 1)
		{
			log_warning("m2: received message with parts != 1, skipping");
			return;
		}

		M2RequestPacket mreq;
		if(!mreq.fromByteArray(message[0]))
		{
			log_warning("m2: received message with invalid format, skipping");
			return;
		}

		if(mreq.isDisconnect)
		{
			log_debug("m2: id=%s disconnected", mreq.id.data());

			Session *s = sessionsById.value(mreq.id);
			if(s)
			{
				// if a worker had ack'd this session, then send cancel
				if(!s->replyAddress.isEmpty())
				{
					ZurlRequestPacket zreq;
					zreq.sender = m2_out_ident;
					zreq.id = s->id;
					zreq.seq = (s->outSeq)++;
					zreq.cancel = true;
					zhttp_out_write(zreq, s->replyAddress);
				}

				sessionsById.remove(mreq.id);
				delete s;
			}

			return;
		}

		// TODO: handle upload stream packets

		QByteArray uri;

		if(mreq.scheme == "https")
			uri += "https://";
		else
			uri += "http://";

		QByteArray host = mreq.headers.get("Host");
		if(host.isEmpty())
			host = "localhost";

		int at = host.indexOf(':');
		if(at != -1)
			host = host.mid(0, at);

		if(!validateHost(host))
		{
			log_warning("m2: invalid host [%s], skipping", host.data());
			return;
		}

		if(!mreq.uri.startsWith('/'))
		{
			log_warning("m2: invalid uri [%s], skipping", mreq.uri.data());
			return;
		}

		uri += host;
		uri += mreq.uri;

		Session *s = sessionsById.value(mreq.id);
		if(s)
		{
			log_warning("m2: received duplicate request id=%s, skipping", mreq.id.data());
			return;
		}
		else
		{
			if(mreq.version != "HTTP/1.0" && mreq.version != "HTTP/1.1")
			{
				log_warning("m2: id=%s skipping unknown version: %s", mreq.id.data(), mreq.version.data());
				return;
			}

			s = new Session;
			s->id = mreq.id;
			s->httpVersion = mreq.version;

			if(mreq.version == "HTTP/1.0")
			{
				if(mreq.headers.getAll("Connection").contains("Keep-Alive"))
				{
					s->persistent = true;
					s->respondKeepAlive = true;
				}

				s->noChunked = true;
			}
			else if(mreq.version == "HTTP/1.1")
			{
				if(mreq.headers.getAll("Connection").contains("close"))
					s->respondClose = true;
				else
					s->persistent = true;
			}

			sessionsById.insert(mreq.id, s);

			log_info("m2: id=%s request %s", s->id.data(), uri.data());

			ZurlRequestPacket zreq;
			zreq.sender = m2_out_ident;
			zreq.id = s->id;
			zreq.seq = (s->outSeq)++;
			zreq.credits = ZHTTP_IDEAL_CREDITS;
			zreq.stream = true;
			zreq.method = mreq.method;
			zreq.ignorePolicies = true;
			zreq.uri = QUrl::fromEncoded(uri, QUrl::StrictMode);
			zhttp_out_write(zreq);
		}
	}

	void zhttp_in_readyRead(const QList<QByteArray> &message)
	{
		if(message.count() != 1)
		{
			log_warning("zhttp: received message with parts != 1, skipping");
			return;
		}

		int at = message[0].indexOf(' ');
		if(at == -1)
		{
			log_warning("zhttp: received message with invalid format, skipping");
			return;
		}

		QByteArray dataRaw = message[0].mid(at + 1);
		QVariant data = TnetString::toVariant(dataRaw);
		if(data.isNull())
		{
			log_warning("zhttp: received message with invalid format (tnetstring parse failed), skipping");
			return;
		}

		log_debug("zhttp: IN %s", dataRaw.data());

		ZurlResponsePacket zresp;
		if(!zresp.fromVariant(data))
		{
			log_warning("zhttp: received message with invalid format (parse failed), skipping");
			return;
		}

		Session *s = sessionsById.value(zresp.id);
		if(!s)
		{
			log_debug("zhttp: received message for unknown request id, canceling");

			// if this was not an error packet, send cancel
			if(zresp.condition.isEmpty() && !zresp.replyAddress.isEmpty())
			{
				ZurlRequestPacket zreq;
				zreq.sender = m2_out_ident;
				zreq.id = zresp.id;
				zreq.seq = (s->outSeq)++;
				zreq.cancel = true;
				zhttp_out_write(zreq, zresp.replyAddress);
			}

			return;
		}

		if(s->replyAddress.isEmpty() && zresp.replyAddress.isEmpty())
		{
			log_warning("zhttp: received first response with no reply address, canceling");
			sessionsById.remove(s->id);
			delete s;
			return;
		}

		if(!zresp.replyAddress.isEmpty())
			s->replyAddress = zresp.replyAddress;

		if(!zresp.body.isNull())
		{
			log_debug("zhttp: id=%s response data size=%d%s", s->id.data(), zresp.body.size(), zresp.more ? " M" : "");

			if(s->written == 0)
			{
				// TODO: if response has no fixed-length and s->noChunked then disable persistence
				// TODO: if response has no fixed-length and !s->noChunked then use chunking

				s->persistent = false;
				s->respondKeepAlive = false;

				M2ResponsePacket mresp;
				mresp.sender = m2_out_ident;
				mresp.id = s->id;
				mresp.data = "HTTP/1.1 200 OK\r\n";
				if(s->respondKeepAlive)
					mresp.data += "Connection: Keep-Alive\r\n";
				if(s->respondClose)
					mresp.data += "Connection: close\r\n";
				mresp.data += "Content-Type: " + zresp.headers.get("Content-Type") + "\r\n";
				//mresp.data += "Content-Length: " + QByteArray::number(zresp.body.size()) + "\r\n";
				mresp.data += "\r\n";
				mresp.data += zresp.body;

				m2_out_write(mresp);

				s->written += mresp.data.size();
			}
			else if(!zresp.body.isEmpty())
			{
				M2ResponsePacket mresp;
				mresp.sender = m2_out_ident;
				mresp.id = s->id;
				mresp.data = zresp.body;
				m2_out_write(mresp);

				s->written += mresp.data.size();
			}

			if(!zresp.more)
			{
				if(!s->persistent)
				{
					// close
					M2ResponsePacket mresp;
					mresp.sender = m2_out_ident;
					mresp.id = s->id;
					mresp.data = "";
					m2_out_write(mresp);
				}

				sessionsById.remove(s->id);
				delete s;
			}
			else
			{
				// FIXME: m2 flow control instead of giving credits immediately
				// give credits
				if(!zresp.body.isEmpty())
				{
					ZurlRequestPacket zreq;
					zreq.sender = m2_out_ident;
					zreq.id = s->id;
					zreq.seq = (s->outSeq)++;
					zreq.credits = zresp.body.size();
					zhttp_out_write(zreq, s->replyAddress);
				}
			}
		}
	}

	void doQuit()
	{
		log_info("stopping...");

		// remove the handler, so if we get another signal then we crash out
		ProcessQuit::cleanup();

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
