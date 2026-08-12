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

    // Pick up landed drakes before they reach anyone's aggro table. Skipping the airborne ones is
    // what keeps the off-tank from force-threating Vesperon off his ledge and pulling him solo.
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!IsDrakeLanded(unit)) { continue; }

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
    // Offtank holds everything that isn't Sartharion - landed drakes, Twilight Whelps and Lava
    // Blazes - at one spot away from the raid stack. Force-threat locks all of them onto him.
    else
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

        // Drakes first, then the rest of the pack. Dragging the blazes along means they follow the
        // off-tank into the tsunami safe lane, which is what stops one being caught and enraging.
        for (Unit* add : ObsidianSanctumHelpers::FindOffTankAdds(botAI, bot, 40.0f))
        {
            ObsidianSanctumHelpers::ForceThreat(add, bot);
            if (AI_VALUE(Unit*, "current target") != add && add->GetVictim() != bot)
                return Attack(add);

            if (!held)
                held = add;
        }

        // Park at the off-tank spot. A drake faces whoever it is attacking, so what keeps its
        // frontal Shadow Breath off the raid is where the off-tank stands, not where he looks -
        // facing the pack is only so he can swing at it.
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

            // Waves are summoned facing the way they travel: the left wave at X 3211 with
            // orientation 0 (eastbound), the right wave at X 3286 with orientation pi (westbound).
            // Orientation holds for every segment and doesn't change in flight, unlike matching on a
            // segment's Y - the right wave has six of those and only two were ever recognised.
            float const pi = static_cast<float>(M_PI);
            bool const isRightWave = std::fabs(unit->GetOrientation() - pi) < pi / 4.0f;
            if (isRightWave)
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

    // 1. Twilight Whelps (Tenebron). The eggs themselves sit in phase 16 and cannot be touched from
    //    the ground, so the whelps only become killable once they hatch and cross into phase 1.
    if (!target)
        target = FindTwilightAdd(botAI);

    // 2. Lava Blaze adds (Sartharion Lava Strike) must be killed.
    if (!target)
        target = FindLavaBlaze(botAI);

    // 3. Whichever drake has landed, in landing order.
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
