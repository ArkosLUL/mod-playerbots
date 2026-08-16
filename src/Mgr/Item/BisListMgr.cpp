/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BisListMgr.h"
#include "AiFactory.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Player.h"
#include "Playerbots.h"
#include "ProgressionMgr.h"
#include "QueryResult.h"

#include <algorithm>

void BisListMgr::LoadAll()
{
    // Two independent tables: either can be absent without disabling the other.
    LoadGear();
    LoadRanked();
}

void BisListMgr::LoadGear()
{
    _bis.clear();

    QueryResult result = PlayerbotsDatabase.Query(
        "SELECT class, tab, slot, faction, auto_gear_score_limit, item_id FROM playerbots_bis_gear");
    if (!result)
    {
        LOG_INFO("server.loading", "playerbots_bis_gear table missing or empty");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint8  cls       = fields[0].Get<uint8>();
        uint8  tab       = fields[1].Get<uint8>();
        uint8  slot      = fields[2].Get<uint8>();
        uint8  faction   = fields[3].Get<uint8>();
        uint16 autoGearScoreLimit = fields[4].Get<uint16>();
        uint32 item      = fields[5].Get<uint32>();

        _bis[autoGearScoreLimit][MakeKey(cls, tab)][faction][slot] = item;
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", "Loaded {} BiS entries across {} item levels",
             count, static_cast<uint32>(_bis.size()));
}

void BisListMgr::LoadRanked()
{
    _ranked.clear();

    QueryResult result = PlayerbotsDatabase.Query(
        "SELECT item_id, expansion, class, tab, phase, bis_rank FROM playerbots_bis_ranked");
    if (!result)
    {
        LOG_INFO("server.loading", "playerbots_bis_ranked table missing or empty");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 itemId = fields[0].Get<uint32>();

        RankedEntry entry;
        entry.expansion = fields[1].Get<uint8>();
        entry.cls = fields[2].Get<uint8>();
        entry.tab = fields[3].Get<uint8>();
        entry.phase = fields[4].Get<uint8>();
        entry.rank = fields[5].Get<uint8>();

        _ranked[itemId].push_back(entry);
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", "Loaded {} ranked BiS entries across {} items",
             count, static_cast<uint32>(_ranked.size()));
}

bool BisListMgr::ResolveSpecKey(Player* bot, uint8& cls, uint8& tab)
{
    if (!bot)
        return false;

    // The lists carry no resilience gear anywhere, so they would steer a PvP bot into PvE itemization.
    if (sRandomPlayerbotMgr.IsSpecPvp(bot->GetGUID().GetCounter(), bot->getClass()))
        return false;

    uint8 const botClass = bot->getClass();
    uint8 const specTab = AiFactory::GetPlayerSpecTab(bot);

    // Role comes from the active strategy, not the talent tab (see GetSpecTraits), so the two can
    // disagree - a prot-talented bot running a dps strategy, say. Handing that bot the Protection list
    // would gate-pass and nudge it toward avoidance gear it is not using, so refuse instead.
    bool const isTank = PlayerbotAI::IsTank(bot);
    bool const isHealer = !isTank && PlayerbotAI::IsHeal(bot);

    uint8 resolved = BIS_TAB_NONE;

    switch (botClass)
    {
        case CLASS_WARRIOR:
            if (specTab == WARRIOR_TAB_PROTECTION)
                resolved = isTank ? specTab : BIS_TAB_NONE;
            else if (specTab == WARRIOR_TAB_FURY)
                resolved = isHealer ? BIS_TAB_NONE : (isTank ? BIS_TAB_WARRIOR_FURY_PROT : specTab);
            else
                resolved = (!isTank && !isHealer) ? specTab : BIS_TAB_NONE;
            break;
        case CLASS_PALADIN:
            if (specTab == PALADIN_TAB_HOLY)
                resolved = isHealer ? specTab : BIS_TAB_NONE;
            else if (specTab == PALADIN_TAB_PROTECTION)
                resolved = isTank ? specTab : BIS_TAB_NONE;
            else
                resolved = (!isTank && !isHealer) ? specTab : BIS_TAB_NONE;
            break;
        case CLASS_PRIEST:
            if (specTab == PRIEST_TAB_SHADOW)
                resolved = (!isTank && !isHealer) ? specTab : BIS_TAB_NONE;
            else
                resolved = isHealer ? specTab : BIS_TAB_NONE;
            break;
        case CLASS_SHAMAN:
            if (specTab == SHAMAN_TAB_RESTORATION)
                resolved = isHealer ? specTab : BIS_TAB_NONE;
            else
                resolved = (!isTank && !isHealer) ? specTab : BIS_TAB_NONE;
            break;
        case CLASS_DRUID:
            if (specTab == DRUID_TAB_RESTORATION)
                resolved = isHealer ? specTab : BIS_TAB_NONE;
            else if (specTab == DRUID_TAB_FERAL)
                resolved = isHealer ? BIS_TAB_NONE : (isTank ? BIS_TAB_DRUID_BEAR : specTab);
            else
                resolved = (!isTank && !isHealer) ? specTab : BIS_TAB_NONE;
            break;
        case CLASS_DEATH_KNIGHT:
            if (specTab == DEATH_KNIGHT_TAB_BLOOD)
                resolved = isHealer ? BIS_TAB_NONE : (isTank ? BIS_TAB_DK_BLOOD_TANK : specTab);
            else
                resolved = (!isTank && !isHealer) ? specTab : BIS_TAB_NONE;
            break;
        // Rogue subtlety has no list at all; it simply finds no rows below.
        case CLASS_ROGUE:
        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            resolved = (!isTank && !isHealer) ? specTab : BIS_TAB_NONE;
            break;
        default:
            break;
    }

    if (resolved == BIS_TAB_NONE)
        return false;

    cls = botClass;
    tab = resolved;
    return true;
}

BisProgress BisListMgr::ProgressForBot(Player* bot)
{
    // Each tier maps to the phase whose content it unlocks, reading IP's own enum comments. The
    // WotLK block reproduces the previous BIS_PHASE_T7 + (tier - IP_TIER_WOTLK) arithmetic exactly,
    // and Vanilla and TBC follow the same shape. A table rather than arithmetic because the three
    // blocks are not evenly spaced and single tiers need retuning without touching the logic.
    static constexpr BisProgress kTierToProgress[IP_TIER_MAX + 1] = {
        {BIS_EXP_VANILLA, 0},  // 0  START           - pre-raid
        {BIS_EXP_VANILLA, 1},  // 1  MOLTEN_CORE
        {BIS_EXP_VANILLA, 2},  // 2  ONYXIA
        {BIS_EXP_VANILLA, 3},  // 3  BLACKWING_LAIR
        {BIS_EXP_VANILLA, 4},  // 4  PRE_AQ
        {BIS_EXP_VANILLA, 5},  // 5  AQ_WAR
        {BIS_EXP_VANILLA, 6},  // 6  AQ              - Naxx40
        {BIS_EXP_VANILLA, 6},  // 7  NAXX40
        {BIS_EXP_TBC, 1},      // 8  PRE_TBC         - Kara / Gruul / Mag
        {BIS_EXP_TBC, 2},      // 9  TBC_TIER_1      - SSC / TK
        {BIS_EXP_TBC, 3},      // 10 TBC_TIER_2      - Hyjal / BT
        {BIS_EXP_TBC, 4},      // 11 (TBC_TIER_3)    - ZA, disabled in IP
        {BIS_EXP_TBC, 5},      // 12 TBC_TIER_4      - Sunwell
        {BIS_EXP_WOTLK, 1},    // 13 TBC_TIER_5      - Naxx / EoE / OS
        {BIS_EXP_WOTLK, 2},    // 14 WOTLK_TIER_1    - Ulduar
        {BIS_EXP_WOTLK, 3},    // 15 WOTLK_TIER_2    - ToC
        {BIS_EXP_WOTLK, 4},    // 16 WOTLK_TIER_3    - ICC
        {BIS_EXP_WOTLK, 5},    // 17 WOTLK_TIER_4    - Ruby Sanctum
        {BIS_EXP_WOTLK, 5},    // 18 WOTLK_TIER_5
    };

    uint8 const tier = std::min<uint8>(sProgressionMgr.GetBotProgressionTier(bot), IP_TIER_MAX);
    return kTierToProgress[tier];
}

uint8 BisListMgr::GetBisRankFor(uint32 itemId, uint8 cls, uint8 tab, BisProgress max, uint8* outPhase) const
{
    if (outPhase)
        *outPhase = 0;

    if (_ranked.empty())
        return 0;

    auto it = _ranked.find(itemId);
    if (it == _ranked.end())
        return 0;

    uint8 best = 0;
    uint8 bestPhase = 0;
    for (RankedEntry const& entry : it->second)
    {
        // Own expansion only. Admitting a lower expansion's tail would hand a level-80 bot the spec
        // gate bypass on ilvl-164 TBC epics, and phase numbers restart per expansion so the caller's
        // decay arithmetic would be comparing unrelated ladders.
        if (entry.expansion != max.expansion)
            continue;

        if (entry.cls != cls || entry.tab != tab || entry.phase > max.phase)
            continue;

        // Latest phase that still lists the item at its best rank: an item re-listed every tier is
        // current BiS, one that only ever held the rank pre-raid is not.
        if (!best || entry.rank < best)
        {
            best = entry.rank;
            bestPhase = entry.phase;
        }
        else if (entry.rank == best && entry.phase > bestPhase)
            bestPhase = entry.phase;
    }

    if (outPhase)
        *outPhase = bestPhase;

    return best;
}

uint8 BisListMgr::GetBisRank(Player* bot, ItemTemplate const* proto, BisProgress max) const
{
    if (!bot || !proto || _ranked.empty())
        return 0;

    // Cheap reject before the talent walk in ResolveSpecKey: most items are on nobody's list.
    if (_ranked.find(proto->ItemId) == _ranked.end())
        return 0;

    uint8 cls = 0;
    uint8 tab = 0;
    if (!ResolveSpecKey(bot, cls, tab))
        return 0;

    return GetBisRankFor(proto->ItemId, cls, tab, max);
}

std::map<uint8, uint32> BisListMgr::GetBisFor(uint16 autoGearScoreLimit, uint8 cls, uint8 tab, uint8 faction) const
{
    auto ilvlIt = _bis.find(autoGearScoreLimit);
    if (ilvlIt == _bis.end())
        return {};

    auto comboIt = ilvlIt->second.find(MakeKey(cls, tab));
    if (comboIt == ilvlIt->second.end())
        return {};

    std::map<uint8, uint32> result;

    // Base: faction=0 (Both).
    auto bothIt = comboIt->second.find(0);
    if (bothIt != comboIt->second.end())
        result = bothIt->second;

    // Faction-specific overrides Both.
    if (faction == 1 || faction == 2)
    {
        auto facIt = comboIt->second.find(faction);
        if (facIt != comboIt->second.end())
            for (auto const& kv : facIt->second)
                result[kv.first] = kv.second;
    }

    return result;
}

std::map<uint8, uint32> BisListMgr::GetBisForNearest(uint16 requestedIlvl, uint16 maxDrop, uint8 cls, uint8 tab,
                                                    uint8 faction, uint16* outResolved) const
{
    uint16 floor = requestedIlvl > maxDrop ? requestedIlvl - maxDrop : 1;
    for (uint16 try_ilvl = requestedIlvl; try_ilvl >= floor; --try_ilvl)
    {
        auto result = GetBisFor(try_ilvl, cls, tab, faction);
        if (!result.empty())
        {
            if (outResolved)
                *outResolved = try_ilvl;
            return result;
        }
        if (try_ilvl == 0)
            break;
    }
    if (outResolved)
        *outResolved = 0;
    return {};
}
