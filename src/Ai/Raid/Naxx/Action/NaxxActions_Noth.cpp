/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"

#include <vector>

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "NaxxBossHelper.h"
#include "NaxxSpellIds.h"

namespace
{
    Unit* FirstAvailable(std::vector<Unit*> const& candidates)
    {
        for (Unit* candidate : candidates)
        {
            if (candidate)
            {
                return candidate;
            }
        }
        return nullptr;
    }

    void KeepNearest(Player* bot, Unit* candidate, Unit*& best)
    {
        if (!best || bot->GetDistance2d(candidate) < bot->GetDistance2d(best))
        {
            best = candidate;
        }
    }
}  // namespace

bool NothChooseTargetAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Unit* boss = helper.GetBoss();
    bool balcony = helper.IsBalconyPhase();

    // Noth drops out of "attackers" for the whole balcony phase, so the main tank correctly picks up
    // an add - and then keeps it, because an explicit main tank in a group with two tanks sticks to
    // its current target (TankTargetValue.cpp, FindTankTargetSmartStrategy::IsBetter). Pin him back
    // on the boss the moment the balcony flag clears.
    if (!balcony && boss && botAI->IsMainTank(bot))
    {
        return AI_VALUE(Unit*, "current target") != boss && Attack(boss);
    }

    bool ownsAdds = botAI->IsAssistTank(bot) || (botAI->IsMainTank(bot) && !helper.GetAliveAssistTank());

    Unit* guardian = nullptr;
    Unit* champion = nullptr;
    Unit* warrior = nullptr;
    Unit* looseGuardian = nullptr;
    Unit* looseChampion = nullptr;
    Unit* looseWarrior = nullptr;

    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
        {
            continue;
        }

        Unit** nearest = nullptr;
        Unit** loose = nullptr;
        if (helper.IsGuardian(unit))
        {
            nearest = &guardian;
            loose = &looseGuardian;
        }
        else if (helper.IsChampion(unit))
        {
            nearest = &champion;
            loose = &looseChampion;
        }
        else if (helper.IsWarrior(unit))
        {
            nearest = &warrior;
            loose = &looseWarrior;
        }
        else
        {
            continue;
        }

        KeepNearest(bot, unit, *nearest);

        // Every add is summoned with SetInCombatWithZone() and runs at a random raid member, so
        // "loose" is the normal state right after a spawn and is what the add tank has to chase.
        Player* victim = unit->GetVictim() ? unit->GetVictim()->ToPlayer() : nullptr;
        if (ownsAdds && victim && !botAI->IsTank(victim))
        {
            KeepNearest(bot, unit, *loose);
        }
    }

    std::vector<Unit*> targets;
    if (ownsAdds)
    {
        targets = {looseGuardian, looseChampion, looseWarrior, guardian, champion, warrior};
        if (!balcony)
        {
            targets.push_back(boss);
        }
    }
    else if (balcony)
    {
        targets = {guardian, champion, warrior};
    }
    else
    {
        // Guardians nuke the raid from range and are the one add worth pulling DPS off the boss for.
        targets = {guardian, boss, champion, warrior};
    }

    Unit* target = FirstAvailable(targets);
    if (!target || AI_VALUE(Unit*, "current target") == target)
    {
        return false;
    }
    return Attack(target);
}

bool NothPositionAction::MoveToClamped(float x, float y)
{
    helper.ClampToRoom(x, y);
    return MoveTo(NAXX_MAP_ID, x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool NothPositionAction::PositionAssistTank(Unit* currentTarget)
{
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !helper.IsAdd(unit))
        {
            continue;
        }

        Player* victim = unit->GetVictim() ? unit->GetVictim()->ToPlayer() : nullptr;
        if (victim && !botAI->IsTank(victim) && bot->GetDistance2d(unit) > CleaveSpread)
        {
            return MoveToClamped(unit->GetPositionX(), unit->GetPositionY());
        }
    }

    if (!currentTarget || !helper.IsAdd(currentTarget))
    {
        return false;
    }

    // Adds spawn from alcoves 25-50 yd out, so a tank that walks out to meet them can end up with
    // the pack parked outside every healer's range.
    if (AI_VALUE2(bool, "has aggro", "current target"))
    {
        Unit* closestHealer = nullptr;
        float closestDistance = 0.0f;
        for (ObjectGuid const& guid : AI_VALUE(GuidVector, "group members"))
        {
            Unit* member = botAI->GetUnit(guid);
            Player* memberPlayer = member ? member->ToPlayer() : nullptr;
            if (!memberPlayer || memberPlayer == bot || !memberPlayer->IsAlive() || !botAI->IsHeal(memberPlayer))
            {
                continue;
            }

            float distance = bot->GetDistance2d(member);
            if (!closestHealer || distance < closestDistance)
            {
                closestHealer = member;
                closestDistance = distance;
            }
        }

        if (closestHealer && closestDistance > HealerLeashDistance + CleaveSpread)
        {
            float angle = closestHealer->GetAngle(bot);
            return MoveToClamped(closestHealer->GetPositionX() + cos(angle) * HealerLeashDistance,
                                 closestHealer->GetPositionY() + sin(angle) * HealerLeashDistance);
        }
    }

    if (helper.IsWarrior(currentTarget))
    {
        Unit* closestPlayer = nullptr;
        float closestDistance = 0.0f;
        for (ObjectGuid const& guid : AI_VALUE(GuidVector, "nearest friendly players"))
        {
            Unit* member = botAI->GetUnit(guid);
            if (!member || member == bot)
            {
                continue;
            }

            float distance = bot->GetDistance2d(member);
            if (distance <= CleaveSpread && (!closestPlayer || distance < closestDistance))
            {
                closestPlayer = member;
                closestDistance = distance;
            }
        }

        if (closestPlayer)
        {
            float angle = closestPlayer->GetAngle(bot);
            return MoveToClamped(closestPlayer->GetPositionX() + cos(angle) * CleaveSpread,
                                 closestPlayer->GetPositionY() + sin(angle) * CleaveSpread);
        }
    }

    return false;
}

bool NothPositionAction::KiteChampions()
{
    Unit* nearest = nullptr;
    float nearestDistance = 0.0f;

    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !helper.IsChampion(unit))
        {
            continue;
        }

        float distance = bot->GetDistance2d(unit);
        if (!nearest || distance < nearestDistance)
        {
            nearest = unit;
            nearestDistance = distance;
        }
    }

    if (!nearest || nearestDistance >= ChampionKiteDistance)
    {
        return false;
    }

    float angle = nearest->GetAngle(bot);
    return MoveToClamped(nearest->GetPositionX() + cos(angle) * ChampionKiteDistance,
                         nearest->GetPositionY() + sin(angle) * ChampionKiteDistance);
}

bool NothPositionAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    if (botAI->IsAssistTank(bot))
    {
        return PositionAssistTank(currentTarget);
    }

    if (botAI->IsRanged(bot))
    {
        return KiteChampions();
    }

    // Melee lose their target for the whole balcony phase and the replacements spawn at the alcoves,
    // so they need telling to walk over rather than standing where the boss used to be.
    if (helper.IsBalconyPhase() && currentTarget && helper.IsAdd(currentTarget) &&
        bot->GetDistance2d(currentTarget) > MeleeCloseDistance)
    {
        return MoveNear(currentTarget, CleaveSpread, MovementPriority::MOVEMENT_COMBAT);
    }

    return false;
}

int32 NothDispelCurseAction::GetDecurserIndex() const
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        return -1;
    }

    int32 index = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive())
        {
            continue;
        }
        switch (member->getClass())
        {
            case CLASS_MAGE:
            case CLASS_DRUID:
            case CLASS_SHAMAN:
                break;
            default:
                continue;
        }
        if (member == bot)
        {
            return index;
        }
        ++index;
    }
    return -1;
}

Unit* NothDispelCurseAction::GetAssignedTarget()
{
    std::vector<Player*> cursed = helper.GetCursedMembers();
    if (cursed.empty())
    {
        return nullptr;
    }

    float range = botAI->GetRange("heal");
    int32 index = GetDecurserIndex();
    if (index >= 0 && index < static_cast<int32>(cursed.size()) && bot->IsWithinDistInMap(cursed[index], range))
    {
        return cursed[index];
    }

    // More decursers than targets, or the assigned one is out of range - take whatever is left.
    for (Player* member : cursed)
    {
        if (bot->IsWithinDistInMap(member, range))
        {
            return member;
        }
    }
    return nullptr;
}

bool NothDispelCurseAction::isUseful()
{
    if (!helper.UpdateBossAI() || !NaxxCanDispelCurse(botAI, bot))
    {
        return false;
    }
    return GetAssignedTarget() != nullptr;
}

bool NothDispelCurseAction::Execute(Event event)
{
    Unit* target = GetAssignedTarget();
    if (!target)
    {
        return false;
    }

    if (bot->getClass() == CLASS_SHAMAN)
    {
        return botAI->CanCastSpell("cleanse spirit", target) && botAI->CastSpell("cleanse spirit", target);
    }
    return botAI->CanCastSpell("remove curse", target) && botAI->CastSpell("remove curse", target);
}
