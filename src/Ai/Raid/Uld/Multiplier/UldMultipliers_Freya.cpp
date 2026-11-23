/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Freya.h"

#include "AttackAction.h"
#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "DBCStores.h"
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
#include "UldData.h"
#include "UldEncounter_Freya.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

float FreyaTrioSyncMultiplier::GetValue(Action* action)
{
    if (PlayerbotAI::IsTank(bot) || !PlayerbotAI::IsDps(bot))
        return 1.0f;

    bool const isDamage = dynamic_cast<MeleeAction*>(action) ||
                          (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastHealingSpellAction*>(action));
    if (!isDamage)
        return 1.0f;

    Unit* const target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    uint32 const entry = target->GetEntry();
    if (entry != NPC_SNAPLASHER && entry != NPC_STORM_LASHER && entry != NPC_ANCIENT_WATER_SPIRIT)
        return 1.0f;

    Unit* freya = GetFreyaScan(botAI).Boss();
    if (!freya || !freya->IsAlive())
        return 1.0f;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return FreyaTrioSyncSuppress(state, target) ? 0.0f : 1.0f;
}

float FreyaLasherFinishAoeMultiplier::GetValue(Action* action)
{
    if (!action || action->getThreatType() != Action::ActionThreatType::Aoe)
        return 1.0f;

    if (dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    Unit* freya = GetFreyaScan(botAI).Boss();
    if (!freya || !freya->IsAlive())
        return 1.0f;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return GetFreyaFinishingPackNear(botAI, state, ULDUAR_FREYA_LASHER_PACK_CLEAR) ? 0.0f : 1.0f;
}

float FreyaLasherTrapReserveMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastExplosiveTrapAction*>(action) && !dynamic_cast<CastTrapLauncherExplosiveAction*>(action))
        return 1.0f;

    Unit* freya = GetFreyaScan(botAI).Boss();
    if (!freya || !freya->IsAlive())
        return 1.0f;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return state.detonatingLashers.empty() ? 1.0f : 0.0f;
}

float FreyaArmyRuneReserveMultiplier::GetValue(Action* action)
{
    if (bot->getClass() != CLASS_DEATH_KNIGHT)
        return 1.0f;

    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction)
        return 1.0f;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", spellAction->getSpell());
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !spellInfo->RuneCostID)
        return 1.0f;

    SpellRuneCostEntry const* runeCost = sSpellRuneCostStore.LookupEntry(spellInfo->RuneCostID);
    if (!runeCost || runeCost->NoRuneCost())
        return 1.0f;

    return ReserveThisTick() ? 0.0f : 1.0f;
}

bool FreyaArmyRuneReserveMultiplier::ReserveThisTick()
{
    uint32 const now = getMSTime();
    if (cachedAtMs && now == cachedAtMs)
        return cachedReserve;

    cachedAtMs = now;
    cachedReserve = false;

    if (!bot->HasSpell(SPELL_ARMY_OF_THE_DEAD) || bot->HasSpellCooldown(SPELL_ARMY_OF_THE_DEAD))
        return false;

    Unit* freya = GetFreyaScan(botAI).Boss();
    if (!freya || !freya->IsAlive())
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);
    if (state.detonatingLashers.empty())
        return false;

    // Keep the order: IsFreyaArmyTurn writes the ghoul mark HasUncoveredFreyaLasher reads.
    cachedReserve = IsFreyaArmyTurn(botAI) && HasUncoveredFreyaLasher(botAI, state);
    return cachedReserve;
}

float FreyaAddTankExternalHoldMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    auto cached = held.find(action);
    if (cached == held.end())
    {
        static std::set<std::string> const names = {"pain suppression", "pain suppression on party",
                                                     "guardian spirit on party", "hand of sacrifice on party"};
        cached = held.emplace(action, names.count(action->getName()) > 0).first;
    }

    if (!cached->second || !PlayerbotAI::IsHeal(bot) || !IsFreyaAddTankExternalHeld(botAI))
        return 1.0f;

    Unit* target = action->GetTarget();
    Player* player = target ? target->ToPlayer() : nullptr;
    return player && PlayerbotAI::IsMainTank(player) ? 1.0f : 0.0f;
}

float FreyaGroundTremorCastGateMultiplier::GetValue(Action* action)
{
    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction || dynamic_cast<CastMeleeSpellAction*>(action) || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    if (action->getName() == FreyaGroundTremorHoldCastAction::Name)
        return 1.0f;

    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedRemainingMs = EvaluateWindow();
    }

    if (cachedRemainingMs <= 0)
        return 1.0f;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", spellAction->getSpell());
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 1.0f;

    int32 busyMs = static_cast<int32>(spellInfo->CalcCastTime());
    if (spellInfo->IsChanneled())
        busyMs += spellInfo->GetDuration();

    return busyMs < cachedRemainingMs ? 1.0f : 0.0f;
}

int32 FreyaGroundTremorCastGateMultiplier::EvaluateWindow()
{
    Unit* boss = GetFreyaBossByEntry(botAI);
    if (!IsFreyaGroundTremorCasting(boss))
        return 0;

    Spell* tremor = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);

    return tremor ? tremor->GetCastTimeRemaining() : 0;
}
