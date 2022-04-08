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

#include "requeststate.h"

RequestState RequestState::fromVariant(const QVariant &in)
{
	if(in.type() != QVariant::Hash)
		return RequestState();

	QVariantHash r = in.toHash();
	RequestState rs;

	if(!r.contains("rid") || r["rid"].type() != QVariant::Hash)
		return RequestState();

	QVariantHash vrid = r["rid"].toHash();

	if(!vrid.contains("sender") || vrid["sender"].type() != QVariant::ByteArray)
		return RequestState();

	if(!vrid.contains("id") || vrid["id"].type() != QVariant::ByteArray)
		return RequestState();

	rs.rid = ZhttpRequest::Rid(vrid["sender"].toByteArray(), vrid["id"].toByteArray());

	if(!r.contains("in-seq") || !r["in-seq"].canConvert(QVariant::Int))
		return RequestState();

	rs.inSeq = r["in-seq"].toInt();

	if(!r.contains("out-seq") || !r["out-seq"].canConvert(QVariant::Int))
		return RequestState();

	rs.outSeq = r["out-seq"].toInt();

	if(!r.contains("out-credits") || !r["out-credits"].canConvert(QVariant::Int))
		return RequestState();

	rs.outCredits = r["out-credits"].toInt();

	if(r.contains("peer-address"))
	{
		if(r["peer-address"].type() != QVariant::ByteArray)
			return RequestState();

		if(!rs.peerAddress.setAddress(QString::fromUtf8(r["peer-address"].toByteArray())))
			return RequestState();
	}

	if(r.contains("https"))
	{
		if(r["https"].type() != QVariant::Bool)
			return RequestState();

		rs.isHttps = r["https"].toBool();
	}

	if(r.contains("auto-cross-origin"))
	{
		if(r["auto-cross-origin"].type() != QVariant::Bool)
			return RequestState();

		rs.autoCrossOrigin = r["auto-cross-origin"].toBool();
	}

	if(r.contains("jsonp-callback"))
	{
		if(r["jsonp-callback"].type() != QVariant::ByteArray)
			return RequestState();

		rs.jsonpCallback = r["jsonp-callback"].toByteArray();
	}

	if(r.contains("jsonp-extended-response"))
	{
		if(r["jsonp-extended-response"].type() != QVariant::Bool)
			return RequestState();

		rs.jsonpExtendedResponse = r["jsonp-extended-response"].toBool();
	}

	if(r.contains("user-data"))
	{
		rs.userData = r["user-data"];
	}

	return rs;
}
