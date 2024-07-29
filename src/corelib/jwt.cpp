/*
 * Copyright (C) 2012-2022 Fanout, Inc.
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

#include "jwt.h"

#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace Jwt {

EncodingKey EncodingKey::fromInternal(JwtEncodingKey key)
{
	EncodingKey k;
	k.raw_ = key.key;
	k.type_ = (KeyType)key.type;
	return k;
}

EncodingKey::~EncodingKey()
{
	jwt_encoding_key_destroy(raw_);
}

EncodingKey EncodingKey::fromSecret(const QByteArray &key)
{
	return fromInternal(jwt_encoding_key_from_secret((const quint8 *)key.data(), key.size()));
}

EncodingKey EncodingKey::fromPem(const QByteArray &key)
{
	return fromInternal(jwt_encoding_key_from_pem((const quint8 *)key.data(), key.size()));
}

EncodingKey EncodingKey::fromPemFile(const QString &fileName)
{
	QFile f(fileName);
	if(!f.open(QFile::ReadOnly))
	{
		return EncodingKey();
	}

	return fromPem(f.readAll());
}

DecodingKey DecodingKey::fromInternal(JwtDecodingKey key)
{
	DecodingKey k;
	k.raw_ = key.key;
	k.type_ = (KeyType)key.type;
	return k;
}

DecodingKey::~DecodingKey()
{
	jwt_decoding_key_destroy(raw_);
}

DecodingKey DecodingKey::fromSecret(const QByteArray &key)
{
	return fromInternal(jwt_decoding_key_from_secret((const quint8 *)key.data(), key.size()));
}

DecodingKey DecodingKey::fromPem(const QByteArray &key)
{
	return fromInternal(jwt_decoding_key_from_pem((const quint8 *)key.data(), key.size()));
}

DecodingKey DecodingKey::fromPemFile(const QString &fileName)
{
	QFile f(fileName);
	if(!f.open(QFile::ReadOnly))
	{
		return DecodingKey();
	}

	return fromPem(f.readAll());
}

QByteArray encodeWithAlgorithm(Algorithm alg, const QByteArray &claim, const EncodingKey &key)
{
	char *token;

	if(jwt_encode((int)alg, (const char *)claim.data(), key.raw(), &token) != 0)
	{
		// error
		return QByteArray();
	}

	QByteArray out = QByteArray(token);
	jwt_str_destroy(token);

	return out;
}

QByteArray decodeWithAlgorithm(Algorithm alg, const QByteArray &token, const DecodingKey &key)
{
	char *claim;

	if(jwt_decode((int)alg, (const char *)token.data(), key.raw(), &claim) != 0)
	{
		// error
		return QByteArray();
	}

	QByteArray out = QByteArray(claim);
	jwt_str_destroy(claim);

	return out;
}

QByteArray encode(const QVariant &claim, const QByteArray &key)
{
	QByteArray claimJson = QJsonDocument(QJsonObject::fromVariantMap(claim.toMap())).toJson(QJsonDocument::Compact);
	if(claimJson.isNull())
		return QByteArray();

	return encodeWithAlgorithm(HS256, claimJson, EncodingKey::fromSecret(key));
}

QVariant decode(const QByteArray &token, const QByteArray &key)
{
	QByteArray claimJson = decodeWithAlgorithm(HS256, token, DecodingKey::fromSecret(key));
	if(claimJson.isEmpty())
		return QVariant();

	QJsonParseError error;
	QJsonDocument doc = QJsonDocument::fromJson(claimJson, &error);
	if(error.error != QJsonParseError::NoError || !doc.isObject())
		return QVariant();

	return doc.object().toVariantMap();
}

}
