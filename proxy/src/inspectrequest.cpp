/*
 * Copyright (C) 2012-2015 Fanout, Inc.
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

#include "inspectrequest.h"

#include "packet/httprequestdata.h"
#include "inspectdata.h"
#include "uuidutil.h"

static InspectData resultToData(const QVariant &in, bool *ok)
{
	InspectData out;

	if(in.type() != QVariant::Hash)
	{
		*ok = false;
		return InspectData();
	}

	QVariantHash obj = in.toHash();

	if(!obj.contains("no-proxy") || obj["no-proxy"].type() != QVariant::Bool)
	{
		*ok = false;
		return InspectData();
	}
	out.doProxy = !obj["no-proxy"].toBool();

	out.sharingKey.clear();
	if(obj.contains("sharing-key"))
	{
		if(obj["sharing-key"].type() != QVariant::ByteArray)
		{
			*ok = false;
			return InspectData();
		}

		out.sharingKey = obj["sharing-key"].toByteArray();
	}

	out.userData = obj["user-data"];

	*ok = true;
	return out;
}

class InspectRequest::Private : public QObject
{
	Q_OBJECT

public:
	InspectRequest *q;
	InspectData idata;

	Private(InspectRequest *_q) :
		QObject(_q),
		q(_q)
	{
	}
};

InspectRequest::InspectRequest(ZrpcManager *manager, QObject *parent) :
	ZrpcRequest(manager, parent)
{
	d = new Private(this);
}

InspectRequest::~InspectRequest()
{
	delete d;
}

InspectData InspectRequest::result() const
{
	return d->idata;
}

void InspectRequest::start(const HttpRequestData &hdata, bool truncated)
{
	QVariantHash args;

	args["method"] = hdata.method.toLatin1();
	args["uri"] = hdata.uri.toEncoded();

	QVariantList vheaders;
	foreach(const HttpHeader &h, hdata.headers)
	{
		QVariantList vheader;
		vheader += h.first;
		vheader += h.second;
		vheaders += QVariant(vheader);
	}

	args["headers"] = vheaders;
	args["body"] = hdata.body;

	if(truncated)
		args["truncated"] = true;

	ZrpcRequest::start("inspect", args);
}

void InspectRequest::onSuccess()
{
	bool ok;
	d->idata = resultToData(ZrpcRequest::result(), &ok);
	if(!ok)
	{
		setError(ErrorFormat);
		return;
	}
}

#include "inspectrequest.moc"
