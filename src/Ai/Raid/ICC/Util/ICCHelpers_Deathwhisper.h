/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_DEATHWHISPER_H
#define PLAYERBOTS_ICCHELPERS_DEATHWHISPER_H

#include <vector>

#include "EncounterHelpers.h"
#include "ICCData.h"
#include "Playerbots.h"
#include "RaidBurstWindow.h"

// Base ids. The Spell constructor remaps a cast per difficulty (rows 2123, 2133, 2187, 2188), so each
// is matched through GetSpellIdForDifficulty at its call site.
constexpr uint32 SPELL_LDW_DEATH_AND_DECAY = 71001;
constexpr uint32 SPELL_LDW_FROSTBOLT = 71420;
constexpr uint32 SPELL_LDW_DARK_MARTYRDOM_FANATIC = 71236;
constexpr uint32 SPELL_LDW_DARK_MARTYRDOM_ADHERENT = 70903;
// No difficulty row: one id on every difficulty.
constexpr uint32 SPELL_LDW_DOMINATE_MIND = 71289;
constexpr uint32 SPELL_LDW_SHROUD_OF_THE_OCCULT = 70768;

// Base entries; GetEntry reports these on every difficulty.
constexpr uint32 NPC_LADY_DEATHWHISPER = 36855;
constexpr uint32 NPC_LDW_CULT_FANATIC = 37890;
constexpr uint32 NPC_LDW_DEFORMED_FANATIC = 38135;
constexpr uint32 NPC_LDW_REANIMATED_FANATIC = 38009;  // Fanatic's Determination: no physical damage
constexpr uint32 NPC_LDW_CULT_ADHERENT = 37949;
constexpr uint32 NPC_LDW_EMPOWERED_ADHERENT = 38136;
constexpr uint32 NPC_LDW_REANIMATED_ADHERENT = 38010;  // Adherent's Determination: no magic damage

// Death and Decay is an 8 yd dynobject that also counts the player's 1.5 yd combat reach.
constexpr float LDW_DND_REACH = 10.0f;
// Dark Martyrdom and Dark Reckoning are creature area spells, measured centre to centre.
constexpr float LDW_MARTYRDOM_REACH = 15.0f;
constexpr float LDW_DARK_RECKONING_REACH = 15.0f;
// Vengeful Blast on heroic hits everyone this far from the shade's victim, 10H then 25H.
constexpr float LDW_BLAST_REACH_10H = 15.0f;
constexpr float LDW_BLAST_REACH_25H = 20.0f;
// A shade chases at 5.25 yd/s against a player's 7 and blows up on its first melee swing.
constexpr float LDW_SHADE_TRIGGER = 12.0f;
constexpr float LDW_SHADE_CLEAR = 16.0f;
// Others only clear a heroic blast once the shade is this close to its victim.
constexpr float LDW_BLAST_IMMINENT = 10.0f;
// Park tolerance plus a margin, past each hazard's reach.
constexpr float LDW_DODGE_MARGIN = 2.5f;
constexpr float LDW_DODGE_SWEEP = 30.0f;

// Touch of Insignificance stacks on the tank she holds before a normal-mode taunt swap.
constexpr uint8 LDW_SWAP_STACKS = 3;
constexpr float LDW_TAUNT_RANGE = 30.0f;

// A Frostbolt with this much cast left and no kicker ready is going to land.
constexpr uint32 LDW_FROSTBOLT_LAST_MS = 1100;
constexpr float LDW_LOW_HEALTH_PCT = 35.0f;

// She pulls at 110% of her victim's threat. A damage dealer holds fire from 90% of the top tank's.
constexpr float LDW_THREAT_CAP = 0.9f;
constexpr float LDW_THREAT_RELEASE = 0.8f;
constexpr float LDW_BRAKE_AREA_REACH = 12.0f;

enum class LdwHazardKind : uint8
{
    None,
    DeathAndDecay,
    Martyrdom,
    Shade,
    Blast,
    DarkReckoning
};

struct LdwHazard
{
    Position pos;
    float trigger;
    float clear;
    LdwHazardKind kind;
};

// Her, while her encounter is live and she is alive.
Creature* LdwBoss(PlayerbotAI* botAI);
// Mana Barrier holds: phase 1. Writes ldw.phase.
bool LdwBarrierUp(Player* bot, Unit* boss);

// The bot's pick: a player's skull, then the adds by the bot's damage school, then her. Tanks pick up
// their side's adds while the barrier holds and her afterwards. Null for healers and outside the fight.
// Writes ldw.target.
Unit* LdwTarget(PlayerbotAI* botAI);
// A tank's pick is an add a non-tank holds, in taunt range.
Unit* LdwLooseAddToTaunt(PlayerbotAI* botAI);
// Normal mode, phase 2: this tank takes her off a tank carrying LDW_SWAP_STACKS of Touch of
// Insignificance while carrying none itself, the lowest guid of those that can.
bool LdwSwapTaunt(PlayerbotAI* botAI);
// Phase 2: the tank she is on, else the main tank.
Player* LdwRedirectTank(PlayerbotAI* botAI);

// Assigns each Dominate Mind victim not held by CC one bot caster, best spell first. A victim that
// already had a control aura gets an instant CC, and a Cyclone in its last second hands the duty out
// early, so the follow-up lands the tick the Cyclone fades. Runs from the tick.
void LdwUpdateCcDuties(PlayerbotAI* botAI);
// The victim this bot holds the CC duty on, and the spell it casts. Null without a duty. `waiting`: the
// victim is Cycloned, so the bot only gets into range and sight until the Cyclone fades.
Unit* LdwCcDuty(PlayerbotAI* botAI, char const*& spell, bool& waiting);
// Cancels the form the spell can't be cast from: cat form, Tree of Life, ghost wolf.
void LdwLeaveFormFor(PlayerbotAI* botAI, char const* spell);
// A Dominate Mind victim takes damage within range of the bot or of its target.
bool LdwVictimInSplash(PlayerbotAI* botAI, float range);

// Heroic phase 2 threat brake, set by the tick: this damage dealer holds fire on her.
bool LdwThreatBraked(PlayerbotAI* botAI);

// The button this tank should press now, by what is about to hit it: a Deformed Fanatic in melee, a
// Frostbolt nobody can kick, or low health. 0 when none. `note` writes ldw.defensive.
uint32 LdwTankDefensive(PlayerbotAI* botAI, bool note);

// She casts Frostbolt and this bot is the lowest-guid one with an interrupt ready and in reach. Writes
// ldw.interrupter.
bool LdwIsFrostboltInterrupter(PlayerbotAI* botAI);
// First interrupt the bot can land on her right now.
char const* LdwReadyInterrupt(Player* bot, Unit* boss);

// Every hazard this bot has to clear right now. The victim of a shade dodges the shade, the rest a
// heroic blast around that victim.
std::vector<LdwHazard> LdwHazardsFor(PlayerbotAI* botAI);
// The hazard the bot stands inside the trigger of, None outside every one. Writes ldw.dodge.
LdwHazardKind LdwDodgeKind(PlayerbotAI* botAI);
// Inside some hazard's clearance, where a generic mover would walk the bot back in.
bool LdwNearHazard(PlayerbotAI* botAI);

// Dark Reckoning, on the Deathspeaker High Priests' trash: a marked bot clears every group member, and
// everyone clears a marked tank or player, who stay put.
std::vector<LdwHazard> LdwDarkReckoningHazards(PlayerbotAI* botAI);
bool LdwDarkReckoningDodging(PlayerbotAI* botAI);

// Lust waits for phase 2, when she can be hit and every bot is still alive.
RaidBurstWindow IccDeathwhisperBurstWindow(PlayerbotAI* botAI);

// Heroic buffs, the per-instance reset, the CC duties, the threat brake and the human tank calls, from
// the definition's tick.
void LdwTick(PlayerbotAI* botAI);

inline bool LdwIsEmpty(Position const& pos) { return !pos.GetPositionX() && !pos.GetPositionY(); }

#endif
