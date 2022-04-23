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

#ifndef VARIANTUTIL_H
#define VARIANTUTIL_H

#include <QVariant>

namespace VariantUtil {

void setSuccess(bool *ok, QString *errorMessage);
void setError(bool *ok, QString *errorMessage, const QString &msg);

bool isKeyedObject(const QVariant &in);
QVariant createSameKeyedObject(const QVariant &in);
bool keyedObjectIsEmpty(const QVariant &in);
bool keyedObjectContains(const QVariant &in, const QString &name);
QVariant keyedObjectGetValue(const QVariant &in, const QString &name);
void keyedObjectInsert(QVariant *in, const QString &name, const QVariant &value);

QVariant getChild(const QVariant &in, const QString &parentName, const QString &childName, bool required, bool *ok = 0, QString *errorMessage = 0);
QVariant getKeyedObject(const QVariant &in, const QString &parentName, const QString &childName, bool required, bool *ok = 0, QString *errorMessage = 0);
QVariantList getList(const QVariant &in, const QString &parentName, const QString &childName, bool required, bool *ok = 0, QString *errorMessage = 0);
QString getString(const QVariant &in, bool *ok = 0);
QString getString(const QVariant &in, const QString &parentName, const QString &childName, bool required, bool *ok = 0, QString *errorMessage = 0);

// return true if item modified
bool convertToJsonStyleInPlace(QVariant *in);

QVariant convertToJsonStyle(const QVariant &in);

}

#endif
