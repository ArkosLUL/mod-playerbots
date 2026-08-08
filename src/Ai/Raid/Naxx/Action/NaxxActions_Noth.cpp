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

    void KeepLowestGuid(Unit* candidate, Unit*& best)
    {
        if (!best || candidate->GetGUID() < best->GetGUID())
        {
            best = candidate;
        }
    }

    // One add type, picked four ways. The tank wants whatever is closest to it; DPS go by GUID so
    // that every bot in the raid ends up on the same add instead of each chasing its own nearest.
    struct AddPick
    {
        Unit* loose = nullptr;
        Unit* nearest = nullptr;
        Unit* tanked = nullptr;
        Unit* any = nullptr;
    };
}  // namespace

bool NothChooseTargetAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Unit* boss = helper.GetBoss();
    bool balcony = helper.IsBalconyPhase();
    bool ownsAdds = botAI->IsAssistTank(bot) || (botAI->IsMainTank(bot) && !helper.GetAliveAssistTank());

    // Noth drops out of "attackers" for the whole balcony phase, so the main tank correctly picks up
    // an add - and then keeps it, because an explicit main tank in a group with two tanks sticks to
    // its current target (TankTargetValue.cpp, FindTankTargetSmartStrategy::IsBetter). Pin him back
    // on the boss the moment the balcony flag clears - unless there is nobody else to hold the adds.
    // A tank doing both jobs still comes back for Blink: the taunt that undoes the threat reset only
    // fires while the boss is the current target, and an add can wait those few seconds.
    if (!balcony && boss && botAI->IsMainTank(bot) && (!ownsAdds || helper.IsBlinkWindow()))
    {
        return AI_VALUE(Unit*, "current target") != boss && Attack(boss);
    }

    AddPick guardians;
    AddPick champions;
    AddPick warriors;

    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
        {
            continue;
        }

        AddPick* pick = nullptr;
        if (helper.IsGuardian(unit))
        {
            pick = &guardians;
        }
        else if (helper.IsChampion(unit))
        {
            pick = &champions;
        }
        else if (helper.IsWarrior(unit))
        {
            pick = &warriors;
        }
        else
        {
            continue;
        }

        KeepNearest(bot, unit, pick->nearest);
        KeepLowestGuid(unit, pick->any);

        Player* victim = unit->GetVictim() ? unit->GetVictim()->ToPlayer() : nullptr;
        if (victim && botAI->IsTank(victim))
        {
            KeepLowestGuid(unit, pick->tanked);
        }
        // Every add is summoned with SetInCombatWithZone() and runs at a random raid member, so
        // "loose" is the normal state right after a spawn and is what the add tank has to chase.
        else if (ownsAdds && victim)
        {
            KeepNearest(bot, unit, pick->loose);
        }
    }

    std::vector<Unit*> targets;
    if (ownsAdds)
    {
        targets = {guardians.loose, champions.loose, warriors.loose};
        // A main tank covering both jobs holds the boss too, so he only leaves it for an add that
        // nobody has picked up yet.
        if (!balcony && botAI->IsMainTank(bot))
        {
            targets.push_back(boss);
        }
        targets.insert(targets.end(), {guardians.nearest, champions.nearest, warriors.nearest});
        if (!balcony)
        {
            targets.push_back(boss);
        }
    }
    else if (balcony)
    {
        // Anything a tank already holds comes first, whatever its type: chasing the nearest add
        // instead just rips it off the add tank.
        targets = {guardians.tanked, champions.tanked, warriors.tanked,
                   guardians.any,    champions.any,    warriors.any};
    }
    else if (botAI->IsRanged(bot))
    {
        // Ranged clear the adds. They spawn every 30s from alcoves 25-50 yd out and nobody but the
        // add tank ever touched them. Ordered by type rather than by distance so the whole back line
        // lands on the same add; lowest GUID inside a type keeps that stable as they trade health.
        targets = {guardians.tanked, guardians.any, champions.tanked, champions.any,
                   warriors.tanked,  warriors.any,  boss};
    }
    else if (helper.IsBlinkWindow())
    {
        // Nobody may touch Noth while the threat wipe settles. No boss fallback on purpose: with no
        // adds up, melee stand still rather than hand the boss to whoever hits hardest.
        targets = {guardians.tanked, guardians.any, champions.tanked, champions.any,
                   warriors.tanked,  warriors.any};
    }
    else
    {
        // Melee stay on Noth - chasing adds across the room costs more than the adds are worth, and
        // the ranged are already on them.
        targets = {boss};
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
        // Class is not enough: an Enhancement shaman never learns Cleanse Spirit, and counting one
        // would push every later decurser onto somebody else's target.
        if (!NaxxIsCurseDispeller(member))
        {
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
