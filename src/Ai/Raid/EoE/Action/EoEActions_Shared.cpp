/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEActions_Shared.h"
#include "EoEData.h"
#include "EoEEncounter_Malygos.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "ServerFacade.h"
#include "Vehicle.h"

#include <limits>
#include <utility>

using namespace EncounterHelpers;

bool MalygosPositionAction::isUseful()
{
    if (bot->GetVehicle())
        return false;

    MalygosPhase const phase = GetMalygosPhase(bot);
    return phase == MalygosPhase::P1 || phase == MalygosPhase::P2 || phase == MalygosPhase::Transition;
}

bool MalygosPositionAction::Execute(Event /*event*/)
{
    MalygosPhase const phase = GetMalygosPhase(bot);
    Unit* boss = GetMalygos(bot);

    bool const intro = phase == MalygosPhase::Transition && boss && boss->IsFullHealth();

    if (phase == MalygosPhase::P1 || intro)
    {
        MalygosAggro const& aggro = GetMalygosAggro(botAI);
        bool const isBossTank = aggro.mainTank || (!intro && aggro.victim);

        bool const gripDuty = !isBossTank && IsOnPowerSparkGripDuty(botAI);
        MalygosP1Layout const& layout = GetMalygosP1Layout(bot);
        bool const hunter = PlayerbotAI::IsRangedDps(bot) && bot->IsClass(CLASS_HUNTER);
        bool const onStack = !isBossTank && !gripDuty && !hunter;
        std::pair<float, float> const& spot = isBossTank ? layout.tank
                                              : gripDuty ? layout.grip
                                              : hunter   ? layout.hunter
                                                         : layout.stack;
        float const tolerance = gripDuty ? POWER_SPARK_GRIP_TOLERANCE : MALYGOS_P1_POSITION_TOLERANCE;

        float spotX = spot.first;
        float spotY = spot.second;

        if (onStack && !intro && boss)
        {
            float const bossDist = boss->GetExactDist2d(spotX, spotY);
            if (bossDist > MALYGOS_MELEE_HOLD_DISTANCE)
            {
                float const bossX = boss->GetPositionX();
                float const bossY = boss->GetPositionY();
                spotX = bossX + (spotX - bossX) / bossDist * MALYGOS_MELEE_HOLD_DISTANCE;
                spotY = bossY + (spotY - bossY) / bossDist * MALYGOS_MELEE_HOLD_DISTANCE;
            }
        }

        if (bot->GetDistance2d(spotX, spotY) > tolerance)
        {
            return MoveTo(EOE_MAP_ID, spotX, spotY, bot->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }

        if (isBossTank && boss)
            ServerFacade::instance().SetFacingTo(bot, boss);
        return false;
    }
    else if (phase == MalygosPhase::P2 || phase == MalygosPhase::Transition)
    {
        float const cx = MALYGOS_CENTER_POSITION.first;
        float const cy = MALYGOS_CENTER_POSITION.second;

        float const dist = bot->GetDistance2d(cx, cy);
        if (dist > MALYGOS_ANTIFALL_RADIUS)
        {
            float const target = MALYGOS_ANTIFALL_RADIUS - MALYGOS_ANTIFALL_INSET;
            float const tx = cx + (bot->GetPositionX() - cx) / dist * target;
            float const ty = cy + (bot->GetPositionY() - cy) / dist * target;
            return MoveTo(EOE_MAP_ID, tx, ty, bot->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
        return false;
    }

    return false;
}

bool MalygosTargetAction::Execute(Event /*event*/)
{
    Unit* boss = GetMalygos(bot);
    MalygosPhase const phase = GetMalygosPhase(bot);

    if (phase == MalygosPhase::P1)
    {
        if (PlayerbotAI::IsHeal(bot))
            return false;
        if (!boss)
            return false;

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        Unit* newTarget = boss;
        MalygosAggro const& aggro = GetMalygosAggro(botAI);
        bool const isBossTank = aggro.mainTank || aggro.victim;
        if (!isBossTank && PlayerbotAI::IsDps(bot))
        {
            if (Unit* spark = GetPowerSparkToKill(botAI, currentTarget))
                newTarget = spark;
        }

        if (!currentTarget || currentTarget->GetGUID() != newTarget->GetGUID())
            return Attack(newTarget);
    }
    else if (phase == MalygosPhase::P2)
    {
        if (bot->GetGuardianPet())
        {
            if (Unit* petTarget = GetNearestEoECreature(bot, NPC_NEXUS_LORD))
                CommandPetAttack(botAI, petTarget);
            else
                StopPet(botAI);
        }

        if (PlayerbotAI::IsHeal(bot))
            return false;

        if (bot->GetVehicle())
            return false;

        Unit* nexusLord = nullptr;
        Unit* scionOfEternity = nullptr;
        Unit* anyLord = nullptr;
        Unit* anyScion = nullptr;

        bool const gateByReach = PlayerbotAI::IsRanged(bot);
        float const reach = sPlayerbotAIConfig.SpellDistance;
        float closestLord = std::numeric_limits<float>::max();
        float closestScion = std::numeric_limits<float>::max();
        float closestAnyLord = std::numeric_limits<float>::max();
        float closestAnyScion = std::numeric_limits<float>::max();

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (ObjectGuid const& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (!unit)
                continue;

            float dist = bot->GetExactDist(unit);
            bool inReach = !gateByReach || bot->IsWithinCombatRange(unit, reach);

            if (unit->GetEntry() == NPC_NEXUS_LORD)
            {
                if (dist < closestAnyLord)
                {
                    closestAnyLord = dist;
                    anyLord = unit;
                }
                if (inReach && dist < closestLord)
                {
                    closestLord = dist;
                    nexusLord = unit;
                }
            }
            else if (unit->GetEntry() == NPC_SCION_OF_ETERNITY)
            {
                if (dist < closestAnyScion)
                {
                    closestAnyScion = dist;
                    anyScion = unit;
                }
                if (inReach && dist < closestScion)
                {
                    closestScion = dist;
                    scionOfEternity = unit;
                }
            }
        }

        if (!nexusLord && !scionOfEternity)
        {
            nexusLord = anyLord;
            scionOfEternity = anyScion;
        }

        Unit* newTarget = nexusLord;
        if (!newTarget && PlayerbotAI::IsRangedDps(bot))
            newTarget = scionOfEternity;
        if (!newTarget)
            return false;

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == newTarget->GetEntry() &&
            (!gateByReach || bot->IsWithinCombatRange(currentTarget, reach)))
            return false;

        if (!currentTarget || currentTarget->GetGUID() != newTarget->GetGUID())
            return Attack(newTarget);
    }

    return false;
}
