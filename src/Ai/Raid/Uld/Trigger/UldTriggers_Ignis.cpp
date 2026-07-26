#include "UldTriggers_Ignis.h"

#include "GameObject.h"
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
    Unit* boss = AI_VALUE2(Unit*, "find target", "ignis the furnace master");
    if (!boss || !boss->IsAlive())
        return false;

    TooCloseToCreatureTrigger tooCloseToScorchedGround(botAI);
    return tooCloseToScorchedGround.TooCloseToCreature(NPC_IGNIS_SCORCHED_GROUND, 8.0f);
}

bool IgnisIronConstructTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ignis the furnace master");
    if (!boss || !boss->IsAlive())
        return false;

    if (!IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    // Only an activated construct is selectable and worth focusing; passive ones are skipped
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_IGNIS_IRON_CONSTRUCT)
            continue;

        if (unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            continue;

        Group* group = bot->GetGroup();
        if (group && group->GetTargetIcon(RtiTargetValue::skullIndex) == unit->GetGUID())
            return false;

        return true;
    }

    return false;
}
