/*
 * Copyright (C) 2017 Fanout, Inc.
 *
 * This file is part of P-8.
 *
 * $FANOUT_BEGIN_LICENSE:AGPL$
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
 *
 * Alternatively, P-8 may be used under the terms of a commercial license,
 * where the commercial license agreement is provided with the software or
 * contained in a written agreement between you and Fanout. For further
 * information use the contact form at <https://fanout.io/enterprise/>.
 *
 * $FANOUT_END_LICENSE$
 */

#include "refreshworker.h"

#include "zrpcrequest.h"
#include "controlrequest.h"
#include "statsmanager.h"

RefreshWorker::RefreshWorker(ZrpcRequest *req, ZrpcManager *proxyControlClient, QObject *parent) :
	Deferred(parent),
	req_(req)
{
	req_->setParent(this);

	QVariantHash args = req_->args();

	if(!args.contains("cid") || args["cid"].type() != QVariant::ByteArray)
	{
		respondError("bad-request");
		return;
	}

	QByteArray cid = args["cid"].toByteArray();

	Deferred *d = ControlRequest::refresh(proxyControlClient, cid, this);
	connect(d, &Deferred::finished, this, &RefreshWorker::proxyRefresh_finished);
}

void RefreshWorker::respondError(const QByteArray &condition)
{
	req_->respondError(condition);
	setFinished(true);
}

void RefreshWorker::proxyRefresh_finished(const DeferredResult &result)
{
	if(result.success)
	{
		req_->respond();
		setFinished(true);
	}
	else
	{
		respondError(result.value.toByteArray());
	}
}
