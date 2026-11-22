/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSTriggers.h"
#include "EncounterHelpers.h"
#include "RaidObs.h"
#include "SharedDefines.h"

#include <cmath>

using namespace OsHelpers;

bool SartharionDpsTrigger::IsActive()
{
    if (!PlayerbotAI::IsDps(bot))
        return false;

    return SartharionEncounterActive(bot) || HasTwilightShift(bot);
}

bool SartharionMeleePositioningTrigger::IsActive()
{
    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsMainTank(bot) || IsOffTank(bot))
        return false;

    return SartharionEncounterActive(bot) && bot->IsInCombat();
}

bool OsTsunamiCorridorTrigger::IsActive()
{
    return NeedsTsunamiDodge(bot);
}

bool OsTwilightFissureTrigger::IsActive()
{
    return NeedsFissureDodge(bot);
}

bool OsMainTankHoldTrigger::IsActive()
{
    return PlayerbotAI::IsMainTank(bot) && OnThePlatform(bot) && SartharionEncounterActive(bot);
}

bool OsDrakeLandingTrigger::IsActive()
{
    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    if (!IsOffTank(bot) && !(PlayerbotAI::IsMelee(bot) && PlayerbotAI::IsDps(bot)))
        return false;

    Unit* drake = FindInboundDrake(bot);
    return drake && SecondsUntilLanding(drake) <= 3.0f;
}

bool OsOffTankHoldTrigger::IsActive()
{
    if (!IsOffTank(bot) || !OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    if (!RequireOffTank(botAI, bot))
        return false;

    return true;
}

bool OsRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    Player* tank = RedirectTankFor(botAI, bot);
    return tank && tank != bot;
}

bool OsMainTankCooldownTrigger::IsActive()
{
    if (!PlayerbotAI::IsMainTank(bot) || !OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    return MainTankCooldownWindowOpen(bot) && NextTankDefensive(botAI, bot, nullptr) != 0;
}

bool OsTranquilizeTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER || !OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    return TranquilizeTargetFor(bot) != nullptr;
}

bool OsRaidHoldTrigger::IsActive()
{
    if (!PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot))
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    if (std::abs(bot->GetPositionY() - SafeCorridorY(bot)) > CORRIDOR_ARRIVAL_TOLERANCE)
        return true;

    return std::abs(bot->GetPositionX() - RaidLineX(bot)) > RAID_LINE_TOLERANCE_X;
}

bool OsSartharionFlankTrigger::IsActive()
{
    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsMainTank(bot) || IsOffTank(bot))
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    Unit* boss = GetSartharion(bot);
    if (!boss || bot->GetVictim() != boss)
        return false;

    return InSartharionCone(boss, *bot) || bot->GetPositionY() < boss->GetPositionY();
}

bool OsDrakeRearTrigger::IsActive()
{
    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsMainTank(bot) || IsOffTank(bot))
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    Unit* drake = bot->GetVictim();
    if (!drake || !IsDrakeEntry(drake->GetEntry()))
        return false;

    return !BehindDrake(drake, *bot);
}

bool OsTankShapeshiftTrigger::IsActive()
{
    if (bot->getClass() != CLASS_DRUID || !PlayerbotAI::IsTank(bot))
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    if (!botAI->CanCastSpell("dire bear form", bot) && !botAI->CanCastSpell("bear form", bot))
        return false;

    return !botAI->HasAnyAuraOf(bot, "dire bear form", "bear form", nullptr);
}

bool OsOffPlatformTrigger::IsActive()
{
    return NeedsPlatformReturn(botAI, bot);
}

bool TwilightPortalEnterTrigger::IsActive()
{
    if (!SartharionEncounterActive(bot) || HasTwilightShift(bot) || !PortalSquadMember(bot))
        return false;

    if (!TwilightRealmWorthEntering(bot))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "sartharion.realm", "notworth");

        return false;
    }

    bool const portal = bot->FindNearestGameObject(GoId::TwilightPortal, 100.0f) != nullptr;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "sartharion.realm", portal ? "enter" : "noportal");

    return portal;
}

bool TwilightPortalExitTrigger::IsActive()
{
    return HasTwilightShift(bot) && !TwilightAddsAlive(bot);
}
