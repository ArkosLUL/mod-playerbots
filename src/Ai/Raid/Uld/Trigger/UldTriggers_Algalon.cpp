#include "UldTriggers_Algalon.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

//
// Algalon the Observer
//

// Dodge the Cosmic Smash meteors that fall on the asteroid-target stalkers
bool AlgalonCosmicSmashTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon the observer");
    if (!boss || !boss->IsAlive())
        return false;

    // Meteor damage falls off past ~10 yd; move away if we are still in the blast radius
    Creature* asteroid = bot->FindNearestCreature(NPC_ALGALON_ASTEROID_TARGET_1, 11.0f);
    if (!asteroid)
        asteroid = bot->FindNearestCreature(NPC_ALGALON_ASTEROID_TARGET_2, 11.0f);

    return asteroid != nullptr;
}

// Big Bang is raid-wide lethal to anyone not phased; entering a Black/Worm Hole grants the safe phase aura
bool AlgalonBigBangTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon the observer");
    if (!boss || !boss->IsAlive())
        return false;

    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG))
        return false;

    // Already safe inside a hole
    if (bot->HasAura(SPELL_ALGALON_BLACK_HOLE_DAMAGE))
        return false;

    // The designated Shadow Priest soaks Big Bang with Dispersion instead of hiding
    if (GetAlgalonBigBangDispersionPriest(bot) == bot)
        return false;

    return true;
}

// The designated Shadow Priest stays out and Disperses to survive Big Bang instead of hiding in a hole
bool AlgalonBigBangDispersionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon the observer");
    if (!boss || !boss->IsAlive())
        return false;

    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG))
        return false;

    // Only the designated (first alive) Shadow Priest reacts this way
    if (GetAlgalonBigBangDispersionPriest(bot) != bot)
        return false;

    // Already mitigating
    if (bot->HasAura(SPELL_DISPERSION))
        return false;

    return true;
}

// Off-tank taunts before the active tank reaches ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS Phase Punch stacks
bool AlgalonPhasePunchSwapTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon the observer");
    if (!boss || !boss->IsAlive())
        return false;

    // Only the two designated swap partners (main tank + first assist tank) trade the boss
    bool const isMainTank = botAI->IsMainTank(bot);
    bool const isFirstAssistTank = botAI->IsAssistTankOfIndex(bot, 0);
    if (!isMainTank && !isFirstAssistTank)
        return false;

    // The bot must be the off-tank (not the one currently holding the boss)
    Unit* activeTank = boss->GetVictim();
    if (!activeTank || activeTank == bot)
        return false;

    // The active tank must be the bot's swap partner, so the taunt is symmetric both ways
    Player* activeTankPlayer = activeTank->ToPlayer();
    if (!activeTankPlayer)
        return false;

    bool const partnerIsSwapTank = isMainTank ? PlayerbotAI::IsAssistTankOfIndex(activeTankPlayer, 0)
                                              : PlayerbotAI::IsMainTank(activeTankPlayer);
    if (!partnerIsSwapTank)
        return false;

    // Don't taunt if our own stacks are still high (they must decay first)
    Aura* selfAura = bot->GetAura(SPELL_ALGALON_PHASE_PUNCH);
    if (selfAura && selfAura->GetStackAmount() >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
        return false;

    // Swap once the active tank reaches the stack threshold
    Aura* tankAura = activeTank->GetAura(SPELL_ALGALON_PHASE_PUNCH);
    if (!tankAura || tankAura->GetStackAmount() < ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
        return false;

    return true;
}

// The designated kiter leads each active Living Constellation onto a live Black Hole (both despawn)
bool AlgalonConstellationKiteTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon the observer");
    if (!boss || !boss->IsAlive())
        return false;

    if (!IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    // Only active constellations are selectable; passive/pre-activation ones are flagged out.
    // Fire whether or not a Black Hole exists: with one, the action drags the constellation
    // through it; without one, the action falls back to kiting it clear of the raid.
    Creature* constellation = bot->FindNearestCreature(PB_NPC_LIVING_CONSTELLATION, 100.0f);
    if (!constellation || !constellation->IsAlive() || constellation->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    return true;
}

// Focus-kill Unleashed Dark Matter when it spawns (phase 2)
bool AlgalonDarkMatterTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon the observer");
    if (!boss || !boss->IsAlive())
        return false;

    if (!IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    Unit* darkMatter = GetFirstAliveUnitByEntry(botAI, PB_NPC_UNLEASHED_DARK_MATTER);
    if (!darkMatter)
        return false;

    // Skip if it is already the skull target
    Group* group = bot->GetGroup();
    if (group && group->GetTargetIcon(RtiTargetValue::skullIndex) == darkMatter->GetGUID())
        return false;

    return true;
}

//
// Algalon the Observer
//
bool AlgalonCollapsingStarTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon the observer");
    if (!boss || !boss->IsAlive())
        return false;

    if (!IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    // Unleashed Dark Matter owns the skull marker while it is up
    if (GetFirstAliveUnitByEntry(botAI, PB_NPC_UNLEASHED_DARK_MATTER))
        return false;

    Unit* star = GetFirstAliveUnitByEntry(botAI, PB_NPC_COLLAPSING_STAR);
    if (!star)
        return false;

    Group* group = bot->GetGroup();
    if (group && group->GetTargetIcon(RtiTargetValue::skullIndex) == star->GetGUID())
        return false;

    return true;
}
