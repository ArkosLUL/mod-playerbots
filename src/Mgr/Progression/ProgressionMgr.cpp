/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ProgressionMgr.h"

#include "Config.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "QuestDef.h"
#include "SharedDefines.h"

namespace
{
// mod-individual-progression stores progression as rewarded quests 66001..66018.
constexpr uint32 IP_QUEST_BASE = 66000;

struct TierRange
{
    uint32 lo;
    uint32 hi;
    uint8 minTier;
};

// Gem families straggle across the id space (Living Ruby cuts run 24027..38292), so the general case
// is decided by ItemLevel/Quality in GetGemMinTier. These two blocks are the ones that rule gets
// wrong, so they are matched by id first.
constexpr TierRange GEM_OVERRIDES[] = {
    {30546, 30607, IP_TIER_SSC},     // Tanzanite / Fire Opal / Chrysoprase, 2.1 raid drops
    {45862, 45987, IP_TIER_ULDUAR},  // Stormjewels, 3.1
};

// Enchants whose id says nothing useful about when they became available: late-vanilla formulas
// (Zul'Gurub and the AQ war effort, patches 1.7-1.9) and the handful added mid-expansion. Anything
// not listed here is judged by the expansion boundaries below; the vanilla ids outside these blocks
// are 1.0-1.6 content that never needs gating.
constexpr TierRange ENCHANT_OVERRIDES[] = {
    {22749, 22750, IP_TIER_BWL},  // Weapon - Spellpower / Healing Power
    {23799, 23804, IP_TIER_BWL},  // Weapon - Strength / Agility, Bracer - Mana Regen / Healing Power
    {24149, 24149, IP_TIER_BWL},  // Presence of Might, ZG head/leg
    {24160, 24168, IP_TIER_BWL},  // the other nine ZG head/leg enchants
    {25072, 25086, IP_TIER_BWL},  // Gloves and Cloak formulas
    {42974, 42974, IP_TIER_SUNWELL},  // Weapon - Executioner, 2.4
    {62256, 62257, IP_TIER_ULDUAR},  // Bracer - Major Stamina, Weapon - Titanguard, 3.1
    {62948, 62948, IP_TIER_ULDUAR},  // Staff - Greater Spellpower, 3.1
    {64441, 64441, IP_TIER_ULDUAR},  // Weapon - Blade Ward, Ulduar drop
    {64579, 64579, IP_TIER_ULDUAR},  // Weapon - Blood Draining, Ulduar drop
};

// Same boundaries RandomItemMgr::IsAllowedForLevelExpansion uses, so the level gate and the
// progression gate stay consistent about where an expansion starts.
constexpr uint32 EXPANSION_SPELL_ID_TBC = 27899;
constexpr uint32 EXPANSION_SPELL_ID_WOTLK = 44483;
constexpr uint32 EXPANSION_ITEM_ID_TBC = 23728;
constexpr uint32 EXPANSION_ITEM_ID_WOTLK = 35570;
}  // namespace

void ProgressionMgr::Init()
{
    // The quests come from the module's SQL, which is applied whether or not the module is switched
    // on. With IndividualProgression.Enable = 0 nobody is ever rewarded one, so every real player
    // would read as tier 0 and the gates would strip their bots bare. Default matches the module's.
    bool ipEnabled = sConfigMgr->GetOption<bool>("IndividualProgression.Enable", true);

    _ipActive = ipEnabled && sObjectMgr->GetQuestTemplate(IP_QUEST_BASE + 1) != nullptr;

    if (_ipActive)
        LOG_INFO("playerbots", "Individual Progression detected, bot item gating is available");
}

bool ProgressionMgr::IsEnabled() const { return _ipActive && sPlayerbotAIConfig.limitProgressionTier; }

uint8 ProgressionMgr::GetPlayerProgressionTier(Player* player) const
{
    if (!player || !player->IsInWorld())
        return 0;

    uint8 tier = 0;
    for (uint8 i = 1; i <= IP_TIER_MAX; ++i)
        if (player->GetQuestStatus(IP_QUEST_BASE + i) == QUEST_STATUS_REWARDED)
            tier = i;

    return tier;
}

uint8 ProgressionMgr::GetBotProgressionTier(Player* bot) const
{
    if (!IsEnabled() || !bot)
        return IP_TIER_MAX;

    // A grouped bot follows its leader. Individual Progression already pushes the leader's tier onto
    // bot group members itself, so reading it here agrees with that module rather than fighting it.
    if (Group* group = bot->GetGroup())
    {
        if (Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID()))
        {
            if (leader != bot && !GET_PLAYERBOT_AI(leader))
                return GetPlayerProgressionTier(leader);
        }
    }

    // Ungrouped bots on a bot account get their tier force-set to 0/8/13 from level alone on every
    // login, so a zero here means "never progressed", not "stuck at the start".
    if (uint8 own = GetPlayerProgressionTier(bot))
        return own;

    return static_cast<uint8>(sPlayerbotAIConfig.progressionTierCap);
}

uint8 ProgressionMgr::GetGemMinTier(ItemTemplate const* proto) const
{
    // Dragon's Eye cuts are ilvl80 epics that shipped with 3.0, so they would otherwise fall through
    // to the 3.2 epic-cut rule below.
    if (proto->RequiredSkill == SKILL_JEWELCRAFTING)
        return IP_TIER_WOTLK;

    for (TierRange const& range : GEM_OVERRIDES)
        if (proto->ItemId >= range.lo && proto->ItemId <= range.hi)
            return range.minTier;

    // ItemLevel and Quality track the content tier a gem was designed for, which is what makes this
    // work where raw id ordering does not. ilvl70 epics are Sunwell (2.4), ilvl80 epics are the
    // Cardinal Ruby family from Trial of the Crusade (3.2).
    if (proto->ItemLevel <= 70)
        return proto->Quality >= ITEM_QUALITY_EPIC ? IP_TIER_SUNWELL : IP_TIER_TBC;

    return proto->Quality >= ITEM_QUALITY_EPIC ? IP_TIER_TOTC : IP_TIER_WOTLK;
}

uint8 ProgressionMgr::GetEnchantSpellMinTier(uint32 enchantSpellId) const
{
    for (TierRange const& range : ENCHANT_OVERRIDES)
        if (enchantSpellId >= range.lo && enchantSpellId <= range.hi)
            return range.minTier;

    if (enchantSpellId >= EXPANSION_SPELL_ID_WOTLK)
        return IP_TIER_WOTLK;

    if (enchantSpellId >= EXPANSION_SPELL_ID_TBC)
        return IP_TIER_TBC;

    return IP_TIER_START;
}

uint8 ProgressionMgr::GetItemMinTier(uint32 itemId) const
{
    if (itemId >= EXPANSION_ITEM_ID_WOTLK)
        return IP_TIER_WOTLK;

    if (itemId >= EXPANSION_ITEM_ID_TBC)
        return IP_TIER_TBC;

    return IP_TIER_START;
}

bool ProgressionMgr::IsGemAllowed(ItemTemplate const* proto, uint8 tier) const
{
    if (!IsEnabled())
        return true;

    // Sockets are a TBC mechanic, so below that no gem is legitimate whatever it scores.
    if (tier < IP_TIER_TBC)
        return false;

    return proto && tier >= GetGemMinTier(proto);
}

bool ProgressionMgr::IsEnchantSpellAllowed(uint32 enchantSpellId, uint8 tier) const
{
    if (!IsEnabled())
        return true;

    return tier >= GetEnchantSpellMinTier(enchantSpellId);
}

bool ProgressionMgr::IsItemAllowed(uint32 itemId, uint8 tier) const
{
    if (!IsEnabled())
        return true;

    return tier >= GetItemMinTier(itemId);
}
