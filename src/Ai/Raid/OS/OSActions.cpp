/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSActions.h"
#include "OSShared.h"
#include "OSTriggers.h"

#include "Playerbots.h"

using namespace ObsidianSanctumHelpers;

bool SartharionTankPositionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return false; }

    Unit* shadron = nullptr;
    Unit* tenebron = nullptr;
    Unit* vesperon = nullptr;

    // Detect incoming drakes before they are on aggro table
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit) { continue; }

        switch (unit->GetEntry())
        {
            case NPC_SHADRON:
                shadron = unit;
                continue;
            case NPC_TENEBRON:
                tenebron = unit;
                continue;
            case NPC_VESPERON:
                vesperon = unit;
                continue;
            default:
                continue;
        }
    }

    Position currentPos = bot->GetPosition();
    // Adjustable, this is the acceptable distance to stack point that will be accepted as "safe"
    float looseDistance = 12.0f;

    if (botAI->IsMainTank(bot))
    {
        if (bot->GetExactDist2d(SARTHARION_MAINTANK_POSITION.first, SARTHARION_MAINTANK_POSITION.second) > looseDistance)
        {
            return MoveTo(OS_MAP_ID, SARTHARION_MAINTANK_POSITION.first, SARTHARION_MAINTANK_POSITION.second, currentPos.GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    // Offtank grabs and holds every landed drake (kept and to-kill alike) far from the raid stack, so
    // boss-centred AoE never clips a kept drake. Force-threat locks all of them onto one off-tank.
    else if (shadron || tenebron || vesperon)
    {
        float triggerDistance = 100.0f;
        Unit* held = nullptr;
        float heldDist = triggerDistance;

        Unit* drakes[3] = {tenebron, shadron, vesperon};
        for (Unit* drake : drakes)
        {
            if (!drake)
                continue;

            float dist = bot->GetExactDist2d(drake);
            if (dist >= triggerDistance)
                continue;

            ObsidianSanctumHelpers::ForceThreat(drake, bot);
            if (AI_VALUE(Unit*, "current target") != drake && drake->GetVictim() != bot)
                return Attack(drake);

            // Track the nearest engaged drake so we face the one we're actually holding, not a fixed
            // pick that might be out of reach and turn the held drakes' breath toward the raid.
            if (dist < heldDist)
            {
                heldDist = dist;
                held = drake;
            }
        }

        // Once the drakes are in hand, park at the off-tank spot and face them away from the raid so
        // their frontal Shadow Breath points away from the stack.
        if (held)
        {
            if (bot->GetExactDist2d(SARTHARION_OFFTANK_POSITION.first, SARTHARION_OFFTANK_POSITION.second) > looseDistance)
            {
                return MoveTo(OS_MAP_ID, SARTHARION_OFFTANK_POSITION.first, SARTHARION_OFFTANK_POSITION.second, currentPos.GetPositionZ(),
                    false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }

            bot->SetFacingToObject(held);
        }
    }
    return false;
}

bool AvoidTwilightFissureAction::Execute(Event /*event*/)
{
    const float radius = 5.0f;

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == NPC_TWILIGHT_FISSURE)
        {
            float currentDistance = bot->GetDistance2d(unit);
            if (currentDistance < radius)
                return MoveAway(unit, radius - currentDistance);
        }
    }
    return false;
}

bool AvoidFlameTsunamiAction::Execute(Event /*event*/)
{
    // Adjustable, this is the acceptable distance to stack point that will be accepted as "safe"
    float looseDistance = 4.0f;

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == NPC_FLAME_TSUNAMI)
        {
            Position currentPos = bot->GetPosition();

            // I think these are centrepoints for the wave segments. Either way they uniquely identify the wave
            // direction as they have different coords for the left and right waves
            // int casting is not a mistake, need to avoid FP errors somehow.
            // I always saw these accurate to around 6 decimal places, but if there are issues,
            // can switch this to abs comparison of floats which would technically be more robust.
            int posY = (int) unit->GetPositionY();
            if (posY == 505 || posY == 555)     // RIGHT WAVE
            {
                bool wavePassed = currentPos.GetPositionX() > unit->GetPositionX();
                if (wavePassed)
                    return false;

                if (bot->GetExactDist2d(currentPos.GetPositionX(), TSUNAMI_RIGHT_SAFE_ALL) > looseDistance)
                    return MoveTo(OS_MAP_ID, currentPos.GetPositionX(), TSUNAMI_RIGHT_SAFE_ALL, currentPos.GetPositionZ(),
                        false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            else    // LEFT WAVE
            {
                bool wavePassed = currentPos.GetPositionX() < unit->GetPositionX();
                if (wavePassed)
                    return false;

                if (botAI->IsMelee(bot))
                {
                    if (bot->GetExactDist2d(currentPos.GetPositionX(), TSUNAMI_LEFT_SAFE_MELEE) > looseDistance)
                        return MoveTo(OS_MAP_ID, currentPos.GetPositionX(), TSUNAMI_LEFT_SAFE_MELEE, currentPos.GetPositionZ(),
                            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                }
                else    // Ranged/healers
                {
                    if (bot->GetExactDist2d(currentPos.GetPositionX(), TSUNAMI_LEFT_SAFE_RANGED) > looseDistance)
                        return MoveTo(OS_MAP_ID, currentPos.GetPositionX(), TSUNAMI_LEFT_SAFE_RANGED, currentPos.GetPositionZ(),
                            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                }
            }
        }
    }
    return false;
}

bool SartharionAttackPriorityAction::Execute(Event /*event*/)
{
    Unit* target = nullptr;

    // Inside the twilight realm: only the acolyte keeping the portal open is reachable (Shadron
    // first, then Vesperon). Killing it drops the boss's Gift of Twilight Fire / Twilight Torment on
    // the raid. Every other priority below targets the normal phase, which can't be hit from here, so
    // if no acolyte is in reach we do nothing and wait for the exit trigger to pull us out.
    if (bot->HasAura(SPELL_TWILIGHT_SHIFT))
    {
        target = FindTwilightRealmAcolyte(botAI);
        if (target && AI_VALUE(Unit*, "current target") != target)
            return Attack(target);
        return false;
    }

    // 1. Twilight eggs/whelps (Tenebron) must be killed, eggs before they hatch.
    if (!target)
        target = FindTwilightAdd(botAI);

    // 2. Lava Blaze adds (Sartharion Lava Strike) must be killed.
    if (!target)
        target = FindLavaBlaze(botAI);

    // 3. A to-kill drake whose acolyte is already cleared (the clear-gate prevents Twilight Revenge).
    //    Kept drakes are never selected here.
    if (!target)
        target = FindDrakeToKill(botAI);

    // 4. Otherwise burn Sartharion.
    if (!target)
        target = AI_VALUE2(Unit*, "find target", "sartharion");

    if (target && AI_VALUE(Unit*, "current target") != target)
        return Attack(target);

    return false;
}

bool SartharionRangedPositionAction::Execute(Event /*event*/)
{
    // Give ranged/healers a stable baseline stack away from the boss: steadier tsunami dodging and
    // clear of drake breath. Don't drag portal-runners out of the twilight realm.
    if (bot->HasAura(SPELL_TWILIGHT_SHIFT)) { return false; }

    float looseDistance = 8.0f;
    Position currentPos = bot->GetPosition();
    if (bot->GetExactDist2d(SARTHARION_RANGED_POSITION.first, SARTHARION_RANGED_POSITION.second) > looseDistance)
    {
        return MoveTo(OS_MAP_ID, SARTHARION_RANGED_POSITION.first, SARTHARION_RANGED_POSITION.second, currentPos.GetPositionZ(),
            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

bool EnterTwilightPortalAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return false; }

    // A twilight portal being open is itself the signal that an acolyte (Shadron's or Vesperon's)
    // needs killing inside the realm, so enter on the portal alone rather than gating on a single
    // drake's aura. Because kept drakes keep respawning acolytes, this re-fires each cycle.
    GameObject* portal = bot->FindNearestGameObject(GO_TWILIGHT_PORTAL, 100.0f);
    if (!portal) { return false; }

    if (!portal->IsAtInteractDistance(bot))
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f));

    // Go through portal
    WorldPacket data1(CMSG_GAMEOBJ_USE);
    data1 << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data1);

    return true;
}

bool ExitTwilightPortalAction::Execute(Event /*event*/)
{
    GameObject* portal = bot->FindNearestGameObject(GO_NORMAL_PORTAL, 100.0f);
    if (!portal)
        return false;

    if (!portal->IsAtInteractDistance(bot))
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f));

    // Go through portal
    WorldPacket data1(CMSG_GAMEOBJ_USE);
    data1 << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data1);

    return true;
}
