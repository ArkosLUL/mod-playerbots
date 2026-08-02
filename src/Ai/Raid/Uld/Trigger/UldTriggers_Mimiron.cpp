#include "UldTriggers_Mimiron.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

bool MimironShockBlastTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "leviathan mk ii");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(SPELL_SHOCK_BLAST))
    {
        return false;
    }

    if (botAI->IsMelee(bot))
    {
        return true;
    }
    else
    {
        return bot->GetDistance2d(boss) < 15.0f;
    }
}

bool MimironPhase1PositioningTrigger::IsActive()
{
    if (!botAI->IsRanged(bot))
    {
        return false;
    }

    Unit* leviathanMkII = nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target || !target->IsAlive())
            continue;

        if (target->GetEntry() == NPC_LEVIATHAN_MKII)
            leviathanMkII = target;
        else if (target->GetEntry() == NPC_VX001)
            return false;
        else if (target->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
            return false;
    }

    if (!leviathanMkII || !leviathanMkII->IsAlive())
        return false;

    return AI_VALUE(float, "disperse distance") != 6.0f;
}

bool MimironP3Wx2LaserBarrageTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "vx-001");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    bool isCasting = boss->HasUnitState(UNIT_STATE_CASTING);
    bool isP3WX2LaserBarrage = boss->FindCurrentSpellBySpellId(SPELL_SPINNING_UP) ||
        boss->FindCurrentSpellBySpellId(SPELL_P3WX2_LASER_BARRAGE_1) ||
        boss->FindCurrentSpellBySpellId(SPELL_P3WX2_LASER_BARRAGE_2) ||
        boss->FindCurrentSpellBySpellId(SPELL_P3WX2_LASER_BARRAGE_AURA_1) ||
        boss->FindCurrentSpellBySpellId(SPELL_P3WX2_LASER_BARRAGE_AURA_2) ||
        boss->FindCurrentSpellBySpellId(SPELL_P3WX2_LASER_BARRAGE_3);
    bool hasP3WX2LaserBarrageAura =
        boss->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1) || boss->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_2);

    if ((!isCasting && !hasP3WX2LaserBarrageAura) || !isP3WX2LaserBarrage)
        return false;

    return true;
}

bool MimironRapidBurstTrigger::IsActive()
{
    Unit* leviathanMkII = nullptr;
    Unit* vx001 = nullptr;
    Unit* aerialCommandUnit = nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target || !target->IsAlive())
            continue;

        if (target->GetEntry() == NPC_LEVIATHAN_MKII)
            leviathanMkII = target;
        else if (target->GetEntry() == NPC_VX001)
            vx001 = target;
        else if (target->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
            aerialCommandUnit = target;
    }

    if (!vx001 || !vx001->IsAlive())
        return false;

    if (leviathanMkII && leviathanMkII->HasUnitState(UNIT_STATE_CASTING) &&
        leviathanMkII->FindCurrentSpellBySpellId(SPELL_SHOCK_BLAST))
        return false;

    if (botAI->IsMainTank(bot) && leviathanMkII && leviathanMkII->IsAlive() && leviathanMkII->GetVictim() != bot)
        return false;

    if (botAI->IsMelee(bot) && !botAI->IsMainTank(bot) && leviathanMkII && aerialCommandUnit)
        return false;

    MimironP3Wx2LaserBarrageTrigger mimironP3Wx2LaserBarrageTrigger(botAI);
    if (mimironP3Wx2LaserBarrageTrigger.IsActive())
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    uint32 memberSpotNumber = 0;
    Position memberPosition;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member)
            continue;

        if (bot->GetGUID() == member->GetGUID())
        {
            if (botAI->IsRanged(bot))
            {
                switch (memberSpotNumber)
                {
                    case 0:
                        memberPosition = ULDUAR_MIMIRON_PHASE2_SIDE1RANGE_SPOT;
                        break;
                    case 1:
                        memberPosition = ULDUAR_MIMIRON_PHASE2_SIDE2RANGE_SPOT;
                        break;
                    case 2:
                        memberPosition = ULDUAR_MIMIRON_PHASE2_SIDE3RANGE_SPOT;
                        break;
                    default:
                        break;
                }
            }
            else
            {
                if (botAI->IsMainTank(bot) && leviathanMkII)
                {
                    memberPosition = ULDUAR_MIMIRON_PHASE4_TANK_SPOT;
                }
                else
                {
                    switch (memberSpotNumber)
                    {
                        case 0:
                            memberPosition = ULDUAR_MIMIRON_PHASE2_SIDE1MELEE_SPOT;
                            break;
                        case 1:
                            memberPosition = ULDUAR_MIMIRON_PHASE2_SIDE2MELEE_SPOT;
                            break;
                        case 2:
                            memberPosition = ULDUAR_MIMIRON_PHASE2_SIDE3MELEE_SPOT;
                            break;
                        default:
                            break;
                    }
                }
            }
            break;
        }

        memberSpotNumber++;

        if (memberSpotNumber == 3)
            memberSpotNumber = 0;
    }

    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    float nearestRocketStrikeDistance = std::numeric_limits<float>::max();
    bool rocketStrikeDetected = false;

    for (const ObjectGuid& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        if (unit->GetEntry() == NPC_ROCKET_STRIKE_N)
        {
            rocketStrikeDetected = true;
            float distance = bot->GetDistance2d(memberPosition.GetPositionX(), memberPosition.GetPositionY());
            if (distance < nearestRocketStrikeDistance)
            {
                nearestRocketStrikeDistance = distance;
            }
        }
    }

    return (bot->GetDistance(memberPosition) > 7.0f && !rocketStrikeDetected) ||
           bot->GetDistance(memberPosition) > 20.0f;
}

bool MimironAerialCommandUnitTrigger::IsActive()
{
    Unit* leviathanMkII = nullptr;
    Unit* vx001 = nullptr;
    Unit* aerialCommandUnit = nullptr;
    Unit* assaultBot = nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target || !target->IsAlive())
            continue;

        if (target->GetEntry() == NPC_LEVIATHAN_MKII)
            leviathanMkII = target;
        else if (target->GetEntry() == NPC_VX001)
            vx001 = target;
        else if (target->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
            aerialCommandUnit = target;
        else if (target->GetEntry() == NPC_ASSAULT_BOT)
            assaultBot = target;
    }

    if (!aerialCommandUnit || !aerialCommandUnit->IsAlive() || leviathanMkII || vx001)
        return false;

    if (!botAI->IsRanged(bot) && !botAI->IsMainTank(bot) && !botAI->IsAssistTankOfIndex(bot, 0))
        return false;

    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0))
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        ObjectGuid skullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);
        ObjectGuid crossTarget = group->GetTargetIcon(RtiTargetValue::crossIndex);

        if (!crossTarget || aerialCommandUnit->GetGUID() != crossTarget)
            return true;
        else if (assaultBot && (!skullTarget || assaultBot->GetGUID() != skullTarget))
            return true;

        return false;
    }

    std::string rtiMark = AI_VALUE(std::string, "rti");
    if (rtiMark != "cross")
        return true;

    return false;
}

bool MimironRocketStrikeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "vx-001");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    Creature* rocketStrikeN = bot->FindNearestCreature(NPC_ROCKET_STRIKE_N, 100.0f);

    if (!rocketStrikeN)
        return false;

    return bot->GetDistance2d(rocketStrikeN->GetPositionX(), rocketStrikeN->GetPositionY()) <= 10.0f;
}

bool MimironPhase4MarkDpsTrigger::IsActive()
{
    Unit* leviathanMkII = nullptr;
    Unit* vx001 = nullptr;
    Unit* aerialCommandUnit = nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target || !target->IsAlive())
            continue;

        if (target->GetEntry() == NPC_LEVIATHAN_MKII)
            leviathanMkII = target;
        else if (target->GetEntry() == NPC_VX001)
            vx001 = target;
        else if (target->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
            aerialCommandUnit = target;
    }

    if (!leviathanMkII || !vx001 || !aerialCommandUnit)
        return false;

    if (botAI->IsMainTank(bot))
    {
        Unit* highestHealthUnit = nullptr;
        uint32 highestHealth = 0;

        if (leviathanMkII && leviathanMkII->GetHealth() > highestHealth)
        {
            highestHealth = leviathanMkII->GetHealth();
            highestHealthUnit = leviathanMkII;
        }
        if (vx001 && vx001->GetHealth() > highestHealth)
        {
            highestHealth = vx001->GetHealth();
            highestHealthUnit = vx001;
        }
        if (aerialCommandUnit && aerialCommandUnit->GetHealth() > highestHealth)
            highestHealthUnit = aerialCommandUnit;

        ObjectGuid skullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);
        if (!skullTarget)
            return true;

        return highestHealthUnit->GetGUID() != skullTarget;
    }
    else
        return AI_VALUE(std::string, "rti") != "skull";

    return false;
}

bool MimironCheatTrigger::IsActive()
{
    if (!botAI->HasCheat(BotCheatMask::raid))
        return false;

    if (!botAI->IsMainTank(bot))
        return false;

    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    for (const ObjectGuid& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() == NPC_PROXIMITY_MINE)
            return true;
        else if (unit->GetEntry() == NPC_BOMB_BOT)
            return true;
    }

    return false;
}

//
// Mimiron
//
bool MimironProximityMineTrigger::IsActive()
{
    TooCloseToCreatureTrigger tooCloseToProximityMine(botAI);
    return tooCloseToProximityMine.TooCloseToCreature(NPC_PROXIMITY_MINE, 6.0f);
}

bool MimironBombBotTrigger::IsActive()
{
    TooCloseToCreatureTrigger tooCloseToBombBot(botAI);
    return tooCloseToBombBot.TooCloseToCreature(NPC_BOMB_BOT, 6.0f);
}

bool MimironDodgeFlamesTrigger::IsActive()
{
    if (!IsMimironHardModeActive(botAI))
        return false;

    // The fire nodes are non-selectable trigger creatures, so they never show up in attack-target
    // lists - scan the raw nearby-npc list instead.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FLAMES_SPREAD && unit->GetEntry() != NPC_FLAMES_INITIAL)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            return true;
    }

    return false;
}

bool MimironFrostBombTrigger::IsActive()
{
    if (!IsMimironHardModeActive(botAI))
        return false;

    TooCloseToCreatureTrigger tooCloseToFrostBomb(botAI);
    return tooCloseToFrostBomb.TooCloseToCreature(NPC_FROST_BOMB, ULDUAR_MIMIRON_FROST_BOMB_RADIUS);
}
