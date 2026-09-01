/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDDATA_H
#define PLAYERBOTS_ULDDATA_H

#include "Common.h"

// The handful of Ulduar values that are not owned by one encounter. No behaviour lives here.
//
// Nothing boss-specific may be added: ids, tuning constants and positions belong in that boss's
// UldEncounter_<Boss>.h. This file stays small on purpose: a shared bucket for "everything the
// raid needs" is exactly how the previous 2400-line Ulduar header accumulated.
constexpr uint32 ULDUAR_MAP_ID = 603;

// Class spells the encounters name by id rather than by the bot's own spellbook.
constexpr uint32 SPELL_FROST_TRAP = 13809;
// Hunter buff; its charges are what the redirect action spends.
constexpr uint32 SPELL_MISDIRECTION = 35079;

#endif
