/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidObs.h"

#include "InstanceScript.h"
#include "SpellAuraDefines.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Unit.h"

#include <algorithm>

// Hook surface for RaidObs. Registration is deliberately narrow: this core filters dispatch by the
// enabled-hook ids passed here (CALL_ENABLED_HOOKS), and an empty vector would opt into every hook
// of that script type.
//
// The Send*Log hooks used below are not mainline AzerothCore. They come from the local core's
// observation-hook set, so this module no longer builds against upstream cores.

namespace
{
bool IsPeriodicDamage(AuraType type)
{
    return type == SPELL_AURA_PERIODIC_DAMAGE || type == SPELL_AURA_PERIODIC_DAMAGE_PERCENT ||
           type == SPELL_AURA_PERIODIC_LEECH;
}

bool IsPeriodicHeal(AuraType type)
{
    return type == SPELL_AURA_PERIODIC_HEAL || type == SPELL_AURA_OBS_MOD_HEALTH;
}
}  // namespace

class RaidObsMapScript : public AllMapScript
{
public:
    RaidObsMapScript()
        : AllMapScript("RaidObsMapScript", {ALLMAPHOOK_ON_MAP_UPDATE, ALLMAPHOOK_ON_DESTROY_MAP})
    {
    }

    void OnMapUpdate(Map* map, uint32 diff) override { RaidObs::OnMapUpdate(map, diff); }

    void OnDestroyMap(Map* map) override { RaidObs::OnMapDestroyed(map); }
};

class RaidObsUnitScript : public UnitScript
{
public:
    RaidObsUnitScript()
        : UnitScript("RaidObsUnitScript", true,
                     {UNITHOOK_ON_SEND_SPELL_NON_MELEE_DAMAGE_LOG, UNITHOOK_ON_SEND_ATTACK_STATE_UPDATE,
                      UNITHOOK_ON_SEND_PERIODIC_AURA_LOG, UNITHOOK_ON_SEND_HEAL_SPELL_LOG,
                      UNITHOOK_ON_SCHOOL_ABSORB_APPLIED, UNITHOOK_ON_DAMAGE, UNITHOOK_ON_UNIT_DEATH,
                      UNITHOOK_ON_UNIT_ENTER_COMBAT})
    {
    }

    void OnSendSpellNonMeleeDamageLog(SpellNonMeleeDamage const* log, int32 overkill) override
    {
        if (!RaidObs::Active() || !log)
            return;

        RaidObs::NoteDamage(log->attacker, log->target, log->spellInfo, log->damage, overkill, log->schoolMask,
                            log->absorb, log->resist);
    }

    void OnSendAttackStateUpdate(CalcDamageInfo const* damageInfo, int32 overkill) override
    {
        if (!RaidObs::Active() || !damageInfo)
            return;

        uint32 damage = 0;
        uint32 absorb = 0;
        uint32 resist = 0;
        for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            damage += damageInfo->damages[i].damage;
            absorb += damageInfo->damages[i].absorb;
            resist += damageInfo->damages[i].resist;
        }

        if (!damage && !absorb)
            return;

        RaidObs::NoteDamage(damageInfo->attacker, damageInfo->target, nullptr, damage, overkill,
                            damageInfo->damages[0].damageSchoolMask, absorb, resist);
    }

    void OnSendPeriodicAuraLog(Unit* victim, SpellPeriodicAuraLogInfo* pInfo) override
    {
        if (!RaidObs::Active() || !victim || !pInfo || !pInfo->auraEff)
            return;

        AuraType const type = pInfo->auraEff->GetAuraType();
        SpellInfo const* spell = pInfo->auraEff->GetSpellInfo();
        Unit* caster = pInfo->auraEff->GetCaster();

        if (IsPeriodicDamage(type))
            RaidObs::NoteDamage(caster, victim, spell, pInfo->damage, static_cast<int32>(pInfo->overDamage),
                                spell ? spell->GetSchoolMask() : 0, pInfo->absorb, pInfo->resist);
        else if (IsPeriodicHeal(type))
            RaidObs::NoteHeal(caster, victim, spell, pInfo->damage, pInfo->overDamage);
    }

    void OnSendHealSpellLog(HealInfo const& healInfo, bool /*critical*/) override
    {
        if (!RaidObs::Active())
            return;

        uint32 const heal = healInfo.GetHeal();
        uint32 const effective = healInfo.GetEffectiveHeal();
        RaidObs::NoteHeal(healInfo.GetHealer(), healInfo.GetTarget(), healInfo.GetSpellInfo(), heal,
                          heal > effective ? heal - effective : 0);
    }

    void OnSchoolAbsorbApplied(DamageInfo& dmgInfo, SpellInfo const* absorbSpellInfo, Unit* absorbCaster,
                              uint32 absorbAmount) override
    {
        if (!RaidObs::Active() || !absorbSpellInfo)
            return;

        RaidObs::NoteAbsorb(dmgInfo.GetVictim(), absorbCaster, absorbSpellInfo, absorbAmount);
    }

    // Unit::DealDamage, which every other hook here sits downstream of. Only the fatal blow is taken
    // from it; see NoteKillingBlow for why the rest would be a duplicate.
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!RaidObs::Active())
            return;

        RaidObs::NoteKillingBlow(attacker, victim, damage);
    }

    void OnUnitDeath(Unit* unit, Unit* killer) override { RaidObs::NoteDeath(unit, killer); }

    void OnUnitEnterCombat(Unit* unit, Unit* victim) override { RaidObs::OnCreatureEngage(unit, victim); }
};

class RaidObsGlobalScript : public GlobalScript
{
public:
    RaidObsGlobalScript()
        : GlobalScript("RaidObsGlobalScript",
                       {GLOBALHOOK_ON_AURA_APPLICATION_CLIENT_UPDATE, GLOBALHOOK_ON_BEFORE_SET_BOSS_STATE})
    {
    }

    // Preferred over UnitScript::OnAuraApply, which fires twice for a stack refresh.
    void OnAuraApplicationClientUpdate(Unit* target, Aura* aura, bool remove) override
    {
        RaidObs::NoteAura(target, aura, remove);
    }

    // Fires before InstanceScript::SetBossState decides anything, and it rejects two of these: every
    // change while a boss is still loading from the DB (oldState TO_BE_DECIDED), and DONE while one of
    // the boss's world-boss minions is alive. Acting on the request would open traces for bosses
    // nobody is fighting and close live ones on a kill the core refused, so only the boss id is
    // passed on and RaidObs reads the state that actually stuck on the next map update.
    void OnBeforeSetBossState(uint32 id, EncounterState newState, EncounterState oldState, Map* instance) override
    {
        if (newState == oldState || oldState == TO_BE_DECIDED)
            return;

        RaidObs::OnBossState(id, instance);
    }
};

class RaidObsSpellScript : public AllSpellScript
{
public:
    RaidObsSpellScript() : AllSpellScript("RaidObsSpellScript", {ALLSPELLHOOK_ON_PREPARE}) {}

    // Cast start, not cast success: the telegraph is the whole window a strategy has to react in.
    void OnSpellPrepare(Spell* spell, Unit* caster, SpellInfo const* spellInfo) override
    {
        if (!RaidObs::Active() || !spell || !caster || !spellInfo)
            return;

        RaidObs::NoteCast(caster, spellInfo, spell->m_targets.GetUnitTarget(),
                          static_cast<uint32>(std::max(0, spell->GetCastTime())));
    }
};

class RaidObsWorldScript : public WorldScript
{
public:
    RaidObsWorldScript()
        : WorldScript("RaidObsWorldScript", {WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_SHUTDOWN})
    {
    }

    // Startup rather than config load: the module's own config is read in OnBeforeWorldInitialized,
    // and this reads values out of it.
    void OnStartup() override { RaidObs::LoadConfig(); }

    void OnShutdown() override { RaidObs::Shutdown(); }
};

void AddSC_playerbots_raid_obs()
{
    new RaidObsMapScript();
    new RaidObsUnitScript();
    new RaidObsGlobalScript();
    new RaidObsSpellScript();
    new RaidObsWorldScript();
}
