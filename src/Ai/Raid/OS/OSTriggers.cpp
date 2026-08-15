/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSTriggers.h"

#include "RaidBossHelpers.h"
#include "SharedDefines.h"

#include <cmath>

using namespace OsHelpers;

namespace
{

// A shifted bot is in phase 16 and cannot be touched by, or even see, anything on the platform.
bool OnThePlatform(Player* bot)
{
    return bot->GetMapId() == OS_MAP_ID && !HasTwilightShift(bot);
}

}

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
    // Not a dodge: nothing in phase 16 can be touched by a wave. This is where the bot will be standing
    // when the shift is stripped, which the raid's shared portal refcount only allows once the last
    // twilight add is dead - and then on any tick, with no warning and no time to walk out of a lane.
    if (HasTwilightShift(bot))
    {
        if (!TwilightRealmWaveWait(bot) || WaveClearsY(bot->GetPositionY(), ClassifyTsunamiWave(bot)))
            return false;

        return std::abs(bot->GetPositionY() - SafeCorridorY(bot)) > CorridorToleranceFor(bot);
    }

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    TsunamiWave const wave = ClassifyTsunamiWave(bot);
    if (wave == TsunamiWave::None)
        return false;

    // A bot already standing where this pattern cannot reach stays put. The corridor holds are the
    // fallback, not the only safe ground: the off-tank's drake spots and melee behind a drake clear
    // some of the lines outright, and walking them 28yd to a lane that is no safer costs the trip
    // twice - and the walk back is what their own hold spends the next tick undoing.
    if (WaveClearsY(bot->GetPositionY(), wave))
        return false;

    return std::abs(bot->GetPositionY() - SafeCorridorY(bot)) > CorridorToleranceFor(bot);
}

bool OsTwilightFissureTrigger::IsActive()
{
    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    // requireSelectable off. The fissure is UNIT_FLAG_NOT_SELECTABLE for its whole life, so the
    // default search skipped it and this trigger had never fired.
    return FindUnitByEntries(bot, { NpcId::TwilightFissure, NpcId::TwilightFissureH },
                             FISSURE_CLEAR_RADIUS, false) != nullptr;
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
    if (bot->getClass() != CLASS_HUNTER || !OnThePlatform(bot))
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
    // Not gated on OnThePlatform: that excludes shifted bots, and the Twilight Realm sits on the same
    // coordinates. A shifted bot cannot resolve Sartharion anyway, so the encounter gate covers it.
    // Bounded by the 200yd Sartharion search, so this catches a bot on its first step off the arena
    // rather than one that is already halfway across the zone.
    if (bot->GetMapId() != OS_MAP_ID || !SartharionEncounterActive(bot))
        return false;

    if (!InsideRoom(bot))
        return true;

    // The pull drag corner is a hand-measured position 1.74yd south of PLATFORM_MIN_Y, so the one bot
    // meant to stand off the box is exempt until the drag latches.
    Unit* boss = GetSartharion(bot);
    if (botAI->IsMainTank(bot) && boss && !MainTankDragDone(boss))
        return false;

    // The room box is 18yd wider than the platform on X, so without this a bot standing in the lava
    // off the east rim reads as in the fight and nothing ever walks it back.
    return OffThePlatform(bot);
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
