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

// Every argument these handlers take arrives from a client over the
// connection: the client id, the extension id, the target string, the
// attribute name and its value. The manager is what keeps one client's writes
// out of another's extension, and what has to survive a client sending
// nonsense - so that is what is exercised here, rather than the happy path a
// well-behaved input context produces.

#include "mattributeextensionmanager.h"
#include "mattributeextensionid.h"

#include <maliit/plugins/attributeextension.h>
#include <maliit/plugins/keyoverride.h>
#include <maliit/plugins/keyoverridedata.h>

#include <QtTest>

class Ut_MAttributeExtensionManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testEmpty();
    void testRegisterAndUnregister();
    void testRegisterIsIdempotent();
    void testRegisterRejectsInvalidId();
    void testRegisterRejectsMissingFile();
    void testUnregisterUnknownIsHarmless();

    void testClientsAreIsolated();
    void testClientDisconnectDropsOnlyItsOwn();

    void testKeyOverrideCreatedSignal();
    void testKeyOverrideValueIsStored();
    void testKeyOverrideLengthVariantsAreTrimmed();
    void testAttributeForUnknownExtensionIsIgnored();
    void testAttributeWithEmptyTargetItemIsIgnored();
    void testAttributeWithInvalidValueIsIgnored();
    void testUnknownTargetIsIgnored();
    void testGlobalTargetIsSignalled();

    void testKeyOverridesForUnknownIdIsEmpty();
    void testCopyPasteState();
};

void Ut_MAttributeExtensionManager::testEmpty()
{
    MAttributeExtensionManager manager;

    QVERIFY(manager.attributeExtensionIdList().isEmpty());
    QVERIFY(!manager.contains(MAttributeExtensionId(1, QStringLiteral("1"))));
    QVERIFY(manager.attributeExtension(MAttributeExtensionId(1, QStringLiteral("1"))).isNull());
}

void Ut_MAttributeExtensionManager::testRegisterAndUnregister()
{
    MAttributeExtensionManager manager;
    const MAttributeExtensionId id(1, QStringLiteral("100"));

    manager.registerAttributeExtension(id, QString());
    QVERIFY(manager.contains(id));
    QVERIFY(!manager.attributeExtension(id).isNull());

    manager.unregisterAttributeExtension(id);
    QVERIFY(!manager.contains(id));
    QVERIFY(manager.attributeExtension(id).isNull());
}

void Ut_MAttributeExtensionManager::testRegisterIsIdempotent()
{
    MAttributeExtensionManager manager;
    const MAttributeExtensionId id(1, QStringLiteral("100"));

    manager.registerAttributeExtension(id, QString());
    const QSharedPointer<MAttributeExtension> first = manager.attributeExtension(id);

    // A client re-registering the same id must not replace the extension and
    // silently discard the attributes already written into it.
    manager.registerAttributeExtension(id, QString());
    QCOMPARE(manager.attributeExtension(id).data(), first.data());
    QCOMPARE(manager.attributeExtensionIdList().size(), 1);
}

void Ut_MAttributeExtensionManager::testRegisterRejectsInvalidId()
{
    MAttributeExtensionManager manager;

    manager.registerAttributeExtension(MAttributeExtensionId(-1, QStringLiteral("100")), QString());
    manager.registerAttributeExtension(MAttributeExtensionId(1, QString()), QString());
    manager.registerAttributeExtension(MAttributeExtensionId(), QString());

    QVERIFY(manager.attributeExtensionIdList().isEmpty());
}

void Ut_MAttributeExtensionManager::testRegisterRejectsMissingFile()
{
    MAttributeExtensionManager manager;
    const MAttributeExtensionId id(1, QStringLiteral("100"));

    // A non-empty file name that does not resolve is a client typo, not a
    // request to register the default extension.
    manager.registerAttributeExtension(id, QStringLiteral("no-such-toolbar-file.xml"));

    QVERIFY(!manager.contains(id));
}

void Ut_MAttributeExtensionManager::testUnregisterUnknownIsHarmless()
{
    MAttributeExtensionManager manager;

    manager.unregisterAttributeExtension(MAttributeExtensionId(9, QStringLiteral("9")));
    QVERIFY(manager.attributeExtensionIdList().isEmpty());
}

void Ut_MAttributeExtensionManager::testClientsAreIsolated()
{
    MAttributeExtensionManager manager;

    // Same local extension id, two different clients.
    manager.handleAttributeExtensionRegistered(100, 1, QString());
    manager.handleAttributeExtensionRegistered(200, 1, QString());

    QCOMPARE(manager.attributeExtensionIdList().size(), 2);

    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/keys"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("label"),
                                          QVariant(QStringLiteral("A")));
    manager.handleExtendedAttributeUpdate(200, 1, QStringLiteral("/keys"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("label"),
                                          QVariant(QStringLiteral("B")));

    const QMap<QString, QSharedPointer<MKeyOverride> > first =
        manager.keyOverrides(MAttributeExtensionId(1, QStringLiteral("100")));
    const QMap<QString, QSharedPointer<MKeyOverride> > second =
        manager.keyOverrides(MAttributeExtensionId(1, QStringLiteral("200")));

    QCOMPARE(first.value(QStringLiteral("enter"))->label(), QStringLiteral("A"));
    QCOMPARE(second.value(QStringLiteral("enter"))->label(), QStringLiteral("B"));
}

void Ut_MAttributeExtensionManager::testClientDisconnectDropsOnlyItsOwn()
{
    MAttributeExtensionManager manager;

    manager.handleAttributeExtensionRegistered(100, 1, QString());
    manager.handleAttributeExtensionRegistered(200, 1, QString());

    manager.handleClientDisconnect(100);

    QVERIFY(!manager.contains(MAttributeExtensionId(1, QStringLiteral("100"))));
    QVERIFY(manager.contains(MAttributeExtensionId(1, QStringLiteral("200"))));
}

void Ut_MAttributeExtensionManager::testKeyOverrideCreatedSignal()
{
    MAttributeExtensionManager manager;
    manager.handleAttributeExtensionRegistered(100, 1, QString());

    QSignalSpy spy(&manager, SIGNAL(keyOverrideCreated()));

    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/keys"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("label"),
                                          QVariant(QStringLiteral("Go")));
    QCOMPARE(spy.count(), 1);

    // Writing a second attribute on the same key is not a new override.
    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/keys"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("highlighted"),
                                          QVariant(true));
    QCOMPARE(spy.count(), 1);
}

void Ut_MAttributeExtensionManager::testKeyOverrideValueIsStored()
{
    MAttributeExtensionManager manager;
    manager.handleAttributeExtensionRegistered(100, 1, QString());

    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/keys"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("label"),
                                          QVariant(QStringLiteral("Go")));
    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/keys"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("enabled"),
                                          QVariant(false));

    const QMap<QString, QSharedPointer<MKeyOverride> > overrides =
        manager.keyOverrides(MAttributeExtensionId(1, QStringLiteral("100")));

    QCOMPARE(overrides.size(), 1);
    QCOMPARE(overrides.value(QStringLiteral("enter"))->label(), QStringLiteral("Go"));
    QCOMPARE(overrides.value(QStringLiteral("enter"))->enabled(), false);
}

void Ut_MAttributeExtensionManager::testKeyOverrideLengthVariantsAreTrimmed()
{
    MAttributeExtensionManager manager;
    manager.handleAttributeExtensionRegistered(100, 1, QString());

    // Localised labels arrive as U+009C separated length variants, longest
    // first; only the first is meant to reach the plugin.
    const QString variants = QString::fromUtf8("Longest") + QChar(0x9c) + QString::fromUtf8("Short");

    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/keys"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("label"),
                                          QVariant(variants));

    const QMap<QString, QSharedPointer<MKeyOverride> > overrides =
        manager.keyOverrides(MAttributeExtensionId(1, QStringLiteral("100")));
    QCOMPARE(overrides.value(QStringLiteral("enter"))->label(), QStringLiteral("Longest"));
}

void Ut_MAttributeExtensionManager::testAttributeForUnknownExtensionIsIgnored()
{
    MAttributeExtensionManager manager;

    // No registration at all: a write must be dropped rather than creating
    // the extension implicitly.
    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/keys"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("label"),
                                          QVariant(QStringLiteral("Go")));

    QVERIFY(manager.keyOverrides(MAttributeExtensionId(1, QStringLiteral("100"))).isEmpty());
}

void Ut_MAttributeExtensionManager::testAttributeWithEmptyTargetItemIsIgnored()
{
    MAttributeExtensionManager manager;
    manager.handleAttributeExtensionRegistered(100, 1, QString());

    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/keys"),
                                          QString(),
                                          QStringLiteral("label"),
                                          QVariant(QStringLiteral("Go")));

    QVERIFY(manager.keyOverrides(MAttributeExtensionId(1, QStringLiteral("100"))).isEmpty());
}

void Ut_MAttributeExtensionManager::testAttributeWithInvalidValueIsIgnored()
{
    MAttributeExtensionManager manager;
    manager.handleAttributeExtensionRegistered(100, 1, QString());

    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/keys"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("label"),
                                          QVariant());

    QVERIFY(manager.keyOverrides(MAttributeExtensionId(1, QStringLiteral("100"))).isEmpty());
}

void Ut_MAttributeExtensionManager::testUnknownTargetIsIgnored()
{
    MAttributeExtensionManager manager;
    manager.handleAttributeExtensionRegistered(100, 1, QString());

    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/nonsense"),
                                          QStringLiteral("enter"),
                                          QStringLiteral("label"),
                                          QVariant(QStringLiteral("Go")));

    QVERIFY(manager.keyOverrides(MAttributeExtensionId(1, QStringLiteral("100"))).isEmpty());
}

void Ut_MAttributeExtensionManager::testGlobalTargetIsSignalled()
{
    MAttributeExtensionManager manager;
    manager.handleAttributeExtensionRegistered(100, 1, QString());

    qRegisterMetaType<MAttributeExtensionId>("MAttributeExtensionId");
    QSignalSpy spy(&manager,
                   SIGNAL(globalAttributeChanged(MAttributeExtensionId,QString,QString,QVariant)));

    manager.handleExtendedAttributeUpdate(100, 1, QStringLiteral("/"),
                                          QStringLiteral("inputMethod"),
                                          QStringLiteral("loadAll"),
                                          QVariant(true));

    QCOMPARE(spy.count(), 1);
}

void Ut_MAttributeExtensionManager::testKeyOverridesForUnknownIdIsEmpty()
{
    MAttributeExtensionManager manager;

    QVERIFY(manager.keyOverrides(MAttributeExtensionId()).isEmpty());
    QVERIFY(manager.keyOverrides(
                MAttributeExtensionId(9, QStringLiteral("9"))).isEmpty());
}

void Ut_MAttributeExtensionManager::testCopyPasteState()
{
    MAttributeExtensionManager manager;

    // Just has to be callable repeatedly without surprises; the state itself
    // is private.
    manager.setCopyPasteState(false, false);
    manager.setCopyPasteState(true, false);
    manager.setCopyPasteState(true, false);
    manager.setCopyPasteState(false, true);
    manager.setCopyPasteState(false, false);
}

QTEST_GUILESS_MAIN(Ut_MAttributeExtensionManager)
#include "ut_mattributeextensionmanager.moc"
