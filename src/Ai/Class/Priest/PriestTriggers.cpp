/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PriestTriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "TargetValue.h"

namespace
{
constexpr uint32 SPELL_SHADOW_WEAVING_R1 = 15257;
constexpr uint32 SPELL_SHADOW_WEAVING_R2 = 15331;
constexpr uint32 SPELL_SHADOW_WEAVING_R3 = 15332;
constexpr uint32 SPELL_SHADOW_WEAVING_BUFF = 15258;
constexpr uint32 VAMPIRIC_TOUCH_CAST_MS = 1500;
}  // namespace

bool ShadowProtectionTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("prayer of shadow protection", GetTarget());
}

bool PowerWordFortitudeTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("power word: fortitude", GetTarget()) &&
           !botAI->HasAura("prayer of fortitude", GetTarget());
}

bool DivineSpiritTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("divine spirit", GetTarget()) &&
           !botAI->HasAura("prayer of spirit", GetTarget());
}

bool InnerFireTrigger::IsActive()
{
    Unit* target = GetTarget();
    return SpellTrigger::IsActive() && !botAI->HasAura(spell, target);
}

bool FearWardOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", spell);
    if (!spellId || bot->HasSpellCooldown(spellId))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

bool ShadowformTrigger::IsActive() { return !botAI->HasAura("shadowform", bot); }

bool ShadowWordPainTrigger::IsActive()
{
    if (!DebuffTrigger::IsActive())
        return false;

    if (Aura const* weaving = bot->GetAura(SPELL_SHADOW_WEAVING_BUFF))
        if (weaving->GetStackAmount() >= weaving->GetSpellInfo()->StackAmount)
            return true;

    // passive talents only sit in the talent map, HasSpell never finds them
    uint8 const spec = bot->GetActiveSpec();
    return !bot->HasTalent(SPELL_SHADOW_WEAVING_R1, spec) && !bot->HasTalent(SPELL_SHADOW_WEAVING_R2, spec) &&
           !bot->HasTalent(SPELL_SHADOW_WEAVING_R3, spec);
}

// a recast resets VT's tick timer and drops the tick in progress, so start it one hasted cast
// before VT runs out
uint32 VampiricTouchTrigger::RefreshWindow(Player* bot)
{
    return uint32(VAMPIRIC_TOUCH_CAST_MS * bot->GetFloatValue(UNIT_MOD_CAST_SPEED));
}

bool VampiricTouchTrigger::IsActive()
{
    beforeDuration = RefreshWindow(bot);
    return DebuffTrigger::IsActive();
}

bool InnerFocusTrigger::IsActive()
{
    return SpellNoCooldownTrigger::IsActive() && !botAI->HasAura("inner focus", bot);
}

BindingHealTrigger::BindingHealTrigger(PlayerbotAI* botAI)
    : PartyMemberLowHealthTrigger(botAI, "binding heal", sPlayerbotAIConfig.LowHealth, 0)
{
}

bool BindingHealTrigger::IsActive()
{
    return PartyMemberLowHealthTrigger::IsActive() &&
           AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig.MediumHealth;
}

const std::set<uint32> MindSearChannelCheckTrigger::MIND_SEAR_SPELL_IDS = {
    48045,  // Mind Sear Rank 1
    53023   // Mind Sear Rank 2
};

bool MindSearChannelCheckTrigger::IsActive()
{
    Player* bot = botAI->GetBot();

    // Check if the bot is channeling a spell
    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
    {
        // Only trigger if the spell being channeled is Mind Sear
        if (MIND_SEAR_SPELL_IDS.count(spell->m_spellInfo->Id))
            return !HasEnoughAoeTargets(botAI, bot->GetPosition(), sPlayerbotAIConfig.SightDistance, minEnemies);
    }

    // Not channeling Mind Sear
    return false;
}

bool WeakenedSoulOnPartyMemberTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    return target->GetHealthPct() < sPlayerbotAIConfig.AlmostFullHealth &&
           botAI->HasAura("weakened soul", target);
}

bool PowerWordShieldOnMainTankTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || botAI->HasAura("weakened soul", target))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

bool FlashHealOnPartyMemberTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    if (target->GetHealthPct() >= sPlayerbotAIConfig.LowHealth || !botAI->HasAura("weakened soul", target))
        return false;

    // A priest without the talent has no Penance id at all, which counts as "cannot Penance" here.
    uint32 penanceId = AI_VALUE2(uint32, "spell id", "penance");
    return !penanceId || bot->HasSpellCooldown(penanceId);
}

bool PriestHymnOfHopeTrigger::IsActive()
{
    if (!AI_VALUE2(bool, "has mana", "self target"))
        return false;

    if (AI_VALUE2(uint8, "mana", "self target") >= sPlayerbotAIConfig.MediumMana)
        return false;

    // An 8 second channel must not start while somebody is about to die.
    return AI_VALUE2(uint8, "aoe heal", "critical") == 0;
}

bool PriestShadowWordDeathExecuteTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    // SW:D backlashes onto the caster whenever the target survives it.
    if (AI_VALUE2(uint8, "health", "self target") <= sPlayerbotAIConfig.MediumHealth)
        return false;

    return (target->GetHealth() / AI_VALUE(float, "estimated group dps")) <= lifeTime;
}
