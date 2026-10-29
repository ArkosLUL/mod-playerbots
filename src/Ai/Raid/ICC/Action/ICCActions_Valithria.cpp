/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Valithria.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "ICCHelpers_Shared.h"
#include "LastMovementValue.h"
#include "MotionMaster.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "SpellAuras.h"
#include "Timer.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
// A kite leg or a flight re-aims once its point has moved this far
constexpr float REAIM = 2.0f;
constexpr float FLIGHT_ARRIVE = 1.5f;

constexpr float HEAL_RANGE = 36.0f;
constexpr float HEAL_WALK_TO = 25.0f;
// A heal over time is renewed once this little of it is left
constexpr int32 HOT_REFRESH_MS = 2000;

struct Heal
{
    char const* spell;
    // A heal over time kept on her, up to this many stacks; 0 for a direct heal
    uint8 keep;
};

// Earth Shield, Beacon of Light and Sacred Shield are left out: each sits on one target at a time, and the
// generic nodes keep them on the tank
constexpr Heal DRUID_HEALS[] = {{"lifebloom", 3},   {"rejuvenation", 1}, {"regrowth", 1},
                                {"wild growth", 0}, {"nourish", 0},      {"healing touch", 0}};
constexpr Heal PRIEST_HEALS[] = {{"renew", 1}, {"penance", 0}, {"greater heal", 0}, {"flash heal", 0}};
constexpr Heal SHAMAN_HEALS[] = {{"riptide", 1}, {"healing wave", 0}, {"lesser healing wave", 0}};
constexpr Heal PALADIN_HEALS[] = {{"holy shock", 0}, {"holy light", 0}, {"flash of light", 0}};
}  // namespace

Position const* IccVdWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccVdWalkAction::WalkTo(Position const& spot, MovementPriority priority, bool interrupt)
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

bool IccVdWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccValithriaDodgeAction::Execute(Event /*event*/)
{
    if (!IsVdInDanger(botAI))
    {
        DropWalk();
        return false;
    }

    if (WalkInFlight())
        return true;

    Position spot;
    if (!FindVdDodgeSpot(botAI, spot))
        return false;

    // Column of Frost hits 11-20k and throws the bot up, Acid Burst 9-20k plus a 20 s DoT: worth any cast
    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED, true);
}

bool IccValithriaKiteAction::Execute(Event /*event*/)
{
    Position leg;
    if (!GetVdKiteLeg(botAI, leg))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(leg) <= REAIM)
            return true;

    // The boots refuse a casting bot, so the cast goes first
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(true);
    EncounterHelpers::UseNitroBoosts(bot);

    return WalkTo(leg, MovementPriority::MOVEMENT_FORCED, true);
}

bool IccValithriaDreamAction::Execute(Event /*event*/)
{
    Position dest;
    switch (GetVdFlight(botAI, dest))
    {
        case VdFlight::Land:
            flying = false;
            return Land();
        case VdFlight::None:
        case VdFlight::Hover:
            flying = false;
            return false;
        default:
            return FlyTo(dest);
    }
}

bool IccValithriaDreamAction::FlyTo(Position const& dest)
{
    if (bot->GetExactDist(dest) <= FLIGHT_ARRIVE)
    {
        flying = false;
        return false;
    }

    if (flying && bot->isMoving() && flightGoal.GetExactDist(dest) <= REAIM)
        return true;

    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    // A bot gets no client to answer the fly aura, and the generic movers set these flags only while the
    // master flies too
    bot->AddUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING);

    MotionMaster* mm = bot->GetMotionMaster();
    mm->Clear(false);
    mm->MovePoint(0, dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), FORCED_MOVEMENT_NONE, 0.0f, 0.0f,
                  /*generatePath*/ false, /*forceDestination*/ true);
    bot->SendMovementFlagUpdate();

    flying = true;
    flightGoal = dest;
    float const speed = std::max(bot->GetSpeed(MOVE_FLIGHT), 1.0f);
    float const delay = std::min(IN_MILLISECONDS * bot->GetExactDist(dest) / speed,
                                 static_cast<float>(sPlayerbotAIConfig.MaxWaitForMove));
    AI_VALUE(LastMovement&, "last movement")
        .Set(bot->GetMapId(), dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), bot->GetOrientation(),
             delay, MovementPriority::MOVEMENT_FORCED);
    if (RaidObs::Active())
        RaidObs::NoteMove(bot, RaidObs::MoveKind::Point, dest.GetPositionX(), dest.GetPositionY(),
                          dest.GetPositionZ(), ObjectGuid::Empty, RaidObs::MoveOutcome::Issued,
                          RaidObs::MovePriority::Forced);
    return true;
}

bool IccValithriaDreamAction::Land()
{
    bot->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING);
    bot->SendMovementFlagUpdate();

    float const x = bot->GetPositionX();
    float const y = bot->GetPositionY();
    float const startZ = bot->GetPositionZ();
    float groundZ = startZ;
    bot->UpdateAllowedPositionZ(x, y, groundZ);
    if (startZ - groundZ <= WALK_SPOT_TOLERANCE)
        return false;

    // A real fall, damage included: a bot has no client to send the landing that would apply it
    bot->GetMotionMaster()->MoveFall();
    MovementInfo fallInfo = bot->m_movementInfo;
    bot->SetFallInformation(0, startZ);
    fallInfo.pos.Relocate(x, y, groundZ);
    bot->HandleFall(fallInfo);
    bot->SetFallInformation(0, groundZ);
    bot->RemoveUnitMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR);
    return true;
}

bool IccValithriaPortalAction::Execute(Event /*event*/)
{
    Position spot;
    Creature* portal = nullptr;
    if (!GetVdPortalWalk(botAI, spot, portal))
    {
        DropWalk();
        return false;
    }

    if (portal && portal->IsWithinDistInMap(bot, INTERACTION_DISTANCE))
    {
        DropWalk();
        botAI->RemoveShapeshift();
        // The click makes the bot cast Dream State, which a cast in progress would refuse
        if (bot->IsNonMeleeSpellCast(false))
            bot->InterruptNonMeleeSpells(false);

        bot->StopMoving();
        portal->HandleSpellClick(bot);
        return true;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    // An open portal lasts 10 s and anyone may take it: drop a cast to reach it, but wait one out before
    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT, portal != nullptr);
}

bool IccValithriaTauntAction::Execute(Event /*event*/)
{
    Unit* add = GetVdTankTaunt(botAI);
    if (!add || !IccCastClassTaunt(bot, botAI, add))
        return false;

    // A taunt moves the add, not the bot's own target
    AttackDirect(add);
    return true;
}

bool IccValithriaSnareAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    Unit* zombie = GetVdZombieToSnare(botAI, spell);
    return zombie && spell && botAI->CastSpell(spell, zombie);
}

bool IccValithriaVoidAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetVdVoidStep(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (WalkInFlight())
        return true;

    // A void burns mana, not health: the cast in progress finishes first
    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}

bool IccValithriaHealAction::Execute(Event /*event*/)
{
    Creature* valithria = GetVdHealTarget(botAI);
    if (!valithria)
        return false;

    if (!bot->IsWithinDistInMap(valithria, HEAL_RANGE) || !bot->IsWithinLOSInMap(valithria))
    {
        float const angle = valithria->GetAngle(bot);
        return MoveTo(bot->GetMapId(), valithria->GetPositionX() + HEAL_WALK_TO * std::cos(angle),
                      valithria->GetPositionY() + HEAL_WALK_TO * std::sin(angle), valithria->GetPositionZ(), false,
                      false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    return CastHeal(valithria);
}

bool IccValithriaHealAction::CastHeal(Creature* valithria)
{
    Heal const* begin = nullptr;
    Heal const* end = nullptr;
    switch (bot->getClass())
    {
        case CLASS_DRUID:
            begin = std::begin(DRUID_HEALS);
            end = std::end(DRUID_HEALS);
            break;
        case CLASS_PRIEST:
            begin = std::begin(PRIEST_HEALS);
            end = std::end(PRIEST_HEALS);
            break;
        case CLASS_SHAMAN:
            begin = std::begin(SHAMAN_HEALS);
            end = std::end(SHAMAN_HEALS);
            break;
        case CLASS_PALADIN:
            begin = std::begin(PALADIN_HEALS);
            end = std::end(PALADIN_HEALS);
            break;
        default:
            return false;
    }

    for (Heal const* heal = begin; heal != end; ++heal)
    {
        if (heal->keep)
            if (Aura* hot = botAI->GetAura(heal->spell, valithria, true))
                if (hot->GetStackAmount() >= heal->keep && hot->GetDuration() > HOT_REFRESH_MS)
                    continue;

        if (botAI->CanCastSpell(heal->spell, valithria))
            return botAI->CastSpell(heal->spell, valithria);
    }

    return false;
}
