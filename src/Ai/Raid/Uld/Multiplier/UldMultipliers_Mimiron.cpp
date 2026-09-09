/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Mimiron.h"

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
#include "UldEncounter_Mimiron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

namespace
{
// The Mimiron windows where taking the hit ends the bot: Shock Blast is 100000 in a 15 yd circle, the
// Laser Barrage cone one-shots, a Rocket Strike lands on the ranged ring, and Firefighter's fire and
// Frost Bomb both tick people down. Proximity Mines and Bomb Bots are deliberately absent - the mine
// node was demoted below the whole ladder because eating one is healable, and letting it veto a charge
// would contradict its own ranking.
bool MimironLethalWindowActive(PlayerbotAI* botAI)
{
    if (!GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
        !GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
        !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    MimironShockBlastTrigger shockBlast(botAI);
    if (shockBlast.IsActive())
        return true;

    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return true;

    MimironRocketStrikeTrigger rocketStrike(botAI);
    if (rocketStrike.IsActive())
        return true;

    // Both of these check the hard-mode config first, so they cost nothing on a normal clear.
    MimironDodgeFlamesTrigger dodgeFlames(botAI);
    if (dodgeFlames.IsActive())
        return true;

    MimironFrostBombTrigger frostBomb(botAI);
    return frostBomb.IsActive();
}
}  // namespace

float MimironChargeGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Cheap gate first. Two unrelated branches for the same behaviour: the gap-closer spells are all
    // CastReachTargetSpellAction - Charge, Intercept and both Feral Charges - while "reach melee" is a
    // plain ReachTargetAction that walks the bot in without casting anything. Matched by name and not
    // by that base, which also covers "reach spell", "reach party member to heal" and "reach pull":
    // vetoing those strands ranged and healers out of range in the window they are needed most.
    if (!dynamic_cast<CastReachTargetSpellAction*>(action) && action->getName() != "reach melee")
        return 1.0f;

    return MimironLethalWindowActive(botAI) ? 0.0f : 1.0f;
}

float MimironFormationGuardMultiplier::GetValue(Action* action)
{
    // By name, not by type: TankFaceAction derives from CombatFormationMoveAction and keeps the tank
    // pointed away from the raid, which is real work this must not cancel.
    if (!action || action->getName() != "combat formation move")
        return 1.0f;

    Position slot;
    if (!GetMimironSpreadSlot(botAI, bot, slot))
        return 1.0f;

    return bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) <=
                   ULDUAR_MIMIRON_SPREAD_TOLERANCE
               ? 0.0f
               : 1.0f;
}

float MimironAvoidAoeGuardMultiplier::GetValue(Action* action)
{
    if (!action || action->getName() != "avoid aoe")
        return 1.0f;

    // Only while the fire is actually there to be mishandled. On a normal clear Mimiron has no ground
    // hazard this node would answer, so leaving it alone costs nothing.
    return IsMimironHardModeActive(botAI) ? 0.0f : 1.0f;
}

float MimironThreatRedirectGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Every BuffOnMainTankAction names itself "<spell> on main tank". Matching the two spells rather
    // than the type on purpose: the type would also catch every blessing and buff aimed at the tank.
    std::string const name = action->getName();
    if (name != "misdirection on main tank" && name != "tricks of the trade on main tank")
        return 1.0f;

    // Phase 1 only, where the two tanks trade Plasma Blast every 22 s. A taunt equalises threat for one
    // moment; twenty seconds of a hunter's and a rogue's redirected threat undoes that long before the
    // next cast, so the swap never sticks while these run. Nothing to swap in the later phases, so the
    // redirects stay useful there.
    return GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
                   !GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
                   !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT)
               ? 0.0f
               : 1.0f;
}

float MimironTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Engaged, not merely present: the stand-down hands targeting to a node that does not run before
    // the pull, so on sight alone it would leave every non-tank with no picker at all.
    if (!IsMimironEngaged(botAI))
        return 1.0f;

    // "mimiron set dps priority" owns every non-tank's target. "attack rti target" is deliberately
    // left alone: bots no longer set marks for each other, but a mark a player sets should still win.
    if (!botAI->IsTank(bot) && dynamic_cast<DpsAssistAction*>(action))
        return 0.0f;

    return 1.0f;
}
