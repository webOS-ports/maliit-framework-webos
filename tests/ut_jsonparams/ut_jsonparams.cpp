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

// Maliit::Json::toInt() is the gate in front of every number that reaches
// MaliitServer from the LS2 bus, so it is exercised here with the values a
// hostile - or merely sloppy - caller can actually put in a payload, not just
// the ones the IME service happens to send.

#include "mimjsonparams.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <cmath>
#include <climits>

class Ut_JsonParams : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testAcceptsValuesInRange_data();
    void testAcceptsValuesInRange();

    void testRejectsValuesOutOfRange_data();
    void testRejectsValuesOutOfRange();

    void testRejectsNonNumbers_data();
    void testRejectsNonNumbers();

    void testRejectsNonFinite();
    void testTruncatesTowardsZero_data();
    void testTruncatesTowardsZero();

    void testLeavesResultUntouchedOnFailure();
    void testRejectsNullResultPointer();

    void testFullIntRangeBoundaries();
    void testValuesJustOutsideIntRange();

    void testFromParsedPayload_data();
    void testFromParsedPayload();
};

void Ut_JsonParams::testAcceptsValuesInRange_data()
{
    QTest::addColumn<double>("raw");
    QTest::addColumn<int>("min");
    QTest::addColumn<int>("max");
    QTest::addColumn<int>("expected");

    QTest::newRow("at minimum")        << 1.0    << 1 << 100 <<    1;
    QTest::newRow("at maximum")        << 100.0  << 1 << 100 <<  100;
    QTest::newRow("inside")            << 42.0   << 1 << 100 <<   42;
    QTest::newRow("zero in range")     << 0.0    << -5 << 5  <<    0;
    QTest::newRow("negative in range") << -5.0   << -5 << 5  <<   -5;
    QTest::newRow("single value")      << 7.0    << 7 << 7   <<    7;
}

void Ut_JsonParams::testAcceptsValuesInRange()
{
    QFETCH(double, raw);
    QFETCH(int, min);
    QFETCH(int, max);
    QFETCH(int, expected);

    int result = -12345;
    QVERIFY(Maliit::Json::toInt(QJsonValue(raw), min, max, &result));
    QCOMPARE(result, expected);
}

void Ut_JsonParams::testRejectsValuesOutOfRange_data()
{
    QTest::addColumn<double>("raw");
    QTest::addColumn<int>("min");
    QTest::addColumn<int>("max");

    QTest::newRow("one below minimum")  << 0.0            << 1 << 100;
    QTest::newRow("one above maximum")  << 101.0          << 1 << 100;
    QTest::newRow("far below")          << -1.0e18        << 1 << 100;
    QTest::newRow("far above")          << 1.0e18         << 1 << 100;
    // The value that made the unguarded "(int) toDouble()" cast undefined.
    QTest::newRow("beyond INT_MAX")     << 4294967296.0   << 1 << INT_MAX;
    QTest::newRow("below INT_MIN")      << -4294967296.0  << INT_MIN << INT_MAX;
    QTest::newRow("empty range")        << 5.0            << 10 << 1;
}

void Ut_JsonParams::testRejectsValuesOutOfRange()
{
    QFETCH(double, raw);
    QFETCH(int, min);
    QFETCH(int, max);

    int result = 0;
    QVERIFY(!Maliit::Json::toInt(QJsonValue(raw), min, max, &result));
}

void Ut_JsonParams::testRejectsNonNumbers_data()
{
    QTest::addColumn<QJsonValue>("value");

    QTest::newRow("undefined")    << QJsonValue(QJsonValue::Undefined);
    QTest::newRow("null")         << QJsonValue(QJsonValue::Null);
    QTest::newRow("bool true")    << QJsonValue(true);
    QTest::newRow("bool false")   << QJsonValue(false);
    QTest::newRow("string")       << QJsonValue(QStringLiteral("12"));
    QTest::newRow("empty string") << QJsonValue(QString());
    QTest::newRow("array")        << QJsonValue(QJsonArray() << 1 << 2);
    QTest::newRow("object")       << QJsonValue(QJsonObject());
}

void Ut_JsonParams::testRejectsNonNumbers()
{
    QFETCH(QJsonValue, value);

    int result = 0;
    QVERIFY(!Maliit::Json::toInt(value, 0, 100, &result));
}

void Ut_JsonParams::testRejectsNonFinite()
{
    int result = 0;

    // NaN compares false against every bound, which is exactly why the range
    // test is written as a positive assertion rather than a negated one.
    QVERIFY(!Maliit::Json::toInt(QJsonValue(std::nan("")), INT_MIN, INT_MAX, &result));
    QVERIFY(!Maliit::Json::toInt(QJsonValue(std::numeric_limits<double>::infinity()),
                                 INT_MIN, INT_MAX, &result));
    QVERIFY(!Maliit::Json::toInt(QJsonValue(-std::numeric_limits<double>::infinity()),
                                 INT_MIN, INT_MAX, &result));
}

void Ut_JsonParams::testTruncatesTowardsZero_data()
{
    QTest::addColumn<double>("raw");
    QTest::addColumn<int>("expected");

    QTest::newRow("positive, round down") << 1.9   <<  1;
    QTest::newRow("positive, just over")  << 1.0001 << 1;
    QTest::newRow("negative, round up")   << -1.9  << -1;
    QTest::newRow("negative, just under") << -1.0001 << -1;
    QTest::newRow("half")                 << 0.5   <<  0;
    QTest::newRow("negative half")        << -0.5  <<  0;
}

void Ut_JsonParams::testTruncatesTowardsZero()
{
    QFETCH(double, raw);
    QFETCH(int, expected);

    int result = 0;
    QVERIFY(Maliit::Json::toInt(QJsonValue(raw), INT_MIN, INT_MAX, &result));
    QCOMPARE(result, expected);
}

void Ut_JsonParams::testLeavesResultUntouchedOnFailure()
{
    // Callers seed the output with their own default and only overwrite it on
    // success, so a rejected value must not clobber it.
    int result = 4242;

    QVERIFY(!Maliit::Json::toInt(QJsonValue(QStringLiteral("nope")), 0, 10, &result));
    QCOMPARE(result, 4242);

    QVERIFY(!Maliit::Json::toInt(QJsonValue(1.0e18), 0, 10, &result));
    QCOMPARE(result, 4242);

    QVERIFY(!Maliit::Json::toInt(QJsonValue(std::nan("")), 0, 10, &result));
    QCOMPARE(result, 4242);
}

void Ut_JsonParams::testRejectsNullResultPointer()
{
    QVERIFY(!Maliit::Json::toInt(QJsonValue(5.0), 0, 10, 0));
}

void Ut_JsonParams::testFullIntRangeBoundaries()
{
    int result = 0;

    QVERIFY(Maliit::Json::toInt(QJsonValue(static_cast<double>(INT_MAX)),
                                INT_MIN, INT_MAX, &result));
    QCOMPARE(result, INT_MAX);

    QVERIFY(Maliit::Json::toInt(QJsonValue(static_cast<double>(INT_MIN)),
                                INT_MIN, INT_MAX, &result));
    QCOMPARE(result, INT_MIN);
}

void Ut_JsonParams::testValuesJustOutsideIntRange()
{
    int result = 7;

    // INT_MAX and INT_MIN are exactly representable as doubles, so "one past"
    // is a genuine test of the bound rather than of floating point rounding.
    QVERIFY(!Maliit::Json::toInt(QJsonValue(static_cast<double>(INT_MAX) + 1.0),
                                 INT_MIN, INT_MAX, &result));
    QVERIFY(!Maliit::Json::toInt(QJsonValue(static_cast<double>(INT_MIN) - 1.0),
                                 INT_MIN, INT_MAX, &result));
    QCOMPARE(result, 7);
}

void Ut_JsonParams::testFromParsedPayload_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<bool>("accepted");
    QTest::addColumn<int>("expected");

    // Payloads in the shape com.webos.service.ime/deleteCharacters receives.
    QTest::newRow("plain count")     << QByteArray("{\"count\": 3}")        << true  << 3;
    QTest::newRow("count as string") << QByteArray("{\"count\": \"3\"}")   << false << 0;
    QTest::newRow("count zero")      << QByteArray("{\"count\": 0}")       << false << 0;
    QTest::newRow("count negative")  << QByteArray("{\"count\": -3}")      << false << 0;
    QTest::newRow("count huge")      << QByteArray("{\"count\": 1e300}")   << false << 0;
    QTest::newRow("count missing")   << QByteArray("{}")                   << false << 0;
    QTest::newRow("count exponent")  << QByteArray("{\"count\": 1e2}")     << true  << 100;
}

void Ut_JsonParams::testFromParsedPayload()
{
    QFETCH(QByteArray, payload);
    QFETCH(bool, accepted);
    QFETCH(int, expected);

    const QJsonObject object = QJsonDocument::fromJson(payload).object();

    int result = 0;
    QCOMPARE(Maliit::Json::toInt(object.value("count"), 1, INT_MAX, &result), accepted);
    if (accepted) {
        QCOMPARE(result, expected);
    }
}

QTEST_GUILESS_MAIN(Ut_JsonParams)
#include "ut_jsonparams.moc"
