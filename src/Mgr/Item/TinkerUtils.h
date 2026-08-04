/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TINKERUTILS_H
#define PLAYERBOTS_TINKERUTILS_H

#include "Common.h"

class Item;

namespace ai::tinker
{
    // True only for on-use enchant spells that grant a combat stat aura - the ones UseTinkerAction
    // will actually press. Damage grenades, parachutes and sprint effects fail it, which is what
    // keeps Hand-Mounted Pyro Rocket from outscoring Hyperspeed Accelerators on the same gloves.
    bool IsUsableTinkerSpell(uint32 spellId);

    // Every WotLK tinker worth pressing sits on a one minute cooldown; only used if the DBC reports
    // none.
    constexpr uint32 TINKER_FALLBACK_COOLDOWN_MS = 60 * IN_MILLISECONDS;

    // Cooldown in ms used to amortize a tinker's uptime when scoring it. Falls back to
    // TINKER_FALLBACK_COOLDOWN_MS rather than 0, so a caller never mistakes a short buff for a
    // permanent one.
    uint32 GetTinkerCooldownMs(uint32 spellId);

    // On-use enchant spell on this item worth pressing, or 0.
    uint32 GetUsableTinkerSpell(Item const* item);
}

#endif
