/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERFREYA_H
#define PLAYERBOTS_ULDENCOUNTERFREYA_H

#include "EncounterHelpers.h"
#include "ObjectGuid.h"
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

    // Freya's other big hit, and unlike Ground Tremor this one can be left: a 1.5s cast on a random
    // threat-list target, 8 yd at that target's feet. 62623 is 10-man, 62872 is 25-man.
    SPELL_FREYA_SUNBEAM_10 = 62623,
    SPELL_FREYA_SUNBEAM_25 = 62872,

    // The Ancient Conservator's mark, thrown every 14s at a random player within 100 yd. A 10s aura
    // whose only effect is a 2s periodic trigger, so it fires five times, each one 8 yd around whoever
    // is wearing it. 62589 is 10-man, 63571 is 25-man.
    SPELL_NATURES_FURY_10 = 62589,
    SPELL_NATURES_FURY_25 = 63571,

    // Freya hard mode: Elders left alive permanently empower Freya with an extra ability each.
    // NPC_FREYA comes from core ulduar.h via UldScripts.h.
    NPC_FREYA_IRON_ROOTS = 33088,               // Ironbranch's Iron Roots trap (selectable)
    NPC_FREYA_STRENGTHENED_IRON_ROOTS = 33168,  // Freya's empowered Iron Roots trap (selectable)
    NPC_FREYA_SUN_BEAM = 33170,                 // Freya's Unstable Sun Beam stalker (non-selectable)
    NPC_FREYA_UNSTABLE_SUN_BEAM = 33050,        // Brightleaf's Unstable Sun Beam stalker (non-selectable)
    // The root DoT, and the one place a difficulty pair is easy to miss: acore_world.spelldifficulty_dbc
    // maps 62283 -> 62930 and 62861 -> 62438, HasAura takes an exact id, and only the 25-man half ever
    // lands in a 25-man raid. Listing the 10-man ids alone is a check that can never be true.
    SPELL_IRON_ROOTS_DAMAGE_10 = 62283,        // DoT on a player trapped by Ironbranch's roots
    SPELL_IRON_ROOTS_DAMAGE_25 = 62930,
    SPELL_IRON_ROOTS_FREYA_DAMAGE_10 = 62861,  // DoT on a player trapped by Freya's roots
    SPELL_IRON_ROOTS_FREYA_DAMAGE_25 = 62438,

    // Applied to allies within 6 yd of a Healthy Spore; grants immunity to Conservator's Grip.
    // 62541 is what the spore casts on itself - this is the spell that actually lands on players.
    SPELL_POTENT_PHEROMONES = 64321,
};

// Freya hard mode: bots step this far out of an Unstable Sun Beam before it detonates. Exact beam
// radius is DBC, not in the server script, so this is a conservative default to confirm in-game.
constexpr float ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS = 12.0f;

// The escape aims past the radius, not at it. FindNearestPositionClearOfHazards rings outward and takes
// the first clear spot, so clearing by a single yard answers a bot on the rim with a ~2 yd step, and the
// next beam spawn puts it back inside the trigger radius immediately. Three yards of hysteresis is what
// lets the trigger stand down after one move instead of re-firing for the rest of the wave.
constexpr float ULDUAR_FREYA_SUN_BEAM_CLEARANCE = 15.0f;

// Ceiling on the escape latch. Long enough to walk the clearance at 7 yd/s, short enough that a bot
// rooted mid-dodge hands the tick back instead of holding it for the rest of the wave.
constexpr uint32 ULDUAR_FREYA_SUN_BEAM_LATCH_MS = 3000;

// Sunbeam's blast, and how far its neighbours aim. The destination is picked when the cast ends rather
// than when it starts - Spell::SelectSpellTargets runs from Spell::cast - so it follows whoever it is
// aimed at and only the bots around that target can leave. Trigger at AVOID, aim at CLEAR, the same
// split the Nature Bomb uses and for the same reason.
constexpr float ULDUAR_FREYA_SUNBEAM_AVOID_RADIUS = 11.0f;
constexpr float ULDUAR_FREYA_SUNBEAM_CLEAR_RADIUS = 13.0f;

// The cast itself. Anything longer holds the bot still after the beam has already landed.
constexpr uint32 ULDUAR_FREYA_SUNBEAM_LATCH_MS = 1500;

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

// How many raid members make a spore full, so the back line spreads over the spores instead of piling
// onto one. Nearest-spore is what piles them up: the parked spore already holds the whole melee group,
// the back line starts the wave in one ball, and 18 of 25 bots ended up sheltering on a single spore
// with four others holding one bot each. Six leaves the melee spore over the line from the start, which
// is the point - both of this wave's big hits are 8 yd wide, and a spore is 6.
constexpr uint32 ULDUAR_FREYA_SPORE_CROWD = 6;

// What one Nature's Fury tick covers, centred on the bot wearing the mark: EffectRadiusIndex 14 on the
// triggered 63570, and the trace agrees - victims sat a median 2.3 yd out, none past 9.1.
constexpr float ULDUAR_FREYA_NATURES_FURY_RADIUS = 8.0f;

// Where the carrier aims, past the radius for the usual reason: landing on the boundary is answered
// with a two yard step that combat movement undoes before the next tick.
constexpr float ULDUAR_FREYA_NATURES_FURY_CLEAR = 11.0f;

// Ceiling on the bail latch, about the walk to the next spore at 7 yd/s. The mark itself runs 10s.
constexpr uint32 ULDUAR_FREYA_NATURES_FURY_LATCH_MS = 3000;

// Where a bot actually stops. Inside the aura with room to spare, and clear of the spore's own
// collision - aiming at the centre gives MoveTo a point the bot can never occupy, so it re-issues the
// same rejected move forever. The trigger stands down a yard further out, which is the hysteresis.
constexpr float ULDUAR_FREYA_SPORE_STAND_RANGE = 4.0f;

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

// Nature Bomb (64587) is 10 yd in both raid sizes and lands at the target's own feet. The blast itself,
// with no margin: the last-resort escape only has to leave the circles the bot is standing in, because
// a volley drops one on seven to ten players at once and there is often nowhere that clears them all.
constexpr float ULDUAR_FREYA_NATURE_BOMB_BLAST_RADIUS = 10.0f;

// The extra yard covers the bot's own reach so it does not clip the edge of the blast while holding
// still.
constexpr float ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS = 11.0f;

// Where the escape aims, deliberately past the trigger radius: landing on the boundary would re-fire
// the node every tick as combat movement pulls the bot back toward its target.
constexpr float ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS = 13.0f;

// Ceiling on the bomb escape latch, and it has to outlast the fuse. A bomb goes off 6s after it lands,
// not the 11s its explode timer reads: boss_freya_nature_bomb::UpdateAI fires at _explodeTimer >= 11000
// but the branch under it snaps the timer from 5000 straight to 10000. At 3000 the latch expired with
// half the fuse left, the bot walked back on its own DPS node, and it was inside the blast again a
// median 0.7s before it fired.
constexpr uint32 ULDUAR_FREYA_NATURE_BOMB_LATCH_MS = 7000;

// Ceiling on the main tank's reposition. Long enough to walk the clearance at 7 yd/s, short enough
// that a tank stopped on the way hands the tick back instead of holding Freya still for a whole volley.
constexpr uint32 ULDUAR_FREYA_TANK_BOMB_LATCH_MS = 3000;

// Where Freya is held. navprobe on the bot filter (--nav 0x09) puts it 0.52 yd from the nearest poly
// with UpdateAllowedPositionZ 425.333, and a 12 yd ring around it is 8/8 on mesh at Z 423.8-425.8.
extern const Position ULDUAR_FREYA_TANK_ANCHOR;

// How far the tank may take her from it. Freya follows whoever holds her, the bomb escape aims away
// from the back line, a volley lands every 18s and nothing used to walk her back - so the bias
// compounded: one pull ended with her 100 yd east of the anchor, six yards down the slope toward the
// water, with the melee ring and half the raid strung out behind her.
constexpr float ULDUAR_FREYA_TANK_LEASH = 20.0f;

// Ceiling on the walk home. Same reason as every other latch here: a re-issued MoveTo clears the motion
// master out from under the walk it is repeating.
constexpr uint32 ULDUAR_FREYA_TANK_HOLD_LATCH_MS = 3000;

// How far the escapes look for hazards to route around. Every one of Freya's comes in numbers - a
// bomb per player, ten lashers, overlapping sun beams - so stepping clear of the nearest is not
// enough; the whole cluster has to be visible or the bot walks out of one and into the next.
constexpr float ULDUAR_FREYA_HAZARD_SEARCH_RADIUS = 30.0f;

// Detonating Lasher wave. Nothing is tanked and nothing is ferried: the raid holds one camp, the
// lashers come to it on their own, and the pack is AoE'd down together. Below FINISH_PCT the AoE stops
// and the pack is picked off one at a time, so the 15 yd blasts land one by one instead of at once.
//
// The camp is 10 yd inside a 15 yd blast, so the raid does eat each detonation whole. That is the
// trade, and it is the cheaper half: standing further apart than a bot's own reach is worse, because
// ReachTargetAction is the only generic way any bot closes on a target, so bots that cannot reach
// what charged them stop killing the wave - and a wave that does not die outlasts every cooldown the
// raid has. See docs/raids/ulduar/freya.md for the measurements.
//
// PACK_RADIUS is MediumAoeTrigger's own 8 yd / 3 attackers, which is what actually decides whether
// class AoE fires; PACK_CLEAR is one yard past Detonate, so the AoE hold covers a bot standing at the
// edge of a pile rather than only one inside it.
constexpr float ULDUAR_FREYA_LASHER_PACK_RADIUS = 8.0f;
constexpr float ULDUAR_FREYA_LASHER_PACK_CLEAR = 16.0f;
constexpr uint32 ULDUAR_FREYA_LASHER_PACK_MIN_COUNT = 3;
constexpr float ULDUAR_FREYA_LASHER_FINISH_PCT = 20.0f;

// Frost Nova is a 10 yd sphere centred on the caster, so this is also how close the lashers have to
// have closed on the mage for it to reach anything.
constexpr float ULDUAR_FREYA_FROST_NOVA_RADIUS = 10.0f;
constexpr uint32 ULDUAR_FREYA_FROST_NOVA_MIN_LASHERS = 2;

// Frost Trap lays a 10 yd, 30s, -50% movement field (13809 -> 13810), and a Detonating Lasher has no
// creature_immunities row, so it lands: 8.0 yd/s becomes 4.0, under a player's 7.0. The trap is
// dropped at the hunter's feet while a lasher is still closing, so it is armed by the time the lasher
// walks onto it - that is the whole trigger condition, since one that charged this bot arrives here.
constexpr float ULDUAR_FREYA_FROST_TRAP_ARM_RANGE = 20.0f;

// A lasher this low is about to detonate: measured across twelve pulls, one below this sits there a
// median 2.6s before it blows, which at 7.0 yd/s is the walk out of a 15 yd blast. Higher costs more
// damage than it saves - melee already spend a quarter of the wave stepping out at this number.
constexpr float ULDUAR_FREYA_LASHER_BAIL_PCT = 15.0f;

// How long the step-out holds its answer. Lashers die one at a time, so without this the exit is
// re-derived every tick a yard along and every MoveTo clears the motion master, which resets the walk
// instead of finishing it. Roughly the time the walk itself takes at 7.0 yd/s.
constexpr uint32 ULDUAR_FREYA_LASHER_BAIL_LATCH_MS = 3000;

// Clearance from the nearest lasher that is about to blow, not from the pack and not from its middle.
// They spread over a 17 yd radius, so a standoff measured off the centroid parks the camp on whatever
// walked out in front of it; and one owed to every living lasher is a retreat from something faster
// than the bot, which is why only the ones under _LASHER_BAIL_PCT count. 18 clears the 15 yd blast
// with a yard to spare either side of the walk.
constexpr float ULDUAR_FREYA_LASHER_CAMP_STANDOFF = 18.0f;

// How far out the search may push a camp before giving up on a bearing. Past this the far side of the
// pile is outside AiPlayerbot.SpellDistance (28.5) and the camp stops being a firing position.
constexpr float ULDUAR_FREYA_LASHER_CAMP_MAX_STANDOFF = 28.0f;
constexpr float ULDUAR_FREYA_LASHER_CAMP_STEP = 2.0f;

// Eonar's Gift heals Freya 30-60% if it lives 12s, and ten bots take one from full to dead in about 5s.
// Five clear it well inside that, which leaves the rest of the ranged on the lasher pack instead of
// emptying it for six seconds in the middle of a wave.
constexpr uint32 ULDUAR_FREYA_GIFT_SHARE = 5;

// If the share has not finished the Gift by now, every ranged bot joins it. Leaves about 3s of margin
// on the 12s.
constexpr uint32 ULDUAR_FREYA_GIFT_SHARE_MS = 5000;

// How tight the camp holds. Ranged are pulled in harder because the ball has to fit inside one AoE;
// healers get the slack, since they also have to stay in range of the melee group and the tanks.
constexpr float ULDUAR_FREYA_RANGED_CAMP_TOLERANCE = 10.0f;
constexpr float ULDUAR_FREYA_HEALER_CAMP_TOLERANCE = 15.0f;

// How far short of the camp spot a bot stops, so the back line is a ring instead of a pile: each one
// keeps the bearing it arrived on, which spreads them without shared state and without moving anybody
// who was already inside the tolerance. It cannot beat an 8 yd splash on its own - twelve bots would
// need a 15.5 yd ring for that, a 31 yd ball, past AiPlayerbot.SpellDistance and wide enough to spread
// the pack the camp exists to gather. Standing on one square is what it fixes.
constexpr float ULDUAR_FREYA_RANGED_CAMP_SPACING = 6.0f;

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

// Per-bot cache for the lookups every Freya trigger, action and multiplier repeats each tick. The
// values behind them recompute on every read: a 100 yd grid sweep for the target list, that sweep
// plus a line-of-sight ray per npc for the stalkers, a threat-list walk with a name conversion per
// unit for the boss. Keyed on getMSTime(), since a bot never ticks twice in one ms, and holding guids
// so a despawn between two reads drops out instead of dangling.
//
// Tick-scoped on purpose. A queued action can be up to AiPlayerbot.ExpireActionTime old when it is
// finally popped, so an answer held across ticks would decide on a world that has moved.
class FreyaScan
{
public:
    explicit FreyaScan(PlayerbotAI* botAI) : botAI(botAI) {}

    // "find target" freya. Null until she has this bot on her threat list.
    Unit* Boss();

    // "possible targets no los", exactly as the value returns it.
    GuidVector const& PossibleTargets();

    // Healthy Spores and both Sun Beam stalkers, in the order "nearest npcs" walks them and through
    // its own alive, non-player and line-of-sight filters. One visit for all three entries, because
    // the spore pick wants two of them in the same tick the hazard sweep wants the third.
    GuidVector const& Stalkers();

    // Where the back line gathers. The trigger, isUseful and Execute all ask for it in one tick, so
    // it gathers the wave itself rather than trusting every caller to hand in the same one.
    Position const& LasherCamp();

private:
    PlayerbotAI* botAI;

    uint32 bossAtMs = 0;
    ObjectGuid boss;
    uint32 targetsAtMs = 0;
    GuidVector targets;
    uint32 stalkersAtMs = 0;
    GuidVector stalkers;
    uint32 campAtMs = 0;
    Position camp;
};

// This bot's FreyaScan, held by the "freya scan" value.
FreyaScan& GetFreyaScan(PlayerbotAI* botAI);

// First living unit with this entry in the scan's target list: the same list and the same test as
// EncounterHelpers::GetFirstAliveUnitByEntry, off one sweep a tick instead of one per call.
Unit* GetFreyaScanUnitByEntry(PlayerbotAI* botAI, uint32 entry);

// Freya, found by a visit that tests her entry first rather than through the whole target list. Both
// Ground Tremor nodes run wherever the encounter gate is open, so on trash and in every other fight
// nothing else wants that list and building it costs more than this does.
Unit* GetFreyaBossByEntry(PlayerbotAI* botAI);

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

// The spore this bot should be sheltering on. Melee get the parked one; ranged and healers get the
// nearest one that is not already full, so the back line spreads across the spores instead of piling
// onto whichever is nearest - which, starting from one ball, is the same spore for all of them, and is
// normally the one the melee group is standing on. One derivation for both the trigger and the action:
// resolving it twice let a bot be woken by a spore in line of sight and then walked at a different one
// behind the Conservator.
Unit* GetFreyaTargetSpore(PlayerbotAI* botAI);

// Every living Healthy Spore the bot can see.
std::vector<Unit*> GetFreyaSpores(PlayerbotAI* botAI);

// Living raid members within radius of a point, ignoring one of them - normally the bot asking.
uint32 CountFreyaRaidNear(PlayerbotAI* botAI, Position const& centre, float radius, Player* except);

// Where a bot wearing Nature's Fury should run: the nearest spore with nobody else inside the splash,
// so it keeps Potent Pheromones and stays out of Conservator's Grip. Null when no spore is free, which
// is when the bail settles for open floor and eats the pacify for the rest of the mark.
Unit* GetFreyaNaturesFuryShelter(PlayerbotAI* botAI);

// True while any bot in the group that counts as ranged DPS is alive. Eonar's Gift is a ranged job,
// but a melee-only raid still has to kill it or Freya heals 30-60%.
bool FreyaHasLivingRangedDps(PlayerbotAI* botAI);

// Every live Nature Bomb near the bot. GameObjects, not creatures: the bomb NPC is banished and never
// shows up in the npc value lists.
std::vector<Position> GetFreyaNatureBombPositions(Player* bot, float searchRadius);

// Every live Sun Beam near the bot. Takes the AI rather than the Player because the beam stalkers are
// non-selectable and only ever appear in the raw nearby-npc list.
std::vector<Position> GetFreyaSunBeamPositions(PlayerbotAI* botAI, float searchRadius);

// Every hazard both Freya escapes have to route around, each paired with the clearance it needs. A bot
// that only reads its own kind steps out of a bomb into a beam and back again; both nodes sit at the
// same relevance, so that alternation never resolves on its own.
std::vector<EncounterHelpers::HazardCircle> GetFreyaEscapeHazards(PlayerbotAI* botAI, float searchRadius);

// The same list from hazards the caller already holds, so an escape that collected the bombs to test
// its own spot does not collect them again to route around them.
std::vector<EncounterHelpers::HazardCircle> BuildFreyaEscapeHazards(std::vector<Position> const& bombs,
                                                                    std::vector<Position> const& beams);

// The bot the ranged half and the healers gather on: lowest-GUID living ranged DPS in the group on
// this map, so every bot picks the same one with no shared state. A live bot rather than a fixed
// point, so the camp is always on the mesh and always within reach of what the raid is shooting.
Player* GetFreyaRangedCampAnchor(PlayerbotAI* botAI);

// This bot's place in the GUID order of the living ranged DPS, so a job can be handed to the first few
// of them without shared state. UINT32_MAX for anyone who is not ranged DPS.
uint32 GetFreyaRangedDpsRank(PlayerbotAI* botAI);

// Where the ranged half stands during the wave: the nearest point on the bearing the raid is already
// on that keeps every lasher about to detonate STANDOFF away, so nobody crosses the pile to reach it.
// With none of them low it is the anchor itself, which still gathers the back line into one ball for
// the AoE without asking it to outrun a healthy pack. GetFreyaRangedCampAnchor is also the fallback
// when no bearing clears collision - a live bot is always on the mesh, which a computed point is not.
//
// Takes no wave state on purpose: the answer is cached for the tick, so a state handed in by the
// first caller would silently become the camp every later caller gets.
Position GetFreyaLasherCampSpot(PlayerbotAI* botAI);

// Living detonating lashers under maxPct within radius of the bot. The health filter is the point:
// clearing every lasher would push melee out of the fight, clearing only the ones about to blow costs
// a quarter of the wave.
std::vector<Position> GetFreyaLowLasherPositions(PlayerbotAI* botAI, FreyaWaveState const& state, float maxPct,
                                                 float radius);

// The lasher with the most living lashers around it, lowest GUID breaking ties. The AoE-phase focus,
// and the reason it is a focus at all: AoeTrigger counts attackers within 8 yd of the *current target*,
// not of the bot, so the raid pointing at the middle of the pile is what makes class AoE fire.
Unit* GetFreyaLasherPackFocus(FreyaWaveState const& state);

// Whether the pack around this point is ready for the staggered finish: at least PACK_MIN_COUNT lashers
// inside PACK_RADIUS and every one of them at or below FINISH_PCT.
bool IsFreyaLasherPackFinishing(FreyaWaveState const& state, Position const& centre);

// The finishing pack within radius of this bot, or nullptr. Measured from the pack's own centre rather
// than from the bot, so a bot standing at the edge of the pile still sees the finish it is feeding.
Unit* GetFreyaFinishingPackNear(PlayerbotAI* botAI, FreyaWaveState const& state, float radius);

// Takes a point rather than a bot: the same count is wanted around a candidate pack focus, and around
// the bot itself for the nova and the trap.
uint32 CountFreyaLashersNear(Position const& centre, FreyaWaveState const& state, float radius);

// Freya is mid-cast on Ground Tremor. Matches both difficulty ids, since 62437 is the 10-man twin.
bool IsFreyaGroundTremorCasting(Unit* boss);

// Whoever Freya's Sunbeam is currently aimed at, or null when she is not casting it. Both difficulty
// ids, as above. Pets count: three of one pull's sixteen beams were aimed at one, and the worst of them
// landed because the pet ran 15 yd into the raid while the cast was still going.
Unit* GetFreyaSunbeamTarget(Unit* boss);

#endif
