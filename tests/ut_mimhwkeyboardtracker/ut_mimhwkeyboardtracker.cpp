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

// The transitions, which are what the rest of the stack reacts to. A keyboard
// that goes away mid-sentence - a PineTab2's, a USB or a Bluetooth one - has to
// bring the on-screen keyboard back for the field that is focused right now, so
// stateChanged() has to fire exactly when the answer moves and not otherwise.
//
// The device list is driven through MALIIT_HW_INPUT_DEVICES rather than by
// plugging things in, and refresh() stands in for the udev event.

#include "mimhwkeyboardtracker.h"
#include "mimkeyboardkind.h"

#include <QtTest>

namespace {

const char *const KeyboardBlock =
    "N: Name=\"AT Translated Set 2 keyboard\"\n"
    "S: Sysfs=/devices/platform/i8042/serio0/input/input2\n"
    "H: Handlers=sysrq kbd event2 leds \n"
    "B: KEY=402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe\n"
    "\n";

const char *const PowerButtonBlock =
    "N: Name=\"Power Button\"\n"
    "S: Sysfs=/devices/LNXSYSTM:00/LNXPWRBN:00/input/input0\n"
    "H: Handlers=kbd event0 \n"
    "B: KEY=10000000000000 0\n"
    "\n";

const char *const KeypadBlock =
    "N: Name=\"mtk-tpd-kpd\"\n"
    "S: Sysfs=/devices/platform/mtk-kpd/input/input3\n"
    "H: Handlers=kbd event3 \n"
    "B: KEY=800 0 0 0 0 0 0 0 0 8 0 0 0 1c0000 0 0 ffc\n"
    "\n";

const char *const VirtualKeyboardBlock =
    "N: Name=\"RustDesk UInput Keyboard\"\n"
    "S: Sysfs=/devices/virtual/input/input481\n"
    "H: Handlers=sysrq kbd event9 leds \n"
    "B: KEY=402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe\n"
    "\n";

} // namespace

class Ut_MImHwKeyboardTracker : public QObject
{
    Q_OBJECT

public:
    //! Replaces the device list the next scan will read.
    void setDevices(const QByteArray &contents);

private Q_SLOTS:
    void init();
    void cleanup();

    void testNoKeyboard();
    void testKeyboard();
    void testVirtualKeyboardDoesNotCount();

    void testKeyboardArriving();
    void testKeyboardLeaving();
    void testRescanWithNoChangeIsQuiet();

    void testKeypadIsNotCountedByDefault();
    void testKeypadCountedWhenAccepted();
    void testAcceptingKeypadsReportsTheChange();

    void testForcedOn();
    void testForcedOff();

private:
    QTemporaryFile *m_devices = nullptr;
};

void Ut_MImHwKeyboardTracker::setDevices(const QByteArray &contents)
{
    m_devices->resize(0);
    QCOMPARE(m_devices->write(contents), qint64(contents.size()));
    m_devices->flush();
}

void Ut_MImHwKeyboardTracker::init()
{
    m_devices = new QTemporaryFile;
    QVERIFY(m_devices->open());
    qputenv("MALIIT_HW_INPUT_DEVICES", m_devices->fileName().toLocal8Bit());
}

void Ut_MImHwKeyboardTracker::cleanup()
{
    qunsetenv("MALIIT_HW_INPUT_DEVICES");
    delete m_devices;
    m_devices = nullptr;
}

void Ut_MImHwKeyboardTracker::testNoKeyboard()
{
    setDevices(PowerButtonBlock);

    const MImHwKeyboardTracker tracker;

    QVERIFY(!tracker.isPresent());
    QVERIFY(!tracker.isOpen());
    QCOMPARE(tracker.attachedKinds(),
             MImKeyboard::KeyboardKinds(MImKeyboard::NotAKeyboard));
}

void Ut_MImHwKeyboardTracker::testKeyboard()
{
    setDevices(QByteArray(PowerButtonBlock) + KeyboardBlock);

    const MImHwKeyboardTracker tracker;

    QVERIFY(tracker.isPresent());
    QVERIFY(tracker.isOpen());
    QVERIFY(tracker.attachedKinds().testFlag(MImKeyboard::TextKeyboard));
}

void Ut_MImHwKeyboardTracker::testVirtualKeyboardDoesNotCount()
{
    setDevices(QByteArray(PowerButtonBlock) + VirtualKeyboardBlock);

    const MImHwKeyboardTracker tracker;

    QVERIFY(!tracker.isPresent());
    QVERIFY(!tracker.isOpen());
}

void Ut_MImHwKeyboardTracker::testKeyboardArriving()
{
    setDevices(PowerButtonBlock);

    MImHwKeyboardTracker tracker;
    QSignalSpy changed(&tracker, &MImHwKeyboardTracker::stateChanged);
    QVERIFY(changed.isValid());

    setDevices(QByteArray(PowerButtonBlock) + KeyboardBlock);
    tracker.refresh();

    QCOMPARE(changed.count(), 1);
    QVERIFY(tracker.isOpen());
}

void Ut_MImHwKeyboardTracker::testKeyboardLeaving()
{
    // The case that matters most: a detachable keyboard pulled off while a text
    // field has focus. Nothing else will tell the shell to put the on-screen
    // keyboard back.
    setDevices(QByteArray(PowerButtonBlock) + KeyboardBlock);

    MImHwKeyboardTracker tracker;
    QVERIFY(tracker.isOpen());

    QSignalSpy changed(&tracker, &MImHwKeyboardTracker::stateChanged);

    setDevices(PowerButtonBlock);
    tracker.refresh();

    QCOMPARE(changed.count(), 1);
    QVERIFY(!tracker.isOpen());
    QVERIFY(!tracker.isPresent());
}

void Ut_MImHwKeyboardTracker::testRescanWithNoChangeIsQuiet()
{
    // Every input device coming or going wakes a rescan, and most of them have
    // nothing to do with keyboards. A signal per event would restart the input
    // source handling for no reason.
    setDevices(QByteArray(PowerButtonBlock) + KeyboardBlock);

    MImHwKeyboardTracker tracker;
    QSignalSpy changed(&tracker, &MImHwKeyboardTracker::stateChanged);

    tracker.refresh();
    setDevices(QByteArray(PowerButtonBlock) + KeyboardBlock + KeypadBlock);
    tracker.refresh();

    QCOMPARE(changed.count(), 0);
    QVERIFY(tracker.isOpen());
}

void Ut_MImHwKeyboardTracker::testKeypadIsNotCountedByDefault()
{
    setDevices(KeypadBlock);

    const MImHwKeyboardTracker tracker;

    QVERIFY(tracker.attachedKinds().testFlag(MImKeyboard::TelephoneKeypad));
    QVERIFY(tracker.isPresent());
    QVERIFY(!tracker.isOpen());
}

void Ut_MImHwKeyboardTracker::testKeypadCountedWhenAccepted()
{
    setDevices(KeypadBlock);

    MImHwKeyboardTracker tracker;
    tracker.setAcceptedKinds(MImKeyboard::TextKeyboard
                             | MImKeyboard::TelephoneKeypad);

    QVERIFY(tracker.isOpen());
}

void Ut_MImHwKeyboardTracker::testAcceptingKeypadsReportsTheChange()
{
    setDevices(KeypadBlock);

    MImHwKeyboardTracker tracker;
    QSignalSpy changed(&tracker, &MImHwKeyboardTracker::stateChanged);

    tracker.setAcceptedKinds(MImKeyboard::TextKeyboard
                             | MImKeyboard::TelephoneKeypad);
    QCOMPARE(changed.count(), 1);

    // Setting the same policy again changes nothing and says nothing.
    tracker.setAcceptedKinds(MImKeyboard::TextKeyboard
                             | MImKeyboard::TelephoneKeypad);
    QCOMPARE(changed.count(), 1);
}

void Ut_MImHwKeyboardTracker::testForcedOn()
{
    setDevices(PowerButtonBlock);

    MImHwKeyboardTracker tracker;
    QVERIFY(!tracker.isOpen());

    QSignalSpy changed(&tracker, &MImHwKeyboardTracker::stateChanged);
    tracker.setForcedState(1);

    QCOMPARE(changed.count(), 1);
    QVERIFY(tracker.isPresent());
    QVERIFY(tracker.isOpen());
}

void Ut_MImHwKeyboardTracker::testForcedOff()
{
    setDevices(QByteArray(PowerButtonBlock) + KeyboardBlock);

    MImHwKeyboardTracker tracker;
    QVERIFY(tracker.isOpen());

    tracker.setForcedState(0);

    QVERIFY(!tracker.isOpen());
    QVERIFY(!tracker.isPresent());

    // Back to deciding from the hardware.
    tracker.setForcedState(-1);
    QVERIFY(tracker.isOpen());
}

QTEST_GUILESS_MAIN(Ut_MImHwKeyboardTracker)
#include "ut_mimhwkeyboardtracker.moc"
