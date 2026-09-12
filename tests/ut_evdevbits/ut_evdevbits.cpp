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

// MaliitServer used to abort on startup with a stack smash because the
// EVIOCGBIT destinations were sized from the bit count rather than from what
// the kernel actually copies out. The sizing rule now lives in a header, so
// it can be asserted here - at compile time, on whichever architecture is
// being built - instead of being discovered by a crash on a device.

#include "mimevdevbits.h"

#include <linux/input.h>

#include <QtTest>

namespace {

//! Bytes bits_to_user() in drivers/input/evdev.c will copy for \a maxbit when
//! the caller offers \a maxlen bytes. Mirrors the kernel exactly.
size_t kernelBytesCopied(size_t maxbit, size_t maxlen)
{
    const size_t bitsPerLong = sizeof(long) * 8;
    const size_t longs = (maxbit + bitsPerLong - 1) / bitsPerLong;
    const size_t len = longs * sizeof(long);

    return len > maxlen ? maxlen : len;
}

} // namespace

// The invariant the crash came down to: whatever the kernel is willing to
// write for a given ioctl has to fit in the buffer we hand it. Checked at
// compile time so a wrong answer cannot be built at all.
static_assert(EVDEV_BITS_BUFSIZE(EV_MAX) >= sizeof(long),
              "an evdev bitmap buffer is never smaller than one long");
static_assert(EVDEV_BITS_BUFSIZE(EV_MAX) % sizeof(long) == 0,
              "evdev bitmap buffers are a whole number of longs");
static_assert(EVDEV_BITS_BUFSIZE(SW_MAX) % sizeof(long) == 0,
              "evdev bitmap buffers are a whole number of longs");
static_assert(EVDEV_BITS_BUFSIZE(EV_MAX) >= BITS2BYTES(EV_MAX),
              "rounding up to longs never loses capacity");
static_assert(EVDEV_BITS_BUFSIZE(SW_MAX) >= BITS2BYTES(SW_MAX),
              "rounding up to longs never loses capacity");

class Ut_EvdevBits : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBits2Bytes_data();
    void testBits2Bytes();

    void testBufsizeIsWholeLongs_data();
    void testBufsizeIsWholeLongs();

    void testBufferHoldsWhatTheKernelWrites_data();
    void testBufferHoldsWhatTheKernelWrites();

    void testTheRegressionCase();

    void testTestBit_data();
    void testTestBit();

    void testTestBitAcrossByteBoundaries();
};

void Ut_EvdevBits::testBits2Bytes_data()
{
    QTest::addColumn<int>("bits");
    QTest::addColumn<int>("bytes");

    QTest::newRow("1")  <<  1 << 1;
    QTest::newRow("8")  <<  8 << 1;
    QTest::newRow("9")  <<  9 << 2;
    QTest::newRow("16") << 16 << 2;
    QTest::newRow("17") << 17 << 3;
    QTest::newRow("31") << 31 << 4;
    QTest::newRow("32") << 32 << 4;
    QTest::newRow("33") << 33 << 5;
}

void Ut_EvdevBits::testBits2Bytes()
{
    QFETCH(int, bits);
    QFETCH(int, bytes);

    QCOMPARE(static_cast<int>(BITS2BYTES(bits)), bytes);
}

void Ut_EvdevBits::testBufsizeIsWholeLongs_data()
{
    QTest::addColumn<int>("bits");

    QTest::newRow("EV_MAX") << static_cast<int>(EV_MAX);
    QTest::newRow("SW_MAX") << static_cast<int>(SW_MAX);
    QTest::newRow("KEY_MAX") << static_cast<int>(KEY_MAX);
    QTest::newRow("1") << 1;
    QTest::newRow("64") << 64;
    QTest::newRow("65") << 65;
}

void Ut_EvdevBits::testBufsizeIsWholeLongs()
{
    QFETCH(int, bits);

    const size_t size = EVDEV_BITS_BUFSIZE(bits);
    const size_t needed = static_cast<size_t>(BITS2BYTES(bits));

    QCOMPARE(size % sizeof(long), static_cast<size_t>(0));
    QVERIFY(size >= needed);
    // Never over-allocates by more than the rounding itself.
    QVERIFY(size - needed < sizeof(long));
}

void Ut_EvdevBits::testBufferHoldsWhatTheKernelWrites_data()
{
    QTest::addColumn<int>("bufferBits");
    QTest::addColumn<int>("ioctlMaxbit");
    QTest::addColumn<int>("ioctlLen");

    // The three calls tryEvdevDevice() makes, with the buffer size and the
    // ioctl arguments the code actually passes.
    QTest::newRow("EVIOCGBIT(0, EV_MAX)")
        << static_cast<int>(EV_MAX) << static_cast<int>(EV_MAX) << static_cast<int>(EV_MAX);
    QTest::newRow("EVIOCGBIT(EV_SW, SW_CNT)")
        << static_cast<int>(EV_MAX) << static_cast<int>(SW_CNT) << static_cast<int>(SW_CNT);
    QTest::newRow("EVIOCGSW(SW_MAX)")
        << static_cast<int>(SW_MAX) << static_cast<int>(SW_MAX) << static_cast<int>(SW_MAX);
}

void Ut_EvdevBits::testBufferHoldsWhatTheKernelWrites()
{
    QFETCH(int, bufferBits);
    QFETCH(int, ioctlMaxbit);
    QFETCH(int, ioctlLen);

    const size_t buffer = EVDEV_BITS_BUFSIZE(bufferBits);
    const size_t written = kernelBytesCopied(static_cast<size_t>(ioctlMaxbit),
                                             static_cast<size_t>(ioctlLen));

    QVERIFY2(written <= buffer,
             qPrintable(QString("kernel writes %1 bytes into a %2 byte buffer")
                        .arg(written).arg(buffer)));
}

void Ut_EvdevBits::testTheRegressionCase()
{
    // On a 64-bit kernel EVIOCGBIT(0, EV_MAX) copies 8 bytes while
    // BITS2BYTES(EV_MAX) is 4 - the exact four-byte overflow that tripped the
    // stack protector. Assert that the old sizing really was too small there,
    // so this test would have caught it, and that the new one is not.
    if (sizeof(long) == 8) {
        QCOMPARE(static_cast<size_t>(BITS2BYTES(EV_MAX)), static_cast<size_t>(4));
        QCOMPARE(kernelBytesCopied(EV_MAX, EV_MAX), static_cast<size_t>(8));
        QVERIFY(kernelBytesCopied(EV_MAX, EV_MAX) > static_cast<size_t>(BITS2BYTES(EV_MAX)));
    }

    QVERIFY(kernelBytesCopied(EV_MAX, EV_MAX) <= static_cast<size_t>(EVDEV_BITS_BUFSIZE(EV_MAX)));
    QVERIFY(kernelBytesCopied(SW_CNT, SW_CNT) <= static_cast<size_t>(EVDEV_BITS_BUFSIZE(EV_MAX)));
    QVERIFY(kernelBytesCopied(SW_MAX, SW_MAX) <= static_cast<size_t>(EVDEV_BITS_BUFSIZE(SW_MAX)));
}

void Ut_EvdevBits::testTestBit_data()
{
    QTest::addColumn<int>("bit");

    for (int bit = 0; bit < 24; ++bit) {
        QTest::newRow(qPrintable(QString::number(bit))) << bit;
    }
}

void Ut_EvdevBits::testTestBit()
{
    QFETCH(int, bit);

    unsigned char array[EVDEV_BITS_BUFSIZE(EV_MAX)];
    memset(array, 0, sizeof(array));

    QVERIFY(!TEST_BIT(bit, array));
    array[bit / 8] |= (1 << (bit % 8));
    QVERIFY(TEST_BIT(bit, array));
}

void Ut_EvdevBits::testTestBitAcrossByteBoundaries()
{
    // TEST_BIT indexes bytes, so it only works on a byte-addressed array.
    // SW_TABLET_MODE lives in the first byte, but the switch bitmap is read
    // as a whole and a caller could reasonably ask about a later switch.
    unsigned char array[EVDEV_BITS_BUFSIZE(SW_MAX)];
    memset(array, 0, sizeof(array));

    array[1] = 0x01; // bit 8
    QVERIFY(TEST_BIT(8, array));
    QVERIFY(!TEST_BIT(7, array));
    QVERIFY(!TEST_BIT(9, array));

    memset(array, 0, sizeof(array));
    array[0] = (1 << SW_TABLET_MODE);
    QVERIFY(TEST_BIT(SW_TABLET_MODE, array));
}

QTEST_GUILESS_MAIN(Ut_EvdevBits)
#include "ut_evdevbits.moc"
