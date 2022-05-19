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

#ifndef ACCEPTDATA_H
#define ACCEPTDATA_H

#include <QList>
#include "httpheaders.h"
#include "packet/httprequestdata.h"
#include "packet/httpresponsedata.h"
#include "inspectdata.h"
#include "zhttprequest.h"

class AcceptData
{
public:
	class Request
	{
	public:
		ZhttpRequest::Rid rid;
		bool https;
		QHostAddress peerAddress;
		bool autoCrossOrigin;
		QByteArray jsonpCallback;
		bool jsonpExtendedResponse;

		// zhttp
		int responseCode;
		int inSeq;
		int outSeq;
		int outCredits;
		QVariant userData;

		Request() :
			https(false),
			autoCrossOrigin(false),
			jsonpExtendedResponse(false),
			responseCode(-1),
			inSeq(-1),
			outSeq(-1),
			outCredits(-1)
		{
		}
	};

	QList<Request> requests;
	HttpRequestData requestData;

	bool haveInspectData;
	InspectData inspectData;

	bool haveResponse;
	HttpResponseData response;

	QByteArray route;
	QByteArray channelPrefix;
	bool useSession;
	bool responseSent;

	AcceptData() :
		haveInspectData(false),
		haveResponse(false),
		useSession(false),
		responseSent(false)
	{
	}
};

#endif
