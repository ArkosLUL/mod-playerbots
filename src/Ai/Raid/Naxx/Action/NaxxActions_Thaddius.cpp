/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "NaxxSpellIds.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"

using namespace EncounterHelpers;

bool ThaddiusPrepullSplitAction::isUseful() { return helper.IsPrepullStagingUsable(); }

bool ThaddiusPrepullSplitAction::Execute(Event event)
{
    // Covers the measured-spot and slime-pit checks too: no usable spot means hold off entirely
    // rather than parking someone in the slime or starting the fight early.
    if (!helper.IsPrepullStagingUsable())
        return false;

    Unit* pet = helper.GetPetForSide(helper.IsAssignedToPrimarySide(bot));
    if (!pet)
        return false;

    ThaddiusBossHelper::PrepullStaging pos = helper.PrepullGetStagingPos(pet);

    // Already parked: let the lower-priority nodes run so bots can still buff, eat and drink.
    if (bot->GetExactDist2d(pos.x, pos.y) <= ThaddiusBossHelper::PREPULL_ARRIVED)
        return false;

    // normal_only so an off-mesh destination fails outright instead of handing back an unvalidated
    // path that walks the bot into the pit.
    return MoveTo(NAXX_MAP_ID, pos.x, pos.y, pos.z, false, false, true, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool ThaddiusAttackNearestPetAction::isUseful()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    if (!helper.IsPhasePet())
    {
        return false;
    }

    // Do not start moving/acting just because RTI marks exist.
    // Wait until the pull actually started (MT engaged) or we are already in combat.
    if (!bot->IsInCombat() && !helper.IsMainTankEngagedOnPets())
    {
        return false;
    }

    Unit* target = helper.GetAssignedPetForBot();
    return !IsDownOrFeigning(target);
}

bool ThaddiusAttackNearestPetAction::Execute(Event event)
{
    Unit* target = helper.GetAssignedPetForBot();
    if (IsDownOrFeigning(target))
    {
        return false;
    }

    if (!bot->IsInCombat() && !helper.IsMainTankEngagedOnPets())
        return false;

    if (!botAI->IsTank(bot))
    {
        bool primarySide = helper.IsAssignedToPrimarySide(bot);
        if (!primarySide && !helper.IsOffTankEngagedOnPets())
        {
            std::pair<float, float> waitPos = helper.PetPhaseGetPosForRanged(target);
            return MoveTo(533, waitPos.first, waitPos.second, helper.tankPosZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }

    if (botAI->IsHeal(bot))
    {
        std::pair<float, float> pos = helper.PetPhaseGetPosForRanged(target);
        return MoveTo(533, pos.first, pos.second, helper.tankPosZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    if (!botAI->IsTank(bot) && !bot->IsWithinLOSInMap(target))
    {
        std::pair<float, float> pos = helper.PetPhaseGetPosForRanged(target);
        return MoveTo(533, pos.first, pos.second, helper.tankPosZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    if (!bot->IsWithinLOSInMap(target))
        return MoveTo(target, 0, MovementPriority::MOVEMENT_COMBAT);

    if (AI_VALUE(Unit*, "current target") != target && !botAI->IsHeal(bot))
        return Attack(target);

    if (botAI->IsTank(bot))
    {
        // Pin the add at its coil the moment we engage, not only after aggro,
        // so it is never dragged far enough to overload the tesla coil.
        std::pair<float, float> posForTank = helper.PetPhaseGetPosForTank(target);
        return MoveTo(533, posForTank.first, posForTank.second, helper.tankPosZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    if (botAI->IsRanged(bot) || botAI->IsHeal(bot))
    {
        std::pair<float, float> posForRanged = helper.PetPhaseGetPosForRanged(target);
        return MoveTo(533, posForRanged.first, posForRanged.second, helper.tankPosZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

Player* ThaddiusRedirectThreatAction::GetRedirectTank()
{
    if (!helper.UpdateBossAI())
    {
        return nullptr;
    }

    if (helper.IsPhasePet())
    {
        // Aim at the tank already on our own pet, so the two sides never redirect into each other.
        Unit* pet = helper.GetAssignedPetForBot();
        if (!pet)
        {
            return nullptr;
        }
        if (Player* tank = GetTankHolding(pet))
        {
            return tank;
        }
        return helper.IsAssignedToPrimarySide(bot) ? GetGroupMainTank(bot)
                                                   : GetGroupAssistTank(bot, 0);
    }

    // Thaddius wakes up with an empty threat table - nobody owns him until the tank rebuilds it.
    return GetGroupMainTank(bot);
}

Unit* ThaddiusRedirectThreatAction::GetThreatDumpTarget()
{
    if (helper.IsPhasePet())
    {
        return helper.GetAssignedPetForBot();
    }

    // No dump shot once Thaddius is up: Polarity Shift can land at any point and a bot standing
    // still to finish a Steady Shot dies to it. Applying the buff is enough, the rotation spends
    // the charges on its own.
    return nullptr;
}

bool ThaddiusMoveToPlatformAction::isUseful() { return true; }

bool ThaddiusMoveToPlatformAction::Execute(Event event)
{
    std::vector<std::pair<float, float>> position = {
        // high left
        {3462.99f, -2918.90f},
        // high right
        {3520.65f, -2976.51f},
        // low left
        {3471.36f, -2910.65f},
        // low right
        {3528.80f, -2967.04f},
        // center
        {3512.19f, -2928.58f},
    };
    float high_z = 312.00f, low_z = 304.02f;
    bool is_left = bot->GetDistance2d(position[0].first, position[0].second) <
                   bot->GetDistance2d(position[1].first, position[1].second);
    if (bot->GetPositionZ() >= (high_z - 3.0f))
    {
        if (is_left)
        {
            if (!MoveTo(bot->GetMapId(), position[0].first, position[0].second, high_z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
            {
                float distance = bot->GetExactDist2d(position[0].first, position[0].second);
                if (distance < sPlayerbotAIConfig.contactDistance)
                    JumpTo(bot->GetMapId(), position[2].first, position[2].second, low_z, MovementPriority::MOVEMENT_COMBAT);
            }
        }
        else
        {
            if (!MoveTo(bot->GetMapId(), position[1].first, position[1].second, high_z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
            {
                float distance = bot->GetExactDist2d(position[1].first, position[1].second);
                if (distance < sPlayerbotAIConfig.contactDistance)
                    JumpTo(bot->GetMapId(), position[3].first, position[3].second, low_z, MovementPriority::MOVEMENT_COMBAT);
            }
        }
    }
    else
    {
        return MoveTo(bot->GetMapId(), position[4].first, position[4].second, low_z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return true;
}

bool ThaddiusMovePolarityAction::isUseful()
{
    return !botAI->IsMainTank(bot) || AI_VALUE2(bool, "has aggro", "current target");
}

bool ThaddiusMovePolarityAction::Execute(Event event)
{
    std::vector<std::pair<float, float>> position = {
        // left melee
        {3508.29f, -2920.12f},
        // left ranged
        {3501.72f, -2913.36f},
        // right melee
        {3519.74f, -2931.69f},
        // right ranged
        {3524.32f, -2936.26f},
        // center melee
        {3512.19f, -2928.58f},
        // center ranged
        {3504.68f, -2936.68f},
    };
    uint32 idx;
    if (NaxxSpellIds::HasAnyAura(bot,
            {NaxxSpellIds::NegativeCharge10, NaxxSpellIds::NegativeCharge25, NaxxSpellIds::NegativeChargeStack}) ||
        botAI->HasAura("negative charge", bot, false, false, -1, true))
    {
        idx = 0;
    }
    else if (NaxxSpellIds::HasAnyAura(bot,
                 {NaxxSpellIds::PositiveCharge10, NaxxSpellIds::PositiveCharge25, NaxxSpellIds::PositiveChargeStack}) ||
             botAI->HasAura("positive charge", bot, false, false, -1, true))
    {
        idx = 1;
    }
    else
    {
        idx = 2;
    }
    idx = idx * 2 + botAI->IsRanged(bot);
    return MoveTo(bot->GetMapId(), position[idx].first, position[idx].second, bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}
