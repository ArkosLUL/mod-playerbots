/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Sindragosa.h"
#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "DruidActions.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "ICCActions_Sindragosa.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PetsAction.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"
#include "RaidDifficulty.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "ShamanActions.h"
#include "WarriorActions.h"

float IccSindragosaMultiplier::GetValue(Action* action)
{
    // Cached per ms: asked for every action the bot weighs in every ICC fight, and the boss lookup is a
    // 200 yd grid search. A bot never ticks twice in one ms.
    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        Unit* const found = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);
        _bossGuid = found ? found->GetGUID() : ObjectGuid::Empty;
        _lastPhase = found && IccSindragosaPhase(bot, found) == IccSindragosaPhaseId::Last;
        _overBombStation = found && IccSindragosaOverAirStation(bot, found, 30.0f);
    }

    if (_bossGuid.IsEmpty())
        return 1.0f;

    Unit* boss = ObjectAccessor::GetUnit(*bot, _bossGuid);
    if (!boss)
        return 1.0f;

    // HoT support is an instant cast that never moves the bot. Always allow so
    // beaconed targets stay topped up across air phase, blistering cold cast,
    // phase 3 tank lockdown, and other "everything else 0.0f" branches below.
    if (dynamic_cast<IccSindragosaHotAction*>(action))
        return 1.0f;

    // Tanks never chase raid icons; they stay on the boss at the tank spot.
    if (PlayerbotAI::IsTank(bot) && dynamic_cast<AttackRtiTargetAction*>(action))
        return 0.0f;

    Aura* aura = botAI->GetAura("Unchained Magic", bot, false, true);

    Difficulty diff = GetRaidMapDifficulty(bot);

    if (boss->HealthBelowPct(95))
    {
        if (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FleeAction*>(action) ||
            dynamic_cast<FollowAction*>(action) || dynamic_cast<CastStarfallAction*>(action))
            return 0.0f;
    }

    if (aura && (diff == RAID_DIFFICULTY_10MAN_HEROIC || diff == RAID_DIFFICULTY_25MAN_HEROIC) &&
        !dynamic_cast<IccSindragosaFrostBombAction*>(action))
    {
        if (dynamic_cast<MovementAction*>(action) || dynamic_cast<IccSindragosaUnchainedMagicAction*>(action))
            return 1.0f;
        else
            return 0.0f;
    }

    // Check if boss is casting blistering cold (using both normal and heroic spell IDs)
    if (boss->HasUnitState(UNIT_STATE_CASTING) && IccBossCastingBlisteringCold(boss))
    {
        // If this is the blistering cold action, give it highest priority
        if (dynamic_cast<IccSindragosaBlisteringColdAction*>(action) ||
            dynamic_cast<HealPartyMemberAction*>(action) ||
            dynamic_cast<ReachPartyMemberToHealAction*>(action))
            return 1.0f;

        // Ranged / healer already beyond the blast radius: keep DPSing or
        // healing, just block any movement so they don't wander back in.
        bool const safe = bot->GetExactDist2d(boss) >= 33.0f;
        if (safe && (PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot)))
        {
            if (dynamic_cast<MovementAction*>(action))
                return 0.0f;
            return 1.0f;
        }

        // Disable all other actions while blistering cold is casting
        return 0.0f;
    }

    // Highest priority if we have beacon
    if (bot->HasAura(SPELL_FROST_BEACON))
    {
        if (dynamic_cast<IccSindragosaFrostBeaconAction*>(action))
            return 1.0f;
        else
            return 0.0f;
    }

    bool anyoneHasFrostBeacon = IccAnyGroupMemberHasAura(bot, SPELL_FROST_BEACON);

    if (anyoneHasFrostBeacon && _overBombStation)
    {
        if (dynamic_cast<IccSindragosaFrostBeaconAction*>(action))
            return 1.0f;
        else
            return 0.0f;
    }

    if (anyoneHasFrostBeacon && !PlayerbotAI::IsTank(bot))
    {
        if (dynamic_cast<IccSindragosaGroupPositionAction*>(action))
            return 0.0f;
    }

    // Pin healers at the LOS2 hide spot while the hide is in effect (last
    // phase, tomb up, no beacon). A beacon releases the pin so healers can
    // reposition with the raid.
    if (PlayerbotAI::IsHeal(bot) && dynamic_cast<MovementAction*>(action) && !anyoneHasFrostBeacon && _lastPhase &&
        bot->GetExactDist2d(ICC_SINDRAGOSA_LOS2_POSITION.GetPositionX(),
                            ICC_SINDRAGOSA_LOS2_POSITION.GetPositionY()) <= 2.0f &&
        !IccGetCreaturesByEntries(bot, {NPC_TOMB1, NPC_TOMB2, NPC_TOMB3, NPC_TOMB4}, 150.0f).empty())
        return 0.0f;

    // Last phase with a beacon out: only ranged DPS burn the tomb. Melee and
    // healers reposition (via FrostBeaconAction) instead of chasing the skull.
    if (anyoneHasFrostBeacon && _lastPhase && !PlayerbotAI::IsTank(bot) &&
        !(PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot)) && dynamic_cast<AttackAction*>(action))
        return 0.0f;

    if (!PlayerbotAI::IsTank(bot) && _lastPhase)
    {
        if (dynamic_cast<IccSindragosaGroupPositionAction*>(action))
            return 0.0f;
    }

    if (PlayerbotAI::IsTank(bot))
    {
        if (_lastPhase)
        {
            // Assist tank: hold the tank position and face the boss only, never
            // chase the marked tomb. A beaconed assist tank returns at the Frost
            // Beacon branch above and moves to its beacon spot instead.
            if (!PlayerbotAI::IsMainTank(bot))
            {
                if (dynamic_cast<IccSindragosaGroupPositionAction*>(action) ||
                    dynamic_cast<TankFaceAction*>(action))
                    return 1.0f;
                return 0.0f;
            }

            if (dynamic_cast<TankFaceAction*>(action) ||
                dynamic_cast<AttackAction*>(action) || dynamic_cast<MovementAction*>(action))
                return 1.0f;
            else
                return 0.0f;
        }
    }

    if (_overBombStation)
    {
        if (dynamic_cast<IccSindragosaFrostBombAction*>(action))
            return 1.0f;

        if (dynamic_cast<FollowAction*>(action) || dynamic_cast<IccSindragosaBlisteringColdAction*>(action) ||
            dynamic_cast<IccSindragosaChilledToTheBoneAction*>(action) || dynamic_cast<IccSindragosaMysticBuffetAction*>(action) ||
            dynamic_cast<IccSindragosaFrostBeaconAction*>(action) || dynamic_cast<IccSindragosaUnchainedMagicAction*>(action) ||
            dynamic_cast<FleeAction*>(action) || dynamic_cast<CastDisengageAction*>(action) || dynamic_cast<PetAttackAction*>(action) ||
            dynamic_cast<IccSindragosaGroupPositionAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
            dynamic_cast<DpsAoeAction*>(action) || dynamic_cast<CastHurricaneAction*>(action) ||
            dynamic_cast<CastVolleyAction*>(action) || dynamic_cast<CastBlizzardAction*>(action) ||
            dynamic_cast<CastStarfallAction*>(action) || dynamic_cast<FanOfKnivesAction*>(action) ||
            dynamic_cast<CastWhirlwindAction*>(action) || dynamic_cast<CastMindSearAction*>(action) ||
            dynamic_cast<CastMagmaTotemAction*>(action) || dynamic_cast<CastConsecrationAction*>(action) ||
            dynamic_cast<CastFlamestrikeAction*>(action) || dynamic_cast<CastExplosiveTrapAction*>(action) ||
            dynamic_cast<CastExplosiveShotBaseAction*>(action) || dynamic_cast<CastBlinkBackAction*>(action))
            return 0.0f;
    }

    return 1.0f;
}
