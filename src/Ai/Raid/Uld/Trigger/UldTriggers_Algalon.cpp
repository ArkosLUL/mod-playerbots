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
    if (bot->getClass() != CLASS_PRIEST || IsAlgalonPhased(bot) || !AlgalonBigBangLatched(bot))
        return false;

    if (AlgalonBigBangRemainingMs(botAI) < ULDUAR_ALGALON_EXTERNAL_MIN_REMAINING_MS)
        return false;

    Player* soaker = GetAlgalonBigBangSoaker(botAI);
    if (!soaker || soaker == bot || GetAlgalonBigBangBackup(botAI) == bot)
        return false;

    return (!soaker->HasAura(SPELL_ALGALON_PAIN_SUPPRESSION) && botAI->CanCastSpell("pain suppression", soaker)) ||
           (!soaker->HasAura(SPELL_ALGALON_GUARDIAN_SPIRIT) && botAI->CanCastSpell("guardian spirit", soaker));
}

bool AlgalonBigBangHideTrigger::IsActive() { return GetAlgalonHideRole(bot) == AlgalonHideRole::Run; }

bool AlgalonCosmicSmashTrigger::IsActive()
{
    if (!AlgalonEngaged(botAI))
        return false;

    bool const threatened = AlgalonCosmicSmashThreatens(bot);
    // The dodge writes its branch change-only, so without a reset two waves dodged the same way read as one.
    if (!threatened && RaidObs::Active())
        RaidObs::NoteDerived(bot, "algalon.dodge", "idle");

    return threatened;
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

// At the end of the intro he swings at a random player, and a phased tank drops him the same way.
bool AlgalonTankPickupTrigger::IsActive() { return AlgalonShouldPickUp(bot); }

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
