/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

bool KelthuzadChooseTargetAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    GuidVector attackers = context->GetValue<GuidVector>("possible targets")->Get();
    Unit* target = nullptr;
    Unit *target_soldier = nullptr, *target_weaver = nullptr, *target_abomination = nullptr, *target_kelthuzad = nullptr,
         *target_guardian = nullptr;

    bool isOffTankForKT = botAI->IsTank(bot) && !botAI->IsMainTank(bot) &&
                          (botAI->IsAssistTank(bot) || botAI->HasStrategy("tank assist", BOT_STATE_COMBAT));

    for (auto i = attackers.begin(); i != attackers.end(); ++i)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (!unit)
            continue;

        bool isKelthuzad = botAI->EqualLowercaseName(unit->GetName(), "kel'thuzad");
        if (isKelthuzad)
        {
            if (unit->GetDistance2d(helper.center.first, helper.center.second) > 30.0f)
            {
                continue;
            }
        }
        else
        {
            // Parked perimeter adds are decoration until activated - never body-pull them.
            if (!helper.IsAddActive(unit))
            {
                continue;
            }
            if (unit->GetDistance2d(helper.center.first, helper.center.second) >
                KelthuzadBossHelper::P1_ACTIVE_ADD_MAX_CENTER_DIST)
            {
                continue;
            }
        }
        if (bot->GetDistance2d(unit) > sPlayerbotAIConfig.spellDistance)
        {
            continue;
        }
        if (botAI->EqualLowercaseName(unit->GetName(), "unstoppable abomination"))
        {
            if (target_abomination == nullptr ||
                target_abomination->GetDistance2d(helper.center.first, helper.center.second) >
                    unit->GetDistance2d(helper.center.first, helper.center.second))
            {
                target_abomination = unit;
            }
        }
        if (botAI->EqualLowercaseName(unit->GetName(), "soldier of the frozen wastes"))
        {
            if (target_soldier == nullptr ||
                target_soldier->GetDistance2d(helper.center.first, helper.center.second) >
                    unit->GetDistance2d(helper.center.first, helper.center.second))
            {
                target_soldier = unit;
            }
        }
        if (botAI->EqualLowercaseName(unit->GetName(), "soul weaver"))
        {
            if (target_weaver == nullptr || target_weaver->GetDistance2d(helper.center.first, helper.center.second) >
                                                unit->GetDistance2d(helper.center.first, helper.center.second))
            {
                target_weaver = unit;
            }
        }
        if (isKelthuzad)
        {
            target_kelthuzad = unit;
        }
    }

    std::vector<Unit*> guardians = helper.GetGuardians();
    bool guardiansPresent = !guardians.empty();
    if (isOffTankForKT && guardiansPresent)
    {
        target_guardian = helper.GetGuardianToPickup(bot);
    }
    std::vector<Unit*> targets;
    if (botAI->IsRanged(bot))
    {
        bool hasRemainingP1Adds = (target_weaver || target_soldier || target_abomination);

        if (helper.IsPhaseTwo() && hasRemainingP1Adds && !botAI->IsHeal(bot))
            targets = {target_weaver, target_soldier, target_abomination, target_kelthuzad};
        else if (helper.IsPhaseTwo())
            targets = {target_kelthuzad, target_weaver, target_soldier, target_abomination};
        else
            targets = {target_weaver, target_soldier, target_abomination, target_kelthuzad};
    }
    else if (isOffTankForKT)
    {
       if (guardiansPresent)
           targets = {target_guardian};
       else
           targets = {target_abomination, target_kelthuzad};
    }
    else
    {
        targets = {target_abomination, target_kelthuzad};
    }
    for (Unit* t : targets)
    {
        if (!botAI->IsRanged(bot))
        {
            float maxCenterDist = 20.0f;

            if (isOffTankForKT && guardiansPresent)
                maxCenterDist = KelthuzadBossHelper::ROOM_MAX_RADIUS + 2.0f;

            if (t && t->GetDistance2d(helper.center.first, helper.center.second) > maxCenterDist)
            {
                continue;
            }
        }
        if (t)
        {
            target = t;
            break;
        }
    }
    if (context->GetValue<Unit*>("current target")->Get() == target)
    {
        return false;
    }
    if (target_kelthuzad && target == target_kelthuzad)
    {
        return Attack(target, true);
    }
    return Attack(target, false);
}

bool KelthuzadPositionAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    helper.RecallControlledPetsToBot();

    if (helper.IsPhaseOne())
    {
        if (botAI->IsTank(bot))
        {
            float dx = helper.center.first;
            float dy = helper.center.second;

            helper.ClampToRoom(dx, dy,
                KelthuzadBossHelper::PHASE1_TANK_HOLD_RADIUS,
                KelthuzadBossHelper::PHASE1_TANK_HOLD_RADIUS);

            if (bot->GetDistance2d(helper.center.first, helper.center.second) > KelthuzadBossHelper::PHASE1_TANK_MAX_RADIUS)
            {
                return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT);
            }

            Unit* currentTarget = AI_VALUE(Unit*, "current target");
            if (currentTarget &&
                currentTarget->GetDistance2d(helper.center.first, helper.center.second) <= KelthuzadBossHelper::PHASE1_TANK_MAX_RADIUS)
            {
                if (bot->GetDistance2d(currentTarget) > 3.0f)
                    return MoveNear(currentTarget, 3.0f, MovementPriority::MOVEMENT_COMBAT);
            }

            return false;
        }
        if (bot->GetDistance2d(helper.center.first, helper.center.second) > 20.0f)
        {
            return MoveInside(NAXX_MAP_ID, helper.center.first, helper.center.second, bot->GetPositionZ(), 3.0f,
                              MovementPriority::MOVEMENT_COMBAT);
        }
        if (!botAI->IsRanged(bot))
        {
            Unit* currentTarget = AI_VALUE(Unit*, "current target");
            if (currentTarget &&
                currentTarget->GetDistance2d(helper.center.first, helper.center.second) <= 20.0f &&
                bot->GetDistance2d(currentTarget) > 3.0f)
            {
                return MoveNear(currentTarget, 3.0f, MovementPriority::MOVEMENT_COMBAT);
            }
        }
        if (AI_VALUE(Unit*, "current target") == nullptr)
        {
            return MoveInside(NAXX_MAP_ID, helper.center.first, helper.center.second, bot->GetPositionZ(), 3.0f,
                              MovementPriority::MOVEMENT_COMBAT);
        }
    }
    else if (helper.IsPhaseTwo())
    {
        if (helper.HasDetonateMana(bot))
        {
            // Blast fires ~5s after application - park in the widest gap reachable with the least travel.
            Group* group = bot->GetGroup();
            struct Candidate
            {
                float x, y, score, travel;
            };
            std::vector<Candidate> candidates;
            float bestScore = 0.0f;
            for (uint32 k = 0; k < 24; ++k)
            {
                float angle = 2.0f * float(M_PI) * float(k) / 24.0f;
                float cx = helper.center.first + std::cos(angle) * KelthuzadBossHelper::DETONATE_MAX_RADIUS;
                float cy = helper.center.second + std::sin(angle) * KelthuzadBossHelper::DETONATE_MAX_RADIUS;
                if (helper.IsNearShadowFissure(cx, cy))
                {
                    continue;
                }

                float minDist = std::numeric_limits<float>::max();
                if (group)
                {
                    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                    {
                        Player* member = ref->GetSource();
                        if (!member || !member->IsAlive() || member == bot)
                        {
                            continue;
                        }
                        minDist = std::min(minDist, member->GetDistance2d(cx, cy));
                    }
                }
                candidates.push_back({cx, cy, minDist, bot->GetDistance2d(cx, cy)});
                bestScore = std::max(bestScore, minDist);
            }

            Candidate const* pick = nullptr;
            for (Candidate const& candidate : candidates)
            {
                if (candidate.score >= bestScore - 1.0f && (!pick || candidate.travel < pick->travel))
                {
                    pick = &candidate;
                }
            }
            if (!pick)
            {
                return false;
            }

            float dx = pick->x;
            float dy = pick->y;
            helper.ClampToRoom(dx, dy, KelthuzadBossHelper::DETONATE_MIN_RADIUS, KelthuzadBossHelper::DETONATE_MAX_RADIUS);
            return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        }
        if (helper.HasChains(bot))
        {
            bot->AttackStop();
            return false;
        }
        Player* frostBlastTarget = helper.GetPlayerWithAura(NaxxSpellIds::FrostBlast);
        if (frostBlastTarget && frostBlastTarget != bot &&
            bot->GetDistance2d(frostBlastTarget) < KelthuzadBossHelper::FROST_BLAST_SAFE_DIST)
        {
            float dx, dy;
            if (helper.ComputeEscapeFromPoint(frostBlastTarget->GetPositionX(), frostBlastTarget->GetPositionY(),
                                              KelthuzadBossHelper::FROST_BLAST_SAFE_DIST, dx, dy) &&
                !helper.IsNearShadowFissure(dx, dy))
            {
                return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT);
            }
            return false;
        }

        bool isOffTankForKT = botAI->IsTank(bot) && !botAI->IsMainTank(bot) &&
                              (botAI->IsAssistTank(bot) || botAI->HasStrategy("tank assist", BOT_STATE_COMBAT));

        if (botAI->IsMainTank(bot))
        {
            if (AI_VALUE2(bool, "has aggro", "current target"))
            {
                auto hold = helper.GetMainTankHoldPosition();
                if (helper.IsNearShadowFissure(hold.first, hold.second))
                {
                    return false;
                }
                return MoveTo(NAXX_MAP_ID, hold.first, hold.second, bot->GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT);
            }
            else
            {
                return false;
            }
        }
        else if (botAI->IsRanged(bot))
        {
            float dx, dy;
            uint32 index = botAI->GetRangedIndex(bot);
            uint32 total = std::max<uint32>(1, helper.GetRangedCount());
            helper.ComputeRangedSpreadPosition(index, total, dx, dy);
            if (helper.IsNearShadowFissure(dx, dy))
            {
                return false;
            }
            if (bot->GetDistance2d(dx, dy) <= 2.0f)
            {
                return false;
            }
            return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        }
        else if (botAI->IsTank(bot))
        {
            if (isOffTankForKT)
            {
                std::vector<Unit*> guardians = helper.GetGuardians();
                if (!guardians.empty())
                {
                    Unit* pickup = helper.GetGuardianToPickup(bot);
                    if (pickup && pickup->GetVictim() != bot)
                    {
                        if (bot->GetDistance2d(pickup) > 6.0f)
                        {
                            return MoveNear(pickup, 4.0f, MovementPriority::MOVEMENT_COMBAT);
                        }
                        return false;
                    }
                    if (helper.AllGuardiansOnAssistTank(bot))
                    {
                        auto hold = helper.GetAssistTankHoldPosition();
                        if (bot->GetDistance2d(hold.first, hold.second) > 3.0f)
                        {
                            return MoveTo(NAXX_MAP_ID, hold.first, hold.second, bot->GetPositionZ(), false, false, false, false,
                                          MovementPriority::MOVEMENT_COMBAT);
                        }
                    }
                    return false;
                }
            }

            return false;
        }
        else
        {
            float dx, dy;
            uint32 index = helper.GetMeleeDpsIndex(bot);
            uint32 total = std::max<uint32>(1, helper.GetMeleeDpsCount());
            if (!helper.ComputeMeleeSpreadPosition(index, total, dx, dy))
            {
                return false;
            }
            if (helper.IsNearShadowFissure(dx, dy))
            {
                return false;
            }
            // Boss-relative slot drifts as the boss shifts - hysteresis avoids constant micro-moves.
            if (bot->GetDistance2d(dx, dy) <= 2.5f)
            {
                return false;
            }
            return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return false;
}

bool KelthuzadFleeShadowFissureAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Unit* fissure = helper.GetNearestShadowFissure();
    if (!fissure || !bot->IsWithinDistInMap(fissure, KelthuzadBossHelper::FISSURE_DANGER_RADIUS))
    {
        return false;
    }

    // Main tank keeps the boss anchored: fall back to the hold spot if it's already clear of
    // fissures, so clearing the void blast doesn't drag Kel'thuzad across the room.
    if (botAI->IsMainTank(bot))
    {
        auto hold = helper.GetMainTankHoldPosition();
        if (!helper.IsNearShadowFissure(hold.first, hold.second))
        {
            return MoveTo(NAXX_MAP_ID, hold.first, hold.second, bot->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_FORCED, true, false);
        }
    }

    float dx, dy;
    if (!helper.ComputeEscapeFromPoint(fissure->GetPositionX(), fissure->GetPositionY(),
                                       KelthuzadBossHelper::FISSURE_DANGER_RADIUS, dx, dy))
    {
        return false;
    }
    return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

bool KelthuzadMisdirectBossToMainTankAction::Execute(Event event)
{
    if (bot->getClass() != CLASS_HUNTER)
    {
        return false;
    }
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Player* mainTank = GetGroupMainTank(botAI, bot);
    if (!mainTank || mainTank == bot)
    {
        return false;
    }

    if (botAI->CanCastSpell("misdirection", mainTank))
    {
        return botAI->CastSpell("misdirection", mainTank);
    }

    // Threat transfer shot: P2 dumps onto the boss, P1 onto whatever active add we already fight.
    if (bot->HasAura(NaxxSpellIds::Misdirection))
    {
        Unit* target = helper.IsPhaseTwo() ? helper.GetBoss() : AI_VALUE(Unit*, "current target");
        if (target && botAI->CanCastSpell("steady shot", target))
        {
            return botAI->CastSpell("steady shot", target);
        }
    }
    return false;
}