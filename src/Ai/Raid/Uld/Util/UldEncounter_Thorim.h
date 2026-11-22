/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERTHORIM_H
#define PLAYERBOTS_ULDENCOUNTERTHORIM_H

#include "ObjectGuid.h"
#include "Position.h"
#include "RaidDefensive.h"
#include "RaidObs.h"
#include "UldData.h"

#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Player;
class PlayerbotAI;
class Unit;
class WorldObject;

enum UlduarThorimIds
{
    NPC_DARK_RUNE_ACOLYTE_I = 32886,
    NPC_CAPTURED_MERCENARY_SOLDIER_ALLY = 32885,
    NPC_CAPTURED_MERCENARY_SOLDIER_HORDE = 32883,
    NPC_CAPTURED_MERCENARY_CAPTAIN_ALLY = 32908,
    NPC_CAPTURED_MERCENARY_CAPTAIN_HORDE = 32907,
    NPC_JORMUNGAR_BEHEMOT = 32882,
    NPC_DARK_RUNE_WARBRINGER = 32877,
    NPC_DARK_RUNE_EVOKER = 32878,
    NPC_DARK_RUNE_CHAMPION = 32876,
    NPC_DARK_RUNE_COMMONER = 32904,
    NPC_IRON_RING_GUARD = 32874,
    NPC_RUNIC_COLOSSUS = 32872,
    NPC_ANCIENT_RUNE_GIANT = 32873,
    NPC_DARK_RUNE_ACOLYTE_G = 33110,
    NPC_IRON_HONOR_GUARD = 32875,
    NPC_THORIM_THUNDER_ORB = 33378,
    SPELL_UNBALANCING_STRIKE = 62130,
    SPELL_THORIM_LIGHTNING_CHARGE_BUFF = 62279,

    SPELL_THORIM_RUNIC_SMASH_LEFT = 62057,
    SPELL_THORIM_RUNIC_SMASH_RIGHT = 62058,
    SPELL_THORIM_RUNIC_BARRIER = 62338,
    SPELL_THORIM_LIGHTNING_ORB_VISUAL = 62186,
    SPELL_THORIM_CHARGE_ORB = 62016,

    NPC_SIF = 33196,
    NPC_SIF_BLIZZARD = 32879,
    SPELL_SIF_BLIZZARD_ZONE_10 = 62576,
    SPELL_SIF_BLIZZARD_ZONE_25 = 62602,
};

inline constexpr float ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD = 429.6094f;
inline constexpr float ULDUAR_THORIM_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;

inline constexpr float ULDUAR_THORIM_SIF_BLIZZARD_RADIUS = 15.0f;

inline constexpr float ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE = 11.0f;

inline constexpr float ULDUAR_THORIM_BLIZZARD_SCAN_RANGE = 70.0f;

inline constexpr float ULDUAR_THORIM_CAMP_BLIZZARD_ESCAPE_RADIUS = 15.0f;
inline constexpr float ULDUAR_THORIM_CAMP_MAX_BOSS_RANGE = 32.0f;

inline constexpr float ULDUAR_THORIM_SWEEP_STEP = 2.0f;
inline constexpr float ULDUAR_THORIM_SWEEP_ANGLE_STEP = static_cast<float>(M_PI) / 8.0f;

inline constexpr float ULDUAR_THORIM_FLOOR_RANGE = 110.0f;

inline constexpr float ULDUAR_THORIM_GATE_RADIUS = 10.0f;
inline constexpr float ULDUAR_THORIM_GATE_FOLLOW_DISTANCE = 5.0f;

inline constexpr float ULDUAR_THORIM_GAUNTLET_TETHER_DISTANCE = 20.0f;

inline constexpr float ULDUAR_THORIM_ENCOUNTER_PROXIMITY = 200.0f;
inline constexpr float ULDUAR_THORIM_WING_MAX_Z = 425.0f;

inline constexpr float ULDUAR_THORIM_BALCONY_BOX_MIN_X = 2100.0f;
inline constexpr float ULDUAR_THORIM_BALCONY_BOX_MAX_X = 2205.0f;
inline constexpr float ULDUAR_THORIM_BALCONY_BOX_MIN_Y = -455.0f;
inline constexpr float ULDUAR_THORIM_BALCONY_BOX_MAX_Y = -280.0f;
inline constexpr float ULDUAR_THORIM_BALCONY_BOX_MAX_Z = 450.0f;

inline constexpr float ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE = 150.0f;

inline constexpr uint32 ULDUAR_THORIM_RUNIC_SMASH_LATCH_MS = 10000;

inline constexpr uint32 ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS = 500;

inline constexpr float ULDUAR_THORIM_BARRIER_BAIL_HEALTH_PCT = 55.0f;
inline constexpr float ULDUAR_THORIM_BARRIER_RESUME_HEALTH_PCT = 80.0f;
inline constexpr float ULDUAR_THORIM_BARRIER_BAIL_DISTANCE = 14.0f;

inline constexpr float ULDUAR_THORIM_MELEE_RING_RADIUS = 8.8f;
inline constexpr uint8 ULDUAR_THORIM_MELEE_SLOTS = 3;

inline constexpr float ULDUAR_THORIM_RING_SLOT_SEPARATION = 1.0472f;

inline constexpr float ULDUAR_THORIM_PET_SLOT_TOLERANCE = 0.3927f;

inline constexpr uint8 ULDUAR_THORIM_RANGED_SLOTS = 6;

inline constexpr uint8 ULDUAR_THORIM_OPENING_SLOTS = 4;
inline constexpr uint32 ULDUAR_THORIM_OPENING_HOLD_MIN_MS = 12500;
inline constexpr uint32 ULDUAR_THORIM_OPENING_HOLD_MAX_MS = 25000;
inline constexpr float ULDUAR_THORIM_OPENING_SETTLED_RADIUS = 8.0f;

inline constexpr float ULDUAR_THORIM_PHASE2_SPOT_TOLERANCE = 1.0f;

inline constexpr float ULDUAR_THORIM_RING_ARRIVE_TOLERANCE = 3.0f;
inline constexpr float ULDUAR_THORIM_RING_REPOSITION_TOLERANCE = 5.0f;

inline constexpr float ULDUAR_THORIM_DPS_TARGET_RANGE = 50.0f;

inline constexpr float ULDUAR_THORIM_TARGET_SWITCH_MARGIN = 8.0f;

inline constexpr float ULDUAR_THORIM_MELEE_TARGET_REACH = 15.0f;

inline constexpr float ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE = 1.3090f;
inline constexpr float ULDUAR_THORIM_LIGHTNING_CHARGE_MARGIN = 0.2618f;
inline constexpr float ULDUAR_THORIM_RING_CONE_CLEARANCE = 0.0873f;
inline constexpr float ULDUAR_THORIM_LIGHTNING_CHARGE_RANGE = 150.0f;

inline constexpr float ULDUAR_THORIM_LIGHTNING_CHARGE_RANGED_MARGIN = 0.0436f;

inline constexpr uint32 ULDUAR_THORIM_LIGHTNING_CHARGE_WARNING_MS = 4800;

inline constexpr uint32 ULDUAR_THORIM_LIGHTNING_CHARGE_DAMAGE = 27000;

inline constexpr RaidDefensiveScope ULDUAR_THORIM_DEFENSIVE_SCOPE = {ULDUAR_MAP_ID, "thorim.cover",
                                                                     "thorim.personal"};

inline constexpr float ULDUAR_THORIM_THUNDER_ORB_MATCH_RADIUS = 4.0f;

inline constexpr float ULDUAR_THORIM_ARENA_BOX_MIN_X = 2085.0f;
inline constexpr float ULDUAR_THORIM_ARENA_BOX_MAX_X = 2185.0f;
inline constexpr float ULDUAR_THORIM_ARENA_BOX_MIN_Y = -305.0f;
inline constexpr float ULDUAR_THORIM_ARENA_BOX_MAX_Y = -214.0f;
inline constexpr float ULDUAR_THORIM_ARENA_BOX_MAX_Z = 425.0f;

inline constexpr float ULDUAR_THORIM_ARENA_LEASH_RADIUS = 30.0f;

inline constexpr float ULDUAR_THORIM_PET_OWNER_LEASH_RADIUS = ULDUAR_THORIM_DPS_TARGET_RANGE;

inline constexpr uint32 ULDUAR_THORIM_ARENA_MIN_MEMBERS = 3;

inline constexpr float ULDUAR_THORIM_ARENA_RING_INNER = 13.0f;
inline constexpr float ULDUAR_THORIM_ARENA_RING_OUTER = 18.0f;
inline constexpr uint8 ULDUAR_THORIM_ARENA_RING_INNER_SLOTS = 5;

inline constexpr float ULDUAR_THORIM_CHARGED_ORB_RADIUS = 32.3f;
inline constexpr float ULDUAR_THORIM_CHARGED_ORB_MARGIN = 4.0f;

inline constexpr float ULDUAR_THORIM_ARENA_MELEE_LEASH = 24.0f;

extern const Position ULDUAR_THORIM_NEAR_ARENA_CENTER;
extern const Position ULDUAR_THORIM_NEAR_ENTRANCE_POSITION;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3;
extern const Position ULDUAR_THORIM_BALCONY_1;
extern const Position ULDUAR_THORIM_BALCONY_2;
extern const Position ULDUAR_THORIM_BALCONY_3;
extern const Position ULDUAR_THORIM_BALCONY_4;
extern const Position ULDUAR_THORIM_BALCONY_5;
extern const Position ULDUAR_THORIM_JUMP_START_POINT;
extern const Position ULDUAR_THORIM_JUMP_END_POINT;
extern const Position ULDUAR_THORIM_BALCONY_HOLD1_SPOT;
extern const Position ULDUAR_THORIM_BALCONY_HOLD2_SPOT;
extern const Position ULDUAR_THORIM_BALCONY_HOLD3_SPOT;
extern const Position ULDUAR_THORIM_BALCONY_HOLD4_SPOT;
extern const Position ULDUAR_THORIM_BALCONY_HOLD5_SPOT;
extern const Position ULDUAR_THORIM_BALCONY_HOLD6_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_TANK_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE1_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE2_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE3_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE4_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE5_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE6_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_OPENING1_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_OPENING2_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_OPENING3_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_OPENING4_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_MELEE1_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_MELEE2_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_MELEE3_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_OFFTANK_SPOT;

struct ThorimEncounterState
{
    RaidObs::ObsGuidMap<uint8> meleeSlots{"thorim.slot"};

    RaidObs::ObsGuidMap<uint8> rangedSlots{"thorim.rangedslot"};

    RaidObs::ObsGuidMap<uint8> openingSlots{"thorim.openingslot"};
    RaidObs::ObsGuidMap<uint8> balconyHoldSlots{"thorim.balconyhold"};

    RaidObs::ObsGuidSet openingReleased{"thorim.openingreleased"};

    RaidObs::ObsValue<uint32> phase2StartMs{"thorim.p2start"};

    ObjectGuid humanCallHolder;
    bool humanCallTurnDone = false;
    uint32 humanCallScanMs = 0;

    struct RingSlot
    {
        float latched = 0.0f;
        float composed = 0.0f;
    };

    std::array<RingSlot, ULDUAR_THORIM_MELEE_SLOTS> ringSlots;
    float ringAnchor = 0.0f;
    bool ringLatched = false;
    ObjectGuid ringOrb;
    uint32 ringSolveMs = 0;

    struct RangedShelter
    {
        ObjectGuid orb;
        uint8 orbIndex = 0;
        bool sheltered = false;
    };

    std::unordered_map<ObjectGuid, RangedShelter> rangedShelters;

    RaidObs::ObsValue<uint32> runicSmashSide{"thorim.smashside"};
    uint32 runicSmashSeenMs = 0;
    uint32 smashScanMs = 0;

    RaidObs::ObsValue<ObjectGuid> chargedOrbGuid{"thorim.chargedorb"};
    uint32 orbScanMs = 0;

    RaidObs::ObsValue<ObjectGuid> lightningOrbGuid{"thorim.lightningorb"};
    uint32 lightningOrbScanMs = 0;
    uint32 lightningOrbLitMs = 0;

    std::vector<Position> blizzardSpots;
    uint32 blizzardScanMs = 0;

    RaidObs::ObsGuidMap<Position> orbEscapes{"thorim.orbescape"};

    ObjectGuid bossGuid;

    ObjectGuid colossusGuid;
    uint32 colossusScanMs = 0;

    ObjectGuid runeGiantGuid;
    uint32 runeGiantScanMs = 0;

    RaidObs::ObsGuidSet barrierBailing{"thorim.barrierbail"};

    RaidObs::ObsValue<ObjectGuid> frostAuraPaladin{"thorim.frostaura"};
    uint32 frostAuraScanMs = 0;

    RaidObs::ObsValue<ObjectGuid> anchorTank{"thorim.anchortank"};
    uint32 anchorTankScanMs = 0;

    RaidObs::ObsValue<bool> sifJoined{"thorim.sifjoined"};

    RaidObs::ObsGuidMap<uint8> squads{"thorim.squad"};
    RaidObs::ObsValue<bool> squadsAssigned{"thorim.squadsassigned"};

    uint32 humanSquadScanMs = 0;

    bool squadsNoted = false;

    RaidObs::ObsGuidMap<ObjectGuid> dpsTargets{"thorim.dpstarget"};

    RaidObs::ObsGuidSet followMasterStripped{"thorim.followstripped"};

    RaidObs::ObsGuidMap<uint8> balconyStep{"thorim.balcony"};

    RaidObs::ObsGuidMap<ObjectGuid> petRecalls{"thorim.petrecall"};

    std::unordered_map<ObjectGuid, uint32> petRecallMs;

    RaidObs::ObsGuidSet arenaAnchorArrived{"thorim.arenaarrived"};

    RaidObs::ObsGuidSet ringArrived{"thorim.ringarrived"};

    bool gauntletTraced = false;

    RaidObs::ObsValue<bool> leverPulled{"thorim.leverpulled"};
    uint8 leadNext = 0;
    uint32 leadStepMs = 0;

    RaidObs::ObsValue<bool> marksCleared{"thorim.markscleared"};

    bool engagedSeen = false;

    std::unordered_set<ObjectGuid> resetDone;
};

enum class ThorimSquad : uint8
{
    None,
    Arena,
    Gauntlet
};

enum class ThorimPhase2Role : uint8
{
    None,
    MainTank,
    OffTank,
    Ranged,
    MeleeRing
};

Unit* GetThorim(PlayerbotAI* botAI);

Unit* GetThorimRunicColossus(PlayerbotAI* botAI);

struct ThorimEncounterTargets
{
    std::vector<Unit*> acolytes;
    std::vector<Unit*> evokers;
    std::vector<Unit*> champions;
    std::vector<Unit*> warbringers;
    std::vector<Unit*> commoners;
    std::vector<Unit*> guards;
    std::vector<Unit*> trash;
    Unit* colossus = nullptr;
    Unit* runeGiant = nullptr;
};

void GatherThorimEncounterTargets(PlayerbotAI* botAI, ThorimEncounterTargets& out);

bool ThorimDpsTargetAllowed(PlayerbotAI* botAI, Unit* target);

bool ThorimBalconyHitAllowed(PlayerbotAI* botAI, Player* bot);

Unit* GetThorimDpsTarget(PlayerbotAI* botAI, Player* bot, Unit* currentTarget);

bool ThorimHasDpsTarget(PlayerbotAI* botAI, Player* bot);

void ThorimClearStaleMarks(PlayerbotAI* botAI, Player* bot);

inline constexpr uint8 ULDUAR_THORIM_GAUNTLET_WAYPOINTS = 6;

Position const& GetThorimGauntletWaypoint(bool leftLane, uint8 index);

bool ThorimGauntletLaneIndexInLane(WorldObject const* who, bool leftLane, uint8& index);
bool ThorimGauntletLaneIndex(WorldObject const* who, uint8& index, bool& leftLane);

inline constexpr float ULDUAR_THORIM_LEAD_COHESION_DISTANCE = 30.0f;
inline constexpr uint32 ULDUAR_THORIM_LEAD_STALL_MS = 15000;

inline constexpr float ULDUAR_THORIM_LEAD_COLOSSUS_DISTANCE = 30.0f;

inline constexpr float ULDUAR_THORIM_LEAD_FOLLOW_DISTANCE = 25.0f;

Player* ThorimGauntletLeader(PlayerbotAI* botAI);

bool ThorimGauntletLeaderIsBot(Player const* leader);

bool ThorimIsBotGauntletLeader(PlayerbotAI* botAI, Player* bot);

bool ThorimLeverUsable(Player* bot, ObjectGuid& lever);
void ThorimNoteLeverPulled(Player* bot);

bool ThorimGauntletLeadStep(PlayerbotAI* botAI, Player* bot, Position& out);

Unit* ThorimGauntletLeadColossus(PlayerbotAI* botAI, Player* bot);

Unit* ThorimGauntletLeadRuneGiant(PlayerbotAI* botAI, Player* bot);

bool ThorimResolveGauntletIndex(PlayerbotAI* botAI, Player* bot, uint8& index);

bool ThorimPreferredGauntletLane(PlayerbotAI* botAI, bool& useLeftLane);

bool ThorimRunicSmashImminent(PlayerbotAI* botAI);

inline constexpr uint8 ULDUAR_THORIM_BALCONY_WAYPOINTS = 6;

inline constexpr float ULDUAR_THORIM_BALCONY_ARRIVE_TOLERANCE = 6.0f;

inline constexpr uint8 ULDUAR_THORIM_BALCONY_HOLD_SLOTS = 6;
inline constexpr float ULDUAR_THORIM_BALCONY_HOLD_TOLERANCE = 1.5f;

Position const& GetThorimBalconyWaypoint(uint8 index);

Unit* GetThorimAncientRuneGiant(PlayerbotAI* botAI);

bool ThorimBalconyOpen(PlayerbotAI* botAI, Player* bot);

uint8 ThorimAdvanceBalconyStep(Player* bot);

void ThorimTickBarrierBail(PlayerbotAI* botAI, Player* bot);
bool ThorimBarrierBailLatched(PlayerbotAI* botAI, Player* bot);

inline constexpr uint32 ULDUAR_THORIM_HUMAN_CALL_STACKS = 6;
inline constexpr float ULDUAR_THORIM_HUMAN_CALL_LOW_PCT = 50.0f;
void ThorimTickHumanTankCalls(PlayerbotAI* botAI);

bool ThorimSplitActive(PlayerbotAI* botAI);

ThorimSquad GetThorimSquad(PlayerbotAI* botAI, Player* bot);

bool ThorimInArenaBox(WorldObject const* who);

bool NearThorimEncounter(Player const* bot);

bool ThorimArenaLeashBreached(PlayerbotAI* botAI, Player* bot);

void ThorimTickAnchorTank(PlayerbotAI* botAI);

bool IsThorimAnchorTank(Player const* bot);

void ThorimTickFrostAuraPaladin(PlayerbotAI* botAI);

bool IsThorimFrostAuraPaladin(Player const* bot);

bool ThorimStrayPets(PlayerbotAI* botAI, Player* bot, std::vector<Unit*>& out);

void ThorimRecallPet(Player* bot, Unit* pet);

bool ThorimPetsOffRingSlot(PlayerbotAI* botAI, Player* bot, std::vector<Unit*>& out);

void ThorimSendPetToRingSlot(PlayerbotAI* botAI, Player* bot, Unit* pet);

bool GetThorimArenaAnchor(PlayerbotAI* botAI, Player* bot, Position& out);

bool ThorimArenaAnchorNeedsMove(PlayerbotAI* botAI, Player* bot, Position const& spot);

bool ThorimArenaAnchorSettled(PlayerbotAI* botAI, Player* bot);

void ThorimNoteFollowMasterStripped(Player* bot);
bool ThorimFollowMasterStripped(Player const* bot);

bool ThorimPhase2Active(PlayerbotAI* botAI);

bool ThorimPhase2ElapsedMs(PlayerbotAI* botAI, uint32& elapsedMs);

bool ThorimPhase2OpeningHold(PlayerbotAI* botAI, Player* bot, Position const& holdSpot);

bool ThorimBalconyHoldSpot(PlayerbotAI* botAI, Player* bot, Position& out);

ThorimPhase2Role GetThorimPhase2Role(PlayerbotAI* botAI, Player* bot);
bool TryGetThorimPhase2Spot(PlayerbotAI* botAI, Player* bot, ThorimPhase2Role role, Position& position);

bool ThorimRingWantsMove(PlayerbotAI* botAI, Player* bot, Position const& spot);

void ThorimRingMarkArrived(Player* bot);
void ThorimRingClearArrived(Player* bot);

bool ThorimMeleeRingSettled(PlayerbotAI* botAI, Player* bot);

bool ThorimTankOnAnchor(PlayerbotAI* botAI, Player* bot);

bool ThorimSpotUnderBlizzard(Player* bot, Position const& spot);

bool ThorimWalkUnderBlizzard(Player* bot, Position const& from, Position const& to);

bool ThorimCampSpotInLitCone(PlayerbotAI* botAI, Position const& spot);

bool ThorimCampBlizzardEscape(PlayerbotAI* botAI, Player* bot, Position& out);

Unit* ThorimChargedThunderOrb(PlayerbotAI* botAI, uint32 markerSpell);

bool GetThorimLightningChargeHit(PlayerbotAI* botAI, RaidPredictedHit& hit);

bool ThorimChargedOrbEscape(PlayerbotAI* botAI, Player* bot, Position& out);
void ThorimNoteOrbEscape(Player* bot, Position const& spot);

void ResetThorimEncounterState(Player* bot, bool clearInstance);
bool ThorimEncounterStateIsStale(PlayerbotAI* botAI);
bool ThorimBotHasEncounterState(Player* bot);

#endif
