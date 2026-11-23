/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERVEZAX_H
#define PLAYERBOTS_ULDENCOUNTERVEZAX_H

#include "Position.h"
#include "RaidObs.h"
#include "UldData.h"

#include "ObjectGuid.h"

#include <vector>

class Creature;
class Player;
class PlayerbotAI;
class Spell;
class Unit;

enum UlduarVezaxIds
{
    SPELL_MARK_OF_THE_FACELESS = 63276,
    SPELL_VEZAX_SHADOW_CRASH_CAST = 62660,
    SPELL_VEZAX_SHADOW_CRASH_DMG = 62659,
    SPELL_VEZAX_SHADOW_CRASH_FIELD = 63277,
    SPELL_VEZAX_SEARING_FLAMES = 62661,
    SPELL_VEZAX_SURGE_OF_DARKNESS = 62662,
    SPELL_VEZAX_SARONITE_BARRIER = 63364,
    SPELL_VEZAX_SARONITE_VAPORS_AURA = 63323,
    SPELL_VEZAX_SUMMON_SARONITE_ANIMUS = 63145,
    SPELL_VEZAX_SUMMON_SARONITE_VAPORS = 63081,
    SPELL_VEZAX_PROFOUND_DARKNESS = 63420,

    NPC_VEZAX = 33271,
    NPC_VEZAX_SARONITE_ANIMUS = 33524,
    NPC_VEZAX_SARONITE_VAPORS = 33488,
};

inline constexpr float ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS = 10.0f;
inline constexpr float ULDUAR_VEZAX_SHADOW_CRASH_WALK_CLEARANCE = 11.0f;
inline constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_CLEARANCE = 12.0f;
inline constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_SEARCH_RADIUS = 25.0f;
inline constexpr uint32 ULDUAR_VEZAX_SHADOW_CRASH_LAND_SLACK_MS = 500;

inline constexpr uint32 ULDUAR_VEZAX_ANIMUS_TANK_LEAD_MS = 2000;
inline constexpr uint32 ULDUAR_VEZAX_ANIMUS_HOLD_CAP_MS = 6000;
inline constexpr uint8 ULDUAR_VEZAX_REDIRECT_SAVE_SUMMON = 5;
inline constexpr float ULDUAR_VEZAX_TRICKS_RANGE = 20.0f;
inline constexpr float ULDUAR_VEZAX_THREAT_SHARE = 0.9f;
inline constexpr float ULDUAR_VEZAX_VAPOR_AREA_CLEARANCE = 15.0f;
inline constexpr float ULDUAR_VEZAX_HOLD_HEALTH_PCT = 15.0f;
inline constexpr float ULDUAR_VEZAX_TANK_HOLD_HEALTH_PCT = 2.0f;
inline constexpr float ULDUAR_VEZAX_ANIMUS_BRING_BACK_RADIUS = 3.0f;
inline constexpr float ULDUAR_VEZAX_SHADOW_CRASH_MIN_RANGE = 3.0f;
inline constexpr float ULDUAR_VEZAX_ANIMUS_MELEE_MARGIN = 2.0f;
inline constexpr float ULDUAR_VEZAX_ANIMUS_MELEE_SPREAD = 2.0f;
inline constexpr uint8 ULDUAR_VEZAX_ANIMUS_MELEE_LANES = 5;

inline constexpr float ULDUAR_VEZAX_HAZARD_RADIUS = 8.0f;
inline constexpr float ULDUAR_VEZAX_HAZARD_CLEARANCE = 9.0f;
inline constexpr float ULDUAR_VEZAX_HAZARD_SEARCH_RADIUS = 60.0f;
inline constexpr float ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS = 25.0f;

inline constexpr float ULDUAR_VEZAX_SHADOW_CRASH_SOAK_MAX_TRAVEL = 20.0f;

inline constexpr float ULDUAR_VEZAX_ARC_ORIENTATION = 1.5708f;

inline constexpr float ULDUAR_VEZAX_CAMP_RADIUS = 35.0f;
inline constexpr float ULDUAR_VEZAX_CAMP_ROW_SPACING = 2.25f;
inline constexpr float ULDUAR_VEZAX_CAMP_FILE_SPACING = 3.0f;
inline constexpr float ULDUAR_VEZAX_CAMP_GROUP_OFFSET = 4.0f;
inline constexpr float ULDUAR_VEZAX_CAMP_STRAFE = 15.0f;
inline constexpr uint8 ULDUAR_VEZAX_CAMP_ROWS = 5;
inline constexpr uint8 ULDUAR_VEZAX_CAMP_FILES = 2;

inline constexpr uint8 ULDUAR_VEZAX_GROUP_SLOTS = ULDUAR_VEZAX_CAMP_ROWS * ULDUAR_VEZAX_CAMP_FILES;
inline constexpr uint8 ULDUAR_VEZAX_TOTAL_SLOTS = 2 * ULDUAR_VEZAX_GROUP_SLOTS;

inline constexpr float ULDUAR_VEZAX_SLOT_TOLERANCE = 1.2f;
inline constexpr float ULDUAR_VEZAX_TANK_SLOT_TOLERANCE = 3.0f;

inline constexpr float ULDUAR_VEZAX_ARENA_RADIUS = 45.0f;
inline constexpr float ULDUAR_VEZAX_ARENA_HEIGHT = 10.0f;

inline constexpr float ULDUAR_VEZAX_FLOOR_STEP = 5.0f;
inline constexpr float ULDUAR_VEZAX_FLOOR_CLIP = 1.0f;

inline constexpr float ULDUAR_VEZAX_MARK_LEECH_RADIUS = 15.0f;
inline constexpr float ULDUAR_VEZAX_MARK_BREAK_DISTANCE = 18.0f;

inline constexpr float ULDUAR_VEZAX_MARK_SIDE_OFFSET = 40.0f;

inline constexpr float ULDUAR_VEZAX_MARK_SPOT_RADIUS = 26.0f;
inline constexpr float ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET = 2.3208f;
inline constexpr float ULDUAR_VEZAX_MARK_SPOT_TOLERANCE = 3.0f;
inline constexpr uint8 ULDUAR_VEZAX_MARK_SPOT_COUNT = 3;

extern const Position ULDUAR_VEZAX_ANCHOR;

struct VezaxHazard
{
    Position position;
    float radius = ULDUAR_VEZAX_HAZARD_RADIUS;
};

struct VezaxEncounterState
{
    RaidObs::ObsGuidMap<uint8> slotAssignments{"vezax.slot"};

    Position crashImpact;
    uint32 crashCastMs = 0;
    uint32 crashWindowMs = 0;

    bool vaporKilled = false;
    bool animusSummoned = false;
    uint32 animusSummonMs = 0;
    uint8 vaporSummons = 0;
};

Unit* GetVezax(PlayerbotAI* botAI);
bool VezaxEncounterActive(PlayerbotAI* botAI);

bool VezaxFormationActive(PlayerbotAI* botAI);

void TickVezax(PlayerbotAI* botAI);

void GatherVezaxHazards(Player* bot, std::vector<VezaxHazard>& hazards,
                        float searchRadius = ULDUAR_VEZAX_HAZARD_SEARCH_RADIUS);

bool TryGetVezaxNearestHazard(Player* bot, std::vector<VezaxHazard> const& hazards,
                              VezaxHazard& hazard);

bool VezaxCanSoakShadowCrashField(Player* bot);

bool TryGetVezaxSlotPosition(Player* bot, uint8 slotIndex, Position& position);
float VezaxSlotTolerance(Player* bot);
void EnsureVezaxSlotAssignments(Player* bot);

bool TryGetVezaxSlot(Player* bot, Position& position);

bool VezaxPlaceOnFloor(Player* bot, Position& point);

bool TryGetVezaxMarkSpot(Player* bot, Position& position);

Unit* GetVezaxMarkedAlly(Player* bot);

bool TryGetVezaxMarkBreakSpot(Player* bot, Unit* marked, Position& spot);

bool TryGetVezaxShadowCrashImpact(PlayerbotAI* botAI, Position& impact);
void VezaxNoteShadowCrash(Unit* vezax, Position const& impact, uint32 flightMs);
bool VezaxWalkMeetsShadowCrash(PlayerbotAI* botAI, Position const& destination);
bool VezaxReachSpellMeetsShadowCrash(PlayerbotAI* botAI);
bool VezaxMarkWalkMeetsShadowCrash(PlayerbotAI* botAI);
bool VezaxCarriesProfoundDarkness(PlayerbotAI* botAI);

bool VezaxHardModePending(PlayerbotAI* botAI);
bool VezaxDamageHeld(PlayerbotAI* botAI);
bool VezaxVaporInAreaReach(PlayerbotAI* botAI);
void VezaxHushPet(Creature* pet);
void VezaxUnhushPet(Creature* pet);
bool VezaxIsHushedPet(Creature* pet);
bool VezaxHarmfulCastOn(Spell const* spell, Unit* vezax);
void VezaxNoteVaporKilled(Unit* vapor);
void VezaxNoteAnimusSummoned(Unit* vapor);
void VezaxNoteVaporSummon(Unit* vezax);

bool VezaxAnimusDue(PlayerbotAI* botAI);
bool TryGetVezaxAnimusAge(PlayerbotAI* botAI, uint32& ageMs);
char const* VezaxAnimusRedirectSpell(Player* bot);
bool IsVezaxAnimusOnBot(Player* bot);
bool VezaxAnimusPhaseActive(PlayerbotAI* botAI);
bool VezaxTargetingAnimus(PlayerbotAI* botAI);
bool VezaxAnimusOffAnchor(PlayerbotAI* botAI);
float VezaxMeleeHoldDistance(Player* bot, Unit* vezax);
bool TryGetVezaxAnimusMeleeSpot(Player* bot, Unit* animus, Position& spot);

bool VezaxDodgesShadowCrash(Player* bot);
bool TryGetVezaxDodgeSpot(Player* bot, Position const& impact, Position& spot);

void ResetVezaxEncounterState(Player* bot, bool clearInstance);
bool VezaxHasEncounterState(Player* bot);

char const* VezaxReadyInterrupt(Player* bot, Unit* target);

bool VezaxIsSearingFlamesInterrupter(Player* bot, Unit* boss);

#endif
