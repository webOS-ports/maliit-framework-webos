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

// Whether an input device is a keyboard decides whether the on-screen keyboard
// appears at all, so getting it wrong is not cosmetic: a device wrongly called a
// keyboard leaves a phone with no way to type. The rules are checked here
// against bitmaps built by hand, rather than one phone at a time.

#include "miminputdevices.h"
#include "mimkeyboardkind.h"

#include <linux/input.h>

#include <QtTest>

using namespace MImKeyboard;

namespace {

//! The 26 letters, in the order evdev numbers them.
const unsigned int Letters[] = {
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P,
    KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L,
    KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
};

const int LetterCount = int(sizeof(Letters) / sizeof(Letters[0]));

/*! \brief A bitmap advertising exactly \a codes, in the kernel's own text form.
 *
 * Written out as 64-bit words zero-padded to their full width, so the parser
 * reads the same width back whatever this test is built on - a 32-bit host would
 * otherwise infer 32 from short words and index differently.
 */
KeyBitmap bitmapOf(const QList<unsigned int> &codes)
{
    QList<quint64> words;

    for (unsigned int code : codes) {
        const int index = int(code) / 64;

        while (words.size() <= index)
            words.append(0);

        words[index] |= (quint64(1) << (code % 64));
    }

    QStringList text;
    for (int i = words.size() - 1; i >= 0; --i)
        text << QStringLiteral("%1").arg(words.at(i), 16, 16, QLatin1Char('0'));

    return KeyBitmap::fromProcWords(text);
}

//! The first \a count letters, so a keyboard just over and just under the
//! threshold can both be built.
KeyBitmap withLetters(int count)
{
    QList<unsigned int> codes;

    for (int i = 0; i < count && i < LetterCount; ++i)
        codes << Letters[i];

    return bitmapOf(codes);
}

QList<unsigned int> digitCodes()
{
    QList<unsigned int> codes;

    // KEY_1..KEY_9 then KEY_0, which are contiguous in that order.
    for (unsigned int code = KEY_1; code <= KEY_0; ++code)
        codes << code;

    return codes;
}

} // namespace

class Ut_MimKeyboardKind : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNothingIsNotAKeyboard();

    void testDeviceWithAFewKeys_data();
    void testDeviceWithAFewKeys();

    void testFullKeyboard();
    void testKeyboardAtTheThreshold();
    void testKeyboardBelowTheThreshold();
    void testKeyboardWithANumberRowIsAKeyboard();

    void testTelephoneKeypad();
    void testKeypadWithOneLetterIsNotAKeypad();
    void testNumericKeypadIsNotATelephoneKeypad();
    void testPartialDigitsAreNotAKeypad();

    void testCodesPastTheBitmapAreNotAdvertised();

    void testKindNames();
};

void Ut_MimKeyboardKind::testNothingIsNotAKeyboard()
{
    const KeyBitmap empty;

    QVERIFY(empty.isEmpty());
    QCOMPARE(keyboardKindOf(empty), NotAKeyboard);
    QCOMPARE(letterKeyCount(empty), 0);

    const KeyBitmap zeroed = bitmapOf(QList<unsigned int>());
    QCOMPARE(keyboardKindOf(zeroed), NotAKeyboard);
}

void Ut_MimKeyboardKind::testDeviceWithAFewKeys_data()
{
    QTest::addColumn<QList<unsigned int> >("codes");

    // The devices that share the input subsystem with a keyboard and must never
    // be mistaken for one: each would silence the on-screen keyboard on a phone
    // that has nothing else to type with.
    QTest::newRow("power button") << (QList<unsigned int>() << KEY_POWER);
    QTest::newRow("volume rocker")
        << (QList<unsigned int>() << KEY_VOLUMEUP << KEY_VOLUMEDOWN);
    QTest::newRow("lid switch") << (QList<unsigned int>() << KEY_POWER << KEY_SLEEP);
    QTest::newRow("touchscreen") << (QList<unsigned int>() << BTN_TOUCH);
    QTest::newRow("mouse")
        << (QList<unsigned int>() << BTN_LEFT << BTN_RIGHT << BTN_MIDDLE);
    QTest::newRow("headset")
        << (QList<unsigned int>() << KEY_PLAYPAUSE << KEY_NEXTSONG
                                 << KEY_PREVIOUSSONG);
    QTest::newRow("gpio keys")
        << (QList<unsigned int>() << KEY_VOLUMEUP << KEY_VOLUMEDOWN << KEY_POWER
                                 << KEY_HOME << KEY_BACK << KEY_MENU);
}

void Ut_MimKeyboardKind::testDeviceWithAFewKeys()
{
    QFETCH(QList<unsigned int>, codes);

    QCOMPARE(keyboardKindOf(bitmapOf(codes)), NotAKeyboard);
}

void Ut_MimKeyboardKind::testFullKeyboard()
{
    const KeyBitmap keys = withLetters(LetterCount);

    QCOMPARE(letterKeyCount(keys), LetterCount);
    QCOMPARE(keyboardKindOf(keys), TextKeyboard);
}

void Ut_MimKeyboardKind::testKeyboardAtTheThreshold()
{
    // A driver is free to leave keys out, and these drivers do - one that
    // resolves an Alt level in the kernel, as the Zinwa Q25's bbqX0kbd does,
    // reports whatever that resolution produced.
    QCOMPARE(keyboardKindOf(withLetters(MinimumLetterKeys)), TextKeyboard);
}

void Ut_MimKeyboardKind::testKeyboardBelowTheThreshold()
{
    QCOMPARE(keyboardKindOf(withLetters(MinimumLetterKeys - 1)), NotAKeyboard);
}

void Ut_MimKeyboardKind::testKeyboardWithANumberRowIsAKeyboard()
{
    // Tested because the keypad rule would otherwise claim it: every laptop and
    // every phone QWERTY has the ten digits along the top.
    QList<unsigned int> codes(digitCodes());

    for (int i = 0; i < LetterCount; ++i)
        codes << Letters[i];

    QCOMPARE(keyboardKindOf(bitmapOf(codes)), TextKeyboard);
}

void Ut_MimKeyboardKind::testTelephoneKeypad()
{
    QCOMPARE(keyboardKindOf(bitmapOf(digitCodes())), TelephoneKeypad);
}

void Ut_MimKeyboardKind::testKeypadWithOneLetterIsNotAKeypad()
{
    // The strict reading, on purpose. A keypad wrongly missed leaves the
    // on-screen keyboard where it was; a device wrongly called a keypad, on a
    // configuration that counts keypads, loses it.
    QList<unsigned int> codes(digitCodes());
    codes << KEY_A;

    QCOMPARE(keyboardKindOf(bitmapOf(codes)), NotAKeyboard);
}

void Ut_MimKeyboardKind::testNumericKeypadIsNotATelephoneKeypad()
{
    // A USB numeric keypad reports KEY_KP0..KEY_KP9, not the digits a telephone
    // keypad reports, and nobody types prose on one.
    QList<unsigned int> codes;

    for (unsigned int code = KEY_KP7; code <= KEY_KP0; ++code)
        codes << code;

    codes << KEY_NUMLOCK << KEY_KPENTER << KEY_KPDOT;

    QCOMPARE(keyboardKindOf(bitmapOf(codes)), NotAKeyboard);
}

void Ut_MimKeyboardKind::testPartialDigitsAreNotAKeypad()
{
    QList<unsigned int> codes;

    for (unsigned int code = KEY_1; code <= KEY_5; ++code)
        codes << code;

    QCOMPARE(keyboardKindOf(bitmapOf(codes)), NotAKeyboard);
}

void Ut_MimKeyboardKind::testCodesPastTheBitmapAreNotAdvertised()
{
    // The kernel stops printing words once the rest would be zero, so asking
    // about a high code has to answer "no" rather than read off the end.
    const KeyBitmap keys = bitmapOf(QList<unsigned int>() << KEY_A);

    QVERIFY(keys.advertises(KEY_A));
    QVERIFY(!keys.advertises(KEY_MAX));
    QVERIFY(!keys.advertises(BTN_TRIGGER_HAPPY40));
}

void Ut_MimKeyboardKind::testKindNames()
{
    QCOMPARE(QString::fromLatin1(keyboardKindName(TextKeyboard)),
             QStringLiteral("text keyboard"));
    QCOMPARE(QString::fromLatin1(keyboardKindName(TelephoneKeypad)),
             QStringLiteral("telephone keypad"));
    QCOMPARE(QString::fromLatin1(keyboardKindName(NotAKeyboard)),
             QStringLiteral("not a keyboard"));
}

QTEST_GUILESS_MAIN(Ut_MimKeyboardKind)
#include "ut_mimkeyboardkind.moc"
