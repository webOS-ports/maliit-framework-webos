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

#include "miminputdevices.h"

#include <QByteArray>
#include <QFile>
#include <QRegularExpression>

namespace MImKeyboard {

KeyBitmap KeyBitmap::fromProcWords(const QStringList &words)
{
    KeyBitmap bitmap;

    if (words.isEmpty())
        return bitmap;

    int widest = 0;
    for (const QString &word : words)
        widest = qMax(widest, word.size());

    bitmap.m_wordBits = widest > 8 ? 64 : int(sizeof(unsigned long) * 8);

    // Least significant word last in the text, first in the list.
    for (int i = words.size() - 1; i >= 0; --i)
        bitmap.m_words.append(words.at(i).toULongLong(nullptr, 16));

    return bitmap;
}

bool KeyBitmap::advertises(unsigned int code) const
{
    if (m_wordBits <= 0)
        return false;

    const int word = int(code) / m_wordBits;
    const int bit = int(code) % m_wordBits;

    if (word >= m_words.size())
        return false;

    return (m_words.at(word) >> bit) & 1;
}

QString InputDevice::eventNode() const
{
    for (const QString &handler : handlers) {
        // "event3", not "eventfoo": the number is what makes it a node.
        if (handler.startsWith(QLatin1String("event"))) {
            const QStringView number(QStringView(handler).mid(5));

            bool ok = false;
            const int index = number.toInt(&ok);

            if (ok && index >= 0)
                return QLatin1String("/dev/input/") + handler;
        }
    }

    return QString();
}

bool InputDevice::isVirtual() const
{
    return sysfsPath.startsWith(QLatin1String("/devices/virtual/input/"));
}

QList<InputDevice> readInputDevices(const QString &path)
{
    QList<InputDevice> devices;

    QString file(path);

    if (file.isEmpty()) {
        const QByteArray override(qgetenv("MALIIT_HW_INPUT_DEVICES"));

        file = override.isEmpty() ? QStringLiteral("/proc/bus/input/devices")
                                  : QString::fromLocal8Bit(override);
    }

    QFile proc(file);
    if (!proc.open(QIODevice::ReadOnly | QIODevice::Text))
        return devices;

    static const QRegularExpression nameLine(QStringLiteral("^N: Name=\"(.*)\"$"));
    static const QRegularExpression keyLine(QStringLiteral("^B: KEY=(.*)$"));
    static const QRegularExpression switchLine(QStringLiteral("^B: SW=(.*)$"));
    static const QRegularExpression handlerLine(QStringLiteral("^H: Handlers=(.*)$"));
    static const QRegularExpression sysfsLine(QStringLiteral("^S: Sysfs=(.*)$"));

    // Read the whole file up front rather than streaming it. procfs reports a
    // size of 0, and QFileDevice::atEnd() is size() == pos(), so it answers true
    // before a single line has been read - a QTextStream loop guarded on atEnd()
    // silently sees no devices at all, while working fine against a captured
    // copy, which is the worst way for this to fail.
    const QList<QByteArray> lines(proc.readAll().split('\n'));

    for (const QByteArray &raw : lines) {
        const QString line(QString::fromLocal8Bit(raw).trimmed());

        // "N: Name=" opens a device; everything after it up to the next one
        // belongs to it.
        const QRegularExpressionMatch name(nameLine.match(line));
        if (name.hasMatch()) {
            InputDevice device;
            device.name = name.captured(1);
            devices.append(device);
            continue;
        }

        if (devices.isEmpty())
            continue;

        InputDevice &device = devices.last();

        const QRegularExpressionMatch keys(keyLine.match(line));
        if (keys.hasMatch()) {
            device.keys = KeyBitmap::fromProcWords(
                keys.captured(1).split(QLatin1Char(' '), Qt::SkipEmptyParts));
            continue;
        }

        const QRegularExpressionMatch switches(switchLine.match(line));
        if (switches.hasMatch()) {
            device.switches = KeyBitmap::fromProcWords(
                switches.captured(1).split(QLatin1Char(' '), Qt::SkipEmptyParts));
            continue;
        }

        const QRegularExpressionMatch handlers(handlerLine.match(line));
        if (handlers.hasMatch()) {
            device.handlers =
                handlers.captured(1).split(QLatin1Char(' '), Qt::SkipEmptyParts);
            continue;
        }

        const QRegularExpressionMatch sysfs(sysfsLine.match(line));
        if (sysfs.hasMatch())
            device.sysfsPath = sysfs.captured(1).trimmed();
    }

    return devices;
}

} // namespace MImKeyboard
