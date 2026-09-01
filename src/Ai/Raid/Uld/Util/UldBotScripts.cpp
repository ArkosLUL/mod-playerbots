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
#include "UldData.h"
#include "UldEncounter_Vezax.h"

#include <algorithm>

namespace
{

// Both Vezax ground effects appear under a bot's feet with no warning it can act on: the Shadow
// Crash field lands where the missile does, and the vapor puddle where the vapor dies. A bot part
// way through a cast cannot move, so without this it finishes the cast standing in the effect -
// which on a puddle is one more doubling of 100 * 2^stacks. Nothing is stored; both hooks fire once,
// at the instant the effect is created.
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

        // Only break the cast of a bot that is about to be told to move. Everyone else either wants
        // to be standing there or is unaffected, and a wasted interrupt is a wasted global.
        if (!needsToLeave(player))
            continue;

        if (player->GetExactDist2d(hazard.GetPositionX(), hazard.GetPositionY()) > radius)
            continue;

        botAI->RequestSpellInterrupt();
    }
}

bool GainsNothingFromVaporPuddle(Player* bot) { return !VezaxMayStandInVaporPuddle(bot); }

// Everyone the dodge node will move. Melee and the tank are not on it: SelectTarget skips anything
// within 12.5 yd of Vezax, so no impact ever lands near enough to reach them.
bool DodgesShadowCrash(Player* bot)
{
    return bot && PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsMainTank(bot);
}

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

            // TARGET_DEST_TARGET_ENEMY freezes the destination here, at cast time, so this position
            // is where the missile lands however far the target walks in the meantime.
            Position const impact = target->GetPosition();

            InterruptVezaxCastersNear(caster, impact, ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS,
                                      &DodgesShadowCrash);

            // Nothing sweeps for this: the field's DynamicObject only exists once the missile has
            // landed, so the ~2s of flight - the only window a bot can act in - would otherwise be
            // invisible in the trace. This hook already fires exactly once per cast, server-side,
            // which is why the record is written here and not in the per-bot helper.
            if (RaidObs::Active())
            {
                // Straight from distance over Speed rather than Spell::GetDelayMoment, which the core
                // fills in during the same cast this hook runs inside. Both are milliseconds.
                float const speed = spellInfo->Speed;
                uint32 const flightMs =
                    speed > 0.0f
                        ? static_cast<uint32>(caster->GetExactDist(&impact) / speed * 1000.0f)
                        : 0;

                RaidObs::NoteHazardCircle(caster->GetMap(), SPELL_VEZAX_SHADOW_CRASH_DMG, impact,
                                          ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS, flightMs);
            }

            return;
        }

        if (spellInfo->Id == SPELL_VEZAX_SARONITE_VAPORS_SPAWN)
        {
            Position const puddle = caster->GetPosition();
            InterruptVezaxCastersNear(caster, puddle, ULDUAR_VEZAX_HAZARD_RADIUS,
                                      &GainsNothingFromVaporPuddle);

            // 63322 applies a periodic aura rather than a persistent area aura, so it is never a
            // DynamicObject for the snapshot sweep to find, and the corpse holding it leaves the
            // watched set the moment it dies. Without this the puddle is nowhere in the trace.
            // 63323's own duration is the corpse aura's life, and so the puddle's.
            uint32 const ttlMs = static_cast<uint32>(std::max<int32>(0, spellInfo->GetDuration()));
            RaidObs::NoteHazardCircle(caster->GetMap(), SPELL_VEZAX_SARONITE_VAPORS_PUDDLE, puddle,
                                      ULDUAR_VEZAX_HAZARD_RADIUS, ttlMs);
        }
    }
};

void AddSC_UlduarBotScripts()
{
    new VezaxHazardListenerScript();
}
