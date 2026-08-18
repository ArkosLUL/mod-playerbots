/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERIRONASSEMBLY_H
#define PLAYERBOTS_ULDENCOUNTERIRONASSEMBLY_H

#include "Position.h"
#include "UldBossHelper.h"

#include <vector>

class Player;
class PlayerbotAI;
class Unit;

// Assembly of Iron.
//
// Three bosses on one health pool in every way that matters: killing one restores the other two to
// full and hands them a Supercharge stack, so damage spread across members is simply thrown away and
// focus fire is the whole encounter. The last one alive reaches phase 3 and is the only one that
// drops loot.
//
// Kill order is the raid's choice, and it is the only thing the hard-mode config flag changes:
// Steelbreaker first normally, Steelbreaker last for the hard mode.

struct IronAssemblyTargets
{
    Unit* steelbreaker = nullptr;
    Unit* molgeim = nullptr;
    Unit* brundir = nullptr;

    uint8 AliveCount() const;
};

// By entry, never "find target": that value walks only the bot's own threat list and matches on the
// localized creature name. Brundir wipes his threat table every time he lands from Lightning
// Tendrils, so a name lookup goes silently inert for the rest of the fight.
Unit* GetIronAssemblyMember(PlayerbotAI* botAI, uint32 entry);
void GatherIronAssemblyTargets(PlayerbotAI* botAI, IronAssemblyTargets& targets);
bool IronAssemblyEncounterActive(PlayerbotAI* botAI);

// Gate for everything that positions a bot or picks its target. The raid strategy runs in the
// non-combat engine too, and the council is visible from out in the corridor, so a presence gate has
// bots walking the formation through the hall doors and opening on the council before the pull.
bool IronAssemblyFormationActive(PlayerbotAI* botAI);

// True once Brundir is the only member left. His isolation protects nothing at that point, so the
// raid closes onto a second stack point instead of holding the opening one.
bool IronAssemblyBrundirIsLast(PlayerbotAI* botAI);

// What every non-tank should be hitting. A skull a human raid leader set on a living member wins;
// otherwise this is the configured kill order. Never null while any member is alive, which is what
// makes the unconditional targeting suppression safe.
Unit* IronAssemblyFocusTarget(PlayerbotAI* botAI);

// Which member this bot tanks, or nullptr. Assignment needs at least two tanks: with one there is
// nothing to split, so the encounter keeps its hands off and the generic logic runs. Surplus tanks
// get nullptr and fall through to damage on the focus target.
Unit* IronAssemblyAssignedBoss(PlayerbotAI* botAI, Player* bot);
bool TryGetIronAssemblyTankSpot(PlayerbotAI* botAI, Player* bot, Position& position);

// Where ranged and healers stand. Stacked normally; on the hard-mode spread ring once Steelbreaker
// is empowered, because Static Disruption only exists from his phase 2.
bool TryGetIronAssemblyRaidSpot(PlayerbotAI* botAI, Player* bot, Position& position);

bool IronAssemblyOverloadActive(Unit* brundir);
bool IronAssemblyTendrilsActive(Unit* brundir);

// Rune of Death is a persistent area aura, so it is a DynamicObject and AvoidAoeAction cannot see
// it. Both difficulty ids are swept.
void GatherIronAssemblyRunesOfDeath(Player* bot, std::vector<Position>& runes);
bool IsIronAssemblyPositionClearOfRunes(Position const& spot, std::vector<Position> const& runes);

// The council member currently standing in Molgeim's Rune of Power, if any. Molgeim casts it on
// DoSelectLowestHpFriendly, so it lands on a boss rather than a player - which is why the tank has
// to walk his boss out of it while everyone else walks in.
Unit* IronAssemblyRuneOfPowerCarrier(PlayerbotAI* botAI);
bool TryGetIronAssemblyRuneOfPowerSoakSpot(PlayerbotAI* botAI, Player* bot, Position& position);

// A bot committed to a hazard cannot also be the interrupter: it is walking, and a moving bot casts
// nothing. Derived from the hazards themselves rather than isMoving(), so every bot reaches the same
// answer about every other bot.
bool IronAssemblyMemberMustMove(PlayerbotAI* botAI, Player* member);

// The interrupt this bot could land on Brundir right now, or nullptr. Stuns are included and
// silences are not: creature_immunities gives Brundir mask 0x24CB375F, which carries SILENCE but
// neither STUN nor INTERRUPT - so kick and hammer of justice both connect while silencing shot and
// spell lock never do. Steelbreaker and Molgeim carry STUN and INTERRUPT too and are unreachable by
// any of it, which is why only Brundir has an interrupt node.
char const* IronAssemblyReadyInterrupt(Player* bot, Unit* target);

// Rank among the bots that could interrupt right now, lowest guid first. Rank 0 owns Lightning
// Whirl, rank 1 owns Chain Lightning, and nobody else acts - so when only one interrupt is off
// cooldown Chain Lightning is deliberately allowed through, which is what the written strategies
// mean by "let some of them cast". Returns false when this bot has no duty.
bool IronAssemblyInterruptRank(PlayerbotAI* botAI, Player* bot, Unit* brundir, uint8& rank);

bool IronAssemblyLightningWhirlActive(Unit* brundir);
bool IronAssemblyChainLightningCasting(Unit* brundir);

// Shield of Runes absorbs 20,000 and pays Molgeim +50% damage for 15s if it is drained rather than
// removed, so stripping it early is worth more than the dispel costs.
bool IronAssemblyShieldOfRunesUp(Unit* molgeim);
bool IronAssemblyHasFusionPunch(Unit* unit);
bool IronAssemblyHasOverwhelmingPower(Unit* unit);

// Drop this instance's state once the council is back at full health, never on "nobody in combat":
// Brundir spends 16s of every Lightning Tendrils out of combat by design, and the core carries the
// same warning about its own reset path.
void ResetIronAssemblyEncounterState(Player* bot, bool clearInstance);
bool IronAssemblyEncounterStateIsStale(PlayerbotAI* botAI);
// Nothing to reset means nothing to do, which keeps the reset node from swallowing every tick
// before the pull, when all three are alive at full health by definition.
bool IronAssemblyBotHasEncounterState(Player* bot);

#endif
