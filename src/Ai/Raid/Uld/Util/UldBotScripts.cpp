/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "UldBossHelper.h"
#include "UldEncounter_Vezax.h"

namespace
{

// Both Vezax ground effects appear under a bot's feet with no warning it can act on: the Shadow
// Crash field lands where the missile does, and the vapor puddle where the vapor dies. A bot part
// way through a cast cannot move, so without this it finishes the cast standing in the effect -
// which on a puddle is one more doubling of 100 * 2^stacks. Nothing is stored; both hooks fire once,
// at the instant the effect is created.
void InterruptVezaxCastersNear(Unit* reference, Position const& hazard, bool (*needsToLeave)(Player*))
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

        // Only break the cast of a bot that is about to be told to move. Everyone else either wants
        // to be standing there or is unaffected, and a wasted interrupt is a wasted global.
        if (!needsToLeave(player))
            continue;

        if (player->GetExactDist2d(hazard.GetPositionX(), hazard.GetPositionY()) >
            ULDUAR_VEZAX_HAZARD_RADIUS)
        {
            continue;
        }

        botAI->RequestSpellInterrupt();
    }
}

bool GainsNothingFromVaporPuddle(Player* bot) { return !VezaxWantsVaporPuddleMana(bot); }

}  // namespace

class VezaxHazardListenerScript : public AllSpellScript
{
public:
    VezaxHazardListenerScript() : AllSpellScript("VezaxHazardListenerScript") {}

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo,
                     bool /*skipCheck*/) override
    {
        if (!caster || !spellInfo || caster->GetMapId() != ULDUAR_MAP_ID)
            return;

        if (spellInfo->Id == SPELL_VEZAX_SHADOW_CRASH_CAST)
        {
            std::list<TargetInfo> const& targets = *spell->GetUniqueTargetInfo();
            if (targets.empty())
                return;

            Player* target = ObjectAccessor::GetPlayer(*caster, targets.front().targetGUID);
            if (!target)
                return;

            InterruptVezaxCastersNear(caster, target->GetPosition(),
                                      &VezaxMustLeaveShadowCrashField);
            return;
        }

        if (spellInfo->Id == SPELL_VEZAX_SARONITE_VAPORS_SPAWN)
            InterruptVezaxCastersNear(caster, caster->GetPosition(),
                                      &GainsNothingFromVaporPuddle);
    }
};

void AddSC_UlduarBotScripts()
{
    new VezaxHazardListenerScript();
}
