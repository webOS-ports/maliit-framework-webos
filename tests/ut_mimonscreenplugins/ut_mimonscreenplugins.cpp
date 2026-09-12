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

// MImOnScreenPlugins decides which on-screen subviews the server will switch
// to. Its enabled/available bookkeeping is stored as "plugin:subview" strings
// in settings that a client can write, so the parsing has to survive junk, and
// isEnabled() has to keep meaning "enabled AND actually available".

#include "mimonscreenplugins.h"
#include "mimsubviewoverride.h"
#include "mimsettings.h"

#include <QtTest>

class Ut_MImOnScreenPlugins : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();

    void testSubViewEquality();
    void testSubViewDefaultId();

    void testNothingEnabledByDefault();
    void testEnabledSubViewsRoundTrip();
    void testEnabledSubViewsForPlugin();

    void testIsEnabledRequiresAvailability();
    void testIsEnabledAfterAvailabilityAppears();

    void testSubViewIdWithColons();
    void testMalformedSettingEntries_data();
    void testMalformedSettingEntries();

    void testActiveSubViewSignal();
    void testActiveSubViewNotReemittedWhenUnchanged();

    void testAllSubViewsEnabledRestoresPrevious();
    void testSubViewOverrideUndoesItselfOnDestruction();

private:
    QList<MImOnScreenPlugins::SubView> subViews(const QStringList &spec) const
    {
        QList<MImOnScreenPlugins::SubView> result;
        Q_FOREACH (const QString &entry, spec) {
            result.append(MImOnScreenPlugins::SubView(entry.section(':', 0, 0),
                                                      entry.section(':', 1, -1)));
        }
        return result;
    }
};

void Ut_MImOnScreenPlugins::initTestCase()
{
    MImSettings::setPreferredSettingsType(MImSettings::TemporarySettings);
}

void Ut_MImOnScreenPlugins::init()
{
    // MImOnScreenPlugins reads and writes two fixed settings keys, so every
    // instance in this process shares them. Clear both before each test so
    // the order tests happen to run in cannot change the result.
    MImSettings(QStringLiteral(MALIIT_CONFIG_ROOT "onscreen/enabled")).unset();
    MImSettings(QStringLiteral(MALIIT_CONFIG_ROOT "onscreen/active")).unset();
}

void Ut_MImOnScreenPlugins::testSubViewEquality()
{
    const MImOnScreenPlugins::SubView a(QStringLiteral("p"), QStringLiteral("en"));
    const MImOnScreenPlugins::SubView b(QStringLiteral("p"), QStringLiteral("en"));
    const MImOnScreenPlugins::SubView c(QStringLiteral("p"), QStringLiteral("nl"));
    const MImOnScreenPlugins::SubView d(QStringLiteral("q"), QStringLiteral("en"));

    QVERIFY(a == b);
    QVERIFY(!(a == c));
    QVERIFY(!(a == d));
}

void Ut_MImOnScreenPlugins::testSubViewDefaultId()
{
    // The second parameter used to default to a QString built from a null
    // char pointer; whatever it is spelled as, it has to be a null string.
    const MImOnScreenPlugins::SubView subView(QStringLiteral("plugin.so"));

    QCOMPARE(subView.plugin, QStringLiteral("plugin.so"));
    QVERIFY(subView.id.isEmpty());
}

void Ut_MImOnScreenPlugins::testNothingEnabledByDefault()
{
    MImOnScreenPlugins plugins;

    QVERIFY(!plugins.isEnabled(QStringLiteral("nothing.so")));
    QVERIFY(!plugins.isSubViewEnabled(
                MImOnScreenPlugins::SubView(QStringLiteral("nothing.so"), QStringLiteral("en"))));
}

void Ut_MImOnScreenPlugins::testEnabledSubViewsRoundTrip()
{
    MImOnScreenPlugins plugins;

    const QList<MImOnScreenPlugins::SubView> wanted =
        subViews(QStringList() << "a.so:en" << "a.so:nl" << "b.so:de");

    plugins.setEnabledSubViews(wanted);

    const QList<MImOnScreenPlugins::SubView> got = plugins.enabledSubViews();
    QCOMPARE(got.size(), 3);
    QVERIFY(got.contains(MImOnScreenPlugins::SubView(QStringLiteral("a.so"), QStringLiteral("en"))));
    QVERIFY(got.contains(MImOnScreenPlugins::SubView(QStringLiteral("a.so"), QStringLiteral("nl"))));
    QVERIFY(got.contains(MImOnScreenPlugins::SubView(QStringLiteral("b.so"), QStringLiteral("de"))));
}

void Ut_MImOnScreenPlugins::testEnabledSubViewsForPlugin()
{
    MImOnScreenPlugins plugins;

    plugins.setEnabledSubViews(subViews(QStringList() << "a.so:en" << "a.so:nl" << "b.so:de"));

    const QList<MImOnScreenPlugins::SubView> forA = plugins.enabledSubViews(QStringLiteral("a.so"));
    QCOMPARE(forA.size(), 2);

    const QList<MImOnScreenPlugins::SubView> forMissing =
        plugins.enabledSubViews(QStringLiteral("missing.so"));
    QVERIFY(forMissing.isEmpty());
}

void Ut_MImOnScreenPlugins::testIsEnabledRequiresAvailability()
{
    MImOnScreenPlugins plugins;

    // Enabled in the settings but no such subview is installed: the plugin
    // must not be reported as usable, or the server will try to switch to it.
    plugins.setEnabledSubViews(subViews(QStringList() << "ghost.so:en"));

    QVERIFY(plugins.isSubViewEnabled(
                MImOnScreenPlugins::SubView(QStringLiteral("ghost.so"), QStringLiteral("en"))));
    QVERIFY(!plugins.isEnabled(QStringLiteral("ghost.so")));
}

void Ut_MImOnScreenPlugins::testIsEnabledAfterAvailabilityAppears()
{
    MImOnScreenPlugins plugins;

    plugins.setEnabledSubViews(subViews(QStringList() << "real.so:en"));
    QVERIFY(!plugins.isEnabled(QStringLiteral("real.so")));

    plugins.updateAvailableSubViews(subViews(QStringList() << "real.so:en"));
    QVERIFY(plugins.isEnabled(QStringLiteral("real.so")));
}

void Ut_MImOnScreenPlugins::testSubViewIdWithColons()
{
    MImOnScreenPlugins plugins;

    // The setting format is "plugin:subview"; the plugin name is everything
    // before the first colon and the subview id is the rest, so an id that
    // contains a colon has to survive the round trip intact.
    const MImOnScreenPlugins::SubView subView(QStringLiteral("p.so"),
                                              QStringLiteral("en:GB:extra"));
    plugins.setEnabledSubViews(QList<MImOnScreenPlugins::SubView>() << subView);

    const QList<MImOnScreenPlugins::SubView> got = plugins.enabledSubViews();
    QCOMPARE(got.size(), 1);
    QCOMPARE(got.first().plugin, QStringLiteral("p.so"));
    QCOMPARE(got.first().id, QStringLiteral("en:GB:extra"));
}

void Ut_MImOnScreenPlugins::testMalformedSettingEntries_data()
{
    QTest::addColumn<QString>("entry");
    QTest::addColumn<QString>("plugin");
    QTest::addColumn<QString>("id");

    QTest::newRow("no colon")       << "justaplugin" << "justaplugin" << "";
    QTest::newRow("trailing colon") << "plugin:"     << "plugin"      << "";
    QTest::newRow("leading colon")  << ":subview"    << ""            << "subview";
    QTest::newRow("empty")          << ""            << ""            << "";
    QTest::newRow("only colon")     << ":"           << ""            << "";
}

void Ut_MImOnScreenPlugins::testMalformedSettingEntries()
{
    QFETCH(QString, entry);
    QFETCH(QString, plugin);
    QFETCH(QString, id);

    // These come out of a setting a client can write, so the parse must not
    // crash or drop entries - whatever it produces has to be well defined.
    MImSettings raw(QStringLiteral(MALIIT_CONFIG_ROOT "onscreen/enabled"));
    raw.set(QVariant(QStringList() << entry));

    MImOnScreenPlugins plugins;
    const QList<MImOnScreenPlugins::SubView> got = plugins.enabledSubViews();

    QCOMPARE(got.size(), 1);
    QCOMPARE(got.first().plugin, plugin);
    QCOMPARE(got.first().id, id);
}

void Ut_MImOnScreenPlugins::testActiveSubViewSignal()
{
    MImOnScreenPlugins plugins;
    QSignalSpy spy(&plugins, SIGNAL(activeSubViewChanged()));

    plugins.setActiveSubView(
        MImOnScreenPlugins::SubView(QStringLiteral("a.so"), QStringLiteral("en")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(plugins.activeSubView().plugin, QStringLiteral("a.so"));
    QCOMPARE(plugins.activeSubView().id, QStringLiteral("en"));
}

void Ut_MImOnScreenPlugins::testActiveSubViewNotReemittedWhenUnchanged()
{
    MImOnScreenPlugins plugins;

    const MImOnScreenPlugins::SubView subView(QStringLiteral("a.so"), QStringLiteral("en"));
    plugins.setActiveSubView(subView);

    QSignalSpy spy(&plugins, SIGNAL(activeSubViewChanged()));
    plugins.setActiveSubView(subView);

    QCOMPARE(spy.count(), 0);
}

void Ut_MImOnScreenPlugins::testAllSubViewsEnabledRestoresPrevious()
{
    MImOnScreenPlugins plugins;

    plugins.updateAvailableSubViews(subViews(QStringList() << "a.so:en" << "a.so:nl" << "b.so:de"));
    plugins.setEnabledSubViews(subViews(QStringList() << "a.so:en"));

    plugins.setAllSubViewsEnabled(true);
    QCOMPARE(plugins.enabledSubViews().size(), 3);

    plugins.setAllSubViewsEnabled(false);
    const QList<MImOnScreenPlugins::SubView> restored = plugins.enabledSubViews();
    QVERIFY(restored.contains(
                MImOnScreenPlugins::SubView(QStringLiteral("a.so"), QStringLiteral("en"))));
    QVERIFY(!restored.contains(
                MImOnScreenPlugins::SubView(QStringLiteral("b.so"), QStringLiteral("de"))));
}

void Ut_MImOnScreenPlugins::testSubViewOverrideUndoesItselfOnDestruction()
{
    MImOnScreenPlugins plugins;

    plugins.updateAvailableSubViews(subViews(QStringList() << "a.so:en" << "b.so:de"));
    plugins.setEnabledSubViews(subViews(QStringList() << "a.so:en"));

    {
        MImSubViewOverride override(&plugins);
        plugins.setAllSubViewsEnabled(true);
        QCOMPARE(plugins.enabledSubViews().size(), 2);
    }

    // The handle exists so an attribute extension going away cannot leave all
    // subviews permanently enabled.
    QCOMPARE(plugins.enabledSubViews().size(), 1);
}

QTEST_GUILESS_MAIN(Ut_MImOnScreenPlugins)
#include "ut_mimonscreenplugins.moc"
