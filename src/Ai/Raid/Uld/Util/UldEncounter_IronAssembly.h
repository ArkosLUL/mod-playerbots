/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERIRONASSEMBLY_H
#define PLAYERBOTS_ULDENCOUNTERIRONASSEMBLY_H

#include "Position.h"
#include "UldData.h"

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
//
// Reading a pull back: postmortem.py <file> --notes ironassembly.
//
//   ironassembly.alive      which members are up - bit 0 Steelbreaker, 1 Molgeim, 2 Brundir
//   ironassembly.focus      what the raid is killing, and whether a human's skull beat the order
//   ironassembly.tank       the boss a bot tank owns, or the branch that left it without one. Only
//                           bot tanks are ranked, so a row per bot tank and none for a human one
//   ironassembly.interrupt  the duty a bot holds for Brundir's current cast
//   ironassembly.spot       the formation branch that put a bot where it stands - a `-rune` or
//                           `-overload` suffix names the hazard that covered the stack point and
//                           pushed the whole formation onto the shift ring
//   ironassembly.slot       its index on the hard-mode spread ring
//   ironassembly.soak       whether it walked into Rune of Power, and what stopped it
//
// Overload, Lightning Tendrils and Meltdown also write haz circles, because none of the three has a
// world object for the snapshot sweep to find. Rune of Death and Rune of Power do, and are left to
// it - only a swept hazard is tested against a death.

enum UlduarIronAssemblyIds
{
    // Iron Assembly. The council script casts through Unit::CastSpell, which difficulty-maps every
    // id, so each pair below is 10-man then 25-man and callers test both. The auras come first, the
    // damage they trigger after them - 63485 and 61886 are the Tendrils damage triggers and were
    // once mislabelled here as extra Overload auras, which they never are.
    SPELL_LIGHTNING_TENDRILS_10_MAN = 61887,
    SPELL_LIGHTNING_TENDRILS_25_MAN = 63486,
    SPELL_OVERLOAD_10_MAN = 61869,
    SPELL_OVERLOAD_25_MAN = 63481,
    SPELL_CHAIN_LIGHTNING_10_MAN = 61879,
    SPELL_CHAIN_LIGHTNING_25_MAN = 63479,
    SPELL_LIGHTNING_WHIRL_10_MAN = 61915,
    SPELL_LIGHTNING_WHIRL_25_MAN = 63483,
    SPELL_RUNE_OF_DEATH_10_MAN = 62269,
    SPELL_RUNE_OF_DEATH_25_MAN = 63490,
    SPELL_SHIELD_OF_RUNES_10_MAN = 62274,
    SPELL_SHIELD_OF_RUNES_25_MAN = 63489,
    SPELL_FUSION_PUNCH_10_MAN = 61903,
    SPELL_FUSION_PUNCH_25_MAN = 63493,
    SPELL_OVERWHELMING_POWER_10_MAN = 64637,
    SPELL_OVERWHELMING_POWER_25_MAN = 61888,
    // The rune's ground pulse, reapplied every 0.8s to anything standing within 5 yd of it. One id
    // for both raid sizes, and it is what marks a boss as standing in his own damage buff.
    SPELL_RUNE_OF_POWER = 64320,
    // The rune itself. 61973 force-casts this, and it is the only id in the chain with a persistent
    // area aura effect, so it is the one that leaves a DynamicObject on the floor to sweep for. The
    // boss carrying 64320 above can walk off it; the object stays where it was dropped.
    SPELL_RUNE_OF_POWER_AREA = 63513,
    // The damage the auras above trigger. Nothing here is ever cast or tested for - these name the
    // hazard in a trace, where the row has to join onto the damage record that explains it. Only
    // Tendrils is a pair: both Overload auras trigger 61878 and both Overwhelming Power auras
    // trigger 61889.
    SPELL_OVERLOAD_DAMAGE = 61878,
    SPELL_LIGHTNING_TENDRILS_DAMAGE_10_MAN = 61886,
    SPELL_LIGHTNING_TENDRILS_DAMAGE_25_MAN = 63485,
    SPELL_MELTDOWN = 61889,
    // NPC_STEELBREAKER / NPC_MOLGEIM / NPC_BRUNDIR come from core ulduar.h via UldScripts.h
};

// Assembly of Iron. Every distance is measured against the spell that motivates it, and the whole
// formation is bounded by what map 603 actually has floor for: from the anchor below, navprobe
// reports 8/8 headings on mesh at 20 and 30 yd, but at 40 the 45 and 135 degree diagonals settle to
// Z -27.7 and -438, and at 50 three of eight headings leave the mesh entirely. Nothing here sits
// outside 30 yd, and the formation uses cardinals so no slot can drift onto a bad diagonal.

// Overload 61878 is 20,000 nature plus a knockdown in 20 yd. Lightning Tendrils 61886/63485 is
// 3000 (10-man) / 5000 (25-man) a second in 18 - not the 10 yd the written guides give, which is
// 61884, a dummy. The extra yards are arrival slack: a bot that stops on the boundary is still in.
constexpr float ULDUAR_IRON_ASSEMBLY_OVERLOAD_RADIUS = 20.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_OVERLOAD_CLEARANCE = 25.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_TENDRILS_RADIUS = 18.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_TENDRILS_CLEARANCE = 28.0f;

// Rune of Death 62269/63490: a 13 yd persistent area aura ticking 2750 shadow every half second for
// 30s. Search wide enough to see one dropped anywhere in the formation.
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_RADIUS = 13.0f;

// 13 is the DBC radius, but the searcher that applies the aura adds object size on both ends and two
// traces measured applications out to 15.4 yd. Run when inside DANGER, stand at CLEARANCE: without
// the gap a bot parks exactly on the boundary, because the escape search returns the nearest spot
// that clears it, and re-takes the rune on every yard of drift.
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_DANGER_RADIUS = 16.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE = 21.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_SEARCH_RADIUS = 40.0f;

// Where the raid stands when a hazard covers the stack point - a Rune of Death, or Brundir's Overload
// when he is parked anywhere near the raid. Both are the common path rather than an edge case:
// Molgeim drops the rune on a random member, so on a stacked raid it lands on the stack, and one
// traced pull had five of five Overloads cover it with 22-24 of 25 members inside.
//
// A fixed candidate set rather than free geometry because every bot picks its own and they have to
// agree: the eight anchor headings at 25 yd, which navprobe reports 8/8 on mesh with settledZ on the
// floor (20 and 30 are clean too, so the ring has room either side).
constexpr uint8 ULDUAR_IRON_ASSEMBLY_STACK_SHIFT_HEADINGS = 8;
constexpr float ULDUAR_IRON_ASSEMBLY_STACK_SHIFT_RADIUS = 25.0f;

// Meltdown 61889 is 29,250 nature in 15 yd, centred on whoever Overwhelming Power expires on. The
// carrier dies either way - walking this far is what stops it taking the melee with them, and every
// death it causes is another permanent +25% on Steelbreaker via Electrical Charge.
constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_RADIUS = 15.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_CLEARANCE = 20.0f;

// Rune of Power pulses 64320 to everything within 5 yd, worth +50% damage, and the rune lives 60s.
// Molgeim drops it on DoSelectLowestHpFriendly, which is a council member rather than a player, so
// the ranged walk in and the tank's spot shifts clear. Capped travel matters: without the cap a rune
// landing on Brundir would drag the entire ranged group into Overload range.
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_RADIUS = 5.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_SOAK_MAX_TRAVEL = 25.0f;

// How far the tank's spot has to sit from a rune his own boss is standing in: the 5 yd rune, plus the
// 5 the boss stops short at when it follows him there, plus margin. The tank walks to a shifted spot
// instead of running a second node that pushes the boss away - two movers at MOVEMENT_COMBAT trade
// the slot and cancel, and a traced pull spent five seconds alternating while the boss never left the
// rune at all and Overload landed on the melee standing in it.
constexpr float ULDUAR_IRON_ASSEMBLY_TANK_RUNE_CLEARANCE = 12.0f;

// Candidate spots are the designed bearing rotated by these, in order, at the boss's own radius, so a
// shifted tank keeps his distance from the anchor and the raid. navprobe: both the 16 and 28 yd rings
// are 16/16 on mesh at 22.5 degree steps, every heading settling on the floor at Z 427.27.
// Displacement is 2*R*sin(step/2), so two steps clears 12 yd on either ring - 21.4 at 28, 12.2 at 16 -
// and the third is spare. Stopping at three matters: the melee spots are 90 degrees apart, so a fourth
// would put Steelbreaker's furthest candidate exactly on Molgeim's spot. Three also keeps every
// Brundir candidate at least 33 yd from the stack, well outside his own Overload.
constexpr float ULDUAR_IRON_ASSEMBLY_TANK_SHIFT_STEP = 0.3927f;  // pi/8, 22.5 degrees
constexpr uint8 ULDUAR_IRON_ASSEMBLY_TANK_SHIFT_STEPS = 3;

// Formation, all on cardinal bearings from the anchor. Brundir is parked at 28 rather than the 25
// his own Overload needs, so the stack sits 38 yd off him and never has to react to it at all -
// which is what keeps a bot standing still and able to interrupt Lightning Whirl.
constexpr float ULDUAR_IRON_ASSEMBLY_BRUNDIR_BEARING = 0.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_BRUNDIR_RADIUS = 28.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_STEELBREAKER_BEARING = 2.3562f;  // 3*pi/4
constexpr float ULDUAR_IRON_ASSEMBLY_MOLGEIM_BEARING = 3.9270f;       // 5*pi/4
constexpr float ULDUAR_IRON_ASSEMBLY_MELEE_BOSS_RADIUS = 16.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_STACK_BEARING = 3.1416f;         // pi
constexpr float ULDUAR_IRON_ASSEMBLY_STACK_RADIUS = 10.0f;
// Once Brundir is the last one up his isolation protects nothing, so the raid closes to a second
// point 25 yd short of him: outside Overload, inside caster range of him.
constexpr float ULDUAR_IRON_ASSEMBLY_BRUNDIR_LAST_STACK_RADIUS = 3.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_SLOT_TOLERANCE = 3.0f;

// The hall's two doors spawn at (1671.31, 120.70) and (1501.49, 119.70), 84 and 86 yd from the
// anchor, so this bubble stops short of both. Sight range is 100 yd with no line-of-sight check, so
// without it the council drives bot behaviour from out in the corridor.
constexpr float ULDUAR_IRON_ASSEMBLY_ARENA_RADIUS = 78.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_ARENA_HEIGHT = 10.0f;
// A tank that has drifted this far off its spot is walking, not parked.
constexpr float ULDUAR_IRON_ASSEMBLY_TANK_SPOT_TOLERANCE = 4.0f;

// Static Disruption 61912/63494 is 5000 nature in 6 yd plus +75% nature damage taken in 5, and it
// picks a target beyond 10 yd - so it is a ranged and healer problem, never a melee one. It only
// exists from Steelbreaker's phase 2, which the normal kill order never reaches, so this ring is
// hard mode only and everyone stacks otherwise. 16 slots at 18 yd sit 7.0 yd apart and none of them
// lands further than 29.4 yd from Steelbreaker's spot, inside caster range.
constexpr float ULDUAR_IRON_ASSEMBLY_SPREAD_RING_RADIUS = 18.0f;
constexpr uint8 ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS = 16;

// Overload, Lightning Tendrils and Meltdown have no world object behind them, so a trace can only
// know their geometry if the encounter writes it. RaidObs::NoteHazard emits on every call, so this
// paces the rows per instance and doubles as their ttl. A second is fine for a 6s channel and tracks
// Brundir closely enough as he drifts through a 16s Tendrils.
constexpr uint32 ULDUAR_IRON_ASSEMBLY_HAZARD_NOTE_INTERVAL_MS = 1000;

// Assembly of Iron room centre, read off Brundir's out-of-combat channel wander in
// boss_assembly_of_iron.cpp. navprobe: on mesh, vmap floor 427.267, and see the ring results above.
extern const Position ULDUAR_IRON_ASSEMBLY_ANCHOR;

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

// Which member this bot tanks, or nullptr. Bot tanks are ranked among themselves by guid and claim
// from the front of Brundir, Steelbreaker, Molgeim - a lone one still takes Brundir, since the other
// two drift onto it anyway and the assignment really decides where the council parks. Surplus tanks
// get nullptr and fall through to damage on the focus target.
Unit* IronAssemblyAssignedBoss(PlayerbotAI* botAI, Player* bot);

// Where that tank stands, so the boss follows him there rather than being dragged through the raid.
// Rotated off the designed bearing when his own boss is standing in a Rune of Power, which is the
// whole of the answer to that rune - there is no second node pushing the boss around.
bool TryGetIronAssemblyTankSpot(PlayerbotAI* botAI, Player* bot, Position& position);

// Where ranged and healers stand. Stacked normally; on the hard-mode spread ring once Steelbreaker
// is empowered, because Static Disruption only exists from his phase 2.
bool TryGetIronAssemblyRaidSpot(PlayerbotAI* botAI, Player* bot, Position& position);

bool IronAssemblyOverloadActive(Unit* brundir);
bool IronAssemblyTendrilsActive(Unit* brundir);

// Rune of Death is a persistent area aura, so it is a DynamicObject and AvoidAoeAction cannot see
// it. Both difficulty ids are swept.
void GatherIronAssemblyRunesOfDeath(Player* bot, std::vector<Position>& runes);

// Callers pass the radius they mean: DANGER to ask whether a bot has to move, CLEARANCE to ask
// whether a spot is somewhere to stand. Asking both questions at one radius is what left bots
// parked on the rune's edge.
bool IsIronAssemblyPositionClearOfRunes(Position const& spot, std::vector<Position> const& runes,
                                        float radius);

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

// Which of Brundir's two casts this bot is on the hook for right now - "whirl", "chain", or nullptr
// for no duty. Ranked by guid among the bots whose interrupt is off cooldown: rank 0 owns Lightning
// Whirl, rank 1 owns Chain Lightning, and nobody else acts, so with only one interrupt available
// Chain Lightning is deliberately allowed through rather than spending the cooldown the next Whirl
// needs. That is what the written strategies mean by "let some of them cast".
char const* IronAssemblyInterruptDuty(PlayerbotAI* botAI, Player* bot, Unit* brundir);

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
