/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
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

// This file is based on mkeyboardstatetracker.h from libmeegotouch

#ifndef MIMKEYBOARDSTATETRACKER_H
#define MIMKEYBOARDSTATETRACKER_H

#include <QObject>
#include <QScopedPointer>

#include "mimkeyboardkind.h"

class MImHwKeyboardTrackerPrivate;

//! \internal
/*! \ingroup maliitserver
 * \brief Class responsible for tracking the hardware keyboard properties and
 * signaling events.
 *
 * Singleton class. Using isPresent() queries whether the device supports
 * hardware keyboard or not. If hardware keyboard is supported, using isOpen()
 * to query its current state. Signal stateChanged will be emitted when the
 * hardware keyboard state is changed.
 *
 * A keyboard is found in either of two ways, and a device may offer both:
 *
 * \li By its capabilities. An input device advertising a keyboard's worth of
 *     keys is a keyboard - see MImKeyboard::keyboardKindOf(). This is how a
 *     phone with a fixed physical keyboard is recognised, and, through the udev
 *     subscription, how a detachable or a USB or Bluetooth keyboard is noticed
 *     coming and going.
 * \li By a SW_TABLET_MODE switch. A slider or a convertible keeps its keyboard
 *     enumerated while it is folded away, so the switch is the only thing that
 *     says whether the keyboard can be reached. While it reads tablet mode,
 *     isOpen() is false however many keyboards are attached.
 *
 * Which kinds of keyboard count is the caller's to set; see setAcceptedKinds().
 * setForcedState() overrides the lot, for a device whose keyboard this does not
 * recognise or whose on-screen keyboard has to stay regardless.
 */
class MImHwKeyboardTracker
    : public QObject
{
    Q_OBJECT

public:
    MImHwKeyboardTracker();
    virtual ~MImHwKeyboardTracker();

    //! \brief Returns whether device has a hardware keyboard.
    bool isPresent() const;

    //! \brief Returns whether hardware keyboard is opened.
    bool isOpen() const;

    /*! \brief Whether a keyboard-presence switch is being watched.
     *
     * True on a slider or a convertible, where the keyboard stays enumerated
     * while it is folded away and isOpen() follows the switch. False where a
     * keyboard is simply attached or not.
     */
    bool hasSwitch() const;

    //! \brief Which kinds of physical keyboard are attached right now.
    MImKeyboard::KeyboardKinds attachedKinds() const;

    //! \brief Which kinds of keyboard count as the user having one.
    MImKeyboard::KeyboardKinds acceptedKinds() const;

    /*! \brief Sets which kinds of keyboard count as the user having one.
     *
     * MImKeyboard::TextKeyboard by default, and not
     * MImKeyboard::TelephoneKeypad: a television remote control carries the ten
     * digits too, and taking a television's on-screen keyboard away would be
     * worse than leaving a keypad phone with one it does not need.
     */
    void setAcceptedKinds(MImKeyboard::KeyboardKinds kinds);

    /*! \brief Looks at the input devices again now.
     *
     * Normally unnecessary: the udev subscription reports a keyboard coming or
     * going by itself. For anything that changes the device list without one -
     * and for a test driving MALIIT_HW_INPUT_DEVICES.
     */
    void refresh();

    /*! \brief Overrides detection entirely.
     *
     * \param state 1 to report a keyboard always, 0 never, -1 to decide from
     *        the hardware.
     */
    void setForcedState(int state);

Q_SIGNALS:
    //! \brief Emitted whenever the hardware keyboard state changed.
    void stateChanged();

private:
    const QScopedPointer<MImHwKeyboardTrackerPrivate> d_ptr;

    Q_DISABLE_COPY(MImHwKeyboardTracker)
    Q_DECLARE_PRIVATE(MImHwKeyboardTracker)
};
//! \internal_end

#endif // MIMKEYBOARDSTATETRACKER_H
