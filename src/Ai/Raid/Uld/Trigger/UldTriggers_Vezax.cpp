#include "UldTriggers_Vezax.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Spell.h"
#include "UldBossHelper.h"
#include "UldEncounter_Vezax.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "RangeTriggers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

using namespace EncounterHelpers;

bool VezaxResetEncounterStateTrigger::IsActive()
{
    if (bot->GetMapId() != ULDUAR_MAP_ID || VezaxEncounterActive(botAI))
        return false;

    return vezaxEncounterStates.find(bot->GetInstanceId()) != vezaxEncounterStates.end();
}

bool VezaxMarkOfTheFacelessTrigger::IsActive()
{
    if (!bot->HasAura(SPELL_MARK_OF_THE_FACELESS))
        return false;

    if (!VezaxEncounterActive(botAI))
        return false;

    Position spot;
    if (!TryGetVezaxMarkSpot(bot, spot))
        return false;

    return bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) >
           ULDUAR_VEZAX_MARK_SPOT_TOLERANCE;
}

bool VezaxVaporPuddleClearTrigger::IsActive()
{
    if (!VezaxShouldLeaveVaporPuddle(bot))
        return false;

    return VezaxEncounterActive(botAI);
}

bool VezaxShadowCrashClearTrigger::IsActive()
{
    if (!bot->HasAura(SPELL_VEZAX_SHADOW_CRASH_FIELD))
        return false;

    if (!VezaxMustLeaveShadowCrashField(bot))
        return false;

    return VezaxEncounterActive(botAI);
}

bool VezaxSearingFlamesInterruptTrigger::IsActive()
{
    Unit* boss = GetVezax(botAI);
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return false;

    // Exact spell id, not "the boss is casting something": Vezax also casts Shadow Crash and Surge of
    // Darkness, and neither is worth an interrupt.
    Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell || spell->m_spellInfo->Id != SPELL_VEZAX_SEARING_FLAMES)
        return false;

    return VezaxIsSearingFlamesInterrupter(bot, boss);
}

bool VezaxSurgeOfDarknessTrigger::IsActive()
{
    if (!botAI->IsTank(bot))
        return false;

    Unit* boss = GetVezax(botAI);
    if (!boss || !boss->HasAura(SPELL_VEZAX_SURGE_OF_DARKNESS))
        return false;

    // Only the bot actually being hit for double physical damage spends a cooldown on it.
    return boss->GetVictim() == bot;
}

bool VezaxSaroniteAnimusTrigger::IsActive()
{
    if (!IsVezaxHardModeActive(botAI))
        return false;

    if (!VezaxEncounterActive(botAI))
        return false;

    Unit* animus = GetFirstAliveUnitByEntry(botAI, NPC_VEZAX_SARONITE_ANIMUS);
    if (!animus)
        return false;

    // Vezax is invulnerable behind the Saronite Barrier until the Animus dies, so everyone
    // switches to it. Only fire when the bot is not already on it.
    return AI_VALUE(Unit*, "current target") != animus;
}

bool VezaxVaporSoakTrigger::IsActive()
{
    if (IsVezaxHardModeActive(botAI) || !VezaxWantsVaporPuddleMana(bot))
        return false;

    if (AI_VALUE2(uint8, "mana", "self target") >= sPlayerbotAIConfig.lowMana)
        return false;

    // Already in one - VezaxShouldLeaveVaporPuddle owns the exit from here.
    if (bot->HasAura(SPELL_VEZAX_SARONITE_VAPORS_PUDDLE))
        return false;

    if (!VezaxEncounterActive(botAI))
        return false;

    std::vector<VezaxHazard> hazards;
    GatherVezaxHazards(bot, hazards, ULDUAR_VEZAX_VAPOR_SOAK_MAX_TRAVEL);

    VezaxHazard puddle;
    if (!TryGetVezaxNearestHazard(bot, hazards, false, puddle))
        return false;

    return bot->GetExactDist2d(puddle.position.GetPositionX(), puddle.position.GetPositionY()) <=
           ULDUAR_VEZAX_VAPOR_SOAK_MAX_TRAVEL;
}

bool VezaxKillVaporTrigger::IsActive()
{
    // In hard mode a dead vapor is a lost hard mode, so this node never arms.
    if (IsVezaxHardModeActive(botAI) || !VezaxIsVaporKiller(bot))
        return false;

    if (!VezaxEncounterActive(botAI))
        return false;

    Unit* vapor = GetFirstAliveUnitByEntry(botAI, NPC_VEZAX_SARONITE_VAPORS);
    if (!vapor)
        return false;

    return AI_VALUE(Unit*, "current target") != vapor;
}

bool VezaxShadowCrashSoakTrigger::IsActive()
{
    // Cheap tests first. GatherVezaxHazards runs two grid searches, and this is one of the two Vezax
    // triggers that needs them - everything above it here keeps that off the common tick.
    if (!VezaxCanSoakShadowCrashField(bot))
        return false;

    // Already inside one - the action would only re-issue a move onto a spot the bot is standing on.
    if (bot->HasAura(SPELL_VEZAX_SHADOW_CRASH_FIELD))
        return false;

    if (!VezaxEncounterActive(botAI))
        return false;

    std::vector<VezaxHazard> hazards;
    GatherVezaxHazards(bot, hazards, ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS);

    VezaxHazard field;
    if (!TryGetVezaxNearestHazard(bot, hazards, true, field))
        return false;

    return bot->GetExactDist2d(field.position.GetPositionX(), field.position.GetPositionY()) <=
           ULDUAR_VEZAX_SHADOW_CRASH_SOAK_MAX_TRAVEL;
}

bool VezaxRaidPositionTrigger::IsActive()
{
    return VezaxFormationActive(botAI);
}
