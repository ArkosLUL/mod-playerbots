/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCScripts.h"
#include "Creature.h"
#include "ICCData.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "RaidInstanceState.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include <algorithm>
#include <string>

namespace
{
    RaidInstanceState<IcecrownHelpers::IccInstanceState> iccStates;
}

namespace IcecrownHelpers
{
    IccInstanceState& IccState(uint32 instanceId) { return iccStates.For(instanceId); }

    std::vector<Position> ActiveGooPositions(uint32 instanceId, uint32 lifetimeMs)
    {
        std::vector<Position> out;
        IccInstanceState const* st = iccStates.Find(instanceId);
        if (!st)
            return out;

        uint32 const now = getMSTime();
        for (auto const& impact : st->malleableGoo)
            if (getMSTimeDiff(impact.castTime, now) <= lifetimeMs)
                out.push_back(impact.position);

        return out;
    }
}

class IccPutricideListenerScript : public AllSpellScript
{
public:
    IccPutricideListenerScript() : AllSpellScript("IccPutricideListenerScript") { }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        if (!caster || !spellInfo)
            return;

        // Neither id has a spelldifficulty row, so they hold on every difficulty. The missiles and
        // impacts they trigger carry Putricide as their unit target, which the player test refuses.
        if (spellInfo->Id != SPELL_MALLEABLE_GOO_10N && spellInfo->Id != SPELL_MALLEABLE_GOO_BALCONY)
            return;

        // Malleable Goo is cast triggered, so m_UniqueTargetInfo is not yet
        // populated at this point; read the explicit unit target directly.
        Unit* target = spell->m_targets.GetUnitTarget();
        if (!target || !target->IsPlayer())
            return;

        uint32 now = getMSTime();

        IcecrownHelpers::MalleableGooImpact impact;
        impact.position = target->GetPosition();
        impact.castTime = now;

        auto& impacts = IcecrownHelpers::IccState(caster->GetMap()->GetInstanceId()).malleableGoo;
        impacts.push_back(impact);

        // Festergut's avoid trigger reads an impact as 12 yd for 8 s, the widest model any consumer uses.
        RaidObs::NoteHazardCircle(caster->GetMap(), spellInfo->Id, impact.position, 12.0f, 8000);

        // Evict stale entries to keep the list bounded. Retention covers the
        // longest consumer window (Festergut avoid: 8s) + slack.
        impacts.erase(
            std::remove_if(impacts.begin(), impacts.end(),
                           [now](IcecrownHelpers::MalleableGooImpact const& i)
                           { return getMSTimeDiff(i.castTime, now) > 9000; }),
            impacts.end());
    }
};

class IccRotfaceListenerScript : public AllSpellScript
{
public:
    IccRotfaceListenerScript() : AllSpellScript("IccRotfaceListenerScript") { }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        // Professor Putricide casts Vile Gas from the balcony during Rotface heroic. Festergut casts the
        // same spell himself, so the caster entry is what scopes this hook to Rotface.
        if (!caster || !spellInfo || caster->GetEntry() != NPC_PROFESSOR_PUTRICIDE)
            return;

        // Heroic only, so the Spell constructor has already remapped the base id to 73019 or 73020.
        if (spellInfo->Id != sSpellMgr->GetSpellIdForDifficulty(SPELL_VILE_GAS_H, caster))
            return;

        Unit* target = spell->m_targets.GetUnitTarget();
        if (!target || !target->IsPlayer())
            return;

        IcecrownHelpers::VileGasVictim& entry =
            IcecrownHelpers::IccState(caster->GetMap()->GetInstanceId()).rotfaceVileGas;
        entry.victimGuid = target->GetGUID();
        entry.castTime = getMSTime();

        RaidObs::Note(target->ToPlayer(), "rf.vilegas", std::to_string(spellInfo->Id));
    }
};

class IccLichKingListenerScript : public AllSpellScript
{
public:
    IccLichKingListenerScript() : AllSpellScript("IccLichKingListenerScript") { }

    // OnSpellPrepare fires at cast START (Spell::prepare). OnSpellCast fires
    // at cast END, which for Defile (2s cast time) is too late - the puddle
    // is already spawning and bots have no time to move out.
    void OnSpellPrepare(Spell* spell, Unit* caster, SpellInfo const* spellInfo) override
    {
        if (!caster || !spellInfo)
            return;

        if (spellInfo->Id != DEFILE_CAST_ID)
            return;

        Unit* target = spell->m_targets.GetUnitTarget();
        if (!target || !target->IsPlayer())
            return;

        IcecrownHelpers::DefileCastInfo& entry =
            IcecrownHelpers::IccState(caster->GetMap()->GetInstanceId()).defileCast;
        entry.targetGuid = target->GetGUID();
        entry.castTime = getMSTime();

        // Where the puddle lands at the 6 yd it spawns with; once down it is a creature the trace samples.
        RaidObs::Note(target->ToPlayer(), "lk.defile", std::to_string(spellInfo->Id));
        RaidObs::NoteHazardCircle(caster->GetMap(), spellInfo->Id, target->GetPosition(), 6.0f,
                                  static_cast<uint32>(std::max(spell->GetCastTime(), 0)));
    }
};

// Keyed on boss state rather than on a boss in combat: Gunship, Valithria, Svalna and two of the
// three princes are not dungeon bosses, so their latches used to outlive a wipe.
class IccBossStateResetScript : public AllMapScript
{
public:
    IccBossStateResetScript() : AllMapScript("IccBossStateResetScript") { }

    void OnMapUpdate(Map* map, uint32 /*diff*/) override
    {
        if (!map || map->GetId() != ICC_MAP_ID)
            return;

        InstanceMap* instance = map->ToInstanceMap();
        InstanceScript* script = instance ? instance->GetInstanceScript() : nullptr;
        if (!script)
            return;

        uint32 const instanceId = map->GetInstanceId();
        uint32 const now = getMSTime();

        for (uint32 bossId = 0; bossId < script->GetEncounterCount(); ++bossId)
        {
            if (script->GetBossState(bossId) == IN_PROGRESS)
            {
                iccStates.For(instanceId).lastBossCombatMs = now;
                return;
            }
        }

        IcecrownHelpers::IccInstanceState const* st = iccStates.Find(instanceId);
        if (st && st->lastBossCombatMs != 0 && getMSTimeDiff(st->lastBossCombatMs, now) > ICC_RESET_GRACE_MS)
            iccStates.Reset(instanceId);
    }
};

// Boss state 14 never goes IN_PROGRESS and none of this trash is boss flagged, so nothing else opens
// its trace.
class IccBloodPrinceTrashScript : public UnitScript
{
public:
    IccBloodPrinceTrashScript() : UnitScript("IccBloodPrinceTrashScript", true, {UNITHOOK_ON_UNIT_ENTER_COMBAT}) { }

    void OnUnitEnterCombat(Unit* unit, Unit* victim) override
    {
        if (!unit || !victim || unit->GetMapId() != ICC_MAP_ID || !victim->IsPlayer())
            return;

        switch (unit->GetEntry())
        {
            case NPC_DARKFALLEN_ADVISOR:
            case NPC_DARKFALLEN_ARCHMAGE:
            case NPC_DARKFALLEN_BLOOD_KNIGHT:
            case NPC_DARKFALLEN_NOBLE:
                RaidObs::MarkPull(unit->GetMap(), unit, "blood-prince-trash");
                break;
            default:
                break;
        }
    }
};

void AddSC_IcecrownBotScripts()
{
    // Ids are the core's DATA_* boss order in icecrown_citadel.h.
    RaidObs::NameBossStates(ICC_MAP_ID, {
        {0, "lord-marrowgar"},
        {1, "lady-deathwhisper"},
        {2, "icecrown-gunship-battle"},
        {3, "deathbringer-saurfang"},
        {4, "festergut"},
        {5, "rotface"},
        {6, "professor-putricide"},
        {7, "blood-prince-council"},
        {8, "blood-queen-lana-thel"},
        {9, "sister-svalna"},
        {10, "valithria-dreamwalker"},
        {11, "sindragosa"},
        {12, "the-lich-king"},
        {13, "sindragosa-gauntlet"},
        {14, "blood-prince-trash"},
    });

    new IccPutricideListenerScript();
    new IccRotfaceListenerScript();
    new IccLichKingListenerScript();
    new IccBossStateResetScript();
    new IccBloodPrinceTrashScript();
}
