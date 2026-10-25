#include "ToCActions_TwinValkyr.h"
#include "ToCData.h"
#include "ToCHelpers_Defensive.h"
#include "ToCHelpers_TwinValkyr.h"
#include "Playerbots.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "Timer.h"
#include "Unit.h"
#include "WorldSession.h"
#include "WorldPacket.h"

#include <cmath>

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{
// MoveTo refuses the point it last issued for 5 s, moving or not, so a walk stopped short never
// restarts unless its booking goes once the bot has stood still this long.
constexpr uint32 STALLED_WALK_MS = 500;
constexpr float SAME_WALK_DESTINATION = 1.0f;
// a spot kept from an earlier wave is no answer to this one
constexpr uint32 DODGE_SPOT_LATCH_MS = 1000;
// inside the 30 yd taunt with room for her to drift
constexpr float TWIN_FETCH_STOP = 25.0f;
// the floor read is taken only this close to the traced height
constexpr float DOOR_FLOOR_TOLERANCE = 3.0f;

Position DoorFloor(Player* bot, Position const& spot)
{
    float const ground = bot->GetMapWaterOrGroundLevel(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ());
    if (std::fabs(ground - spot.GetPositionZ()) > DOOR_FLOOR_TOLERANCE)
        return spot;

    return Position(spot.GetPositionX(), spot.GetPositionY(), ground);
}

// Drops a stalled booking, and with releaseElsewhere a booking for any other destination too, since
// forced doesn't outrank forced. A walk still headed for destination is kept so the duplicate check
// stops a re-issue every tick.
void ReleaseWalk(PlayerbotAI* botAI, Player* bot, Position const& destination, bool releaseElsewhere)
{
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (!last.msTime)
        return;

    bool const stalled = !bot->isMoving() && getMSTimeDiff(last.msTime, getMSTime()) > STALLED_WALK_MS;
    bool const elsewhere = last.lastMoveShort.GetExactDist2d(destination.GetPositionX(),
                                                             destination.GetPositionY()) > SAME_WALK_DESTINATION;
    if (stalled || (releaseElsewhere && elsewhere))
        last.clear();
}
}  // namespace

bool TwinValkyrInterruptPactAction::Execute(Event /*event*/)
{
    Unit* twin = GetTwinCastingPact(botAI);
    if (!twin)
        return false;

    char const* interrupt = TwinReadyInterrupt(bot, twin);
    return interrupt && botAI->CastSpell(interrupt, twin);
}

bool TwinValkyrEssenceActionBase::Execute(Event /*event*/)
{
    TwinEssenceWant const want = GetWantedEssence(botAI);
    if (want.reason != reason || want.colour == TwinColour::None)
        return false;

    return AcquireEssence(want.colour);
}

bool TwinValkyrEssenceActionBase::AcquireEssence(TwinColour colour)
{
    Creature* portal = GetEssencePortal(bot, colour);
    if (!portal)
        return false;

    // 3D on purpose: GetNPCIfCanInteractWith gates on IsWithinDistInMap, so a 2D check can call an
    // elevated portal in range and the gossip silently does nothing
    if (bot->GetDistance(portal) > INTERACTION_DISTANCE)
    {
        // a cast pins the feet; the base colour can wait for it to end, the others can't
        if (reason != TwinEssenceReason::Base && bot->IsMovementPreventedByCasting())
            bot->InterruptNonMeleeSpells(true);

        bool const urgent = reason == TwinEssenceReason::Touch || reason == TwinEssenceReason::Vortex;
        ReleaseWalk(botAI, bot, portal->GetPosition(), urgent);

        if (MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, portal->GetPositionX(), portal->GetPositionY(),
                   portal->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_FORCED, true, false))
        {
            return true;
        }

        // Hold the tick while the walk is refused as a duplicate: lower nodes like a feral charge
        // would leap the bot back onto the twin, and the forced lock doesn't stop spell movers
        return bot->isMoving();
    }

    // The essence comes from the portal's C++ OnGossipHello, so the hello opcode fires it with or
    // without DB gossip items.
    bot->CastStop();
    bot->SetFacingToObject(portal);

    WorldPacket packet;
    packet << portal->GetGUID();
    bot->GetSession()->HandleGossipHelloOpcode(packet);

    if (EssenceOf(bot) != colour)
        return false;

    // Gossip reach ends well short of the portal the walk is booked to. Stop here so the bot heads
    // straight back instead of finishing the walk under a forced lock.
    if (bot->isMoving())
    {
        bot->StopMoving();
        AI_VALUE(LastMovement&, "last movement").clear();
    }

    return true;
}

bool TwinValkyrDodgeOrbAction::Execute(Event /*event*/)
{
    // only clip a cast when the orb is about to go off on this bot, a far one isn't worth it
    if (bot->IsMovementPreventedByCasting() && TwinOrbThreatens(botAI, TWIN_ORB_TRIGGER_RADIUS + 0.5f))
        bot->InterruptNonMeleeSpells(true);

    uint32 const now = getMSTime();
    bool const kept = dodgeSpotMs && getMSTimeDiff(dodgeSpotMs, now) < DODGE_SPOT_LATCH_MS &&
                      TwinOrbSpotClear(botAI, dodgeSpot, TWIN_ORB_DODGE_CLEARANCE);
    if (!kept && !FindTwinOrbDodgeSpot(botAI, dodgeSpot))
    {
        dodgeSpotMs = 0;
        return false;
    }

    dodgeSpotMs = now;

    if (bot->GetExactDist2d(dodgeSpot.GetPositionX(), dodgeSpot.GetPositionY()) <= CONTACT_DISTANCE)
        return false;

    ReleaseWalk(botAI, bot, dodgeSpot, true);

    if (MoveTo(bot->GetMapId(), dodgeSpot.GetPositionX(), dodgeSpot.GetPositionY(), dodgeSpot.GetPositionZ(),
               false, false, false, true, MovementPriority::MOVEMENT_FORCED, true, false))
        return true;

    // the walk in flight is refused as a duplicate; hold the tick until it lands
    return bot->isMoving();
}

bool TwinValkyrCastVortexRaidCoverAction::Execute(Event /*event*/)
{
    ToCPredictedHit hit;
    ToCRaidCover const cover = GetToCRaidCover(botAI, GetTwinVortexRaidHit(botAI, hit) ? &hit : nullptr);
    uint32 const spellId = ToCRaidCoverSpellId(botAI, cover);
    if (!spellId)
        return false;

    // a channel fails any other cast's check, and a moving bot breaks its own channel
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    if (IsToCRaidCoverChannel(cover) && bot->isMoving())
        bot->StopMoving();

    if (!botAI->CastSpell(spellId, bot))
        return false;

    FinishToCRaidCover(botAI, cover);
    return true;
}

bool TwinValkyrCastVortexDefensiveAction::Execute(Event /*event*/)
{
    ToCPredictedHit hit;
    ToCPersonalPick const pick = GetToCPersonalDefensive(botAI, GetTwinVortexHit(botAI, hit) ? &hit : nullptr);
    if (!pick)
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    return pick.consumable ? botAI->DoSpecificAction(pick.consumable, Event(), true)
                           : botAI->CastSpell(pick.spellId, bot);
}

bool TwinValkyrMoveToDoorStandAction::Execute(Event /*event*/)
{
    TwinDoorStand const stand = GetTwinDoorStand(bot);
    if (stand.role == TwinDoorRole::None)
        return false;

    Position const spot = DoorFloor(bot, stand.spot);
    if (bot->GetExactDist2d(spot) <= TWIN_DOOR_PARK)
        return false;

    ReleaseWalk(botAI, bot, spot, false);

    // the tunnel is straight and has no navmesh
    if (MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false,
               false, false, true, MovementPriority::MOVEMENT_COMBAT, true, false))
        return true;

    return bot->isMoving();
}

bool TwinValkyrTankHoldAction::HoldTwin(Unit* twin)
{
    // the target can be out of sight round the tunnel mouth, and the walk below still has to run
    if (AI_VALUE(Unit*, "current target") != twin && Attack(twin))
        return true;

    // Taunt back an assigned twin that's on someone else. Taunts diminish on the twins, so the 5th
    // without a 15 s gap is immune.
    for (Unit* assigned : {GetFjola(botAI), GetEydis(botAI)})
    {
        if (assigned && assigned->GetVictim() != bot && IsTwinTank(bot, assigned) &&
            CastClassTaunt(botAI, assigned))
        {
            return true;
        }
    }

    return TwinValkyrFetchDue(botAI, twin) ? FetchTwin(twin) : WalkToDoor(twin);
}

// Walks to 25 yd of her for the taunt. If it can't land, the door hold is off while fetching, so
// reach melee walks the rest.
bool TwinValkyrTankHoldAction::FetchTwin(Unit* twin)
{
    doorWalk = false;
    if (bot->GetExactDist2d(twin) <= TWIN_FETCH_STOP)
        return false;

    Position const door = GetTwinDoorSpot(bot, twin);
    float const span = twin->GetExactDist2d(door);
    if (span <= 0.0f)
        return false;

    float const x = twin->GetPositionX() + (door.GetPositionX() - twin->GetPositionX()) * TWIN_FETCH_STOP / span;
    float const y = twin->GetPositionY() + (door.GetPositionY() - twin->GetPositionY()) * TWIN_FETCH_STOP / span;
    float const ground = bot->GetMapWaterOrGroundLevel(x, y, bot->GetPositionZ());
    float const z = ground > INVALID_HEIGHT ? ground : bot->GetPositionZ();

    ReleaseWalk(botAI, bot, Position(x, y, z), false);
    if (MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT,
               true, false))
        return true;

    return bot->isMoving();
}

bool TwinValkyrTankHoldAction::WalkToDoor(Unit* twin)
{
    Position const spot = DoorFloor(bot, GetTwinDoorStand(bot).spot);
    float const off = bot->GetExactDist2d(spot);
    if (off <= TWIN_DOOR_PARK || (!doorWalk && off <= TWIN_DOOR_UNPARK))
    {
        doorWalk = false;
        return false;
    }

    doorWalk = true;
    ReleaseWalk(botAI, bot, spot, false);

    // Backing up keeps her in front, and a tank takes no parry or block from behind. Only when she's on
    // the far side of the walk, or it would turn its back on her.
    float const toSpotX = spot.GetPositionX() - bot->GetPositionX();
    float const toSpotY = spot.GetPositionY() - bot->GetPositionY();
    float const toTwinX = twin->GetPositionX() - bot->GetPositionX();
    float const toTwinY = twin->GetPositionY() - bot->GetPositionY();
    bool const backwards = twin->GetVictim() == bot && toSpotX * toTwinX + toSpotY * toTwinY < 0.0f;

    if (MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false,
               false, false, true, MovementPriority::MOVEMENT_COMBAT, true, backwards))
        return true;

    return bot->isMoving();
}

bool TwinValkyrMainTankHoldLightTwinAction::Execute(Event /*event*/)
{
    Unit* fjola = GetFjola(botAI);
    if (!fjola)
        return false;

    return HoldTwin(fjola);
}

bool TwinValkyrAssistTankHoldDarkTwinAction::Execute(Event /*event*/)
{
    Unit* eydis = GetEydis(botAI);
    if (!eydis)
        return false;

    return HoldTwin(eydis);
}

Player* TwinValkyrRedirectThreatAction::GetRedirectTank()
{
    Unit* twin = GetTwinDpsTarget(botAI);
    if (!twin)
        return nullptr;

    Player* tank = GetTwinTank(bot, twin);
    return tank && tank->IsAlive() ? tank : nullptr;
}

Unit* TwinValkyrRedirectThreatAction::GetThreatDumpTarget() { return GetTwinDpsTarget(botAI); }

bool TwinValkyrFocusTwinAction::Execute(Event /*event*/) { return AttackDirect(GetTwinDpsTarget(botAI)); }
