/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

#include <maliit/plugins/extensionevent.h>
#include <maliit/plugins/extensionevent_p.h>

MImExtensionEventPrivate::~MImExtensionEventPrivate() = default;

// cppcheck does not expand Q_DISABLE_COPY, which this class does use, so it
// reports the copy constructor and assignment operator as missing. The
// suppressions have to sit on the line above the initialiser list, which is
// where it points.
MImExtensionEvent::MImExtensionEvent(Type type)
    // cppcheck-suppress noCopyConstructor
    // cppcheck-suppress noOperatorEq
    : d_ptr(new MImExtensionEventPrivate)
{
    d_ptr->type = type;
}

MImExtensionEvent::MImExtensionEvent(MImExtensionEventPrivate *dd,
                                     Type type)
    : d_ptr(dd)
{
    d_ptr->type = type;
}

MImExtensionEvent::~MImExtensionEvent()
{
    Q_D(MImExtensionEvent);
    delete d;
}

// The member access is inside Q_D, which cppcheck does not expand, so it
// concludes the function touches no members and could be static.
// cppcheck-suppress functionStatic
MImExtensionEvent::Type MImExtensionEvent::type() const
{
    Q_D(const MImExtensionEvent);
    return d->type;
}
