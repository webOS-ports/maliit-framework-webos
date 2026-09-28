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

#ifndef MIMINPUTDEVICES_H
#define MIMINPUTDEVICES_H

#include <QList>
#include <QString>
#include <QStringList>

//! \internal
/*! \ingroup maliitserver
 * \brief Reading what input devices the kernel has, and what each can report.
 *
 * From /proc/bus/input/devices rather than by opening the evdev nodes: it is
 * world readable, so this works whether or not MaliitServer has a seat on
 * /dev/input, and a copy captured off a phone can be replayed anywhere - which
 * is how detection is checked on a device nobody has in hand.
 *
 * webos-keyboard's own hardware keyboard support reads the same file the same
 * way. Deliberately: the two have to agree about what is attached, and they
 * cannot disagree if they are looking at one source.
 */

namespace MImKeyboard {

/*! \brief One input device's capability bitmap, as the kernel prints it.
 *
 * The kernel prints a bitmap most significant word first, and whether it pads
 * the words is not something to rely on. An MP01 gives
 *
 *     B: KEY=1000000000007 ff9f207ac14057ff febeffdfffefffff fffffffffffffffe
 *
 * where every word happens to fill its width, while a Mindset gives
 *
 *     B: KEY=800 0 0 0 0 0 0 0 0 8 0 0 0 1c0000 0 0 ffc
 *
 * with nothing padded at all. So the width comes from the longest word - no
 * word can need more digits than the machine's long has - falling back to this
 * process's own long, which belongs to the same kernel, when every value is
 * small enough to be ambiguous.
 */
class KeyBitmap
{
public:
    //! \brief Parses the words following "B: KEY=" or "B: SW=".
    static KeyBitmap fromProcWords(const QStringList &words);

    //! \brief Whether the device can report \a code.
    bool advertises(unsigned int code) const;

    bool isEmpty() const { return m_words.isEmpty(); }

    //! \brief The word width taken from the text, in bits. For the tests.
    int wordBits() const { return m_wordBits; }

private:
    //! Least significant word first, so a code indexes straight into the list.
    QList<quint64> m_words;
    int m_wordBits = 0;
};

//! \brief One entry of /proc/bus/input/devices.
struct InputDevice
{
    //! As the driver registered it, e.g. "aw9523-key" or "Lid Switch".
    QString name;
    //! What EV_KEY codes it can report.
    KeyBitmap keys;
    //! What EV_SW codes it can report.
    KeyBitmap switches;
    //! The "H: Handlers=" list, e.g. "sysrq kbd event3 leds".
    QStringList handlers;
    //! The "S: Sysfs=" path, e.g. "/devices/platform/i8042/serio0/input/input2".
    QString sysfsPath;

    /*! \brief The device's evdev node, or an empty string when it has none.
     *
     * Needed because a switch's state has to be read and then watched, and
     * neither can be done through procfs.
     */
    QString eventNode() const;

    /*! \brief Whether this device was made by a program rather than plugged in.
     *
     * A uinput device - a remote-control tool such as RustDesk, a key injector,
     * a test harness - registers with no parent, so it lands directly under
     * /devices/virtual/input/. It can look exactly like a full keyboard, and one
     * showing up must not take the on-screen keyboard away.
     *
     * Only that one path, not everything virtual: a Bluetooth keyboard arrives
     * through uhid, which puts it under /devices/virtual/misc/uhid/... and is a
     * real keyboard that has to count.
     */
    bool isVirtual() const;
};

/*! \brief Reads every input device the kernel knows about.
 *
 * \param path the file to read. Empty means /proc/bus/input/devices, or
 *        whatever MALIIT_HW_INPUT_DEVICES names - the override exists so a
 *        capture from a phone can be replayed.
 */
QList<InputDevice> readInputDevices(const QString &path = QString());

} // namespace MImKeyboard

//! \internal_end

#endif // MIMINPUTDEVICES_H
