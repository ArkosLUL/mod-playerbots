#ifndef PLAYERBOTS_RAID_TOCHELPERS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCHELPERS_ANUBARAK_H

#include <string>
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
// How close the boss hold leads him to its point, measured on him
constexpr float ANUBARAK_DRAG_ARRIVE = 2.0f;
// He submerges 80 s after the pull or an emerge, the spike is summoned on him, and the guide drags
// him off the patches 15 s before
constexpr uint32 ANUBARAK_SUBMERGE_PREP_MS = 65000;
// Spike reach round a patch, plus the 3.5 yd/s * 1.5 s it walks before its first test, plus the drag's
// arrive
constexpr float ANUBARAK_SUBMERGE_PATCH_CLEARANCE = ANUBARAK_SPIKE_PATCH_REACH + 5.25f + ANUBARAK_DRAG_ARRIVE;

// DPS focus mark. Side tanks mark their own pick under a side name with no group icon.
constexpr char const* ANUBARAK_BURROWER_RTI = "cross";
constexpr char const* ANUBARAK_SIDE_RTI[2] = {"square", "triangle"};

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
// From 65 s after the pull or an emerge: a spot clear of every patch by
// ANUBARAK_SUBMERGE_PATCH_CLEARANCE, so the spike summoned on him doesn't burn one. False otherwise.
bool GetAnubarakSubmergeSpot(Player* bot, Position& out);
// 0 or 1 for the two side tanks latched for the pull, else -1
int8 GetAnubarakBurrowerTankSide(Player* player);
// Main tank while alive, else side 0's tank
bool IsAnubarakPickupTank(Player* bot);
// IsTank can read false for a tank for a tick, so the spec is asked too
bool IsAnubarakTankPlayer(Player* player);
bool IsAnubarakSideRti(std::string const& rti);

// Alive burrowers in the room, submerged ones too: they come back up where they went down
std::vector<Creature*> GetAnubarakBurrowers(Player* bot);
// What a side tank takes. Empty for a bot with no side, and with nothing selectable to take.
AnubarakBurrowerPick GetAnubarakBurrowerPick(PlayerbotAI* botAI);
// The burrower DPS focus. Only ones standing on Permafrost count, since one below 80% off it
// submerges and comes back full. The one the cross already marks stays the pick while it qualifies.
Creature* GetAnubarakFocusBurrower(Player* bot);
// Nearest scarab within 30 yd hitting someone who isn't a tank
Creature* GetAnubarakScarabToPickUp(Player* bot);
// Lowest health Penetrating Cold carrier under 90% this healer can reach
Player* GetAnubarakPenetratingColdHealTarget(PlayerbotAI* botAI);

// The flying sphere this bot shoots now, null when it has no sphere duty
Creature* GetAnubarakSphereToShoot(Player* bot);
// Where the spike's target runs. previous is the stand the caller latched last time, may be null.
// False while there's no spike.
bool GetAnubarakKiteStand(Player* bot, AnubarakKiteStand const* previous, AnubarakKiteStand& out);

bool AnubarakInSpikeDanger(Player* bot);
bool GetAnubarakSpikeDodgeSpot(Player* bot, Position& out);

// The burrower whose Shadow Strike this bot interrupts, null when it has none
Creature* GetAnubarakShadowStrikeDuty(Player* bot);
// Name of the first interrupt or stun the bot can cast on target now, null when none.
// Burrowers are silence-immune, so no silence-only spells here.
char const* AnubarakReadyInterrupt(Player* bot, Unit* target);

}

#endif
