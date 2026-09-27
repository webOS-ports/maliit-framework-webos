/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Copyright (C) 2021 LG Electronics, Inc.
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

// This file is based on mkeyboardstatetracker_p.h from libmeegotouch

#ifndef MIMHWKEYBOARDTRACKER_P_H
#define MIMHWKEYBOARDTRACKER_P_H

#include "miminputdevices.h"
#include "mimkeyboardkind.h"

#include <QFile>
#include <QObject>
#include <QString>
#include <QTimer>

struct udev;
struct udev_monitor;

class MImHwKeyboardTracker;
class QSocketNotifier;

class MImHwKeyboardTrackerPrivate
    : public QObject
{
    Q_OBJECT

public:
    explicit MImHwKeyboardTrackerPrivate(MImHwKeyboardTracker *q_ptr);
    ~MImHwKeyboardTrackerPrivate();

    //! \brief Enumerates the input devices and classifies each one.
    //!
    //! Also takes over watching a keyboard-presence switch, if a device has one
    //! and none is being watched yet.
    void scan();

    //! \brief Rescans, and emits stateChanged() if the answer moved.
    void rescanAndReport();

    //! \brief Subscribes to udev, so a keyboard plugged in later is noticed.
    void startMonitoring();

    //! \brief Logs a line about a device: at info on the first scan, where the
    //!        inventory is worth having, at debug on every later one.
    void announce(const char *format, ...) const
#if defined(__GNUC__)
        __attribute__((format(printf, 2, 3)))
#endif
        ;

    //! \brief Looks one device over: what it can type, and whether it carries
    //!        the switch.
    void examine(const MImKeyboard::InputDevice &device);

    //! \brief Reads a switch device's current state and watches it for changes.
    //!
    //! The only part of this that needs /dev/input: procfs says a device *has* a
    //! SW_TABLET_MODE switch, never which way it is thrown, and never tells us
    //! when it moves.
    bool watchSwitch(const MImKeyboard::InputDevice &device);

    //! \brief Lets go of the switch device, when it is unplugged or unreadable.
    void forgetSwitch();

    //! \brief Whether the device has a physical keyboard at all.
    bool hasKeyboard() const;

    //! \brief Whether a keyboard the caller counts can be typed on right now.
    bool usable() const;

    struct udev *udev = nullptr;
    struct udev_monitor *monitor = nullptr;
    QSocketNotifier *monitorNotifier = nullptr;

    //! The device carrying SW_TABLET_MODE, while one does. Its socket notifier
    //! is a child of it, so the two are dropped together.
    QFile *evdevFile = nullptr;
    //! Which node that is, so a rescan can tell it has been unplugged.
    QString switchNode;
    int evdevTabletModePending = -1;
    bool evdevTabletMode = false;

    //! Which kinds of keyboard are attached, and which of them the caller wants
    //! to hear about. A telephone keypad is not counted by default: a television
    //! remote control has the ten digits on it too, and taking the on-screen
    //! keyboard away from a television would be worse than leaving it in front
    //! of a keypad.
    MImKeyboard::KeyboardKinds attached;
    MImKeyboard::KeyboardKinds accepted = MImKeyboard::TextKeyboard;

    //! Set when the caller wants detection ignored: 0 never a keyboard, 1
    //! always one, -1 decide from the hardware.
    int forced = -1;

    //! What the last stateChanged() told the world, so a rescan that changes
    //! nothing stays quiet.
    bool reportedUsable = false;
    bool reportedPresent = false;

    //! The first scan lists every device it looked at, so a port that is not
    //! recognised can be diagnosed from the log. Later scans say only what
    //! changed: any input device coming or going wakes one, and re-listing them
    //! all each time buries everything else.
    bool firstScan = true;

    //! One keyboard arrives as a burst of udev events, and its event node is not
    //! always readable the instant the first of them lands. Coalesce, and come
    //! back a moment later.
    QTimer rescan;

public Q_SLOTS:
    void evdevEvent();
    void monitorEvent();

Q_SIGNALS:
    void stateChanged();
};


#endif // MIMHWKEYBOARDTRACKER_P_H
