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

// True if the entry is any Faction Champion (either faction roster)
bool IsFactionChampion(uint32 entry);

// True if the entry is a Faction Champion healer spec (Resto Druid/Shaman, Holy Paladin, Disc Priest)
bool IsFactionChampionHealer(uint32 entry);

// True while at least one Faction Champion is alive (the encounter is in progress). Used to gate the
// shared ToC strategy's Faction Champions behaviour so it stays idle during the other three bosses.
bool FactionChampionsEncounterActive(PlayerbotAI* botAI);

// The champion to focus down: the lowest-current-health alive healer, or (once every healer is dead)
// the lowest-current-health champion of any spec. Returns nullptr when no champion is alive.
Unit* GetPriorityFactionChampion(PlayerbotAI* botAI);

// A second alive healer to crowd-control, distinct from killTarget. Returns the highest-health
// off-target healer (so it stays locked longest) or nullptr when fewer than two healers are up.
Unit* GetCcFactionChampionHealer(PlayerbotAI* botAI, Unit* killTarget);

}

#endif
