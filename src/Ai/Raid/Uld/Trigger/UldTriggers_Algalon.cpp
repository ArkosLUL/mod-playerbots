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
           NextTankDefensive(botAI, bot, "algalon.defensive", true) != nullptr;
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

bool AlgalonCosmicSmashTrigger::IsActive() { return AlgalonEngaged(botAI) && AlgalonCosmicSmashThreatens(bot); }

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
bool AlgalonTankPickupTrigger::IsActive()
{
    if (IsAlgalonPhased(bot) || !IsAlgalonSwapTank(bot) || !AlgalonEngaged(botAI))
        return false;

    Unit* boss = GetAlgalon(botAI);
    if (!boss || boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (holder && IsAlgalonSwapTank(holder) && !IsAlgalonPhased(holder))
        return false;

    // Mid cast the elected soaker is the one staying out, so it is the one to take him.
    if (AlgalonBigBangLatched(bot))
        return GetAlgalonBigBangSoaker(botAI) == bot && holder != bot;

    return GetAlgalonPickupTank(botAI) == bot;
}

bool AlgalonPhasePunchSwapTrigger::IsActive()
{
    if (IsAlgalonPhased(bot) || !IsAlgalonSwapTank(bot) || !AlgalonEngaged(botAI))
        return false;

    Player* holder = GetAlgalonBossTank(botAI);
    if (!holder || holder == bot || !IsAlgalonSwapTank(holder))
        return false;

    // The holder is this cast's soaker. Taking him now leaves the soaker hiding and the wrong tank out.
    if (AlgalonBigBangCasting(botAI))
        return false;

    if (IsAlgalonPhased(holder))
        return true;

    uint8 const own = GetAlgalonPhasePunchStacks(bot);
    uint8 const theirs = GetAlgalonPhasePunchStacks(holder);
    if (own >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
        return false;

    if (theirs >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
        return true;

    return theirs >= ULDUAR_ALGALON_EARLY_SWAP_STACKS && own <= ULDUAR_ALGALON_EARLY_SWAP_PARTNER_STACKS &&
           AlgalonBigBangWithin(botAI, ULDUAR_ALGALON_EARLY_SWAP_SECONDS);
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

    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    return !constellation || constellation->GetVictim() != bot;
}
