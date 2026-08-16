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
        "SELECT item_id, class, tab, phase, bis_rank FROM playerbots_bis_ranked");
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
        entry.cls = fields[1].Get<uint8>();
        entry.tab = fields[2].Get<uint8>();
        entry.phase = fields[3].Get<uint8>();
        entry.rank = fields[4].Get<uint8>();

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

uint8 BisListMgr::MaxPhaseForBot(Player* bot)
{
    uint8 const tier = sProgressionMgr.GetBotProgressionTier(bot);

    if (tier < IP_TIER_WOTLK)
        return BIS_PHASE_PRERAID;

    uint8 const phase = static_cast<uint8>(BIS_PHASE_T7 + (tier - IP_TIER_WOTLK));
    return std::min<uint8>(phase, BIS_PHASE_MAX);
}

uint8 BisListMgr::GetBisRankFor(uint32 itemId, uint8 cls, uint8 tab, uint8 maxPhase, uint8* outPhase) const
{
    if (outPhase)
        *outPhase = BIS_PHASE_PRERAID;

    if (_ranked.empty())
        return 0;

    auto it = _ranked.find(itemId);
    if (it == _ranked.end())
        return 0;

    uint8 best = 0;
    uint8 bestPhase = BIS_PHASE_PRERAID;
    for (RankedEntry const& entry : it->second)
    {
        if (entry.cls != cls || entry.tab != tab || entry.phase > maxPhase)
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

uint8 BisListMgr::GetBisRank(Player* bot, ItemTemplate const* proto, uint8 maxPhase) const
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

    return GetBisRankFor(proto->ItemId, cls, tab, maxPhase);
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
