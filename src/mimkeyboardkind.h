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

#ifndef MIMKEYBOARDKIND_H
#define MIMKEYBOARDKIND_H

#include "miminputdevices.h"

#include <QFlags>

//! \internal
/*! \ingroup maliitserver
 * \brief What an input device is, read off its EV_KEY capabilities.
 *
 * MImHwKeyboardTracker used to answer "does this device have a hardware
 * keyboard" from a SW_TABLET_MODE switch and nothing else. Only a convertible
 * or a slider offers that switch, so every phone with a fixed physical
 * keyboard - the Unihertz Titan and Titan Pocket, the Zinwa Q25, the MP01 -
 * was reported as having no keyboard, the Maliit::Hardware handler was never
 * selected, and the on-screen keyboard came up on top of a real one.
 *
 * What those devices do have is a keyboard that says so in its capability
 * bitmap. The rules for reading that live here, away from the udev
 * enumeration, so they can be checked against a captured bitmap in a unit test
 * instead of only on hardware.
 *
 * The rules are deliberately biased towards answering "no": a device wrongly
 * called a keyboard loses its on-screen keyboard and becomes unusable, while a
 * keyboard wrongly missed only leaves things as they were.
 */

namespace MImKeyboard {

//! \brief What a device's EV_KEY capability bitmap says it is.
enum KeyboardKind {
    //! Nothing text can be typed on: a power button, a volume rocker, a lid
    //! switch, a touchscreen, a mouse, a numeric keypad.
    NotAKeyboard = 0x0,
    //! Letter keys: a QWERTY, QWERTZ or AZERTY keyboard, whether it is the
    //! phone's own or plugged in over USB or Bluetooth.
    TextKeyboard = 0x1,
    //! The digits of a telephone and no letters: a keypad, which text is typed
    //! on by multi-tap rather than by one key per letter.
    TelephoneKeypad = 0x2,
};

Q_DECLARE_FLAGS(KeyboardKinds, KeyboardKind)

/*! \brief How many of the 26 Latin letters make a device a text keyboard.
 *
 * Not all 26. A driver is free to leave a key out or report it as something
 * else, and these keyboards' drivers do: the MP01's reports two keys the
 * hardware does not have, and a keyboard whose Alt or Sym level is resolved in
 * the kernel reports whatever that resolution produced. Still far above the
 * handful of keys a device that is not a keyboard advertises, so the two
 * cannot be confused.
 */
const int MinimumLetterKeys = 20;

//! \brief Classifies a device from the EV_KEY codes it advertises.
KeyboardKind keyboardKindOf(const KeyBitmap &keys);

//! \brief How many of the 26 Latin letters the device advertises.
//!
//! Separate from keyboardKindOf() only so a log line can say how close a device
//! came, which is the first thing worth knowing when a keyboard is not
//! recognised on a new port.
int letterKeyCount(const KeyBitmap &keys);

//! \brief Names a kind for the log.
const char *keyboardKindName(KeyboardKind kind);

} // namespace MImKeyboard

Q_DECLARE_OPERATORS_FOR_FLAGS(MImKeyboard::KeyboardKinds)

//! \internal_end

#endif // MIMKEYBOARDKIND_H
