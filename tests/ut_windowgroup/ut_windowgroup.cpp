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

// WindowGroup is the server's guard rail around plugin windows: a plugin is
// not trusted to only show itself when it is the active input method, and the
// input method area it reports is what the compositor uses to move the
// application out of the way. Both of those are enforced here rather than in
// the plugin, so both are worth pinning down.
//
// Runs on the offscreen platform plugin; see tests.pri.

#include "windowgroup.h"
#include "unknownplatform.h"
#include "abstractplatform.h"

#include <maliit/namespace.h>

#include <QWindow>
#include <QtTest>

namespace {

//! Records what the platform layer was asked to do.
class RecordingPlatform : public Maliit::AbstractPlatform
{
public:
    RecordingPlatform()
        : setupCount(0)
        , inputRegionCount(0)
        , applicationWindowCount(0)
        , lastApplicationWindowId(0)
    {}

    void setupInputPanel(QWindow *window, Maliit::Position position) override
    {
        Q_UNUSED(window);
        Q_UNUSED(position);
        ++setupCount;
    }

    void setInputRegion(QWindow *window, const QRegion &region) override
    {
        Q_UNUSED(window);
        ++inputRegionCount;
        lastInputRegion = region;
    }

    void setApplicationWindow(QWindow *window, WId appWindowId) override
    {
        Q_UNUSED(window);
        ++applicationWindowCount;
        lastApplicationWindowId = appWindowId;
    }

    int setupCount;
    int inputRegionCount;
    int applicationWindowCount;
    WId lastApplicationWindowId;
    QRegion lastInputRegion;
};

} // namespace

class Ut_WindowGroup : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSetupWindowReachesPlatform();
    void testSetupWindowIsIdempotent();
    void testNullWindowIsIgnored();
    void testChildWindowWithUnregisteredParentIsRejected();

    void testWindowFlagsAreForced();

    void testInputMethodAreaIsEmptyWhileHidden();
    void testInputMethodAreaFollowsVisibility();
    void testInputMethodAreaIsNotReemittedUnchanged();
    void testInputMethodAreaIsTranslatedByWindowPosition();

    void testInactiveGroupHidesAWindowThatShowsItself();
    void testActiveGroupLeavesVisibleWindowAlone();

    void testDeactivateImmediateHides();
    void testDeactivateDelayedDoesNotHideAtOnce();
    void testActivateCancelsPendingHide();

    void testSetScreenRegionDefaultsToFirstWindow();
    void testSetScreenRegionWithNoWindows();
    void testSetApplicationWindowSkipsChildren();

private:
    QWindow *makeWindow(Maliit::WindowGroup *group, const QRect &geometry)
    {
        QWindow *window = new QWindow;
        window->setGeometry(geometry);
        group->setupWindow(window, Maliit::PositionCenterBottom);
        return window;
    }
};

void Ut_WindowGroup::testSetupWindowReachesPlatform()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(new QWindow);
    group.setupWindow(window.data(), Maliit::PositionCenterBottom);

    QCOMPARE(platform->setupCount, 1);
}

void Ut_WindowGroup::testSetupWindowIsIdempotent()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(new QWindow);
    group.setupWindow(window.data(), Maliit::PositionCenterBottom);
    group.setupWindow(window.data(), Maliit::PositionCenterBottom);

    // A plugin re-registering the same window must not stack a second set of
    // signal connections onto it.
    QCOMPARE(platform->setupCount, 1);
}

void Ut_WindowGroup::testNullWindowIsIgnored()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    group.setupWindow(0, Maliit::PositionCenterBottom);

    QCOMPARE(platform->setupCount, 0);
}

void Ut_WindowGroup::testChildWindowWithUnregisteredParentIsRejected()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> parent(new QWindow);
    QScopedPointer<QWindow> child(new QWindow(parent.data()));

    group.setupWindow(child.data(), Maliit::PositionCenterBottom);

    QCOMPARE(platform->setupCount, 0);
}

void Ut_WindowGroup::testWindowFlagsAreForced()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(new QWindow);
    group.setupWindow(window.data(), Maliit::PositionCenterBottom);

    // An input method window must never be framed nor take focus, whatever
    // the plugin asked for.
    QVERIFY(window->flags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(window->flags().testFlag(Qt::WindowStaysOnTopHint));
    QVERIFY(window->flags().testFlag(Qt::WindowDoesNotAcceptFocus));
}

void Ut_WindowGroup::testInputMethodAreaIsEmptyWhileHidden()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(0, 0, 100, 50)));

    QSignalSpy spy(&group, SIGNAL(inputMethodAreaChanged(QRegion)));
    group.activate();
    group.setInputMethodArea(QRegion(0, 0, 100, 50), window.data());

    // The window was never shown, so there is no area to report. QSignalSpy is
    // not copyable, so index rather than iterate it.
    for (int i = 0; i < spy.count(); ++i) {
        QVERIFY(spy.at(i).at(0).value<QRegion>().isEmpty());
    }
}

void Ut_WindowGroup::testInputMethodAreaFollowsVisibility()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(0, 0, 100, 50)));

    group.activate();
    window->show();
    QTRY_VERIFY(window->isVisible());

    QSignalSpy spy(&group, SIGNAL(inputMethodAreaChanged(QRegion)));
    group.setInputMethodArea(QRegion(0, 0, 100, 50), window.data());

    QVERIFY(spy.count() >= 1);
    QCOMPARE(spy.last().at(0).value<QRegion>(), QRegion(0, 0, 100, 50));

    // Hiding the group must retract the area, or the application stays
    // squeezed against a keyboard that is no longer there.
    QSignalSpy hideSpy(&group, SIGNAL(inputMethodAreaChanged(QRegion)));
    group.deactivate(Maliit::WindowGroup::HideImmediate);

    QVERIFY(hideSpy.count() >= 1);
    QVERIFY(hideSpy.last().at(0).value<QRegion>().isEmpty());
}

void Ut_WindowGroup::testInputMethodAreaIsNotReemittedUnchanged()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(0, 0, 100, 50)));

    group.activate();
    window->show();
    QTRY_VERIFY(window->isVisible());
    group.setInputMethodArea(QRegion(0, 0, 100, 50), window.data());

    QSignalSpy spy(&group, SIGNAL(inputMethodAreaChanged(QRegion)));
    group.setInputMethodArea(QRegion(0, 0, 100, 50), window.data());

    QCOMPARE(spy.count(), 0);
}

void Ut_WindowGroup::testInputMethodAreaIsTranslatedByWindowPosition()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(10, 20, 100, 50)));

    group.activate();
    window->show();
    QTRY_VERIFY(window->isVisible());

    QSignalSpy spy(&group, SIGNAL(inputMethodAreaChanged(QRegion)));
    // The plugin reports the area in window coordinates; the compositor needs
    // it in screen coordinates.
    group.setInputMethodArea(QRegion(0, 0, 100, 50), window.data());

    QVERIFY(spy.count() >= 1);
    const QRegion reported = spy.last().at(0).value<QRegion>();
    QCOMPARE(reported.boundingRect().topLeft(), window->position());
}

void Ut_WindowGroup::testInactiveGroupHidesAWindowThatShowsItself()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(0, 0, 100, 50)));

    // The group was never activated: a plugin showing its window anyway is
    // misbehaving and gets overruled.
    window->show();

    QTRY_VERIFY(!window->isVisible());
}

void Ut_WindowGroup::testActiveGroupLeavesVisibleWindowAlone()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(0, 0, 100, 50)));

    group.activate();
    window->show();

    QTRY_VERIFY(window->isVisible());
}

void Ut_WindowGroup::testDeactivateImmediateHides()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(0, 0, 100, 50)));

    group.activate();
    window->show();
    QTRY_VERIFY(window->isVisible());

    group.deactivate(Maliit::WindowGroup::HideImmediate);
    QVERIFY(!window->isVisible());
}

void Ut_WindowGroup::testDeactivateDelayedDoesNotHideAtOnce()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(0, 0, 100, 50)));

    group.activate();
    window->show();
    QTRY_VERIFY(window->isVisible());

    // Delayed hiding exists so a switch between two plugins does not flash
    // the application's layout; the window stays up until the timer fires.
    group.deactivate(Maliit::WindowGroup::HideDelayed);
    QVERIFY(window->isVisible());
}

void Ut_WindowGroup::testActivateCancelsPendingHide()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(0, 0, 100, 50)));

    group.activate();
    window->show();
    QTRY_VERIFY(window->isVisible());

    group.deactivate(Maliit::WindowGroup::HideDelayed);
    group.activate();

    // Re-activating within the delay must cancel the hide outright, not let
    // it fire later and pull the keyboard out from under the user.
    QTest::qWait(100);
    QVERIFY(window->isVisible());
}

void Ut_WindowGroup::testSetScreenRegionDefaultsToFirstWindow()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> window(makeWindow(&group, QRect(0, 0, 100, 50)));

    group.setScreenRegion(QRegion(1, 2, 3, 4), 0);

    QCOMPARE(platform->inputRegionCount, 1);
    QCOMPARE(platform->lastInputRegion, QRegion(1, 2, 3, 4));
}

void Ut_WindowGroup::testSetScreenRegionWithNoWindows()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    // No window registered and none passed: must not crash, and the platform
    // still gets told so it can decide.
    group.setScreenRegion(QRegion(1, 2, 3, 4), 0);

    QCOMPARE(platform->inputRegionCount, 1);
}

void Ut_WindowGroup::testSetApplicationWindowSkipsChildren()
{
    QSharedPointer<RecordingPlatform> platform(new RecordingPlatform);
    Maliit::WindowGroup group(platform);

    QScopedPointer<QWindow> parent(new QWindow);
    group.setupWindow(parent.data(), Maliit::PositionCenterBottom);

    QScopedPointer<QWindow> child(new QWindow(parent.data()));
    group.setupWindow(child.data(), Maliit::PositionCenterBottom);

    group.setApplicationWindow(4242);

    // Only top level windows are associated with the application window.
    QCOMPARE(platform->applicationWindowCount, 1);
    QCOMPARE(platform->lastApplicationWindowId, static_cast<WId>(4242));
}

QTEST_MAIN(Ut_WindowGroup)
#include "ut_windowgroup.moc"
