/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_LichKing.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "ICCActions_LichKing.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "ShamanActions.h"
#include "WarriorActions.h"

float IccLichKingAddsMultiplier::GetValue(Action* action)
{
    // Cached per ms: two grid searches and the boss state, asked for every action the bot weighs in
    // every ICC fight. A bot never ticks twice in one ms.
    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        _nearValithria = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f) != nullptr;
        _nearHeroicTerenas = !_nearValithria && bot->FindNearestCreature(NPC_TERENAS_MENETHIL_HC, 55.0f);
        Unit* const found =
            _nearValithria || _nearHeroicTerenas ? nullptr : AI_VALUE2(Unit*, "find target", "the lich king");
        _bossGuid = found ? found->GetGUID() : ObjectGuid::Empty;
        _phaseOne = found && IccLichKingPhase(bot, found) == IccLichKingPhaseId::One;
        _winter = found && IccBossHasRemorselessWinter(found);
        _castingWinter = found && IccBossCastingRemorselessWinter(found);
    }

    if (_nearValithria)
        return 1.0f;

    if (_nearHeroicTerenas)
    {
        // Warlocks and melee stay functional (movement + adds action only)
        if (PlayerbotAI::IsMelee(bot) || bot->getClass() == CLASS_WARLOCK)
        {
            if (dynamic_cast<MovementAction*>(action) || dynamic_cast<IccLichKingAddsAction*>(action))
                return 1.0f;
            return 0.0f;
        }

        // Main tank near another tank: suppress movement jitter
        Unit* mainTank = AI_VALUE(Unit*, "main tank");
        if (!PlayerbotAI::IsMainTank(bot) && mainTank && bot->GetExactDist2d(mainTank) < 2.0f &&
            dynamic_cast<MovementAction*>(action))
            return 0.0f;

        // Suppress all these regardless of role
        if (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action) ||
            dynamic_cast<FleeAction*>(action) || dynamic_cast<CastBlinkBackAction*>(action) ||
            dynamic_cast<CastDisengageAction*>(action) || dynamic_cast<CastChargeAction*>(action) ||
            dynamic_cast<CastFeralChargeBearAction*>(action) || dynamic_cast<CastIceBlockAction*>(action) ||
            dynamic_cast<CastRevivePetAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
            dynamic_cast<CastArmyOfTheDeadAction*>(action))
            return 0.0f;

        return 1.0f;
    }

    Unit* boss = _bossGuid.IsEmpty() ? nullptr : ObjectAccessor::GetUnit(*bot, _bossGuid);
    if (!boss)
        return 1.0f;

    // Allow cure actions only after a brief delay so the plague can spread once
    if (dynamic_cast<CurePartyMemberAction*>(action) || dynamic_cast<CastCleanseDiseaseAction*>(action) ||
        dynamic_cast<CastCleanseDiseaseOnPartyAction*>(action) ||
        dynamic_cast<CastCleanseSpiritCurseOnPartyAction*>(action) || dynamic_cast<CastCleanseSpiritAction*>(action))
    {
        Group* group = bot->GetGroup();
        if (!group)
            return 1.0f;

        static constexpr float DELIVER_RANGE = 3.0f;
        static constexpr std::array<uint32, 4> HorrorEntries = {NPC_SHAMBLING_HORROR1, NPC_SHAMBLING_HORROR2,
                                                                NPC_SHAMBLING_HORROR3, NPC_SHAMBLING_HORROR4};

        // Check whether any Shambling Horror is alive anywhere in the encounter
        auto const anyHorrorAlive = [&]() -> bool
        {
            GuidVector const& npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
            for (ObjectGuid const& guid : npcs)
            {
                Unit* unit = botAI->GetUnit(guid);
                if (!unit || !unit->IsAlive())
                    continue;

                uint32 const entry = unit->GetEntry();
                if (entry == NPC_SHAMBLING_HORROR1 || entry == NPC_SHAMBLING_HORROR2 ||
                    entry == NPC_SHAMBLING_HORROR3 || entry == NPC_SHAMBLING_HORROR4)
                    return true;
            }
            return false;
        };

        bool anyPlagued = false;
        bool allDelivered = true;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive())
                continue;

            if (!botAI->HasAura("Necrotic Plague", member))
                continue;

            anyPlagued = true;

            bool nearHorror = false;
            for (uint32 const entry : HorrorEntries)
            {
                Creature* horror = member->FindNearestCreature(entry, DELIVER_RANGE);
                if (horror && horror->IsAlive())
                {
                    nearHorror = true;
                    break;
                }
            }

            if (!nearHorror)
            {
                allDelivered = false;
                break;
            }
        }

        if (!anyPlagued)
            return 1.0f;

        // No Horror alive at all — allow immediate dispel to prevent
        // uncontrolled spread wiping the raid
        if (!anyHorrorAlive())
            return 1.0f;

        // Horrors exist but not everyone has delivered yet — suppress cures
        return allDelivered ? 1.0f : 0.0f;
    }

    if (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action) ||
        dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action))
        return 0.0f;

    // Hunters may flee (kite mechanics); everyone else stays put
    if (dynamic_cast<FleeAction*>(action) && bot->getClass() != CLASS_HUNTER)
        return 0.0f;

    if (_phaseOne)
    {
        // Assist tank targeting is fully managed by HandleAssistTankAddManagement —
        // suppress generic target-switching actions so they don't override it.
        if (PlayerbotAI::IsAssistTank(bot) &&
            (dynamic_cast<TankAssistAction*>(action) || dynamic_cast<AttackRtiTargetAction*>(action) ||
             dynamic_cast<DpsAssistAction*>(action)))
            return 0.0f;

        if (!PlayerbotAI::IsTank(bot) && dynamic_cast<CastConsecrationAction*>(action))
            return 0.0f;

        if (dynamic_cast<DpsAoeAction*>(action) || dynamic_cast<CastHurricaneAction*>(action) ||
            dynamic_cast<CastVolleyAction*>(action) || dynamic_cast<CastBlizzardAction*>(action) ||
            dynamic_cast<CastStarfallAction*>(action) || dynamic_cast<FanOfKnivesAction*>(action) ||
            dynamic_cast<CastWhirlwindAction*>(action) || dynamic_cast<CastMindSearAction*>(action) ||
            dynamic_cast<CastMagmaTotemAction*>(action) || dynamic_cast<CastFlamestrikeAction*>(action) ||
            dynamic_cast<CastExplosiveTrapAction*>(action) || dynamic_cast<CastExplosiveShotBaseAction*>(action) ||
            dynamic_cast<CastArmyOfTheDeadAction*>(action))
            return 0.0f;
    }

    if (_winter)
    {
        // Winter action and facing take priority
        if (dynamic_cast<IccLichKingWinterAction*>(action) || dynamic_cast<SetFacingTargetAction*>(action))
            return 1.0f;

        // Staging window: while boss is casting Winter, non-tanks must commit
        // to the staging move. Only heals are allowed; everything else blocked.
        if (_castingWinter && !PlayerbotAI::IsTank(bot))
        {
            if (dynamic_cast<HealPartyMemberAction*>(action) ||
                dynamic_cast<ReachPartyMemberToHealAction*>(action))
                return 1.0f;
            return 0.0f;
        }

        // Sphere-targeted bot holds at the winter midpoint spot: block all other movement
        if (!PlayerbotAI::IsTank(bot) && dynamic_cast<MovementAction*>(action))
        {
            GuidVector const& npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
            for (ObjectGuid const& guid : npcs)
            {
                uint32 const entry = guid.GetEntry();
                if (entry != NPC_ICE_SPHERE1 && entry != NPC_ICE_SPHERE2 &&
                    entry != NPC_ICE_SPHERE3 && entry != NPC_ICE_SPHERE4)
                    continue;

                Unit* sphere = botAI->GetUnit(guid);
                if (!sphere || !sphere->IsAlive())
                    continue;

                if (sphere->GetVictim() == bot)
                    return 0.0f;
            }
        }

        // Adds action is suppressed during winter
        if (dynamic_cast<IccLichKingAddsAction*>(action))
            return 0.0f;

        if (dynamic_cast<CastArmyOfTheDeadAction*>(action))
            return 0.0f;

        // Assist tank should not pick up adds independently during winter
        if (PlayerbotAI::IsAssistTank(bot) && dynamic_cast<TankAssistAction*>(action))
            return 0.0f;

        // MT movement is owned by the winter hold logic; reach actions chase
        // far taunt targets and tug him off the hold spot (adds come to him).
        if (PlayerbotAI::IsMainTank(bot) &&
            (dynamic_cast<ReachMeleeAction*>(action) || dynamic_cast<ReachSpellAction*>(action) ||
             dynamic_cast<ReachTargetAction*>(action)))
            return 0.0f;

        // Suppress movement/attack toward the boss if we are far away
        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        if (currentTarget && currentTarget == boss && bot->GetDistance2d(boss) > 50.0f)
        {
            if (dynamic_cast<ReachSpellAction*>(action) ||
                dynamic_cast<ReachMeleeAction*>(action) || dynamic_cast<ReachTargetAction*>(action) ||
                dynamic_cast<TankAssistAction*>(action) || dynamic_cast<DpsAssistAction*>(action))
                return 0.0f;
        }

        // Suppress movement toward boss/sphere — but allow target-switching actions
        // (DpsAssistAction, AttackRtiTargetAction) so bots can pick up skull-marked adds.
        if (currentTarget &&
            (currentTarget == boss || currentTarget->GetEntry() == NPC_ICE_SPHERE1 ||
             currentTarget->GetEntry() == NPC_ICE_SPHERE2 ||
             currentTarget->GetEntry() == NPC_ICE_SPHERE3 || currentTarget->GetEntry() == NPC_ICE_SPHERE4))
        {
            if (dynamic_cast<ReachMeleeAction*>(action) || dynamic_cast<ReachSpellAction*>(action) ||
                dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<TankAssistAction*>(action))
                return 0.0f;
        }
    }

    if (PlayerbotAI::IsRanged(bot) &&
        (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action) ||
         dynamic_cast<FleeAction*>(action) || dynamic_cast<MoveRandomAction*>(action) ||
         dynamic_cast<MoveFromGroupAction*>(action)) &&
        !botAI->GetAura("Harvest Soul", bot, false, false))
    {
        GuidVector const& npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
        for (ObjectGuid const& guid : npcs)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (unit && unit->IsAlive() && unit->GetEntry() == DEFILE_NPC_ID)
                return 0.0f;
        }
    }

    if (PlayerbotAI::IsAssistTank(bot) && _phaseOne)
    {
        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        if (currentTarget && currentTarget == boss && dynamic_cast<AttackRtiTargetAction*>(action))
            return 0.0f;
    }

    return 1.0f;
}

float IccLichKingSpiritBombMultiplier::GetValue(Action* action)
{
    if (!IccLichKingSpiritBombAction::IsBombThreatActive(botAI, bot))
        return 1.0f;

    // Allowlist: only the avoidance move and facing run during a bomb threat.
    // Everything else is suppressed so the avoidance move sticks.
    if (dynamic_cast<IccLichKingSpiritBombAction*>(action) ||
        dynamic_cast<SetFacingTargetAction*>(action))
        return 1.0f;

    return 0.0f;
}
