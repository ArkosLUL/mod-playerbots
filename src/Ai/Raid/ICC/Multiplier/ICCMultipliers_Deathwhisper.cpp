/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Deathwhisper.h"
#include "FollowActions.h"
#include "ICCActions_Deathwhisper.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"

float IccLadyDeathwhisperMultiplier::GetValue(Action* action)
{
    // Cached per ms: asked for every action the bot weighs, and "nearest hostile npcs" casts a sight ray
    // per unit on every read. A bot never ticks twice in one ms.
    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        _bossPresent = AI_VALUE2(Unit*, "find target", "lady deathwhisper") != nullptr;
        _shadeOnBot = false;
        if (_bossPresent)
        {
            for (auto const& npcGuid : AI_VALUE(GuidVector, "nearest hostile npcs"))
            {
                Unit* shade = botAI->GetUnit(npcGuid);
                if (shade && shade->GetEntry() == NPC_SHADE && shade->GetVictim() == bot)
                {
                    _shadeOnBot = true;
                    break;
                }
            }
        }
    }

    if (!_bossPresent)
        return 1.0f;

    if (dynamic_cast<FleeAction*>(action) || dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<FollowAction*>(action) || dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    // Allow the IccShadeLadyDeathwhisperAction to run
    if (dynamic_cast<IccShadeLadyDeathwhisperAction*>(action))
        return 1.0f;

    // Cancel all other actions when we need to handle Vengeful Shade
    return _shadeOnBot ? 0.0f : 1.0f;
}
