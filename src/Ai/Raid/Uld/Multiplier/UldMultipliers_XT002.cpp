/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_XT002.h"

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
#include "RaidCombatHelpers.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_XT002.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"
#include "WarlockActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

float XT002BurstWindowMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    std::string const name = action->getName();
    if (!IsBurstCooldownAction(name) || IsManaReturnCooldown(bot, name))
        return 1.0f;

    uint32 now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedValue = EvaluateWindow();
    }

    bool const lust = name == "bloodlust" || name == "heroism";
    return (lust ? cachedValue.allowLust : cachedValue.allowAll) ? 1.0f : 0.0f;
}

XT002BurstWindowMultiplier::Window XT002BurstWindowMultiplier::EvaluateWindow()
{
    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return {true, true};

    if (IsXT002HeartbreakActive(bot, xt002))
        return {true, true};

    bool const hardMode = IsXT002HardModeActive(botAI);

    if (GetXT002ExposedHeart(botAI))
    {
        return {hardMode, hardMode};
    }

    if (xt002->GetHealthPct() < ULDUAR_XT002_FINAL_PUSH_HP_PCT)
        return {true, true};

    return {false, false};
}

float XT002TargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    bool const movement = dynamic_cast<MovementAction*>(action) != nullptr;

    bool carryingKnown = false;
    bool carrying = false;
    auto const carryingDebuff = [&]() -> bool
    {
        if (!carryingKnown)
        {
            carrying =
                bot->HasAura(GetXT002SearingLightSpellId(bot)) || bot->HasAura(GetXT002GravityBombSpellId(bot));
            carryingKnown = true;
        }

        return carrying;
    };

    bool const genericRedirect = dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
                                 dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action);

    bool const genericAvoidAoe = dynamic_cast<AvoidAoeAction*>(action) != nullptr;

    bool const gapCloser = dynamic_cast<CastReachTargetSpellAction*>(action) && carryingDebuff();

    bool const purgingImmunity = (dynamic_cast<CastDivineShieldAction*>(action) ||
                                  dynamic_cast<CastIceBlockAction*>(action)) &&
                                 carryingDebuff();

    bool const genericPicker =
        (dynamic_cast<DpsAssistAction*>(action) && !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsHeal(bot)) ||
        (dynamic_cast<TankAssistAction*>(action) && PlayerbotAI::IsTank(bot));

    bool pinnedMover = false;
    if (movement && !dynamic_cast<AttackAction*>(action) &&
        (PlayerbotAI::IsRangedDps(bot) || PlayerbotAI::IsHeal(bot) || carryingDebuff()))
    {
        static std::set<std::string> const encounterMovers = {"xt002 raid position action",
                                                              "xt002 debuff carrier action",
                                                              "xt002 avoid hazard action", "xt002 bomb heal action"};

        pinnedMover = !encounterMovers.count(action->getName());
    }

    Unit* currentTarget = nullptr;
    bool heartHit = false;
    if (!IsXT002HardModeActive(botAI) && !movement && !dynamic_cast<CastHealingSpellAction*>(action) &&
        !dynamic_cast<XT002BombCarrierHealAction*>(action))
    {
        currentTarget = AI_VALUE(Unit*, "current target");
        if (currentTarget && currentTarget->GetEntry() == NPC_HEART_OF_DECONSTRUCTOR)
        {
            static std::set<std::string> const retargets = {"xt002 set dps priority action",
                                                            "xt002 pummeller taunt action",
                                                            "xt002 redirect threat action"};

            heartHit = !retargets.count(action->getName());
        }
    }

    bool const standDown =
        genericRedirect || genericAvoidAoe || purgingImmunity || genericPicker || pinnedMover || gapCloser;
    if (!standDown && !heartHit)
        return 1.0f;

    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return 1.0f;

    if (standDown && xt002->IsInCombat())
        return 0.0f;

    if (!heartHit)
        return 1.0f;

    Unit* heart = GetXT002ExposedHeart(botAI);
    if (!heart || heart->GetHealthPct() > ULDUAR_XT002_HEART_SAFE_HP_PCT)
        return 1.0f;

    return currentTarget == heart ? 0.0f : 1.0f;
}

float XT002ThreatBrakeMultiplier::GetValue(Action* action)
{
    if (!action || !IsRaidDpsBot(bot) || GetDpsHoldValue(bot, action) > 0.0f ||
        dynamic_cast<CastHealingSpellAction*>(action) || dynamic_cast<CastSoulshatterAction*>(action))
        return 1.0f;

    Unit* target = action->GetTarget();
    Unit* aimed = target ? target : AI_VALUE(Unit*, "current target");
    if (aimed && aimed->GetEntry() == NPC_XT002)
        return IsXT002ThreatBraked(bot, aimed) ? 0.0f : 1.0f;

    if (!IsRaidAreaDamage(botAI, action, _splash))
        return 1.0f;

    Unit* xt002 = GetXT002(botAI);
    if (!xt002 || IsXT002Submerged(xt002) || !IsXT002ThreatBraked(bot, xt002))
        return 1.0f;

    bool const reaches = xt002->GetExactDist2d(bot) <= ULDUAR_XT002_AREA_THREAT_REACH ||
                         (aimed && xt002->GetExactDist2d(aimed) <= ULDUAR_XT002_AREA_THREAT_REACH);
    return reaches ? 0.0f : 1.0f;
}
