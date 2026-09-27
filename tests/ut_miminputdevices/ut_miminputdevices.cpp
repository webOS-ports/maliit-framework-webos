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

// The bitmaps in here are real: copied from /proc/bus/input/devices on the
// phones concerned. The word-width rule in particular cannot be checked any
// other way - the kernel pads the words on one device and not on another, and
// getting the width wrong shifts every scancode without failing to parse.

#include "miminputdevices.h"

#include <linux/input.h>

#include <QtTest>

using namespace MImKeyboard;

namespace {

//! Writes \a contents to a file the test then reads back, standing in for
//! /proc/bus/input/devices.
class Capture
{
public:
    explicit Capture(const QByteArray &contents)
    {
        QVERIFY2(m_file.open(), "could not open a temporary file");
        m_file.write(contents);
        m_file.flush();
    }

    QString path() const { return m_file.fileName(); }

private:
    QTemporaryFile m_file;
};

} // namespace

class Ut_MImInputDevices : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testMissingFileIsNoDevices();
    void testEmptyFileIsNoDevices();

    void testReadsNameKeysSwitchesAndHandlers();
    void testSeveralDevices();
    void testLinesBeforeAnyNameAreIgnored();

    void testPaddedWordsFromAnMp01();
    void testUnpaddedWordsFromAMindset();
    void testWordWidthPlacesHighCodes();

    void testEventNode();
    void testEventNodeWithoutOne();
    void testEventNodeIgnoresLookalikes();

    void testVirtualDevice();
    void testBluetoothKeyboardIsNotVirtual();

    void testEnvironmentOverride();
};

void Ut_MImInputDevices::testMissingFileIsNoDevices()
{
    QVERIFY(readInputDevices(QStringLiteral("/nonexistent/input/devices")).isEmpty());
}

void Ut_MImInputDevices::testEmptyFileIsNoDevices()
{
    const Capture capture("");

    QVERIFY(readInputDevices(capture.path()).isEmpty());
}

void Ut_MImInputDevices::testReadsNameKeysSwitchesAndHandlers()
{
    // A lid switch, as an x86 laptop reports it.
    const Capture capture(
        "I: Bus=0019 Vendor=0000 Product=0005 Version=0000\n"
        "N: Name=\"Lid Switch\"\n"
        "P: Phys=PNP0C0D/button/input0\n"
        "S: Sysfs=/devices/LNXSYSTM:00/LNXSYBUS:00/PNP0C0D:00/input/input1\n"
        "U: Uniq=\n"
        "H: Handlers=event1 \n"
        "B: PROP=0\n"
        "B: EV=21\n"
        "B: SW=1\n"
        "\n");

    const QList<InputDevice> devices(readInputDevices(capture.path()));

    QCOMPARE(devices.size(), 1);
    QCOMPARE(devices.at(0).name, QStringLiteral("Lid Switch"));
    QCOMPARE(devices.at(0).handlers, QStringList() << QStringLiteral("event1"));
    QCOMPARE(devices.at(0).sysfsPath,
             QStringLiteral("/devices/LNXSYSTM:00/LNXSYBUS:00/PNP0C0D:00/input/input1"));
    QVERIFY(devices.at(0).keys.isEmpty());
    QVERIFY(devices.at(0).switches.advertises(SW_LID));
    QVERIFY(!devices.at(0).switches.advertises(SW_TABLET_MODE));
}

void Ut_MImInputDevices::testSeveralDevices()
{
    const Capture capture(
        "N: Name=\"Power Button\"\n"
        "H: Handlers=kbd event0 \n"
        "B: KEY=10000000000000 0\n"
        "\n"
        "N: Name=\"AT Translated Set 2 keyboard\"\n"
        "S: Sysfs=/devices/platform/i8042/serio0/input/input2\n"
        "H: Handlers=sysrq kbd event2 leds \n"
        "B: KEY=402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe\n"
        "\n");

    const QList<InputDevice> devices(readInputDevices(capture.path()));

    QCOMPARE(devices.size(), 2);
    QCOMPARE(devices.at(0).name, QStringLiteral("Power Button"));
    QCOMPARE(devices.at(1).name, QStringLiteral("AT Translated Set 2 keyboard"));

    // Each device keeps its own bitmap; the second must not inherit the first's.
    QVERIFY(devices.at(1).keys.advertises(KEY_A));
    QVERIFY(!devices.at(0).keys.advertises(KEY_A));
    QVERIFY(devices.at(0).keys.advertises(KEY_POWER));
}

void Ut_MImInputDevices::testLinesBeforeAnyNameAreIgnored()
{
    // The file opens with an "I:" line, and a bitmap with no device to attach to
    // must not crash or invent one.
    const Capture capture(
        "B: KEY=fffffffffffffffe\n"
        "H: Handlers=event0\n"
        "\n"
        "N: Name=\"Real\"\n"
        "B: KEY=1\n"
        "\n");

    const QList<InputDevice> devices(readInputDevices(capture.path()));

    QCOMPARE(devices.size(), 1);
    QCOMPARE(devices.at(0).name, QStringLiteral("Real"));
}

void Ut_MImInputDevices::testPaddedWordsFromAnMp01()
{
    // Every word fills its width here, so the longest is 16 digits and the
    // width is unambiguous.
    const Capture capture(
        "N: Name=\"mtk-kpd\"\n"
        "B: KEY=1000000000007 ff9f207ac14057ff febeffdfffefffff fffffffffffffffe\n"
        "\n");

    const QList<InputDevice> devices(readInputDevices(capture.path()));

    QCOMPARE(devices.size(), 1);
    QCOMPARE(devices.at(0).keys.wordBits(), 64);

    // Word 0 is fffffffffffffffe: every code 1..63 set, 0 clear.
    QVERIFY(!devices.at(0).keys.advertises(0));
    QVERIFY(devices.at(0).keys.advertises(KEY_ESC));
    QVERIFY(devices.at(0).keys.advertises(KEY_A));
    QVERIFY(devices.at(0).keys.advertises(KEY_M));

    // Word 3 is 1000000000007: bits 0, 1, 2 and 48, i.e. codes 192..194 and 240.
    QVERIFY(devices.at(0).keys.advertises(192));
    QVERIFY(devices.at(0).keys.advertises(194));
    QVERIFY(!devices.at(0).keys.advertises(195));
    QVERIFY(devices.at(0).keys.advertises(240));
}

void Ut_MImInputDevices::testUnpaddedWordsFromAMindset()
{
    // Nothing is padded here, so the width cannot be read off the longest word
    // and falls back to this process's own long - which belongs to the same
    // kernel that printed the line.
    const Capture capture(
        "N: Name=\"mtk-tpd-kpd\"\n"
        "B: KEY=800 0 0 0 0 0 0 0 0 8 0 0 0 1c0000 0 0 ffc\n"
        "\n");

    const QList<InputDevice> devices(readInputDevices(capture.path()));

    QCOMPARE(devices.size(), 1);
    QCOMPARE(devices.at(0).keys.wordBits(), int(sizeof(unsigned long) * 8));

    // Word 0 is ffc: codes 2..11, which is KEY_1..KEY_0 - the ten digits of a
    // telephone keypad and not one letter, which is what this device is.
    QVERIFY(!devices.at(0).keys.advertises(KEY_ESC));
    for (unsigned int code = KEY_1; code <= KEY_0; ++code)
        QVERIFY(devices.at(0).keys.advertises(code));
    QVERIFY(!devices.at(0).keys.advertises(KEY_A));
    QVERIFY(!devices.at(0).keys.advertises(KEY_Q));
}

void Ut_MImInputDevices::testWordWidthPlacesHighCodes()
{
    // The same bits read as 32-bit words and as 64-bit words put a code in
    // different places, which is exactly the failure the width rule prevents.
    const KeyBitmap wide(KeyBitmap::fromProcWords(
        QStringList() << QStringLiteral("0000000000000001")
                      << QStringLiteral("0000000000000000")));

    QCOMPARE(wide.wordBits(), 64);
    QVERIFY(wide.advertises(64));
    QVERIFY(!wide.advertises(32));

    const KeyBitmap narrow(KeyBitmap::fromProcWords(
        QStringList() << QStringLiteral("00000001") << QStringLiteral("00000000")));

    // Eight digits is ambiguous, so this falls back to the host's long.
    QCOMPARE(narrow.wordBits(), int(sizeof(unsigned long) * 8));
    QVERIFY(narrow.advertises(sizeof(unsigned long) * 8));
}

void Ut_MImInputDevices::testEventNode()
{
    InputDevice device;
    device.handlers = QStringList() << QStringLiteral("sysrq")
                                    << QStringLiteral("kbd")
                                    << QStringLiteral("event3")
                                    << QStringLiteral("leds");

    QCOMPARE(device.eventNode(), QStringLiteral("/dev/input/event3"));
}

void Ut_MImInputDevices::testEventNodeWithoutOne()
{
    InputDevice device;
    device.handlers = QStringList() << QStringLiteral("kbd");

    QVERIFY(device.eventNode().isEmpty());
}

void Ut_MImInputDevices::testEventNodeIgnoresLookalikes()
{
    InputDevice device;
    device.handlers = QStringList() << QStringLiteral("eventful")
                                    << QStringLiteral("event12");

    QCOMPARE(device.eventNode(), QStringLiteral("/dev/input/event12"));
}

void Ut_MImInputDevices::testVirtualDevice()
{
    // Seen in the wild: RustDesk registers a uinput keyboard advertising all 26
    // letters. It must not count as the user having a keyboard.
    const Capture capture(
        "I: Bus=0003 Vendor=1234 Product=5678 Version=0111\n"
        "N: Name=\"RustDesk UInput Keyboard\"\n"
        "P: Phys=\n"
        "S: Sysfs=/devices/virtual/input/input481\n"
        "H: Handlers=sysrq rfkill kbd event3 leds \n"
        "B: KEY=402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe\n"
        "\n");

    const QList<InputDevice> devices(readInputDevices(capture.path()));

    QCOMPARE(devices.size(), 1);
    QVERIFY(devices.at(0).keys.advertises(KEY_A));
    QVERIFY(devices.at(0).isVirtual());
}

void Ut_MImInputDevices::testBluetoothKeyboardIsNotVirtual()
{
    // A Bluetooth keyboard arrives through uhid, which is itself a virtual
    // device - but the keyboard hanging off it is real and has to count. Only
    // input devices registered with no parent at all are software's.
    const Capture capture(
        "N: Name=\"Keyboard K380\"\n"
        "S: Sysfs=/devices/virtual/misc/uhid/0005:046D:B342.0003/input/input44\n"
        "H: Handlers=sysrq kbd event15 leds \n"
        "B: KEY=e080ffdf01cfffff fffffffffffffffe\n"
        "\n");

    const QList<InputDevice> devices(readInputDevices(capture.path()));

    QCOMPARE(devices.size(), 1);
    QVERIFY(!devices.at(0).isVirtual());
}

void Ut_MImInputDevices::testEnvironmentOverride()
{
    const Capture capture(
        "N: Name=\"Captured\"\n"
        "B: KEY=1\n"
        "\n");

    qputenv("MALIIT_HW_INPUT_DEVICES", capture.path().toLocal8Bit());

    const QList<InputDevice> devices(readInputDevices());

    qunsetenv("MALIIT_HW_INPUT_DEVICES");

    QCOMPARE(devices.size(), 1);
    QCOMPARE(devices.at(0).name, QStringLiteral("Captured"));
}

QTEST_GUILESS_MAIN(Ut_MImInputDevices)
#include "ut_miminputdevices.moc"
