/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Kologarn.h"

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
#include "UldEncounter_Kologarn.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

// Kologarn
//
// Every bot's target here is decided per role in code rather than through a raid icon, so the
// generic pickers have to be silenced or they fight it: "dps target" falls back to a smart-target
// strategy when no icon is set and is therefore never null, which keeps NotDpsTargetActiveTrigger
// permanently true. Same three casts as the Eredar Twins guard in SWP - the debuff one matters
// because it is what lands DoTs on whatever the bot drifted onto.
float KologarnDisableAutomaticTargetingMultiplier::GetValue(Action* action)
{
    if (botAI->GetState() == BOT_STATE_NON_COMBAT)
        return 1.0f;

    if (!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<TankAssistAction*>(action) &&
        !dynamic_cast<CastDebuffSpellOnAttackerAction*>(action))
    {
        return 1.0f;
    }

    // Asked last, because it walks the target list: only once something would actually be blocked.
    // Engagement, not proximity: before the pull the per-role pickers are inert, so silencing the
    // generic ones would leave the bot with none, and trash within 100yd of him would be unfightable.
    return KologarnEncounterActive(botAI) ? 0.0f : 1.0f;
}

float KologarnMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // A gripped bot is a stunned passenger on the right arm until the arm releases it or it dies;
    // movement orders only fight the ride and leave it facing the wrong way when it drops.
    if (!IsKologarnStoneGripped(bot))
        return 1.0f;

    return GetKologarn(botAI) ? 0.0f : 1.0f;
}
