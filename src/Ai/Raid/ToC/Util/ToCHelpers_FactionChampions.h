#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONS_H

#include <functional>
#include <vector>

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
// One rank, and damage doesn't break it
constexpr uint32 SPELL_CYCLONE = 33786;
// A pulled kill target this low sends the grip DK ahead to the next healer, which CC then leaves alone
constexpr float NEXT_GRIP_WALK_PCT = 40.0f;

// True if the entry is any Faction Champion (either faction roster). Pets are not champions.
bool IsFactionChampion(uint32 entry);

// True if the entry is a Faction Champion healer spec (Resto Druid/Shaman, Holy Paladin, Disc Priest)
bool IsFactionChampionHealer(uint32 entry);

// DK, Warrior, Rogue, Enhancement Shaman or Retribution Paladin
bool IsFactionChampionMelee(uint32 entry);

// The raid's one kill target, latched per instance. Switches only when it dies, leaves combat or turns
// immune to all damage, to go back to the one it left over immunity, to a player's skull on a
// champion that can be hurt, or off one still in the pack to a healer pulled out of it, a human
// DK's grip included. Null unless the encounter is live.
Unit* FactionChampionsKillTarget(PlayerbotAI* botAI);

// The champion the kill latch left over immunity and goes back to once it can be hurt again. Null
// unless the encounter is live.
Unit* FactionChampionsSuspendedTarget(PlayerbotAI* botAI);

// First alive, in-combat champion in kill order that accept takes, skipping the kill target, the
// suspended one and any FactionChampionsCcProtected. Null unless the encounter is live.
Unit* FactionChampionsNextTarget(PlayerbotAI* botAI, std::function<bool(Unit*)> const& accept);

// Every alive, in-combat champion in kill order, pets left out. Empty unless the encounter is live.
std::vector<Unit*> FactionChampionsInCombat(PlayerbotAI* botAI);

// On heroic while live, the taunt or threat spell this bot's pet autocasts, else 0. Champions are
// immune to taunt there, and a pet's threat never reaches the 10M the script seeds each player at.
uint32 FactionChampionsPetTauntAutocast(PlayerbotAI* botAI);

// Every non-healer bot but the pack tanks burns the kill target
bool FactionChampionsFocusBot(PlayerbotAI* botAI);

// Bot tanks hold the pack where it landed, away from the kill target
bool FactionChampionsIsPackTank(Player* player);

// While live: where the champions landed, and the middle of those still within 25 yd of it, the kill
// target left out
bool FactionChampionsPack(PlayerbotAI* botAI, Position& anchor, Position& centre);

// A healer champion 20+ yd off the pack centre with no champion within 12 yd
bool FactionChampionsIsPulled(PlayerbotAI* botAI, Unit* champion);

// A champion 20+ yd off where the pack landed whose victim isn't a tank
bool FactionChampionsIsEscapee(PlayerbotAI* botAI, Unit* champion);

// How long the encounter has been live, 0 when it isn't
uint32 FactionChampionsLiveMs(PlayerbotAI* botAI);

// First healer in kill order still in the pack, not the kill target or the suspended one
Unit* FactionChampionsNextHealerInPack(PlayerbotAI* botAI);

// Live, and a champion that fears is still up: warlock, disc or shadow priest, warrior.
bool FactionChampionsFearWindowActive(PlayerbotAI* botAI);

// This bot's CC cast, the champion and spell id, when it holds a CC duty. CC bots are dps mages
// (Polymorph), warlocks (Fear, Banish on an elemental or demon) and ranged druids (Cyclone). None for
// 1.5 s after the pull, while a human's opening grip lands. Candidates skip the kill target, the
// suspended, the skulled and a pulled one, and the next healer in the pack once the kill target is
// pulled and at 40% or under: healers off the tanks nearest the kill target, other healers, champions
// off the tanks casters first, the rest in kill order. A cast at a champion that stops being a
// candidate is interrupted, so it never makes a new kill target immune. One duty per
// candidate, none while it is CC'd for over 2 s more or a CC cast at it is under way. The caster is in
// range and sight with a spell that lands and, by DR, holds over 2.5 s: never-breaking first, then the
// longest, then the nearest. Polymorph and Fear only on a champion 12+ yd off every pack tank, with no
// group member or pet on it and no group DoT. Latched until the cast lands or 3 s pass with none going,
// written to fc.cc. Null unless the encounter is live.
Unit* FactionChampionsCcTarget(PlayerbotAI* botAI, uint32& spellId);

// The member holds a CC duty
bool FactionChampionsHasCcDuty(PlayerbotAI* botAI, ObjectGuid member);

// A player's CC on it, or a CC duty's cast at it, breaks on damage
bool FactionChampionsCcProtected(PlayerbotAI* botAI, Unit* unit);

// This bot is the mage on duty and the kill target is casting a heal Counterspell would stop.
bool FactionChampionsCounterspellDuty(PlayerbotAI* botAI);

}

#endif
