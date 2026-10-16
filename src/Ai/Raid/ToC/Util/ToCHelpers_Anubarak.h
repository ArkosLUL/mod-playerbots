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

// On a flying Frost Sphere only, lost when it lands as a Permafrost patch
constexpr uint32 SPELL_FROST_SPHERE = 67539;
// 10N ids, remap through sSpellMgr->GetSpellIdForDifficulty at the call
constexpr uint32 SPELL_PERMAFROST = 66193;
constexpr uint32 SPELL_PENETRATING_COLD = 66013;
// Heroic burrower cast, no difficulty row
constexpr uint32 SPELL_SHADOW_STRIKE = 66134;

// Middle of the open floor, navprobe-clean out to 50 yd
extern const Position ANUBARAK_ROOM_CENTER;
// Sphere and spike sweeps reach the whole room from anywhere in it
constexpr float ANUBARAK_ROOM_RADIUS = 120.0f;
// Tank patch side points sit at ROOM_CENTER.y +/- this
constexpr float ANUBARAK_TANK_PATCH_OFFSET = 12.0f;
// Furthest a patch can be from its side point and still count as that side's tank patch
constexpr float ANUBARAK_TANK_PATCH_LATCH = 30.0f;
constexpr float ANUBARAK_TANK_PATCH_ARRIVE = 2.0f;
// 6 yd aura plus 1.5 player reach
constexpr float ANUBARAK_PERMAFROST_SLOW_REACH = 7.5f;
// The spike's 8 yd sphere test adds both sizes: 1.5 spike, 2.0 patch, 1.0 falling sphere. So 11.5 yd
// round a patch and 10.5 round a falling one.
constexpr float ANUBARAK_SPIKE_PATCH_REACH = 11.5f;
// Past the patch on the far side from the spike: it fails 11.5 yd out, long before Impale's 6 + 1.5
// can reach a kiter standing here
constexpr float ANUBARAK_KITE_STAND_OFFSET = 8.5f;
constexpr float ANUBARAK_KITE_ARRIVE = 2.0f;
constexpr float ANUBARAK_KITE_REAIM_DEGREES = 30.0f;
constexpr float ANUBARAK_KITE_RING_RADIUS = 35.0f;
constexpr float ANUBARAK_KITE_RING_STEP_DEGREES = 40.0f;
constexpr float ANUBARAK_SPIKE_DANGER_RADIUS = 10.0f;
constexpr float ANUBARAK_SPIKE_CLEARANCE = 13.0f;
constexpr float ANUBARAK_SPIKE_LANE_HALF_WIDTH = 7.0f;
constexpr float ANUBARAK_SPIKE_LANE_CLEARANCE = 9.0f;
constexpr float ANUBARAK_SPIKE_DODGE_SEARCH = 30.0f;
constexpr float ANUBARAK_INTERRUPT_RANGE = 30.0f;
// Heroic has six spheres for the whole pull: tank patches never take the last two
constexpr uint8 ANUBARAK_HEROIC_KITE_RESERVE = 2;
// He submerges 80 s after the pull or an emerge, the spike is summoned on him, and the guide drags
// him off the patches 15 s before
constexpr uint32 ANUBARAK_SUBMERGE_PREP_MS = 65000;

// Drag deadband, measured on him. In the submerge window DONE is also the start radius.
constexpr float ANUBARAK_DRAG_START = 8.0f;
constexpr float ANUBARAK_DRAG_DONE = 4.0f;
// He has stood still past his 400 ms chase recheck
constexpr uint32 ANUBARAK_DRAG_SETTLE_MS = 500;
// A leg he has not settled from by then is dropped
constexpr uint32 ANUBARAK_DRAG_LEG_MS = 6000;
// Spike reach round a patch, plus its 3.5 yd/s * 1.5 s first walk, plus the drag's done radius
constexpr float ANUBARAK_SUBMERGE_PATCH_CLEARANCE = ANUBARAK_SPIKE_PATCH_REACH + 5.25f + ANUBARAK_DRAG_DONE;
// Second search when no spot clears the first
constexpr float ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN = ANUBARAK_SPIKE_PATCH_REACH + 5.25f;

// DPS bots stop on him at this share of his tank victim's threat, resume under the release share
constexpr float ANUBARAK_THREAT_CAP = 0.9f;
constexpr float ANUBARAK_THREAT_RELEASE = 0.8f;

constexpr uint8 ANUBARAK_SHOOTERS_PER_NEED = 2;
// Patches that aren't tank patches kept ready in phase 2, falling spheres counted
constexpr uint8 ANUBARAK_P2_PATCH_STOCK = 2;
constexpr uint8 ANUBARAK_P2_PATCH_STOCK_HEROIC = 1;
// A shooter reaches a sphere within its "spell" range less this, 3D, in line of sight
constexpr float ANUBARAK_SHOOT_MARGIN = 2.0f;

constexpr float ANUBARAK_DODGE_ARRIVE = 2.0f;
// A held bot further off its spot lets go
constexpr float ANUBARAK_DODGE_HOLD_TRIGGER = 2.5f;
// Spike this far from the spot: it has passed
constexpr float ANUBARAK_DODGE_HOLD_RELEASE = 20.0f;
constexpr uint32 ANUBARAK_DODGE_WALK_MS = 3000;
constexpr uint32 ANUBARAK_DODGE_HOLD_MS = 8000;

// Spike this close: a paladin HoPs the kiter
constexpr float ANUBARAK_KITE_PROTECT_RANGE = 12.0f;
// Impale's 6 + 1.5 plus a margin: the kiter's own immunity
constexpr float ANUBARAK_KITE_IMMUNITY_RANGE = 9.0f;
constexpr float ANUBARAK_HOP_RANGE = 30.0f;
constexpr uint32 SPELL_FORBEARANCE = 25771;
// Hand of Protection ranks, Divine Shield, Ice Block
constexpr uint32 ANUBARAK_KITE_IMMUNITIES[] = {1022, 5599, 10278, 642, 45438};

// Written raw into anub.phase
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
    Detour,
    Ring,
    Hold
};

// For Detour, stand is the waypoint round the patch, not the final stand
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
};

struct AnubarakBurrowerPick
{
    // On this side tank, its current target first
    Creature* held = nullptr;
    // On someone holding no burrower side, nearest hold
    Creature* loose = nullptr;
    // This side's patch, else its side point
    Position hold;
    uint8 side = 0;

    explicit operator bool() const { return held || loose; }
};

// Through the instance's guid slot, so he still resolves while submerged and unselectable
Creature* GetAnubarak(Player* bot);
// He's attackable before the pull and the stage gate opens at the floor break, so only his combat counts
bool AnubarakEngaged(PlayerbotAI* botAI);
// None while not engaged. A read 5 s after the last one starts a new pull, which clears every latch.
AnubarakPhase GetAnubarakPhase(PlayerbotAI* botAI);
bool AnubarakSubmerged(PlayerbotAI* botAI);
bool AnubarakLeechingSwarmActive(PlayerbotAI* botAI);

// Unselectable, so only a grid search finds it
Creature* GetPursuingSpike(Player* bot);
Player* GetSpikeTarget(Player* bot);
// Spot sits within ANUBARAK_SPIKE_LANE_CLEARANCE of the spike's line to its target
bool AnubarakSpikeLaneCrosses(Player* bot, Position const& spot);

bool IsFrostSphereFlying(Unit* sphere);
// Killed but still dropping, turns into a patch 1.5 s after the kill
bool IsFrostSphereFalling(Unit* sphere);
// A patch never carries Permafrost itself, its own area aura skips the caster
bool IsPermafrostPatch(Unit* sphere);
// Every alive sphere: flying, falling and patches
std::vector<Creature*> GetFrostSpheres(Player* bot);
bool UnitOnPermafrost(Unit* unit);

// Side 0 sits at ROOM_CENTER.y + offset, side 1 at y - offset
Position GetAnubarakTankSidePoint(uint8 side);
// Null until a patch lies within the latch distance of the side point
Creature* GetAnubarakTankPatch(Player* bot, uint8 side);
// Midpoint of both tank patches, else the room centre
Position GetAnubarakBossAnchor(Player* bot);
// Midpoint of both latched tank patches. False until both have landed: he is held where picked up.
bool GetAnubarakDragAnchor(Player* bot, Position& out);
// From 65 s after the pull or an emerge: a spot clear of every patch by
// ANUBARAK_SUBMERGE_PATCH_CLEARANCE, else by _MIN, so the spike summoned on him doesn't burn one.
// False otherwise.
bool GetAnubarakSubmergeSpot(Player* bot, Position& out);
// The pickup tank's leg toward point, latched per instance. Called by the hold action only.
AnubarakDragLeg GetAnubarakDragLeg(Player* bot, Creature* boss, Position const& point, bool submerge);
void ClearAnubarakDrag(Player* bot);
// A latched leg is walking or waiting for him to settle: the Block rule's predicate, a pure read
bool IsAnubarakDragInFlight(PlayerbotAI* botAI);

// Current target is him and his victim is a tank other than this bot
bool AnubarakTauntWouldSteal(PlayerbotAI* botAI);
// Latched per bot: capped at ANUBARAK_THREAT_CAP of his tank victim's threat, freed under RELEASE
bool AnubarakThreatCapped(Player* bot);
// 0 or 1 for the two side tanks latched for the pull, else -1
int8 GetAnubarakBurrowerTankSide(Player* player);
// Main tank while alive, else side 0's tank
bool IsAnubarakPickupTank(Player* bot);
// IsTank can read false for a tank for a tick, so the spec is asked too
bool IsAnubarakTankPlayer(Player* player);

// Alive burrowers in the room, submerged ones too: they come back up where they went down
std::vector<Creature*> GetAnubarakBurrowers(Player* bot);
// What a side tank takes. Empty for a bot with no side, and with nothing selectable to take.
AnubarakBurrowerPick GetAnubarakBurrowerPick(PlayerbotAI* botAI);
// The burrower DPS focus, latched per instance in anub.focus. Only ones standing on Permafrost count,
// since one below 80% off it submerges and comes back full. The latched one stays while it qualifies.
Creature* GetAnubarakFocusBurrower(Player* bot);
// This DPS bot's target: a player's skull, the focus burrower, him while selectable, else while he's
// down the nearest scarab in reach. Null outside the pull, for non-DPS and for a sphere shooter.
Unit* GetAnubarakDpsTarget(PlayerbotAI* botAI);
// Nearest scarab within 30 yd hitting someone who isn't a tank
Creature* GetAnubarakScarabToPickUp(Player* bot);
// Lowest health Penetrating Cold carrier under 90% this healer can reach
Player* GetAnubarakPenetratingColdHealTarget(PlayerbotAI* botAI);

// The flying sphere this bot shoots now, null when it has no sphere duty
Creature* GetAnubarakSphereToShoot(Player* bot);
// 3D within the bot's spell range less ANUBARAK_SHOOT_MARGIN, in line of sight
bool AnubarakSphereInReach(Player* bot, Unit* sphere);
// Floor point toward the sphere where it is in reach, off the spike and its lane. False when none.
bool GetAnubarakSphereStand(Player* bot, Unit* sphere, Position& out);
// Where the spike's target runs. previous is the stand the caller latched last time, may be null.
// False while there's no spike.
bool GetAnubarakKiteStand(Player* bot, AnubarakKiteStand const* previous, AnubarakKiteStand& out);

bool AnubarakInSpikeDanger(Player* bot);
// Heroic keeps out of Permafrost's slow reach when any spot allows it
bool GetAnubarakSpikeDodgeSpot(Player* bot, Position& out);
// The bot's dodge spot while the hold is valid: walking (set within ANUBARAK_DODGE_WALK_MS) or
// parked within ANUBARAK_DODGE_HOLD_TRIGGER. Pure read; an invalid hold reads false.
bool GetAnubarakDodgeHold(Player* bot, Position& spot);
void SetAnubarakDodgeHold(Player* bot, Position const& spot);
bool IsAnubarakDodgeHeld(PlayerbotAI* botAI);

// The kiter this paladin bot covers with Hand of Protection now, null when it has no such duty
Player* GetAnubarakKiterToProtect(Player* bot);
// The marked bot itself must cast an immunity now: no protector, spike within the immunity range
bool AnubarakKiterNeedsImmunity(Player* bot);
bool AnubarakKiterImmune(Unit* kiter);

// The burrower whose Shadow Strike this bot interrupts, null when it has none
Creature* GetAnubarakShadowStrikeDuty(Player* bot);
// Name of the first interrupt or stun the bot can cast on target now, null when none.
// Burrowers are silence-immune, so no silence-only spells here.
char const* AnubarakReadyInterrupt(Player* bot, Unit* target);

}

#endif
