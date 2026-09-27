/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Copyright (C) 2012 One Laptop per Child Association
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

// This file is based on mkeyboardstatetracker.cpp from libmeegotouch

#include <QDebug>
#include <QString>

#include <stdarg.h>
#include <QSocketNotifier>

#include <libudev.h>
#include <linux/input.h>
#include <sys/ioctl.h>

#include "mimevdevbits.h"
#include "mimhwkeyboardtracker.h"
#include "mimhwkeyboardtracker_p.h"
#include "miminputdevices.h"
#include "mimkeyboardkind.h"

namespace {

//! How long to wait after a udev event before looking again. One keyboard
//! appears as several nodes, and the event node is not always openable the
//! instant its own add event arrives.
const int RescanDelay = 250;

} // namespace

MImHwKeyboardTrackerPrivate::MImHwKeyboardTrackerPrivate(MImHwKeyboardTracker *q_ptr)
{
    QObject::connect(this, SIGNAL(stateChanged()),
                     q_ptr, SIGNAL(stateChanged()));

    rescan.setSingleShot(true);
    rescan.setInterval(RescanDelay);
    connect(&rescan, &QTimer::timeout,
            this, &MImHwKeyboardTrackerPrivate::rescanAndReport);

    // udev is subscribed to for one thing only: being told that the set of input
    // devices changed. What they are is read from procfs, which needs no
    // privileges and can be replayed from a capture.
    udev = udev_new();
    if (!udev)
        qWarning() << "no udev context; a keyboard plugged in later will be missed";

    scan();
    startMonitoring();

    reportedUsable = usable();
    reportedPresent = hasKeyboard();
}

MImHwKeyboardTrackerPrivate::~MImHwKeyboardTrackerPrivate()
{
    // The notifiers are children of this object, or of evdevFile which is, so
    // Qt takes them down. The udev handles are not Qt's and have to be undone
    // in the order they were made.
    if (monitor)
        udev_monitor_unref(monitor);

    if (udev)
        udev_unref(udev);
}

void MImHwKeyboardTrackerPrivate::scan()
{
    attached = MImKeyboard::NotAKeyboard;

    const QList<MImKeyboard::InputDevice> devices(MImKeyboard::readInputDevices());

    if (devices.isEmpty())
        qWarning() << "no input devices readable; assuming no hardware keyboard";

    // Let go of a switch whose device is no longer listed. Otherwise a detached
    // convertible keyboard leaves a descriptor behind that reads nothing and a
    // tablet-mode flag stuck at whatever it last said.
    if (evdevFile && !switchNode.isEmpty()) {
        bool stillThere = false;

        for (const MImKeyboard::InputDevice &device : devices) {
            if (device.eventNode() == switchNode) {
                stillThere = true;
                break;
            }
        }

        if (!stillThere) {
            qInfo("the keyboard switch on %s is gone", qUtf8Printable(switchNode));
            forgetSwitch();
        }
    }

    for (const MImKeyboard::InputDevice &device : devices)
        examine(device);

    firstScan = false;
}

void MImHwKeyboardTrackerPrivate::rescanAndReport()
{
    scan();

    const bool nowUsable = usable();
    const bool nowPresent = hasKeyboard();

    if (nowUsable == reportedUsable && nowPresent == reportedPresent)
        return;

    reportedUsable = nowUsable;
    reportedPresent = nowPresent;

    qInfo() << "hardware keyboard:" << (nowPresent ? "present" : "absent")
            << (nowUsable ? "and usable" : "and not usable");

    Q_EMIT stateChanged();
}

void MImHwKeyboardTrackerPrivate::startMonitoring()
{
    if (!udev)
        return;

    monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (!monitor) {
        qWarning() << "no udev monitor; a keyboard plugged in later will be missed";
        return;
    }

    udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", nullptr);

    if (udev_monitor_enable_receiving(monitor) < 0) {
        qWarning() << "cannot receive udev events; a keyboard plugged in later"
                   << "will be missed";
        udev_monitor_unref(monitor);
        monitor = nullptr;
        return;
    }

    monitorNotifier = new QSocketNotifier(udev_monitor_get_fd(monitor),
                                          QSocketNotifier::Read, this);
    connect(monitorNotifier, &QSocketNotifier::activated,
            this, &MImHwKeyboardTrackerPrivate::monitorEvent);
}

void MImHwKeyboardTrackerPrivate::monitorEvent()
{
    if (!monitor)
        return;

    // Drain what is queued. The events are not read for their contents - any
    // input device coming or going means the answer may have moved, and a
    // rescan is cheap next to keeping a model of every node in step.
    while (struct udev_device *device = udev_monitor_receive_device(monitor))
        udev_device_unref(device);

    rescan.start();
}

void MImHwKeyboardTrackerPrivate::announce(const char *format, ...) const
{
    va_list args;
    va_start(args, format);

    QString message(QString::vasprintf(format, args));

    va_end(args);

    if (firstScan)
        qInfo("%s", qUtf8Printable(message));
    else
        qDebug("%s", qUtf8Printable(message));
}

void MImHwKeyboardTrackerPrivate::examine(const MImKeyboard::InputDevice &device)
{
    const MImKeyboard::KeyboardKind kind =
        MImKeyboard::keyboardKindOf(device.keys);

    if (kind != MImKeyboard::NotAKeyboard && device.isVirtual()) {
        // A program's keyboard, not the user's. Seen in the wild: RustDesk
        // registers a uinput device advertising all 26 letters, and counting it
        // would take the on-screen keyboard away from a phone that has none.
        announce("input device \"%s\" looks like a %s but is software-made (%s);"
                 " not counting it",
                 qUtf8Printable(device.name),
                 MImKeyboard::keyboardKindName(kind),
                 qUtf8Printable(device.sysfsPath));
    } else if (kind != MImKeyboard::NotAKeyboard) {
        attached |= kind;
        announce("input device \"%s\" is a %s (%d letter keys)",
                 qUtf8Printable(device.name),
                 MImKeyboard::keyboardKindName(kind),
                 MImKeyboard::letterKeyCount(device.keys));
    } else if (!device.keys.isEmpty()) {
        qDebug("input device \"%s\" has keys but is not a keyboard"
               " (%d letter keys)",
               qUtf8Printable(device.name),
               MImKeyboard::letterKeyCount(device.keys));
    }

    // Take the first device offering a keyboard-presence switch. Its EV_SW
    // events are the only way a slider or a convertible says its keyboard has
    // gone away, because the node stays right where it was.
    if (!evdevFile && device.switches.advertises(SW_TABLET_MODE))
        watchSwitch(device);
}

bool MImHwKeyboardTrackerPrivate::watchSwitch(const MImKeyboard::InputDevice &device)
{
    const QString node(device.eventNode());

    if (node.isEmpty()) {
        qWarning() << "device" << device.name
                   << "reports keyboard presence but has no evdev node";
        return false;
    }

    QFile *qfile = new QFile(node, this);

    if (!qfile->open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        qWarning() << "cannot read" << node << "to follow the keyboard switch:"
                   << qfile->errorString();
        delete qfile;
        return false;
    }

    const int fd = qfile->handle();
    if (fd == -1) {
        delete qfile;
        return false;
    }

    unsigned char state[EVDEV_BITS_BUFSIZE(SW_MAX)];
    if (ioctl(fd, EVIOCGSW(SW_MAX), state) < 0) {
        qWarning() << "cannot read the switch state of" << node;
        delete qfile;
        return false;
    }

    QSocketNotifier *sn = new QSocketNotifier(fd, QSocketNotifier::Read, qfile);
    sn->setEnabled(true);
    connect(sn, &QSocketNotifier::activated,
            this, &MImHwKeyboardTrackerPrivate::evdevEvent);

    evdevFile = qfile;
    switchNode = node;
    evdevTabletModePending = -1;
    evdevTabletMode = TEST_BIT(SW_TABLET_MODE, state);

    qInfo("input device \"%s\" reports keyboard presence; tablet mode is %s",
          qUtf8Printable(device.name), evdevTabletMode ? "on" : "off");

    return true;
}

void MImHwKeyboardTrackerPrivate::forgetSwitch()
{
    // Deleting the file takes its socket notifier with it, which is the point:
    // a notifier left on a closed descriptor spins.
    delete evdevFile;
    evdevFile = nullptr;
    switchNode.clear();
    evdevTabletModePending = -1;
    evdevTabletMode = false;
}

void MImHwKeyboardTrackerPrivate::evdevEvent()
{
    // Parse the evdev event and look for SW_TABLET_MODE status.

    struct input_event ev;

    if (!evdevFile) {
        return;
    }

    qint64 len = evdevFile->read((char *) &ev, sizeof(ev));
    if (len < 0) {
        qWarning() << "Failed to read from the evdev node:" << evdevFile->errorString();
        // Almost always the device having been unplugged. Let it go and look
        // again, rather than leaving a notifier on a descriptor that will only
        // fail the same way.
        forgetSwitch();
        rescan.start();
        return;
    }
    if (len != sizeof(ev)) {
        qWarning() << "Short read from the evdev node, discarding" << len << "bytes";
        return;
    }

    // We wait for a SYN before "committing" the new state, just in case.
    if (ev.type == EV_SW && ev.code == SW_TABLET_MODE) {
        evdevTabletModePending = ev.value;
    } else if (ev.type == EV_SYN && ev.code == SYN_REPORT
            && evdevTabletModePending != -1) {
        evdevTabletMode = evdevTabletModePending;
        evdevTabletModePending = -1;

        reportedUsable = usable();
        reportedPresent = hasKeyboard();
        Q_EMIT stateChanged();
    }

}

bool MImHwKeyboardTrackerPrivate::hasKeyboard() const
{
    // Deliberately not overridden by "forced". This is the physical fact - is
    // there a keyboard attached - and the override is about what to do about it,
    // which is usable()'s business.
    //
    // They were the same answer once, and it stranded the user: asking for the
    // on-screen keyboard sets forced to 0, which made isPresent() false, which
    // made the shell's "Show On-screen Keyboard" entry - shown only where there
    // is a hardware keyboard - hide itself. The way back disappeared the moment
    // it was used.
    return attached != MImKeyboard::NotAKeyboard || evdevFile != nullptr;
}

bool MImHwKeyboardTrackerPrivate::usable() const
{
    if (forced >= 0)
        return forced == 1;

    // While the switch says tablet mode, the keyboard is folded away or slid in
    // and nothing can be typed on it.
    if (evdevFile && evdevTabletMode)
        return false;

    // Either a keyboard of a kind the caller counts, or - as it was before any
    // kind was recognised - a switch saying the keyboard is out.
    return (attached & accepted) != MImKeyboard::NotAKeyboard
        || evdevFile != nullptr;
}

MImHwKeyboardTracker::MImHwKeyboardTracker()
    : d_ptr(new MImHwKeyboardTrackerPrivate(this))
{
}

MImHwKeyboardTracker::~MImHwKeyboardTracker() = default;

bool MImHwKeyboardTracker::isPresent() const
{
    Q_D(const MImHwKeyboardTracker);

    return d->hasKeyboard();
}

bool MImHwKeyboardTracker::isOpen() const
{
    Q_D(const MImHwKeyboardTracker);

    return d->usable();
}

bool MImHwKeyboardTracker::hasSwitch() const
{
    Q_D(const MImHwKeyboardTracker);

    return d->evdevFile != nullptr;
}

MImKeyboard::KeyboardKinds MImHwKeyboardTracker::attachedKinds() const
{
    Q_D(const MImHwKeyboardTracker);

    return d->attached;
}

MImKeyboard::KeyboardKinds MImHwKeyboardTracker::acceptedKinds() const
{
    Q_D(const MImHwKeyboardTracker);

    return d->accepted;
}

void MImHwKeyboardTracker::refresh()
{
    Q_D(MImHwKeyboardTracker);

    d->rescanAndReport();
}

void MImHwKeyboardTracker::setAcceptedKinds(MImKeyboard::KeyboardKinds kinds)
{
    Q_D(MImHwKeyboardTracker);

    if (d->accepted == kinds)
        return;

    d->accepted = kinds;
    d->rescanAndReport();
}

void MImHwKeyboardTracker::setForcedState(int state)
{
    Q_D(MImHwKeyboardTracker);

    if (d->forced == state)
        return;

    d->forced = state;
    d->rescanAndReport();
}
