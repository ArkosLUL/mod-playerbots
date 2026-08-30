/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSTriggers.h"
#include "EncounterHelpers.h"
#include "SharedDefines.h"

#include <cmath>

using namespace OsHelpers;

bool SartharionDpsTrigger::IsActive()
{
    if (!botAI->IsDps(bot))
        return false;

    // Shifted bots keep their own kill order inside the realm, where Sartharion is not resolvable.
    return SartharionEncounterActive(bot) || HasTwilightShift(bot);
}

bool SartharionMeleePositioningTrigger::IsActive()
{
    if (!botAI->IsMelee(bot) || botAI->IsMainTank(bot) || IsOffTank(bot))
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
    return botAI->IsMainTank(bot) && OnThePlatform(bot) && SartharionEncounterActive(bot);
}

bool OsDrakeLandingTrigger::IsActive()
{
    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    if (!IsOffTank(bot) && !(botAI->IsMelee(bot) && botAI->IsDps(bot)))
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

    // Active for the whole encounter, not only while something needs holding. With nothing up the
    // action parks the off-tank on its anchor, and that is what keeps it away from Sartharion.
    return true;
}

bool OsRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    // Live from the pull. For a hunter RedirectTankFor hands back the main tank for the first 10s,
    // which is what gets Misdirection onto him before anything else can take Sartharion off him. For a
    // rogue it returns nothing at all unless he is on a drake or the boss, which is what keeps a 30s
    // Tricks off Lava Blazes and whelps.
    Player* tank = RedirectTankFor(botAI, bot);
    return tank && tank != bot;
}

bool OsMainTankCooldownTrigger::IsActive()
{
    if (!botAI->IsMainTank(bot) || !OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    return MainTankCooldownWindowOpen(bot) && NextTankDefensive(botAI, bot) != nullptr;
}

bool OsTranquilizeTrigger::IsActive()
{
    // Encounter-gated ahead of the sweep: without it every hunter ran a 35yd search every tick
    // anywhere on map 615, trash and pre-pull included.
    if (bot->getClass() != CLASS_HUNTER || !OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    return TranquilizeTargetFor(bot) != nullptr;
}

bool OsRaidHoldTrigger::IsActive()
{
    if (!botAI->IsRanged(bot) && !botAI->IsHeal(bot))
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    if (std::abs(bot->GetPositionY() - SafeCorridorY(bot)) > CORRIDOR_ARRIVAL_TOLERANCE)
        return true;

    return std::abs(bot->GetPositionX() - RaidLineX(bot)) > RAID_LINE_TOLERANCE_X;
}

bool OsSartharionFlankTrigger::IsActive()
{
    if (!botAI->IsMelee(bot) || botAI->IsMainTank(bot) || IsOffTank(bot))
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    Unit* boss = GetSartharion(bot);
    if (!boss || bot->GetVictim() != boss)
        return false;

    // Gated on standing in a cone rather than on being off the flank. He sits 10-12yd from the tank, so
    // a 17yd corridor swap turns him ~99 degrees; tracking that every tick would walk melee a 32yd arc
    // around him on every wave, for nothing.
    //
    // Being south of him is the other way to be wrong, and the facing has nothing to do with it: the
    // right-wave corridor dodge takes melee down to Y 490, and standing there is perfectly safe from the
    // cones, so without this nothing ever brings them back north. He is parked, so his own Y does not
    // move and there is 14yd between it and the northern flank - no room for this to chatter.
    return InSartharionCone(boss, *bot) || bot->GetPositionY() < boss->GetPositionY();
}

bool OsDrakeRearTrigger::IsActive()
{
    if (!botAI->IsMelee(bot) || botAI->IsMainTank(bot) || IsOffTank(bot))
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    Unit* drake = bot->GetVictim();
    if (!drake || !IsDrakeEntry(drake->GetEntry()))
        return false;

    // The gate, not the destination: the action draws inside 150 degrees and this releases at 140, so
    // arriving ends it. Without that gap the redraw every tick would walk melee around the tail.
    return !BehindDrake(drake, *bot);
}

bool OsTankShapeshiftTrigger::IsActive()
{
    if (bot->getClass() != CLASS_DRUID || !botAI->IsTank(bot))
        return false;

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    // Only a druid who actually tanks in a form. One with neither spell is a healer or a moonkin
    // standing in for a tank, and shifting him would cost more than it buys.
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

    // Both acolytes are worth the trip and Tenebron's eggs are not, and the portal itself cannot say
    // which of them is on the other side.
    if (!TwilightRealmWorthEntering(bot))
        return false;

    return bot->FindNearestGameObject(GoId::TwilightPortal, 100.0f) != nullptr;
}

bool TwilightPortalExitTrigger::IsActive()
{
    // Advisory only. The instance refcounts one shared portal across all three drakes and force
    // removes Twilight Shift raid-wide when the count hits zero, so bots get yanked out on a schedule
    // no bot chooses; this just covers the case where the adds die and the count has not zeroed yet.
    return HasTwilightShift(bot) && !TwilightAddsAlive(bot);
}
