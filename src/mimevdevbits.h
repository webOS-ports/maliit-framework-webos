/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2012 One Laptop per Child Association
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

#ifndef MIMEVDEVBITS_H
#define MIMEVDEVBITS_H

#include <stddef.h>

//! \internal
/*! \ingroup maliitserver
 * \brief Bit-array helpers for the evdev ioctls.
 *
 * Kept in a header of their own so the sizing rule can be asserted by the
 * tests rather than only by whether MaliitServer happens to survive startup.
 */

//! Bytes needed to hold \a x bits.
#define BITS2BYTES(x) ((((x) - 1) / 8) + 1)

/*! \brief Bytes to reserve for an evdev bitmap covering \a x bits.
 *
 * The length argument of EVIOCGBIT is a size in bytes, not a bit count, and
 * the kernel copies the bitmap out in sizeof(long) granularity:
 *
 *     // drivers/input/evdev.c
 *     static int bits_to_user(unsigned long *bits, unsigned int maxbit,
 *                             unsigned int maxlen, void __user *p, int compat)
 *     {
 *             int len = BITS_TO_LONGS(maxbit) * sizeof(long);
 *             if (len > maxlen)
 *                     len = maxlen;
 *             return copy_to_user(p, bits, len) ? -EFAULT : len;
 *     }
 *
 * A buffer sized to the exact number of bytes the bits need is therefore not
 * enough - EVIOCGBIT(0, EV_MAX) writes 8 bytes on a 64-bit kernel where
 * BITS2BYTES(EV_MAX) is only 4. Round up to whole longs so the destination can
 * hold whatever the kernel is willing to write.
 */
#define EVDEV_BITS_BUFSIZE(x) \
    ((((BITS2BYTES(x)) + sizeof(long) - 1) / sizeof(long)) * sizeof(long))

//! Is \a bit set in the byte-addressed bit array \a array?
#define TEST_BIT(bit, array) (array[(bit) / 8] & (1 << (bit) % 8))

//! \internal_end

#endif // MIMEVDEVBITS_H
