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

//
// Algalon the Observer
//

// Whoever holds him stays out and eats the hit: he resets the moment nobody alive is left unphased.
// Big Bang is physical, so a tank's armour and a physical defensive do the work. The button follows
// what the bot can cast, not its slot: a promoted backup is still a priest.
bool AlgalonBigBangSoakTrigger::IsActive()
{
    if (IsAlgalonPhased(bot) || !AlgalonBigBangLatched(bot) || !AlgalonStaysOut(bot))
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
    {
        return false;
    }

    if (AlgalonBigBangRemainingMs(botAI) < ULDUAR_ALGALON_EXTERNAL_MIN_REMAINING_MS)
        return false;

    Player* soaker = GetAlgalonBigBangSoaker(botAI);
    if (!soaker || soaker == bot || GetAlgalonBigBangBackup(botAI) == bot)
        return false;

    return GetAlgalonSoakerExternal(botAI, soaker) != nullptr;
}

// One layer per healer while it still has a global cooldown to spare before its walk to the hole: the
// healers are phased for a second after the hit, and whatever is already ticking on him is all he has.
bool AlgalonSoakerPrepTrigger::IsActive()
{
    if (!AlgalonBigBangLatched(bot) || IsAlgalonPhased(bot) || AlgalonHideSlackMs(bot) <= ULDUAR_ALGALON_PREP_GCD_MS ||
        !PlayerbotAI::IsHeal(bot))
    {
        return false;
    }

    Player* soaker = GetAlgalonBigBangSoaker(botAI);
    if (!soaker || soaker == bot || IsAlgalonPhased(soaker))
        return false;

    return GetAlgalonSoakerPrep(botAI, soaker) != nullptr;
}

// Once out of the field, a healer's first cast goes on the soaker while he is low.
bool AlgalonSoakerRescueTrigger::IsActive()
{
    Player* soaker = GetAlgalonRescueTarget(botAI);
    if (!soaker || soaker == bot || IsAlgalonPhased(bot) || AlgalonHoleNear(bot, ULDUAR_ALGALON_SHELTER_RADIUS) ||
        !PlayerbotAI::IsHeal(bot))
    {
        return false;
    }

    return GetAlgalonRescueSpell(botAI, soaker) != nullptr;
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
        // The dodge writes its branch change-only, so without a reset two waves dodged the same way read as
        // one.
        if (!smash && !star)
            RaidObs::NoteDerived(bot, "algalon.dodge", "idle");
    }

    return smash || star;
}

// A bot still inside the field when its 10s phase ends is phased again within a second, and while
// phased it can't see the hole at all, only the cached position.
bool AlgalonLeaveBlackHoleTrigger::IsActive()
{
    if (!AlgalonEngaged(botAI))
        return false;

    if (IsAlgalonPhased(bot))
        return AlgalonHoleNear(bot, ULDUAR_ALGALON_HOLE_EXIT_RADIUS);

    return !AlgalonBigBangCasting(botAI) && AlgalonHoleNear(bot, ULDUAR_ALGALON_SHELTER_RADIUS);
}

// Misdirection and Tricks on the pull go to the tank holding him, or the one about to. Out of range the
// rogue waits: the class value would give Tricks to a dps, who then pulls him.
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

// At the end of the intro he swings at a random player, and a phased tank drops him the same way. A holder
// that got him by threat still targets whatever it hit before, a constellation for a former handler.
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

// Auto-attack is no action, so the damage guard never stops it, and white swings alone take him back.
bool AlgalonYieldBossTrigger::IsActive()
{
    Unit* boss = GetAlgalon(botAI);
    return boss && AlgalonYieldsBoss(bot) && (bot->GetVictim() == boss || AI_VALUE(Unit*, "current target") == boss);
}

bool AlgalonConstellationTauntTrigger::IsActive()
{
    if (IsAlgalonPhased(bot) || !AlgalonEngaged(botAI) || AlgalonBigBangCasting(botAI))
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
    if (IsAlgalonPhased(bot) || !AlgalonEngaged(botAI) || AlgalonBigBangCasting(botAI))
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

// For the humans in the raid. Not skull: every bot's "attack rti target" reads skull.
bool AlgalonStarMarkTrigger::IsActive()
{
    if (!IsMechanicTrackerBot(bot, ULDUAR_MAP_ID) || !AlgalonEngaged(botAI))
        return false;

    Unit* star = GetAlgalonFocusStar(botAI);
    Group* group = bot->GetGroup();
    return star && group && group->GetTargetIcon(RtiTargetValue::starIndex) != star->GetGUID();
}

// Presence gated, not combat gated: the intro is the only free window to put the raid on its spots.
bool AlgalonRaidPositionTrigger::IsActive()
{
    if (!AlgalonPresent(botAI))
        return false;

    // A hider belongs to its hole from the moment it runs; the soaker and backup keep their spots.
    if (AlgalonShouldRunForShelter(bot))
        return false;

    // The swap tank not holding him works the adds from wherever they are; its spot is for idle time.
    if (AlgalonOffTankWorking(bot))
        return false;

    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    return !constellation || constellation->GetVictim() != bot;
}
