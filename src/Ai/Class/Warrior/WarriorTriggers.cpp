/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "WarriorTriggers.h"

#include "ArmorDebuff.h"
#include "Playerbots.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"

namespace
{
constexpr uint32 SPELL_VIGILANCE = 50720;
constexpr uint32 SPELL_SHATTERING_THROW = 64382;
constexpr uint32 SPELL_DIVINE_SHIELD = 642;
constexpr uint32 SPELL_ICE_BLOCK = 45438;
constexpr uint32 SPELL_BLESSING_OF_PROTECTION = 41450;
constexpr uint32 SPELL_COMMANDING_PRESENCE_RANKS[] = { 12318, 12857, 12858, 12860, 12861 };
constexpr uint32 SPELL_SLAM_PROC = 46916;
constexpr uint32 SPELL_TASTE_FOR_BLOOD_PROC = 60503;
constexpr float REND_MIN_TIME_TO_DIE = 8.0f;
constexpr uint8 REND_RAGE_COST = 10;
constexpr uint32 DANCE_MIN_BLOODTHIRST_COOLDOWN_MS = 1000;
constexpr uint32 DANCE_MIN_WHIRLWIND_COOLDOWN_MS = 1500;
}

bool ShouldCastRend(PlayerbotAI* botAI, Unit* target)
{
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    // no Rend aura ever lands here, so Fury would stance dance for it all fight
    Creature* creature = target->ToCreature();
    if (creature && creature->HasMechanicTemplateImmunity(1ULL << MECHANIC_BLEED))
        return false;

    float groupDps = botAI->GetAiObjectContext()->GetValue<float>("estimated group dps")->Get();
    if (target->GetHealth() / groupDps < REND_MIN_TIME_TO_DIE)
        return false;

    Aura* rend = botAI->GetAura("rend", target, true);
    if (!rend)
        return true;

    // hasted Rend (mod-spell-tweaks, Trauma) usually has dead time after its last tick, since the
    // core caps ticks at max duration / interval. Recasting there costs nothing
    AuraEffect const* dot = rend->GetEffect(EFFECT_0);
    return dot && dot->GetTotalTicks() > 0 && dot->GetTickNumber() >= uint32(dot->GetTotalTicks());
}

bool BattleStanceDanceTrigger::IsActive()
{
    if (bot->GetShapeshiftForm() != FORM_BERSERKERSTANCE)
        return false;

    if (AI_VALUE2(uint8, "rage", "self target") < REND_RAGE_COST)
        return false;

    // Bloodsurge's Slam would take the GCD the swap is for
    if (bot->HasAura(SPELL_SLAM_PROC))
        return false;

    uint32 const bloodthirst = AI_VALUE2(uint32, "spell id", "bloodthirst");
    uint32 const whirlwind = AI_VALUE2(uint32, "spell id", "whirlwind");
    if (!bloodthirst || !whirlwind || bot->GetSpellCooldownDelay(bloodthirst) < DANCE_MIN_BLOODTHIRST_COOLDOWN_MS ||
        bot->GetSpellCooldownDelay(whirlwind) < DANCE_MIN_WHIRLWIND_COOLDOWN_MS)
    {
        return false;
    }

    uint32 const rend = AI_VALUE2(uint32, "spell id", "rend");
    SpellInfo const* rendInfo = rend ? sSpellMgr->GetSpellInfo(rend) : nullptr;
    if (!rendInfo || bot->GetGlobalCooldownMgr().HasGlobalCooldown(rendInfo))
        return false;

    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !bot->IsWithinMeleeRange(target))
        return false;

    uint32 const overpower = AI_VALUE2(uint32, "spell id", "overpower");
    if (overpower && bot->HasAura(SPELL_TASTE_FOR_BLOOD_PROC) && !bot->HasSpellCooldown(overpower))
        return true;

    return ShouldCastRend(botAI, target);
}

bool BloodrageBuffTrigger::IsActive()
{
    return AI_VALUE2(uint8, "health", "self target") >= sPlayerbotAIConfig.MediumHealth &&
           AI_VALUE2(uint8, "rage", "self target") < 20;
}

bool SunderArmorStackTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (!PlayerbotAI::IsTank(bot, false))
    {
        // Expose Armor and a worm pet's Acid Spit occupy the same 20% slot at full value, and a DPS
        // warrior has no threat reason to overwrite either. Tanks still sunder - for them it is a
        // threat tool.
        if (TargetHasNonSunderMajorArmorDebuff(botAI, target))
            return false;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive() || !member->IsInWorld() ||
                member->GetMapId() != bot->GetMapId())
            {
                continue;
            }

            if (member->getClass() == CLASS_WARRIOR && PlayerbotAI::IsTank(member, false))
                return false;
        }
    }

    Aura* aura = botAI->GetAura("sunder armor", target, false, true);
    return !aura || aura->GetStackAmount() < 5 || aura->GetDuration() <= 6000;
}

bool VigilanceTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_VIGILANCE))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        // The action casts Vigilance on dps, but the trigger fails if any party member has
        // Vigilance from the bot (so the player can have a bot cast Vigilance on any group member).
        if (member->HasAura(SPELL_VIGILANCE, bot->GetGUID()))
            return false;
    }

    return true;
}

bool ShatteringThrowTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_SHATTERING_THROW) || bot->HasSpellCooldown(SPELL_SHATTERING_THROW))
        return false;

    GuidVector enemies = AI_VALUE(GuidVector, "possible targets");

    for (ObjectGuid const& guid : enemies)
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (!enemy || !enemy->IsAlive() || enemy->IsFriendlyTo(bot))
            continue;

        if (bot->IsWithinDistInMap(enemy, 25.0f) &&
            (enemy->HasAura(SPELL_DIVINE_SHIELD) ||
             enemy->HasAura(SPELL_ICE_BLOCK) ||
             enemy->HasAura(SPELL_BLESSING_OF_PROTECTION)))
        {
            return true;
        }
    }

    return false;
}

bool BattleShoutTrigger::IsActive()
{
    if (!BuffTrigger::IsActive())
        return false;

    uint32 battleShoutSpellId = AI_VALUE2(uint32, "spell id", "battle shout");
    if (!battleShoutSpellId)
        return false;

    SpellInfo const* bsInfo = sSpellMgr->GetSpellInfo(battleShoutSpellId);
    if (!bsInfo)
        return false;

    int32 bsApValue = 0;
    for (uint8 eff = 0; eff < MAX_SPELL_EFFECTS; ++eff)
    {
        if (bsInfo->Effects[eff].ApplyAuraName == SPELL_AURA_MOD_ATTACK_POWER)
        {
            bsApValue = bsInfo->Effects[eff].BasePoints + 1;
            break;
        }
    }
    if (!bsApValue)
        return false;

    static const float commandingPresenceBonus[]   = {
        0.05f, 0.10f, 0.15f, 0.20f, 0.25f };

    float cpBonus = 0.0f;
    for (int rank = 4; rank >= 0; --rank)
    {
        if (bot->HasAura(SPELL_COMMANDING_PRESENCE_RANKS[rank]))
        {
            cpBonus = commandingPresenceBonus[rank];
            break;
        }
    }
    int32 effectiveBsAp = int32(bsApValue * (1.0f + cpBonus));

    static char const* blessingNames[] = {
        "blessing of might", "greater blessing of might", nullptr
    };
    for (int i = 0; blessingNames[i] != nullptr; ++i)
    {
        Aura* bom = botAI->GetAura(blessingNames[i], bot);
        if (!bom)
            continue;

        SpellInfo const* bomInfo = bom->GetSpellInfo();
        if (!bomInfo)
            continue;

        for (uint8 eff = 0; eff < MAX_SPELL_EFFECTS; ++eff)
        {
            if (bomInfo->Effects[eff].ApplyAuraName == SPELL_AURA_MOD_ATTACK_POWER)
            {
                int32 bomApValue = bomInfo->Effects[eff].BasePoints + 1;
                if (bomApValue >= effectiveBsAp)
                    return false;
                break;
            }
        }
    }

    return true;
}
