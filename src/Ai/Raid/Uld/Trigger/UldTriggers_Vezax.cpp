#include "UldTriggers_Vezax.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Spell.h"
#include "UldData.h"
#include "UldEncounter_Vezax.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTargets.h"
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
    if (bot->GetMapId() != ULDUAR_MAP_ID)
        return false;

    Unit* vezax = GetVezax(botAI);
    if (vezax && vezax->IsInCombat())
        return false;

    return VezaxHasEncounterState(bot);
}

bool VezaxMarkOfTheFacelessTrigger::IsActive()
{
    if (!bot->HasAura(SPELL_MARK_OF_THE_FACELESS))
        return false;

    if (!VezaxEncounterActive(botAI))
        return false;

    Unit* vezax = GetVezax(botAI);
    if (vezax && vezax->GetVictim() == bot)
        return false;

    Position spot;
    if (!TryGetVezaxMarkSpot(bot, spot))
        return false;

    return bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) >
           ULDUAR_VEZAX_MARK_SPOT_TOLERANCE;
}

bool VezaxMarkOfTheFacelessBreakTrigger::IsActive()
{
    if (PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsMainTank(bot))
        return false;

    if (bot->HasAura(SPELL_MARK_OF_THE_FACELESS))
        return false;

    if (!VezaxEncounterActive(botAI))
        return false;

    return GetVezaxMarkedAlly(bot) != nullptr;
}

bool VezaxShadowCrashDodgeTrigger::IsActive()
{
    if (!VezaxDodgesShadowCrash(bot))
        return false;

    if (!VezaxFormationActive(botAI))
        return false;

    Position impact;
    if (!TryGetVezaxShadowCrashImpact(botAI, impact))
        return false;

    return bot->GetExactDist2d(impact.GetPositionX(), impact.GetPositionY()) <=
           ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS + 1.0f;
}

bool VezaxSearingFlamesInterruptTrigger::IsActive()
{
    Unit* boss = GetVezax(botAI);
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return false;

    Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell || spell->m_spellInfo->Id != SPELL_VEZAX_SEARING_FLAMES)
        return false;

    return VezaxIsSearingFlamesInterrupter(bot, boss);
}

bool VezaxSurgeOfDarknessTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* boss = GetVezax(botAI);
    if (!boss || !boss->HasAura(SPELL_VEZAX_SURGE_OF_DARKNESS))
        return false;

    return boss->GetVictim() == bot;
}

bool VezaxSaroniteAnimusTrigger::IsActive()
{
    Unit* animus = nullptr;
    if (IsVezaxHardModeActive(botAI) && VezaxEncounterActive(botAI))
        animus = GetUldUnitByEntry(botAI, NPC_VEZAX_SARONITE_ANIMUS);

    if (!animus)
    {
        tankHold.Reset();
        return false;
    }

    // Keep this above the target check: the dwell only counts if it's asked every tick.
    bool const tankHeld = TankHasHeldBoss(bot, animus, tankHold, ULDUAR_VEZAX_ANIMUS_TANK_LEAD_MS);

    if (AI_VALUE(Unit*, "current target") == animus)
        return false;

    if (PlayerbotAI::IsTank(bot) || VezaxAnimusRedirectSpell(bot))
        return true;

    uint32 ageMs = 0;
    return tankHeld || !TryGetVezaxAnimusAge(botAI, ageMs) || ageMs >= ULDUAR_VEZAX_ANIMUS_HOLD_CAP_MS;
}

bool VezaxAnimusRedirectTrigger::IsActive()
{
    char const* redirect = VezaxAnimusRedirectSpell(bot);
    if (!redirect || !IsVezaxHardModeActive(botAI) || !VezaxEncounterActive(botAI))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || target->GetEntry() != NPC_VEZAX_SARONITE_ANIMUS)
        return false;

    uint32 ageMs = 0;
    if (!TryGetVezaxAnimusAge(botAI, ageMs) || ageMs >= ULDUAR_VEZAX_ANIMUS_HOLD_CAP_MS)
        return false;

    Player* mainTank = GetGroupMainTank(bot);
    if (!mainTank || mainTank == bot)
        return false;

    if (bot->getClass() == CLASS_ROGUE && bot->GetExactDist(mainTank) > ULDUAR_VEZAX_TRICKS_RANGE)
        return false;

    return botAI->CanCastSpell(redirect, mainTank);
}

bool VezaxAnimusBringBackTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot) || !IsVezaxAnimusOnBot(bot))
        return false;

    Player* mainTank = GetGroupMainTank(bot);
    return mainTank && mainTank != bot &&
           bot->GetExactDist2d(mainTank) > ULDUAR_VEZAX_ANIMUS_BRING_BACK_RADIUS;
}

bool VezaxAnimusMeleeSpotTrigger::IsActive()
{
    if (PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsMainTank(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || target->GetEntry() != NPC_VEZAX_SARONITE_ANIMUS || !VezaxAnimusPhaseActive(botAI))
        return false;

    Unit* vezax = GetVezax(botAI);
    return vezax && bot->GetExactDist(vezax) > VezaxMeleeHoldDistance(bot, vezax);
}

bool VezaxShadowCrashSoakTrigger::IsActive()
{
    if (!VezaxCanSoakShadowCrashField(bot))
        return false;

    if (bot->HasAura(SPELL_VEZAX_SHADOW_CRASH_FIELD))
        return false;

    if (!VezaxEncounterActive(botAI))
        return false;

    std::vector<VezaxHazard> hazards;
    GatherVezaxHazards(bot, hazards, ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS);

    VezaxHazard field;
    if (!TryGetVezaxNearestHazard(bot, hazards, field))
        return false;

    return bot->GetExactDist2d(field.position.GetPositionX(), field.position.GetPositionY()) <=
           ULDUAR_VEZAX_SHADOW_CRASH_SOAK_MAX_TRAVEL;
}

bool VezaxHoldTargetTrigger::IsActive()
{
    if (!VezaxEncounterActive(botAI))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return true;

    return target->GetEntry() != NPC_VEZAX && target->GetEntry() != NPC_VEZAX_SARONITE_ANIMUS;
}

bool VezaxDamageHoldTrigger::IsActive()
{
    if (!IsVezaxHardModeActive(botAI))
        return false;

    if (!VezaxDamageHeld(botAI))
    {
        for (Unit* controlled : bot->m_Controlled)
        {
            Creature* pet = controlled->ToCreature();
            if (pet && pet->HasReactState(REACT_PASSIVE) && VezaxIsHushedPet(pet))
                return true;
        }

        return false;
    }

    Unit* vezax = GetVezax(botAI);
    if (!vezax)
        return false;

    if (bot->GetVictim() == vezax)
        return true;

    for (CurrentSpellTypes const type : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL, CURRENT_AUTOREPEAT_SPELL})
        if (VezaxHarmfulCastOn(bot->GetCurrentSpell(type), vezax))
            return true;

    for (Unit* controlled : bot->m_Controlled)
    {
        Creature* pet = controlled->ToCreature();
        if (pet && !pet->IsTotem() && (!pet->HasReactState(REACT_PASSIVE) || pet->GetVictim() == vezax))
            return true;
    }

    return false;
}

bool VezaxRaidPositionTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot) && IsVezaxAnimusOnBot(bot))
        return false;

    return VezaxFormationActive(botAI);
}
