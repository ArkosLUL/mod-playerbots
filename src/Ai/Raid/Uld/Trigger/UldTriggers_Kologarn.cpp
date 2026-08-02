#include "UldTriggers_Kologarn.h"

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

bool KologarnMarkDpsTargetTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");
    if (!boss || !boss->IsAlive())
        return false;

    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
        return false;

    // Get current raid dps target
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    int8 skullIndex = 7;
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);

    // Check that rubble is marked
    if (currentSkullUnit && currentSkullUnit->IsAlive() && currentSkullUnit->GetEntry() == NPC_RUBBLE)
    {
        return false;  // Skull marker is already set on rubble
    }

    // Check that there is rubble to mark
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target)
            continue;

        if (target->GetEntry() == NPC_RUBBLE && target->IsAlive())
        {
            return true;  // Found a rubble to mark
        }
    }

    // Check that right arm is marked
    if (currentSkullUnit && currentSkullUnit->IsAlive() && currentSkullUnit->GetEntry() == NPC_RIGHT_ARM)
    {
        return false;  // Skull marker is already set on right arm
    }

    // Check that there is right arm to mark
    Unit* rightArm = AI_VALUE2(Unit*, "find target", "right arm");
    if (rightArm && rightArm->IsAlive())
    {
        return true;  // Found a right arm to mark
    }

    // Check that main body is marked
    if (currentSkullUnit && currentSkullUnit->IsAlive() && currentSkullUnit->GetEntry() == NPC_KOLOGARN)
    {
        return false;  // Skull marker is already set on main body
    }

    // Main body is not marked
    return true;
}

bool KologarnFallFromFloorTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if bot is on the floor
    return bot->GetPositionZ() < ULDUAR_KOLOGARN_AXIS_Z_PATHING_ISSUE_DETECT;
}

bool KologarnRubbleSlowdownTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is hunter
    if (bot->getClass() != CLASS_HUNTER)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Check that the current skull mark is set on rubble
    int8 skullIndex = 7;
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);
    if (!currentSkullUnit || !currentSkullUnit->IsAlive() || currentSkullUnit->GetEntry() != NPC_RUBBLE)
        return false;

    if (bot->HasSpellCooldown(SPELL_FROST_TRAP))
        return false;

    return true;
}

bool KologarnEyebeamTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    GuidVector triggers = AI_VALUE(GuidVector, "possible triggers");

    if (!triggers.empty())
    {
        for (ObjectGuid const guid : triggers)
        {
            if (Unit* unit = botAI->GetUnit(guid))
            {
                std::string triggerName = unit->GetNameForLocaleIdx(sWorld->GetDefaultDbcLocale());

                if (triggerName.rfind("Focused Eyebeam", 0) == 0 &&
                    bot->GetDistance2d(unit) < ULDUAR_KOLOGARN_EYEBEAM_RADIUS + 1.0f)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool KologarnAttackDpsTargetTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    // Get bot's current target
    Unit* currentTarget = botAI->GetUnit(bot->GetTarget());
    if (!currentTarget || !currentTarget->IsAlive())
        return false;

    // Get the current raid marker from the group
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid skullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);
    ObjectGuid crossTarget = group->GetTargetIcon(RtiTargetValue::crossIndex);

    if (crossTarget && (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0)))
    {
        return currentTarget->GetGUID() != crossTarget;
    }
    else
    {
        return currentTarget->GetGUID() != skullTarget;
    }
}

bool KologarnRtiTargetTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    std::string rtiMark = AI_VALUE(std::string, "rti");

    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0))
        return rtiMark != "cross";

    return rtiMark != "skull";
}

bool KologarnCrunchArmorTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    return bot->HasAura(SPELL_CRUNCH_ARMOR);
}
