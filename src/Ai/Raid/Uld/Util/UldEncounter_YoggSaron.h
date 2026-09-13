/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERYOGGSARON_H
#define PLAYERBOTS_ULDENCOUNTERYOGGSARON_H

#include "Position.h"
#include "UldData.h"

#include <vector>

class PlayerbotAI;
class Player;
class Unit;

// Yogg-Saron.
//
// Three phases in three different rooms. P1 is tentacles and Ominous Clouds around the central
// platform; P2 opens the brain room below, where the raid splits between killing tentacles above and
// clearing illusions - Stormwind, Icecrown and the Chamber of Aspects - inside the brain; P3 is the
// boss himself on the platform once the Shadow Barrier drops and the last Guardian is down.
//
// Sanity (63050) is the currency underneath all of it: it drains on gaze and illusion exposure, and
// at zero the bot is Insane and lost. That is why the geometry below is per-room rather than one
// arena, and why phase is read off the boss's aura rather than a timer.

enum UlduarYoggSaronIds
{
    ACTION_ILLUSION_DRAGONS = 1,
    ACTION_ILLUSION_ICECROWN = 2,
    ACTION_ILLUSION_STORMWIND = 3,
    NPC_GUARDIAN_OF_YS = 33136,
    NPC_YOGG_SARON = 33288,
    NPC_OMINOUS_CLOUD = 33292,
    NPC_RUBY_CONSORT = 33716,
    NPC_AZURE_CONSORT = 33717,
    NPC_BRONZE_CONSORT = 33718,
    NPC_EMERALD_CONSORT = 33719,
    NPC_OBSIDIAN_CONSORT = 33720,
    NPC_ALEXTRASZA = 33536,
    NPC_MALYGOS_ILLUSION = 33535,
    NPC_NELTHARION = 33523,
    NPC_YSERA = 33495,
    GO_DRAGON_SOUL = 194462,
    NPC_SARA_PHASE_1 = 33134,
    NPC_LICH_KING_ILLUSION = 33441,
    NPC_IMMOLATED_CHAMPION = 33442,
    NPC_SUIT_OF_ARMOR = 33433,
    NPC_GARONA = 33436,
    NPC_KING_LLANE = 33437,
    NPC_DEATHSWORN_ZEALOT = 33567,
    NPC_INFLUENCE_TENTACLE = 33943,
    NPC_DEATH_ORB = 33882,
    NPC_BRAIN = 33890,
    NPC_CRUSHER_TENTACLE = 33966,
    NPC_CONSTRICTOR_TENTACLE = 33983,
    NPC_CORRUPTOR_TENTACLE = 33985,
    NPC_IMMORTAL_GUARDIAN = 33988,
    NPC_LAUGHING_SKULL = 33990,
    NPC_SANITY_WELL = 33991,
    NPC_DESCEND_INTO_MADNESS = 34072,
    NPC_MARKED_IMMORTAL_GUARDIAN = 36064,
    SPELL_SANITY = 63050,
    SPELL_BRAIN_LINK = 63802,
    SPELL_MALADY_OF_THE_MIND = 63830,
    SPELL_SHADOW_BARRIER = 63894,
    SPELL_TELEPORT_TO_CHAMBER = 63997,
    SPELL_TELEPORT_TO_ICECROWN = 63998,
    SPELL_TELEPORT_TO_STORMWIND = 63989,
    SPELL_TELEPORT_BACK = 63992,
    SPELL_CANCEL_ILLUSION_AURA = 63993,
    SPELL_INDUCE_MADNESS = 64059,
    SPELL_LUNATIC_GAZE_YS = 64163,
    SPELL_DARK_VOLLEY = 63038,  // Guardian's 1.5s cast, 35 yd - distance is no answer, only a kick
    SPELL_DARK_VOLLEY_H = 65330,  // 25 normal casts 63038, so the split is not 10/25: test both
    SPELL_SQUEEZE = 64125,  // Constrictor Tentacle's grip; base id, difficulty-mapped at runtime
    SPELL_WEAKENED = 64162,  // Immortal Guardian's killable window; Thorim's Titanic Storm executes it
    GO_FLEE_TO_THE_SURFACE_PORTAL = 194625,
};

constexpr float ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 300.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 200.0f;
constexpr float ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS = 50.0f;

// Yogg-Saron reduced-Keeper hard mode: a bot whose Sanity (63050, 100 stacks) is at or below this
// pulls behind Yogg and faces away to conserve it. With Freya absent there are no Sanity Wells, so the
// drain is one-way - kept low so only near-Insane bots pull out. Confirm in-game.
constexpr uint32 ULDUAR_YOGG_SARON_SANITY_CONSERVE_THRESHOLD = 15;

// Ominous Clouds orbit Sara at 11/21/31/41/51/61 yd, constant 3 yd/s, and summon a Guardian on any
// player within 6 yd. Trigger and clear are kept apart on purpose: the sweep parks the bot on
// whatever boundary it gets, so one radius re-fires every time a cloud drifts a yard in.
constexpr float ULDUAR_YOGG_SARON_CLOUD_TRIGGER_RADIUS = 10.0f;
constexpr float ULDUAR_YOGG_SARON_CLOUD_CLEAR_RADIUS = 14.0f;

// Shadow Nova, the Guardian's death explosion: DBC radius 15, plus both object sizes at apply time.
// Ranged and healers stay out of it; melee and tanks have to eat it to kill the thing at all.
constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS = 17.0f;
constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS = 20.0f;

// The second cap is the load-bearing one: a bot dodging outward otherwise walks out of spell range
// and stops contributing for the rest of the phase.
constexpr float ULDUAR_YOGG_SARON_SPACING_SEARCH_RADIUS = 25.0f;
constexpr float ULDUAR_YOGG_SARON_SPACING_MAX_FROM_MIDDLE = 35.0f;

// Backstop only - "is the held spot still clear" normally invalidates first.
constexpr uint32 ULDUAR_YOGG_SARON_SPACING_HOLD_MS = 3000;

// How far out to look for a Guardian worth kicking. Wider than any interrupt's range on purpose - the
// action drops the ones it cannot reach, and a short list here would hide a cast from a bot who could.
constexpr float ULDUAR_YOGG_SARON_INTERRUPT_SEARCH_RADIUS = 40.0f;

extern const Position ULDUAR_YOGG_SARON_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT;

// Yogg-Saron phase reads. Yogg is not reliably on a bot's threat list, so both scan for the creature
// instead of going through "find target".
// Phase 1. Sara lives on into P2/P3 at 1 health, so "Sara is alive" is not a phase test by itself.
bool YoggSaronInPhase1(PlayerbotAI* botAI);
bool YoggSaronInPhase2(PlayerbotAI* botAI);
bool YoggSaronInPhase3(PlayerbotAI* botAI);

// Guardians casting Dark Volley right now, for the interrupt node. Shared between trigger and action
// so the two cannot disagree about what is being kicked.
std::vector<Unit*> GetYoggSaronDarkVolleyCasters(PlayerbotAI* botAI);

// Classes carrying an interrupt the action can aim. Avenger's Shield is left out - it picks its own
// target and cannot be pointed at a named Guardian.
bool YoggSaronCanInterrupt(Player* bot);

// Window in which a counterable fear can land, for the shared anti-fear component. Yogg-Saron fears
// in P2 (Malady of the Mind, which re-casts on removal) and again in P3 (Deafening Roar).
bool YoggSaronFearWindowActive(PlayerbotAI* botAI);

#endif
