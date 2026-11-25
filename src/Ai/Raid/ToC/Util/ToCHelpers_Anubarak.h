#ifndef PLAYERBOTS_RAID_TOCHELPERS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCHELPERS_ANUBARAK_H

#include <vector>

#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

class Creature;
class Player;
class Unit;

namespace TrialOfTheCrusaderHelpers
{

inline constexpr uint32 SPELL_FROST_SPHERE = 67539;
inline constexpr uint32 SPELL_PERMAFROST = 66193;
inline constexpr uint32 SPELL_PENETRATING_COLD = 66013;
inline constexpr uint32 SPELL_SHADOW_STRIKE = 66134;
inline constexpr uint32 SPELL_EXPOSE_WEAKNESS = 67721;
inline constexpr uint32 SPELL_DEATH_GRIP = 49576;

extern const Position ANUBARAK_ROOM_CENTER;
inline constexpr float ANUBARAK_ROOM_RADIUS = 120.0f;
inline constexpr float ANUBARAK_TANK_PATCH_OFFSET = 12.0f;
inline constexpr float ANUBARAK_TANK_PATCH_LATCH = 30.0f;
inline constexpr float ANUBARAK_TANK_PATCH_ARRIVE = 2.0f;
inline constexpr float ANUBARAK_PERMAFROST_SLOW_REACH = 7.5f;
inline constexpr float ANUBARAK_TANK_KEEPOUT = ANUBARAK_PERMAFROST_SLOW_REACH + 1.0f;
inline constexpr float ANUBARAK_PICKUP_STAND_OFFSET = 10.0f;

inline constexpr float ANUBARAK_SIDE_SHOOT_NEAR = 12.0f;
inline constexpr float ANUBARAK_SIDE_SHOOT_FAR = 20.0f;
inline constexpr uint32 ANUBARAK_SIDE_SHOOT_WAIT_MS = 10000;
inline constexpr float ANUBARAK_TANK_PATCH_GAP = 20.0f;

inline constexpr float ANUBARAK_GRIP_RANGE = 28.0f;
inline constexpr float ANUBARAK_GRIP_LANDED = 3.0f;
inline constexpr float ANUBARAK_GRIP_LANDED_HEIGHT = 4.0f;
inline constexpr uint32 ANUBARAK_GRIP_JUDGE_MS = 3000;
inline constexpr uint32 ANUBARAK_GRIP_COOLDOWN_WAIT_MS = 3000;
inline constexpr float ANUBARAK_GRIP_STEP_OUT = 16.0f;
inline constexpr float ANUBARAK_GRIP_REACH_MARGIN = 3.0f;

inline constexpr float ANUBARAK_SIDE_TANK_RIM = 7.0f;
inline constexpr float ANUBARAK_SIDE_BACKUP_REACH = 8.0f;
inline constexpr float ANUBARAK_MELEE_BURROWER_REACH = 4.5f;
inline constexpr float ANUBARAK_MELEE_PATCH_CLEARANCE = 8.0f;
inline constexpr float ANUBARAK_SWARM_MELEE_REACH_MARGIN = 0.25f;
inline constexpr float ANUBARAK_SWARM_MELEE_PATCH_CLEARANCE = 7.9f;

inline constexpr uint8 ANUBARAK_BURROWERS_PER_SIDE = 2;
inline constexpr uint32 ANUBARAK_WAVE_FIRST_MS = 5000;
inline constexpr uint32 ANUBARAK_WAVE_INTERVAL_MS = 45000;
inline constexpr uint32 ANUBARAK_WAVE_GAP_MS = 10000;
inline constexpr uint32 ANUBARAK_WAVE_READY_MS = 8000;
inline constexpr uint32 ANUBARAK_SURFACED_MS = 80000;
inline constexpr uint32 ANUBARAK_SUBMERGED_MS = 62000;
inline constexpr float ANUBARAK_FOCUS_OFF_FROST_PCT = 10.0f;
inline constexpr float ANUBARAK_FOCUS_SETTLE = 5.0f;
inline constexpr uint32 ANUBARAK_FOCUS_SETTLE_MS = 4000;
inline constexpr uint32 ANUBARAK_FOCUS_STACKS = 6;
inline constexpr uint32 ANUBARAK_BURROWER_FREE_MS = 25000;

inline constexpr uint32 ANUBARAK_SIDE_DEFENSIVE_STACKS = 5;
inline constexpr float ANUBARAK_SIDE_DEFENSIVE_PCT = 50.0f;
inline constexpr uint32 ANUBARAK_SIDE_DEFENSIVE_KEEP_COOLDOWN_MS = 120000;
inline constexpr float ANUBARAK_BATTLE_RES_REACH = 28.0f;
inline constexpr float ANUBARAK_LEAD_REACH = 25.0f;
inline constexpr float ANUBARAK_LEAD_STOP = 20.0f;
inline constexpr float ANUBARAK_LEAD_REAIM = 3.0f;
inline constexpr float ANUBARAK_TAUNT_ARRIVE = 40.0f;
inline constexpr float ANUBARAK_TAUNT_RESCUE_PCT = 50.0f;
inline constexpr float ANUBARAK_SPIKE_PATCH_REACH = 11.5f;
inline constexpr float ANUBARAK_KITE_STAND_OFFSET = 8.5f;
inline constexpr float ANUBARAK_KITE_ARRIVE = 2.0f;
inline constexpr float ANUBARAK_KITE_RING_RADIUS = 40.0f;
inline constexpr float ANUBARAK_KITE_BOOST_RANGE = 15.0f;
inline constexpr float ANUBARAK_KITE_BOOST_CLOSE = 10.0f;
inline constexpr float ANUBARAK_SPIKE_ROUTE_CLEARANCE = 14.5f;
inline constexpr float ANUBARAK_IMPALE_REACH = 7.5f;
inline constexpr float ANUBARAK_CIRCLE_TELEPORT_GAIN = 15.0f;
inline constexpr int32 ANUBARAK_CIRCLE_REFRESH_MS = 100000;
inline constexpr float ANUBARAK_CIRCLE_CENTRE_SLACK = 2.0f;
inline constexpr int32 ANUBARAK_CIRCLE_PREPULL_MS = 290000;
inline constexpr float ANUBARAK_BLINK_DISTANCE = 20.0f;
inline constexpr float ANUBARAK_GATHER_ARRIVE = 6.0f;
inline constexpr float ANUBARAK_GATHER_SPREAD = 3.0f;
inline constexpr float ANUBARAK_HEALER_STAND_LEAVE = 6.0f;
inline constexpr float ANUBARAK_GATHER_MELEE_SCARAB = 10.0f;
inline constexpr float ANUBARAK_SPIKE_DANGER_RADIUS = 10.0f;
inline constexpr float ANUBARAK_SPIKE_CLEARANCE = 13.0f;
inline constexpr float ANUBARAK_SPIKE_LANE_HALF_WIDTH = 7.0f;
inline constexpr float ANUBARAK_SPIKE_LANE_CLEARANCE = 9.0f;
inline constexpr float ANUBARAK_SPIKE_DANGER_RADIUS_FAST = 16.0f;
inline constexpr float ANUBARAK_SPIKE_CLEARANCE_FAST = 19.0f;
inline constexpr float ANUBARAK_SPIKE_LANE_HALF_WIDTH_FAST = 10.0f;
inline constexpr float ANUBARAK_SPIKE_LANE_CLEARANCE_FAST = 12.0f;
inline constexpr float ANUBARAK_SPIKE_DODGE_SEARCH = 30.0f;
inline constexpr float ANUBARAK_INTERRUPT_RANGE = 30.0f;
inline constexpr uint32 ANUBARAK_SUBMERGE_PREP_MS = 65000;

inline constexpr float ANUBARAK_DRAG_START = 8.0f;
inline constexpr float ANUBARAK_DRAG_DONE = 4.0f;
inline constexpr uint32 ANUBARAK_DRAG_SETTLE_MS = 500;
inline constexpr uint32 ANUBARAK_DRAG_LEG_MS = 6000;
inline constexpr float ANUBARAK_SUBMERGE_PATCH_CLEARANCE = ANUBARAK_SPIKE_PATCH_REACH + 5.25f + ANUBARAK_DRAG_DONE;
inline constexpr float ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN = ANUBARAK_SPIKE_PATCH_REACH + 5.25f;

inline constexpr float ANUBARAK_THREAT_CAP = 0.9f;
inline constexpr float ANUBARAK_THREAT_RELEASE = 0.8f;
inline constexpr float ANUBARAK_SIDE_BOSS_CAP = 0.7f;
inline constexpr float ANUBARAK_SIDE_BOSS_RELEASE = 0.6f;
inline constexpr float ANUBARAK_MELEE_AREA_CAP = 0.6f;
inline constexpr float ANUBARAK_MELEE_AREA_RELEASE = 0.5f;

inline constexpr uint8 ANUBARAK_SHOOTERS_PER_NEED = 2;
inline constexpr uint8 ANUBARAK_SHOOTERS_PER_STOCK_UP = 1;
inline constexpr uint8 ANUBARAK_P2_PATCH_STOCK = 2;
inline constexpr uint8 ANUBARAK_P2_PATCH_STOCK_HEROIC = 2;
inline constexpr float ANUBARAK_SHOOT_MARGIN = 2.0f;

inline constexpr float ANUBARAK_DODGE_ARRIVE = 2.0f;
inline constexpr float ANUBARAK_DODGE_HOLD_TRIGGER = 2.5f;
inline constexpr uint32 ANUBARAK_DODGE_WALK_MS = 3000;
inline constexpr uint32 ANUBARAK_DODGE_HOLD_MS = 8000;
inline constexpr float ANUBARAK_DODGE_TRACK_S = 3.0f;

inline constexpr float ANUBARAK_ESCORT_HOLD = 4.0f;
inline constexpr float ANUBARAK_KITE_IMMUNITY_RANGE = 9.0f;
inline constexpr float ANUBARAK_HOP_RANGE = 30.0f;
inline constexpr uint32 SPELL_FORBEARANCE = 25771;
inline constexpr uint32 ANUBARAK_KITE_HOP_EMERGE_MS = 10000;
inline constexpr uint32 ANUBARAK_KITE_IMMUNITIES[] = {1022, 5599, 10278, 642, 45438};

enum class AnubarakPhase : uint8
{
    None = 0,
    Surface = 1,
    Submerged = 2,
    Swarm = 3
};

enum class AnubarakKiteBranch : uint8
{
    None,
    Patch,
    Ring,
    Hold
};

enum class AnubarakKiteStage : uint8
{
    None,
    Run,
    Boost,
    Immune,
    Patch
};

struct AnubarakKiteBoost
{
    uint32 spellId = 0;
    bool nitro = false;
    bool face = false;
    float facing = 0.0f;
    bool catForm = false;
    ObjectGuid target;
};

struct AnubarakKiteStand
{
    Position stand;
    ObjectGuid patch;
    AnubarakKiteBranch branch = AnubarakKiteBranch::None;
};

enum class AnubarakDragStep : uint8
{
    None,
    Walk,
    Settle
};

struct AnubarakDragLeg
{
    AnubarakDragStep step = AnubarakDragStep::None;
    Position tankSpot;
    bool preStep = false;
};

struct AnubarakBurrowerPick
{
    Creature* held = nullptr;
    bool heldTaunted = false;
    Creature* loose = nullptr;
    Position hold;
    uint8 side = 0;
    uint8 looseNear = 0;
    bool otherNear = false;
    Player* rescue = nullptr;
    float rescueFar = 0.0f;

    explicit operator bool() const { return held || loose; }
};

struct AnubarakBurrowerLead
{
    Player* tank = nullptr;
    bool walk = false;
    Position to;
};

struct AnubarakSideTankAoe
{
    char const* spell = nullptr;
    Unit* target = nullptr;
    bool point = false;
    Position pos;
};

enum class AnubarakGripStep : uint8
{
    None,
    Walk,
    Hold,
    Cast,
    Kill
};

struct AnubarakGripDuty
{
    Creature* sphere = nullptr;
    Position spot;
    uint8 side = 0;
    AnubarakGripStep step = AnubarakGripStep::None;
};

Creature* GetAnubarak(Player* bot);
bool AnubarakEngaged(PlayerbotAI* botAI);
AnubarakPhase GetAnubarakPhase(PlayerbotAI* botAI);
bool AnubarakLeechingSwarmActive(PlayerbotAI* botAI);
bool AnubarakSwarmHealing(PlayerbotAI* botAI);
bool AnubarakBurstHeld(PlayerbotAI* botAI);
bool AnubarakLustOpen(PlayerbotAI* botAI);

Creature* GetPursuingSpike(Player* bot);
Player* GetSpikeTarget(Player* bot);
bool AnubarakSpikeLaneCrosses(Player* bot, Position const& spot);
bool AnubarakSpikeReachesFirst(Player* bot, Position const& spot, bool backwards = false);

bool IsFrostSphereFlying(Unit* sphere);
bool IsFrostSphereFalling(Unit* sphere);
bool IsPermafrostPatch(Unit* sphere);
std::vector<Creature*> GetFrostSpheres(Player* bot);
bool UnitOnPermafrost(Unit* unit);

Position GetAnubarakTankSidePoint(uint8 side);
Creature* GetAnubarakTankPatch(Player* bot, uint8 side);
Position GetAnubarakTankSpot(Player* bot, uint8 side);
Position GetAnubarakSideTankHold(Player* bot, uint8 side);
bool AnubarakSideTankBacksUp(Player* bot, Position const& hold);
Position GetAnubarakBossAnchor(Player* bot);
Position GetAnubarakPickupStand(Player* bot);
Position GetAnubarakPrepullSideStand(Player* bot, uint8 side);
bool GetAnubarakDragAnchor(Player* bot, Position& out);
std::vector<Position> GetAnubarakDragPoints(Player* bot, Creature* boss);
bool GetAnubarakSubmergeSpot(Player* bot, Position& out);
std::vector<Position> GetAnubarakSubmergeDragPoints(Player* bot, Creature* boss, Position const& spot);
AnubarakDragLeg GetAnubarakDragLeg(Player* bot, Creature* boss, Position const& point, bool submerge);
bool GetAnubarakTankStepOut(Player* bot, Creature* boss, Position& out);
bool AnubarakTankPatchLatched(Player* bot);
void ClearAnubarakDrag(Player* bot);
bool IsAnubarakDragInFlight(PlayerbotAI* botAI);

bool AnubarakTauntWouldSteal(PlayerbotAI* botAI);
bool AnubarakThreatCapped(Player* bot);
bool AnubarakSideTankBossCapped(Player* bot);
bool GetAnubarakSideTankAoe(PlayerbotAI* botAI, AnubarakSideTankAoe& out);
bool AnubarakSideTankAreaAllowed(PlayerbotAI* botAI, std::string const& spell, Unit* target, bool area);
char const* GetAnubarakCleaveBuffToDrop(PlayerbotAI* botAI);
bool AnubarakBurrowerThreatCapped(Player* bot, Unit* burrower);
bool AnubarakBurrowerThreatCappedNear(Player* bot, Position const& at, float range);
bool AnubarakBurrowerAreaCapped(Player* bot, Unit* burrower);
bool AnubarakBurrowerAreaCappedNear(Player* bot, Position const& at, float range);
int8 GetAnubarakBurrowerTankSide(Player* player);
bool AnubarakSideTankNeedsDefensive(Player* bot);
bool AnubarakSideTankDefensiveKept(PlayerbotAI* botAI, uint32 spellId);
bool AnubarakSideTankSwarmLeash(PlayerbotAI* botAI);
bool AnubarakSwarmSavesHeld(PlayerbotAI* botAI);
uint32 GetAnubarakColdSelfSave(PlayerbotAI* botAI);
bool IsAnubarakColdBubble(uint32 spellId);
bool AnubarakColdBubbleUp(Player* bot);
Player* GetAnubarakBattleResTarget(PlayerbotAI* botAI);
bool AnubarakWaveImminent(PlayerbotAI* botAI);
bool IsAnubarakPickupTank(Player* bot);
bool IsAnubarakTankPlayer(Player* player);

struct AnubarakPrepullTanks
{
    ObjectGuid pickup;
    ObjectGuid side[2];
};
AnubarakPrepullTanks GetAnubarakPrepullTanks(Player* bot);

std::vector<Creature*> GetAnubarakBurrowers(Player* bot);
AnubarakBurrowerPick GetAnubarakBurrowerPick(PlayerbotAI* botAI);
bool GetAnubarakBurrowerLead(PlayerbotAI* botAI, AnubarakBurrowerLead& out);
Unit* GetAnubarakDpsTarget(PlayerbotAI* botAI);
bool AnubarakBurrowerNear(Player* bot, Position const& at, float range, bool waiting);
bool AnubarakLooseBurrowerReaches(Player* bot, Position const& at, float radius, float seconds);
bool GetAnubarakMeleeBurrowerStand(Player* bot, Creature* burrower, Position& out);
bool AnubarakMeleeBurrowerStandApplies(PlayerbotAI* botAI);
bool AnubarakMeleeNeedsBurrowerStand(PlayerbotAI* botAI);
bool AnubarakNonTankOnPermafrost(Player* bot);
bool GetAnubarakPermafrostStepOut(Player* bot, Position& out);
Position GetAnubarakPatchSafeLeg(Player* bot, Position const& to);
Creature* GetAnubarakScarabToPickUp(Player* bot);
bool GetAnubarakScarabKite(PlayerbotAI* botAI, Position& out);
struct AnubarakRedirect
{
    Player* tank = nullptr;
    Unit* dump = nullptr;
};
bool GetAnubarakRedirect(Player* bot, AnubarakRedirect& out);
inline constexpr float ANUBARAK_PENETRATING_COLD_HEAL_PCT = 90.0f;
Player* GetAnubarakPenetratingColdHealTarget(PlayerbotAI* botAI);

enum class AnubarakHealDuty : uint8
{
    None,
    Cold,
    Tank
};

struct AnubarakSwarmHealPick
{
    Player* target = nullptr;
    bool urgent = false;
    bool rescue = false;
    bool floor = false;
    bool hot = false;
};
bool CastAnubarakSwarmHot(PlayerbotAI* botAI, Player* member);
bool CastAnubarakSwarmFloor(PlayerbotAI* botAI);
AnubarakSwarmHealPick GetAnubarakSwarmHealPick(PlayerbotAI* botAI);
bool AnubarakSwarmHealWasted(Player* target);
bool CastAnubarakColdCarrierCover(PlayerbotAI* botAI, Player* member);
bool CastAnubarakColdRescue(PlayerbotAI* botAI, Player* member);
bool AnubarakAuraMasteryReserved(PlayerbotAI* botAI);
bool AnubarakAuraMasteryDue(PlayerbotAI* botAI);
bool AnubarakDivinePleaDue(PlayerbotAI* botAI);
Player* GetAnubarakInnervateTarget(PlayerbotAI* botAI);
bool AnubarakInnervateHealerDue(PlayerbotAI* botAI);
bool AnubarakSelfInnervateHeld(PlayerbotAI* botAI);
bool AnubarakSwarmShieldsTank(Player* bot, Player* target);

inline constexpr float ANUBARAK_TANK_RESCUE_PCT = 35.0f;
struct AnubarakTankRescue
{
    Player* tank = nullptr;
    char const* spell = nullptr;
};
bool GetAnubarakTankRescue(PlayerbotAI* botAI, AnubarakTankRescue& out);
char const* GetAnubarakLust(PlayerbotAI* botAI);
char const* GetAnubarakManaTopUp(PlayerbotAI* botAI);
bool AnubarakManaTopUpDue(PlayerbotAI* botAI);
Player* GetAnubarakLightJudge(Player* bot);
bool AnubarakLightJudgeDue(PlayerbotAI* botAI);
bool AnubarakSwarmLightHeld(PlayerbotAI* botAI);
bool AnubarakSwarmJudgementHeld(PlayerbotAI* botAI);
bool AnubarakHealingStreamDue(PlayerbotAI* botAI);

Creature* GetAnubarakSphereToShoot(Player* bot);
bool GetAnubarakGripDuty(Player* bot, AnubarakGripDuty& out);
void TrackAnubarakGripCast(Player* bot, Creature* sphere, uint8 side);
Creature* GetAnubarakDutySphere(Player* bot);
bool AnubarakSphereInReach(Player* bot, Unit* sphere);
bool GetAnubarakSphereStand(Player* bot, Unit* sphere, Position& out);
bool GetAnubarakKiteStand(Player* bot, AnubarakKiteStand& out);
AnubarakKiteStage GetAnubarakKiteStage(Player* kiter, AnubarakKiteBoost* boost = nullptr);
bool GetAnubarakGatherSpot(Player* bot, Position& out);
bool AnubarakGathers(PlayerbotAI* botAI);
bool GetAnubarakHealerStand(Player* bot, Position& out);
bool AnubarakHealerStandApplies(PlayerbotAI* botAI);
bool AnubarakHealerStandWaits(Player* bot);
bool GetAnubarakRangedStand(Player* bot, Position& out);
bool AnubarakRangedStandApplies(PlayerbotAI* botAI);
bool AnubarakDashRunningOrReady(Player* bot);
bool AnubarakKiteCatHeld(PlayerbotAI* botAI);
bool AnubarakKiteCatDropDue(PlayerbotAI* botAI);
bool AnubarakMainTankKeepsBear(PlayerbotAI* botAI);
bool AnubarakDemonicCircleDue(PlayerbotAI* botAI);
bool GetAnubarakCircleSpot(PlayerbotAI* botAI, Position& spot);

bool AnubarakSpikeDangerAt(Player* bot, Position const& at);
bool AnubarakInSpikeDanger(Player* bot);
bool GetAnubarakSpikeDodgeSpot(Player* bot, Position& out);
bool GetAnubarakDodgeHold(Player* bot, Position& spot);
void SetAnubarakDodgeHold(Player* bot, Position const& spot);
bool IsAnubarakDodgeHeld(PlayerbotAI* botAI);
bool AnubarakDodgeWalking(Player* bot);
bool AnubarakWalkClearOfSpike(Player* bot, Position const& to);

Player* GetAnubarakKiterToProtect(Player* bot);
bool GetAnubarakEscortSpot(Player* bot, Position& out, bool* post = nullptr, bool* patchPost = nullptr,
                           bool* standby = nullptr);
bool IsAnubarakEscort(PlayerbotAI* botAI);
bool AnubarakHandReserved(PlayerbotAI* botAI);
Player* GetAnubarakKiterToFree(Player* bot);
bool AnubarakKiterNeedsImmunity(Player* bot);
bool AnubarakKiterKeepsMark(Player* bot);
bool AnubarakKiterImmune(Unit* kiter);

Creature* GetAnubarakShadowStrikeDuty(Player* bot);
char const* AnubarakReadyInterrupt(Player* bot, Unit* target);

}

#endif
