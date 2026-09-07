/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PriestTriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

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

bool InnerFocusTrigger::IsActive()
{
    return SpellNoCooldownTrigger::IsActive() && !botAI->HasAura("inner focus", bot);
}

BindingHealTrigger::BindingHealTrigger(PlayerbotAI* botAI)
    : PartyMemberLowHealthTrigger(botAI, "binding heal", sPlayerbotAIConfig.lowHealth, 0)
{
}

bool BindingHealTrigger::IsActive()
{
    return PartyMemberLowHealthTrigger::IsActive() &&
           AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig.mediumHealth;
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
        {
            uint8 attackerCount = AI_VALUE(uint8, "attacker count");
            return attackerCount < minEnemies;
        }
    }

    // Not channeling Mind Sear
    return false;
}

bool MindFlayChannelCheckTrigger::IsActive()
{
    Spell* channeled = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (!channeled)
        return false;

    uint32 mindFlayId = AI_VALUE2(uint32, "spell id", "mind flay");
    if (!mindFlayId || channeled->m_spellInfo->Id != mindFlayId)
        return false;

    Unit* target = GetTarget();
    if (!target)
        return false;

    // Only clip once two of the three ticks have landed, otherwise we throw away more than we gain.
    Aura* aura = botAI->GetAura("mind flay", target, true);
    if (!aura || aura->GetDuration() > aura->GetMaxDuration() / 3)
        return false;

    uint32 mindBlastId = AI_VALUE2(uint32, "spell id", "mind blast");
    if (mindBlastId && !bot->HasSpellCooldown(mindBlastId))
        return true;

    return !botAI->HasAura("vampiric touch", target, false, true) ||
           !botAI->HasAura("devouring plague", target, false, true) ||
           !botAI->HasAura("shadow word: pain", target, false, true);
}

bool WeakenedSoulOnPartyMemberTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    return target->GetHealthPct() < sPlayerbotAIConfig.almostFullHealth &&
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

    if (target->GetHealthPct() >= sPlayerbotAIConfig.lowHealth || !botAI->HasAura("weakened soul", target))
        return false;

    // A priest without the talent has no Penance id at all, which counts as "cannot Penance" here.
    uint32 penanceId = AI_VALUE2(uint32, "spell id", "penance");
    return !penanceId || bot->HasSpellCooldown(penanceId);
}

bool PriestHymnOfHopeTrigger::IsActive()
{
    if (!AI_VALUE2(bool, "has mana", "self target"))
        return false;

    if (AI_VALUE2(uint8, "mana", "self target") >= sPlayerbotAIConfig.mediumMana)
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
    if (AI_VALUE2(uint8, "health", "self target") <= sPlayerbotAIConfig.mediumHealth)
        return false;

    return (target->GetHealth() / AI_VALUE(float, "estimated group dps")) <= lifeTime;
}
