/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Gunship.h"
#include "ChooseTargetActions.h"
#include "FollowActions.h"
#include "ICCActions_Gunship.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"

float IccGunshipMultiplier::GetValue(Action* action)
{
    // Cached per ms: asked for every action the bot weighs, and a bot never ticks twice in one ms.
    uint32 const now = getMSTime();
    if (now != _captainCheckedMs)
    {
        _captainCheckedMs = now;
        Unit* captain = IccGunshipEnemyCaptain(bot, 200.0f);
        _inGunship = captain != nullptr;
        _hordeSide = captain && captain->GetEntry() == NPC_MURADIN_BRONZEBEARD;
    }

    if (!_inGunship)
        return 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action))
        return 0.0f;

    // Main tank is locked to captain via IccGunshipRocketJumpAction — block RTI targeting
    if (PlayerbotAI::IsMainTank(bot) && dynamic_cast<AttackRtiTargetAction*>(action))
        return 0.0f;

    // Bot in transit between ships: lock to rocket-jump action only so combat/movement
    // actions don't interfere with jump packet timing.
    Position const friendlyPoint = _hordeSide ? ICC_GUNSHIP_ROCKET_JUMP_HORDE_FRIENDLY_POINT
                                              : ICC_GUNSHIP_ROCKET_JUMP_ALLY_FRIENDLY_POINT;
    Position const middlePoint = _hordeSide ? ICC_GUNSHIP_ROCKET_JUMP_HORDE_MIDDLE_POINT
                                            : ICC_GUNSHIP_ROCKET_JUMP_ALLY_MIDDLE_POINT;
    Position const attackPos = _hordeSide ? ICC_GUNSHIP_ROCKET_JUMP_HORDE
                                          : ICC_GUNSHIP_ROCKET_JUMP_ALLY;
    static constexpr float JUMP_GATE = 30.0f;
    bool const nearFriendly = bot->GetExactDist2d(friendlyPoint) <= JUMP_GATE;
    bool const nearMiddle = bot->GetExactDist2d(middlePoint) <= JUMP_GATE;
    bool const nearAttack = bot->GetExactDist2d(attackPos) <= JUMP_GATE;
    if (!nearFriendly && !nearMiddle && !nearAttack)
    {
        if (!dynamic_cast<IccGunshipRocketJumpAction*>(action))
            return 0.0f;
    }

    return 1.0f;
}
