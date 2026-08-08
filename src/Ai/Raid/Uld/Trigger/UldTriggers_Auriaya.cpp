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

#include <cmath>

bool AuriayaFallFromFloorTrigger::IsActive()
{
    if (!AuriayaEncounterActive(botAI))
        return false;

    // Check if bot is on the floor
    return bot->GetPositionZ() < ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT;
}

//
// Auriaya
//
bool AuriayaSonicScreechTrigger::IsActive()
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    // Whoever Auriaya is facing holds the front so the cone points away from everyone else, and must
    // not step out. Gated on her actual victim rather than the tank role: the off-tank spends the
    // fight chasing Sanctum Sentries around the room and gets clipped like anyone else.
    if (boss->GetVictim() == bot)
        return false;

    // Sonic Screech is the frontal cone. Sentinel Blast is raid-wide and has no dodge, so nothing
    // here should ever grow a second arc for it.
    return IsBotInFrontalCone(bot, boss, ULDUAR_AURIAYA_SONIC_SCREECH_CONE, ULDUAR_AURIAYA_SONIC_SCREECH_RANGE);
}

bool AuriayaSeepingEssenceTrigger::IsActive()
{
    if (!AuriayaEncounterActive(botAI))
        return false;

    TooCloseToCreatureTrigger tooCloseToSeepingEssence(botAI);
    return tooCloseToSeepingEssence.TooCloseToCreature(NPC_AURIAYA_SEEPING_FERAL_ESSENCE,
                                                      ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS);
}

bool AuriayaMarkDpsTargetTrigger::IsActive()
{
    if (!AuriayaEncounterActive(botAI))
        return false;

    if (!IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    Unit* target = GetAuriayaFocusTarget(botAI);
    if (!target)
        return false;

    Group* group = bot->GetGroup();
    if (group && group->GetTargetIcon(RtiTargetValue::skullIndex) == target->GetGUID())
        return false;

    return true;
}

bool AuriayaAttackDpsTargetTrigger::IsActive()
{
    if (!AuriayaEncounterActive(botAI))
        return false;

    // The main tank stays on Auriaya; everyone else swaps onto whatever the skull is on
    if (botAI->IsMainTank(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Unit* marked = botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::skullIndex));
    if (!marked || IsDownOrFeigning(marked))
        return false;

    uint32 const entry = marked->GetEntry();
    if (entry != NPC_AURIAYA_SANCTUM_SENTRY && entry != NPC_AURIAYA_FERAL_DEFENDER)
        return false;

    return bot->GetTarget() != marked->GetGUID();
}

bool AuriayaSentryTauntTrigger::IsActive()
{
    if (!AuriayaEncounterActive(botAI))
        return false;

    if (!botAI->IsAssistTankOfIndex(bot, 0, true))
        return false;

    // A sentry already on the off-tank stays in melee, where its Savage Pounce (8-25 yd) cannot fire
    return GetAuriayaLooseSentry(botAI, bot) != nullptr;
}

bool AuriayaTankFacingTrigger::IsActive()
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss || !botAI->IsMainTank(bot))
        return false;

    // Only the bot Auriaya is actually chasing can steer where she points
    if (boss->GetVictim() != bot)
        return false;

    float error = 0.0f;
    if (!GetAuriayaFacingError(botAI, bot, error))
        return false;

    // Hold still inside the tolerance: a tank that steps every tick never lands a cast
    return std::abs(error) > ULDUAR_AURIAYA_FACING_TOLERANCE;
}
