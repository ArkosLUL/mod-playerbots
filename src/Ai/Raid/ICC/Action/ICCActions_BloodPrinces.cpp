/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_BloodPrinces.h"

#include <cmath>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "ICCHelpers_Shared.h"
#include "ICCSeams.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidDefensive.h"
#include "Spell.h"
#include "Timer.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
// A ball, soak or nucleus walk re-aims once its point has moved this far
constexpr float REAIM = 2.0f;
}  // namespace

Position const* IccBpcWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccBpcWalkAction::WalkTo(Position const& spot, MovementPriority priority, bool interrupt)
{
    // A walk issued mid-cast never starts but still books its travel time, and the same spot is refused
    // until that runs out
    if (bot->IsMovementPreventedByCasting())
    {
        if (!interrupt)
            return false;

        bot->InterruptNonMeleeSpells(true);
    }

    // A stun or an item use stopped the last walk short, and its booking would refuse this one
    uint32 const now = getMSTime();
    if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
        AI_VALUE(LastMovement&, "last movement").clear();

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, priority);
}

bool IccBpcWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccBpcDodgeAction::Execute(Event /*event*/)
{
    if (!IsBpcInVortex(botAI))
    {
        DropWalk();
        return false;
    }

    if (WalkInFlight())
        return true;

    Position spot;
    if (!FindBpcDodgeSpot(botAI, spot))
        return false;

    // The vortex knocks back and hits for 8k or more every 1.5 s for 25 s, which costs more than any cast
    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED, true);
}

bool IccBpcCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetBpcTankDefensive(botAI, "bpc.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccBpcCastLanceDefensiveAction::Execute(Event /*event*/)
{
    RaidPredictedHit hit;
    if (!GetBpcLanceHit(botAI, hit))
        return false;

    RaidPersonalPick const pick = GetRaidPersonalDefensive(botAI, ICC_DEFENSIVE_SCOPE, &hit);
    if (!pick)
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    return pick.consumable ? botAI->DoSpecificAction(pick.consumable, Event(), true)
                           : botAI->CastSpell(pick.spellId, bot);
}

bool IccBpcCastFlameDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetBpcFlameDefensive(botAI);
    if (!spellId)
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    return botAI->CastSpell(spellId, bot);
}

bool IccBpcDispelAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    Player* carrier = GetBpcDispelTarget(botAI, spell);
    return carrier && spell && botAI->CastSpell(spell, carrier);
}

bool IccBpcBrakeStopAction::Execute(Event /*event*/)
{
    Creature* prince = GetBpcBrakeTarget(botAI);
    if (!prince)
        return false;

    bool stopped = false;
    if (bot->GetVictim() == prince)
    {
        bot->AttackStop();
        stopped = true;
    }

    // AttackStop leaves Auto Shot and wands running
    Spell* autoRepeat = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
    if (autoRepeat && autoRepeat->m_targets.GetUnitTarget() == prince)
    {
        bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
        stopped = true;
    }

    if (AI_VALUE(Unit*, "current target") == prince)
    {
        context->GetValue<Unit*>("current target")->Set(nullptr);
        stopped = true;
    }

    return stopped;
}

bool IccBpcTargetAction::Execute(Event /*event*/)
{
    Unit* pick = GetBpcTarget(botAI);
    Unit* petPick = GetBpcPetTarget(botAI);
    if (petPick == pick)
        return AttackDirect(pick);

    // The pet stays on its bomb while the bot fights on
    EncounterHelpers::CommandPetAttack(botAI, petPick);
    return pick && AI_VALUE(Unit*, "current target") != pick && Attack(pick);
}

bool IccBpcBallAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetBpcBallWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= REAIM)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT, true);
}

bool IccBpcPrinceTauntAction::Execute(Event /*event*/)
{
    Unit* prince = GetBpcPrinceToTaunt(botAI);
    if (!prince || !IccCastClassTaunt(bot, botAI, prince))
        return false;

    // A taunt moves the prince, not the bot's own target
    AttackDirect(prince);
    return true;
}

bool IccBpcNucleusAction::Execute(Event /*event*/)
{
    // A taunt moves the nucleus onto the tank, which stays on Keleseth
    if (Unit* nucleus = GetBpcNucleusToTaunt(botAI))
    {
        DropWalk();
        return IccCastClassTaunt(bot, botAI, nucleus);
    }

    Position spot;
    if (!GetBpcNucleusWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= REAIM)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}

bool IccBpcSoakAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetBpcSoakWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= REAIM)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}

bool IccBpcStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetBpcStandWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    // A bot still short of its node when Empowered Shock Vortex lands takes every neighbour's hit
    bool const scattering = IsBpcScattering(botAI);
    return WalkTo(spot, scattering ? MovementPriority::MOVEMENT_FORCED : MovementPriority::MOVEMENT_COMBAT,
                  scattering);
}
