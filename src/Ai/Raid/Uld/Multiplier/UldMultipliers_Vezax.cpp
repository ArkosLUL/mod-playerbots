/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Vezax.h"

#include "AttackAction.h"
#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_Vezax.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "WarlockActions.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

float VezaxControlMovementMultiplier::GetValue(Action* action)
{
    if (!action || !VezaxFormationActive(botAI))
        return 1.0f;

    // Only the roles the formation actually places. Melee ride the boss and keep every generic mover.
    // The main tank is not one of them any more: it holds the anchor slot, and FollowAction would
    // drag it off the spawn point that every other radius here is measured from.
    if (!botAI->IsRanged(bot) && !botAI->IsMainTank(bot))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting;
    // ReachTargetAction is what keeps the tank in melee and walks a healer into range of someone the
    // formation cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "vezax raid position action", "vezax mark of the faceless action",
        "vezax shadow crash dodge action", "vezax shadow crash soak action"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
}

float VezaxSuppressLifeTapMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<CastLifeTapAction*>(action))
        return 1.0f;

    // No mana test, because there is no amount of missing mana a tap here could fix. The glyph
    // refresh goes with it: that proc rides the same energize the boss makes everyone immune to.
    return VezaxEncounterActive(botAI) ? 0.0f : 1.0f;
}

float VezaxTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // The debuff one matters as much as the two pickers: it is what lands DoTs on whatever a caster
    // drifted onto, which on this boss is a vapor often enough to end hard mode.
    if (!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<TankAssistAction*>(action) &&
        !dynamic_cast<CastDebuffSpellOnAttackerAction*>(action))
    {
        return 1.0f;
    }

    return VezaxEncounterActive(botAI) ? 0.0f : 1.0f;
}

float VezaxHoldCastOutsideFieldMultiplier::GetValue(Action* action)
{
    if (!action || botAI->IsHeal(bot) || !botAI->IsRanged(bot) || botAI->IsMainTank(bot))
        return 1.0f;

    // Damage, DoTs and the wand. Buffs and defensives stay available - a held bot that could not
    // Barkskin through a leech tick is worse off than one that skipped a Shadow Bolt - and so does
    // every movement action, or it would stop dodging. Debuffs are deliberately not exempt: a
    // caster's DoTs are most of its mana here.
    //
    // Nothing exempts the class interrupts, and nothing needs to. Searing Flames is the only
    // interruptible cast Vezax has, and its own node sits above this one and casts through
    // botAI->CastSpell rather than a CastSpellAction, so a multiplier never sees it.
    bool const isDamage =
        dynamic_cast<MeleeAction*>(action) ||
        (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastBuffSpellAction*>(action) &&
         !dynamic_cast<CastHealingSpellAction*>(action));
    if (!isDamage)
        return 1.0f;

    if (!VezaxEncounterActive(botAI))
        return 1.0f;

    // 63277, not the linked 65269 that actually carries the mana cost cut: the link trails the field
    // by 11-15% of uptime, and a bot inside a field without it would hold its casts for something it
    // cannot influence.
    return bot->HasAura(SPELL_VEZAX_SHADOW_CRASH_FIELD) ? 1.0f : 0.0f;
}
