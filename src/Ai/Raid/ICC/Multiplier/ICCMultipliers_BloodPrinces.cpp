/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_BloodPrinces.h"

#include "Creature.h"
#include "GenericSpellActions.h"
#include "ICCHelpers_BloodPrinces.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace
{
// Most area spells reach about 10 yd round their caster or target, so from this close one hits him too
constexpr float BRAKE_AREA_REACH = 15.0f;

// Effects whose hit carries damage, so they proc the bomb's Unstable or move a nucleus
bool LandsDamage(SpellInfo const* info)
{
    if (info->IsAutoRepeatRangedSpell())
        return true;

    for (SpellEffectInfo const& effect : info->Effects)
    {
        switch (effect.Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_TRIGGER_SPELL:
            case SPELL_EFFECT_TRIGGER_MISSILE:
                return true;
            case SPELL_EFFECT_APPLY_AURA:
                if (effect.ApplyAuraName == SPELL_AURA_PERIODIC_TRIGGER_SPELL)
                    return true;
                break;
            default:
                break;
        }
    }

    return false;
}
}  // namespace

float IccBpcSplashMultiplier::GetValue(Action* action)
{
    // Heals carry the Aoe threat type too.
    if (dynamic_cast<CastHealingSpellAction*>(action) || !IsRaidAreaDamage(botAI, action))
        return 1.0f;

    return IsBpcAreaDamageHeld(botAI) ? 0.0f : 1.0f;
}

float IccBpcBrakeMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    Creature* prince = GetBpcBrakeTarget(botAI);
    if (!prince)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == prince || (!target && AI_VALUE(Unit*, "current target") == prince))
        return 0.0f;

    return IsRaidAreaDamage(botAI, action) && bot->GetExactDist2d(prince) <= BRAKE_AREA_REACH ? 0.0f : 1.0f;
}

float IccBpcDirectHitMultiplier::GetValue(Action* action)
{
    CastSpellAction* cast = dynamic_cast<CastSpellAction*>(action);
    if (!cast || dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    Unit* pick = GetBpcDirectHitTarget(botAI);
    Unit* target = pick ? action->GetTarget() : nullptr;
    if (!pick || (target != pick && (target || AI_VALUE(Unit*, "current target") != pick)))
        return 1.0f;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", cast->getSpell());
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return !info || LandsDamage(info) ? 1.0f : 0.0f;
}
