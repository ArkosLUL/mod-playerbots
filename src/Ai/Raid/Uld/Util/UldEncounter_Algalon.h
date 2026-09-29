/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERALGALON_H
#define PLAYERBOTS_ULDENCOUNTERALGALON_H

#include "EncounterHelpers.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "RaidObs.h"
#include "UldData.h"

#include <unordered_map>
#include <vector>

class Player;
class PlayerbotAI;
class Unit;

// Algalon the Observer.

enum UlduarAlgalonIds
{
    PB_NPC_ALGALON = 32871,
    PB_NPC_LIVING_CONSTELLATION = 33052,
    PB_NPC_COLLAPSING_STAR = 32955,
    PB_NPC_BLACK_HOLE = 32953,
    PB_NPC_WORM_HOLE = 34099,
    PB_NPC_UNLEASHED_DARK_MATTER = 34097,
    NPC_ALGALON_ASTEROID_TARGET_1 = 33104,
    NPC_ALGALON_ASTEROID_TARGET_2 = 33105,
    SPELL_ALGALON_BIG_BANG = 64443,
    SPELL_ALGALON_BIG_BANG_25 = 64584,
    SPELL_ALGALON_PHASE_PUNCH = 64412,
    // Priest externals put on the Big Bang soaker, matched by aura so two priests don't double up.
    SPELL_ALGALON_PAIN_SUPPRESSION = 33206,
    SPELL_ALGALON_GUARDIAN_SPIRIT = 47788,
    SPELL_ALGALON_DISPERSION = 47585,
};

// He casts Ascend the moment he leaves a 47 yd disc around home or drops under Z 410.
constexpr float ULDUAR_ALGALON_ROOM_RADIUS = 47.0f;
constexpr float ULDUAR_ALGALON_ROOM_Z_MIN = 410.0f;
constexpr float ULDUAR_ALGALON_ROOM_Z_MAX = 425.0f;
// Constellations spawn up to 67 yd out from home, well past the room edge.
constexpr float ULDUAR_ALGALON_SCAN_RADIUS = 70.0f;

// Black Hole (62168) and Worm Hole (65250) phase anyone inside 6 yd for 10s, no target cap.
constexpr float ULDUAR_ALGALON_SHELTER_RADIUS = 6.0f;
// Big Bang's 64445 strips every phase a second after the hit and the field pulses every second, so a
// bot still inside it then is phased again for 10s. Exits clear it with room to spare.
constexpr float ULDUAR_ALGALON_HOLE_EXIT_RADIUS = 8.0f;

// The handler parks this far past the hole on the far side, so the chase drags the constellation
// through the field while the handler stays outside it.
constexpr float ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 9.0f;
// Park points and dodges stay this far inside the room edge.
constexpr float ULDUAR_ALGALON_ROOM_CLAMP = 44.0f;

// Phase Punch is a 45s aura refreshed every 15.5s and the 5th stack phases the tank out. Swapping at
// 4 leaves ~17s of margin before either tank is stuck unable to take him back.
constexpr uint8 ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS = 4;
// The Phase Punch queued behind the cast lands on the soaker while everyone else is still phased, and a
// 5th stack then leaves nobody unphased, which resets him. So a holder deep in stacks hands him over first.
constexpr uint8 ULDUAR_ALGALON_EARLY_SWAP_STACKS = 3;
constexpr uint8 ULDUAR_ALGALON_EARLY_SWAP_PARTNER_STACKS = 1;
constexpr uint32 ULDUAR_ALGALON_EARLY_SWAP_SECONDS = 15;

// First cast 90s after the intro ends, then every 90.5s. The prediction is re-latched on every cast.
constexpr uint32 ULDUAR_ALGALON_FIRST_BIG_BANG_MS = 90000;
constexpr uint32 ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS = 90500;
// How early the raid needs a hole standing, and keeps the last one out of the kite.
constexpr uint32 ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS = 30;
// Tank defensives, Dispersion, Pain Suppression and Guardian Spirit stay unspent this long before a cast.
constexpr uint32 ULDUAR_ALGALON_COOLDOWN_HOLD_SECONDS = 20;
// Hiders keep fighting until their walk plus this margin is all that's left of the cast. The phase only
// has to cover the hit, and 64445 brings everyone back a second after it, so hiding early buys nothing.
constexpr int32 ULDUAR_ALGALON_HIDE_MARGIN_MS = 2500;
// Cast remaining when the soaker's defensive goes up, so it also covers his swings until the raid is back.
constexpr int32 ULDUAR_ALGALON_SOAK_DEFENSIVE_MS = 3000;
// Dispersion only lasts 6s.
constexpr int32 ULDUAR_ALGALON_SOAK_DISPERSION_MS = 5000;
// An external needs the priest to still reach a hole after the GCD.
constexpr int32 ULDUAR_ALGALON_EXTERNAL_MIN_REMAINING_MS = 4000;

// Collapsing Star pacing. Each death is 16-21k to the whole raid, and Collapse drains 1% max health a
// second, so four ignored stars explode within seconds of each other.
constexpr float ULDUAR_ALGALON_STAR_PACING_RAID_HP_PCT = 80.0f;
constexpr uint32 ULDUAR_ALGALON_STAR_PACING_GAP_MS = 8000;
// Below this Collapse finishes the star anyway, so the raid picks the moment instead.
constexpr float ULDUAR_ALGALON_STAR_FINISH_HP_PCT = 15.0f;
constexpr uint8 ULDUAR_ALGALON_STAR_TEAM_10 = 2;
constexpr uint8 ULDUAR_ALGALON_STAR_TEAM_25 = 3;

// Cosmic Smash: full damage inside 6 yd, doubled dmg/dist to 10, dmg/dist past that. Impact is ~4.8s
// after the marker lands, one marker on 10-man, three on 25-man.
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_TRIGGER_RADIUS = 12.0f;
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE = 15.0f;
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_MIN_CLEARANCE = 10.0f;
constexpr float ULDUAR_ALGALON_DODGE_SEARCH_RADIUS = 24.0f;
// The marker creatures live 10s but the meteor lands ~4.8s in; past this they mark nothing.
constexpr uint32 ULDUAR_ALGALON_MARKER_LIFETIME_MS = 5500;

// Algalon formation. The raid arrives from +Y, so the tank slot sits on the -Y edge of the worm hole
// square. Rings hang off that slot because the tank is what healers have to stay in range of.
//
// Every radius and arc below is navprobe-verified on map 603: flat WMO floor at Z 417.321, 19/19
// points on mesh. The arcs are trimmed because the two -Y worm hole spots sit level with the tank
// slot; the trims keep every slot at least 7.7 yd from all four.
constexpr float ULDUAR_ALGALON_HEALER_RADIUS = 14.0f;
constexpr float ULDUAR_ALGALON_RANGED_INNER_RADIUS = 20.5f;
constexpr float ULDUAR_ALGALON_RANGED_OUTER_RADIUS = 27.0f;
constexpr uint8 ULDUAR_ALGALON_HEALER_SLOTS = 4;
constexpr uint8 ULDUAR_ALGALON_RANGED_INNER_SLOTS = 6;
constexpr uint8 ULDUAR_ALGALON_RANGED_OUTER_SLOTS = 8;
constexpr uint8 ULDUAR_ALGALON_TOTAL_SLOTS =
    ULDUAR_ALGALON_HEALER_SLOTS + ULDUAR_ALGALON_RANGED_INNER_SLOTS + ULDUAR_ALGALON_RANGED_OUTER_SLOTS;
constexpr float ULDUAR_ALGALON_HEALER_ARC_CENTER = 1.5708f;        // +Y, 35..145 degrees
constexpr float ULDUAR_ALGALON_HEALER_ARC_WIDTH = 1.9199f;
constexpr float ULDUAR_ALGALON_RANGED_INNER_ARC_CENTER = 1.7977f;  // 28..178 degrees
constexpr float ULDUAR_ALGALON_RANGED_INNER_ARC_WIDTH = 2.6180f;
constexpr float ULDUAR_ALGALON_RANGED_OUTER_ARC_CENTER = 1.5708f;  // 0..180 degrees
constexpr float ULDUAR_ALGALON_RANGED_OUTER_ARC_WIDTH = 3.1416f;
constexpr float ULDUAR_ALGALON_SLOT_TOLERANCE = 2.0f;
// How far a slot buried under a hole may be stepped aside before the formation gives up on it.
constexpr float ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS = 12.0f;

// Per instance, not per bot: one room scan serves the whole raid.
constexpr uint32 ULDUAR_ALGALON_STATE_TICK_MS = 250;
// A walk standing still this long was stopped short and has to be issued again.
constexpr uint32 ULDUAR_ALGALON_STALL_MS = 500;

// Algalon's home, and the tank slot 18.7 yd out on the -Y edge, 10.2 yd clear of the nearest worm hole.
extern const Position ULDUAR_ALGALON_ROOM_CENTER;
extern const Position ULDUAR_ALGALON_TANK_SLOT;

// Written as its number into algalon.phase.
enum class AlgalonPhase : uint32
{
    Idle = 0,
    Intro = 1,
    One = 2,
    Two = 3,
    Won = 4,
};

// One creature from the room scan. Positions stay valid for phased bots, which can't see the room.
struct AlgalonScanUnit
{
    ObjectGuid guid;
    Position position;
    ObjectGuid victim;
    float healthPct = 0.0f;
    bool active = false;
};

struct AlgalonEncounterState
{
    // Slots are held, not re-derived, so one death doesn't renumber everyone behind the corpse.
    RaidObs::ObsGuidMap<uint8> slotAssignments{"algalon.slot"};
    // Latched per cast so a hole appearing closer can't turn a bot around mid run.
    RaidObs::ObsGuidMap<ObjectGuid> shelterAssignments{"algalon.shelter"};
    RaidObs::ObsGuidMap<ObjectGuid> kiteHoleAssignments{"algalon.kitehole"};

    RaidObs::ObsValue<AlgalonPhase> phase{"algalon.phase"};
    RaidObs::ObsValue<uint32> bigBang{"algalon.bigbang"};
    RaidObs::ObsValue<ObjectGuid> bigBangSoaker{"algalon.soaker"};
    RaidObs::ObsValue<ObjectGuid> bigBangBackup{"algalon.backup"};
    RaidObs::ObsValue<bool> urgent{"algalon.urgent"};
    RaidObs::ObsValue<uint32> holeCount{"algalon.holes"};
    RaidObs::ObsValue<ObjectGuid> focusStar{"algalon.focusstar"};
    RaidObs::ObsGuidSet starTeam{"algalon.starteam"};
    RaidObs::ObsValue<ObjectGuid> handler{"algalon.handler"};

    ObjectGuid boss;
    bool engaged = false;
    bool engagedSeen = false;
    uint32 introEndMs = 0;
    uint32 nextBigBangMs = 0;
    uint32 bigBangCount = 0;
    bool bigBangCasting = false;

    std::vector<AlgalonScanUnit> stars;
    std::vector<AlgalonScanUnit> holes;
    std::vector<AlgalonScanUnit> constellations;
    std::vector<AlgalonScanUnit> markers;
    std::vector<AlgalonScanUnit> darkMatter;
    std::unordered_map<ObjectGuid, uint32> markerSeenMs;
    bool wormHolesSeen = false;

    uint32 lastStarDeathMs = 0;
    uint32 lastTickMs = 0;
};

// Instance lookup, never "find target" or "nearest npcs": both are phase filtered, and a bot inside a
// hole would lose him and every node with him.
Unit* GetAlgalon(PlayerbotAI* botAI);
bool AlgalonInRoom(Player* bot);
// Alive and the bot is in his room, pull or not. The formation settles on this before the pull.
bool AlgalonPresent(PlayerbotAI* botAI);
// In combat and not beaten.
bool AlgalonEngaged(PlayerbotAI* botAI);
bool IsAlgalonPhased(Unit const* unit);

// Runs from the definition tick, at most once per ULDUAR_ALGALON_STATE_TICK_MS per instance.
void AlgalonTickEncounterState(PlayerbotAI* botAI);

bool AlgalonBigBangCasting(PlayerbotAI* botAI);
// -1 when he isn't casting it.
int32 AlgalonBigBangRemainingMs(PlayerbotAI* botAI);
// Off the predicted clock; false while casting.
bool AlgalonBigBangWithin(PlayerbotAI* botAI, uint32 seconds);
// The cast the instance tick has latched a soaker for. Roles read this, never the live cast: in the
// tick's first 250ms of a cast nobody is the soaker yet, and every bot would run for a hole.
bool AlgalonBigBangLatched(Player* bot);

// What this bot does about the current Big Bang. Traced as algalon.hide.
enum class AlgalonHideRole : uint8
{
    None,
    Soak,
    Backup,
    Run,
    In,
    Exit,
    NoShelter,
    Wait,
    Hold,
};
AlgalonHideRole GetAlgalonHideRole(Player* bot);
// The Run branch alone, untraced and without latching a shelter, for rule predicates.
bool AlgalonShouldRunForShelter(Player* bot);
// The soaker, the backup, and a handler holding constellations: one follows its victim into a hole
// and closes it on the raid.
bool AlgalonStaysOut(Player* bot);
bool AlgalonHoldsConstellation(Player* bot);
// By spell id: CanCastSpell refuses during a channel, and a shadow priest is nearly always in Mind Flay.
bool AlgalonCanDisperse(Player* bot);

Player* GetAlgalonBigBangSoaker(PlayerbotAI* botAI);
Player* GetAlgalonBigBangBackup(PlayerbotAI* botAI);
// The hole this bot runs to for the current cast, latched.
bool GetAlgalonShelter(Player* bot, Position& shelter);
// Nearest cached hole within radius, readable while phased.
bool AlgalonHoleNear(Player* bot, float radius, Position* hole = nullptr);

// Whoever Algalon is swinging at. Alternates between the two tanks with the Phase Punch swap.
Player* GetAlgalonBossTank(PlayerbotAI* botAI);
bool IsAlgalonSwapTank(Player* player);
uint8 GetAlgalonPhasePunchStacks(Player* player);
// The swap tank that takes him when his target is nobody who should have him.
Player* GetAlgalonPickupTank(PlayerbotAI* botAI);

// Constellations in phase 1, Unleashed Dark Matter in phase 2: a third tank, else the swap tank not
// holding Algalon.
Player* GetAlgalonHandler(PlayerbotAI* botAI);
// A loose constellation to taunt first, else one already on the handler while a hole is spare to drag it
// through. Traced as algalon.kite.
Unit* GetAlgalonHandlerConstellation(Player* bot);
// Where the handler parks to drag its constellation through a spare hole.
bool GetAlgalonKiteSpot(Player* bot, Unit* constellation, Position& spot);
// A Dark Matter on anyone but the handler, the one hurting the most fragile victim first.
Unit* GetAlgalonLooseDarkMatter(Player* bot);

// Ranged DPS assigned to the stars; every ranged DPS joins while no hole stands with a Big Bang due.
bool IsAlgalonStarTeam(Player* bot);
// The star the team may kill right now, or nullptr while the pacing window is shut.
Unit* GetAlgalonFocusStar(PlayerbotAI* botAI);
uint8 AlgalonAliveStarCount(PlayerbotAI* botAI);

bool AlgalonCosmicSmashThreatens(Player* bot);
bool AlgalonCosmicSmashMarkerNear(PlayerbotAI* botAI, Position const& spot, float radius);
// Every marker and hole a dodge or an exit must stay clear of.
std::vector<EncounterHelpers::HazardCircle> GetAlgalonDodgeHazards(Player* bot, float markerClearance);
bool AlgalonSpotInRoom(float x, float y);

// Ranged and healers ring the tank slot; melee and the other tanks keep normal positioning.
bool AlgalonTakesRingSlot(Player* bot);
bool TryGetAlgalonSlotPosition(uint8 slotIndex, Position& position);
// The spot this bot should stand on, stepped aside when a hole buried it. Traced as algalon.spot.
bool TryGetAlgalonSlot(Player* bot, Position& position);
// The undisplaced slot, untraced, for dodges that prefer to land near it.
bool GetAlgalonSlotAnchor(Player* bot, Position& slot);

// Stars, constellations and Dark Matter the bot's own job doesn't cover.
void AppendAlgalonTargetExclusions(PlayerbotAI* botAI, GuidSet& exclusions);
// Same rule for damage already on its way: false vetoes a hit on this unit.
bool AlgalonMayDamage(Player* bot, Unit* target);

#endif
