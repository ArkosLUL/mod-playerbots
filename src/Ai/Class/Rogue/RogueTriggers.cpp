/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RogueTriggers.h"
#include "ArmorDebuff.h"
#include "BleedDebuff.h"
#include "GenericTriggers.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "SpellAuras.h"

namespace
{
constexpr uint32 SPELL_STEALTH = 1784;
constexpr uint32 SPELL_SPRINT_RANK_1 = 2983;
constexpr uint32 SPELL_ADRENALINE_RUSH = 13750;

// regen past the energy cap is lost, and a 2 s Killing Spree or Adrenaline Rush's doubled regen hits it fast
constexpr uint8 BOOST_MAX_ENERGY = 50;
// time to build 5 points again before the Rupture refresh, about 4 Sinister Strikes
constexpr int32 EVISCERATE_MIN_RUPTURE_MS = 10000;
constexpr int32 EVISCERATE_MIN_RUPTURE_RUSHING_MS = 6000;
constexpr int32 SLICE_AND_DICE_PAST_RUPTURE_MS = 2000;
}

// bool AdrenalineRushTrigger::isPossible()
// {
//     return !botAI->HasAura("stealth", bot);
// }

bool UnstealthTrigger::IsActive()
{
    if (!botAI->HasAura("stealth", bot))
        return false;

    if (!AI_VALUE2(bool, "moving", "self target"))
        return false;

    if (!AI_VALUE(uint8, "attacker count"))
        return true;

    return botAI->GetMaster() &&
           ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", "group leader"), 10.0f) &&
           AI_VALUE2(bool, "moving", "group leader");
}

bool StealthTrigger::IsActive()
{
    if (bot->HasAura(SPELL_STEALTH) || bot->IsInCombat() || bot->HasSpellCooldown(SPELL_STEALTH))
        return false;

    float distance = 30.f;

    Unit* target = AI_VALUE(Unit*, "enemy player target");
    if (target && !target->IsInWorld())
    {
        return false;
    }
    if (!target)
        target = AI_VALUE(Unit*, "grind target");

    if (!target)
        target = AI_VALUE(Unit*, "dps target");

    if (!target)
        return false;

    if (target && target->GetVictim())
        distance -= 10;

    if (target->isMoving() && target->GetVictim())
        distance -= 10;

    if (bot->InBattleground())
        distance += 15;

    if (bot->InArena())
        distance += 15;

    return target && ServerFacade::instance().GetDistance2d(bot, target) < distance;
}

bool SapTrigger::IsPossible() { return bot->GetLevel() > 10 && botAI->HasSpell("sap") && !bot->IsInCombat(); }

bool SprintTrigger::IsPossible() { return bot->HasSpell(SPELL_SPRINT_RANK_1); }

bool SprintTrigger::IsActive()
{
    if (bot->HasSpellCooldown(SPELL_SPRINT_RANK_1))
        return false;

    float distance = botAI->GetMaster() ? 45.0f : 35.0f;
    if (botAI->HasAura("stealth", bot))
        distance -= 10;

    bool targeted = false;

    Unit* dps = AI_VALUE(Unit*, "dps target");
    Unit* enemyPlayer = AI_VALUE(Unit*, "enemy player target");

    if (enemyPlayer && !enemyPlayer->IsInWorld())
    {
        return false;
    }
    if (dps)
        targeted = (dps == AI_VALUE(Unit*, "current target"));

    if (enemyPlayer && !targeted)
        targeted = (enemyPlayer == AI_VALUE(Unit*, "current target"));

    if (!targeted)
        return false;

    if ((dps && dps->IsInCombat()) || enemyPlayer)
        distance -= 10;

    return AI_VALUE2(bool, "moving", "self target") &&
           (AI_VALUE2(bool, "moving", "dps target") || AI_VALUE2(bool, "moving", "enemy player target")) && targeted &&
           (ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", "dps target"), distance) ||
            ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", "enemy player target"), distance));
}

bool SliceAndDiceTrigger::IsActive()
{
    return AI_VALUE2(uint8, "combo", "current target") >= 1 && BuffTrigger::IsActive();
}

bool HungerForBloodTrigger::IsActive()
{
    if (!BuffTrigger::IsActive())
        return false;

    // Needs a bleed on the target, ours or anyone else's.
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    return TargetHasBleed(target);
}

bool AdrenalineRushTrigger::IsActive()
{
    return AI_VALUE2(uint8, "energy", "self target") <= BOOST_MAX_ENERGY && BoostTrigger::IsActive();
}

bool KillingSpreeTrigger::IsActive()
{
    return AI_VALUE2(uint8, "energy", "self target") <= BOOST_MAX_ENERGY && BoostTrigger::IsActive();
}

bool RuptureTrigger::IsActive()
{
    return AI_VALUE2(uint8, "combo", "current target") >= _minComboPoints && DebuffTrigger::IsActive();
}

// keeps the 5 points for the Rupture refresh when there's no time left to build 5 more
bool CombatEviscerateTrigger::IsActive()
{
    if (!ComboPointsAvailableTrigger::IsActive())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    Aura const* rupture = target->GetAura(AI_VALUE2(uint32, "spell id", "rupture"), bot->GetGUID());
    int32 const minRuptureMs =
        bot->HasAura(SPELL_ADRENALINE_RUSH) ? EVISCERATE_MIN_RUPTURE_RUSHING_MS : EVISCERATE_MIN_RUPTURE_MS;
    if (!rupture || rupture->GetDuration() >= minRuptureMs)
        return true;

    // no refresh coming on a target that dies first, same check as RuptureTrigger
    return target->GetHealth() / AI_VALUE(float, "estimated group dps") < RuptureTrigger::NEED_LIFE_TIME;
}

// refresh at 3+ points when it'd run out before Rupture, so it never takes the 5 points saved for Rupture
bool SliceAndDiceBeforeRuptureTrigger::IsActive()
{
    if (AI_VALUE2(uint8, "combo", "current target") < 3)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    uint32 const sliceAndDiceId = AI_VALUE2(uint32, "spell id", "slice and dice");
    Aura const* sliceAndDice = bot->GetAura(sliceAndDiceId);
    Aura const* rupture = target->GetAura(AI_VALUE2(uint32, "spell id", "rupture"), bot->GetGUID());
    if (!sliceAndDice || !rupture)
        return false;

    int32 const due = rupture->GetDuration() + SLICE_AND_DICE_PAST_RUPTURE_MS;
    if (sliceAndDice->GetDuration() > due)
        return false;

    // pointless if the new one runs out first too (no Improved Slice and Dice), it'd eat every 3 points
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(sliceAndDiceId);
    if (!spellInfo)
        return false;

    int32 duration = bot->CalcSpellDuration(spellInfo);
    bot->ApplySpellMod(sliceAndDiceId, SPELLMOD_DURATION, duration);
    return duration > due;
}

bool AssassinationRuptureTrigger::IsActive()
{
    if (!RuptureTrigger::IsActive())
        return false;

    // Assassination keeps Rupture up mostly to feed Hunger for Blood, so once a group mate's bleed is
    // already on the target those 5 combo points are worth more as an Envenom.
    return !GroupSuppliesBleedOn(bot, AI_VALUE(Unit*, "current target"));
}

bool EnvenomTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    if (AI_VALUE2(uint8, "combo", "current target") < 4)
        return false;

    // Envenom consumes our own Deadly Poison stacks; the aura state is what the spell itself checks,
    // so it stays correct across poison ranks.
    return target->HasAuraState(AURA_STATE_DEADLY_POISON, nullptr, bot);
}

bool ExposeArmorTrigger::IsActive()
{
    // Cheapest predicate first: it rejects most evaluations before the name-based aura lookups below,
    // which are the expensive part.
    if (AI_VALUE2(uint8, "combo", "current target") > 3)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    return DebuffTrigger::IsActive() && !TargetHasMajorArmorDebuff(botAI, target) &&
           !GroupSuppliesMajorArmorDebuff(bot);
}

bool MainHandWeaponNoEnchantTrigger::IsActive()
{
    Item* const itemForSpell = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!itemForSpell || itemForSpell->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return false;
    return true;
}

bool OffHandWeaponNoEnchantTrigger::IsActive()
{
    Item* const itemForSpell = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!itemForSpell || itemForSpell->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return false;
    return true;
}
