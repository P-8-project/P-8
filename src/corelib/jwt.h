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

#ifndef JWT_H
#define JWT_H

#include <QByteArray>
#include <QVariant>
#include "rust/jwt.h"

class QString;

namespace Jwt {

enum KeyType {
	Secret = JWT_KEYTYPE_SECRET,
	Ec = JWT_KEYTYPE_EC,
	Rsa = JWT_KEYTYPE_RSA,
};

enum Algorithm
{
	HS256 = JWT_ALGORITHM_HS256,
	ES256 = JWT_ALGORITHM_ES256,
	RS256 = JWT_ALGORITHM_RS256,
};

class EncodingKey
{
public:
	~EncodingKey();

	bool isNull() const { return (bool)(!raw_); }
	KeyType type() const { return type_; }

	const void *raw() const { return raw_; }

	static EncodingKey fromSecret(const QByteArray &key);
	static EncodingKey fromPem(const QByteArray &key);
	static EncodingKey fromPemFile(const QString &fileName);

private:
	void *raw_;
	KeyType type_;

	EncodingKey() :
		raw_(0),
		type_((KeyType)-1)
	{
	}

	static EncodingKey fromInternal(JwtEncodingKey key);
};

class DecodingKey
{
public:
	~DecodingKey();

	bool isNull() const { return (bool)(!raw_); }
	KeyType type() const { return type_; }

	const void *raw() const { return raw_; }

	static DecodingKey fromSecret(const QByteArray &key);
	static DecodingKey fromPem(const QByteArray &key);
	static DecodingKey fromPemFile(const QString &fileName);

private:
	void *raw_;
	KeyType type_;

	DecodingKey() :
		raw_(0),
		type_((KeyType)-1)
	{
	}

	static DecodingKey fromInternal(JwtDecodingKey key);
};

// returns token, null on error
QByteArray encodeWithAlgorithm(Algorithm alg, const QByteArray &claim, const EncodingKey &key);

// returns claim, null on error
QByteArray decodeWithAlgorithm(Algorithm alg, const QByteArray &token, const DecodingKey &key);

QByteArray encode(const QVariant &claim, const QByteArray &key);
QVariant decode(const QByteArray &token, const QByteArray &key);

}

#endif
