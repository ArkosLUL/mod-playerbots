#include "UldTriggers_Algalon.h"

#include "GameObject.h"
#include "Group.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldEncounter_Algalon.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

//
// Algalon the Observer
//

bool AlgalonResetEncounterStateTrigger::IsActive()
{
    if (bot->GetMapId() != ULDUAR_MAP_ID || AlgalonEncounterActive(botAI))
        return false;

    return algalonEncounterStates.find(bot->GetInstanceId()) != algalonEncounterStates.end();
}

// Big Bang is 76312 on 10-man and 107249 on 25-man to everyone the spell can see, at any range.
// Standing in a hole is not cover, it is a phase change - that is the whole mechanic.
bool AlgalonBigBangHideTrigger::IsActive()
{
    if (!AlgalonEncounterActive(botAI) || !AlgalonBigBangCasting(botAI))
        return false;

    if (bot->HasAura(SPELL_ALGALON_BLACK_HOLE_DAMAGE))
        return false;

    // Somebody has to still be standing when CheckTargets runs, or the spell finds no targets and
    // Algalon ascends and evades.
    if (GetAlgalonBigBangSoaker(botAI) == bot)
        return false;

    return GetAlgalonShelter(bot) != nullptr;
}

bool AlgalonBigBangSoakTrigger::IsActive()
{
    if (!AlgalonEncounterActive(botAI) || !AlgalonBigBangCasting(botAI))
        return false;

    if (bot->HasAura(SPELL_ALGALON_BLACK_HOLE_DAMAGE))
        return false;

    return GetAlgalonBigBangSoaker(botAI) == bot;
}

bool AlgalonCosmicSmashTrigger::IsActive()
{
    return AlgalonEncounterActive(botAI) && GetAlgalonCosmicSmashMarker(bot) != nullptr;
}

// Phase 1 holes land wherever a Collapsing Star happened to die, so bots end up standing in one
// without ever having run to it. Outside a Big Bang that is 1531 a tick for nothing.
bool AlgalonLeaveBlackHoleTrigger::IsActive()
{
    if (!AlgalonEncounterActive(botAI) || AlgalonBigBangCasting(botAI))
        return false;

    return GetAlgalonShelterUnderfoot(bot) != nullptr;
}

// Phase Punch stacks to five and then phases the tank out for ten seconds, with nobody on the boss.
// Two tanks trade him well before that; a raid that brought one is not supported here, because a
// damage dealer taking Quantum Strike dies in two swings.
bool AlgalonPhasePunchSwapTrigger::IsActive()
{
    if (!AlgalonEncounterActive(botAI))
        return false;

    Player* mainTank = GetGroupMainTank(botAI, bot);
    Player* offTank = GetGroupAssistTank(botAI, bot, 0);
    if (!mainTank || !offTank || mainTank == offTank)
        return false;

    if (bot != mainTank && bot != offTank)
        return false;

    Player* activeTank = GetAlgalonBossTank(botAI);
    if (!activeTank || activeTank == bot)
        return false;

    if (activeTank != mainTank && activeTank != offTank)
        return false;

    // The partner already rode to five and is phased out. Nothing is holding the boss, so taunt now
    // and worry about our own stacks after. During a Big Bang the whole raid wears that aura, which
    // is not the same thing at all.
    if (activeTank->HasAura(SPELL_ALGALON_BLACK_HOLE_DAMAGE) && !AlgalonBigBangCasting(botAI))
        return true;

    Aura* ownStacks = bot->GetAura(SPELL_ALGALON_PHASE_PUNCH);
    if (ownStacks && ownStacks->GetStackAmount() >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
        return false;

    Aura* partnerStacks = activeTank->GetAura(SPELL_ALGALON_PHASE_PUNCH);
    return partnerStacks && partnerStacks->GetStackAmount() >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS;
}

// A constellation picks its victim at activation and chases it. The one bot that must not be dragged
// anywhere is whoever is holding Algalon, so the other tank pulls that one off and kites it instead.
bool AlgalonConstellationTauntTrigger::IsActive()
{
    if (!AlgalonEncounterActive(botAI))
        return false;

    if (bot != GetGroupMainTank(botAI, bot) && bot != GetGroupAssistTank(botAI, bot, 0))
        return false;

    if (bot == GetAlgalonBossTank(botAI))
        return false;

    Unit* constellation = GetAlgalonConstellationOnBossTank(botAI);
    return constellation && constellation->GetVictim() != bot;
}

bool AlgalonConstellationKiteTrigger::IsActive()
{
    return AlgalonEncounterActive(botAI) && GetAlgalonKiteTarget(bot) != nullptr;
}

bool AlgalonCollapsingStarFocusTrigger::IsActive()
{
    if (!AlgalonEncounterActive(botAI) || !IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    Unit* star = GetAlgalonFocusStar(botAI);
    if (!star || !AlgalonStarKillWindowOpen(botAI))
        return false;

    Group* group = bot->GetGroup();
    return !group || group->GetTargetIcon(RtiTargetValue::skullIndex) != star->GetGUID();
}

// Unleashed Dark Matter runs at 1.43x player speed, so it cannot be kited - it gets picked up or it
// eats whoever it picked. Collecting them on the boss keeps the tank in its swap position and puts
// them where the melee already are.
bool AlgalonDarkMatterTankTrigger::IsActive()
{
    if (!AlgalonEncounterActive(botAI) || bot != GetAlgalonAddTank(botAI, bot))
        return false;

    Unit* darkMatter = GetFirstAliveUnitByEntry(botAI, PB_NPC_UNLEASHED_DARK_MATTER);
    return darkMatter && darkMatter->GetVictim() != bot;
}

bool AlgalonDarkMatterMarkTrigger::IsActive()
{
    if (!AlgalonEncounterActive(botAI) || !IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    Unit* darkMatter = GetFirstAliveUnitByEntry(botAI, PB_NPC_UNLEASHED_DARK_MATTER);
    if (!darkMatter)
        return false;

    Group* group = bot->GetGroup();
    return !group || group->GetTargetIcon(RtiTargetValue::skullIndex) != darkMatter->GetGUID();
}

bool AlgalonRaidPositionTrigger::IsActive()
{
    // Presence-gated, not combat-gated: the intro runs 26s on a first pull and that is the only free
    // window the raid gets to put 25 bots on their spots.
    return AlgalonEncounterActive(botAI);
}
