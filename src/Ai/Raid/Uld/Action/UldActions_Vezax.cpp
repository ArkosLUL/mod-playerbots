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
#include "UldBossHelper.h"
#include "UldEncounter_Vezax.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

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

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool VezaxVaporPuddleClearAction::Execute(Event /*event*/)
{
    std::vector<VezaxHazard> hazards;
    GatherVezaxHazards(bot, hazards, ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS);

    std::vector<Position> avoid;
    VezaxBuildAvoidPositions(bot, hazards, avoid);
    if (avoid.empty())
        return false;

    // Not FleePosition: that clamps travel to AiPlayerbot.FleeDistance, which defaults to 5 yards and
    // cannot walk a bot out of the middle of an 8 yard puddle.
    Position const clear = FindNearestPositionClearOfHazards(
        bot, avoid, ULDUAR_VEZAX_HAZARD_CLEARANCE, ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS);
    if (clear == Position())
        return false;

    return MoveTo(bot->GetMapId(), clear.GetPositionX(), clear.GetPositionY(), clear.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool VezaxShadowCrashClearAction::Execute(Event /*event*/)
{
    Unit* boss = GetVezax(botAI);
    if (!boss)
        return false;

    float bossX = boss->GetPositionX();
    float bossY = boss->GetPositionY();
    float bossZ = boss->GetPositionZ();

    float currentAngle = atan2(bot->GetPositionY() - bossY, bot->GetPositionX() - bossX);
    float currentDistance = bot->GetDistance2d(boss);

    // Strafe at the bot's own range rather than dragging it to a fixed radius. A paladin healer can
    // answer IsMelee, which is why the melee band is still here.
    bool const stayInMelee = botAI->IsMelee(bot) || botAI->IsTank(bot);
    float const minDistance = stayInMelee ? ULDUAR_VEZAX_SHADOW_CRASH_MELEE_MIN_RANGE
                                          : ULDUAR_VEZAX_SHADOW_CRASH_RANGED_MIN_RANGE;
    float const maxDistance = stayInMelee ? ULDUAR_VEZAX_SHADOW_CRASH_MELEE_MAX_RANGE
                                          : ULDUAR_VEZAX_SHADOW_CRASH_RANGED_MAX_RANGE;
    float const desiredDistance = std::clamp(currentDistance, minDistance, maxDistance);

    // Constant step length around the boss, so a bot on a tight radius still clears the field in as
    // few ticks as one further out.
    float const angleIncrement = ULDUAR_VEZAX_SHADOW_CRASH_STEP_YARDS / std::max(desiredDistance, 1.0f);
    float newAngle = currentAngle + angleIncrement;

    float newX = bossX + desiredDistance * cos(newAngle);
    float newY = bossY + desiredDistance * sin(newAngle);

    return MoveTo(boss->GetMapId(), newX, newY, bossZ, false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT, true);
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

bool VezaxVaporSoakAction::Execute(Event /*event*/)
{
    std::vector<VezaxHazard> hazards;
    GatherVezaxHazards(bot, hazards, ULDUAR_VEZAX_VAPOR_SOAK_MAX_TRAVEL);

    VezaxHazard puddle;
    if (!TryGetVezaxNearestHazard(bot, hazards, false, puddle))
        return false;

    // Anywhere inside is enough; the aura does not care how central the bot stands.
    if (bot->GetExactDist2d(puddle.position.GetPositionX(), puddle.position.GetPositionY()) <=
        puddle.radius - 1.0f)
    {
        return false;
    }

    return MoveTo(bot->GetMapId(), puddle.position.GetPositionX(), puddle.position.GetPositionY(),
                  puddle.position.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool VezaxKillVaporAction::Execute(Event /*event*/)
{
    Unit* vapor = GetFirstAliveUnitByEntry(botAI, NPC_VEZAX_SARONITE_VAPORS);
    if (!vapor)
        return false;

    return Attack(vapor);
}

bool VezaxShadowCrashSoakAction::Execute(Event /*event*/)
{
    std::vector<VezaxHazard> hazards;
    GatherVezaxHazards(bot, hazards, ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS);

    VezaxHazard field;
    if (!TryGetVezaxNearestHazard(bot, hazards, true, field))
        return false;

    // Stop short of the centre. Arriving anywhere inside the 8 yard radius is the whole point, and
    // walking to the exact middle costs cast time for nothing.
    if (bot->GetExactDist2d(field.position.GetPositionX(), field.position.GetPositionY()) <=
        field.radius - 1.0f)
    {
        return false;
    }

    return MoveTo(bot->GetMapId(), field.position.GetPositionX(), field.position.GetPositionY(),
                  field.position.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool VezaxRaidPositionAction::Execute(Event /*event*/)
{
    std::vector<VezaxHazard> hazards;
    GatherVezaxHazards(bot, hazards, ULDUAR_VEZAX_HAZARD_SEARCH_RADIUS);

    Position slot;
    if (!TryGetVezaxSlot(bot, hazards, slot))
    {
        // Tanks and melee hold the boss instead of taking a slot. All they need is not to be stacked
        // on each other, so one Shadow Crash impact does not catch the whole group.
        _slotReached = false;
        if (Player* crowd = GetNearestPlayerInRadius(bot, ULDUAR_VEZAX_MELEE_DECLUMP_RADIUS))
            return FleePosition(crowd->GetPosition(), ULDUAR_VEZAX_MELEE_DECLUMP_RADIUS);

        return false;
    }

    float const distance = bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY());

    // Reach then hold, with a deadband. Re-issuing a move on every yard of drift restarts the spline,
    // and a moving bot cannot start a cast - it slides on the spot and never casts. Yielding once
    // parked also matters because every class interrupt sits below this node at ACTION_INTERRUPT.
    if (_slotReached && distance > ULDUAR_VEZAX_SLOT_TOLERANCE * 2.0f)
        _slotReached = false;

    if (_slotReached || distance <= ULDUAR_VEZAX_SLOT_TOLERANCE)
    {
        _slotReached = true;
        return false;
    }

    return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
}
