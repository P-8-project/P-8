/*
 * Copyright (C) 2014-2017 Fanout, Inc.
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

#include "wsproxysession.h"

#include <assert.h>
#include <QTimer>
#include <QDateTime>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include "packet/httprequestdata.h"
#include "log.h"
#include "zhttpmanager.h"
#include "zwebsocket.h"
#include "websocketoverhttp.h"
#include "zroutes.h"
#include "wscontrolmanager.h"
#include "wscontrolsession.h"
#include "xffrule.h"
#include "proxyutil.h"
#include "statsmanager.h"
#include "inspectdata.h"
#include "connectionmanager.h"
#include "testwebsocket.h"

#define ACTIVITY_TIMEOUT 60000
#define FRAME_SIZE_MAX 16384

class HttpExtension
{
public:
	bool isNull() const { return name.isEmpty(); }

	QByteArray name;
	QHash<QByteArray, QByteArray> params;
};

static int findNext(const QByteArray &in, const char *charList, int start = 0)
{
	int len = qstrlen(charList);
	for(int n = start; n < in.size(); ++n)
	{
		char c = in[n];
		for(int i = 0; i < len; ++i)
		{
			if(c == charList[i])
				return n;
		}
	}

	return -1;
}

static QHash<QByteArray, QByteArray> parseParams(const QByteArray &in, bool *ok = 0)
{
	QHash<QByteArray, QByteArray> out;

	int start = 0;
	while(start < in.size())
	{
		QByteArray var;
		QByteArray val;

		int at = findNext(in, "=;", start);
		if(at != -1)
		{
			var = in.mid(start, at - start).trimmed();
			if(in[at] == '=')
			{
				if(at + 1 >= in.size())
				{
					if(ok)
						*ok = false;
					return QHash<QByteArray, QByteArray>();
				}

				++at;

				if(in[at] == '\"')
				{
					++at;

					bool complete = false;
					for(int n = at; n < in.size(); ++n)
					{
						if(in[n] == '\\')
						{
							if(n + 1 >= in.size())
							{
								if(ok)
									*ok = false;
								return QHash<QByteArray, QByteArray>();
							}

							++n;
							val += in[n];
						}
						else if(in[n] == '\"')
						{
							complete = true;
							at = n + 1;
							break;
						}
						else
							val += in[n];
					}

					if(!complete)
					{
						if(ok)
							*ok = false;
						return QHash<QByteArray, QByteArray>();
					}

					at = in.indexOf(';', at);
					if(at != -1)
						start = at + 1;
					else
						start = in.size();
				}
				else
				{
					int vstart = at;
					at = in.indexOf(';', vstart);
					if(at != -1)
					{
						val = in.mid(vstart, at - vstart).trimmed();
						start = at + 1;
					}
					else
					{
						val = in.mid(vstart).trimmed();
						start = in.size();
					}
				}
			}
			else
				start = at + 1;
		}
		else
		{
			var = in.mid(start).trimmed();
			start = in.size();
		}

		out[var] = val;
	}

	if(ok)
		*ok = true;

	return out;
}

static QByteArray getExtensionRaw(const QList<QByteArray> &extStrings, const QByteArray &name)
{
	foreach(const QByteArray &ext, extStrings)
	{
		int at = ext.indexOf(';');
		if(at != -1)
		{
			if(ext.mid(0, at).trimmed() == name)
				return ext;
		}
		else
		{
			if(ext == name)
				return ext;
		}
	}

	return QByteArray();
}

static HttpExtension getExtension(const QList<QByteArray> &extStrings, const QByteArray &name)
{
	QByteArray ext = getExtensionRaw(extStrings, name);
	if(ext.isNull())
		return HttpExtension();

	HttpExtension e;
	e.name = name;

	int at = ext.indexOf(';');
	if(at != -1)
	{
		bool ok;
		e.params = parseParams(ext.mid(at + 1), &ok);
		if(!ok)
			return HttpExtension();
	}

	return e;
}

class WsProxySession::Private : public QObject
{
	Q_OBJECT

public:
	enum State
	{
		Idle,
		Connecting,
		Connected,
		Closing
	};

	typedef QPair<WebSocket::Frame, bool> QueuedFrame;

	WsProxySession *q;
	State state;
	ZRoutes *zroutes;
	ZhttpManager *zhttpManager;
	ConnectionManager *connectionManager;
	StatsManager *statsManager;
	WsControlManager *wsControlManager;
	WsControlSession *wsControl;
	QByteArray defaultSigIss;
	QByteArray defaultSigKey;
	QByteArray defaultUpstreamKey;
	bool passToUpstream;
	bool acceptXForwardedProtocol;
	bool useXForwardedProtocol;
	XffRule xffRule;
	XffRule xffTrustedRule;
	QList<QByteArray> origHeadersNeedMark;
	HttpRequestData requestData;
	QHostAddress logicalClientAddress;
	WebSocket *inSock;
	WebSocket *outSock;
	int inPendingBytes;
	QList<bool> inPendingFrames; // true means we should ack a send event
	int outPendingBytes;
	int outReadInProgress; // frame type or -1
	QByteArray pathBeg;
	QByteArray routeId;
	QByteArray channelPrefix;
	QList<DomainMap::Target> targets;
	DomainMap::Target target;
	bool acceptGripMessages;
	QByteArray messagePrefix;
	bool detached;
	QString subChannel;
	QDateTime activityTime;
	QByteArray publicCid;
	QTimer *keepAliveTimer;
	QList<QueuedFrame> queuedInFrames; // frames to deliver after out read finishes
	LogUtil::Config logConfig;

	Private(WsProxySession *_q, ZRoutes *_zroutes, ConnectionManager *_connectionManager, const LogUtil::Config &_logConfig, StatsManager *_statsManager, WsControlManager *_wsControlManager) :
		QObject(_q),
		q(_q),
		state(Idle),
		zroutes(_zroutes),
		zhttpManager(0),
		connectionManager(_connectionManager),
		statsManager(_statsManager),
		wsControlManager(_wsControlManager),
		wsControl(0),
		passToUpstream(false),
		acceptXForwardedProtocol(false),
		useXForwardedProtocol(false),
		inSock(0),
		outSock(0),
		inPendingBytes(0),
		outPendingBytes(0),
		outReadInProgress(-1),
		acceptGripMessages(false),
		detached(false),
		keepAliveTimer(0),
		logConfig(_logConfig)
	{
	}

	~Private()
	{
		cleanup();
	}

	void cleanup()
	{
		cleanupKeepAliveTimer();

		cleanupInSock();

		delete outSock;
		outSock = 0;

		delete wsControl;
		wsControl = 0;

		if(zhttpManager)
		{
			zroutes->removeRef(zhttpManager);
			zhttpManager = 0;
		}
	}

	void cleanupInSock()
	{
		if(inSock)
		{
			connectionManager->removeConnection(inSock);
			delete inSock;
			inSock = 0;
		}
	}

	void cleanupKeepAliveTimer()
	{
		if(keepAliveTimer)
		{
			keepAliveTimer->disconnect(this);
			keepAliveTimer->setParent(0);
			keepAliveTimer->deleteLater();
			keepAliveTimer = 0;
		}
	}

	void start(WebSocket *sock, const QByteArray &_publicCid, const DomainMap::Entry &entry)
	{
		assert(!inSock);

		state = Connecting;

		publicCid = _publicCid;

		if(statsManager)
			activityTime = QDateTime::currentDateTimeUtc();

		inSock = sock;
		inSock->setParent(this);
		connect(inSock, &WebSocket::readyRead, this, &Private::in_readyRead);
		connect(inSock, &WebSocket::framesWritten, this, &Private::in_framesWritten);
		connect(inSock, &WebSocket::peerClosed, this, &Private::in_peerClosed);
		connect(inSock, &WebSocket::closed, this, &Private::in_closed);
		connect(inSock, &WebSocket::error, this, &Private::in_error);

		requestData.uri = inSock->requestUri();
		requestData.headers = inSock->requestHeaders();

		bool trustedClient = ProxyUtil::checkTrustedClient("wsproxysession", q, requestData, defaultUpstreamKey);

		logicalClientAddress = ProxyUtil::getLogicalAddress(requestData.headers, trustedClient ? xffTrustedRule : xffRule, inSock->peerAddress());

		QString host = requestData.uri.host();

		if(entry.isNull())
		{
			log_warning("wsproxysession: %p %s has 0 routes", q, qPrintable(host));
			reject(false, 502, "Bad Gateway", QString("No route for host: %1").arg(host));
			return;
		}

		if(!entry.asHost.isEmpty())
			ProxyUtil::applyHost(&requestData.uri, entry.asHost);

		QByteArray path = requestData.uri.path(QUrl::FullyEncoded).toUtf8();

		if(entry.pathRemove > 0)
			path = path.mid(entry.pathRemove);

		if(!entry.pathPrepend.isEmpty())
			path = entry.pathPrepend + path;

		requestData.uri.setPath(QString::fromUtf8(path), QUrl::StrictMode);

		QByteArray sigIss;
		QByteArray sigKey;
		if(!entry.sigIss.isEmpty() && !entry.sigKey.isEmpty())
		{
			sigIss = entry.sigIss;
			sigKey = entry.sigKey;
		}
		else
		{
			sigIss = defaultSigIss;
			sigKey = defaultSigKey;
		}

		pathBeg = entry.pathBeg;
		routeId = entry.id;
		channelPrefix = entry.prefix;
		targets = entry.targets;

		log_debug("wsproxysession: %p %s has %d routes", q, qPrintable(host), targets.count());

		foreach(const HttpHeader &h, entry.headers)
		{
			requestData.headers.removeAll(h.first);
			requestData.headers += HttpHeader(h.first, h.second);
		}

		QHostAddress clientAddress = inSock->peerAddress();

		ProxyUtil::manipulateRequestHeaders("wsproxysession", q, &requestData, trustedClient, entry, sigIss, sigKey, acceptXForwardedProtocol, useXForwardedProtocol, xffTrustedRule, xffRule, origHeadersNeedMark, clientAddress, InspectData(), true);

		// don't proxy extensions, as we may not know how to handle them
		requestData.headers.removeAll("Sec-WebSocket-Extensions");

		// send grip extension
		requestData.headers += HttpHeader("Sec-WebSocket-Extensions", "grip");

		if(trustedClient)
			passToUpstream = true;

		tryNextTarget();
	}

	void writeInFrame(const WebSocket::Frame &frame, bool fromSendEvent = false)
	{
		inPendingBytes += frame.data.size();
		inPendingFrames += fromSendEvent;

		inSock->writeFrame(frame);
	}

	void tryNextTarget()
	{
		if(targets.isEmpty())
		{
			reject(false, 502, "Bad Gateway", "Error while proxying to origin.");
			return;
		}

		target = targets.takeFirst();

		QUrl uri = requestData.uri;
		if(target.ssl)
			uri.setScheme("wss");
		else
			uri.setScheme("ws");

		if(!target.host.isEmpty())
			ProxyUtil::applyHost(&uri, target.host);

		subChannel = target.subChannel;

		if(zhttpManager)
		{
			zroutes->removeRef(zhttpManager);
			zhttpManager = 0;
		}

		if(target.type == DomainMap::Target::Test)
		{
			// for test route, auto-adjust path
			if(!pathBeg.isEmpty())
			{
				int pathRemove = pathBeg.length();
				if(pathBeg.endsWith('/'))
					--pathRemove;

				if(pathRemove > 0)
					uri.setPath(uri.path(QUrl::FullyEncoded).mid(pathRemove));
			}

			outSock = new TestWebSocket(this);
		}
		else
		{
			if(target.type == DomainMap::Target::Custom)
			{
				zhttpManager = zroutes->managerForRoute(target.zhttpRoute);
				log_debug("wsproxysession: %p forwarding to %s", q, qPrintable(target.zhttpRoute.baseSpec));
			}
			else // Default
			{
				zhttpManager = zroutes->defaultManager();
				log_debug("wsproxysession: %p forwarding to %s:%d", q, qPrintable(target.connectHost), target.connectPort);
			}

			zroutes->addRef(zhttpManager);

			if(target.overHttp)
			{
				WebSocketOverHttp *woh = new WebSocketOverHttp(zhttpManager, this);
				woh->setConnectionId(publicCid);
				outSock = woh;
			}
			else
			{
				// websockets don't work with zhttp req mode
				if(zhttpManager->clientUsesReq())
				{
					reject(false, 502, "Bad Gateway", "Error while proxying to origin.");
					return;
				}

				outSock = zhttpManager->createSocket();
				outSock->setParent(this);
			}
		}

		connect(outSock, &WebSocket::connected, this, &Private::out_connected);
		connect(outSock, &WebSocket::readyRead, this, &Private::out_readyRead);
		connect(outSock, &WebSocket::framesWritten, this, &Private::out_framesWritten);
		connect(outSock, &WebSocket::peerClosed, this, &Private::out_peerClosed);
		connect(outSock, &WebSocket::closed, this, &Private::out_closed);
		connect(outSock, &WebSocket::error, this, &Private::out_error);

		if(target.trusted)
			outSock->setIgnorePolicies(true);

		if(target.trustConnectHost)
			outSock->setTrustConnectHost(true);

		if(target.insecure)
			outSock->setIgnoreTlsErrors(true);

		if(target.type == DomainMap::Target::Default)
		{
			outSock->setConnectHost(target.connectHost);
			outSock->setConnectPort(target.connectPort);
		}

		ProxyUtil::applyHostHeader(&requestData.headers, uri);

		outSock->start(uri, requestData.headers);
	}

	void reject(bool proxied, int code, const QByteArray &reason, const HttpHeaders &headers, const QByteArray &body)
	{
		assert(state == Connecting);

		state = Closing;
		inSock->respondError(code, reason, headers, body);

		logConnection(proxied, code, body.size());
	}

	void reject(bool proxied, int code, const QString &reason, const QString &errorMessage)
	{
		reject(proxied, code, reason.toUtf8(), HttpHeaders(), (errorMessage + '\n').toUtf8());
	}

	void tryReadIn()
	{
		while(inSock->framesAvailable() > 0 && ((outSock && outSock->canWrite()) || detached))
		{
			WebSocket::Frame f = inSock->readFrame();

			tryLogActivity();

			if(detached)
				continue;

			outSock->writeFrame(f);
			outPendingBytes += f.data.size();
		}
	}

	void tryReadOut()
	{
		while(outSock->framesAvailable() > 0 && ((inSock && inSock->canWrite()) || detached))
		{
			WebSocket::Frame f = outSock->readFrame();

			tryLogActivity();

			if(detached && outReadInProgress == -1)
				continue;

			if(f.type == WebSocket::Frame::Text || f.type == WebSocket::Frame::Binary || f.type == WebSocket::Frame::Continuation)
			{
				// we are skipping the rest of this message
				if(f.type == WebSocket::Frame::Continuation && outReadInProgress == -1)
					continue;

				if(f.type != WebSocket::Frame::Continuation)
					outReadInProgress = (int)f.type;

				if(wsControl && acceptGripMessages)
				{
					if(f.type == WebSocket::Frame::Text && f.data.startsWith("c:"))
					{
						// grip messages must only be one frame
						if(!f.more)
							wsControl->sendGripMessage(f.data.mid(2)); // process
						else
							outReadInProgress = -1; // ignore rest of message
					}
					else if(f.type != WebSocket::Frame::Continuation)
					{
						if(f.data.startsWith(messagePrefix))
						{
							f.data = f.data.mid(messagePrefix.size());
							writeInFrame(f);

							restartKeepAlive();
						}
						else
						{
							log_debug("wsproxysession: dropping unprefixed message");
						}
					}
					else if(f.type == WebSocket::Frame::Continuation)
					{
						assert(outReadInProgress != -1);

						writeInFrame(f);

						restartKeepAlive();
					}
				}
				else
				{
					writeInFrame(f);

					restartKeepAlive();
				}

				if(!f.more)
					outReadInProgress = -1;
			}
			else
			{
				// always relay non-content frames
				writeInFrame(f);

				restartKeepAlive();
			}

			if(outReadInProgress == -1 && !queuedInFrames.isEmpty())
			{
				foreach(const QueuedFrame &i, queuedInFrames)
					writeInFrame(i.first, i.second);
			}
		}
	}

	void tryFinish()
	{
		if(!inSock && !outSock)
		{
			cleanup();
			emit q->finishedByPassthrough();
		}
	}

	void tryLogActivity()
	{
		if(statsManager && !activityTime.isNull())
		{
			QDateTime now = QDateTime::currentDateTimeUtc();
			if(now >= activityTime.addMSecs(ACTIVITY_TIMEOUT))
			{
				statsManager->addActivity(routeId);

				activityTime = activityTime.addMSecs((activityTime.msecsTo(now) / ACTIVITY_TIMEOUT) * ACTIVITY_TIMEOUT);
			}
		}
	}

	void logConnection(bool proxied, int responseCode, int responseBodySize)
	{
		LogUtil::RequestData rd;

		rd.routeId = routeId;

		if(responseCode != -1)
		{
			rd.status = LogUtil::Response;
			rd.responseData.code = responseCode;
			rd.responseBodySize = responseBodySize;
		}
		else
		{
			rd.status = LogUtil::Error;
		}

		rd.requestData.method = "GET";
		rd.requestData.uri = inSock->requestUri();
		rd.requestData.headers = inSock->requestHeaders();

		if(proxied)
		{
			rd.targetStr = ProxyUtil::targetToString(target);
			rd.targetOverHttp = target.overHttp;
		}

		rd.fromAddress = logicalClientAddress;

		LogUtil::logRequest(LOG_LEVEL_INFO, rd, logConfig);
	}

	void restartKeepAlive()
	{
		if(keepAliveTimer)
			keepAliveTimer->start();
	}

private slots:
	void in_readyRead()
	{
		if((outSock && outSock->state() == WebSocket::Connected) || detached)
			tryReadIn();
	}

	void in_framesWritten(int count, int contentBytes)
	{
		Q_UNUSED(count);

		inPendingBytes -= contentBytes;

		for(int n = 0; n < count; ++n)
		{
			bool fromSendEvent = inPendingFrames.takeFirst();
			if(fromSendEvent)
				wsControl->sendEventWritten();
		}

		if(!detached && outSock)
			tryReadOut();
	}

	void in_peerClosed()
	{
		if(detached)
		{
			inSock->close();
		}
		else
		{
			if(outSock)
			{
				if(outSock->state() == WebSocket::Connecting)
				{
					delete outSock;
					outSock = 0;

					inSock->close();
				}
				else if(outSock->state() == WebSocket::Connected)
				{
					outSock->close(inSock->peerCloseCode());
				}
			}
		}
	}

	void in_closed()
	{
		int code = inSock->peerCloseCode();
		cleanupInSock();

		if(!detached && outSock && outSock->state() != WebSocket::Closing)
			outSock->close(code);

		tryFinish();
	}

	void in_error()
	{
		cleanupInSock();

		if(!detached)
		{
			delete outSock;
			outSock = 0;
		}

		tryFinish();
	}

	void out_connected()
	{
		log_debug("wsproxysession: %p connected", q);

		state = Connected;

		HttpHeaders headers = outSock->responseHeaders();

		// don't proxy extensions, as we may not know how to handle them
		QList<QByteArray> wsExtensions = headers.takeAll("Sec-WebSocket-Extensions");

		HttpExtension grip = getExtension(wsExtensions, "grip");
		if(!grip.isNull() || !subChannel.isEmpty())
		{
			if(!grip.isNull())
			{
				if(!passToUpstream)
				{
					if(grip.params.contains("message-prefix"))
						messagePrefix = grip.params.value("message-prefix");
					else
						messagePrefix = "m:";

					acceptGripMessages = true;
					log_debug("wsproxysession: %p grip enabled, message-prefix=[%s]", q, messagePrefix.data());
				}
				else
				{
					// tell upstream to do the grip stuff
					headers += HttpHeader("Sec-WebSocket-Extensions", getExtensionRaw(wsExtensions, "grip"));
				}
			}

			if(wsControlManager)
			{
				wsControl = wsControlManager->createSession(publicCid);
				connect(wsControl, &WsControlSession::sendEventReceived, this, &Private::wsControl_sendEventReceived);
				connect(wsControl, &WsControlSession::keepAliveSetupEventReceived, this, &Private::wsControl_keepAliveSetupEventReceived);
				connect(wsControl, &WsControlSession::closeEventReceived, this, &Private::wsControl_closeEventReceived);
				connect(wsControl, &WsControlSession::detachEventReceived, this, &Private::wsControl_detachEventReceived);
				connect(wsControl, &WsControlSession::cancelEventReceived, this, &Private::wsControl_cancelEventReceived);
				connect(wsControl, &WsControlSession::error, this, &Private::wsControl_error);
				wsControl->start(routeId, channelPrefix, inSock->requestUri());

				if(!subChannel.isEmpty())
				{
					log_debug("wsproxysession: %p implicit subscription to [%s]", q, qPrintable(subChannel));

					QVariantMap msg;
					msg["type"] = "subscribe";
					msg["channel"] = subChannel;

					QByteArray msgJson = QJsonDocument(QJsonObject::fromVariantMap(msg)).toJson(QJsonDocument::Compact);
					wsControl->sendGripMessage(msgJson);
				}
			}
		}

		inSock->respondSuccess(outSock->responseReason(), headers);

		logConnection(true, 101, 0);

		// send any pending frames
		tryReadIn();
	}

	void out_readyRead()
	{
		tryReadOut();
	}

	void out_framesWritten(int count, int contentBytes)
	{
		Q_UNUSED(count);

		outPendingBytes -= contentBytes;

		if(!detached && inSock)
			tryReadIn();
	}

	void out_peerClosed()
	{
		if(!detached && inSock && inSock->state() != WebSocket::Closing)
			inSock->close(outSock->peerCloseCode());
	}

	void out_closed()
	{
		int code = outSock->peerCloseCode();
		delete outSock;
		outSock = 0;

		if(!detached && inSock && inSock->state() != WebSocket::Closing)
			inSock->close(code);

		tryFinish();
	}

	void out_error()
	{
		WebSocket::ErrorCondition e = outSock->errorCondition();
		log_debug("wsproxysession: %p target error state=%d, condition=%d", q, (int)state, (int)e);

		if(detached)
		{
			delete outSock;
			outSock = 0;

			tryFinish();
			return;
		}

		if(state == Connecting)
		{
			bool tryAgain = false;

			switch(e)
			{
				case WebSocket::ErrorConnect:
				case WebSocket::ErrorConnectTimeout:
				case WebSocket::ErrorTls:
					tryAgain = true;
					break;
				case WebSocket::ErrorRejected:
					reject(true, outSock->responseCode(), outSock->responseReason(), outSock->responseHeaders(), outSock->responseBody());
					break;
				default:
					reject(true, 502, "Bad Gateway", "Error while proxying to origin.");
					break;
			}

			delete outSock;
			outSock = 0;

			if(tryAgain)
				tryNextTarget();
		}
		else
		{
			cleanupInSock();

			delete outSock;
			outSock = 0;

			tryFinish();
		}
	}

	void wsControl_sendEventReceived(WebSocket::Frame::Type type, const QByteArray &message, bool queue)
	{
		// this method accepts a full message, which must be typed
		if(type == WebSocket::Frame::Continuation)
			return;

		// if we have no socket to write to, say the data was written anyway.
		//   this is not quite correct but better than leaving the send event
		//   dangling
		if(!inSock)
		{
			wsControl->sendEventWritten();
			return;
		}

		// if queue == false, drop if we can't send right now
		if(!queue && (!inSock->canWrite() || outReadInProgress != -1))
		{
			// if drop is allowed, drop is success :)
			wsControl->sendEventWritten();
			return;
		}

		// split into frames to avoid credits issue
		QList<WebSocket::Frame> frames;

		if(message.size() > 0)
		{
			for(int n = 0; n < message.size(); n += FRAME_SIZE_MAX)
			{
				WebSocket::Frame::Type ftype;
				if(n == 0)
					ftype = type;
				else
					ftype = WebSocket::Frame::Continuation;

				QByteArray data = message.mid(n, FRAME_SIZE_MAX);
				bool more = (n + FRAME_SIZE_MAX < message.size());

				frames += WebSocket::Frame(ftype, data, more);
			}
		}
		else
		{
			frames += WebSocket::Frame(type, QByteArray(), false);
		}

		for(int n = 0; n < frames.count(); ++n)
		{
			bool fromSendEvent = (n + 1 >= frames.count());

			if(outReadInProgress != -1)
			{
				queuedInFrames += QueuedFrame(frames[n], fromSendEvent);
			}
			else
			{
				writeInFrame(frames[n], fromSendEvent);
			}
		}

		restartKeepAlive();
	}

	void wsControl_keepAliveSetupEventReceived(bool enable, int timeout)
	{
		if(enable)
		{
			if(!keepAliveTimer)
			{
				keepAliveTimer = new QTimer(this);
				connect(keepAliveTimer, &QTimer::timeout, this, &Private::keepAliveTimer_timeout);
				keepAliveTimer->setSingleShot(true);
			}

			keepAliveTimer->setInterval(timeout * 1000);
			keepAliveTimer->start();
		}
		else
		{
			cleanupKeepAliveTimer();
		}
	}

	void wsControl_closeEventReceived(int code)
	{
		if(!detached && outSock && outSock->state() != WebSocket::Closing)
			outSock->close();

		if(inSock && inSock->state() != WebSocket::Closing)
			inSock->close(code);
	}

	void wsControl_detachEventReceived()
	{
		// if already detached, do nothing
		if(detached)
			return;

		detached = true;

		if(outSock && outSock->state() != WebSocket::Closing)
			outSock->close();
	}

	void wsControl_cancelEventReceived()
	{
		if(outSock)
		{
			delete outSock;
			outSock = 0;
		}

		cleanupInSock();

		tryFinish();
	}

	void wsControl_error()
	{
		log_debug("wsproxysession: %p wscontrol session error", q);
		wsControl_cancelEventReceived();
	}

	void keepAliveTimer_timeout()
	{
		wsControl->sendNeedKeepAlive();
	}
};

WsProxySession::WsProxySession(ZRoutes *zroutes, ConnectionManager *connectionManager, const LogUtil::Config &logConfig, StatsManager *statsManager, WsControlManager *wsControlManager, QObject *parent) :
	QObject(parent)
{
	d = new Private(this, zroutes, connectionManager, logConfig, statsManager, wsControlManager);
}

WsProxySession::~WsProxySession()
{
	delete d;
}

QHostAddress WsProxySession::logicalClientAddress() const
{
	return d->logicalClientAddress;
}

QByteArray WsProxySession::routeId() const
{
	return d->routeId;
}

QByteArray WsProxySession::cid() const
{
	return d->publicCid;
}

WebSocket *WsProxySession::inSocket() const
{
	return d->inSock;
}

WebSocket *WsProxySession::outSocket() const
{
	return d->outSock;
}

void WsProxySession::setDefaultSigKey(const QByteArray &iss, const QByteArray &key)
{
	d->defaultSigIss = iss;
	d->defaultSigKey = key;
}

void WsProxySession::setDefaultUpstreamKey(const QByteArray &key)
{
	d->defaultUpstreamKey = key;
}

void WsProxySession::setAcceptXForwardedProtocol(bool enabled)
{
	d->acceptXForwardedProtocol = enabled;
}

void WsProxySession::setUseXForwardedProtocol(bool enabled)
{
	d->useXForwardedProtocol = enabled;
}

void WsProxySession::setXffRules(const XffRule &untrusted, const XffRule &trusted)
{
	d->xffRule = untrusted;
	d->xffTrustedRule = trusted;
}

void WsProxySession::setOrigHeadersNeedMark(const QList<QByteArray> &names)
{
	d->origHeadersNeedMark = names;
}

void WsProxySession::start(WebSocket *sock, const QByteArray &publicCid, const DomainMap::Entry &route)
{
	d->start(sock, publicCid, route);
}

#include "wsproxysession.moc"
