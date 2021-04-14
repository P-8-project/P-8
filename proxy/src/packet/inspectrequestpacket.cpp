/*
 * Copyright (C) 2012 Fan Out Networks, Inc.
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

#include "inspectrequestpacket.h"

#include "tnetstring.h"

InspectRequestPacket::InspectRequestPacket() :
	https(false)
{
}

QVariant InspectRequestPacket::toVariant() const
{
	QVariantHash obj;
	obj["id"] = id;
	obj["method"] = method.toLatin1();
	obj["path"] = path;

	QVariantList vheaders;
	foreach(const HttpHeader &h, headers)
	{
		QVariantList vheader;
		vheader += h.first;
		vheader += h.second;
		vheaders += QVariant(vheader);
	}

	obj["headers"] = vheaders;

	if(https)
		obj["https"] = true;

	return obj;
}
