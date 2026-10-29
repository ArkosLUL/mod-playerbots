/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Putricide.h"

#include <cmath>
#include <string>

#include "Creature.h"
#include "CreatureAI.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "ICCHelpers_Shared.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Vehicle.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
// The kite plans its next leg this far before the end of the last, so it never stops
constexpr float KITE_REPLAN = 3.0f;
// A walk onto a bot re-aims once that bot has moved this far
constexpr float FOLLOW_REAIM = 1.0f;
constexpr float DRIVE_REAIM = 2.0f;
constexpr uint32 DRINK_RETRY_MS = 2000;
// The vehicle spells' RecoveryTime. A creature caster gets no cooldown of its own, so it is stamped on a
// cast whose receipt shows.
constexpr uint32 VEHICLE_SPELL_RECOVERY_MS = 1000;

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

MovementPriority Below(MovementPriority priority)
{
    return priority == MovementPriority::MOVEMENT_IDLE ? priority
                                                      : static_cast<MovementPriority>(static_cast<int>(priority) - 1);
}
}  // namespace

Position const* IccPutricideWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccPutricideWalkAction::WalkTo(Position const& spot, MovementPriority priority)
{
    // A walk issued mid-cast never starts but still books its travel time, and the same spot is refused
    // until that runs out
    if (bot->IsMovementPreventedByCasting())
        return false;

    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    // A stun or an item use stopped the last walk short, and its booking would refuse this one
    uint32 const now = getMSTime();
    if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
        last.clear();

    MovementPriority const booked = last.priority;
    bool const lowered = walking && BookedOnWalkSpot() && booked >= priority &&
                         spot.GetExactDist2d(walkSpot) > WALK_SPOT_TOLERANCE;
    if (lowered)
        last.priority = Below(priority);

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    bool const moved = MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false,
                              false, false, false, priority);
    if (!moved && lowered)
        last.priority = booked;

    return moved;
}

bool IccPutricideWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccPutricideDodgeAction::Execute(Event /*event*/)
{
    char const* kind = GetPpHazardKind(botAI);
    if (!kind)
    {
        DropWalk();
        return false;
    }

    if (!botAI->CanMove())
    {
        NoteDerived(bot, "pp.dodge", "pinned");
        return false;
    }

    Position const* inFlight = WalkInFlight();
    if (inFlight && IsPpDodgeSpotClear(botAI, *inFlight))
    {
        NoteDerived(bot, "pp.dodge", "hold");
        return true;
    }

    Position spot;
    bool interrupt = false;
    std::string branch;
    if (!FindPpDodgeSpot(botAI, spot, interrupt, branch))
    {
        NoteDerived(bot, "pp.dodge", std::string(kind) + " none");
        return false;
    }

    // A puddle tick, gas, a blast or goo about to land: each costs more than the cast
    if (interrupt && bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    float const travel = bot->GetExactDist2d(spot);
    if (!WalkTo(spot, MovementPriority::MOVEMENT_FORCED))
    {
        NoteDerived(bot, "pp.dodge", std::string(kind) + " locked");
        return false;
    }

    std::string const yards = std::to_string(static_cast<int32>(std::lround(travel)));
    NoteDerived(bot, "pp.dodge", std::string(kind) + " " + branch + " " + yards);
    return true;
}

bool IccPutricideSwapAction::Execute(Event /*event*/)
{
    uint32 defensive = 0;
    bool taunt = false;
    Creature* boss = GetPpBoss(botAI);
    if (!boss || !IsPpSwapTaunter(botAI, defensive, taunt))
        return false;

    // A sharer takes his swings for one application, so its button goes up first
    if (defensive && botAI->CastSpell(defensive, bot))
        return true;

    if (!taunt || !IccCastClassTaunt(bot, botAI, boss))
        return false;

    // A taunt moves him, not the bot's own target
    AttackDirect(boss);
    return true;
}

bool IccPutricideCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetPpTankDefensive(botAI, "pp.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccPutricideKiteAction::Execute(Event /*event*/)
{
    if (!IsPpKiter(botAI))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (bot->GetExactDist2d(*inFlight) > KITE_REPLAN && IsPpKiteLegSafe(botAI, *inFlight))
            return true;

    Position spot;
    char const* branch = nullptr;
    if (!GetPpKiteLeg(botAI, spot, branch))
    {
        NoteDerived(bot, "pp.kite", "none");
        return false;
    }

    // A bot standing to finish a cast lets the cloud close 8 yd a second
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    EncounterHelpers::UseNitroBoosts(bot);
    if (!WalkTo(spot, MovementPriority::MOVEMENT_COMBAT))
    {
        NoteDerived(bot, "pp.kite", "locked");
        return false;
    }

    CommitPpKiteLeg(botAI, spot);
    NoteDerived(bot, "pp.kite", branch);
    return true;
}

bool IccPutricidePlagueAction::Execute(Event /*event*/)
{
    Position spot;
    char const* branch = nullptr;
    if (!GetPpPlagueWalk(botAI, spot, branch))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= FOLLOW_REAIM)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}

bool IccPutricideStackAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetPpStackWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= FOLLOW_REAIM)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}

bool IccPutricideAbominationAction::Execute(Event /*event*/)
{
    PpAbomPlan plan;
    if (!GetPpAbomPlan(botAI, plan))
    {
        DropWalk();
        return false;
    }

    if (plan.duty == PpAbomDuty::Drink)
    {
        if (plan.walk)
        {
            if (Position const* inFlight = WalkInFlight())
                if (inFlight->GetExactDist2d(plan.spot) <= WALK_SPOT_TOLERANCE)
                    return true;

            return WalkTo(plan.spot, MovementPriority::MOVEMENT_COMBAT);
        }

        uint32 const now = getMSTime();
        if (!plan.cast || (lastDrinkMs && getMSTimeDiff(lastDrinkMs, now) < DRINK_RETRY_MS))
            return true;

        lastDrinkMs = now;
        bot->StopMoving();
        plan.table->Use(bot);
        return true;
    }

    Unit* base = bot->GetVehicleBase();
    Creature* abo = base ? base->ToCreature() : nullptr;
    if (!abo)
        return false;

    if (plan.duty == PpAbomDuty::Exit)
    {
        driving = false;
        bot->ExitVehicle();
        return true;
    }

    AiObjectContext* aiContext = botAI->GetAiObjectContext();
    if (plan.duty == PpAbomDuty::Spit)
    {
        uint32 const spitId = aiContext->GetValue<uint32>("vehicle spell id", "Regurgitated Ooze")->Get();
        uint32 const before = abo->GetPower(POWER_ENERGY);
        if (!botAI->CastVehicleSpell(spitId, plan.target))
            return false;

        // CastVehicleSpell answers true on a refused cast too: the energy is the receipt
        if (abo->GetPower(POWER_ENERGY) < before)
            abo->AddSpellCooldown(spitId, 0, VEHICLE_SPELL_RECOVERY_MS);

        return true;
    }

    if (plan.duty == PpAbomDuty::Slash && plan.target && abo->GetVictim() != plan.target && abo->IsAIEnabled)
        abo->AI()->AttackStart(plan.target);

    if (plan.walk)
    {
        bool const reaimed = !driving || driveSpot.GetExactDist2d(plan.spot) > DRIVE_REAIM || !abo->isMoving();
        if (reaimed)
        {
            LastMovement& last = AI_VALUE(LastMovement&, "last movement");
            if (driving && last.priority >= MovementPriority::MOVEMENT_COMBAT)
                last.priority = MovementPriority::MOVEMENT_NORMAL;

            driving = true;
            driveSpot = plan.spot;
            MoveTo(bot->GetMapId(), plan.spot.GetPositionX(), plan.spot.GetPositionY(), plan.spot.GetPositionZ(), false,
                   false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    else
        driving = false;

    if (!plan.cast)
        return true;

    if (plan.duty == PpAbomDuty::Eat)
    {
        uint32 const eatId = aiContext->GetValue<uint32>("vehicle spell id", "Eat Ooze")->Get();
        uint32 const before = abo->GetPower(POWER_ENERGY);
        if (botAI->CanCastVehicleSpell(eatId, abo) && botAI->CastVehicleSpell(eatId, abo) &&
            abo->GetPower(POWER_ENERGY) > before)
            abo->AddSpellCooldown(eatId, 0, VEHICLE_SPELL_RECOVERY_MS);

        return true;
    }

    uint32 const slashId = aiContext->GetValue<uint32>("vehicle spell id", "Mutated Slash")->Get();
    if (botAI->CanCastVehicleSpell(slashId, plan.target) && botAI->CastVehicleSpell(slashId, plan.target))
        abo->AddSpellCooldown(slashId, 0, VEHICLE_SPELL_RECOVERY_MS);

    return true;
}

bool IccPutricideBrakeStopAction::Execute(Event /*event*/)
{
    Creature* boss = GetPpBoss(botAI);
    if (!boss || !IsPpBraked(botAI))
        return false;

    bool stopped = false;
    if (bot->GetVictim() == boss)
    {
        bot->AttackStop();
        stopped = true;
    }

    if (AI_VALUE(Unit*, "current target") == boss)
    {
        context->GetValue<Unit*>("current target")->Set(nullptr);
        stopped = true;
    }

    return stopped;
}

bool IccPutricideStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetPpStandWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}
