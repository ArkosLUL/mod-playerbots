/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "LastMovementValue.h"
#include "Playerbots.h"

namespace
{
    // MoveInside parks the bot at exactly this distance from the waypoint along its follow angle, so
    // it has to stay under the tightest wedge's clearance: waypoint 1 is only ~3.9 yd from the
    // section 1/2 divider.
    constexpr float SafeZoneSpread = 3.0f;
    constexpr float MainTankSafeZoneSpread = 2.0f;
    constexpr float PlatformSpread = 2.0f;
}  // namespace

bool HeiganDanceAction::MoveToSafeZone(float tolerance)
{
    std::pair<float, float> const& safe = helper.waypoints[helper.SafeIndex()];
    return MoveInside(bot->GetMapId(), safe.first, safe.second, helper.arenaZ, tolerance,
                      MovementPriority::MOVEMENT_COMBAT);
}

bool HeiganDanceAction::MoveToPlatform()
{
    if (MoveTo(bot->GetMapId(), helper.platform.first, helper.platform.second, helper.platformZ, false, false, false,
               false, MovementPriority::MOVEMENT_COMBAT))
    {
        return true;
    }
    return MoveInside(bot->GetMapId(), helper.platform.first, helper.platform.second, helper.platformZ, PlatformSpread,
                      MovementPriority::MOVEMENT_COMBAT);
}

bool HeiganDanceMeleeAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    // A main tank who lost the boss has a bigger problem than the dance; let the threat actions run.
    if (!helper.IsFastDance() && botAI->IsMainTank(bot) && !AI_VALUE2(bool, "has aggro", "boss target"))
    {
        return false;
    }

    if (!helper.IsSynced())
    {
        // No clock, so no safe zone to step to. Wait it out on the ledge rather than hand the bot
        // back to normal combat AI in a room that erupts every 10s. The main tank cannot leave -
        // dragging Heigan up the ramp would evade the encounter.
        if (botAI->IsMainTank(bot) || !helper.ShouldHoldLedge())
        {
            return false;
        }
        return MoveToPlatform();
    }

    // Everyone but the tank settles a few yards off the waypoint centre, otherwise the whole melee
    // group stacks on one point and never gets close enough to swing.
    return MoveToSafeZone(botAI->IsMainTank(bot) ? MainTankSafeZoneSpread : SafeZoneSpread);
}

bool HeiganDanceRangedAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    if (helper.ShouldHoldLedge())
    {
        Unit* boss = helper.GetBoss();
        bool tooCloseToBoss = boss && bot->IsWithinDistInMap(boss, 20.0f);

        if (!helper.IsOnPlatform() || tooCloseToBoss)
        {
            return MoveToPlatform();
        }
        return false;
    }

    if (!helper.IsSynced())
    {
        return false;
    }

    std::pair<float, float> const& safe = helper.waypoints[helper.SafeIndex()];
    if (bot->GetDistance2d(safe.first, safe.second) > SafeZoneSpread)
    {
        bot->CastStop();
    }
    return MoveToSafeZone(SafeZoneSpread);
}

Unit* HeiganDispelDecrepitFeverAction::GetDecrepitFeverTarget() const
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    // Prioritize the main tank if possible.
    Unit* best = nullptr;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        if (!NaxxSpellIds::HasAnyAura(member, {NaxxSpellIds::DecrepitFever10, NaxxSpellIds::DecrepitFever25}))
            continue;

        if (!bot->IsWithinDistInMap(member, botAI->GetRange("heal")))
            continue;

        if (botAI->IsMainTank(member))
            return member;

        // Keep first match as fallback.
        if (!best)
            best = member;
    }
    return best;
}

bool HeiganDispelDecrepitFeverAction::CanDispelDisease() const
{
    if (!bot->IsAlive())
    {
        return false;
    }

    // Keep it simple: only classes that can dispel disease in WotLK.
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
            return botAI->CanCastSpell("cleanse", bot) || botAI->CanCastSpell("purify", bot);
        case CLASS_PRIEST:
            return botAI->CanCastSpell("cure disease", bot) || botAI->CanCastSpell("abolish disease", bot);
        case CLASS_SHAMAN:
            return botAI->CanCastSpell("cure disease", bot) || botAI->CanCastSpell("cleanse spirit", bot);
        default:
            return false;
    }
}

bool HeiganDispelDecrepitFeverAction::isUseful()
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    return CanDispelDisease() && GetDecrepitFeverTarget();
}

bool HeiganDispelDecrepitFeverAction::Execute(Event event)
{
    Unit* target = GetDecrepitFeverTarget();
    if (!target)
    {
        return false;
    }
    if (bot->getClass() == CLASS_PALADIN)
    {
        if (botAI->CanCastSpell("cleanse", target) && botAI->CastSpell("cleanse", target))
        {
            return true;
        }
        return botAI->CanCastSpell("purify", target) && botAI->CastSpell("purify", target);
    }
    if (botAI->CanCastSpell("cure disease", target) && botAI->CastSpell("cure disease", target))
    {
        return true;
    }

    if (botAI->CanCastSpell("cleanse spirit", target) && botAI->CastSpell("cleanse spirit", target))
    {
        return true;
    }

    return botAI->CanCastSpell("abolish disease", target) && botAI->CastSpell("abolish disease", target);
}
