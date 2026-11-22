#include "UldTriggers_Algalon.h"

#include "EncounterHelpers.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidTankDefensive.h"
#include "SharedDefines.h"
#include "UldData.h"
#include "UldEncounter_Algalon.h"
#include "Unit.h"
#include <RtiTargetValue.h>

using namespace EncounterHelpers;

bool AlgalonBigBangSoakTrigger::IsActive()
{
    if (IsAlgalonPhased(bot) || !AlgalonBigBangLatched(bot) || (!AlgalonStaysOut(bot) && !AlgalonTankCaughtOut(bot)))
        return false;

    int32 const remaining = AlgalonBigBangRemainingMs(botAI);
    if (remaining < 0)
        return false;

    if (AlgalonCanDisperse(bot))
        return remaining <= ULDUAR_ALGALON_SOAK_DISPERSION_MS;

    return remaining <= ULDUAR_ALGALON_SOAK_DEFENSIVE_MS &&
           NextTankDefensive(botAI, bot, "algalon.defensive", true) != 0;
}

bool AlgalonBigBangExternalTrigger::IsActive()
{
    uint8 const playerClass = bot->getClass();
    if ((playerClass != CLASS_PRIEST && playerClass != CLASS_PALADIN) || IsAlgalonPhased(bot) ||
        !AlgalonBigBangLatched(bot))
        return false;

    if (AlgalonBigBangRemainingMs(botAI) < ULDUAR_ALGALON_EXTERNAL_MIN_REMAINING_MS)
        return false;

    Player* soaker = GetAlgalonBigBangSoaker(botAI);
    if (!soaker || soaker == bot || GetAlgalonBigBangBackup(botAI) == bot || soaker->HasSpell(SPELL_ALGALON_DISPERSION))
        return false;

    return GetAlgalonSoakerExternal(botAI, soaker) != nullptr;
}

bool AlgalonSoakerPrepTrigger::IsActive()
{
    if (!AlgalonBigBangLatched(bot) || IsAlgalonPhased(bot) || !PlayerbotAI::IsHeal(bot) ||
        AlgalonHideSlackMs(bot) <= ULDUAR_ALGALON_PREP_GCD_MS)
        return false;

    Player* soaker = GetAlgalonBigBangSoaker(botAI);
    if (!soaker || soaker == bot || IsAlgalonPhased(soaker) || soaker->HasSpell(SPELL_ALGALON_DISPERSION))
        return false;

    return GetAlgalonSoakerPrep(botAI, soaker) != nullptr;
}

bool AlgalonSoakerRescueTrigger::IsActive()
{
    Player* soaker = GetAlgalonRescueTarget(botAI);
    if (!soaker || soaker == bot || IsAlgalonPhased(bot) || AlgalonHoleNear(bot, ULDUAR_ALGALON_SHELTER_RADIUS) ||
        !PlayerbotAI::IsHeal(bot))
        return false;

    return GetAlgalonRescueSpell(botAI, soaker) != nullptr;
}

bool AlgalonTankHealCriticalTrigger::IsActive()
{
    if (!IsAlgalonTankHealer(bot))
        return false;

    AlgalonTankHealPick const pick = GetAlgalonTankHealPick(botAI);
    return pick.critical && GetAlgalonTankHealSpell(botAI, pick);
}

bool AlgalonTankHealTrigger::IsActive()
{
    if (!IsAlgalonTankHealer(bot))
        return false;

    AlgalonTankHealPick const pick = GetAlgalonTankHealPick(botAI);
    return pick.target && !pick.critical && GetAlgalonTankHealSpell(botAI, pick);
}

bool AlgalonTankBeaconTrigger::IsActive()
{
    if (bot->getClass() != CLASS_PALADIN || !IsAlgalonTankHealer(bot) || IsAlgalonPhased(bot) ||
        GetAlgalonTankHealPick(botAI).critical)
        return false;

    Player* target = GetAlgalonBeaconTarget(botAI);
    return target && botAI->CanCastSpell("beacon of light", target);
}

bool AlgalonTankDefensiveTrigger::IsActive() { return GetAlgalonTankCut(botAI).spellId != 0; }

bool AlgalonExplosionCoverTrigger::IsActive() { return GetAlgalonExplosionCover(botAI) != nullptr; }

bool AlgalonExplosionPersonalTrigger::IsActive() { return static_cast<bool>(GetAlgalonExplosionPersonal(botAI)); }

bool AlgalonShadowAuraTrigger::IsActive()
{
    return IsAlgalonShadowAuraPaladin(bot) && AlgalonInPhaseOne(botAI) &&
           !botAI->HasAura("shadow resistance aura", bot, false, true) &&
           botAI->CanCastSpell("shadow resistance aura", bot);
}

bool AlgalonFireTotemTrigger::IsActive()
{
    return IsAlgalonFireTotemShaman(bot) && !IsAlgalonPhased(bot) && !AlgalonBigBangCasting(botAI) &&
           !AI_VALUE2(bool, "has totem", "fire resistance totem") && botAI->CanCastSpell("fire resistance totem", bot);
}

bool AlgalonBigBangHideTrigger::IsActive() { return GetAlgalonHideRole(bot) == AlgalonHideRole::Run; }

bool AlgalonCosmicSmashTrigger::IsActive()
{
    if (!AlgalonEngaged(botAI))
        return false;

    bool const smash = AlgalonCosmicSmashThreatens(bot);
    bool const star = !smash && AlgalonDyingStarThreatens(bot);
    if (RaidObs::Active())
    {
        RaidObs::NoteDerived(bot, "algalon.hazard", smash ? "smash" : (star ? "star" : "none"));
        if (!smash && !star)
            RaidObs::NoteDerived(bot, "algalon.dodge", "idle");
    }

    return smash || star;
}

bool AlgalonLeaveBlackHoleTrigger::IsActive()
{
    if (!AlgalonEngaged(botAI))
        return false;

    if (IsAlgalonPhased(bot))
        return AlgalonHoleNear(bot, ULDUAR_ALGALON_HOLE_EXIT_RADIUS) ||
               AlgalonUnderStarNear(bot, ULDUAR_ALGALON_STAR_DYING_RADIUS);

    return !AlgalonBigBangCasting(botAI) && AlgalonHoleNear(bot, ULDUAR_ALGALON_SHELTER_RADIUS);
}

bool AlgalonThreatRedirectTrigger::IsActive()
{
    char const* spell = AlgalonThreatRedirectSpell(bot);
    if (!spell || IsAlgalonPhased(bot) || !AlgalonInOpener(botAI))
        return false;

    Player* tank = GetAlgalonOpenerTank(botAI);
    if (!tank || tank == bot)
        return false;

    float const range =
        bot->getClass() == CLASS_ROGUE ? ULDUAR_ALGALON_TRICKS_RANGE : ULDUAR_ALGALON_MISDIRECTION_RANGE;
    return bot->GetExactDist(tank) <= range && botAI->CanCastSpell(spell, tank);
}

bool AlgalonTankPickupTrigger::IsActive()
{
    if (AlgalonShouldPickUp(bot))
        return true;

    Unit* boss = GetAlgalon(botAI);
    return boss && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) && boss->GetVictim() == bot &&
           IsAlgalonSwapTank(bot) && !IsAlgalonPhased(bot) && AlgalonEngaged(botAI) &&
           AI_VALUE(Unit*, "current target") != boss;
}

bool AlgalonPhasePunchSwapTrigger::IsActive() { return AlgalonShouldSwap(bot); }

bool AlgalonYieldBossTrigger::IsActive()
{
    Unit* boss = GetAlgalon(botAI);
    return boss && AlgalonYieldsBoss(bot) && (bot->GetVictim() == boss || AI_VALUE(Unit*, "current target") == boss);
}

bool AlgalonConstellationTauntTrigger::IsActive()
{
    if (IsAlgalonPhased(bot) || !AlgalonEngaged(botAI) || AlgalonBigBangCasting(botAI) || AlgalonTakesBossNow(bot))
        return false;

    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    return constellation && constellation->GetVictim() != bot;
}

bool AlgalonDarkMatterTankTrigger::IsActive()
{
    if (IsAlgalonPhased(bot) || !AlgalonEngaged(botAI) || AlgalonBigBangCasting(botAI))
        return false;

    return GetAlgalonLooseDarkMatter(bot) != nullptr;
}

bool AlgalonConstellationKiteTrigger::IsActive()
{
    if (IsAlgalonPhased(bot) || !AlgalonEngaged(botAI) || AlgalonBigBangCasting(botAI) || AlgalonTakesBossNow(bot))
        return false;

    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    return constellation && constellation->GetVictim() == bot;
}

bool AlgalonStarTeamTrigger::IsActive()
{
    if (IsAlgalonPhased(bot) || !IsAlgalonStarTeam(bot) || !AlgalonEngaged(botAI))
        return false;

    Unit* star = GetAlgalonFocusStar(botAI);
    return star && AI_VALUE(Unit*, "current target") != star;
}

bool AlgalonStarMarkTrigger::IsActive()
{
    if (!IsMechanicTrackerBot(bot, ULDUAR_MAP_ID) || !AlgalonEngaged(botAI))
        return false;

    Unit* star = GetAlgalonFocusStar(botAI);
    Group* group = bot->GetGroup();
    return star && group && group->GetTargetIcon(RtiTargetValue::starIndex) != star->GetGUID();
}

bool AlgalonRaidPositionTrigger::IsActive()
{
    if (!AlgalonPresent(botAI))
        return false;

    if (AlgalonShouldRunForShelter(bot))
        return false;

    if (AlgalonOffTankWorking(bot))
        return false;

    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    return !constellation || constellation->GetVictim() != bot || AlgalonTakesBossNow(bot);
}
