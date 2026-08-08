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

    Unit* brittle = GetIgnisBrittleConstruct(botAI);
    if (!brittle)
        return false;

    return AI_VALUE(Unit*, "current target") != brittle;
}

bool IgnisMoltenConstructAvoidTrigger::IsActive()
{
    Unit* boss = GetIgnis(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    if (GetIgnisConstructTank(botAI, bot) == bot)
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

    return GetIgnisSlagPotVictim(botAI) != nullptr;
}
