/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "UldData.h"
#include "UldEncounter_Vezax.h"
#include "UldEncounter_YoggSaron.h"

#include <algorithm>

namespace
{

void InterruptVezaxCastersNear(Unit* reference, Position const& hazard, float radius,
                               bool (*needsToLeave)(Player*))
{
    if (!reference || !needsToLeave)
        return;

    Map::PlayerList const& players = reference->GetMap()->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
        Player* player = itr->GetSource();
        if (!player || !player->IsAlive())
            continue;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
        if (!botAI || !botAI->HasStrategy("ulduar", BOT_STATE_COMBAT))
            continue;

        if (!needsToLeave(player))
            continue;

        if (player->GetExactDist2d(hazard.GetPositionX(), hazard.GetPositionY()) > radius)
            continue;

        botAI->RequestSpellInterrupt();
    }
}

}

class VezaxHazardListenerScript : public AllSpellScript
{
public:
    VezaxHazardListenerScript() : AllSpellScript("VezaxHazardListenerScript") {}

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo,
                     bool /*skipCheck*/) override
    {
        if (!caster || !spellInfo || caster->GetMapId() != ULDUAR_MAP_ID)
            return;

        if (spellInfo->Id == SPELL_VEZAX_SUMMON_SARONITE_VAPORS && caster->GetEntry() == NPC_VEZAX)
        {
            VezaxNoteVaporSummon(caster);
            return;
        }

        if (spellInfo->Id == SPELL_VEZAX_SURGE_OF_DARKNESS && caster->GetEntry() == NPC_VEZAX)
        {
            VezaxNoteSurge(caster);
            return;
        }

        if (spellInfo->Id != SPELL_VEZAX_SHADOW_CRASH_CAST)
            return;

        {
            Position impact;
            if (WorldLocation const* dst = spell->m_targets.GetDstPos())
                impact.Relocate(dst->GetPositionX(), dst->GetPositionY(), dst->GetPositionZ());
            else if (Unit* target = spell->m_targets.GetUnitTarget())
                impact = target->GetPosition();
            else
                return;

            float const speed = spellInfo->Speed;
            uint32 const flightMs =
                speed > 0.0f ? static_cast<uint32>(caster->GetExactDist(&impact) / speed * IN_MILLISECONDS) : 0;

            VezaxNoteShadowCrash(caster, impact, flightMs);
            InterruptVezaxCastersNear(caster, impact, ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS,
                                      &VezaxDodgesShadowCrash);

            if (RaidObs::Active())
                RaidObs::NoteHazardCircle(caster->GetMap(), SPELL_VEZAX_SHADOW_CRASH_DMG, impact,
                                          ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS, flightMs);
        }
    }
};

class VezaxVaporListenerScript : public AllSpellScript
{
public:
    VezaxVaporListenerScript() : AllSpellScript("VezaxVaporListenerScript", {ALLSPELLHOOK_ON_PREPARE}) {}

    void OnSpellPrepare(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo) override
    {
        if (!caster || !spellInfo || caster->GetMapId() != ULDUAR_MAP_ID)
            return;

        if (caster->GetEntry() != NPC_VEZAX_SARONITE_VAPORS)
            return;

        if (spellInfo->Id == SPELL_VEZAX_SARONITE_VAPORS_AURA)
            VezaxNoteVaporKilled(caster);
        else if (spellInfo->Id == SPELL_VEZAX_SUMMON_SARONITE_ANIMUS)
            VezaxNoteAnimusSummoned(caster);
    }
};

class YoggSaronBrainLinkListenerScript : public AllSpellScript
{
public:
    YoggSaronBrainLinkListenerScript()
        : AllSpellScript("YoggSaronBrainLinkListenerScript", {ALLSPELLHOOK_ON_PREPARE})
    {
    }

    void OnSpellPrepare(Spell* spell, Unit* caster, SpellInfo const* spellInfo) override
    {
        if (!spell || !caster || !spellInfo || caster->GetMapId() != ULDUAR_MAP_ID)
            return;

        if (spellInfo->Id != SPELL_BRAIN_LINK_DAMAGE && spellInfo->Id != SPELL_BRAIN_LINK_OK)
            return;

        YoggSaronNoteBrainLinkPair(caster, spell->m_targets.GetUnitTarget());
    }
};

class YoggSaronGuardianDeathListenerScript : public AllSpellScript
{
public:
    YoggSaronGuardianDeathListenerScript()
        : AllSpellScript("YoggSaronGuardianDeathListenerScript", {ALLSPELLHOOK_ON_PREPARE})
    {
    }

    void OnSpellPrepare(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo) override
    {
        if (!caster || !spellInfo || caster->GetMapId() != ULDUAR_MAP_ID)
            return;

        if (spellInfo->Id != SPELL_SHADOW_NOVA_SARA || caster->GetEntry() != NPC_GUARDIAN_OF_YS)
            return;

        YoggSaronNoteGuardianDeath(caster);
    }
};

void AddSC_UlduarBotScripts()
{
    new VezaxHazardListenerScript();
    new VezaxVaporListenerScript();
    new YoggSaronBrainLinkListenerScript();
    new YoggSaronGuardianDeathListenerScript();
}
