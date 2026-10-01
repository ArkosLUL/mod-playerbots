#include "UldActions_Vezax.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <algorithm>
#include <cmath>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "UldData.h"
#include "UldEncounter_Vezax.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

namespace
{
// Forced doesn't outrank forced, so a soak walk in flight holds a dodge or a mark move until it lands.
// Our own walk is kept: it's what lets IsDuplicateMove stop the re-issue on the next tick.
void ReleaseOtherWalk(PlayerbotAI* botAI, Position const& spot)
{
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (last.lastMoveShort.GetExactDist(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()) >
        ULDUAR_VEZAX_MARK_SPOT_TOLERANCE)
    {
        last.clear();
    }
}
}  // namespace

bool VezaxResetEncounterStateAction::Execute(Event /*event*/)
{
    // One bot drops the whole instance entry; everyone else only lets go of its own slot, so a bot
    // that wandered out of the room cannot wipe a formation that is still fighting.
    ResetVezaxEncounterState(bot, IsMechanicTrackerBot(bot, ULDUAR_MAP_ID));
    return true;
}

bool VezaxMarkOfTheFacelessAction::Execute(Event /*event*/)
{
    Position spot;
    if (!TryGetVezaxMarkSpot(bot, spot))
        return false;

    // Not while a crash is landing on the bot: the dodge's walk is the one in flight then, and its
    // action declines as a duplicate, so without this the mark would take it over.
    Position impact;
    if (!TryGetVezaxShadowCrashImpact(botAI, impact) ||
        bot->GetExactDist2d(impact.GetPositionX(), impact.GetPositionY()) >
            ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS)
    {
        ReleaseOtherWalk(botAI, spot);
    }

    // FORCED, not COMBAT: IsWaitingForLastMove only yields to a strictly higher priority, so at
    // COMBAT any dodge move still in flight swallows this one - and the debuff is ten seconds long.
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxMarkOfTheFacelessBreakAction::Execute(Event /*event*/)
{
    Unit* marked = GetVezaxMarkedAlly(bot);
    if (!marked)
        return false;

    Position spot;
    if (!TryGetVezaxMarkBreakSpot(bot, marked, spot))
        return false;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxShadowCrashDodgeAction::Execute(Event /*event*/)
{
    Position impact;
    if (!TryGetVezaxShadowCrashImpact(botAI, impact))
        return false;

    Position spot;
    if (!TryGetVezaxDodgeSpot(bot, impact, spot))
        return false;

    // Already clear. Yielding here is what lets the soak walk the bot back into the field the missile
    // leaves, and lets a class interrupt through on the ticks in between.
    if (bot->GetExactDist2d(impact.GetPositionX(), impact.GetPositionY()) >
        ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS)
    {
        return false;
    }

    ReleaseOtherWalk(botAI, spot);
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxSearingFlamesInterruptAction::Execute(Event /*event*/)
{
    Unit* boss = GetVezax(botAI);
    if (!boss)
        return false;

    char const* interrupt = VezaxReadyInterrupt(bot, boss);
    if (!interrupt)
        return false;

    return botAI->CastSpell(interrupt, boss);
}

bool VezaxSurgeOfDarknessAction::Execute(Event /*event*/)
{
    // Tank cooldowns only. Divine shield and the like would shed the boss, and the raid needs him
    // held still far more than it needs the tank untouchable for ten seconds.
    static char const* const defensives[] = {"shield wall",      "icebound fortitude",
                                             "survival instincts", "divine protection",
                                             "last stand",       "barkskin",
                                             "shield block"};

    for (char const* defensive : defensives)
        if (botAI->CanCastSpell(defensive, bot) && botAI->CastSpell(defensive, bot))
            return true;

    return false;
}

bool VezaxSaroniteAnimusAction::Execute(Event /*event*/)
{
    Unit* animus = GetFirstAliveUnitByEntry(botAI, NPC_VEZAX_SARONITE_ANIMUS);
    if (!animus)
        return false;

    return Attack(animus);
}

bool VezaxAnimusRedirectAction::Execute(Event /*event*/)
{
    char const* redirect = VezaxAnimusRedirectSpell(bot);
    Player* mainTank = GetGroupMainTank(bot);
    if (!redirect || !mainTank)
        return false;

    return botAI->CastSpell(redirect, mainTank);
}

bool VezaxAnimusBringBackAction::Execute(Event /*event*/)
{
    Player* mainTank = GetGroupMainTank(bot);
    if (!mainTank)
        return false;

    // FORCED: a dodge destination stays latched for seconds after the dodge and swallows a COMBAT move.
    return MoveTo(bot->GetMapId(), mainTank->GetPositionX(), mainTank->GetPositionY(), mainTank->GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxAnimusMeleeSpotAction::Execute(Event /*event*/)
{
    Position spot;
    if (!TryGetVezaxAnimusMeleeSpot(bot, AI_VALUE(Unit*, "current target"), spot))
        return false;

    // FORCED for the same latched dodge destination as the bring-back.
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxShadowCrashSoakAction::Execute(Event /*event*/)
{
    std::vector<VezaxHazard> hazards;
    GatherVezaxHazards(bot, hazards, ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS);

    VezaxHazard field;
    if (!TryGetVezaxNearestHazard(bot, hazards, field))
        return false;

    // Never walk into a spot a missile is about to land on. Fields overlap - one lands every 10s and
    // lasts 20 - so the field worth soaking can sit exactly where the next crash is aimed, and
    // without this the bot bounces between here and the dodge until it lands.
    Position impact;
    if (TryGetVezaxShadowCrashImpact(botAI, impact) &&
        field.position.GetExactDist2d(impact.GetPositionX(), impact.GetPositionY()) <=
            ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS)
    {
        return false;
    }

    // Stop short of the centre. Arriving anywhere inside the 8 yard radius is the whole point, and
    // walking to the exact middle costs cast time for nothing.
    if (bot->GetExactDist2d(field.position.GetPositionX(), field.position.GetPositionY()) <=
        field.radius - 1.0f)
    {
        return false;
    }

    // FORCED, not COMBAT: the dodge's own destination stays latched in IsWaitingForLastMove for
    // seconds after it has stopped wanting it, and that swallowed a quarter of these moves. The dodge
    // and the mark drop this walk before they move, and the soak already refuses any field under a
    // pending missile.
    return MoveTo(bot->GetMapId(), field.position.GetPositionX(), field.position.GetPositionY(),
                  field.position.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxHoldTargetAction::Execute(Event /*event*/)
{
    Unit* vezax = GetVezax(botAI);
    if (!vezax)
        return false;

    // What the bot was on, because the three cases mean different things: a vapor is hard mode about
    // to end, nothing at all is the target guard with no source behind it.
    Unit* target = AI_VALUE(Unit*, "current target");
    char const* had = "other";
    if (!target)
        had = "none";
    else if (target->GetEntry() == NPC_VEZAX_SARONITE_VAPORS)
        had = "vapor";

    RaidObs::NoteDerived(bot, "vezax.target", had);
    return Attack(vezax);
}

bool VezaxRaidPositionAction::Execute(Event /*event*/)
{
    Position slot;
    // Melee hold the boss instead of taking a slot. Don't spread them here: set behind puts them all on
    // two spots behind him, so any nudge apart fights it and they never stop walking.
    if (!TryGetVezaxSlot(bot, slot))
    {
        _slotReached = false;
        return false;
    }

    // Do not walk a bot into a spot a missile is already aimed at. The dodge node owns the bot until
    // it lands; without this the two fight each other for the whole flight and it never casts.
    Position impact;
    if (TryGetVezaxShadowCrashImpact(botAI, impact) &&
        slot.GetExactDist2d(impact.GetPositionX(), impact.GetPositionY()) <=
            ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS)
    {
        _slotReached = false;
        return false;
    }

    float const distance = bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY());
    float const tolerance = VezaxSlotTolerance(bot);

    // Reach then hold, with a deadband. Re-issuing a move on every yard of drift restarts the spline,
    // and a moving bot cannot start a cast - it slides on the spot and never casts. Yielding once
    // parked also matters because every class interrupt sits below this node at ACTION_INTERRUPT.
    if (_slotReached && distance > tolerance * 2.0f)
        _slotReached = false;

    if (_slotReached || distance <= tolerance)
    {
        _slotReached = true;
        return false;
    }

    return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
}
