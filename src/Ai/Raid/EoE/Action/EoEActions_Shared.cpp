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

bool MalygosPositionAction::Execute(Event /*event*/)
{
    // Disk riders steer their vehicle instead; never drag them off it.
    if (bot->GetVehicle())
    {
        return false;
    }

    uint8 phase = GetMalygosPhase(bot);
    Unit* boss = GetMalygos(bot);

    // GetMalygosPhase reports the pull intro as 4, the same value the real transitions carry. He is
    // untouchable throughout it, so full health is what tells them apart.
    bool const intro = phase == 4 && boss && boss->IsFullHealth();

    if (phase == 1 || intro)
    {
        // Whoever Malygos is chewing on behaves as the tank, assigned or not - but not in the intro,
        // where he is pacified and his victim is only whoever pulled.
        bool isBossTank = botAI->IsMainTank(bot) || (!intro && boss && boss->GetVictim() == bot);

        // This action owns the walking in both directions; PullPowerSparkAction only casts.
        bool const gripDuty = !isBossTank && IsOnPowerSparkGripDuty(botAI);
        MalygosP1Layout const& layout = GetMalygosP1Layout(bot);
        bool const hunter = botAI->IsRangedDps(bot) && bot->IsClass(CLASS_HUNTER);
        bool const onStack = !isBossTank && !gripDuty && !hunter;
        std::pair<float, float> const& spot = isBossTank ? layout.tank
                                              : gripDuty ? layout.grip
                                              : hunter   ? layout.hunter
                                                         : layout.stack;
        float const tolerance = gripDuty ? POWER_SPARK_GRIP_TOLERANCE : MALYGOS_P1_POSITION_TOLERANCE;

        float spotX = spot.first;
        float spotY = spot.second;

        // Slides continuously; a spot that snaps between two positions is what sets a raid bouncing.
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

        // Hand the tick back either way, or this outranks the rotation and the bot stops attacking.
        if (isBossTank && boss)
        {
            ServerFacade::instance().SetFacingTo(bot, boss);
        }
        return false;
    }
    else if (phase == 2 || phase == 4)
    {
        float const cx = MALYGOS_CENTER_POSITION.first;
        float const cy = MALYGOS_CENTER_POSITION.second;
        float const safeRadius = MALYGOS_ANTIFALL_RADIUS;

        float dist = bot->GetDistance2d(cx, cy);
        if (dist > safeRadius)
        {
            float target = safeRadius - MALYGOS_ANTIFALL_INSET;
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
    Unit* boss = GetMalygos(bot);
    uint8 phase = GetMalygosPhase(bot);

    if (phase == 1)
    {
        if (botAI->IsHeal(bot))
        {
            return false;
        }
        if (!boss)
        {
            return false;
        }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        // The tank never peels: dropping Malygos swings his Arcane Breath cone through the raid.
        Unit* newTarget = boss;
        bool const isBossTank = botAI->IsMainTank(bot) || boss->GetVictim() == bot;
        if (!isBossTank && botAI->IsDps(bot))
        {
            if (Unit* spark = GetPowerSparkToKill(botAI, currentTarget))
            {
                newTarget = spark;
            }
        }

        if (!currentTarget || currentTarget->GetGUID() != newTarget->GetGUID())
        {
            return Attack(newTarget);
        }
    }
    else if (phase == 2)
    {
        // Runs before the early returns below so a disk rider's ghoul is covered too.
        if (bot->GetGuardianPet())
        {
            if (Unit* petTarget = GetNearestEoECreature(bot, NPC_NEXUS_LORD))
            {
                CommandPetAttack(botAI, petTarget);
            }
            else
            {
                StopPet(botAI);
            }
        }

        if (botAI->IsHeal(bot))
        {
            return false;
        }

        if (bot->GetVehicle())
        {
            return false;
        }

        Unit* nexusLord = nullptr;
        Unit* scionOfEternity = nullptr;
        Unit* anyLord = nullptr;
        Unit* anyScion = nullptr;

        // IsWithinCombatRange, not a flat 2d distance: the Scions sit 20-30y up. Melee get no gate.
        bool const gateByReach = botAI->IsRanged(bot);
        float const reach = sPlayerbotAIConfig.spellDistance;
        float closestLord = std::numeric_limits<float>::max();
        float closestScion = std::numeric_limits<float>::max();
        float closestAnyLord = std::numeric_limits<float>::max();
        float closestAnyScion = std::numeric_limits<float>::max();

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (!unit)
            {
                continue;
            }

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
        if (!newTarget && botAI->IsRangedDps(bot))
        {
            newTarget = scionOfEternity;
        }
        if (!newTarget)
        {
            return false;
        }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        // Swapped by GUID, not entry: entry alone welded bots to an add they could never reach.
        if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == newTarget->GetEntry() &&
            (!gateByReach || bot->IsWithinCombatRange(currentTarget, reach)))
        {
            return false;
        }

        if (!currentTarget || currentTarget->GetGUID() != newTarget->GetGUID())
        {
            return Attack(newTarget);
        }
    }

    return false;
}
