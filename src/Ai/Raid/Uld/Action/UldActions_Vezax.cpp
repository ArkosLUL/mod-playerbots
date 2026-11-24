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
#include "UldTargets.h"
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
void ReleaseOtherWalk(PlayerbotAI* botAI, Position const& spot)
{
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (last.lastMoveShort.GetExactDist(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()) >
        ULDUAR_VEZAX_MARK_SPOT_TOLERANCE)
        last.clear();
}

void ReleaseStalledDodge(PlayerbotAI* botAI, Player* bot)
{
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (!bot->isMoving() && last.msTime && getMSTimeDiff(last.msTime, getMSTime()) > ULDUAR_VEZAX_DODGE_STALL_MS)
        last.clear();
}
}

bool VezaxResetEncounterStateAction::Execute(Event /*event*/)
{
    ResetVezaxEncounterState(bot, IsMechanicTrackerBot(bot, ULDUAR_MAP_ID));
    return true;
}

bool VezaxMarkOfTheFacelessAction::Execute(Event /*event*/)
{
    Position spot;
    if (!TryGetVezaxMarkSpot(bot, spot))
        return false;

    Position impact;
    if (!TryGetVezaxShadowCrashImpact(botAI, impact) ||
        bot->GetExactDist2d(impact.GetPositionX(), impact.GetPositionY()) >
            ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS)
        ReleaseOtherWalk(botAI, spot);

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

    if (bot->GetExactDist2d(impact.GetPositionX(), impact.GetPositionY()) >
        ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS)
        return false;

    ReleaseStalledDodge(botAI, bot);
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
    Unit* animus = GetUldUnitByEntry(botAI, NPC_VEZAX_SARONITE_ANIMUS);
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

bool VezaxTankBuffAction::Execute(Event /*event*/)
{
    Player* holder = VezaxTankBuffTarget(botAI, spell);
    return holder && botAI->CastSpell(spell, holder);
}

bool VezaxAnimusBringBackAction::Execute(Event /*event*/)
{
    Player* mainTank = GetGroupMainTank(bot);
    if (!mainTank)
        return false;

    return MoveTo(bot->GetMapId(), mainTank->GetPositionX(), mainTank->GetPositionY(), mainTank->GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxAnimusMeleeSpotAction::Execute(Event /*event*/)
{
    Position spot;
    if (!TryGetVezaxAnimusMeleeSpot(bot, AI_VALUE(Unit*, "current target"), spot))
        return false;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxShadowCrashSoakAction::Execute(Event /*event*/)
{
    std::vector<VezaxHazard> hazards;
    GatherVezaxHazards(bot, hazards, ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS);

    VezaxHazard field;
    if (!TryGetVezaxNearestHazard(bot, hazards, field) || VezaxWalkMeetsShadowCrash(botAI, field.position))
        return false;

    if (bot->GetExactDist2d(field.position.GetPositionX(), field.position.GetPositionY()) <=
        field.radius - 1.0f)
        return false;

    return MoveTo(bot->GetMapId(), field.position.GetPositionX(), field.position.GetPositionY(),
                  field.position.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxHoldTargetAction::Execute(Event /*event*/)
{
    Unit* vezax = GetVezax(botAI);
    if (!vezax)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    char const* had = "other";
    if (!target)
        had = "none";
    else if (target->GetEntry() == NPC_VEZAX_SARONITE_VAPORS)
        had = "vapor";

    RaidObs::NoteDerived(bot, "vezax.target", had);

    bool const held = VezaxDamageHeld(botAI);
    if (!Attack(vezax, !held))
        return false;

    if (held)
        bot->AttackStop();

    return true;
}

bool VezaxDamageHoldAction::Execute(Event /*event*/)
{
    Unit* vezax = VezaxDamageHeld(botAI) ? GetVezax(botAI) : nullptr;

    for (Unit* controlled : bot->m_Controlled)
    {
        Creature* pet = controlled->ToCreature();
        if (!pet || pet->IsTotem())
            continue;

        if (!vezax)
        {
            VezaxUnhushPet(pet);
            continue;
        }

        VezaxHushPet(pet);
        if (CharmInfo* charm = pet->GetCharmInfo())
            charm->SetIsCommandAttack(false);

        if (pet->GetVictim() == vezax)
        {
            pet->AttackStop();
            pet->GetMotionMaster()->MoveFollow(bot, PET_FOLLOW_DIST, pet->GetFollowAngle());
        }
    }

    if (!vezax)
        return false;

    if (bot->GetVictim() == vezax)
        bot->AttackStop();

    for (CurrentSpellTypes const type : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL, CURRENT_AUTOREPEAT_SPELL})
        if (VezaxHarmfulCastOn(bot->GetCurrentSpell(type), vezax))
            bot->InterruptSpell(type);

    return false;
}

bool VezaxRaidPositionAction::Execute(Event /*event*/)
{
    Position slot;
    if (!TryGetVezaxSlot(bot, slot))
    {
        _slotReached = false;
        return false;
    }

    if (VezaxWalkMeetsShadowCrash(botAI, slot))
    {
        _slotReached = false;
        return false;
    }

    float const distance = bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY());
    float const tolerance = VezaxSlotTolerance(bot);

    if (_slotReached && distance > tolerance * 2.0f)
        _slotReached = false;

    if (_slotReached || distance <= tolerance)
    {
        _slotReached = true;
        return false;
    }

    if (!VezaxPlaceOnFloor(bot, slot) || bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) <= tolerance)
        return false;

    return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
}
