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

#ifndef MIMJSONPARAMS_H
#define MIMJSONPARAMS_H

#include <QJsonValue>

//! \internal
/*! \ingroup maliitserver
 * \brief Helpers for reading parameters out of a JSON payload that arrived
 * from an untrusted peer.
 *
 * The LS2 methods MaliitServer exports are callable by any client allowed on
 * the bus, so every number in a payload is attacker controlled. JSON numbers
 * are doubles: casting one straight to int is undefined behaviour the moment
 * it does not fit, and a fractional or non-finite value has no meaningful int
 * reading at all.
 */
namespace Maliit
{
namespace Json
{

/*! \brief Read \a value as an int in [\a min, \a max].
 *
 * \param value   the JSON value to read.
 * \param min     smallest accepted value, inclusive.
 * \param max     largest accepted value, inclusive.
 * \param result  receives the value; untouched unless this returns \c true.
 *
 * \return \c true when \a value is a number that lies in the closed range.
 *
 * Returns \c false for a value that is not a JSON number, for one outside the
 * range, and for NaN and the infinities - NaN because it compares false
 * against everything, the infinities because they fail the range test. The
 * fractional part is truncated towards zero, matching a plain cast, but only
 * once the value is known to fit.
 */
inline bool toInt(const QJsonValue &value, int min, int max, int *result)
{
    if (!result)
        return false;

    if (!value.isDouble())
        return false;

    const double raw = value.toDouble();

    // Written as a positive test so NaN, which compares false against every
    // bound, is rejected rather than accepted by a negated one.
    if (!(raw >= static_cast<double>(min) && raw <= static_cast<double>(max)))
        return false;

    *result = static_cast<int>(raw);
    return true;
}

} // namespace Json
} // namespace Maliit
//! \internal_end

#endif // MIMJSONPARAMS_H
