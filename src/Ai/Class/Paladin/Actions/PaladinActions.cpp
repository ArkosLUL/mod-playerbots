/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PaladinActions.h"
#include "AiFactory.h"
#include "Event.h"
#include "PaladinHelper.h"
#include "Pet.h"
#include "Playerbots.h"
#include "SharedDefines.h"

CastHolyShieldAction::CastHolyShieldAction(PlayerbotAI* botAI)
    : CastBuffSpellAction(botAI, "holy shield", false, ai::paladin::HOLY_SHIELD_REFRESH_MS)
{
}

bool CastSealSpellAction::isUseful()
{
    return AI_VALUE2(bool, "combat", "self target");
}

Value<Unit*>* CastTurnUndeadAction::GetTargetValue()
{
    return context->GetValue<Unit*>("cc target", getName());
}

Unit* CastHandOfFreedomOnPartyAction::GetTarget()
{
    bool const selfImpaired = botAI->IsMovementImpaired(bot);
    bool const hasSelfHand =
        selfImpaired && ai::paladin::HasAnyPaladinHandFromCaster(bot, bot);

    if (!bot->GetGroup())
    {
        if (selfImpaired && !hasSelfHand)
            return bot;

        return nullptr;
    }

    if (selfImpaired && !hasSelfHand)
        return bot;

    return CastBuffSpellAction::GetTarget();
}

Value<Unit*>* CastHandOfFreedomOnPartyAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member snared target");
}

bool CastHandOfFreedomOnPartyAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    return CastBuffSpellAction::isUseful() &&
           !ai::paladin::HasAnyPaladinHandFromCaster(target, bot);
}

Unit* CastRighteousDefenseAction::GetTarget()
{
    Unit* current_target = AI_VALUE(Unit*, "current target");
    if (!current_target)
        return nullptr;

    return current_target->GetVictim();
}

bool CastLayOnHandsAction::isUseful()
{
    return CastHealingSpellAction::isUseful() && !botAI->HasAura("forbearance", GetTarget());
}

bool CastLayOnHandsOnPartyAction::isUseful()
{
    return CastHealingSpellAction::isUseful() && !botAI->HasAura("forbearance", GetTarget());
}

bool CastDivinePleaAction::isUseful()
{
    if (!CastBuffSpellAction::isUseful())
        return false;

    // Unglyphed Divine Plea costs 50% healing done for 15s, which is most of a raid damage window.
    if (!PlayerbotAI::IsHeal(bot) || bot->HasAura(ai::paladin::SPELL_GLYPH_DIVINE_PLEA))
        return true;

    // Nearly out of mana beats the penalty - a healer with an empty bar heals nothing at all.
    if (AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.LowMana)
        return true;

    return AI_VALUE2(uint8, "aoe heal", "medium") == 0;
}

bool CastDivineIlluminationAction::isUseful()
{
    if (!CastBuffSpellAction::isUseful())
        return false;

    if (!PlayerbotAI::IsHeal(bot))
        return true;

    // Both are 3 minute cooldowns, so they realign every use: spending the -50% cost window while
    // Avenging Wrath is up is where it returns the most healing per point of mana. On its own it
    // only comes out when the bar is nearly empty anyway.
    return botAI->HasAura("avenging wrath", bot) ||
           AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.LowMana;
}

bool CastHandOfSacrificeOnPartyAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    // It takes 30% of the target's damage, so a hurt paladin kills itself with it.
    if (AI_VALUE2(uint8, "health", "self target") < 50)
        return false;

    return CastProtectSpellAction::isUseful() && !ai::paladin::HasAnyPaladinHandFromCaster(target, bot);
}

Value<Unit*>* CastBeaconOfLightOnTankAction::GetTargetValue() { return context->GetValue<Unit*>("tank to beacon"); }

Value<Unit*>* CastSacredShieldOnTankAction::GetTargetValue() { return context->GetValue<Unit*>("tank to beacon"); }

bool CastDivineSacrificeAction::isUseful()
{
    // Same reason as Hand of Sacrifice: it redirects raid damage onto the paladin.
    if (AI_VALUE2(uint8, "health", "self target") < 50)
        return false;

    return GetTarget() && (GetTarget() != nullptr) && CastSpellAction::isUseful() &&
           !botAI->HasAura("divine guardian", GetTarget(), false, false, -1, true);
}

bool CastCancelDivineSacrificeAction::Execute(Event /*event*/)
{
    botAI->RemoveAura("divine sacrifice");
    return true;
}

bool CastCancelDivineSacrificeAction::isUseful()
{
    return botAI->HasAura("divine sacrifice", GetTarget(), false, true, -1, true);
}
