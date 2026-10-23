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
// Cast-time spells get kicked this close to their end, so the champion wastes the cast. A few 100 ms bot
// ticks of slack. Channels go at once.
constexpr int32 LATE_INTERRUPT_MS = 600;
// One rank, and damage doesn't break it
constexpr uint32 SPELL_CYCLONE = 33786;
// Heroic champions cast it on their first fear, stun, confuse or freeze, then 2 min cooldown
// (boss_faction_champions.cpp IsCCed). It also clears every snare and root.
constexpr uint32 SPELL_PVP_TRINKET = 65547;

// True if the entry is any Faction Champion (either faction roster). Pets are not champions.
bool IsFactionChampion(uint32 entry);

// True if the entry is a Faction Champion healer spec (Resto Druid/Shaman, Holy Paladin, Disc Priest)
bool IsFactionChampionHealer(uint32 entry);

// DK, Warrior, Rogue, Enhancement Shaman or Retribution Paladin
bool IsFactionChampionMelee(uint32 entry);

// The raid's one kill target, latched per instance. Switches only when it dies, leaves combat or turns
// immune to all damage, to go back to the one it left over immunity, to a player's skull on a
// champion that can be hurt, or off one never pulled out to a healer pulled out of the pack, a human
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

// Every non-healer bot holding no champion burns the kill target
bool FactionChampionsFocusBot(PlayerbotAI* botAI);

// Bot tanks. Their Consecration and Death and Decay break CC and roots round them
bool FactionChampionsIsPackTank(Player* player);

// The bot DK that grips second in every chain: lowest guid, not a tank, knows Death Grip. Empty when none
ObjectGuid FactionChampionsChainDk(Player* bot);

struct FactionChampionsHold
{
    Unit* champion = nullptr;
};

// The melee champion this bot holds, fought wherever it is. Each melee champion gets one holder, in the
// order DK, Warrior, Rogue, Retribution, Enhancement: plate DPS first, then cats, then bot tanks, the nearest
// within a tier. The chain DK takes the DK once its grip is out, and on the Warrior a warrior without Mortal
// Strike goes first. Written to fc.hold. Empty unless live.
FactionChampionsHold FactionChampionsHoldOf(PlayerbotAI* botAI);

// The champion this member holds, as of the last refresh. Empty for none
ObjectGuid FactionChampionsHeldBy(Player* member);

// The member holds a melee champion, as of the last refresh
bool FactionChampionsIsHolder(Player* member);

// The champion has a holder
bool FactionChampionsIsHeld(PlayerbotAI* botAI, Unit* champion);

// Heroic, and the champion's PvP trinket is off cooldown
bool FactionChampionsTrinketReady(Unit* champion);

// Fears, stuns, confuses or freezes (a frost root or stun), itself or one trigger down: the champion
// trinkets out of it
bool FactionChampionsSpellPopsTrinket(uint32 spellId);

// Live, and a champion healer is still up
bool FactionChampionsHealerLeft(PlayerbotAI* botAI);

// While live: where the champions landed, and the middle of the pack, which follows it wherever it walks:
// those within 20 yd of the last centre, the kill target left out
bool FactionChampionsPack(PlayerbotAI* botAI, Position& anchor, Position& centre);

// A healer champion gripped off the pack (10+ yd inside 1 s), still 20+ yd off its centre with no champion
// within 12 yd
bool FactionChampionsIsPulled(PlayerbotAI* botAI, Unit* champion);

// The kill target has been pulled out at least once, even if the pack has walked onto it since
bool FactionChampionsKillTargetOut(PlayerbotAI* botAI);

// A champion 20+ yd off the pack centre whose victim isn't a tank or a holder
bool FactionChampionsIsEscapee(PlayerbotAI* botAI, Unit* champion);

// How long the encounter has been live, 0 when it isn't
uint32 FactionChampionsLiveMs(PlayerbotAI* botAI);

// Live, and a champion that fears is still up: warlock, disc or shadow priest, warrior.
bool FactionChampionsFearWindowActive(PlayerbotAI* botAI);

// This bot's CC cast, the champion and spell id, when it holds a CC duty. CC bots are dps mages
// (Polymorph), warlocks (Fear, Banish on an elemental or demon), ranged druids (Cyclone) and dps shamans
// (Hex). None for 1.5 s after the pull, while the opening grips land. Candidates skip the kill target, the
// suspended, the skulled, a pulled and a held one, and a champion the grip guard holds: healers off the
// tanks and holders nearest the kill target, other healers, the danger champion, champions off the tanks
// and holders casters first, the rest in kill order. A cast at a champion that stops being a candidate is interrupted, so it
// never makes a new kill target immune. One duty per candidate, none while it is CC'd for over 2 s more,
// a CC cast at it is under way or it is in Bladestorm. Each of these spells holds one target per caster,
// so a caster whose spell holds a champion for longer than its cast time plus 0.4 s casts it nowhere
// else, unless it moves off a non-healer onto a healer. The caster has a spell that lands and, by DR, holds over 2.5 s, and is in range and sight or
// up to 15 yd past the range, which it walks, never from a Cyclone stand: in range first, then
// never-breaking, then the longest, then the nearest. Polymorph, Fear and Hex only on a champion with no
// group member or pet on it and no group DoT. Latched until a fresh one lands (over its cast time plus
// 0.4 s left) or 3 s pass with none going
// (6 s for a walk), written to fc.cc. Null unless the encounter is live.
Unit* FactionChampionsCcTarget(PlayerbotAI* botAI, uint32& spellId);

// The member holds a CC duty
bool FactionChampionsHasCcDuty(PlayerbotAI* botAI, ObjectGuid member);

// A player's CC on it, or a CC or peel duty's spell at it, breaks on damage
bool FactionChampionsCcProtected(PlayerbotAI* botAI, Unit* unit);

// The spell's own aura ends on a hit, or past its damage cap
bool FactionChampionsBreaksOnDamage(uint32 spellId);

// A player's, pet's, totem's or guardian's periodic damage is on it
bool FactionChampionsCarriesGroupDot(Unit* unit);

// This bot is the mage on duty and the kill target is casting a heal Counterspell would stop: a channel
// at once, a cast in its last LATE_INTERRUPT_MS
bool FactionChampionsCounterspellDuty(PlayerbotAI* botAI);

// This mage gets the Counterspell once the kill target's heal reaches its last LATE_INTERRUPT_MS, so it
// starts no cast meanwhile
bool FactionChampionsCounterspellHold(PlayerbotAI* botAI);

// A ranged dps druid bot that knows Cyclone
bool FactionChampionsCycloneBot(Player* player);

// The champion healer in the pack nearest the kill target: not the kill target, pulled or suspended. Null
// unless live
Unit* FactionChampionsBlobHealer(PlayerbotAI* botAI);

// The melee champion kept stunned, CC'd or disarmed: first alive of rogue, DK, Ret, warrior that has no
// holder and isn't the kill target, pulled or suspended, and not CC'd or disarmed while another one isn't.
// Written to fc.danger. Null unless live
Unit* FactionChampionsDangerTarget(PlayerbotAI* botAI);

}

#endif
