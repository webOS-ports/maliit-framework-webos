/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2026 Herman van Hazendonk <github.com@herrie.org>
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

// MAttributeExtensionId is the key that decides whether an attribute
// extension write from one client can reach another client's extension. Its
// validity rule and its identity semantics are therefore a security boundary,
// not just bookkeeping - and it is used as a QHash key, so hash and equality
// have to agree.

#include "mattributeextensionid.h"

#include <QHash>
#include <QSet>
#include <QtTest>

class Ut_MAttributeExtensionId : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testDefaultIsInvalid();
    void testValidity_data();
    void testValidity();
    void testStandardIdIsNotValid();
    void testEquality();
    void testInequalityOnService();
    void testInequalityOnId();
    void testUsableAsHashKey();
    void testDistinctClientsDoNotCollide();
    void testAccessors();
};

void Ut_MAttributeExtensionId::testDefaultIsInvalid()
{
    const MAttributeExtensionId id;

    QVERIFY(!id.isValid());
    QVERIFY(id.service().isEmpty());
}

void Ut_MAttributeExtensionId::testValidity_data()
{
    QTest::addColumn<int>("id");
    QTest::addColumn<QString>("service");
    QTest::addColumn<bool>("valid");

    QTest::newRow("normal")            << 1  << QStringLiteral("42") << true;
    QTest::newRow("zero id")           << 0  << QStringLiteral("42") << true;
    QTest::newRow("negative id")       << -1 << QStringLiteral("42") << false;
    QTest::newRow("very negative id")  << -99 << QStringLiteral("42") << false;
    QTest::newRow("empty service")     << 1  << QString()            << false;
    QTest::newRow("both bad")          << -1 << QString()            << false;
}

void Ut_MAttributeExtensionId::testValidity()
{
    QFETCH(int, id);
    QFETCH(QString, service);
    QFETCH(bool, valid);

    QCOMPARE(MAttributeExtensionId(id, service).isValid(), valid);
}

void Ut_MAttributeExtensionId::testStandardIdIsNotValid()
{
    // The standard id is a sentinel, not a registrable extension: it carries a
    // negative id and no service, so it must not pass isValid() and be treated
    // as a client's own extension.
    const MAttributeExtensionId standard = MAttributeExtensionId::standardAttributeExtensionId();

    QVERIFY(!standard.isValid());
    QVERIFY(standard != MAttributeExtensionId());
}

void Ut_MAttributeExtensionId::testEquality()
{
    const MAttributeExtensionId a(7, QStringLiteral("100"));
    const MAttributeExtensionId b(7, QStringLiteral("100"));

    QVERIFY(a == b);
    QVERIFY(!(a != b));
}

void Ut_MAttributeExtensionId::testInequalityOnService()
{
    // Same local extension id from two different clients must never compare
    // equal; that is what keeps one client out of another's extension.
    const MAttributeExtensionId a(7, QStringLiteral("100"));
    const MAttributeExtensionId b(7, QStringLiteral("101"));

    QVERIFY(a != b);
    QVERIFY(!(a == b));
}

void Ut_MAttributeExtensionId::testInequalityOnId()
{
    const MAttributeExtensionId a(7, QStringLiteral("100"));
    const MAttributeExtensionId b(8, QStringLiteral("100"));

    QVERIFY(a != b);
}

void Ut_MAttributeExtensionId::testUsableAsHashKey()
{
    QHash<MAttributeExtensionId, QString> hash;

    hash.insert(MAttributeExtensionId(1, QStringLiteral("10")), QStringLiteral("first"));
    hash.insert(MAttributeExtensionId(2, QStringLiteral("10")), QStringLiteral("second"));

    QCOMPARE(hash.size(), 2);
    QCOMPARE(hash.value(MAttributeExtensionId(1, QStringLiteral("10"))), QStringLiteral("first"));
    QCOMPARE(hash.value(MAttributeExtensionId(2, QStringLiteral("10"))), QStringLiteral("second"));

    // Re-inserting an equal key replaces rather than duplicates, which is only
    // true if qHash() agrees with operator==.
    hash.insert(MAttributeExtensionId(1, QStringLiteral("10")), QStringLiteral("replaced"));
    QCOMPARE(hash.size(), 2);
    QCOMPARE(hash.value(MAttributeExtensionId(1, QStringLiteral("10"))), QStringLiteral("replaced"));
}

void Ut_MAttributeExtensionId::testDistinctClientsDoNotCollide()
{
    QSet<MAttributeExtensionId> ids;

    for (int client = 0; client < 16; ++client) {
        for (int extension = 0; extension < 16; ++extension) {
            ids.insert(MAttributeExtensionId(extension, QString::number(client)));
        }
    }

    QCOMPARE(ids.size(), 16 * 16);
}

void Ut_MAttributeExtensionId::testAccessors()
{
    const MAttributeExtensionId id(13, QStringLiteral("service-name"));

    QCOMPARE(id.id(), 13);
    QCOMPARE(id.service(), QStringLiteral("service-name"));
}

QTEST_GUILESS_MAIN(Ut_MAttributeExtensionId)
#include "ut_mattributeextensionid.moc"
