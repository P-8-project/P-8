/*
 * Copyright (C) 2012-2013 Fanout, Inc.
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

#include "proxysession.h"

#include <assert.h>
#include <QSet>
#include <QPointer>
#include <QUrl>
#include <QHostAddress>
#include "packet/httprequestdata.h"
#include "packet/httpresponsedata.h"
#include "bufferlist.h"
#include "log.h"
#include "inspectdata.h"
#include "acceptdata.h"
#include "zhttpmanager.h"
#include "zhttprequest.h"
#include "domainmap.h"
#include "xffrule.h"
#include "requestsession.h"
#include "proxyutil.h"

#define MAX_ACCEPT_REQUEST_BODY 100000
#define MAX_ACCEPT_RESPONSE_BODY 100000

#define MAX_INITIAL_BUFFER 100000
#define MAX_STREAM_BUFFER 100000

class ProxySession::Private : public QObject
{
	Q_OBJECT

public:
	enum State
	{
		Stopped,
		Requesting,
		Accepting,
		Responding
	};

	class SessionItem
	{
	public:
		enum State
		{
			WaitingForResponse,
			Responding,
			Responded,
			Errored,
			Pausing,
			Paused
		};

		RequestSession *rs;
		State state;
		int bytesToWrite;

		SessionItem() :
			rs(0),
			state(WaitingForResponse),
			bytesToWrite(0)
		{
		}
	};

	ProxySession *q;
	State state;
	ZhttpManager *zhttpManager;
	DomainMap *domainMap;
	ZhttpRequest *inRequest;
	bool isHttps;
	QByteArray routeId;
	QByteArray channelPrefix;
	QList<DomainMap::Target> targets;
	ZhttpRequest *zhttpRequest;
	bool addAllowed;
	bool haveInspectData;
	InspectData idata;
	QSet<QByteArray> acceptTypes;
	QSet<SessionItem*> sessionItems;
	HttpRequestData requestData;
	HttpResponseData responseData;
	BufferList requestBody;
	BufferList responseBody;
	QHash<RequestSession*, SessionItem*> sessionItemsBySession;
	QByteArray initialRequestBody;
	int requestBytesToWrite;
	int total;
	bool buffering;
	QByteArray defaultSigIss;
	QByteArray defaultSigKey;
	QByteArray defaultUpstreamKey;
	bool passToUpstream;
	bool useXForwardedProtocol;
	XffRule xffRule;
	XffRule xffTrustedRule;
	QList<QByteArray> origHeadersNeedMark;

	Private(ProxySession *_q, ZhttpManager *_zhttpManager, DomainMap *_domainMap) :
		QObject(_q),
		q(_q),
		state(Stopped),
		zhttpManager(_zhttpManager),
		domainMap(_domainMap),
		inRequest(0),
		isHttps(false),
		zhttpRequest(0),
		addAllowed(true),
		haveInspectData(false),
		requestBytesToWrite(0),
		total(0),
		passToUpstream(false),
		useXForwardedProtocol(false)
	{
		acceptTypes += "application/grip-instruct";
	}

	~Private()
	{
		cleanup();
	}

	void cleanup()
	{
		foreach(SessionItem *si, sessionItems)
			delete si->rs;

		sessionItems.clear();
		sessionItemsBySession.clear();
	}

	void add(RequestSession *rs)
	{
		assert(addAllowed);

		SessionItem *si = new SessionItem;
		si->rs = rs;
		si->rs->setParent(this);
		sessionItems += si;
		sessionItemsBySession.insert(rs, si);
		connect(rs, SIGNAL(bytesWritten(int)), SLOT(rs_bytesWritten(int)));
		connect(rs, SIGNAL(errorResponding()), SLOT(rs_errorResponding()));
		connect(rs, SIGNAL(finished()), SLOT(rs_finished()));
		connect(rs, SIGNAL(paused()), SLOT(rs_paused()));

		if(state == Stopped)
		{
			QString host = rs->requestData().uri.host();
			isHttps = rs->isHttps();

			requestData = rs->requestData();
			requestBody += requestData.body;
			requestData.body.clear();

			DomainMap::Entry entry = domainMap->entry(DomainMap::Http, isHttps, host, requestData.uri.encodedPath());
			if(entry.isNull())
			{
				log_warning("proxysession: %p %s has 0 routes", q, qPrintable(host));
				rejectAll(502, "Bad Gateway", QString("No route for host: %1").arg(host));
				return;
			}

			if(!entry.asHost.isEmpty())
				requestData.uri.setHost(entry.asHost);

			if(entry.pathRemove > 0)
			{
				QByteArray path = requestData.uri.encodedPath();
				path = path.mid(entry.pathRemove);
				requestData.uri.setEncodedPath(path);
			}

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

			routeId = entry.id;
			channelPrefix = entry.prefix;
			targets = entry.targets;

			log_debug("proxysession: %p %s has %d routes", q, qPrintable(host), targets.count());

			bool trustedClient = ProxyUtil::manipulateRequestHeaders("wsproxysession", q, &requestData, defaultUpstreamKey, entry, sigIss, sigKey, useXForwardedProtocol, xffTrustedRule, xffRule, origHeadersNeedMark, rs->peerAddress());

			if(trustedClient)
				passToUpstream = true;

			state = Requesting;
			buffering = true;

			if(!rs->isRetry())
			{
				inRequest = rs->request();
				connect(inRequest, SIGNAL(readyRead()), SLOT(inRequest_readyRead()));
				connect(inRequest, SIGNAL(error()), SLOT(inRequest_error()));

				requestBody += inRequest->readBody();
			}

			initialRequestBody = requestBody.toByteArray();

			if(requestBody.size() > MAX_ACCEPT_REQUEST_BODY)
			{
				requestBody.clear();
				buffering = false;
			}

			tryNextTarget();
		}
		else if(state == Requesting)
		{
			// nothing to do, just wait around until a response comes
		}
		else if(state == Responding)
		{
			// get the session caught up with where we're at

			si->state = SessionItem::Responding;
			rs->startResponse(responseData.code, responseData.reason, responseData.headers);

			if(!responseBody.isEmpty())
			{
				si->bytesToWrite += responseBody.size();
				rs->writeResponseBody(responseBody.toByteArray());
			}
		}
	}

	bool pendingWrites()
	{
		foreach(SessionItem *si, sessionItems)
		{
			if(si->bytesToWrite != -1 && si->bytesToWrite > 0)
				return true;
		}

		return false;
	}

	void tryNextTarget()
	{
		if(targets.isEmpty())
		{
			rejectAll(502, "Bad Gateway", "Error while proxying to origin.");
			return;
		}

		DomainMap::Target target = targets.takeFirst();

		QUrl uri = requestData.uri;
		if(target.ssl)
			uri.setScheme("https");
		else
			uri.setScheme("http");

		if(!target.host.isEmpty())
			uri.setHost(target.host);

		log_debug("proxysession: %p forwarding to %s:%d", q, qPrintable(target.connectHost), target.connectPort);

		zhttpRequest = zhttpManager->createRequest();
		zhttpRequest->setParent(this);
		connect(zhttpRequest, SIGNAL(readyRead()), SLOT(zhttpRequest_readyRead()));
		connect(zhttpRequest, SIGNAL(bytesWritten(int)), SLOT(zhttpRequest_bytesWritten(int)));
		connect(zhttpRequest, SIGNAL(error()), SLOT(zhttpRequest_error()));

		if(target.trusted)
			zhttpRequest->setIgnorePolicies(true);

		if(target.insecure)
			zhttpRequest->setIgnoreTlsErrors(true);

		zhttpRequest->setConnectHost(target.connectHost);
		zhttpRequest->setConnectPort(target.connectPort);

		zhttpRequest->start(requestData.method, uri, requestData.headers);

		if(!initialRequestBody.isEmpty())
		{
			requestBytesToWrite += initialRequestBody.size();
			zhttpRequest->writeBody(initialRequestBody);
		}

		if(!inRequest || inRequest->isInputFinished())
			zhttpRequest->endBody();
	}

	void tryRequestRead()
	{
		QByteArray buf = inRequest->readBody();
		if(buf.isEmpty())
			return;

		log_debug("proxysession: %p input chunk: %d", q, buf.size());

		if(buffering)
		{
			if(requestBody.size() + buf.size() > MAX_ACCEPT_REQUEST_BODY)
			{
				requestBody.clear();
				buffering = false;
			}
			else
				requestBody += buf;
		}

		requestBytesToWrite += buf.size();
		zhttpRequest->writeBody(buf);
	}

	void cannotAcceptAll()
	{
		foreach(SessionItem *si, sessionItems)
		{
			if(si->state != SessionItem::Errored)
			{
				assert(si->state == SessionItem::WaitingForResponse);

				si->state = SessionItem::Responded;
				si->bytesToWrite = -1;
				si->rs->respondCannotAccept();
			}
		}
	}

	void rejectAll(int code, const QString &reason, const QString &errorMessage)
	{
		foreach(SessionItem *si, sessionItems)
		{
			if(si->state != SessionItem::Errored)
			{
				assert(si->state == SessionItem::WaitingForResponse);

				si->state = SessionItem::Responded;
				si->bytesToWrite = -1;
				si->rs->respondError(code, reason, errorMessage);
			}
		}
	}

	void destroyAll()
	{
		// this method is only to be called when we are in Responding state
		assert(state == Responding);

		foreach(SessionItem *si, sessionItems)
		{
			assert(si->state != SessionItem::WaitingForResponse);

			if(si->state == SessionItem::Responding)
			{
				si->state = SessionItem::Responded;
				si->bytesToWrite = -1;
				si->rs->endResponseBody();
			}
		}
	}

	// this method emits signals
	void tryResponseRead()
	{
		// if we're not buffering, then don't read (instead, sync to slowest
		//   receiver before reading again)
		if(!buffering && pendingWrites())
			return;

		QPointer<QObject> self = this;

		QByteArray buf = zhttpRequest->readBody(MAX_STREAM_BUFFER);
		if(!buf.isEmpty())
		{
			total += buf.size();
			log_debug("proxysession: %p recv=%d, total=%d", q, buf.size(), total);

			if(state == Accepting)
			{
				if(responseBody.size() + buf.size() > MAX_ACCEPT_RESPONSE_BODY)
				{
					rejectAll(502, "Bad Gateway", "GRIP instruct response too large.");
					return;
				}

				responseBody += buf;
			}
			else // Responding
			{
				bool wasAllowed = addAllowed;

				if(buffering)
				{
					if(responseBody.size() + buf.size() > MAX_INITIAL_BUFFER)
					{
						responseBody.clear();
						buffering = false;
						addAllowed = false;
					}
					else
						responseBody += buf;
				}

				log_debug("proxysession: %p writing %d to clients", q, buf.size());

				foreach(SessionItem *si, sessionItems)
				{
					assert(si->state != SessionItem::WaitingForResponse);

					if(si->state == SessionItem::Responding)
					{
						si->bytesToWrite += buf.size();
						si->rs->writeResponseBody(buf);
					}
				}

				if(wasAllowed && !addAllowed)
				{
					emit q->addNotAllowed();
					if(!self)
						return;
				}
			}
		}

		checkIncomingResponseFinished();
	}

	// this method emits signals
	void checkIncomingResponseFinished()
	{
		QPointer<QObject> self = this;

		if(zhttpRequest->isFinished() && zhttpRequest->bytesAvailable() == 0)
		{
			log_debug("proxysession: %p response from target finished", q);

			if(!buffering && pendingWrites())
			{
				log_debug("proxysession: %p still stuff left to write, though. we'll wait.", q);
				return;
			}

			delete zhttpRequest;
			zhttpRequest = 0;

			// once the entire response has been received, cut off any new adds
			if(addAllowed)
			{
				addAllowed = false;
				emit q->addNotAllowed();
				if(!self)
					return;
			}

			if(state == Accepting)
			{
				foreach(SessionItem *si, sessionItems)
				{
					si->state = SessionItem::Pausing;
					si->rs->pause();
				}
			}
			else // Responding
			{
				foreach(SessionItem *si, sessionItems)
				{
					assert(si->state != SessionItem::WaitingForResponse);

					if(si->state == SessionItem::Responding)
					{
						si->state = SessionItem::Responded;
						si->rs->endResponseBody();
					}
				}
			}
		}
	}

public slots:
	void inRequest_readyRead()
	{
		tryRequestRead();

		if(inRequest->isInputFinished())
			zhttpRequest->endBody();
	}

	void inRequest_error()
	{
		log_warning("proxysession: %p error reading request", q);

		rejectAll(500, "Internal Server Error", "Primary shared request failed.");
	}

	void zhttpRequest_readyRead()
	{
		log_debug("proxysession: %p data from target", q);

		if(state == Requesting)
		{
			responseData.code = zhttpRequest->responseCode();
			responseData.reason = zhttpRequest->responseReason();
			responseData.headers = zhttpRequest->responseHeaders();
			responseBody += zhttpRequest->readBody(MAX_INITIAL_BUFFER);

			total += responseBody.size();
			log_debug("proxysession: %p recv total: %d", q, total);

			QByteArray contentType = responseData.headers.get("Content-Type");
			int at = contentType.indexOf(';');
			if(at != -1)
				contentType = contentType.mid(0, at);

			if(!passToUpstream && acceptTypes.contains(contentType))
			{
				if(!buffering)
				{
					rejectAll(502, "Bad Gateway", "Request too large to accept GRIP instruct.");
					return;
				}

				state = Accepting;
			}
			else
			{
				state = Responding;

				// don't relay these headers. their meaning is handled by
				//   zurl and they only apply to the outgoing hop.
				responseData.headers.removeAll("Connection");
				responseData.headers.removeAll("Keep-Alive");
				responseData.headers.removeAll("Content-Encoding");
				responseData.headers.removeAll("Transfer-Encoding");

				if(!responseData.headers.contains("Content-Length") && !responseData.headers.contains("Transfer-Encoding"))
					responseData.headers += HttpHeader("Transfer-Encoding", "chunked");

				foreach(SessionItem *si, sessionItems)
				{
					si->state = SessionItem::Responding;
					si->rs->startResponse(responseData.code, responseData.reason, responseData.headers);

					if(!responseBody.isEmpty())
					{
						si->bytesToWrite += responseBody.size();
						si->rs->writeResponseBody(responseBody.toByteArray());
					}
				}
			}

			checkIncomingResponseFinished();
		}
		else
		{
			assert(state == Accepting || state == Responding);

			tryResponseRead();
		}
	}

	void zhttpRequest_bytesWritten(int count)
	{
		requestBytesToWrite -= count;
		assert(requestBytesToWrite >= 0);

		if(requestBytesToWrite == 0)
			tryRequestRead();
	}

	void zhttpRequest_error()
	{
		ZhttpRequest::ErrorCondition e = zhttpRequest->errorCondition();
		log_debug("proxysession: %p target error state=%d, condition=%d", q, (int)state, (int)e);

		if(state == Requesting || state == Accepting)
		{
			bool tryAgain = false;

			switch(e)
			{
				case ZhttpRequest::ErrorLengthRequired:
					rejectAll(411, "Length Required", "Must provide Content-Length header.");
					break;
				case ZhttpRequest::ErrorConnect:
				case ZhttpRequest::ErrorConnectTimeout:
				case ZhttpRequest::ErrorTls:
					// it should not be possible to get one of these errors while accepting
					assert(state == Requesting);
					tryAgain = true;
					break;
				default:
					rejectAll(502, "Bad Gateway", "Error while proxying to origin.");
					break;
			}

			if(tryAgain)
				tryNextTarget();
		}
		else if(state == Responding)
		{
			// if we're already responding, then we can't reply with an error
			destroyAll();
		}
	}

	void rs_bytesWritten(int count)
	{
		RequestSession *rs = (RequestSession *)sender();

		log_debug("proxysession: %p response bytes written id=%s: %d", q, rs->rid().second.data(), count);

		SessionItem *si = sessionItemsBySession.value(rs);
		assert(si);

		if(si->bytesToWrite != -1)
		{
			si->bytesToWrite -= count;
			assert(si->bytesToWrite >= 0);
		}

		if(zhttpRequest)
			tryResponseRead();
	}

	void rs_finished()
	{
		RequestSession *rs = (RequestSession *)sender();

		log_debug("proxysession: %p response finished id=%s", q, rs->rid().second.data());

		SessionItem *si = sessionItemsBySession.value(rs);
		assert(si);

		QPointer<QObject> self = this;
		emit q->requestSessionDestroyed(si->rs, false);
		if(!self)
			return;

		sessionItemsBySession.remove(rs);
		sessionItems.remove(si);
		delete rs;

		delete si;

		if(sessionItems.isEmpty())
		{
			log_debug("proxysession: %p finished by passthrough", q);
			emit q->finishedByPassthrough();
		}
	}

	void rs_paused()
	{
		RequestSession *rs = (RequestSession *)sender();

		log_debug("proxysession: %p response paused id=%s", q, rs->rid().second.data());

		SessionItem *si = sessionItemsBySession.value(rs);
		assert(si);

		assert(si->state == SessionItem::Pausing);
		si->state = SessionItem::Paused;

		bool allPaused = true;
		foreach(SessionItem *si, sessionItems)
		{
			if(si->state != SessionItem::Paused)
			{
				allPaused = false;
				break;
			}
		}

		if(allPaused)
		{
			AcceptData adata;

			QList<RequestSession*> toDestroy;

			foreach(SessionItem *si, sessionItems)
			{
				ZhttpRequest::ServerState ss = si->rs->request()->serverState();

				AcceptData::Request areq;
				areq.rid = si->rs->rid();
				areq.https = si->rs->isHttps();
				areq.peerAddress = si->rs->peerAddress();
				areq.autoCrossOrigin = si->rs->autoCrossOrigin();
				areq.jsonpCallback = si->rs->jsonpCallback();
				areq.inSeq = ss.inSeq;
				areq.outSeq = ss.outSeq;
				areq.outCredits = ss.outCredits;
				areq.userData = ss.userData;
				adata.requests += areq;

				toDestroy += si->rs;
			}

			sessionItems.clear();
			sessionItemsBySession.clear();

			QPointer<QObject> self = this;
			foreach(RequestSession *rs, toDestroy)
			{
				if(self) // <-- weird!
					emit q->requestSessionDestroyed(rs, true);
				delete rs;
			}
			if(!self)
				return;

			adata.requestData = requestData;
			adata.requestData.body = requestBody.take();

			adata.haveResponse = true;
			adata.response = responseData;
			adata.response.body = responseBody.take();

			adata.route = routeId;
			adata.channelPrefix = channelPrefix;

			log_debug("proxysession: %p finished for accept", q);
			cleanup();
			emit q->finishedForAccept(adata);
		}
	}

	void rs_errorResponding()
	{
		RequestSession *rs = (RequestSession *)sender();

		log_debug("proxysession: %p response error id=%s", q, rs->rid().second.data());

		SessionItem *si = sessionItemsBySession.value(rs);
		assert(si);

		assert(si->state != SessionItem::Errored);

		// flag that we should stop attempting to respond
		si->state = SessionItem::Errored;
		si->bytesToWrite = -1;

		// don't destroy the RequestSession here. a finished signal will arrive next.
	}
};

ProxySession::ProxySession(ZhttpManager *zhttpManager, DomainMap *domainMap, QObject *parent) :
	QObject(parent)
{
	d = new Private(this, zhttpManager, domainMap);
}

ProxySession::~ProxySession()
{
	delete d;
}

QByteArray ProxySession::routeId() const
{
	return d->routeId;
}

void ProxySession::setDefaultSigKey(const QByteArray &iss, const QByteArray &key)
{
	d->defaultSigIss = iss;
	d->defaultSigKey = key;
}

void ProxySession::setDefaultUpstreamKey(const QByteArray &key)
{
	d->defaultUpstreamKey = key;
}

void ProxySession::setUseXForwardedProtocol(bool enabled)
{
	d->useXForwardedProtocol = enabled;
}

void ProxySession::setXffRules(const XffRule &untrusted, const XffRule &trusted)
{
	d->xffRule = untrusted;
	d->xffTrustedRule = trusted;
}

void ProxySession::setOrigHeadersNeedMark(const QList<QByteArray> &names)
{
	d->origHeadersNeedMark = names;
}

void ProxySession::setInspectData(const InspectData &idata)
{
	d->haveInspectData = true;
	d->idata = idata;
}

void ProxySession::add(RequestSession *rs)
{
	d->add(rs);
}

void ProxySession::cannotAccept()
{
	d->cannotAcceptAll();
}

#include "proxysession.moc"
