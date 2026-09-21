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

// MAttributeExtensionManager::setExtendedAttribute() calls
// createKeyOverride() and then keyOverride() for the same id, and until
// recently assumed the second call could not come back null - an assumption
// that only held in a debug build, where Q_ASSERT still existed. These tests
// pin down what MKeyOverrideData actually promises for that pair.

#include <maliit/plugins/keyoverridedata.h>
#include <maliit/plugins/keyoverride.h>

#include <QtTest>

class Ut_KeyOverrideData : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testEmpty();
    void testCreateReportsNewness();
    void testCreateThenFetch();
    void testFetchWithoutCreateIsNull();
    void testEmptyKeyId();
    void testOverridesAreSortedByKeyId();
    void testSharedPointerIdentity();
    void testKeyOverrideProperties();
    void testKeyOverrideDefaults();
};

void Ut_KeyOverrideData::testEmpty()
{
    MKeyOverrideData data;

    QVERIFY(data.keyOverrides().isEmpty());
    QVERIFY(data.keyOverride(QStringLiteral("missing")).isNull());
}

void Ut_KeyOverrideData::testCreateReportsNewness()
{
    MKeyOverrideData data;

    // The return value is what drives the keyOverrideCreated() signal, so the
    // "already existed" case has to be distinguishable.
    QVERIFY(data.createKeyOverride(QStringLiteral("a")));
    QVERIFY(!data.createKeyOverride(QStringLiteral("a")));
    QVERIFY(data.createKeyOverride(QStringLiteral("b")));
}

void Ut_KeyOverrideData::testCreateThenFetch()
{
    MKeyOverrideData data;

    data.createKeyOverride(QStringLiteral("enter"));

    const QSharedPointer<MKeyOverride> override = data.keyOverride(QStringLiteral("enter"));
    QVERIFY(!override.isNull());
    QCOMPARE(override->keyId(), QStringLiteral("enter"));
}

void Ut_KeyOverrideData::testFetchWithoutCreateIsNull()
{
    MKeyOverrideData data;

    data.createKeyOverride(QStringLiteral("enter"));

    // A different id was never created, so the caller gets null - which is
    // precisely the case the manager now checks for instead of asserting.
    QVERIFY(data.keyOverride(QStringLiteral("space")).isNull());
}

void Ut_KeyOverrideData::testEmptyKeyId()
{
    MKeyOverrideData data;

    // An empty target item reaches here from client input; whatever the
    // answer is, create and fetch must agree on it.
    const bool created = data.createKeyOverride(QString());
    const QSharedPointer<MKeyOverride> override = data.keyOverride(QString());

    QCOMPARE(created, !override.isNull());
}

void Ut_KeyOverrideData::testOverridesAreSortedByKeyId()
{
    MKeyOverrideData data;

    data.createKeyOverride(QStringLiteral("c"));
    data.createKeyOverride(QStringLiteral("a"));
    data.createKeyOverride(QStringLiteral("b"));

    const QList<QSharedPointer<MKeyOverride> > overrides = data.keyOverrides();
    QCOMPARE(overrides.size(), 3);
    QCOMPARE(overrides.at(0)->keyId(), QStringLiteral("a"));
    QCOMPARE(overrides.at(1)->keyId(), QStringLiteral("b"));
    QCOMPARE(overrides.at(2)->keyId(), QStringLiteral("c"));
}

void Ut_KeyOverrideData::testSharedPointerIdentity()
{
    MKeyOverrideData data;

    data.createKeyOverride(QStringLiteral("key"));

    // Two fetches must hand back the same object, or an attribute written
    // through one would be invisible through the other.
    const QSharedPointer<MKeyOverride> first = data.keyOverride(QStringLiteral("key"));
    const QSharedPointer<MKeyOverride> second = data.keyOverride(QStringLiteral("key"));

    QCOMPARE(first.data(), second.data());
}

void Ut_KeyOverrideData::testKeyOverrideProperties()
{
    MKeyOverrideData data;
    data.createKeyOverride(QStringLiteral("key"));

    const QSharedPointer<MKeyOverride> override = data.keyOverride(QStringLiteral("key"));
    QVERIFY(!override.isNull());

    // The manager writes attributes through the property system, using a name
    // that came off the bus.
    QVERIFY(override->setProperty("label", QVariant(QStringLiteral("OK"))));
    QCOMPARE(override->label(), QStringLiteral("OK"));

    QVERIFY(override->setProperty("enabled", QVariant(false)));
    QCOMPARE(override->enabled(), false);

    QVERIFY(override->setProperty("highlighted", QVariant(true)));
    QCOMPARE(override->highlighted(), true);
}

void Ut_KeyOverrideData::testKeyOverrideDefaults()
{
    MKeyOverride override(QStringLiteral("id"));

    QCOMPARE(override.keyId(), QStringLiteral("id"));
    QVERIFY(override.enabled());
}

QTEST_GUILESS_MAIN(Ut_KeyOverrideData)
#include "ut_keyoverridedata.moc"
