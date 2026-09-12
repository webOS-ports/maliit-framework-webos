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

#ifndef MIMXKBMODIFIERS_H
#define MIMXKBMODIFIERS_H

#include <cstdint>

//! \internal
namespace Maliit
{

//! Number of modifier indices a 32-bit xkb modifier mask can represent.
const uint32_t XkbModifierMaskBits = 32;

/*! \brief Is the modifier at \a modIndex set in \a mods?
 *
 * Deliberately takes plain integers rather than xkb types so it can be
 * reasoned about - and tested - without a keymap.
 *
 * xkb_map_mod_get_index() returns XKB_MOD_INVALID (~0u) for a modifier the
 * keymap does not define, and the wl_keyboard.modifiers mask is 32 bits wide.
 * "1 << index" is undefined behaviour for any index at or beyond the width of
 * the type, so XKB_MOD_INVALID - and anything else out of range - has to be
 * rejected before the index becomes a shift count. Such a modifier simply is
 * not present, so this reports it as unset.
 */
inline bool xkbModifierIsSet(uint32_t mods, uint32_t modIndex)
{
    if (modIndex >= XkbModifierMaskBits)
        return false;

    return (mods & (1u << modIndex)) != 0;
}

} // namespace Maliit
//! \internal_end

#endif // MIMXKBMODIFIERS_H
