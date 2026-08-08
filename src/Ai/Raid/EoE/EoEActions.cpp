/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEActions.h"
#include "EoETriggers.h"
#include "Playerbots.h"

bool MalygosPositionAction::Execute(Event /*event*/)
{
    uint8 phase = MalygosTrigger::getPhase(bot);

    float distance = 5.0f;

    if (phase == 1)
    {
        Unit* spark = nullptr;

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (unit && unit->GetEntry() == NPC_POWER_SPARK)
            {
                spark = unit;
                break;
            }
        }

        // Position tank
        if (botAI->IsMainTank(bot))
        {
            if (bot->GetDistance2d(MALYGOS_MAINTANK_POSITION.first, MALYGOS_MAINTANK_POSITION.second) > distance)
            {
                return MoveTo(EOE_MAP_ID, MALYGOS_MAINTANK_POSITION.first, MALYGOS_MAINTANK_POSITION.second, bot->GetPositionZ(),
                    false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            return false;
        }
        // DK handles the spark via death grip; leave the rest of the raid stacked.
        else if (spark && bot->IsClass(CLASS_DEATH_KNIGHT))
        {
            return false;
        }
        else if (spark)
        {
            return false;
        }
        else if (!bot->IsClass(CLASS_HUNTER))
        {
            if (bot->GetDistance2d(MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second) > (distance * 3.0f))
            {
                return MoveTo(EOE_MAP_ID, MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second, bot->GetPositionZ(),
                    false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            return false;
        }
    }
    else if (phase == 2 || phase == 4)
    {
        // Anti-fall: the platform edge drops into the void. Keep everyone inside a safe
        // interior ring around the centre; also gathers the raid for the P3 drake mount.
        float const cx = MALYGOS_CENTER_POSITION.first;
        float const cy = MALYGOS_CENTER_POSITION.second;
        float const safeRadius = 30.0f;

        float dist = bot->GetDistance2d(cx, cy);
        if (dist > safeRadius)
        {
            float target = safeRadius - 3.0f;
            float tx = cx;
            float ty = cy;
            if (dist > 0.01f)
            {
                tx = cx + (bot->GetPositionX() - cx) / dist * target;
                ty = cy + (bot->GetPositionY() - cy) / dist * target;
            }
            return MoveTo(EOE_MAP_ID, tx, ty, bot->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
        return false;
    }

    return false;
}

bool MalygosTargetAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    uint8 phase = MalygosTrigger::getPhase(bot);

    if (phase == 1)
    {
        if (botAI->IsHeal(bot)) { return false; }
        if (!boss) { return false; }

        // Fall back to Malygos unless a spark should be picked up by ranged DPS.
        Unit* newTarget = boss;
        Unit* spark = nullptr;

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (unit && unit->GetEntry() == NPC_POWER_SPARK)
            {
                spark = unit;
                break;
            }
        }

        if (spark && botAI->IsRangedDps(bot))
        {
            newTarget = spark;
        }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        if (!currentTarget || currentTarget->GetGUID() != newTarget->GetGUID())
        {
            return Attack(newTarget);
        }
    }
    else if (phase == 2)
    {
        if (botAI->IsHeal(bot)) { return false; }

        Unit* newTarget = nullptr;
        Unit* nexusLord = nullptr;
        Unit* scionOfEternity = nullptr;

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (!unit) { continue; }

            if (unit->GetEntry() == NPC_NEXUS_LORD)
            {
                nexusLord = unit;
            }
            else if (unit->GetEntry() == NPC_SCION_OF_ETERNITY)
            {
                scionOfEternity = unit;
            }
        }

        if (botAI->IsRangedDps(bot) && scionOfEternity)
        {
            newTarget = scionOfEternity;
        }
        else
        {
            newTarget = nexusLord;
        }

        if (!newTarget) { return false; }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        if (!currentTarget || currentTarget->GetEntry() != newTarget->GetEntry())
        {
            return Attack(newTarget);
        }
    }

    return false;
}

Unit* PullPowerSparkAction::GetSpark()
{
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && unit->GetEntry() == NPC_POWER_SPARK)
        {
            return unit;
        }
    }
    return nullptr;
}

bool PullPowerSparkAction::isUseful()
{
    if (!bot->IsClass(CLASS_DEATH_KNIGHT)) { return false; }
    Unit* spark = GetSpark();
    if (!spark) { return false; }
    return botAI->CanCastSpell("death grip", spark);
}

bool PullPowerSparkAction::Execute(Event /*event*/)
{
    Unit* spark = GetSpark();
    if (!spark) { return false; }

    // Grip it away from Malygos so it never reaches him and hands him the buff.
    if (spark->GetDistance2d(MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second) > 3.0f)
    {
        return botAI->CastSpell("death grip", spark);
    }

    return false;
}

bool KillPowerSparkAction::isUseful()
{
    // Only ranged DPS peel onto sparks (matching MalygosTargetAction); tanks and melee stay on
    // Malygos so his threat and Arcane Breath cone don't swing into the raid. DK grips handle the
    // spark separately via PullPowerSparkAction. Hunters stay on the boss.
    if (!botAI->IsRangedDps(bot)) { return false; }
    if (bot->IsClass(CLASS_HUNTER)) { return false; }

    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && unit->GetEntry() == NPC_POWER_SPARK)
        {
            return true;
        }
    }
    return false;
}

bool KillPowerSparkAction::Execute(Event /*event*/)
{
    Unit* spark = nullptr;
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && unit->GetEntry() == NPC_POWER_SPARK)
        {
            spark = unit;
            break;
        }
    }
    if (!spark) { return false; }

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (!currentTarget || currentTarget->GetGUID() != spark->GetGUID())
    {
        return Attack(spark);
    }
    return false;
}

bool DeepBreathDodgeAction::Execute(Event /*event*/)
{
    float radius = 8.0f;
    float extraDistance = 2.0f;

    // Arcane Overload is a non-attackable summoned hazard, so it never shows up in "nearest hostile
    // npcs"; scan for the creature directly, same as DeepBreathTrigger.
    Creature* closest = bot->FindNearestCreature(NPC_ARCANE_OVERLOAD, radius + extraDistance + 4.0f, true);
    if (closest && bot->GetExactDist2d(closest) < radius + extraDistance)
    {
        return MoveAway(closest, fmin(4.0f, radius + extraDistance - bot->GetExactDist2d(closest)));
    }

    return false;
}

bool AvoidSurgeOfPowerAction::Execute(Event /*event*/)
{
    // Surge of Power's focus unit is non-attackable too; find it directly like SurgeOfPowerTrigger.
    Creature* surge = bot->FindNearestCreature(NPC_SURGE_OF_POWER, 100.0f, true);

    // The beam runs from Malygos through the surge focus; stepping off that line clears it.
    if (surge && bot->GetExactDist2d(surge) < 12.0f)
    {
        return MoveAway(surge, 6.0f);
    }

    return false;
}

bool EoEFlyDrakeAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}
bool EoEFlyDrakeAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();
    if (!master) { return false; }
    Unit* masterVehicle = master->GetVehicleBase();
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase || !masterVehicle) { return false; }

    MotionMaster* mm = vehicleBase->GetMotionMaster();

    // Fan out around the master so Static Field / Arcane Storm can't clip the whole flight.
    if (vehicleBase->GetExactDist(masterVehicle) > 20.0f)
    {
        uint8 numPlayers;
        bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? numPlayers = 25 : numPlayers = 10;
        // 3/4 of a circle, with a 90 deg frontal cone left clear
        float angle = botAI->GetGroupSlotIndex(bot) * (2*M_PI - M_PI_2)/numPlayers + M_PI_2;
        vehicleBase->SetCanFly(true);
        mm->MoveFollow(masterVehicle, 15.0f, angle);
        vehicleBase->SendMovementFlagUpdate();
        return true;
    }
    return false;
}

bool EoEDrakeAttackAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool EoEDrakeAttackAction::Execute(Event /*event*/)
{
    vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
    {
        return false;
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss)
    {
        GuidVector npcs = AI_VALUE(GuidVector, "possible targets");
        for (auto& npc : npcs)
        {
            Unit* unit = botAI->GetUnit(npc);
            if (!unit || unit->GetEntry() != NPC_MALYGOS)
            {
                continue;
            }

            boss = unit;
            break;
        }
    }
    if (!boss)
    {
        return false;
    }

    if (IsHealDrake())
    {
        return DrakeHealAction();
    }
    return DrakeDpsAction(boss);
}

bool EoEDrakeAttackAction::IsHealDrake()
{
    // Use the bot's real role first; fall back to a count-based split only to guarantee a
    // minimum number of drake healers if too few bots are flagged as healers.
    if (botAI->IsHeal(bot))
    {
        return true;
    }

    uint8 minHealers;
    bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? minHealers = 6 : minHealers = 2;

    Group* group = bot->GetGroup();
    if (!group)
    {
        return false;
    }

    std::vector<ObjectGuid> nonHealers;
    uint8 healerCount = 0;
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member) { continue; }

        if (botAI->IsHeal(member))
        {
            ++healerCount;
        }
        else
        {
            nonHealers.push_back(member->GetGUID());
        }
    }

    if (healerCount >= minHealers)
    {
        return false;
    }

    // Promote the lowest-GUID non-healers to make up the healer shortfall (stable across bots).
    std::sort(nonHealers.begin(), nonHealers.end());
    uint8 needed = minHealers - healerCount;
    for (uint8 i = 0; i < needed && i < nonHealers.size(); ++i)
    {
        if (nonHealers[i] == bot->GetGUID())
        {
            return true;
        }
    }
    return false;
}

bool EoEDrakeAttackAction::CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown)
{
    if (botAI->CanCastVehicleSpell(spellId, target))
        if (botAI->CastVehicleSpell(spellId, target))
        {
            vehicleBase->AddSpellCooldown(spellId, 0, cooldown);
            return true;
        }
    return false;
}

bool EoEDrakeAttackAction::DrakeDpsAction(Unit* target)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    // Close to firing range even if the master idles, without breaking the formation fan.
    float range = 55.0f;    // Drake abilities reach 60yd
    float distance = drake->GetExactDist(target);
    if (distance > range)
    {
        MotionMaster* mm = drake->GetMotionMaster();
        mm->Clear(false);
        mm->MoveForwards(target, distance - range);
        drake->SendMovementFlagUpdate();
        return true;
    }

    uint8 comboPoints = drake->GetComboPoints(target);
    if (comboPoints >= 2)
    {
        return CastDrakeSpellAction(target, SPELL_ENGULF_IN_FLAMES, 0);
    }
    else
    {
        return CastDrakeSpellAction(target, SPELL_FLAME_SPIKE, 0);
    }
}

bool EoEDrakeAttackAction::DrakeHealAction()
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake)
    {
        return false;
    }

    // Find the most-injured drake in the flight, not just our own vehicle.
    Unit* healTarget = nullptr;
    uint8 injuredCount = 0;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member) { continue; }

            Unit* allyDrake = member->GetVehicleBase();
            if (!allyDrake || allyDrake->GetEntry() != NPC_WYRMREST_SKYTALON) { continue; }
            if (allyDrake->IsFullHealth()) { continue; }

            ++injuredCount;
            if (!healTarget || allyDrake->GetHealthPct() < healTarget->GetHealthPct())
            {
                healTarget = allyDrake;
            }
        }
    }

    if (!healTarget)
    {
        healTarget = drake;
    }

    uint8 comboPoints = drake->GetComboPoints(drake);
    // Life Burst is an AoE burst; worth the combo dump when several drakes are hurt.
    if (comboPoints >= 5 && injuredCount >= 2)
    {
        return CastDrakeSpellAction(drake, SPELL_LIFE_BURST, 0);
    }

    // Revivify is single-target. CanCastVehicleSpell reports BAD_TARGETS on drakes, so force it.
    return botAI->CastVehicleSpell(SPELL_REVIVIFY, healTarget);
}

bool AvoidStaticFieldAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool AvoidStaticFieldAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    Creature* field = drake->FindNearestCreature(NPC_STATIC_FIELD, 20.0f, true);
    if (!field) { return false; }

    // Let an in-progress flee finish instead of clearing and restamping every tick, which stutters
    // the drake in place and fights EoEFlyDrakeAction's follow formation.
    if (drake->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE)
    {
        return true;
    }

    float dx = drake->GetPositionX() - field->GetPositionX();
    float dy = drake->GetPositionY() - field->GetPositionY();
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.01f)
    {
        dx = std::cos(drake->GetOrientation());
        dy = std::sin(drake->GetOrientation());
        len = 1.0f;
    }

    float const flee = 25.0f;
    float tx = field->GetPositionX() + dx / len * flee;
    float ty = field->GetPositionY() + dy / len * flee;

    MotionMaster* mm = drake->GetMotionMaster();
    mm->Clear(false);
    mm->MovePoint(0, tx, ty, drake->GetPositionZ());
    drake->SendMovementFlagUpdate();
    return true;
}

bool DrakeDodgeSurgeAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool DrakeDodgeSurgeAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (!boss) { return false; }

    // Flame Shield absorbs the surge; pop it on cooldown as the fixate lands.
    if (botAI->CanCastVehicleSpell(SPELL_FLAME_SHIELD, drake) &&
        !drake->HasSpellCooldown(SPELL_FLAME_SHIELD))
    {
        if (botAI->CastVehicleSpell(SPELL_FLAME_SHIELD, drake))
        {
            drake->AddSpellCooldown(SPELL_FLAME_SHIELD, 0, 30000);
        }
    }

    // Blazing Speed to peel out of the beam faster when available.
    if (botAI->CanCastVehicleSpell(SPELL_BLAZING_SPEED, drake) &&
        !drake->HasSpellCooldown(SPELL_BLAZING_SPEED))
    {
        if (botAI->CastVehicleSpell(SPELL_BLAZING_SPEED, drake))
        {
            drake->AddSpellCooldown(SPELL_BLAZING_SPEED, 0, 60000);
        }
    }

    // Let an in-progress strafe finish rather than clearing and reissuing it every tick, which
    // stutters the drake and stomps EoEFlyDrakeAction's follow formation.
    if (drake->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE)
    {
        return true;
    }

    // Strafe perpendicular to the boss line so we leave the beam's path.
    float angle = boss->GetAngle(drake) + M_PI_2;
    float const strafe = 25.0f;
    float tx = drake->GetPositionX() + std::cos(angle) * strafe;
    float ty = drake->GetPositionY() + std::sin(angle) * strafe;

    MotionMaster* mm = drake->GetMotionMaster();
    mm->Clear(false);
    mm->MovePoint(0, tx, ty, drake->GetPositionZ());
    drake->SendMovementFlagUpdate();
    return true;
}
