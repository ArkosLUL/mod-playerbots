#include "UldActions_Algalon.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

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
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

//
// Algalon the Observer
//

bool AlgalonCosmicSmashAction::isUseful()
{
    AlgalonCosmicSmashTrigger trigger(botAI);
    return trigger.IsActive();
}

bool AlgalonCosmicSmashAction::Execute(Event /*event*/)
{
    Creature* asteroid = bot->FindNearestCreature(NPC_ALGALON_ASTEROID_TARGET_1, 11.0f);
    if (!asteroid)
        asteroid = bot->FindNearestCreature(NPC_ALGALON_ASTEROID_TARGET_2, 11.0f);

    if (!asteroid)
        return false;

    return FleePosition(asteroid->GetPosition(), 12.0f);
}

bool AlgalonBigBangHideAction::isUseful()
{
    AlgalonBigBangTrigger trigger(botAI);
    return trigger.IsActive();
}

bool AlgalonBigBangHideAction::Execute(Event /*event*/)
{
    // Run into the nearest Black Hole (or Worm Hole in phase 2) to gain the safe phase aura
    Creature* hole = bot->FindNearestCreature(PB_NPC_BLACK_HOLE, 200.0f);
    if (!hole)
        hole = bot->FindNearestCreature(PB_NPC_WORM_HOLE, 200.0f);

    if (!hole)
        return false;

    return MoveTo(hole->GetMapId(), hole->GetPositionX(), hole->GetPositionY(), hole->GetPositionZ(),
                  false, false, false, false, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool AlgalonBigBangSoakAction::isUseful()
{
    AlgalonBigBangSoakTrigger trigger(botAI);
    return trigger.IsActive();
}

bool AlgalonBigBangSoakAction::Execute(Event event)
{
    // The designated Shadow Priest stays out and pops Dispersion to survive Big Bang. Big Bang is
    // unavoidable raid-wide damage that immunity cannot prevent; Dispersion's 90% reduction survives it.
    // The cooldown is reserved for this moment by AlgalonMultiplier (blocks normal Dispersion casts).
    if (botAI->DoSpecificAction("dispersion", event, true))
        return true;

    // Dispersion unavailable (on cooldown): hide in a hole like the rest of the raid
    AlgalonBigBangHideAction hide(botAI);
    return hide.Execute(event);
}

bool AlgalonPhasePunchSwapAction::isUseful()
{
    AlgalonPhasePunchSwapTrigger trigger(botAI);
    return trigger.IsActive();
}

bool AlgalonPhasePunchSwapAction::Execute(Event event)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon the observer");
    if (!boss || !boss->IsAlive())
        return false;

    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    if (boss->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", event, true);

    return false;
}

bool AlgalonConstellationKiteAction::isUseful()
{
    AlgalonConstellationKiteTrigger trigger(botAI);
    return trigger.IsActive();
}

bool AlgalonConstellationKiteAction::Execute(Event /*event*/)
{
    Creature* constellation = bot->FindNearestCreature(PB_NPC_LIVING_CONSTELLATION, 100.0f);
    if (!constellation || !constellation->IsAlive() || constellation->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    Creature* blackHole = bot->FindNearestCreature(PB_NPC_BLACK_HOLE, 200.0f);
    if (!blackHole)
    {
        // No Black Hole to drag it into: rather than idling, lead the constellation clear of the
        // raid by moving directly away from the nearest other player so its Arcane pulses don't
        // chain across the group.
        Unit* nearest = GetNearestPlayerInRadius(bot, 30.0f);
        if (!nearest)
            return false;

        float const awayAngle = Position::NormalizeOrientation(nearest->GetAngle(bot));
        float const kiteDistance = 15.0f;
        float const fx = bot->GetPositionX() + std::cos(awayAngle) * kiteDistance;
        float const fy = bot->GetPositionY() + std::sin(awayAngle) * kiteDistance;

        return MoveTo(bot->GetMapId(), fx, fy, bot->GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_COMBAT);
    }

    // Stand just past the Black Hole on the side away from the constellation, so the chasing constellation
    // is dragged through its phase effect while we stay clear of the hole's own phase/damage aura.
    float const farAngle = Position::NormalizeOrientation(blackHole->GetAngle(constellation) + M_PI);
    float const offset = ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET;
    float const x = blackHole->GetPositionX() + std::cos(farAngle) * offset;
    float const y = blackHole->GetPositionY() + std::sin(farAngle) * offset;

    return MoveTo(blackHole->GetMapId(), x, y, blackHole->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

bool AlgalonDarkMatterMarkAction::isUseful()
{
    AlgalonDarkMatterTrigger trigger(botAI);
    return trigger.IsActive();
}

bool AlgalonDarkMatterMarkAction::Execute(Event /*event*/)
{
    Unit* darkMatter = GetFirstAliveUnitByEntry(botAI, PB_NPC_UNLEASHED_DARK_MATTER);
    if (!darkMatter)
        return false;

    MarkTargetWithSkull(bot, darkMatter);
    SetRtiTarget(botAI, "skull", darkMatter);
    return true;
}

bool AlgalonCollapsingStarMarkAction::isUseful()
{
    AlgalonCollapsingStarTrigger algalonCollapsingStarTrigger(botAI);
    return algalonCollapsingStarTrigger.IsActive();
}

bool AlgalonCollapsingStarMarkAction::Execute(Event /*event*/)
{
    Unit* star = GetFirstAliveUnitByEntry(botAI, PB_NPC_COLLAPSING_STAR);
    if (!star)
        return false;

    MarkTargetWithSkull(bot, star);
    SetRtiTarget(botAI, "skull", star);
    return true;
}
