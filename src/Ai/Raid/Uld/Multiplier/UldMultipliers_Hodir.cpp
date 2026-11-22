/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Hodir.h"

#include "GenericSpellActions.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldEncounter_Hodir.h"

float HodirBreakerAoeHoldMultiplier::GetValue(Action* action)
{
    if (!action || action->getThreatType() != Action::ActionThreatType::Aoe ||
        dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    Unit* const target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    uint32 const entry = target->GetEntry();
    return entry == NPC_HODIR_FLASH_FREEZE_BLOCK || entry == NPC_HODIR_FLASH_FREEZE_PLAYER ? 0.0f : 1.0f;
}

float HodirStormGatherCastHoldMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedSlackMs = GetHodirStormGatherCastSlackMs(botAI, bot);
    }

    if (cachedSlackMs == std::numeric_limits<int32>::max())
        return 1.0f;

    auto cached = spellIds.find(action);
    if (cached == spellIds.end())
    {
        CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
        uint32 const spellId =
            spellAction ? context->GetValue<uint32>("spell id", spellAction->getSpell())->Get() : 0;
        cached = spellIds.emplace(action, spellId).first;
    }

    SpellInfo const* info = cached->second ? sSpellMgr->GetSpellInfo(cached->second) : nullptr;
    if (!info || info->IsChanneled())
        return 1.0f;

    int32 const castMs = static_cast<int32>(info->CalcCastTime(bot));
    return castMs > 0 && castMs + ULDUAR_HODIR_STORM_GATHER_CAST_SLACK_MS > cachedSlackMs ? 0.0f : 1.0f;
}
