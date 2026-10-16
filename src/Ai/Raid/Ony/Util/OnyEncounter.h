/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYENCOUNTER_H
#define PLAYERBOTS_ONYENCOUNTER_H

#include <vector>

#include "OnyData.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

// Who is doing what: the per-instance blackboard, phases, the main tank, threat and burst, the
// layout slots, the dodge and the target ladder. Latches are written by TickOnyxia only; triggers,
// actions and rules read them.
namespace OnyHelpers
{
// Raw numbers are what `onyxia.phase` traces.
enum class OnyxiaPhase : uint8
{
    None,
    Ground,   // phase 1
    Liftoff,  // threat wiped, walking to her takeoff point
    Air,      // phase 2
    Landing,  // below 40%, coming down; threat is wiped again on the way
    Final     // phase 3
};

// What the main tank is doing. Traced as `onyxia.tank`.
enum class TankBranch : uint8
{
    None,
    Pickup,   // waiting for her to land
    Taunt,    // she is grounded on someone else and the taunt is up
    Recover,  // she is on someone else and the taunt is down: walk in and hit her
    Drag,     // walking her to the tank spot
    Return,   // back to the tank spot after a knockback
    Hold      // on the spot
};

namespace DodgeReason
{
constexpr uint8 Egg = 1 << 0;
constexpr uint8 Breath = 1 << 1;
constexpr uint8 Nova = 1 << 2;
constexpr uint8 Fireball = 1 << 3;
}  // namespace DodgeReason

// What every rule and guard reads. Resolved once per bot per tick.
struct OnyxiaSnapshot
{
    Creature* boss = nullptr;
    OnyxiaPhase phase = OnyxiaPhase::None;
};
OnyxiaSnapshot const& OnyxiaSnapshotFor(PlayerbotAI* botAI);

inline bool IsGrounded(OnyxiaPhase phase) { return phase == OnyxiaPhase::Ground || phase == OnyxiaPhase::Final; }
inline bool IsAirborne(OnyxiaPhase phase) { return phase == OnyxiaPhase::Liftoff || phase == OnyxiaPhase::Air; }
// Phases the ranged and healers stand in their ground slots for.
inline bool IsGroundLayout(OnyxiaPhase phase) { return IsGrounded(phase) || phase == OnyxiaPhase::Landing; }

// Strategy::OnTick for every bot: phases, latches, caches and the trace.
void TickOnyxia(PlayerbotAI* botAI);

// The main tank if alive, else the living tank highest on her threat list, else nullptr.
Player* EffectiveMainTank(Player* bot, Unit* boss);
bool MainTankIsBot(Player* bot, Unit* boss);
// How long her victim has been the main tank, 0 while it is not.
uint32 MainTankHeldMs(Player* bot);

// This bot's branch while it is the main tank, None otherwise.
TankBranch MainTankBranch(PlayerbotAI* botAI);
bool MainTankOwnsMovement(PlayerbotAI* botAI);
Position const* MainTankDestination(TankBranch branch);
// Re-issue gate for the tank's walk: true when a new MoveTo is due.
bool MainTankWalkDue(Player* bot);
void MarkMainTankWalk(Player* bot);

// 0 free, 1 waiting for the main tank, 2 at the cap of his threat, 3 she is landing.
uint8 ThreatHoldLatch(Player* bot);
// Lust only in phase 3. Everything else gated on the main tank's hold, free in the air, held while
// she lands.
bool BurstAllowed(PlayerbotAI* botAI, bool lust);

Player* RedirectTankFor(PlayerbotAI* botAI);
bool FearWindowActive(PlayerbotAI* botAI);

// Live eggs from the per-instance cache.
std::vector<Position> const& LiveEggs(Player* bot);
bool NearLiveEgg2d(Player* bot, float x, float y, float radius);

// The target ladder per role and phase, nullptr for healers.
Unit* OnyxiaPickTarget(PlayerbotAI* botAI);

// Ranged and healers: the ground slot, validated against the floor, eggs and (in phase 3) fissures.
bool GroundSlotFor(PlayerbotAI* botAI, Position& slot);
// Ranged and healers: the phase 2 cell for her current or next waypoint.
bool AirSpotFor(PlayerbotAI* botAI, Position& spot);
// Melee: the point on her flank, or false while they are already inside the band.
bool RearFlankPointFor(PlayerbotAI* botAI, Position& point, bool forTrigger);

// DodgeReason bits; non-zero means the dodge owns this bot's movement.
uint8 DodgeReasons(PlayerbotAI* botAI);
bool DodgeDestination(PlayerbotAI* botAI, uint8 reasons, Position& destination);
// Players within the Fireball splash of this bot.
uint32 FireballNeighbours(Player* bot);

bool BreathLive(Player* bot);
bool MainTankCooldownWanted(PlayerbotAI* botAI);
}  // namespace OnyHelpers

#endif
