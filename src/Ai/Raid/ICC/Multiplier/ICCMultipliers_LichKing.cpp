/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_LichKing.h"

#include "Creature.h"
#include "GenericSpellActions.h"
#include "ICCActions_LichKing.h"
#include "ICCHelpers_LichKing.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "RaidCombatHelpers.h"
#include "RaidSplash.h"
#include "ShamanActions.h"
#include "Timer.h"

namespace
{
// His combat reach is 5.5: an area spell from this close reaches him
constexpr float BRAKE_AREA_REACH = 15.0f;
// Area damage centred this close to an add can reach it
constexpr float SPLASH_RANGE = 12.0f;

constexpr uint8 CURE_DISEASE = 1;
// Cast for a poison, curse or magic effect, it takes a disease with it
constexpr uint8 CURE_ALSO_DISEASE = 2;
}  // namespace

float IccLichKingBrakeMultiplier::GetValue(Action* action)
{
    // The swap taunt goes out braked: the taunt is what moves him onto the free tank
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || action->getName() == IccLichKingTauntAction::Name)
        return 1.0f;

    // Asked for every action the bot weighs, and a bot never ticks twice in one ms
    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        _boss = IsLkBraked(botAI) ? GetLkBoss(botAI) : nullptr;
    }

    Creature* boss = _boss;
    if (!boss)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == boss || (!target && AI_VALUE(Unit*, "current target") == boss))
        return 0.0f;

    return IsRaidAreaDamage(botAI, action, _splash) && bot->GetExactDist2d(boss) <= BRAKE_AREA_REACH ? 0.0f : 1.0f;
}

bool IccLichKingSplashMultiplier::Splashes(Action* action)
{
    auto const cached = _splashes.find(action);
    if (cached != _splashes.end())
        return cached->second;

    bool const splashes =
        SpellSplashes(botAI, action) || SummonSplashes(action) || MULTI_TARGET_ACTIONS.count(action->getName());
    _splashes.emplace(action, splashes);
    return splashes;
}

float IccLichKingSplashMultiplier::GetValue(Action* action)
{
    // The add tank's own area threat is what holds the pile
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || IsLkTankPlayer(bot) || !Splashes(action))
        return 1.0f;

    // Asked for every action the bot weighs, and a bot never ticks twice in one ms
    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        GetLkSplashAdds(botAI, _adds);
    }

    if (_adds.empty())
        return 1.0f;

    Unit* target = action->GetTarget();
    if (!target)
        target = AI_VALUE(Unit*, "current target");

    for (Position const& add : _adds)
        if (bot->GetExactDist2d(add) <= SPLASH_RANGE || (target && target->GetExactDist2d(add) <= SPLASH_RANGE))
            return 0.0f;

    return 1.0f;
}

uint8 IccLichKingCureHoldMultiplier::CureKind(Action* action)
{
    auto const cached = _kinds.find(action);
    if (cached != _kinds.end())
        return cached->second;

    uint8 kind = 0;
    if (dynamic_cast<CastCureDiseaseAction*>(action) || dynamic_cast<CastCureDiseaseOnPartyAction*>(action) ||
        dynamic_cast<CastAbolishDiseaseAction*>(action) || dynamic_cast<CastAbolishDiseaseOnPartyAction*>(action) ||
        dynamic_cast<CastPurifyDiseaseAction*>(action) || dynamic_cast<CastPurifyDiseaseOnPartyAction*>(action) ||
        dynamic_cast<CastCleanseDiseaseAction*>(action) || dynamic_cast<CastCleanseDiseaseOnPartyAction*>(action) ||
        dynamic_cast<CastCleanseSpiritDiseaseOnPartyAction*>(action) ||
        dynamic_cast<CastCureToxinsDiseaseOnPartyActionSham*>(action) ||
        dynamic_cast<CastCleansingTotemAction*>(action))
        kind = CURE_DISEASE;
    else if (dynamic_cast<CastPurifyPoisonAction*>(action) || dynamic_cast<CastPurifyPoisonOnPartyAction*>(action) ||
             dynamic_cast<CastCleansePoisonAction*>(action) || dynamic_cast<CastCleansePoisonOnPartyAction*>(action) ||
             dynamic_cast<CastCleanseMagicAction*>(action) || dynamic_cast<CastCleanseMagicOnPartyAction*>(action) ||
             dynamic_cast<CastCleanseSpiritAction*>(action) ||
             dynamic_cast<CastCleanseSpiritPoisonOnPartyAction*>(action) ||
             dynamic_cast<CastCleanseSpiritCurseOnPartyAction*>(action) ||
             dynamic_cast<CastCureToxinsActionSham*>(action) ||
             dynamic_cast<CastCureToxinsPoisonOnPartyActionSham*>(action) ||
             dynamic_cast<CastStoneformAction*>(action))
        kind = CURE_ALSO_DISEASE;

    _kinds.emplace(action, kind);
    return kind;
}

float IccLichKingCureHoldMultiplier::GetValue(Action* action)
{
    uint8 const kind = action ? CureKind(action) : 0;
    if (!kind || !IsLkCureHeld(botAI))
        return 1.0f;

    return kind == CURE_DISEASE || IsLkPlagueCarrier(action->GetTarget()) ? 0.0f : 1.0f;
}
