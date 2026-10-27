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
// On the burrower's victim, +25% physical damage taken a stack. 10N id, 25-man pulls carry 67847: remap
// it at the call
constexpr uint32 SPELL_EXPOSE_WEAKNESS = 67721;
constexpr uint32 SPELL_DEATH_GRIP = 49576;

// Middle of the open floor, navprobe-clean out to 50 yd
extern const Position ANUBARAK_ROOM_CENTER;
// Sphere and spike sweeps reach the whole room from anywhere in it
constexpr float ANUBARAK_ROOM_RADIUS = 120.0f;
// Tank spots sit at ROOM_CENTER.y +/- this, 24 yd apart: burrowers held 4 yd inside each patch toward him
// stay 16 yd apart, past Spider Frenzy's 12, and slot 3's sphere starts in grip reach of the north spot
constexpr float ANUBARAK_TANK_PATCH_OFFSET = 12.0f;
// Furthest a patch can be from its side point and still count as that side's tank patch
constexpr float ANUBARAK_TANK_PATCH_LATCH = 30.0f;
constexpr float ANUBARAK_TANK_PATCH_ARRIVE = 2.0f;
// 6 yd aura plus 1.5 player reach
constexpr float ANUBARAK_PERMAFROST_SLOW_REACH = 7.5f;
// The main tank keeps this far off every patch, so a step or a drag never ends in the slow
constexpr float ANUBARAK_TANK_KEEPOUT = ANUBARAK_PERMAFROST_SLOW_REACH + 1.0f;
// Pre-pull pickup stand, west of the room centre: he runs in from the east and stops on the anchor
constexpr float ANUBARAK_PICKUP_STAND_OFFSET = 10.0f;

// Shooting a tank patch: a sphere this close to the spot goes at once, the nearest within FAR after the wait
constexpr float ANUBARAK_SIDE_SHOOT_NEAR = 12.0f;
constexpr float ANUBARAK_SIDE_SHOOT_FAR = 20.0f;
constexpr uint32 ANUBARAK_SIDE_SHOOT_WAIT_MS = 10000;
// A sphere this close to the other tank patch would put both sides' burrowers, 4 yd in toward him, in
// one Spider Frenzy
constexpr float ANUBARAK_TANK_PATCH_GAP = 20.0f;

// Death Grip's 30 yd less a margin, 3D: a flying sphere sits about 13.5 yd up
constexpr float ANUBARAK_GRIP_RANGE = 28.0f;
// A gripped sphere comes down where the DK cast from. One not landed 3 s after the cast failed, and its
// side shoots until a Death Grip is back.
constexpr float ANUBARAK_GRIP_LANDED = 3.0f;
constexpr float ANUBARAK_GRIP_LANDED_HEIGHT = 4.0f;
constexpr uint32 ANUBARAK_GRIP_JUDGE_MS = 3000;
// A DK whose grip is back within this still grips; any later and the side shoots instead
constexpr uint32 ANUBARAK_GRIP_COOLDOWN_WAIT_MS = 3000;
// Only one sphere flies near the tank spots, so with none in reach the DK grips from up to this far off
// its spot, toward the nearest. The landing still counts within ANUBARAK_SIDE_SHOOT_FAR.
constexpr float ANUBARAK_GRIP_STEP_OUT = 16.0f;
// The stepped-out stand puts the sphere this far inside reach, since it keeps drifting
constexpr float ANUBARAK_GRIP_REACH_MARGIN = 3.0f;

// With its patch latched a side tank holds this far from its centre toward him, once every burrower on
// it carries Permafrost. Burrowers stop about 3 yd short, 3.5-5.7 yd in from every exit.
constexpr float ANUBARAK_SIDE_TANK_RIM = 7.0f;
// A melee bot hits a burrower from within this of it, at least this far from every patch centre: out of
// the 7.5 yd slow
constexpr float ANUBARAK_MELEE_BURROWER_REACH = 4.5f;
constexpr float ANUBARAK_MELEE_PATCH_CLEARANCE = 8.0f;

// Burrowers a side tank holds before the next one goes to the other side (Spider Frenzy)
constexpr uint8 ANUBARAK_BURROWERS_PER_SIDE = 2;
// Burrower waves: 5-8 s after the pull or an emerge, then every 45 s, none while he is down
constexpr uint32 ANUBARAK_WAVE_FIRST_MS = 5000;
constexpr uint32 ANUBARAK_WAVE_INTERVAL_MS = 45000;
// New burrowers further apart than this belong to separate waves
constexpr uint32 ANUBARAK_WAVE_GAP_MS = 10000;
// Side tanks leave scarabs alone this long before a wave
constexpr uint32 ANUBARAK_WAVE_READY_MS = 8000;
// He submerges 80 s after the pull or an emerge and is selectable again 62 s later
constexpr uint32 ANUBARAK_SURFACED_MS = 80000;
constexpr uint32 ANUBARAK_SUBMERGED_MS = 62000;
// One this low off Permafrost still dies before its next 3 s submerge check
constexpr float ANUBARAK_FOCUS_OFF_FROST_PCT = 10.0f;
// A burrower is a focus pick once a tank it is on has stood this close this long: before, it is still
// walking in from its exit and the raid would chase it there, or pull it off the tank
constexpr float ANUBARAK_FOCUS_SETTLE = 5.0f;
constexpr uint32 ANUBARAK_FOCUS_SETTLE_MS = 4000;
// The script's first submerge check comes 30 s after a burrower spawns or emerges, so one off Permafrost
// is still worth killing this long after either
constexpr uint32 ANUBARAK_BURROWER_FREE_MS = 25000;

constexpr uint32 ANUBARAK_SIDE_DEFENSIVE_STACKS = 5;
constexpr float ANUBARAK_SIDE_DEFENSIVE_PCT = 50.0f;
// Rebirth's 30 yd less a margin
constexpr float ANUBARAK_BATTLE_RES_REACH = 28.0f;
// The spike's 8 yd sphere test adds both sizes: 1.5 spike, 2.0 patch, 1.0 falling sphere. So 11.5 yd
// round a patch and 10.5 round a falling one.
constexpr float ANUBARAK_SPIKE_PATCH_REACH = 11.5f;
// Past the patch on the far side from the spike: it fails 11.5 yd out, long before Impale's 6 + 1.5
// can reach a kiter standing here
constexpr float ANUBARAK_KITE_STAND_OFFSET = 8.5f;
constexpr float ANUBARAK_KITE_ARRIVE = 2.0f;
constexpr float ANUBARAK_KITE_REAIM_DEGREES = 30.0f;
// navprobe-clean all round; heroic's kite paths ring it or the 45 yd rim
constexpr float ANUBARAK_KITE_RING_RADIUS = 40.0f;
// Heroic: the spike this close and faster than the kiter sends the next speed boost out, and at CLOSE
// whatever its speed
constexpr float ANUBARAK_KITE_BOOST_RANGE = 15.0f;
constexpr float ANUBARAK_KITE_BOOST_CLOSE = 10.0f;
// Heroic: the spike's route keeps this far off a patch it isn't meant to take, its 11.5 yd test plus 3
constexpr float ANUBARAK_SPIKE_ROUTE_CLEARANCE = 14.5f;
// Impale's 6 yd plus 1.5 reach
constexpr float ANUBARAK_IMPALE_REACH = 7.5f;
// Heroic, a patch the kite may end on and the spike faster: lane steps keep the kiter this close to its stand
constexpr float ANUBARAK_KITE_STAND_LEASH = 25.0f;
// The teleport back to the Demonic Circle only goes when the circle is this much farther from the spike
constexpr float ANUBARAK_CIRCLE_TELEPORT_GAIN = 15.0f;
// A circle with less left than this is cast again
constexpr int32 ANUBARAK_CIRCLE_REFRESH_MS = 100000;
constexpr float ANUBARAK_BLINK_DISTANCE = 20.0f;
// Heroic phase 2: slots this far round the raid point, held from up to ARRIVE off them. The healer stand
// uses the same ring while he is up, and walks back once a healer is LEAVE off its slot.
constexpr float ANUBARAK_GATHER_ARRIVE = 6.0f;
constexpr float ANUBARAK_GATHER_SPREAD = 3.0f;
constexpr float ANUBARAK_HEALER_STAND_LEAVE = 6.0f;
// A gathered melee bot takes a scarab this close before the focus burrower
constexpr float ANUBARAK_GATHER_MELEE_SCARAB = 10.0f;
constexpr float ANUBARAK_KITE_RING_STEP_DEGREES = 40.0f;
constexpr float ANUBARAK_SPIKE_DANGER_RADIUS = 10.0f;
constexpr float ANUBARAK_SPIKE_CLEARANCE = 13.0f;
constexpr float ANUBARAK_SPIKE_LANE_HALF_WIDTH = 7.0f;
constexpr float ANUBARAK_SPIKE_LANE_CLEARANCE = 9.0f;
// A spike faster than the bot: 10 yd is under a second at 11.4 yd/s
constexpr float ANUBARAK_SPIKE_DANGER_RADIUS_FAST = 16.0f;
constexpr float ANUBARAK_SPIKE_CLEARANCE_FAST = 19.0f;
constexpr float ANUBARAK_SPIKE_LANE_HALF_WIDTH_FAST = 10.0f;
constexpr float ANUBARAK_SPIKE_LANE_CLEARANCE_FAST = 12.0f;
constexpr float ANUBARAK_SPIKE_DODGE_SEARCH = 30.0f;
constexpr float ANUBARAK_INTERRUPT_RANGE = 30.0f;
// He submerges 80 s after the pull or an emerge and the spike is summoned on him, so he is dragged off
// the patches 15 s before
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
// Last search when no spot clears the others
constexpr float ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN = ANUBARAK_SPIKE_PATCH_REACH + 5.25f;

// DPS bots stop on him at this share of his tank victim's threat, resume under the release share
constexpr float ANUBARAK_THREAT_CAP = 0.9f;
constexpr float ANUBARAK_THREAT_RELEASE = 0.8f;

constexpr uint8 ANUBARAK_SHOOTERS_PER_NEED = 2;
// Patches that aren't tank patches kept ready in phase 2, falling spheres counted. Heroic has six
// spheres for the pull, so its kite spends one tank patch a submerge instead.
constexpr uint8 ANUBARAK_P2_PATCH_STOCK = 2;
constexpr uint8 ANUBARAK_P2_PATCH_STOCK_HEROIC = 0;
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
// The escort walks back to its spot once further off it
constexpr float ANUBARAK_ESCORT_HOLD = 4.0f;
// Impale's 6 + 1.5 plus a margin: the kiter's own immunity
constexpr float ANUBARAK_KITE_IMMUNITY_RANGE = 9.0f;
constexpr float ANUBARAK_HOP_RANGE = 30.0f;
constexpr uint32 SPELL_FORBEARANCE = 25771;
// The kiter's own Hand of Protection keeps the spike off it within this much of the emerge
constexpr uint32 ANUBARAK_KITE_HOP_EMERGE_MS = 10000;
// Hand of Protection ranks, Divine Shield, Ice Block. Only the hand keeps the mark; the others, Feign
// Death and Vanish hand the spike on at its current speed.
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

// Heroic only, written raw into anub.kitestage with the boost's spell
enum class AnubarakKiteStage : uint8
{
    None,
    // Steps whose spike lane clears every patch
    Run,
    // Running, and the next speed boost goes out
    Boost,
    // Hand of Protection up, still running
    Immune,
    // A patch kite whose walk to its stand would no longer beat the catch by the commit margin: to the patch
    Patch
};

struct AnubarakKiteBoost
{
    uint32 spellId = 0;
    // Nitro Boosts is the boots' use, no spell to cast
    bool nitro = false;
    // Blink goes the way the bot faces
    bool face = false;
    float facing = 0.0f;
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
    // The walk is to a pre-step round him, the leg's spot after it
    bool preStep = false;
};

struct AnubarakBurrowerPick
{
    // On this side tank, its current target first
    Creature* held = nullptr;
    // This side's, on someone else: one hitting a healer first, then the nearest the hold
    Creature* loose = nullptr;
    // GetAnubarakSideTankHold
    Position hold;
    uint8 side = 0;
    // Loose ones within 10 yd of the bot, for a challenging shout
    uint8 looseNear = 0;
    // One of the other side's within 10 yd, which a shout would take too
    bool otherNear = false;
    // The group member most of this side's loose ones hit, for Righteous Defense
    Player* rescue = nullptr;

    explicit operator bool() const { return held || loose; }
};

enum class AnubarakGripStep : uint8
{
    None,
    // To its stand
    Walk,
    // On its stand with nothing in reach, the gripped sphere on its way, or a relay link's grip still to come
    Hold,
    Cast,
    // The gripped sphere landed on the DK
    Kill
};

struct AnubarakGripDuty
{
    // Null while no sphere is in reach of the stand
    Creature* sphere = nullptr;
    // The stand: the side's spot, up to ANUBARAK_GRIP_STEP_OUT off it toward the sphere, with every
    // sphere further out just inside Death Grip's reach of the nearest, or a relay link's post
    Position spot;
    uint8 side = 0;
    AnubarakGripStep step = AnubarakGripStep::None;
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
// The spike could run to spot within the bot's walk there plus 2 s
bool AnubarakSpikeReachesFirst(Player* bot, Position const& spot);

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
// Null until a patch lies within the latch distance of the side point, nearer it than the other side's
Creature* GetAnubarakTankPatch(Player* bot, uint8 side);
// The side's patch, else its side point moved with the other side's patch so the pair keeps its spacing
Position GetAnubarakTankSpot(Player* bot, uint8 side);
// Where the side tank stands: ANUBARAK_SIDE_TANK_RIM from its patch toward him once every burrower on it
// carries Permafrost, the patch centre while one doesn't, so it trails in; no patch, the spot
Position GetAnubarakSideTankHold(Player* bot, uint8 side);
// Midpoint of both tank patches, else the room centre
Position GetAnubarakBossAnchor(Player* bot);
// Pre-pull stand of the pickup tank, west of the anchor and off both tank spots
Position GetAnubarakPickupStand(Player* bot);
// Midpoint of both latched tank patches. False until both have landed: he is held where picked up.
bool GetAnubarakDragAnchor(Player* bot, Position& out);
// The anchor, then points 4 and 7.5 yd off it toward his side of the patch line, for when the leg to
// the anchor would walk the tank through Permafrost; then waypoints round a patch on his line to the
// anchor, and once he has stood 8+ yd off it 5 s with no leg, a ring 8 yd round it. Empty without an
// anchor. Called by the hold action only: it times the stall.
std::vector<Position> GetAnubarakDragPoints(Player* bot, Creature* boss);
// From 65 s after the pull or an emerge: a spot 30 yd from every patch, else 25, else
// ANUBARAK_SUBMERGE_PATCH_CLEARANCE, else _MIN, so the spike summoned on him doesn't burn one, on the
// tank's side away from the tank patches. False otherwise.
bool GetAnubarakSubmergeSpot(Player* bot, Position& out);
// The pickup tank's leg toward point, latched per instance. Called by the hold action only. No leg
// whose tank spot or walk comes within the slow reach of a patch.
AnubarakDragLeg GetAnubarakDragLeg(Player* bot, Creature* boss, Position const& point, bool submerge);
// The main tank stands in a patch's slow reach: the nearest spot clear of every patch by
// ANUBARAK_TANK_KEEPOUT, within his melee reach when there is one. False when it is clear already.
bool GetAnubarakTankStepOut(Player* bot, Creature* boss, Position& out);
bool AnubarakTankPatchLatched(Player* bot);
void ClearAnubarakDrag(Player* bot);
// A latched leg is walking or waiting for him to settle: the Block rule's predicate, a pure read
bool IsAnubarakDragInFlight(PlayerbotAI* botAI);

// Current target is him and his victim is a tank other than this bot
bool AnubarakTauntWouldSteal(PlayerbotAI* botAI);
// Latched per bot: capped at ANUBARAK_THREAT_CAP of his tank victim's threat, freed under RELEASE
bool AnubarakThreatCapped(Player* bot);
// The same per DPS bot and burrower, on a burrower a tank holds
bool AnubarakBurrowerThreatCapped(Player* bot, Unit* burrower);
// A selectable burrower within range of at that the bot is capped on
bool AnubarakBurrowerThreatCappedNear(Player* bot, Position const& at, float range);
// 0 or 1 for the two side tanks latched for the pull, else -1. Bots go first; a human takes a side no
// bot can, and his side is the tank spot he stands at.
int8 GetAnubarakBurrowerTankSide(Player* player);
// Expose Weakness at ANUBARAK_SIDE_DEFENSIVE_STACKS or health under the pct, on a side tank
bool AnubarakSideTankNeedsDefensive(Player* bot);
// A dead side tank this druid can Rebirth now, else null
Player* GetAnubarakSideTankToRes(PlayerbotAI* botAI);
// A burrower wave is due within ANUBARAK_WAVE_READY_MS while he is up
bool AnubarakWaveImminent(PlayerbotAI* botAI);
// Main tank while alive, else the side tank he's on, else the first bot side tank
bool IsAnubarakPickupTank(Player* bot);
// IsTank can read false for a tank for a tick, so the spec is asked too
bool IsAnubarakTankPlayer(Player* player);

// Who the pull's latches would pick if it started now, read fresh with nothing latched. pickup is
// the main tank while alive, else the first bot side tank, which then leaves its burrowers to the other.
struct AnubarakPrepullTanks
{
    ObjectGuid pickup;
    ObjectGuid side[2];
};
AnubarakPrepullTanks GetAnubarakPrepullTanks(Player* bot);

// Alive burrowers in the room, submerged ones too: they come back up where they went down
std::vector<Creature*> GetAnubarakBurrowers(Player* bot);
// What a side tank takes. Empty for a bot with no side, and with nothing selectable to take.
// Each burrower belongs to the side of its burrow exit, at most two a side; one on a side tank stays
// with it, and a dead or marked side tank's go to the other, as do all of one holding him. A human side
// tank's still loose 5 s after it came out goes to a bot side tank, past two.
AnubarakBurrowerPick GetAnubarakBurrowerPick(PlayerbotAI* botAI);
// The burrower DPS focus of the bot's side (anub.dside: living DPS bots per role by guid, the sides in
// turn, split again at each wave), else either side's; each side's latched in anub.focus0 and
// anub.focus1 over the burrowers that side holds. Any under ANUBARAK_FOCUS_OFF_FROST_PCT counts; else
// only one a tank stands within ANUBARAK_FOCUS_SETTLE of, or whose side has no usable side tank, and then
// on Permafrost, since one below 80% off it submerges and comes back full, or off it within
// ANUBARAK_BURROWER_FREE_MS of coming up. Those off it go first, the one whose time ends first, then the
// lowest health. The latched one stays while it qualifies and none of those outranks it.
Creature* GetAnubarakFocusBurrower(Player* bot);
// This DPS bot's target: a player's skull; gathered melee in heroic phase 2, a scarab within 10 yd; a
// melee bot, a burrower within 10 yd on a non-tank; its side's focus (a melee bot's only with a stand off
// the patches in its reach); once he's up, a leftover scarab (melee within 10 yd); a ranged bot, the
// other side's focus; him while selectable; while he's down, the nearest scarab in reach. Null outside
// the pull, for non-DPS and for a bot on sphere duty.
Unit* GetAnubarakDpsTarget(PlayerbotAI* botAI);
// A living, selectable burrower within range of at. waiting: only one no tank has held for the settle
// yet, above the off-Permafrost kill line.
bool AnubarakBurrowerNear(Player* bot, Position const& at, float range, bool waiting);
// A floor point within ANUBARAK_MELEE_BURROWER_REACH of the burrower and ANUBARAK_MELEE_PATCH_CLEARANCE
// from every patch: in his melee reach behind him first, then in his reach, nearest the bot; else the
// one nearest him. Held per bot while the burrower stays put. False when none is, or no patch is near
// enough to matter.
bool GetAnubarakMeleeBurrowerStand(Player* bot, Creature* burrower, Position& out);
// A melee DPS bot's current target is a burrower near a patch with a stand off it, so its own walk
// replaces reach melee, set behind and flank: behind a held burrower is its patch
bool AnubarakMeleeBurrowerStandApplies(PlayerbotAI* botAI);
// That, and the bot is out of melee reach or within ANUBARAK_MELEE_PATCH_CLEARANCE of a patch
bool AnubarakMeleeNeedsBurrowerStand(PlayerbotAI* botAI);
// A non-tank, unmarked, off grip duty (killing its gripped sphere aside), within Permafrost's slow reach
bool AnubarakNonTankOnPermafrost(Player* bot);
// For such a bot: the nearest floor point ANUBARAK_TANK_KEEPOUT clear of every patch, within reach of its
// current target and off the spike when one is. False otherwise.
bool GetAnubarakPermafrostStepOut(Player* bot, Position& out);
// The first leg toward to: a corner round a patch whose slow reach the straight walk would cut, else to
Position GetAnubarakPatchSafeLeg(Player* bot, Position const& to);
// Nearest scarab within 30 yd hitting someone who isn't a tank. Heroic spreads Acid-Drenched Mandibles:
// in phase 2 the main tank takes only one already on it, and a side tank only one on a healer.
Creature* GetAnubarakScarabToPickUp(Player* bot);
// What a hunter or rogue bot's Misdirection or Tricks of the Trade does now
struct AnubarakRedirect
{
    Player* tank = nullptr;
    // Where a hunter spends the redirected shots
    Unit* dump = nullptr;
};
// The pull's first 10 s: the first hunter and the first rogue bot, to the main tank. Otherwise to the bot
// side tank owning a selectable burrower that isn't on it: one hunter a burrower by guid, and a rogue
// only for the one it's hitting, within Tricks' 20 yd. False with no such duty.
bool GetAnubarakRedirect(Player* bot, AnubarakRedirect& out);
// Lowest health Penetrating Cold carrier under 90% this healer can reach
Player* GetAnubarakPenetratingColdHealTarget(PlayerbotAI* botAI);

// The flying sphere this bot shoots now, null when it has no sphere duty
Creature* GetAnubarakSphereToShoot(Player* bot);
// The DK's grip of a sphere for its side's tank patch. False without one.
bool GetAnubarakGripDuty(Player* bot, AnubarakGripDuty& out);
// Called once the DK's Death Grip on its duty sphere went out, with the side its duty had before the cast
void NoteAnubarakGripCast(Player* bot, Creature* sphere, uint8 side);
// The shot or gripped sphere this bot's duty is on, else null
Creature* GetAnubarakDutySphere(Player* bot);
// 3D within the bot's spell range less ANUBARAK_SHOOT_MARGIN, in line of sight
bool AnubarakSphereInReach(Player* bot, Unit* sphere);
// Floor point toward the sphere where it is in reach, off the spike and its lane. False when none.
bool GetAnubarakSphereStand(Player* bot, Unit* sphere, Position& out);
// Where the spike's target runs. previous is the stand the caller latched last time, may be null.
// False while there's no spike. Heroic runs lane steps until the kite stage says patch.
bool GetAnubarakKiteStand(Player* bot, AnubarakKiteStand const* previous, AnubarakKiteStand& out);
// Heroic only, None elsewhere and for an unmarked bot. boost is set to the speed boost to use now, if any,
// whatever the stage.
AnubarakKiteStage GetAnubarakKiteStage(Player* kiter, AnubarakKiteBoost* boost = nullptr);
// Heroic phase 2: this bot's spot in the raid's gather, not floor-checked, false when it doesn't gather
// (the kiter, a side tank a burrower is on, the escort). The main tank's is on the gather's edge.
bool GetAnubarakGatherSpot(Player* bot, Position& out);
bool AnubarakGathers(PlayerbotAI* botAI);
// While he is up, every difficulty: a healer bot's slot on the gather's ring round the raid point, clear
// of every patch, not floor-checked. With the tank patches 40+ yd apart the healers split in two groups
// by guid, each ring 17.5 yd off its side's patch toward the raid point, when that keeps it clear and in
// reach of the main tank. False for anyone else or while marked.
bool GetAnubarakHealerStand(Player* bot, Position& out);
bool AnubarakHealerStandApplies(PlayerbotAI* botAI);
// A warlock bot's Demonic Circle is missing or under 100 s: on its Anub'arak stand before the pull, or
// while he is up
bool AnubarakDemonicCircleDue(PlayerbotAI* botAI);

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
// Heroic: the spot of the paladin bot whose Hand of Protection the kite plan ends or bridges the kite
// on: shadowing the kiter, then from 4 s before the catch a post by it (post set). False for every other
// bot.
bool GetAnubarakEscortSpot(Player* bot, Position& out, bool* post = nullptr);
bool IsAnubarakEscort(PlayerbotAI* botAI);
// Heroic: this paladin bot is the one planned or claimed for the kiter's Hand, within 2.5 s of the catch
bool AnubarakHandReserved(PlayerbotAI* botAI);
// The kiter, marked while it carries Permafrost, this paladin bot casts Hand of Freedom on now
Player* GetAnubarakKiterToFree(Player* bot);
// The marked bot itself must cast an immunity now. Heroic: its planned own Hand 1 s before the catch,
// its planned immunity 0.6 s before, or that once the plan failed (no protector, no patch walk that
// beats the catch); a non-paladin's own behind a planned Hand with 0.8 s of the Hand left. Normal: the
// spike within the immunity range, no protector, no reachable patch stand.
bool AnubarakKiterNeedsImmunity(Player* bot);
// The marked paladin's own Hand of Protection keeps it on the spike to a patch or the emerge, or the
// kite plan ends on it; otherwise, and always for a plan ending on its own immunity, Divine Shield hands
// the spike on
bool AnubarakKiterKeepsMark(Player* bot);
bool AnubarakKiterImmune(Unit* kiter);

// The burrower whose Shadow Strike this bot interrupts, null when it has none
Creature* GetAnubarakShadowStrikeDuty(Player* bot);
// Name of the first interrupt or stun the bot can cast on target now, null when none.
// Burrowers are silence-immune, so no silence-only spells here.
char const* AnubarakReadyInterrupt(Player* bot, Unit* target);

}

#endif
