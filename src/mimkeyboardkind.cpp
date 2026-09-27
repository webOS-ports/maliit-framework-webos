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

#include "mimkeyboardkind.h"

#include <linux/input.h>

namespace {

//! The 26 Latin letters, in the order evdev numbers them - which is the three
//! rows of a US QWERTY, because that is what the scancodes were named after.
const unsigned int LetterKeys[] = {
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P,
    KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L,
    KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
};

//! The digits of a telephone, in the order evdev numbers them - KEY_0 comes
//! after KEY_9, not before KEY_1. Deliberately the number-row codes and not
//! KEY_KP0..KEY_KP9: a USB numeric keypad is not something text is typed on.
const unsigned int DigitKeys[] = {
    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
};

const size_t LetterKeyCount = sizeof(LetterKeys) / sizeof(LetterKeys[0]);
const size_t DigitKeyCount = sizeof(DigitKeys) / sizeof(DigitKeys[0]);

int countAdvertised(const MImKeyboard::KeyBitmap &keys,
                    const unsigned int *codes, size_t count)
{
    int found = 0;

    for (size_t i = 0; i < count; ++i) {
        if (keys.advertises(codes[i]))
            ++found;
    }

    return found;
}

} // namespace

namespace MImKeyboard {

int letterKeyCount(const KeyBitmap &keys)
{
    return countAdvertised(keys, LetterKeys, LetterKeyCount);
}

KeyboardKind keyboardKindOf(const KeyBitmap &keys)
{
    if (keys.isEmpty())
        return NotAKeyboard;

    const int letters = letterKeyCount(keys);

    if (letters >= MinimumLetterKeys)
        return TextKeyboard;

    // A keypad has the digits and not one letter. Tested after the letters, so
    // that a keyboard with a number row is a keyboard; and demanding every
    // digit and no letter at all, because the alternative - a partial match
    // counting - would take the on-screen keyboard away from anything with a
    // few number keys on it.
    if (letters == 0
        && countAdvertised(keys, DigitKeys, DigitKeyCount)
               == static_cast<int>(DigitKeyCount)) {
        return TelephoneKeypad;
    }

    return NotAKeyboard;
}

const char *keyboardKindName(KeyboardKind kind)
{
    switch (kind) {
    case TextKeyboard:
        return "text keyboard";
    case TelephoneKeypad:
        return "telephone keypad";
    case NotAKeyboard:
        break;
    }

    return "not a keyboard";
}

} // namespace MImKeyboard
