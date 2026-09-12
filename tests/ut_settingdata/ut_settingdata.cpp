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

// validateSettingValue() is what stands between a plugin-settings write
// arriving over the bus and MImSettings::set(). Everything it is handed -
// the value and the domain/range attributes it is checked against - comes
// from a QVariant that crossed a process boundary, so the interesting cases
// are the mistyped and the missing ones, not the well-formed ones.

#include <maliit/settingdata.h>
#include <maliit/namespace.h>

#include <QtTest>

class Ut_SettingData : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testStringAccepted_data();
    void testStringAccepted();

    void testStringDomain();
    void testStringDomainRejectsOutsider();
    void testInvalidDomainIsNoConstraint();
    void testNonListDomainRejectsEverything();

    void testIntAccepted_data();
    void testIntAccepted();
    void testIntRejectsNonNumeric();

    void testIntRange_data();
    void testIntRange();
    void testIntRangeWithOnlyOneBound();
    void testNonNumericRangeBoundRejects();

    void testBool_data();
    void testBool();

    void testStringList();
    void testStringListDomain();

    void testIntList();
    void testIntListRejectsNonIntElement();
    void testIntListRange();

    void testEmptyAttributes();
    void testInvalidValue();
};

namespace {

QVariantMap domainOf(const QVariant &domain)
{
    QVariantMap attributes;
    attributes[Maliit::SettingEntryAttributes::valueDomain] = domain;
    return attributes;
}

QVariantMap rangeOf(const QVariant &min, const QVariant &max)
{
    QVariantMap attributes;
    if (min.isValid())
        attributes[Maliit::SettingEntryAttributes::valueRangeMin] = min;
    if (max.isValid())
        attributes[Maliit::SettingEntryAttributes::valueRangeMax] = max;
    return attributes;
}

} // namespace

void Ut_SettingData::testStringAccepted_data()
{
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<bool>("valid");

    QTest::newRow("string")       << QVariant(QStringLiteral("hello")) << true;
    QTest::newRow("empty string") << QVariant(QString())               << true;
    // QVariant converts these to strings, so the type check passes.
    QTest::newRow("int")          << QVariant(42)                      << true;
    QTest::newRow("bool")         << QVariant(true)                    << true;
}

void Ut_SettingData::testStringAccepted()
{
    QFETCH(QVariant, value);
    QFETCH(bool, valid);

    QCOMPARE(validateSettingValue(Maliit::StringType, QVariantMap(), value), valid);
}

void Ut_SettingData::testStringDomain()
{
    const QVariantMap attributes =
        domainOf(QVariant(QVariantList() << QStringLiteral("en") << QStringLiteral("nl")));

    QVERIFY(validateSettingValue(Maliit::StringType, attributes, QVariant(QStringLiteral("en"))));
    QVERIFY(validateSettingValue(Maliit::StringType, attributes, QVariant(QStringLiteral("nl"))));
}

void Ut_SettingData::testStringDomainRejectsOutsider()
{
    const QVariantMap attributes =
        domainOf(QVariant(QVariantList() << QStringLiteral("en") << QStringLiteral("nl")));

    QVERIFY(!validateSettingValue(Maliit::StringType, attributes, QVariant(QStringLiteral("de"))));
    QVERIFY(!validateSettingValue(Maliit::StringType, attributes, QVariant(QString())));
}

void Ut_SettingData::testInvalidDomainIsNoConstraint()
{
    // No valueDomain attribute at all: anything of the right type goes.
    QVERIFY(validateSettingValue(Maliit::StringType, QVariantMap(),
                                 QVariant(QStringLiteral("anything"))));
    QVERIFY(validateSettingValue(Maliit::StringType, domainOf(QVariant()),
                                 QVariant(QStringLiteral("anything"))));
}

void Ut_SettingData::testNonListDomainRejectsEverything()
{
    // A domain that is present but not a list is a malformed constraint; it
    // must not silently degrade into "unconstrained".
    const QVariantMap attributes = domainOf(QVariant(QStringLiteral("not-a-list")));

    QVERIFY(!validateSettingValue(Maliit::StringType, attributes,
                                  QVariant(QStringLiteral("not-a-list"))));
}

void Ut_SettingData::testIntAccepted_data()
{
    QTest::addColumn<QVariant>("value");

    QTest::newRow("zero")     << QVariant(0);
    QTest::newRow("positive") << QVariant(1234);
    QTest::newRow("negative") << QVariant(-1234);
    QTest::newRow("numeric string") << QVariant(QStringLiteral("42"));
}

void Ut_SettingData::testIntAccepted()
{
    QFETCH(QVariant, value);

    QVERIFY(validateSettingValue(Maliit::IntType, QVariantMap(), value));
}

void Ut_SettingData::testIntRejectsNonNumeric()
{
    QVERIFY(!validateSettingValue(Maliit::IntType, QVariantMap(),
                                  QVariant(QStringLiteral("not a number"))));
    QVERIFY(!validateSettingValue(Maliit::IntType, QVariantMap(),
                                  QVariant(QVariantList() << 1 << 2)));
}

void Ut_SettingData::testIntRange_data()
{
    QTest::addColumn<int>("value");
    QTest::addColumn<bool>("valid");

    QTest::newRow("below")     << 0   << false;
    QTest::newRow("at min")    << 1   << true;
    QTest::newRow("inside")    << 50  << true;
    QTest::newRow("at max")    << 100 << true;
    QTest::newRow("above")     << 101 << false;
}

void Ut_SettingData::testIntRange()
{
    QFETCH(int, value);
    QFETCH(bool, valid);

    const QVariantMap attributes = rangeOf(QVariant(1), QVariant(100));

    QCOMPARE(validateSettingValue(Maliit::IntType, attributes, QVariant(value)), valid);
}

void Ut_SettingData::testIntRangeWithOnlyOneBound()
{
    const QVariantMap minOnly = rangeOf(QVariant(10), QVariant());
    QVERIFY(!validateSettingValue(Maliit::IntType, minOnly, QVariant(9)));
    QVERIFY(validateSettingValue(Maliit::IntType, minOnly, QVariant(10)));
    QVERIFY(validateSettingValue(Maliit::IntType, minOnly, QVariant(1000000)));

    const QVariantMap maxOnly = rangeOf(QVariant(), QVariant(10));
    QVERIFY(validateSettingValue(Maliit::IntType, maxOnly, QVariant(-1000000)));
    QVERIFY(validateSettingValue(Maliit::IntType, maxOnly, QVariant(10)));
    QVERIFY(!validateSettingValue(Maliit::IntType, maxOnly, QVariant(11)));
}

void Ut_SettingData::testNonNumericRangeBoundRejects()
{
    const QVariantMap attributes = rangeOf(QVariant(QStringLiteral("low")), QVariant());

    QVERIFY(!validateSettingValue(Maliit::IntType, attributes, QVariant(5)));
}

void Ut_SettingData::testBool_data()
{
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<bool>("valid");

    QTest::newRow("true")           << QVariant(true)                    << true;
    QTest::newRow("false")          << QVariant(false)                   << true;
    QTest::newRow("int")            << QVariant(1)                       << true;
    QTest::newRow("string")         << QVariant(QStringLiteral("true"))  << true;
    QTest::newRow("list")           << QVariant(QVariantList() << 1)     << false;
}

void Ut_SettingData::testBool()
{
    QFETCH(QVariant, value);
    QFETCH(bool, valid);

    QCOMPARE(validateSettingValue(Maliit::BoolType, QVariantMap(), value), valid);
}

void Ut_SettingData::testStringList()
{
    QVERIFY(validateSettingValue(Maliit::StringListType, QVariantMap(),
                                 QVariant(QStringList() << "a" << "b")));
    QVERIFY(validateSettingValue(Maliit::StringListType, QVariantMap(),
                                 QVariant(QStringList())));
}

void Ut_SettingData::testStringListDomain()
{
    const QVariantMap attributes =
        domainOf(QVariant(QVariantList() << QStringLiteral("a") << QStringLiteral("b")));

    QVERIFY(validateSettingValue(Maliit::StringListType, attributes,
                                 QVariant(QStringList() << "a")));
    QVERIFY(validateSettingValue(Maliit::StringListType, attributes,
                                 QVariant(QStringList() << "a" << "b")));
    QVERIFY(!validateSettingValue(Maliit::StringListType, attributes,
                                  QVariant(QStringList() << "a" << "c")));
}

void Ut_SettingData::testIntList()
{
    QVERIFY(validateSettingValue(Maliit::IntListType, QVariantMap(),
                                 QVariant(QVariantList() << 1 << 2 << 3)));
    QVERIFY(validateSettingValue(Maliit::IntListType, QVariantMap(),
                                 QVariant(QVariantList())));
}

void Ut_SettingData::testIntListRejectsNonIntElement()
{
    QVERIFY(!validateSettingValue(Maliit::IntListType, QVariantMap(),
                                  QVariant(QVariantList() << 1 << QStringLiteral("two"))));
    QVERIFY(!validateSettingValue(Maliit::IntListType, QVariantMap(),
                                  QVariant(QStringLiteral("1,2,3"))));
}

void Ut_SettingData::testIntListRange()
{
    const QVariantMap attributes = rangeOf(QVariant(0), QVariant(10));

    QVERIFY(validateSettingValue(Maliit::IntListType, attributes,
                                 QVariant(QVariantList() << 0 << 5 << 10)));
    QVERIFY(!validateSettingValue(Maliit::IntListType, attributes,
                                  QVariant(QVariantList() << 0 << 11)));
}

void Ut_SettingData::testEmptyAttributes()
{
    // Missing attributes must read as "no constraint", not as a constraint
    // nothing can satisfy.
    QVERIFY(validateSettingValue(Maliit::StringType, QVariantMap(),
                                 QVariant(QStringLiteral("x"))));
    QVERIFY(validateSettingValue(Maliit::IntType, QVariantMap(), QVariant(0)));
    QVERIFY(validateSettingValue(Maliit::BoolType, QVariantMap(), QVariant(false)));
}

void Ut_SettingData::testInvalidValue()
{
    const QVariant invalid;

    QVERIFY(!validateSettingValue(Maliit::IntType, QVariantMap(), invalid));
    QVERIFY(!validateSettingValue(Maliit::IntListType, QVariantMap(), invalid));
}

QTEST_GUILESS_MAIN(Ut_SettingData)
#include "ut_settingdata.moc"
