/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_YoggSaron.h"

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
#include "RaidSplash.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_YoggSaron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <set>
#include <string>

using namespace EncounterHelpers;

namespace
{
constexpr float SET_BEHIND_SPOT_ANGLE = 3.0f * static_cast<float>(M_PI) / 5.0f;
}

float YoggSaronMovementGuardMultiplier::FleeGuard()
{
    if (!PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot))
        return 1.0f;

    return YoggSaronInPhase1(botAI) ? 0.0f : 1.0f;
}

bool YoggSaronMovementGuardMultiplier::ReachIsWrong(Action* action)
{
    std::string const name = action->getName();
    bool const spellReach =
        name == "reach spell" && !PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot);

    if ((name != "reach melee" && !spellReach) || !YoggSaronInPhase1(botAI))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");

    return target && !YoggSaronGuardianOnTheStack(target) && YoggSaronBotTankAlive(botAI);
}

bool YoggSaronMovementGuardMultiplier::CrusherReachIsWrong()
{
    if (!PlayerbotAI::IsMelee(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");

    return target && target->IsAlive() && target->GetEntry() == NPC_CRUSHER_TENTACLE &&
           !YoggSaronCrusherStunHolds(botAI, target);
}

float YoggSaronMovementGuardMultiplier::SetBehindGuard()
{
    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    float const distance =
        std::max(bot->GetExactDist(target), bot->GetMeleeRange(target) / 2.0f) - bot->GetCombatReach();
    Position const& middle = ULDUAR_YOGG_SARON_MIDDLE;

    bool intoRing = false;
    for (float const side : {1.0f, -1.0f})
    {
        float const angle = Position::NormalizeOrientation(target->GetOrientation() + side * SET_BEHIND_SPOT_ANGLE);
        float const x = target->GetPositionX() + std::cos(angle) * distance;
        float const y = target->GetPositionY() + std::sin(angle) * distance;

        intoRing = intoRing || middle.GetExactDist2d(x, y) < ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS ||
                   !YoggSaronRouteClearOfBody(bot, x, y);
    }

    if (!intoRing)
        return 1.0f;

    uint32 const phase = YoggSaronPhase(botAI);
    return phase == 2 || phase == 3 ? 0.0f : 1.0f;
}

float YoggSaronMovementGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    if (dynamic_cast<FleeAction*>(action))
        return FleeGuard();

    if (dynamic_cast<SetBehindTargetAction*>(action))
        return SetBehindGuard();

    if (!dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    if (action->getName() == "reach party member to heal")
        return 1.0f;

    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return 1.0f;

    if (ReachIsWrong(action))
        return 0.0f;

    if (CrusherReachIsWrong())
        return 0.0f;

    if (YoggSaronHandoverState(botAI).clearing)
        return 0.0f;

    float const fromMiddle =
        bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());
    if (fromMiddle >= ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS)
        return 1.0f;

    uint32 const phase = YoggSaronPhase(botAI);

    if ((phase == 2 || phase == 3) && fromMiddle < ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS)
        return 0.0f;

    return 1.0f;
}

float YoggSaronPhase1AoeHoldMultiplier::GetValue(Action* action)
{
    if (!action || action->getThreatType() != Action::ActionThreatType::Aoe)
        return 1.0f;

    if (dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    if (YoggSaronOnBrainLevel(bot))
        return 1.0f;

    bool const hold = YoggSaronInPhase1(botAI) ? YoggSaronPhase1AoeHold(botAI) : YoggSaronLeftoverAoeHold(botAI);
    return hold ? 0.0f : 1.0f;
}

float YoggSaronIllusionMultiTargetHoldMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    if (bot->GetPositionZ() >= ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return 1.0f;

    YoggSaronRoom const room = YoggSaronRoomOf(bot);
    if (room != YOGG_SARON_ROOM_STORMWIND && room != YOGG_SARON_ROOM_ICECROWN && room != YOGG_SARON_ROOM_CHAMBER)
        return 1.0f;

    if (action->getThreatType() == Action::ActionThreatType::Aoe || MULTI_TARGET_ACTIONS.count(action->getName()))
        return 0.0f;

    return 1.0f;
}

float YoggSaronExitHoldCastMultiplier::GetValue(Action* action)
{
    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction || bot->GetMapId() != ULDUAR_MAP_ID || !YoggSaronOnBrainLevel(bot))
        return 1.0f;

    bool const leaving = YoggSaronLeavingBrainLevel(bot);
    if (!leaving && YoggSaronRoomOf(bot) != YOGG_SARON_ROOM_BRAIN)
        return 1.0f;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(AI_VALUE2(uint32, "spell id", spellAction->getSpell()));
    if (!spellInfo)
        return 1.0f;

    int32 busyMs = spellInfo->CalcCastTime();
    if (spellInfo->IsChanneled() || spellInfo->HasAura(SPELL_AURA_ALLOW_ONLY_ABILITY))
        busyMs = std::max({busyMs, spellInfo->GetMaxDuration(), 1});

    if (busyMs <= 0)
        return 1.0f;

    if (leaving)
        return 0.0f;

    uint32 const left = YoggSaronInduceMadnessLeftMs(botAI);
    return left && static_cast<uint32>(busyMs) + YoggSaronExitLeadMs(bot) > left ? 0.0f : 1.0f;
}

float YoggSaronHeldTargetCastMultiplier::GetValue(Action* action)
{
    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    bool const brainDots = YoggSaronOnBrainLevel(bot) && YoggSaronBrainDotHeld(botAI);
    bool const hunterHold = !brainDots && YoggSaronBeaconHunterHold(botAI);
    bool const platform =
        !brainDots && bot->GetPositionZ() >= ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT;
    if (!brainDots && !hunterHold && !platform)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (!target)
        return 1.0f;

    if (hunterHold && target->GetEntry() == NPC_MARKED_IMMORTAL_GUARDIAN)
        return 0.0f;

    if (!brainDots)
        return GuardianThreatHeld(spellAction, target) ? 0.0f : 1.0f;

    if (target->GetEntry() != NPC_BRAIN)
        return 1.0f;

    if (YoggSaronBrainHeld(botAI))
        return 0.0f;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(AI_VALUE2(uint32, "spell id", spellAction->getSpell()));
    if (!spellInfo)
        return 1.0f;

    return spellInfo->IsChanneled() || spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE) ||
                   spellInfo->HasAura(SPELL_AURA_PERIODIC_LEECH)
               ? 0.0f
               : 1.0f;
}

bool YoggSaronHeldTargetCastMultiplier::GuardianThreatHeld(CastSpellAction* spellAction, Unit* target)
{
    uint32 const entry = target->GetEntry();
    if ((entry != NPC_IMMORTAL_GUARDIAN && entry != NPC_MARKED_IMMORTAL_GUARDIAN) || PlayerbotAI::IsTank(bot))
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(AI_VALUE2(uint32, "spell id", spellAction->getSpell()));
    if (spellInfo && spellInfo->HasEffect(SPELL_EFFECT_INTERRUPT_CAST))
        return false;

    return !YoggSaronGuardianThreatAllows(bot, target, target->GetGUID() == bot->GetTarget());
}

float YoggSaronPhase3AreaMultiplier::GetValue(Action* action)
{
    static std::set<std::string> const areaActions = {
        "blizzard", "flamestrike", "blast wave", "dragon's breath", "cone of cold", "hurricane", "typhoon",
        "volley", "mind sear", "rain of fire", "seed of corruption", "seed of corruption on attacker",
        "fan of knives", "swipe (cat)", "death and decay", "blood boil", "cleave", "fire nova", "magma totem"};
    if (!action || !areaActions.count(action->getName()))
        return 1.0f;

    if (bot->GetMapId() != ULDUAR_MAP_ID ||
        bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT || PlayerbotAI::IsTank(bot))
        return 1.0f;

    Unit* target = action->GetTarget();
    if (!target || target == bot)
        target = AI_VALUE(Unit*, "current target");

    if (!target || !YoggSaronInPhase3(botAI))
        return 1.0f;

    std::list<Creature*> nearby;
    target->GetCreatureListWithEntryInGrid(nearby,
                                           {NPC_YOGG_SARON, NPC_IMMORTAL_GUARDIAN, NPC_MARKED_IMMORTAL_GUARDIAN,
                                            NPC_CRUSHER_TENTACLE, NPC_CORRUPTOR_TENTACLE, NPC_CONSTRICTOR_TENTACLE},
                                           ULDUAR_YOGG_SARON_PHASE_3_AREA_RADIUS + ULDUAR_YOGG_SARON_COMBAT_REACH);

    bool const thorim = YoggThorimKeeperActive(botAI);
    float const floorPct =
        thorim ? ULDUAR_YOGG_SARON_GUARDIAN_WEAKENED_PCT : ULDUAR_YOGG_SARON_PHASE_3_GUARDIAN_FLOOR_PCT;

    bool const sear = action->getName() == "mind sear";

    uint32 worth = 0;
    for (Creature* unit : nearby)
    {
        if (!unit->IsAlive() || (sear && unit == target) ||
            unit->GetDistance(target->GetPosition()) > ULDUAR_YOGG_SARON_PHASE_3_AREA_RADIUS)
            continue;

        uint32 const entry = unit->GetEntry();
        if (entry == NPC_IMMORTAL_GUARDIAN || entry == NPC_MARKED_IMMORTAL_GUARDIAN)
        {
            if (!YoggSaronGuardianThreatAllows(bot, unit, false))
            {
                if (RaidObs::Active())
                    RaidObs::NoteDerived(bot, "yogg.p3area", "fresh");

                return 0.0f;
            }

            if ((entry == NPC_MARKED_IMMORTAL_GUARDIAN && !thorim) || unit->GetHealthPct() <= floorPct)
                continue;
        }

        ++worth;
    }

    bool const allowed = worth >= ULDUAR_YOGG_SARON_PHASE_3_AREA_MIN_WORTH;
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.p3area", allowed ? "allowed" : "few");

    return allowed ? 1.0f : 0.0f;
}

float YoggSaronCrusherAreaHoldMultiplier::GetValue(Action* action)
{
    static std::set<std::string> const neighbourHit = {"maul", "hammer of the righteous", "heart strike",
                                                       "avenger's shield"};
    if (!action)
        return 1.0f;

    RaidSplashCache::Facts& facts = _splash.Of(action);
    if (facts.heal || (!neighbourHit.count(action->getName()) && !IsRaidAreaDamage(botAI, action, _splash)))
        return 1.0f;

    if (bot->GetMapId() != ULDUAR_MAP_ID ||
        bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return 1.0f;

    SpellInfo const* spellInfo =
        facts.cast ? sSpellMgr->GetSpellInfo(facts.spellId.Get(botAI, facts.cast)) : nullptr;
    if (spellInfo && spellInfo->IsPositive())
        return 1.0f;

    uint32 const phase = YoggSaronPhase(botAI);
    if (phase != 2 && phase != 3)
        return 1.0f;

    return YoggSaronInSwingingCrusherReach(botAI, ULDUAR_YOGG_SARON_CRUSHER_AREA_HOLD_SLACK) ? 0.0f : 1.0f;
}

float YoggSaronPhase1WalkGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    std::string const name = action->getName();
    bool const positioning = name == "yogg-saron guardian positioning action";
    bool const station = name == "yogg-saron phase 1 station action";
    bool const reachMelee = name == "reach melee";
    if (!positioning && !station && !reachMelee && name != "reach spell" && name != "set behind" &&
        name != "yogg-saron body detour action" && name != "combat formation move")
        return 1.0f;

    if (!YoggSaronInPhase1Approach(bot) ||
        bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return 1.0f;

    if (!YoggSaronInPhase1(botAI))
        return GetYoggSaronNovaThreats(botAI, ULDUAR_YOGG_SARON_FERVOR_HOLD_RADIUS).empty() ? 1.0f
                                                                                            : 0.0f;

    bool const fervor = bot->HasAura(SPELL_SARAS_FERVOR) &&
                        !GetYoggSaronNovaThreats(botAI, ULDUAR_YOGG_SARON_FERVOR_HOLD_RADIUS).empty();
    if (!fervor && !positioning && !station && !reachMelee)
        return 1.0f;

    if (!fervor && !WalkCrossesCloud(station, reachMelee))
        return 1.0f;

    return positioning && YoggSaronHandoverState(botAI).clearing ? 1.0f : 0.0f;
}

bool YoggSaronPhase1WalkGuardMultiplier::WalkCrossesCloud(bool station, bool reachMelee)
{
    if (station)
    {
        Position const& spot = ULDUAR_YOGG_SARON_P1_RANGED_SPOT;
        return YoggSaronWalkCrossesCloud(
            bot, spot, &spot, ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS + ULDUAR_YOGG_SARON_P1_RANGED_STACK_RADIUS);
    }

    Position const& middle = ULDUAR_YOGG_SARON_MIDDLE;
    if (bot->GetDistance2d(middle.GetPositionX(), middle.GetPositionY()) <= ULDUAR_YOGG_SARON_P1_LEASH)
        return false;

    if (!reachMelee)
        return YoggSaronWalkCrossesCloud(bot, middle);

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && YoggSaronWalkCrossesCloud(bot, target->GetPosition());
}

bool YoggSaronAntiFearTotemGuardMultiplier::FearWindowActive() { return YoggSaronFearWindowActive(botAI); }
