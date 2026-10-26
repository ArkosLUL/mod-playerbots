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
// On the burrower's victim, +25% physical damage taken a stack
constexpr uint32 SPELL_EXPOSE_WEAKNESS = 67721;
constexpr uint32 SPELL_DEATH_GRIP = 49576;

// Middle of the open floor, navprobe-clean out to 50 yd
extern const Position ANUBARAK_ROOM_CENTER;
// Sphere and spike sweeps reach the whole room from anywhere in it
constexpr float ANUBARAK_ROOM_RADIUS = 120.0f;
// Tank spots sit at ROOM_CENTER.y +/- this, so the two patches end up 14 yd apart, past Spider Frenzy's 12
constexpr float ANUBARAK_TANK_PATCH_OFFSET = 7.0f;
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
// A sphere this close to the other tank patch would put both sides' burrowers in one Spider Frenzy
constexpr float ANUBARAK_TANK_PATCH_GAP = 13.0f;

// Death Grip's 30 yd less a margin, 3D: a flying sphere sits about 13.5 yd up
constexpr float ANUBARAK_GRIP_RANGE = 28.0f;
// A gripped sphere comes down where the DK cast from. One not landed 3 s after the cast failed, and its
// side shoots until a Death Grip is back.
constexpr float ANUBARAK_GRIP_LANDED = 3.0f;
constexpr float ANUBARAK_GRIP_LANDED_HEIGHT = 4.0f;
constexpr uint32 ANUBARAK_GRIP_JUDGE_MS = 3000;
// AnubarakGripDuty::side of heroic's kite patch grip
constexpr uint8 ANUBARAK_KITE_GRIP = 2;
// A DK whose grip is back within this still grips; any later and the side shoots instead
constexpr uint32 ANUBARAK_GRIP_COOLDOWN_WAIT_MS = 3000;
// Only one sphere flies near the tank spots, so with none in reach the DK grips from up to this far off
// its spot, toward the nearest
constexpr float ANUBARAK_GRIP_STEP_OUT = 8.0f;
// A ready DK on its spot with no sphere in reach this long hands the side to the shooters
constexpr uint32 ANUBARAK_GRIP_IDLE_MS = 12000;

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
// Focus fire on the burrowers starts once killing every one alive at the raid's rate, plus this, reaches
// the next wave. The margin pays for the switch, the melee walking off him to the patches.
constexpr uint32 ANUBARAK_FOCUS_MARGIN_MS = 15000;
// The raid's rate on burrowers against its rate on him: melee lose reach on the patches (128k against
// 174k/s, 25H)
constexpr float ANUBARAK_FOCUS_RATE_SHARE = 0.7f;
// Heroic phase 2 time before the deadline counts at this share: only the ranged reach the patches from
// the gather (25-40k/s)
constexpr float ANUBARAK_FOCUS_SUBMERGED_SHARE = 0.25f;
// One this low off Permafrost still dies before its next 3 s submerge check
constexpr float ANUBARAK_FOCUS_OFF_FROST_PCT = 10.0f;
// The script's first submerge check comes 30 s after a burrower spawns or emerges, so one off Permafrost
// is still worth killing this long after either
constexpr uint32 ANUBARAK_BURROWER_FREE_MS = 25000;
constexpr uint32 ANUBARAK_DAMAGE_WINDOW_MS = 10000;
constexpr uint32 ANUBARAK_DAMAGE_SAMPLE_MS = 500;
constexpr uint32 ANUBARAK_DAMAGE_MIN_SPAN_MS = 3000;

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
// navprobe-clean all round; heroic's kite steps stay inside it
constexpr float ANUBARAK_KITE_RING_RADIUS = 40.0f;
// Heroic: the spike this close and faster than the kiter sends the next speed boost out, and at CLOSE
// whatever its speed
constexpr float ANUBARAK_KITE_BOOST_RANGE = 15.0f;
constexpr float ANUBARAK_KITE_BOOST_CLOSE = 10.0f;
// Heroic: the spike's route keeps this far off a patch it isn't meant to take, its 11.5 yd test plus 3
constexpr float ANUBARAK_SPIKE_ROUTE_CLEARANCE = 14.5f;
// Heroic: an immunity this close to running out sends the kiter to its patch, when there's none to walk to
constexpr uint32 ANUBARAK_KITE_IMMUNE_LEFT_MS = 2000;
// Heroic, a kite patch alive: the kiter turns for it this much ahead of its walk there, under Hand of
// Protection before the hand runs out, otherwise before the spike closes to Impale's reach
constexpr uint32 ANUBARAK_KITE_IMMUNE_COMMIT_MS = 1500;
constexpr uint32 ANUBARAK_KITE_SPIKE_COMMIT_MS = 1000;
// Impale's 6 yd plus 1.5 reach
constexpr float ANUBARAK_IMPALE_REACH = 7.5f;
// Heroic, a kite patch alive and the spike faster: lane steps keep the kiter this close to its stand
constexpr float ANUBARAK_KITE_STAND_LEASH = 25.0f;
// The teleport back to the Demonic Circle only goes when the circle is this much farther from the spike
constexpr float ANUBARAK_CIRCLE_TELEPORT_GAIN = 15.0f;
// A circle with less left than this is cast again
constexpr int32 ANUBARAK_CIRCLE_REFRESH_MS = 100000;
constexpr float ANUBARAK_BLINK_DISTANCE = 20.0f;
// Heroic phase 2: the raid stands this far past where he went down, away from the tank patches, and
// never further out than the floor radius
constexpr float ANUBARAK_GATHER_PAST = 5.0f;
constexpr float ANUBARAK_GATHER_FLOOR = 35.0f;
constexpr float ANUBARAK_GATHER_ARRIVE = 6.0f;
constexpr float ANUBARAK_GATHER_SPREAD = 3.0f;
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
// spheres for the pull, so its kites get a share per submerge instead.
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
    // The spike gains within 15 yd, or is within 10: the next speed boost goes out
    Boost,
    // Hand of Protection up, still running
    Immune,
    // The hand or the lead running out before the walk to the kite patch, or nothing left: to a patch
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
};

struct AnubarakBurrowerPick
{
    // On this side tank, its current target first
    Creature* held = nullptr;
    // This side's, on someone else: one hitting a healer first, then the nearest the hold
    Creature* loose = nullptr;
    // This side's patch, else its spot
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
    // To the side's spot
    Walk,
    // On the spot with nothing in reach, or the gripped sphere on its way
    Hold,
    Cast,
    // The gripped sphere landed on the DK
    Kill
};

struct AnubarakGripDuty
{
    // Null while no sphere is in reach of the stand
    Creature* sphere = nullptr;
    // The stand: the side's spot, or up to ANUBARAK_GRIP_STEP_OUT off it toward the sphere; the kite spot
    Position spot;
    // 0 or 1, or ANUBARAK_KITE_GRIP
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
// The side's patch, else its side point moved with the other side's patch so the two stay 14 yd apart
Position GetAnubarakTankSpot(Player* bot, uint8 side);
// Midpoint of both tank patches, else the room centre
Position GetAnubarakBossAnchor(Player* bot);
// Pre-pull stand of the pickup tank, west of the anchor and off both tank spots
Position GetAnubarakPickupStand(Player* bot);
// Midpoint of both latched tank patches. False until both have landed: he is held where picked up.
bool GetAnubarakDragAnchor(Player* bot, Position& out);
// The anchor, then points 4 and 7.5 yd off it toward his side of the patch line, for when the leg to
// the anchor would walk the tank through Permafrost. Empty without an anchor.
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
// The burrower DPS focus, latched per instance in anub.focus. Ones standing on Permafrost count, since
// one below 80% off it submerges and comes back full, plus any under ANUBARAK_FOCUS_OFF_FROST_PCT, plus
// any off it within ANUBARAK_BURROWER_FREE_MS of coming up. Those go first, the one whose time ends
// first, then the lowest health. The latched one stays while it qualifies and none of those outranks it.
Creature* GetAnubarakFocusBurrower(Player* bot);
// This DPS bot's target: a player's skull, him while selectable, else the focus burrower, else while
// he's down the nearest scarab in reach. The focus burrower goes ahead of him while he's down, while
// it's off Permafrost in its free time, or once killing every burrower at ANUBARAK_FOCUS_RATE_SHARE of
// the raid's rate would run within the margin of the next wave, then until the focus it came due on
// dies. Null outside the pull, for non-DPS and for a bot on sphere duty.
Unit* GetAnubarakDpsTarget(PlayerbotAI* botAI);
// Nearest scarab within 30 yd hitting someone who isn't a tank
Creature* GetAnubarakScarabToPickUp(Player* bot);
// Lowest health Penetrating Cold carrier under 90% this healer can reach
Player* GetAnubarakPenetratingColdHealTarget(PlayerbotAI* botAI);

// The flying sphere this bot shoots now, null when it has no sphere duty
Creature* GetAnubarakSphereToShoot(Player* bot);
// The DK's grip of a sphere for its side's tank patch. False without one.
bool GetAnubarakGripDuty(Player* bot, AnubarakGripDuty& out);
// Called once the DK's Death Grip on its duty sphere went out
void NoteAnubarakGripCast(Player* bot, Creature* sphere);
// The shot or gripped sphere this bot's duty is on, else null
Creature* GetAnubarakDutySphere(Player* bot);
// 3D within the bot's spell range less ANUBARAK_SHOOT_MARGIN, in line of sight
bool AnubarakSphereInReach(Player* bot, Unit* sphere);
// Floor point toward the sphere where it is in reach, off the spike and its lane. False when none.
bool GetAnubarakSphereStand(Player* bot, Unit* sphere, Position& out);
// Where the spike's target runs. previous is the stand the caller latched last time, may be null.
// False while there's no spike. Heroic runs lane steps until the kite stage says patch.
bool GetAnubarakKiteStand(Player* bot, AnubarakKiteStand const* previous, AnubarakKiteStand& out);
// Heroic only, None elsewhere and for an unmarked bot. With Boost, boost says what to use.
AnubarakKiteStage GetAnubarakKiteStage(Player* kiter, AnubarakKiteBoost* boost = nullptr);
// Heroic phase 2: this bot's spot in the raid's gather, not floor-checked, false when it doesn't gather
// (the kiter, side and main tanks)
bool GetAnubarakGatherSpot(Player* bot, Position& out);
bool AnubarakGathers(PlayerbotAI* botAI);
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
// The marked bot itself must cast an immunity now: the spike within the immunity range, no boost, no
// protector, and no kite patch stand it reaches before the spike reaches it
bool AnubarakKiterNeedsImmunity(Player* bot);
// The marked paladin's own Hand of Protection keeps it on the spike to a kite patch or the emerge;
// otherwise Divine Shield hands the spike on
bool AnubarakKiterKeepsMark(Player* bot);
bool AnubarakKiterImmune(Unit* kiter);

// The burrower whose Shadow Strike this bot interrupts, null when it has none
Creature* GetAnubarakShadowStrikeDuty(Player* bot);
// Name of the first interrupt or stun the bot can cast on target now, null when none.
// Burrowers are silence-immune, so no silence-only spells here.
char const* AnubarakReadyInterrupt(Player* bot, Unit* target);

}

#endif
