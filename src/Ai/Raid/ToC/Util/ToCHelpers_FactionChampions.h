#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONS_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// Faction Champions (Trial of the Crusader, third encounter). Both faction rosters are listed
// because players always fight the opposing faction's champions. Entries are identical across
// 10/25 and normal/heroic (difficulty is an instance property), so no difficulty variants exist.
enum class ToCFactionChampions : uint32
{
    // Healers (priority kill / crowd-control / interrupt targets)
    NPC_ALLIANCE_DRUID_RESTORATION   = 34469,
    NPC_ALLIANCE_SHAMAN_RESTORATION  = 34470,
    NPC_ALLIANCE_PALADIN_HOLY        = 34465,
    NPC_ALLIANCE_PRIEST_DISCIPLINE   = 34466,
    NPC_HORDE_DRUID_RESTORATION      = 34459,
    NPC_HORDE_SHAMAN_RESTORATION     = 34444,
    NPC_HORDE_PALADIN_HOLY           = 34445,
    NPC_HORDE_PRIEST_DISCIPLINE      = 34447,

    // Damage dealers
    NPC_ALLIANCE_DEATH_KNIGHT        = 34461,
    NPC_ALLIANCE_DRUID_BALANCE       = 34460,
    NPC_ALLIANCE_HUNTER              = 34467,
    NPC_ALLIANCE_MAGE                = 34468,
    NPC_ALLIANCE_PALADIN_RETRIBUTION = 34471,
    NPC_ALLIANCE_PRIEST_SHADOW       = 34473,
    NPC_ALLIANCE_ROGUE               = 34472,
    NPC_ALLIANCE_SHAMAN_ENHANCEMENT  = 34463,
    NPC_ALLIANCE_WARLOCK             = 34474,
    NPC_ALLIANCE_WARRIOR             = 34475,
    NPC_HORDE_DEATH_KNIGHT           = 34458,
    NPC_HORDE_DRUID_BALANCE          = 34451,
    NPC_HORDE_HUNTER                 = 34448,
    NPC_HORDE_MAGE                   = 34449,
    NPC_HORDE_PALADIN_RETRIBUTION    = 34456,
    NPC_HORDE_PRIEST_SHADOW          = 34441,
    NPC_HORDE_ROGUE                  = 34454,
    NPC_HORDE_SHAMAN_ENHANCEMENT     = 34455,
    NPC_HORDE_WARLOCK                = 34450,
    NPC_HORDE_WARRIOR                = 34453,
};

// The mage's only interrupt, one rank
constexpr uint32 SPELL_COUNTERSPELL = 2139;

// True if the entry is any Faction Champion (either faction roster). Pets are not champions.
bool IsFactionChampion(uint32 entry);

// True if the entry is a Faction Champion healer spec (Resto Druid/Shaman, Holy Paladin, Disc Priest)
bool IsFactionChampionHealer(uint32 entry);

// The raid's one kill target, latched per instance. Switches only when it dies, leaves combat or turns
// immune to all damage, or to go back to the one it left over immunity. Null unless the encounter is live.
Unit* FactionChampionsKillTarget(PlayerbotAI* botAI);

// Every non-healer burns the kill target, tanks too: threat is assigned by the script, so nobody
// holds a champion anyway.
bool FactionChampionsFocusBot(PlayerbotAI* botAI);

// Live, and a champion that fears is still up: warlock, disc or shadow priest, warrior.
bool FactionChampionsFearWindowActive(PlayerbotAI* botAI);

// This bot's CC target and its raid icon (RtiTargetValue index). Null when it has none.
Unit* FactionChampionsCcTarget(PlayerbotAI* botAI, uint8& iconIndex);

// Skull off the kill target, a CC icon on a champion no assignment gives it to, or once the pull is
// over an icon this encounter placed still up. Apply does exactly what Pending tests.
bool FactionChampionsMarksPending(PlayerbotAI* botAI);
void FactionChampionsApplyMarks(PlayerbotAI* botAI);

// Assigned: "rti cc" or the group icon doesn't match the assignment yet. Released while live: a
// saved "rti cc" is still waiting to be put back. Apply does exactly what Pending tests.
bool FactionChampionsCcIconPending(PlayerbotAI* botAI);
void FactionChampionsApplyCcIcon(PlayerbotAI* botAI);

// Not live and a saved "rti cc" still waiting. Its node carries no encounter prefix, so it still runs
// after the kill closes the gate, for a CC bot that was dead then.
bool FactionChampionsRtiCcRestorePending(PlayerbotAI* botAI);
void FactionChampionsRestoreRtiCc(PlayerbotAI* botAI);

// This bot is the mage on duty and the kill target is casting a heal Counterspell would stop.
bool FactionChampionsCounterspellDuty(PlayerbotAI* botAI);

}

#endif
