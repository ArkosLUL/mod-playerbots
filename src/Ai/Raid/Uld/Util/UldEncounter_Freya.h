/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERFREYA_H
#define PLAYERBOTS_ULDENCOUNTERFREYA_H

#include "Position.h"
#include "UldData.h"

#include <vector>

class GameObject;
class Player;
class PlayerbotAI;
class Unit;

// Freya.
//
// Waves of adds define the fight. The trio wave - Snaplasher, Storm Lasher, Ancient Water Spirit -
// has to die within seconds of each other or the survivors heal back, so the trio is killed in sync
// rather than one at a time. Lasher packs are stacked and AoE'd instead of walked down, and the
// Ancient Conservator is answered with Healthy Spores rather than by out-damaging its Grip.
//
// Everything the encounter needs comes from one grid pass (FreyaWaveState) so the priority action,
// the tank action and both multipliers cannot disagree about what is up.

enum UlduarFreyaIds
{
    NPC_SNAPLASHER = 32916,
    NPC_STORM_LASHER = 32919,
    NPC_DETONATING_LASHER = 32918,
    NPC_ANCIENT_WATER_SPIRIT = 33202,
    NPC_ANCIENT_CONSERVATOR = 33203,
    NPC_HEALTHY_SPORE = 33215,
    NPC_EONARS_GIFT = 33228,
    GOBJECT_NATURE_BOMB = 194902,
    // +8% healing received per stack, 150 stacks on engage: Freya cannot be killed until the wave
    // adds have taken it off her, so damage on her before then is wasted.
    SPELL_ATTUNED_TO_NATURE = 62519,

    // Freya's only telegraph worth reacting to, and hard mode only. A 2s cast carrying Effect_2 = 68
    // SPELL_EFFECT_INTERRUPT_CAST alongside its damage, at radius index 28 = 50000 yd: raid-wide, so
    // there is nothing to dodge, and it school-locks whoever it cuts for 10s. 62437 is 10-man, 62859
    // is 25-man. Hunters are exempt - Steady Shot is PreventionType PACIFY, which never reaches the
    // lockout branch.
    SPELL_FREYA_GROUND_TREMOR_10 = 62437,
    SPELL_FREYA_GROUND_TREMOR_25 = 62859,

    // Freya hard mode: Elders left alive permanently empower Freya with an extra ability each.
    // NPC_FREYA comes from core ulduar.h via UldScripts.h.
    NPC_FREYA_IRON_ROOTS = 33088,               // Ironbranch's Iron Roots trap (selectable)
    NPC_FREYA_STRENGTHENED_IRON_ROOTS = 33168,  // Freya's empowered Iron Roots trap (selectable)
    NPC_FREYA_SUN_BEAM = 33170,                 // Freya's Unstable Sun Beam stalker (non-selectable)
    NPC_FREYA_UNSTABLE_SUN_BEAM = 33050,        // Brightleaf's Unstable Sun Beam stalker (non-selectable)
    SPELL_IRON_ROOTS_DAMAGE = 62283,            // DoT on a player trapped by Ironbranch's roots
    SPELL_IRON_ROOTS_FREYA_DAMAGE = 62861,      // DoT on a player trapped by Freya's roots

    // Applied to allies within 6 yd of a Healthy Spore; grants immunity to Conservator's Grip.
    // 62541 is what the spore casts on itself - this is the spell that actually lands on players.
    SPELL_POTENT_PHEROMONES = 64321,
};

// Freya hard mode: bots step this far out of an Unstable Sun Beam before it detonates. Exact beam
// radius is DBC, not in the server script, so this is a conservative default to confirm in-game.
constexpr float ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS = 12.0f;

// Freya trio wave (Snaplasher / Storm Lasher / Ancient Water Spirit). Each member starts its own 11s
// revive timer on death and comes back unless all three are down when it expires, so they have to die
// together. The band only covers the last tenth of the wave - about 6s of raid damage out of the 60s
// the next wave takes to spawn - so it is deliberately wide.
constexpr float ULDUAR_FREYA_TRIO_SYNC_WINDOW_PCT = 30.0f;    // below this the trio outranks other adds
constexpr float ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT = 15.0f;  // all members below: free burn to the finish
constexpr float ULDUAR_FREYA_TRIO_HARD_FLOOR_PCT = 10.0f;     // never cross while a sibling is still high

// Freya: Potent Pheromones (64321) is a 6 yd ally aura on a Healthy Spore. It is the only counter to
// Conservator's Grip, which is a 50000 yd pacify-silence and so cannot be outranged.
constexpr float ULDUAR_FREYA_SPORE_RADIUS = 6.0f;

// Spores are summoned 20 yd out from the Conservator in three directions, so this only has to cover
// that ring with room for the boss having been dragged part of the way to one.
constexpr float ULDUAR_FREYA_SPORE_SEARCH_RADIUS = 40.0f;

// Freya: Detonate radius, and it fires on death, not on a timer. Two difficulty ids - 62598 in 10-man
// (base 4162) and 62937 in 25-man (base 6824) - but both carry EffectRadiusIndex 18, so the radius is
// the same either way. Every 10s a Detonating Lasher wipes its own threat list and charges a random
// player, so no amount of threat holds one, and speed_run 1.14286 puts it at 8.0 yd/s against a
// player's 7.0, so nothing can walk one anywhere either.
constexpr float ULDUAR_FREYA_DETONATE_RADIUS = 15.0f;

// Melee never chase a lasher; past this they stay on whatever they were already hitting. Deliberately
// tight: "nearby" has to mean the lasher came to the melee group, not that the group crosses the room.
constexpr float ULDUAR_FREYA_MELEE_LASHER_RANGE = 12.0f;

// Margin before the add tank moves between two near-equal trio members. Its own damage is what closes
// the gap, so without this it would swap every few ticks and lose swing timers to nothing.
constexpr float ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT = 5.0f;

// Nature Bomb (64587) is 10 yd in both raid sizes and lands at the target's own feet, leaving ~6s to
// clear the full radius from a standing start. The extra yard covers the bot's own reach so it does
// not clip the edge of the blast while holding still.
constexpr float ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS = 11.0f;

// Where the escape aims, deliberately past the trigger radius: landing on the boundary would re-fire
// the node every tick as combat movement pulls the bot back toward its target.
constexpr float ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS = 13.0f;

// How far the escapes look for hazards to route around. Every one of Freya's comes in numbers - a
// bomb per player, ten lashers, overlapping sun beams - so stepping clear of the nearest is not
// enough; the whole cluster has to be visible or the bot walks out of one and into the next.
constexpr float ULDUAR_FREYA_HAZARD_SEARCH_RADIUS = 30.0f;

// Detonating Lasher wave. Nothing is tanked and nothing is ferried: the raid holds one camp, the
// lashers come to it on their own, and the pack is AoE'd down together. Below FINISH_PCT the AoE stops
// and the pack is picked off one at a time, so the 15 yd blasts land one by one instead of at once.
//
// PACK_RADIUS is MediumAoeTrigger's own 8 yd / 3 attackers, which is what actually decides whether
// class AoE fires; PACK_CLEAR is one yard past Detonate, so a bot that steps out is clear of the whole
// pile going off behind it.
constexpr float ULDUAR_FREYA_LASHER_PACK_RADIUS = 8.0f;
constexpr float ULDUAR_FREYA_LASHER_PACK_CLEAR = 16.0f;
constexpr uint32 ULDUAR_FREYA_LASHER_PACK_MIN_COUNT = 3;
constexpr float ULDUAR_FREYA_LASHER_FINISH_PCT = 20.0f;

// Frost Nova is a 10 yd sphere centred on the caster, so this is also how close the mage has to stand
// to the pack - inside Detonate range, which is why the nova node is followed out by the step-out one.
constexpr float ULDUAR_FREYA_FROST_NOVA_RADIUS = 10.0f;

// How tight the camp holds. Ranged are pulled in harder because the ball has to fit inside one AoE;
// healers get the slack, since they also have to stay in range of the melee group and the tanks.
constexpr float ULDUAR_FREYA_RANGED_CAMP_TOLERANCE = 10.0f;
constexpr float ULDUAR_FREYA_HEALER_CAMP_TOLERANCE = 15.0f;

// Freya. Everything the encounter needs from one grid pass, so the priority action, the tank action
// and both multipliers cannot disagree about what is up.
struct FreyaWaveState
{
    Unit* eonarsGift = nullptr;
    Unit* conservator = nullptr;
    Unit* snaplasher = nullptr;
    Unit* stormLasher = nullptr;
    Unit* waterSpirit = nullptr;
    std::vector<Unit*> detonatingLashers;

    std::vector<Unit*> LivingTrio() const;

    // Any living member below the sync window: the raid must finish this trio before it touches
    // anything else, or the members already low revive.
    bool TrioLocked() const;

    // Every living member at or below the release threshold - the last seconds, where nothing may
    // pull a bot away and no member is held back.
    bool TrioReleased() const;
};

void GatherFreyaWaveState(PlayerbotAI* botAI, FreyaWaveState& state);

// Whether damage on this trio member has to stop so the three converge. A backstop for damage the
// targeting cannot steer - a swing mid-animation, a DoT already ticking - since GetFreyaTrioAssignment
// has normally moved bots off a suppressed member already.
bool FreyaTrioSyncSuppress(FreyaWaveState const& state, Unit* target);

// Which trio member this bot should be hitting. Greedy load balance over remaining health, recomputed
// every tick: every bot walks the same group order over the same numbers and reaches the same split,
// so no shared state is needed. Suppressed members drop out of the candidate list, which is what makes
// the floor redistribute bots instead of idling them.
Unit* GetFreyaTrioAssignment(PlayerbotAI* botAI, FreyaWaveState const& state);

// What this tank should be on. The main tank always gets Freya; the add tank gets the Snaplasher first
// (Hardened Bark 62663 stacks +10% damage done per hit taken, so it needs a dedicated sink), then the
// Conservator, then a trio member, then a lasher standing next to it, then Freya.
//
// The trio pick is the highest-health non-suppressed member, and that is deliberate: tank damage is
// invisible to GetFreyaTrioAssignment, which only counts DPS, so aiming it at the member furthest from
// the floor makes the unaccounted damage help convergence instead of skewing it. currentTarget is what
// keeps that pick from flipping as the tank's own damage closes the gap.
//
// Lashers are reachable only through GetFreyaLocalLasherTarget, so a tank can damage one already on top
// of it but can never walk one back into the raid.
Unit* GetFreyaTankTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget);

// The nearest living lasher inside range, sticky on currentTarget with a switch margin. Range doubles as
// the leash - a lasher that runs past it is dropped, which stops a bot being towed across the room every
// time the add retargets. Melee and tanks pass ULDUAR_FREYA_MELEE_LASHER_RANGE; ranged pass their spell
// range, since for them "local" means anything they can shoot without moving.
Unit* GetFreyaLocalLasherTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget, float range);

// The lasher every ranged bot should be on. Lowest health, GUID breaking ties: raid-wide agreement with
// no shared state, and self-stabilising, since the add being focused stays the lowest.
Unit* GetFreyaRangedLasherFocus(FreyaWaveState const& state);

// The Healthy Spore the Conservator is being parked on. Keyed off the Conservator and never off the
// calling bot, so the tank doing the dragging and the melee walking to shelter resolve the same spore
// without communicating.
Unit* GetFreyaConservatorSpore(PlayerbotAI* botAI, Unit* conservator);

// True while any bot in the group that counts as ranged DPS is alive. Eonar's Gift is a ranged job,
// but a melee-only raid still has to kill it or Freya heals 30-60%.
bool FreyaHasLivingRangedDps(PlayerbotAI* botAI);

// Every live Nature Bomb near the bot. GameObjects, not creatures: the bomb NPC is banished and never
// shows up in the npc value lists.
std::vector<Position> GetFreyaNatureBombPositions(Player* bot, float searchRadius);

// The bot the ranged half and the healers gather on: lowest-GUID living ranged DPS in the group on this
// map, the same tie-break IsFreyaLasherTrapHunter uses, so every bot picks the same one with no shared
// state. A live bot rather than a fixed point, so the camp is always on the mesh and always within
// reach of what the raid is already shooting.
Player* GetFreyaRangedCampAnchor(PlayerbotAI* botAI);

// The lasher with the most living lashers around it, lowest GUID breaking ties. The AoE-phase focus,
// and the reason it is a focus at all: AoeTrigger counts attackers within 8 yd of the *current target*,
// not of the bot, so the raid pointing at the middle of the pile is what makes class AoE fire.
Unit* GetFreyaLasherPackFocus(FreyaWaveState const& state);

// Whether the pack around this point is ready for the staggered finish: at least PACK_MIN_COUNT lashers
// inside PACK_RADIUS and every one of them at or below FINISH_PCT.
bool IsFreyaLasherPackFinishing(FreyaWaveState const& state, Position const& centre);

// The finishing pack within radius of this bot, or nullptr. Measured from the pack's own centre rather
// than from the bot, which is what keeps the finish visible to a bot that has already stepped out of
// it - the hunter's trap and the AoE hold both have to survive the step-out that precedes them.
Unit* GetFreyaFinishingPackNear(PlayerbotAI* botAI, FreyaWaveState const& state, float radius);

// Takes a point rather than a bot: the same count is wanted around a bot, around a candidate focus and
// around the camp anchor.
uint32 CountFreyaLashersNear(Position const& centre, FreyaWaveState const& state, float radius);

// The one hunter that lays the Frost Trap. Lowest GUID among living hunter bots in the group, the same
// tie-break GetFreyaRangedLasherFocus uses, so every bot agrees on it without any shared state.
bool IsFreyaLasherTrapHunter(PlayerbotAI* botAI);

// Freya is mid-cast on Ground Tremor. Matches both difficulty ids, since 62437 is the 10-man twin.
bool IsFreyaGroundTremorCasting(Unit* boss);

#endif
