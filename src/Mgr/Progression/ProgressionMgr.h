/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PROGRESSIONMGR_H
#define PLAYERBOTS_PROGRESSIONMGR_H

#include "ItemTemplate.h"
#include "Player.h"

// Progression tiers as defined by mod-individual-progression's ProgressionState enum. State lives in
// hidden rewarded quests with ids 66000 + tier, which is what we read: that module ships no CMake
// target, so its headers cannot be included from here. mod-levelsync reads it the same way.
enum ProgressionTier : uint8
{
    IP_TIER_START = 0,   // vanilla leveling
    IP_TIER_BWL = 3,     // ZG and the AQ war effort open up
    IP_TIER_NAXX40 = 7,  // end of vanilla
    IP_TIER_TBC = 8,     // Karazhan / Gruul / Magtheridon, patch 2.0
    IP_TIER_SSC = 9,     // SSC / Tempest Keep, patch 2.1
    IP_TIER_SUNWELL = 12,
    IP_TIER_WOTLK = 13,  // WotLK Naxx / EoE / OS, patch 3.0
    IP_TIER_ULDUAR = 14, // patch 3.1
    IP_TIER_TOTC = 15,   // patch 3.2
    IP_TIER_MAX = 18
};

class ProgressionMgr
{
public:
    static ProgressionMgr& instance()
    {
        static ProgressionMgr instance;
        return instance;
    }

    // Must run before the factory builds its enchant/gem caches.
    void Init();

    bool IsEnabled() const;

    // Highest rewarded progression quest, or 0. Mirrors IndividualProgression's own reader.
    uint8 GetPlayerProgressionTier(Player* player) const;

    // Tier the gates should judge a bot by: its leader's, its own, or the configured fallback.
    uint8 GetBotProgressionTier(Player* bot) const;

    bool IsGemAllowed(ItemTemplate const* proto, uint8 tier) const;
    bool IsEnchantSpellAllowed(uint32 enchantSpellId, uint8 tier) const;
    bool IsItemAllowed(uint32 itemId, uint8 tier) const;

private:
    ProgressionMgr() = default;

    uint8 GetGemMinTier(ItemTemplate const* proto) const;
    uint8 GetEnchantSpellMinTier(uint32 enchantSpellId) const;
    uint8 GetItemMinTier(uint32 itemId) const;

    bool _ipActive = false;
};

#define sProgressionMgr ProgressionMgr::instance()

#endif
