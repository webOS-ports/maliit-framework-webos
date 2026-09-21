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

// Unlike MAttributeExtensionManager, this one is deliberately shared: every
// subscribed client sees every plugin-setting change. What it must still get
// right is who counts as subscribed, and that a write is validated against the
// registered setting's type and constraints before it reaches MImSettings.

#include "msharedattributeextensionmanager.h"
#include "mimsettings.h"

#include <maliit/namespace.h>
#include <maliit/settingdata.h>

#include <QtTest>

class Ut_SharedAttributeExtensionManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void testSubscriberBookkeeping();
    void testSubscribingTwiceCountsOnce();
    void testOtherExtensionIdsAreIgnored();
    void testDisconnectRemovesSubscriber();
    void testDisconnectOfUnknownClientIsHarmless();
    void testOutOfRangeClientIdIsRejected();

    void testValidWriteReachesTheSetting();
    void testWriteOutsideDomainIsRejected();
    void testWriteOutsideRangeIsRejected();
    void testWriteOfWrongTypeIsRejected();
    void testWriteToUnregisteredSettingIsIgnored();
    void testWriteWithWrongExtensionIdIsIgnored();

    void testSettingChangeNotifiesSubscribers();

private:
    QString uniqueKey() const
    {
        static int counter = 0;
        return QString("/maliit/pluginsettings/ut%1/value").arg(++counter);
    }
};

void Ut_SharedAttributeExtensionManager::initTestCase()
{
    MImSettings::setPreferredSettingsType(MImSettings::TemporarySettings);
    qRegisterMetaType<QList<int> >("QList<int>");
}

void Ut_SharedAttributeExtensionManager::testSubscriberBookkeeping()
{
    MSharedAttributeExtensionManager manager;

    manager.handleAttributeExtensionRegistered(
        100, MSharedAttributeExtensionManager::PluginSettings, QString());
    manager.handleAttributeExtensionRegistered(
        200, MSharedAttributeExtensionManager::PluginSettings, QString());

    const QString key = uniqueKey();
    manager.registerPluginSetting(key, Maliit::StringType, QVariantMap());

    QSignalSpy spy(&manager,
                   SIGNAL(notifyExtensionAttributeChanged(QList<int>,int,QString,QString,QString,QVariant)));

    MImSettings setting(key);
    setting.set(QVariant(QStringLiteral("x")));

    QCOMPARE(spy.count(), 1);
    const QList<int> clients = spy.first().at(0).value<QList<int> >();
    QVERIFY(clients.contains(100));
    QVERIFY(clients.contains(200));
}

void Ut_SharedAttributeExtensionManager::testSubscribingTwiceCountsOnce()
{
    MSharedAttributeExtensionManager manager;

    manager.handleAttributeExtensionRegistered(
        100, MSharedAttributeExtensionManager::PluginSettings, QString());
    manager.handleAttributeExtensionRegistered(
        100, MSharedAttributeExtensionManager::PluginSettings, QString());

    const QString key = uniqueKey();
    manager.registerPluginSetting(key, Maliit::StringType, QVariantMap());

    QSignalSpy spy(&manager,
                   SIGNAL(notifyExtensionAttributeChanged(QList<int>,int,QString,QString,QString,QVariant)));

    MImSettings setting(key);
    setting.set(QVariant(QStringLiteral("x")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).value<QList<int> >().count(100), 1);
}

void Ut_SharedAttributeExtensionManager::testOtherExtensionIdsAreIgnored()
{
    MSharedAttributeExtensionManager manager;

    // Extension ids other than PluginSettings belong to the per-client
    // manager. Registering one here must not subscribe the client, and - the
    // reason this test exists - must not be reported as a truncation problem
    // either, since the client id involved is perfectly ordinary.
    manager.handleAttributeExtensionRegistered(100, 1, QString());
    manager.handleAttributeExtensionRegistered(100, -5, QString());

    const QString key = uniqueKey();
    manager.registerPluginSetting(key, Maliit::StringType, QVariantMap());

    QSignalSpy spy(&manager,
                   SIGNAL(notifyExtensionAttributeChanged(QList<int>,int,QString,QString,QString,QVariant)));

    MImSettings setting(key);
    setting.set(QVariant(QStringLiteral("x")));

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.first().at(0).value<QList<int> >().isEmpty());
}

void Ut_SharedAttributeExtensionManager::testDisconnectRemovesSubscriber()
{
    MSharedAttributeExtensionManager manager;

    manager.handleAttributeExtensionRegistered(
        100, MSharedAttributeExtensionManager::PluginSettings, QString());
    manager.handleAttributeExtensionRegistered(
        200, MSharedAttributeExtensionManager::PluginSettings, QString());
    manager.handleClientDisconnect(100);

    const QString key = uniqueKey();
    manager.registerPluginSetting(key, Maliit::StringType, QVariantMap());

    QSignalSpy spy(&manager,
                   SIGNAL(notifyExtensionAttributeChanged(QList<int>,int,QString,QString,QString,QVariant)));

    MImSettings setting(key);
    setting.set(QVariant(QStringLiteral("x")));

    const QList<int> clients = spy.first().at(0).value<QList<int> >();
    QVERIFY(!clients.contains(100));
    QVERIFY(clients.contains(200));
}

void Ut_SharedAttributeExtensionManager::testDisconnectOfUnknownClientIsHarmless()
{
    MSharedAttributeExtensionManager manager;

    manager.handleClientDisconnect(12345);
    manager.handleAttributeExtensionUnregistered(
        12345, MSharedAttributeExtensionManager::PluginSettings);
}

void Ut_SharedAttributeExtensionManager::testOutOfRangeClientIdIsRejected()
{
    MSharedAttributeExtensionManager manager;

    // The subscriber list holds ints; a client id that does not fit is
    // dropped rather than wrapped to a negative one that would then alias
    // some other client.
    manager.handleAttributeExtensionRegistered(
        0xffffffffu, MSharedAttributeExtensionManager::PluginSettings, QString());

    const QString key = uniqueKey();
    manager.registerPluginSetting(key, Maliit::StringType, QVariantMap());

    QSignalSpy spy(&manager,
                   SIGNAL(notifyExtensionAttributeChanged(QList<int>,int,QString,QString,QString,QVariant)));

    MImSettings setting(key);
    setting.set(QVariant(QStringLiteral("x")));

    QVERIFY(spy.first().at(0).value<QList<int> >().isEmpty());
}

void Ut_SharedAttributeExtensionManager::testValidWriteReachesTheSetting()
{
    MSharedAttributeExtensionManager manager;

    const QString key = QStringLiteral("/target/item/attribute");
    manager.registerPluginSetting(key, Maliit::StringType, QVariantMap());

    manager.handleExtendedAttributeUpdate(100,
                                          MSharedAttributeExtensionManager::PluginSettings,
                                          QStringLiteral("/target"),
                                          QStringLiteral("item"),
                                          QStringLiteral("attribute"),
                                          QVariant(QStringLiteral("written")));

    MImSettings setting(key);
    QCOMPARE(setting.value().toString(), QStringLiteral("written"));
}

void Ut_SharedAttributeExtensionManager::testWriteOutsideDomainIsRejected()
{
    MSharedAttributeExtensionManager manager;

    QVariantMap attributes;
    attributes[Maliit::SettingEntryAttributes::valueDomain] =
        QVariant(QVariantList() << QStringLiteral("allowed"));

    const QString key = QStringLiteral("/domain/item/attribute");
    manager.registerPluginSetting(key, Maliit::StringType, attributes);

    MImSettings setting(key);
    setting.set(QVariant(QStringLiteral("allowed")));

    manager.handleExtendedAttributeUpdate(100,
                                          MSharedAttributeExtensionManager::PluginSettings,
                                          QStringLiteral("/domain"),
                                          QStringLiteral("item"),
                                          QStringLiteral("attribute"),
                                          QVariant(QStringLiteral("not-allowed")));

    QCOMPARE(setting.value().toString(), QStringLiteral("allowed"));
}

void Ut_SharedAttributeExtensionManager::testWriteOutsideRangeIsRejected()
{
    MSharedAttributeExtensionManager manager;

    QVariantMap attributes;
    attributes[Maliit::SettingEntryAttributes::valueRangeMin] = QVariant(0);
    attributes[Maliit::SettingEntryAttributes::valueRangeMax] = QVariant(10);

    const QString key = QStringLiteral("/range/item/attribute");
    manager.registerPluginSetting(key, Maliit::IntType, attributes);

    MImSettings setting(key);
    setting.set(QVariant(5));

    manager.handleExtendedAttributeUpdate(100,
                                          MSharedAttributeExtensionManager::PluginSettings,
                                          QStringLiteral("/range"),
                                          QStringLiteral("item"),
                                          QStringLiteral("attribute"),
                                          QVariant(11));

    QCOMPARE(setting.value().toInt(), 5);
}

void Ut_SharedAttributeExtensionManager::testWriteOfWrongTypeIsRejected()
{
    MSharedAttributeExtensionManager manager;

    const QString key = QStringLiteral("/type/item/attribute");
    manager.registerPluginSetting(key, Maliit::IntType, QVariantMap());

    MImSettings setting(key);
    setting.set(QVariant(1));

    manager.handleExtendedAttributeUpdate(100,
                                          MSharedAttributeExtensionManager::PluginSettings,
                                          QStringLiteral("/type"),
                                          QStringLiteral("item"),
                                          QStringLiteral("attribute"),
                                          QVariant(QStringLiteral("not a number")));

    QCOMPARE(setting.value().toInt(), 1);
}

void Ut_SharedAttributeExtensionManager::testWriteToUnregisteredSettingIsIgnored()
{
    MSharedAttributeExtensionManager manager;

    const QString key = QStringLiteral("/unregistered/item/attribute");
    MImSettings setting(key);

    manager.handleExtendedAttributeUpdate(100,
                                          MSharedAttributeExtensionManager::PluginSettings,
                                          QStringLiteral("/unregistered"),
                                          QStringLiteral("item"),
                                          QStringLiteral("attribute"),
                                          QVariant(QStringLiteral("nope")));

    QVERIFY(!setting.value().isValid());
}

void Ut_SharedAttributeExtensionManager::testWriteWithWrongExtensionIdIsIgnored()
{
    MSharedAttributeExtensionManager manager;

    const QString key = QStringLiteral("/wrongid/item/attribute");
    manager.registerPluginSetting(key, Maliit::StringType, QVariantMap());

    MImSettings setting(key);

    manager.handleExtendedAttributeUpdate(100, 12345,
                                          QStringLiteral("/wrongid"),
                                          QStringLiteral("item"),
                                          QStringLiteral("attribute"),
                                          QVariant(QStringLiteral("nope")));

    QVERIFY(!setting.value().isValid());
}

void Ut_SharedAttributeExtensionManager::testSettingChangeNotifiesSubscribers()
{
    MSharedAttributeExtensionManager manager;

    manager.handleAttributeExtensionRegistered(
        100, MSharedAttributeExtensionManager::PluginSettings, QString());

    const QString key = QStringLiteral("/notify/item/attribute");
    manager.registerPluginSetting(key, Maliit::StringType, QVariantMap());

    QSignalSpy spy(&manager,
                   SIGNAL(notifyExtensionAttributeChanged(QList<int>,int,QString,QString,QString,QVariant)));

    MImSettings setting(key);
    setting.set(QVariant(QStringLiteral("changed")));

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> arguments = spy.first();
    QCOMPARE(arguments.at(1).toInt(),
             static_cast<int>(MSharedAttributeExtensionManager::PluginSettings));
    QCOMPARE(arguments.at(2).toString(), QStringLiteral("/notify"));
    QCOMPARE(arguments.at(3).toString(), QStringLiteral("item"));
    QCOMPARE(arguments.at(4).toString(), QStringLiteral("attribute"));
    QCOMPARE(arguments.at(5).toString(), QStringLiteral("changed"));
}

QTEST_GUILESS_MAIN(Ut_SharedAttributeExtensionManager)
#include "ut_sharedattributeextensionmanager.moc"
