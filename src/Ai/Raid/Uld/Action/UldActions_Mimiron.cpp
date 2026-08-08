#include "UldActions_Mimiron.h"
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
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

bool MimironShockBlastAction::Execute(Event /*event*/)
{
    Unit* leviathanMkII = nullptr;
    Unit* vx001 = nullptr;
    Unit* aerialCommandUnit = nullptr;

    float radius = 20.0f;

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

    if (!leviathanMkII)
        return false;

    if (!vx001 && !aerialCommandUnit)
    {
        float currentDistance = bot->GetDistance2d(leviathanMkII);

        MoveAway(leviathanMkII, radius - currentDistance);

        if (botAI->IsMelee(bot))
            botAI->SetNextCheckDelay(100);

        return true;
    }
    else
    {
        float init_angle = leviathanMkII->GetAngle(bot);
        float distance = radius - bot->GetDistance2d(leviathanMkII);
        for (float delta = 0; delta <= M_PI / 2; delta += M_PI / 8)
        {
            float angle = init_angle + delta;
            float dx = bot->GetPositionX() + cos(angle) * distance;
            float dy = bot->GetPositionY() + sin(angle) * distance;
            float dz = bot->GetPositionZ();
            if (bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                               bot->GetPositionZ(), dx, dy, dz))
            {
                bot->TeleportTo(target->GetMapId(), dx, dy, dz, target->GetOrientation());
                return true;
            }
        }
        return false;
    }
}

bool MimironShockBlastAction::isUseful()
{
    MimironShockBlastTrigger mimironShockBlastTrigger(botAI);
    return mimironShockBlastTrigger.IsActive();
}

bool MimironPhase1PositioningAction::Execute(Event /*event*/)
{
    SET_AI_VALUE(float, "disperse distance", 6.0f);
    return true;
}

bool MimironPhase1PositioningAction::isUseful()
{
    MimironPhase1PositioningTrigger mimironPhase1PositioningTrigger(botAI);
    return mimironPhase1PositioningTrigger.IsActive();
}

bool MimironP3Wx2LaserBarrageAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "vx-001");
    if (!boss || !boss->IsAlive())
    {
        // No boss to read a facing from - fall back to huddling on the master.
        Player* master = botAI->GetMaster();
        if (!master || !master->IsAlive())
            return false;

        return MoveTo(master->GetMapId(), master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(),
                      false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
    }

    // The beams follow VX-001's facing and the barrage sweeps it clockwise (#26917), so the arc it
    // has just left - a little counterclockwise of where it points now - is the one to trail.
    float const safeAngle = Position::NormalizeOrientation(boss->GetOrientation() + delta_angle);
    float const radius = std::clamp(bot->GetDistance2d(boss), ULDUAR_MIMIRON_BARRAGE_MIN_RADIUS, distance);

    float const x = boss->GetPositionX() + radius * cos(safeAngle);
    float const y = boss->GetPositionY() + radius * sin(safeAngle);

    return MoveTo(boss->GetMapId(), x, y, boss->GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_FORCED, true);
}

bool MimironRapidBurstAction::isUseful()
{
    MimironRapidBurstTrigger mimironRapidBurstTrigger(botAI);
    return mimironRapidBurstTrigger.IsActive();
}

bool MimironRapidBurstAction::Execute(Event /*event*/)
{
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
    }

    Position targetPosition;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    uint32 memberSpotNumber = 0;
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
                        targetPosition = ULDUAR_MIMIRON_PHASE2_SIDE1RANGE_SPOT;
                        break;
                    case 1:
                        targetPosition = ULDUAR_MIMIRON_PHASE2_SIDE2RANGE_SPOT;
                        break;
                    case 2:
                        targetPosition = ULDUAR_MIMIRON_PHASE2_SIDE3RANGE_SPOT;
                        break;
                    default:
                        break;
                }
            }
            else if (botAI->IsMainTank(bot) && leviathanMkII)
            {
                targetPosition = ULDUAR_MIMIRON_PHASE4_TANK_SPOT;
            }
            else
            {
                switch (memberSpotNumber)
                {
                    case 0:
                        targetPosition = ULDUAR_MIMIRON_PHASE2_SIDE1MELEE_SPOT;
                        break;
                    case 1:
                        targetPosition = ULDUAR_MIMIRON_PHASE2_SIDE2MELEE_SPOT;
                        break;
                    case 2:
                        targetPosition = ULDUAR_MIMIRON_PHASE2_SIDE3MELEE_SPOT;
                        break;
                    default:
                        break;
                }
            }
        }

        memberSpotNumber++;

        if (memberSpotNumber == 3)
            memberSpotNumber = 0;
    }

    MoveTo(bot->GetMapId(), targetPosition.GetPositionX(), targetPosition.GetPositionY(), targetPosition.GetPositionZ(),
           false, false, false, true, MovementPriority::MOVEMENT_FORCED, true, false);

    if (AI_VALUE(float, "disperse distance") != 0.0f)
        SET_AI_VALUE(float, "disperse distance", 0.0f);

    TankFaceStrategy tankFaceStrategy(botAI);
    if (botAI->HasStrategy(tankFaceStrategy.getName(), BotState::BOT_STATE_COMBAT))
        botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + tankFaceStrategy.getName(), BotState::BOT_STATE_COMBAT);

    if (botAI->HasStrategy(tankFaceStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
        botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + tankFaceStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);

    if (bot->GetDistance(targetPosition) > 1.0f)
        return false;

    return true;
}

bool MimironAerialCommandUnitAction::Execute(Event /*event*/)
{
    Unit* boss = nullptr;
    Unit* bombBot = nullptr;
    Unit* assaultBot = nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target || !target->IsAlive())
            continue;

        if (target->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
            boss = target;
        else if (target->GetEntry() == NPC_BOMB_BOT)
            bombBot = target;
        else if (target->GetEntry() == NPC_ASSAULT_BOT)
            assaultBot = target;
    }

    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0))
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        if (bombBot)
            group->SetTargetIcon(RtiTargetValue::crossIndex, bot->GetGUID(), bombBot->GetGUID());
        else if (boss)
            group->SetTargetIcon(RtiTargetValue::crossIndex, bot->GetGUID(), boss->GetGUID());

        if (assaultBot)
        {
            ObjectGuid skullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);
            Unit* skullUnit = botAI->GetUnit(skullTarget);
            if (!skullTarget || !skullUnit || !skullUnit->IsAlive())
                group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), assaultBot->GetGUID());
        }

        return true;
    }

    if (AI_VALUE(float, "disperse distance") != 5.0f)
        SET_AI_VALUE(float, "disperse distance", 5.0f);

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("cross");
    return true;
}

bool MimironRocketStrikeAction::isUseful()
{
    MimironRocketStrikeTrigger mimironRocketStrikeTrigger(botAI);
    return mimironRocketStrikeTrigger.IsActive();
}

bool MimironRocketStrikeAction::Execute(Event /*event*/)
{
    Unit* vx001 = nullptr;
    Unit* aerialCommandUnit = nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target || !target->IsAlive())
            continue;

        if (target->GetEntry() == NPC_VX001)
            vx001 = target;
        else if (target->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
            aerialCommandUnit = target;
    }

    Creature* rocketStrikeN = bot->FindNearestCreature(NPC_ROCKET_STRIKE_N, 100.0f);

    if (!rocketStrikeN)
        return false;

    if (!vx001 && !aerialCommandUnit)
    {
        MoveAway(rocketStrikeN, 10.0f);
        return true;
    }
    else
    {
        float init_angle = rocketStrikeN->GetAngle(bot);
        float distance = 10.0f - bot->GetDistance2d(rocketStrikeN);
        for (float delta = 0; delta <= M_PI / 2; delta += M_PI / 8)
        {
            float angle = init_angle + delta;
            float dx = bot->GetPositionX() + cos(angle) * distance;
            float dy = bot->GetPositionY() + sin(angle) * distance;
            float dz = bot->GetPositionZ();
            if (bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                               bot->GetPositionZ(), dx, dy, dz))
            {
                bot->TeleportTo(target->GetMapId(), dx, dy, dz, target->GetOrientation());
                return true;
            }
        }
        return false;
    }
}

bool MimironPhase4MarkDpsAction::Execute(Event /*event*/)
{
    Unit* leviathanMkII = nullptr;
    Unit* vx001 = nullptr;
    Unit* aerialCommandUnit = nullptr;

    Group* group = bot->GetGroup();
    if (!group)
    {
        return false;
    }

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

        group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), highestHealthUnit->GetGUID());
        if (highestHealthUnit == leviathanMkII)
        {
            if (AI_VALUE(std::string, "rti") == "skull")
                botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("skull");
        }
        else
        {
            group->SetTargetIcon(RtiTargetValue::crossIndex, bot->GetGUID(), leviathanMkII->GetGUID());
            if (AI_VALUE(std::string, "rti") != "cross")
                botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("cross");
        }

        botAI->DoSpecificAction("attack rti target");
        return true;
    }
    else
    {
        /*if (AI_VALUE(float, "disperse distance") != 0.0f)
        {
            SET_AI_VALUE(float, "disperse distance", 0.0f);
        }*/
        botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("skull");
        return true;
    }
}

bool MimironCheatAction::Execute(Event /*event*/)
{
    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    for (const ObjectGuid& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() == NPC_PROXIMITY_MINE)
            unit->Kill(bot, unit);
        else if (unit->GetEntry() == NPC_BOMB_BOT)
            unit->Kill(bot, unit);
    }

    return true;
}

bool MimironDodgeFlamesAction::isUseful()
{
    MimironDodgeFlamesTrigger mimironDodgeFlamesTrigger(botAI);
    return mimironDodgeFlamesTrigger.IsActive();
}

bool MimironDodgeFlamesAction::Execute(Event /*event*/)
{
    // Fire nodes are non-selectable, so find them via the raw nearby-npc list. Flee from the centre of the
    // whole in-range fire field (not just the nearest node) out past its edge, so the bot leaves the field
    // instead of stepping out of one node straight into the next.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    std::vector<Position> nodes;

    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FLAMES_SPREAD && unit->GetEntry() != NPC_FLAMES_INITIAL)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            nodes.push_back(unit->GetPosition());
    }

    if (nodes.empty())
        return false;

    float cx = 0.0f, cy = 0.0f;
    for (Position const& node : nodes)
    {
        cx += node.GetPositionX();
        cy += node.GetPositionY();
    }
    cx /= nodes.size();
    cy /= nodes.size();

    // Flee far enough to clear the outermost in-range node, not just the centre.
    Position const centre(cx, cy, 0.0f);
    float spread = 0.0f;
    for (Position const& node : nodes)
    {
        float const d = centre.GetExactDist2d(node.GetPositionX(), node.GetPositionY());
        if (d > spread)
            spread = d;
    }

    return FleePosition(Position(cx, cy, bot->GetPositionZ()), ULDUAR_MIMIRON_FLAMES_RADIUS + spread + 1.0f);
}

bool MimironFrostBombAction::isUseful()
{
    MimironFrostBombTrigger mimironFrostBombTrigger(botAI);
    return mimironFrostBombTrigger.IsActive();
}
