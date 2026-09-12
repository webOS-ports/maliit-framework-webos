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

// parseCommandLine() walks argv handing each element to every registered
// parser in turn, and advances by however many arguments the parser claims it
// consumed. A parser that does not set that count, or that claims one it did
// not take, desynchronises the walk for everything after it - so the argument
// accounting is what these tests are mostly about.

#include "mimserveroptions.h"

#include <QtTest>

namespace {

//! Runs parseCommandLine() over a program name plus \a arguments.
bool parse(const QStringList &arguments)
{
    QList<QByteArray> storage;
    storage.append(QByteArray("MaliitServer"));
    Q_FOREACH (const QString &argument, arguments) {
        storage.append(argument.toUtf8());
    }

    QVector<const char *> argv;
    Q_FOREACH (const QByteArray &argument, storage) {
        argv.append(argument.constData());
    }

    return parseCommandLine(argv.size(), argv.data());
}

} // namespace

class Ut_MImServerOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNoArguments();
    void testHelp();
    void testInstance();
    void testInstanceWithoutArgument();
    void testNoLS2Service();
    void testUnknownOption();
    void testIgnoredOptionWithArgument();
    void testIgnoredOptionWithoutArgument();
    void testOptionAfterIgnoredOptionArgument();
    void testCombination();
    void testDefaults();
    void testInstanceIsNotSharedBetweenParses();
};

void Ut_MImServerOptions::testNoArguments()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    QVERIFY(parse(QStringList()));
    QVERIFY(!common.showHelp);
    QCOMPARE(connection.instanceId, 0);
    QCOMPARE(connection.noLS2Service, false);
}

void Ut_MImServerOptions::testHelp()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    QVERIFY(parse(QStringList() << "-help"));
    QVERIFY(common.showHelp);
}

void Ut_MImServerOptions::testInstance()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    QVERIFY(parse(QStringList() << "-instance" << "3"));
    QCOMPARE(connection.instanceId, 3);
    QVERIFY(!common.showHelp);
}

void Ut_MImServerOptions::testInstanceWithoutArgument()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    // Recognised but incomplete: the option itself is consumed, and no
    // argument is claimed, so nothing after it gets swallowed.
    QVERIFY(parse(QStringList() << "-instance"));
    QCOMPARE(connection.instanceId, 0);
}

void Ut_MImServerOptions::testNoLS2Service()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    QVERIFY(parse(QStringList() << "-no-ls2-service"));
    QVERIFY(connection.noLS2Service);
}

void Ut_MImServerOptions::testUnknownOption()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    QVERIFY(!parse(QStringList() << "-there-is-no-such-option"));
}

void Ut_MImServerOptions::testIgnoredOptionWithArgument()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    // Qt's own options must not be reported as errors.
    QVERIFY(parse(QStringList() << "-style" << "fusion"));
}

void Ut_MImServerOptions::testIgnoredOptionWithoutArgument()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    QVERIFY(parse(QStringList() << "-reverse"));
    QVERIFY(parse(QStringList() << "-sync"));
}

void Ut_MImServerOptions::testOptionAfterIgnoredOptionArgument()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    // -style takes an argument; if the walk did not skip it, "fusion" would
    // be reported as an invalid parameter and -instance would be misread.
    QVERIFY(parse(QStringList() << "-style" << "fusion" << "-instance" << "5"));
    QCOMPARE(connection.instanceId, 5);
}

void Ut_MImServerOptions::testCombination()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    QVERIFY(parse(QStringList() << "-instance" << "2" << "-no-ls2-service" << "-help"));
    QCOMPARE(connection.instanceId, 2);
    QVERIFY(connection.noLS2Service);
    QVERIFY(common.showHelp);
}

void Ut_MImServerOptions::testDefaults()
{
    MImServerCommonOptions common;
    MImServerConnectionOptions connection;

    QVERIFY(!common.showHelp);
    QCOMPARE(connection.instanceId, 0);
    QCOMPARE(connection.noLS2Service, false);
}

void Ut_MImServerOptions::testInstanceIsNotSharedBetweenParses()
{
    {
        MImServerConnectionOptions connection;
        QVERIFY(parse(QStringList() << "-instance" << "9"));
        QCOMPARE(connection.instanceId, 9);
    }

    // The previous options object unregistered itself on destruction, so a
    // fresh one must start from the default rather than from 9.
    MImServerConnectionOptions connection;
    QCOMPARE(connection.instanceId, 0);
    QVERIFY(parse(QStringList()));
    QCOMPARE(connection.instanceId, 0);
}

QTEST_GUILESS_MAIN(Ut_MImServerOptions)
#include "ut_mimserveroptions.moc"
