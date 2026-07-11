#include "UldTriggers_Vezax.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

bool VezaxCheatTrigger::IsActive()
{
    if (!botAI->HasCheat(BotCheatMask::raid))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "general vezax");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    if (!AI_VALUE2(bool, "has mana", "self target"))
        return false;

    return AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.lowMana;
}

bool VezaxShadowCrashTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "general vezax");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    return bot->HasAura(SPELL_VEZAX_SHADOW_CRASH);
}

bool VezaxMarkOfTheFacelessTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "general vezax");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    if (!bot->HasAura(SPELL_MARK_OF_THE_FACELESS))
        return false;

    float distance = bot->GetDistance2d(ULDUAR_VEZAX_MARK_OF_THE_FACELESS_SPOT.GetPositionX(),
                                        ULDUAR_VEZAX_MARK_OF_THE_FACELESS_SPOT.GetPositionY());

    return distance > 2.0f;
}

//
// General Vezax
//
bool VezaxSaroniteVaporsTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "general vezax");
    if (!boss || !boss->IsAlive())
        return false;

    TooCloseToCreatureTrigger tooCloseToSaroniteVapors(botAI);
    return tooCloseToSaroniteVapors.TooCloseToCreature(NPC_VEZAX_SARONITE_VAPORS, 6.0f);
}
