#include "UldTriggers_Auriaya.h"

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

bool AuriayaFallFromFloorTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "auriaya");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is on the floor
    return bot->GetPositionZ() < ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT;
}

//
// Auriaya
//
bool AuriayaSonicScreechTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "auriaya");
    if (!boss || !boss->IsAlive())
        return false;

    // The tank keeps Auriaya facing away from the raid, so tanks must not reposition
    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0))
        return false;

    // Sonic Screech and Sentinel Blast are frontal cones; keep non-tanks out of the front
    return IsBotInFrontalCone(bot, boss, M_PI / 2.0f, 45.0f);
}

bool AuriayaSeepingEssenceTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "auriaya");
    if (!boss || !boss->IsAlive())
        return false;

    TooCloseToCreatureTrigger tooCloseToSeepingEssence(botAI);
    return tooCloseToSeepingEssence.TooCloseToCreature(NPC_AURIAYA_SEEPING_FERAL_ESSENCE, 10.0f);
}

bool AuriayaMarkDpsTargetTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "auriaya");
    if (!boss || !boss->IsAlive())
        return false;

    if (!IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    // Feral Defender is the highest kill priority; otherwise focus a Sanctum Sentry
    Unit* target = GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_FERAL_DEFENDER);
    if (!target)
        target = GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_SANCTUM_SENTRY);

    if (!target)
        return false;

    Group* group = bot->GetGroup();
    if (group && group->GetTargetIcon(RtiTargetValue::skullIndex) == target->GetGUID())
        return false;

    return true;
}
