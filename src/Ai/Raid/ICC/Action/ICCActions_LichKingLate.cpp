/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_LichKingLate.h"

#include <string>

#include "ICCHelpers_LichKing.h"
#include "ICCHelpers_Shared.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"

bool IccLichKingDefileAction::Execute(Event /*event*/)
{
    // The puddle lands where the target stands as the 2 s cast ends
    return WalkFor(&GetLkDefileWalk, MovementPriority::MOVEMENT_FORCED, true, IsLkDefileRun(botAI));
}

bool IccLichKingVileSpiritsAction::Execute(Event /*event*/)
{
    // Guard first: a spirit bursts as soon as the soaker is within 5 yd
    if (uint32 const guard = GetLkSoakGuard(botAI))
        return botAI->CastSpell(guard, bot);

    if (IsLkTrapDue(botAI))
    {
        if (!botAI->CastSpell("frost trap", bot))
            return false;

        NoteLkTrapDropped(botAI);
        return true;
    }

    return WalkFor(&GetLkVileWalk, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool IccLichKingFrostmourneAction::Execute(Event /*event*/)
{
    // A bomb's explosion is lethal in here, and a death inside wipes the raid
    bool const urgent = IsLkFrostmourneUrgent(botAI);
    MovementPriority const priority = urgent ? MovementPriority::MOVEMENT_FORCED : MovementPriority::MOVEMENT_COMBAT;
    return WalkFor(&GetLkFrostmourneWalk, priority, urgent, false);
}

bool IccLichKingSiphonTauntAction::Execute(Event /*event*/)
{
    Unit* valkyr = GetLkSiphonTauntTarget(botAI);
    if (!valkyr || !IccCastClassTaunt(bot, botAI, valkyr))
        return false;

    NoteLkSiphonTaunt(botAI, valkyr);
    return true;
}

bool IccLichKingValkyrControlAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    bool stun = false;
    Unit* valkyr = GetLkValkyrControl(botAI, spellId, stun);
    if (!valkyr || !LkCastControl(botAI, valkyr, spellId))
        return false;

    NoteLkValkyrControl(botAI, valkyr, stun);
    return true;
}

bool IccLichKingHarvestSoulAction::Execute(Event /*event*/)
{
    Unit* target = nullptr;
    std::string spell;
    switch (GetLkHarvestDuty(botAI, target, spell))
    {
        case LkHarvestDuty::Kick:
        case LkHarvestDuty::Dispel:
            return target && botAI->CastSpell(spell, target);
        case LkHarvestDuty::Heal:
            return target && CastRaidThroughputHeal(botAI, target);
        default:
            return false;
    }
}

bool IccLichKingStackAction::Execute(Event /*event*/)
{
    return WalkFor(&GetLkStackWalk, MovementPriority::MOVEMENT_COMBAT, false, false);
}
