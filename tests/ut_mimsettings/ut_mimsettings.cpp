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

// The QSettings backend keeps a process-wide registry keyed on the setting
// name so that every MImSettings watching a key learns about a write through
// any of them. That registry is where the two lifetime bugs lived: an
// unchecked QHash::find() result, and - in the luna backend's copy of the same
// idea - an unregister that appended instead of removing. These tests drive
// the registry through the shapes that matter: several watchers on one key,
// watchers destroyed in either order, and a slot that destroys another
// watcher while the notification is being delivered.

#include "mimsettings.h"

#include <QtTest>

class Ut_MImSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void testUnsetKeyReturnsDefault();
    void testSetAndGet();
    void testUnset();
    void testSetInvalidUnsets();

    void testValueChangedOnSet();
    void testNoValueChangedOnIdenticalSet();
    void testValueChangedReachesEveryWatcher();
    void testWatcherStopsBeingNotifiedOnceDestroyed();
    void testLastWatcherDestroyedThenKeyUsedAgain();
    void testWatcherDestroyedFromWithinNotification();
    void testDistinctKeysAreIndependent();

    void testKeyAccessor();
    void testListEntries();

private:
    //! Every test gets its own key so ordering cannot matter.
    QString uniqueKey() const
    {
        static int counter = 0;
        return QString("/maliit/tests/key%1").arg(++counter);
    }
};

void Ut_MImSettings::initTestCase()
{
    // Temporary settings are backed by a QTemporaryFile, so nothing here
    // touches the real store or needs an LS2 bus.
    MImSettings::setPreferredSettingsType(MImSettings::TemporarySettings);
}

void Ut_MImSettings::testUnsetKeyReturnsDefault()
{
    MImSettings setting(uniqueKey());

    QVERIFY(!setting.value().isValid());
    QCOMPARE(setting.value(QVariant(42)).toInt(), 42);
}

void Ut_MImSettings::testSetAndGet()
{
    MImSettings setting(uniqueKey());

    setting.set(QVariant(QStringLiteral("hello")));
    QCOMPARE(setting.value().toString(), QStringLiteral("hello"));

    setting.set(QVariant(7));
    QCOMPARE(setting.value().toInt(), 7);
}

void Ut_MImSettings::testUnset()
{
    MImSettings setting(uniqueKey());

    setting.set(QVariant(1));
    QCOMPARE(setting.value().toInt(), 1);

    setting.unset();
    QCOMPARE(setting.value(QVariant(99)).toInt(), 99);
}

void Ut_MImSettings::testSetInvalidUnsets()
{
    MImSettings setting(uniqueKey());

    setting.set(QVariant(1));
    setting.set(QVariant());

    QCOMPARE(setting.value(QVariant(99)).toInt(), 99);
}

void Ut_MImSettings::testValueChangedOnSet()
{
    MImSettings setting(uniqueKey());
    QSignalSpy spy(&setting, SIGNAL(valueChanged()));

    setting.set(QVariant(1));

    QCOMPARE(spy.count(), 1);
}

void Ut_MImSettings::testNoValueChangedOnIdenticalSet()
{
    MImSettings setting(uniqueKey());

    setting.set(QVariant(1));

    QSignalSpy spy(&setting, SIGNAL(valueChanged()));
    setting.set(QVariant(1));

    QCOMPARE(spy.count(), 0);
}

void Ut_MImSettings::testValueChangedReachesEveryWatcher()
{
    const QString key = uniqueKey();

    MImSettings writer(key);
    MImSettings watcherA(key);
    MImSettings watcherB(key);

    QSignalSpy spyA(&watcherA, SIGNAL(valueChanged()));
    QSignalSpy spyB(&watcherB, SIGNAL(valueChanged()));

    writer.set(QVariant(QStringLiteral("x")));

    QCOMPARE(spyA.count(), 1);
    QCOMPARE(spyB.count(), 1);
    QCOMPARE(watcherA.value().toString(), QStringLiteral("x"));
}

void Ut_MImSettings::testWatcherStopsBeingNotifiedOnceDestroyed()
{
    const QString key = uniqueKey();

    MImSettings writer(key);
    MImSettings survivor(key);
    QSignalSpy spy(&survivor, SIGNAL(valueChanged()));

    {
        MImSettings shortLived(key);
        writer.set(QVariant(1));
        QCOMPARE(spy.count(), 1);
    }

    // If unregistering had left the destroyed watcher in the registry, this
    // write would notify through freed memory.
    writer.set(QVariant(2));
    QCOMPARE(spy.count(), 2);
}

void Ut_MImSettings::testLastWatcherDestroyedThenKeyUsedAgain()
{
    const QString key = uniqueKey();

    {
        MImSettings first(key);
        first.set(QVariant(1));
    }

    // The registry entry for the key is dropped once its list empties; using
    // the key again has to recreate it rather than look up a stale iterator.
    MImSettings second(key);
    QSignalSpy spy(&second, SIGNAL(valueChanged()));

    second.set(QVariant(2));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(second.value().toInt(), 2);
}

void Ut_MImSettings::testWatcherDestroyedFromWithinNotification()
{
    const QString key = uniqueKey();

    MImSettings writer(key);
    MImSettings *doomed = new MImSettings(key);
    MImSettings survivor(key);

    QSignalSpy spy(&survivor, SIGNAL(valueChanged()));

    // The backend takes a QPointer snapshot before delivering precisely so a
    // slot may delete another watcher for the same key mid-notification.
    connect(&survivor, &MImSettings::valueChanged, this, [&doomed]() {
        delete doomed;
        doomed = 0;
    });

    writer.set(QVariant(1));

    QCOMPARE(spy.count(), 1);
    QVERIFY(doomed == 0);

    // And the registry is still consistent afterwards.
    writer.set(QVariant(2));
    QCOMPARE(spy.count(), 2);
}

void Ut_MImSettings::testDistinctKeysAreIndependent()
{
    MImSettings a(uniqueKey());
    MImSettings b(uniqueKey());

    QSignalSpy spyB(&b, SIGNAL(valueChanged()));

    a.set(QVariant(1));

    QCOMPARE(spyB.count(), 0);
    QVERIFY(!b.value().isValid());
}

void Ut_MImSettings::testKeyAccessor()
{
    const QString key = uniqueKey();
    MImSettings setting(key);

    QCOMPARE(setting.key(), key);
}

void Ut_MImSettings::testListEntries()
{
    MImSettings parent(QStringLiteral("/maliit/tests/group"));
    MImSettings child(QStringLiteral("/maliit/tests/group/child"));

    child.set(QVariant(1));

    const QList<QString> entries = parent.listEntries();
    QVERIFY(entries.contains(QStringLiteral("/maliit/tests/group/child")));
}

QTEST_GUILESS_MAIN(Ut_MImSettings)
#include "ut_mimsettings.moc"
