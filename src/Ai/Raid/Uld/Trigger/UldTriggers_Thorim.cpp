#include "UldTriggers_Thorim.h"

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

bool ThorimUnbalancingStrikeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
        return false;

    if (!boss->IsAlive())
        return false;

    if (!boss->IsHostileTo(bot))
        return false;

    return bot->HasAura(SPELL_UNBALANCING_STRIKE);
}

bool ThorimMarkDpsTargetTrigger::IsActive()
{
    if (bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > 110.0f)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (botAI->IsMainTank(bot))
    {
        ObjectGuid currentSkullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);
        Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);
        if (currentSkullUnit && !currentSkullUnit->IsAlive())
        {
            currentSkullUnit = nullptr;
        }

        Unit* acolyte = AI_VALUE2(Unit*, "find target", "dark rune acolyte");
        Unit* evoker = AI_VALUE2(Unit*, "find target", "dark rune evoker");

        if (acolyte && acolyte->IsAlive() && bot->GetDistance(acolyte) < 50.0f &&
            (!currentSkullUnit || currentSkullUnit->GetEntry() != acolyte->GetEntry()))
            return true;

        if (evoker && evoker->IsAlive() && bot->GetDistance(evoker) < 50.0f &&
            (!currentSkullUnit || currentSkullUnit->GetEntry() != evoker->GetEntry()))
            return true;

        Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");

        if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
            return false;

        if (!boss->IsAlive())
            return false;

        if (!boss->IsHostileTo(bot))
            return false;

        if (boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD && (!currentSkullUnit || !currentSkullUnit->IsAlive()))
        {
            group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), boss->GetGUID());
            return true;
        }

        return false;
    }
    else if (botAI->IsAssistTankOfIndex(bot, 0))
    {
        Player* mainTank = nullptr;
        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            if (member && botAI->IsMainTank(member))
            {
                mainTank = member;
                break;
            }
        }

        if (mainTank && bot->GetDistance(mainTank) < 30.0f)
            return false;

        ObjectGuid currentCrossTarget = group->GetTargetIcon(RtiTargetValue::crossIndex);
        Unit* currentCrossUnit = botAI->GetUnit(currentCrossTarget);
        if (currentCrossUnit && !currentCrossUnit->IsAlive())
        {
            currentCrossUnit = nullptr;
        }

        Unit* acolyte = AI_VALUE2(Unit*, "find target", "dark rune acolyte");
        if (currentCrossUnit && currentCrossUnit->GetEntry() == NPC_DARK_RUNE_ACOLYTE_I)
            return false;

        Unit* runicColossus = AI_VALUE2(Unit*, "find target", "runic colossus");
        Unit* ancientRuneGiant = AI_VALUE2(Unit*, "find target", "ancient rune giant");

        if (acolyte && acolyte->IsAlive() && (!currentCrossUnit || currentCrossUnit->GetEntry() != acolyte->GetEntry()))
            return true;

        if (currentCrossUnit && currentCrossUnit->GetEntry() == NPC_RUNIC_COLOSSUS)
            return false;
        if (runicColossus && runicColossus->IsAlive() &&
            (!currentCrossUnit || currentCrossUnit->GetEntry() != runicColossus->GetEntry()))
            return true;

        if (currentCrossUnit && currentCrossUnit->GetEntry() == NPC_ANCIENT_RUNE_GIANT)
            return false;
        if (ancientRuneGiant && ancientRuneGiant->IsAlive() &&
            (!currentCrossUnit || currentCrossUnit->GetEntry() != ancientRuneGiant->GetEntry()))
            return true;

        return false;
    }

    return false;
}

bool ThorimGauntletPositioningTrigger::IsActive()
{
    if (bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > 110.0f)
        return false;

    Difficulty raidDifficulty = bot->GetRaidDifficulty();

    Group* group = bot->GetGroup();
    if (!group)
        return false;
    uint32 requiredAssistTankQuantity = 1;
    uint32 requiredHealerQuantity = 0;
    uint32 requiredDpsQuantity = 0;

    if (raidDifficulty == Difficulty::RAID_DIFFICULTY_10MAN_NORMAL)
    {
        requiredDpsQuantity = 3;
        requiredHealerQuantity = 1;
    }
    else if (raidDifficulty == Difficulty::RAID_DIFFICULTY_25MAN_NORMAL)
    {
        requiredDpsQuantity = 7;
        requiredHealerQuantity = 2;
    }

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member)
            continue;

        if (requiredDpsQuantity > 0 && botAI->IsDps(member))
        {
            requiredDpsQuantity--;
            if (bot->GetGUID() == member->GetGUID())
                break;
        }

        if (requiredAssistTankQuantity > 0 && botAI->IsAssistTankOfIndex(member, 0))
        {
            requiredAssistTankQuantity--;
            if (bot->GetGUID() == member->GetGUID())
                break;
        }

        if (requiredHealerQuantity > 0 && botAI->IsHeal(member))
        {
            requiredHealerQuantity--;
            if (bot->GetGUID() == member->GetGUID())
                break;
        }

        if (requiredDpsQuantity == 0 && requiredAssistTankQuantity == 0 && requiredHealerQuantity == 0)
            return false;
    }

    Unit* master = botAI->GetMaster();
    if (master->GetDistance(ULDUAR_THORIM_NEAR_ENTRANCE_POSITION) < 10.0f && (bot->GetDistance2d(master) > 5.0f))
    {
        return true;
    }

    if ((master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1) < 6.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2) < 6.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1) < 5.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1) < 10.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2) < 10.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3) < 10.0f) &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1) > 6.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2) > 6.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1) > 5.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1) > 10.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2) > 10.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3) > 10.0f)
    {
        if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
            return false;

        return true;
    }

    if ((master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1) < 6.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2) < 6.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1) < 5.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1) < 10.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2) < 10.0f ||
         master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3) < 10.0f) &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1) > 6.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2) > 6.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1) > 5.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1) > 10.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2) > 10.0f &&
        bot->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3) > 10.0f)
    {
        if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
            return false;

        return true;
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (boss && boss->IsAlive() && bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD &&
        boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
    {
        return true;
    }

    return false;
}

bool ThorimArenaPositioningTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
        return false;

    if (!boss->IsAlive())
        return false;

    if (!boss->IsHostileTo(bot))
        return false;

    if (boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    Difficulty raidDifficulty = bot->GetRaidDifficulty();

    Group* group = bot->GetGroup();
    if (!group)
        return false;
    uint32 requiredAssistTankQuantity = 1;
    uint32 requiredHealerQuantity = 0;
    uint32 requiredDpsQuantity = 0;

    if (raidDifficulty == Difficulty::RAID_DIFFICULTY_10MAN_NORMAL)
    {
        requiredDpsQuantity = 3;
        requiredHealerQuantity = 1;
    }
    else if (raidDifficulty == Difficulty::RAID_DIFFICULTY_25MAN_NORMAL)
    {
        requiredDpsQuantity = 7;
        requiredHealerQuantity = 2;
    }

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member)
            continue;

        if (requiredDpsQuantity > 0 && botAI->IsDps(member))
        {
            requiredDpsQuantity--;
            if (bot->GetGUID() == member->GetGUID())
                return false;
        }

        if (requiredAssistTankQuantity > 0 && botAI->IsAssistTankOfIndex(member, 0))
        {
            requiredAssistTankQuantity--;
            if (bot->GetGUID() == member->GetGUID())
                return false;
        }

        if (requiredHealerQuantity > 0 && botAI->IsHeal(member))
        {
            requiredHealerQuantity--;
            if (bot->GetGUID() == member->GetGUID())
                return false;
        }

        if (requiredDpsQuantity == 0 && requiredAssistTankQuantity == 0 && requiredHealerQuantity == 0)
            break;
    }

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target || !target->IsAlive())
            continue;

        uint32 entry = target->GetEntry();

        if (entry == NPC_DARK_RUNE_ACOLYTE_I || entry == NPC_CAPTURED_MERCENARY_SOLDIER_ALLY ||
            entry == NPC_CAPTURED_MERCENARY_SOLDIER_HORDE || entry == NPC_CAPTURED_MERCENARY_CAPTAIN_ALLY ||
            entry == NPC_CAPTURED_MERCENARY_CAPTAIN_HORDE || entry == NPC_JORMUNGAR_BEHEMOT ||
            entry == NPC_DARK_RUNE_WARBRINGER || entry == NPC_DARK_RUNE_EVOKER || entry == NPC_DARK_RUNE_CHAMPION ||
            entry == NPC_DARK_RUNE_COMMONER)
            return false;
    }

    if (bot && bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > 5.0f)
        return true;

    return false;
}

bool ThorimFallFromFloorTrigger::IsActive()
{
    if (bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > 110.0f)
        return false;

    // Check if bot is on the floor
    return bot->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_PATHING_ISSUE_DETECT;
}

bool ThorimPhase2PositioningTrigger::IsActive()
{
    if (!botAI->IsRanged(bot) && !botAI->IsMainTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
        return false;

    if (!boss->IsAlive())
        return false;

    if (!boss->IsHostileTo(bot))
        return false;

    if (boss->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    if (botAI->IsMainTank(bot))
    {
        if (bot->GetDistance(ULDUAR_THORIM_PHASE2_TANK_SPOT) > 1.0f && boss->GetVictim() == bot)
            return true;

        return false;
    }

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    uint32 memberPositionNumber = 0;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member)
            continue;

        if (botAI->IsRanged(member))
        {
            if (bot->GetGUID() == member->GetGUID())
                break;

            memberPositionNumber++;

            if (memberPositionNumber == 3)
                memberPositionNumber = 0;
        }
    }

    if (memberPositionNumber == 0 && bot->GetDistance(ULDUAR_THORIM_PHASE2_RANGE1_SPOT) > 1.0f)
        return true;

    if (memberPositionNumber == 1 && bot->GetDistance(ULDUAR_THORIM_PHASE2_RANGE2_SPOT) > 1.0f)
        return true;

    if (memberPositionNumber == 2 && bot->GetDistance(ULDUAR_THORIM_PHASE2_RANGE3_SPOT) > 1.0f)
        return true;

    return false;
}

//
// Thorim
//
bool ThorimUnbalancingStrikeSwapTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
        return false;

    if (!boss->IsAlive() || !boss->IsHostileTo(bot))
        return false;

    bool const isMainTank = botAI->IsMainTank(bot);
    bool const isFirstAssistTank = botAI->IsAssistTankOfIndex(bot, 0);
    if (!isMainTank && !isFirstAssistTank)
        return false;

    // This bot must be the off-tank, not the one currently holding the boss
    Unit* activeTank = boss->GetVictim();
    if (!activeTank || activeTank == bot)
        return false;

    Player* activeTankPlayer = activeTank->ToPlayer();
    if (!activeTankPlayer)
        return false;

    // The active tank must be this bot's swap partner so the taunt is symmetric both ways
    bool const partnerIsSwapTank = isMainTank ? PlayerbotAI::IsAssistTankOfIndex(activeTankPlayer, 0)
                                              : PlayerbotAI::IsMainTank(activeTankPlayer);
    if (!partnerIsSwapTank)
        return false;

    // Don't taunt while this bot still carries Unbalancing Strike (let it fall off first)
    if (bot->HasAura(SPELL_UNBALANCING_STRIKE))
        return false;

    // Swap in once the active tank is suffering Unbalancing Strike
    return activeTank->HasAura(SPELL_UNBALANCING_STRIKE);
}
