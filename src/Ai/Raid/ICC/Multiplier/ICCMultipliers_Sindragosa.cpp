/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Sindragosa.h"

#include "Creature.h"
#include "GenericSpellActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidSplash.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
// Her combat reach is 18.75: an area spell from this close reaches her
constexpr float BRAKE_AREA_REACH = 30.0f;
// Area damage this close to a held tomb can reach it
constexpr float TOMB_SPLASH_RANGE = 12.0f;

constexpr uint8 KIND_DOT = 1 << 0;
constexpr uint8 KIND_SPLASH = 1 << 1;

// More of its damage comes from ticks than from the hit, at base values
bool MostlyPeriodic(SpellInfo const* info, Player* caster)
{
    if (!info || info->IsChanneled())
        return false;

    int32 const duration = info->GetMaxDuration();
    float periodic = 0.0f;
    float direct = 0.0f;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        SpellEffectInfo const& effect = info->Effects[i];
        if (effect.Effect == SPELL_EFFECT_SCHOOL_DAMAGE)
        {
            direct += static_cast<float>(effect.CalcValue(caster));
            continue;
        }

        if (effect.Effect != SPELL_EFFECT_APPLY_AURA)
            continue;

        if (effect.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE_PERCENT)
            return true;

        if (effect.ApplyAuraName != SPELL_AURA_PERIODIC_DAMAGE && effect.ApplyAuraName != SPELL_AURA_PERIODIC_LEECH)
            continue;

        if (duration <= 0 || effect.Amplitude <= 0)
            return true;

        periodic += static_cast<float>(effect.CalcValue(caster)) * static_cast<float>(duration / effect.Amplitude);
    }

    return periodic > direct;
}
}  // namespace

float IccSindragosaBrakeMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !IsSgRestrained(botAI))
        return 1.0f;

    Creature* boss = GetSgBoss(botAI);
    if (!boss)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == boss || (!target && AI_VALUE(Unit*, "current target") == boss))
        return 0.0f;

    return IsRaidAreaDamage(botAI, action, _splash) && bot->GetExactDist2d(boss) <= BRAKE_AREA_REACH ? 0.0f : 1.0f;
}

uint8 IccSindragosaTombMultiplier::KindOf(Action* action)
{
    auto const cached = _kinds.find(action);
    if (cached != _kinds.end())
        return cached->second;

    uint8 kind = 0;
    if (CastSpellAction* spell = dynamic_cast<CastSpellAction*>(action))
    {
        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell->getSpell())->Get();
        if (MostlyPeriodic(spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr, bot))
            kind |= KIND_DOT;
    }

    if (SpellSplashes(botAI, action) || SummonSplashes(action))
        kind |= KIND_SPLASH;

    _kinds.emplace(action, kind);
    return kind;
}

float IccSindragosaTombMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    // Asked for every action the bot weighs, and a bot never ticks twice in one ms
    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        GetSgHeldTombs(botAI, _held);
    }

    if (_held.empty())
        return 1.0f;

    uint8 const kind = KindOf(action);
    Unit* target = action->GetTarget();
    if (!target)
        target = AI_VALUE(Unit*, "current target");

    if (target && target->GetEntry() == NPC_SG_ICE_TOMB)
        for (SgHeldTomb const& tomb : _held)
            if (tomb.guid == target->GetGUID() && (IsSgAtTombFloor(target) || (kind & KIND_DOT)))
                return 0.0f;

    if (!(kind & KIND_SPLASH))
        return 1.0f;

    for (SgHeldTomb const& tomb : _held)
        if (bot->GetExactDist2d(tomb.pos) <= TOMB_SPLASH_RANGE ||
            (target && target->GetExactDist2d(tomb.pos) <= TOMB_SPLASH_RANGE))
            return 0.0f;

    return 1.0f;
}
