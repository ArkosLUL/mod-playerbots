/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BISLISTMGR_H
#define PLAYERBOTS_BISLISTMGR_H

#include "Define.h"
#include <map>
#include <unordered_map>
#include <vector>

class Player;
struct ItemTemplate;

// Phase numbering restarts per expansion, so a phase is only meaningful next to one of these.
enum BisExpansion : uint8
{
    BIS_EXP_VANILLA = 0,
    BIS_EXP_TBC = 1,
    BIS_EXP_WOTLK = 2,
    BIS_EXP_COUNT = 3
};

// WotLK phases, ordered so a bot's progression maps onto a ceiling. Vanilla runs PR + P1..P6 and TBC
// runs PR + T4/T5/T6/ZA/SWP over the same 0..N range; only WotLK's are named because it is the only
// expansion the code special-cases.
enum BisPhase : uint8
{
    BIS_PHASE_PRERAID = 0,
    BIS_PHASE_T7 = 1,   // Naxx / EoE / OS
    BIS_PHASE_T8 = 2,   // Ulduar
    BIS_PHASE_T9 = 3,   // Trial of the Crusader
    BIS_PHASE_T10 = 4,  // ICC
    BIS_PHASE_RS = 5,   // Ruby Sanctum
    BIS_PHASE_MAX = BIS_PHASE_RS
};

// Highest phase index each expansion actually has rows for.
constexpr uint8 BIS_MAX_PHASE[BIS_EXP_COUNT] = {6, 5, 5};

// Sentinel tabs for the role splits a talent tab cannot express on its own.
enum BisSpecTab : uint8
{
    BIS_TAB_DRUID_BEAR = 10,
    BIS_TAB_DK_BLOOD_TANK = 11,
    BIS_TAB_WARRIOR_FURY_PROT = 12,  // Vanilla only - a fury-specced tank
    BIS_TAB_NONE = 0xFF
};

// How far a bot has progressed, as a ceiling for list lookups. Lookups never cross expansions, so
// the pair is compared as a unit and phases from different expansions are never ordered against
// each other.
struct BisProgress
{
    uint8 expansion;
    uint8 phase;
};

class BisListMgr
{
public:
    static BisListMgr* instance()
    {
        static BisListMgr inst;
        return &inst;
    }

    void LoadAll();

    // faction: 1=Alliance, 2=Horde. Faction-specific rows override faction=0 (Both).
    // Returns slot -> itemId for the matching auto_gear_score_limit tier. Empty map = no data.
    std::map<uint8, uint32> GetBisFor(uint16 autoGearScoreLimit, uint8 cls, uint8 tab, uint8 faction) const;

    // Closest-lower fallback: scan ilvls down from requested to (requested - maxDrop), return first non-empty set.
    // outResolved receives the matched ilvl (0 if nothing matched within the window).
    std::map<uint8, uint32> GetBisForNearest(uint16 requestedIlvl, uint16 maxDrop, uint8 cls, uint8 tab,
                                             uint8 faction, uint16* outResolved = nullptr) const;

    // Spec key for the ranked lists, or false when the bot must get no BiS signal at all. Refuses when
    // the talent tab and the role the bot actually plays disagree, and for PvP specs (the lists are
    // PvE-only). A wrong key is worse than none: it would bypass the spec gates in the wrong direction.
    static bool ResolveSpecKey(Player* bot, uint8& cls, uint8& tab);

    // Expansion and phase this bot's progression has reached. Without mod-individual-progression the
    // tier falls back to ProgressionTierCap, which clamps here to WotLK RS, i.e. every phase counts.
    static BisProgress ProgressForBot(Player* bot);

    // Best (lowest) rank at or below max, 0 when the item is not listed for this bot's spec.
    uint8 GetBisRank(Player* bot, ItemTemplate const* proto, BisProgress max) const;

    // Same lookup against an already-resolved spec key, for callers that score many items for one bot
    // and should not repeat the talent walk in ResolveSpecKey each time. outPhase receives the latest
    // phase that still lists the item at the returned rank, so callers can tell current BiS from stale;
    // it is only meaningful when the return is non-zero.
    uint8 GetBisRankFor(uint32 itemId, uint8 cls, uint8 tab, BisProgress max, uint8* outPhase = nullptr) const;

    // Listed at or below the bot's own progression phase. The spec gates use this, and they have to
    // agree with the score nudge about what counts as this bot's BiS - answering "any phase" here lets
    // a Naxx-progression bot bypass the gates for gear several tiers past anything it can reach.
    bool IsBisListed(Player* bot, ItemTemplate const* proto) const
    {
        return GetBisRank(bot, proto, ProgressForBot(bot)) != 0;
    }

private:
    BisListMgr() = default;

    void LoadGear();
    void LoadRanked();

    static uint16 MakeKey(uint8 cls, uint8 tab) { return (uint16(cls) << 8) | tab; }

    struct RankedEntry
    {
        uint8 expansion;
        uint8 cls;
        uint8 tab;
        uint8 phase;
        uint8 rank;
    };

    // autoGearScoreLimit -> (cls<<8|tab) -> faction (0/1/2) -> slot -> itemId
    std::map<uint16, std::map<uint16, std::map<uint8, std::map<uint8, uint32>>>> _bis;

    // Keyed by item id because every caller asks about one item at a time. The per-item vector holds
    // one entry per (spec, phase) that lists it - short enough to scan linearly.
    std::unordered_map<uint32, std::vector<RankedEntry>> _ranked;
};

#define sBisListMgr BisListMgr::instance()

#endif
