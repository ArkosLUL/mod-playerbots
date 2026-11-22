/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_IronAssembly.h"

#include "AttackAction.h"
#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_IronAssembly.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

namespace
{
bool IronAssemblyOpenerBurstAllowed(PlayerbotAI* botAI, Player* bot, std::string const& name)
{
    if (name == "offensive potion" || name == "bloodlust" || name == "heroism")
        return false;

    if (!IronAssemblyOpenerBurstWindowOpen(botAI))
        return false;

    if (name == "use trinket" || name == "use tinker")
        return true;

    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
    return spellId && SpellCooldownMs(bot, spellId) <= ULDUAR_IRON_ASSEMBLY_OPENER_BURST_MAX_COOLDOWN_MS;
}
}

float IronAssemblyHoldDpsCooldownsMultiplier::GetValue(Action* action)
{
    if (!IsIronAssemblyHardModeActive(botAI))
        return 1.0f;

    std::string const name = action->getName();
    if (IsManaReturnCooldown(bot, name))
        return 1.0f;

    bool const burst = IsDpsCooldownAction(bot, action) ||
                       ((PlayerbotAI::IsDps(bot) || name == "hysteria") && IsBurstCooldownAction(name));
    if (!burst)
        return 1.0f;

    if (!IronAssemblyFormationActive(botAI))
        return 1.0f;

    if (IsSteelbreakerEmpowered(botAI))
        return 1.0f;

    return IronAssemblyOpenerBurstAllowed(botAI, bot, name) ? 1.0f : 0.0f;
}

float IronAssemblyTauntGuardMultiplier::GetValue(Action* action)
{
    if (!action || !IsTauntAction(bot, action))
        return 1.0f;

    if (!IronAssemblyFormationActive(botAI) || !IsSteelbreakerEmpowered(botAI))
        return 1.0f;

    if (!IronAssemblyHeldByOtherTank(bot, GetIronAssemblyMember(botAI, NPC_STEELBREAKER)))
        return 1.0f;

    return IronAssemblyMeltdownRoleFor(botAI, bot) == IronAssemblyMeltdownRole::Taunt ? 1.0f : 0.0f;
}
