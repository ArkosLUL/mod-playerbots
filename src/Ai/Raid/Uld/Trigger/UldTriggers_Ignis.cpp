#include "UldTriggers_Ignis.h"

#include "GameObject.h"
#include "Group.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

//
// Ignis the Furnace Master
//
bool IgnisScorchedGroundTrigger::IsActive()
{
    Unit* boss = GetIgnis(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    TooCloseToCreatureTrigger tooCloseToScorchedGround(botAI);
    return tooCloseToScorchedGround.TooCloseToCreature(NPC_IGNIS_SCORCHED_GROUND,
                                                       ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS);
}

bool IgnisConstructTankTrigger::IsActive()
{
    Unit* boss = GetIgnis(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    if (GetIgnisConstructTank(botAI, bot) != bot)
        return false;

    return GetIgnisDrivenConstruct(botAI, bot) != nullptr;
}

bool IgnisBrittleConstructMarkTrigger::IsActive()
{
    Unit* boss = GetIgnis(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    if (!IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid const skull = group->GetTargetIcon(RtiTargetValue::skullIndex);

    // The mark only leaves Ignis for the Brittle window - that construct dies to a single hit, so the
    // raid swaps for one global and comes straight back.
    if (Unit* brittle = GetIgnisBrittleConstruct(botAI))
        return skull != brittle->GetGUID();

    return skull != boss->GetGUID();
}

bool IgnisAttackBrittleConstructTrigger::IsActive()
{
    Unit* boss = GetIgnis(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    // Tanks stay on what they are holding: pulling the main tank off Ignis or the construct tank off
    // the next construct costs far more than the one hit it takes to shatter a Brittle one.
    if (botAI->IsTank(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // This drives "attack rti target", so it has to key off the skull itself. Keying off the nearest
    // Brittle construct instead would never clear whenever the two disagree - no mechanic tracker in
    // the raid, or a second construct shattering closer to this bot than the marked one.
    Unit* marked = botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::skullIndex));
    if (!IsIgnisConstructActivated(marked) || !IsIgnisConstructBrittle(marked))
        return false;

    return AI_VALUE(Unit*, "current target") != marked;
}

bool IgnisMoltenConstructAvoidTrigger::IsActive()
{
    Unit* boss = GetIgnis(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    if (GetIgnisConstructTank(botAI, bot) == bot)
        return false;

    // Ignis' own tank stays put too. He is melee-range of a boss that follows him, so running out of
    // a construct's aura drags Ignis (and his Flame Jets) straight through the raid behind him.
    if (boss->GetVictim() == bot)
        return false;

    Unit* molten = GetIgnisNearestMoltenConstruct(botAI, bot);

    return molten && bot->GetExactDist2d(molten) <= ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS;
}

bool IgnisSlagPotHealTrigger::IsActive()
{
    Unit* boss = GetIgnis(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    if (!botAI->IsHeal(bot))
        return false;

    // Sits above every other heal, so it has to wait for the ticks to open a gap - otherwise the
    // whole healing team spends the ten seconds topping off a victim who is still at full health
    // while the tank takes Flame Jets unhealed.
    Player* victim = GetIgnisSlagPotVictim(botAI);

    return victim && victim->GetHealthPct() < ULDUAR_IGNIS_SLAG_POT_HEAL_HP_PCT;
}
