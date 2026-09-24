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
#include "ThreatManager.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_Vezax.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "WarlockActions.h"
#include "VehicleActions.h"

#include <string>

using namespace EncounterHelpers;

namespace
{
bool IsVezaxDamageAction(Action* action)
{
    return dynamic_cast<MeleeAction*>(action) ||
           (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastBuffSpellAction*>(action) &&
            !dynamic_cast<CastHealingSpellAction*>(action));
}
}  // namespace

float VezaxSuppressLifeTapMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<CastLifeTapAction*>(action))
        return 1.0f;

    // No mana test, because there is no amount of missing mana a tap here could fix. The glyph
    // refresh goes with it: that proc rides the same energize the boss makes everyone immune to.
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
    if (!IsVezaxDamageAction(action))
        return 1.0f;

    if (!VezaxEncounterActive(botAI))
        return 1.0f;

    // 63277, not the linked 65269 that actually carries the mana cost cut: the link trails the field
    // by 11-15% of uptime, and a bot inside a field without it would hold its casts for something it
    // cannot influence.
    return bot->HasAura(SPELL_VEZAX_SHADOW_CRASH_FIELD) ? 1.0f : 0.0f;
}

float VezaxAnimusThreatMultiplier::GetValue(Action* action)
{
    if (!action || botAI->IsTank(bot) || botAI->IsHeal(bot) || !IsVezaxDamageAction(action))
        return 1.0f;

    Unit* animus = AI_VALUE(Unit*, "current target");
    if (!animus || animus->GetEntry() != NPC_VEZAX_SARONITE_ANIMUS || !animus->IsAlive())
        return 1.0f;

    Player* mainTank = GetGroupMainTank(bot);
    if (!mainTank || mainTank == bot)
        return 1.0f;

    // Off the threat list, not the "threat" value: that one is a uint8 percentage, and a bot far enough
    // past the tank to be holding the Animus wraps around to a small number.
    ThreatManager& threat = animus->GetThreatMgr();
    float const own = threat.GetThreat(bot);
    return own > 0.0f && own >= ULDUAR_VEZAX_ANIMUS_THREAT_SHARE * threat.GetThreat(mainTank) ? 0.0f : 1.0f;
}
