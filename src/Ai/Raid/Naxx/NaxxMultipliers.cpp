/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxMultipliers.h"
#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "FollowActions.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "NaxxActions.h"
#include "NaxxSpellIds.h"
#include "PaladinActions.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "ScriptedCreature.h"
#include "ShamanActions.h"
#include "Spell.h"
#include "UseMeetingStoneAction.h"
#include "WarriorActions.h"

float GrobbulusMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
    {
        return 1.0f;
    }
    if (dynamic_cast<AvoidAoeAction*>(action))
    {
        return botAI->IsMainTank(bot) ? 0.0f : 1.0f;
    }
    if (dynamic_cast<CombatFormationMoveAction*>(action))
    {
        return 0.0f;
    }
    return 1.0f;
}

float HeiganDanceMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
    {
        return 1.0f;
    }
    if (dynamic_cast<CombatFormationMoveAction*>(action) ||
        dynamic_cast<CastDisengageAction*>(action) ||
        dynamic_cast<CastBlinkBackAction*>(action) )
    {
        return 0.0f;
    }

    // Speed boost for Phase 2 only. In Phase 1, Aspect of the Pack can daze the tank
    // if anyone gets hit, which is a common wipe cause on Heigan.
    if (dynamic_cast<CastAspectOfThePackAction*>(action))
    {
        return helper.IsFastDance() ? 1.0f : 0.0f;
    }

    // Out of sync we cannot say where the safe zone is, so leave the rest of the engine alone.
    if (!helper.IsSynced())
    {
        return 1.0f;
    }

    // Bots parked on the ledge are not stepping anywhere this phase, so the cutoff below would cost
    // them a quarter of their cast windows for a move they never make.
    if (helper.ShouldHoldLedge() && helper.IsOnPlatform())
    {
        return 1.0f;
    }

    // Phase 1 leaves 10s between steps - only clamp down on the run-up to an eruption. The fast
    // dance never leaves enough room for a cast.
    if (!helper.IsFastDance() && helper.MsUntilNextEruption() > EruptionCastCutoffMs)
    {
        return 1.0f;
    }
    if (dynamic_cast<HeiganDanceAction*>(action) || dynamic_cast<HeiganDispelDecrepitFeverAction*>(action) ||
        dynamic_cast<CurePartyMemberAction*>(action))
    {
        return 1.0f;
    }
    if (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastMeleeSpellAction*>(action))
    {
        CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
        uint32 spellId = AI_VALUE2(uint32, "spell id", spellAction->getSpell());
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
        {
            return 0.0f;
        }
        uint32 castTime = spellInfo->CalcCastTime();
        if (castTime == 0 && !spellInfo->IsChanneled())
        {
            return 1.0f;
        }
    }
    return 0.0f;
}

float LoathebGenericMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "loatheb");
    if (!boss)
    {
        return 1.0f;
    }
    context->GetValue<bool>("neglect threat")->Set(true);
    if (botAI->GetState() == BOT_STATE_COMBAT &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) || dynamic_cast<FleeAction*>(action) ||
         dynamic_cast<CombatFormationMoveAction*>(action)))
    {
        return 0.0f;
    }
    if (!dynamic_cast<CastHealingSpellAction*>(action))
    {
        return 1.0f;
    }
    Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::NecroticAura10});
    if (!aura)
    {
        // Fallback to name for custom spell data.
        aura = botAI->GetAura("necrotic aura", bot);
    }
    if (!aura || aura->GetDuration() <= 1500)
    {
        return 1.0f;
    }
    return 0.0f;
}

float ThaddiusPrepullMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<FollowAction*>(action))
        return 1.0f;

    // Same gate the trigger and the action use, so follow is never suppressed while staging has
    // nothing to move the bot to - that combination leaves it standing in the room.
    if (!helper.IsPrepullStagingUsable())
        return 1.0f;

    // Once a bot is parked the staging action returns false so it can still buff and drink, which
    // hands the tick to "follow". That drags it off the spot, staging pulls it back, and the two
    // fight over it on the ramp. Kill follow for as long as the split is held instead - unlike
    // removing the strategy outright (the Thorim approach) this cannot leak, since it is re-decided
    // every tick.
    return 0.0f;
}

float ThaddiusGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
    {
        return 1.0f;
    }
    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    if (helper.IsPhasePet())
    {
        if (dynamic_cast<FollowAction*>(action))
            return 0.0f;

        if (bot->getClass() == CLASS_ROGUE && action->getName() == "sprint")
            return 0.0f;

        if (dynamic_cast<ThaddiusAttackNearestPetAction*>(action))
            return 2.0f;

        if (!botAI->IsTank(bot))
        {
            if (dynamic_cast<ReachSpellAction*>(action))
                return 0.0f;
        }

        // Threat redirects are NaxxThreatRedirectMultiplier's job. The rest of the main-tank buffs
        // (Beacon of Light, Earth Shield, Thorns) are still wanted during the pet phase.
        if (dynamic_cast<ReachPartyMemberToHealAction*>(action))
        {
            return 0.0f;
        }
    }

    Unit* target = AI_VALUE(Unit*, "current target");
    if (helper.IsPhasePet() && !botAI->IsTank(bot) && helper.PetSyncSuppress(target))
    {
        if (dynamic_cast<MeleeAction*>(action))
            return 0.0f;

        if (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastHealingSpellAction*>(action))
            return 0.0f;
    }
    // magnetic pull
    // uint32 curr_timer = eventMap->GetTimer();
    // // if (curr_phase == 2 && bot->GetPositionZ() > 312.5f && dynamic_cast<MovementAction*>(action))
    // {
    // if (curr_phase == 2 && (curr_timer % 20000 >= 18000 || curr_timer % 20000 <= 2000) &&
    // dynamic_cast<MovementAction*>(action))
    // {
    //     // MotionMaster *mm = bot->GetMotionMaster();
    //     // mm->Clear();
    //     return 0.0f;
    // }
    // thaddius phase
    // if (curr_phase == 8 && dynamic_cast<FleeAction*>(action))
    // {
    //         return 0.0f;
    // }
    return 1.0f;
}

float SapphironGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
    {
        return 1.0f;
    }
    if (botAI->IsHeal(bot))
    {
        if (helper.IsBreathWindow())
        {
            if (dynamic_cast<SapphironFlightPositionAction*>(action) ||
                dynamic_cast<CastHealingSpellAction*>(action) ||
                dynamic_cast<HealPartyMemberAction*>(action) ||
                dynamic_cast<CastAoeHealSpellAction*>(action) ||
                dynamic_cast<CurePartyMemberAction*>(action))
            {
                return 1.0f;
            }
            return 0.0f;
        }
        if (helper.WaitForExplosion())
        {
            if (dynamic_cast<SapphironFlightPositionAction*>(action) ||
                dynamic_cast<CastHealingSpellAction*>(action) ||
                dynamic_cast<HealPartyMemberAction*>(action) ||
                dynamic_cast<CastAoeHealSpellAction*>(action) ||
                dynamic_cast<CurePartyMemberAction*>(action))
            {
                return 1.0f;
            }
            return 0.0f;
        }
        if (helper.HasLifeDrainInGroup())
        {
            if (dynamic_cast<SapphironGroundPositionAction*>(action) ||
                dynamic_cast<SapphironFlightPositionAction*>(action) ||
                dynamic_cast<CastHealingSpellAction*>(action) ||
                dynamic_cast<HealPartyMemberAction*>(action) ||
                dynamic_cast<CastAoeHealSpellAction*>(action) ||
                dynamic_cast<CurePartyMemberAction*>(action))
            {
                return 1.0f;
            }
            return 0.0f;
        }
    }
    if (dynamic_cast<CastDeathGripAction*>(action) || dynamic_cast<CombatFormationMoveAction*>(action))
    {
        return 0.0f;
    }
    return 1.0f;
}

float InstructorRazuviousGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
    {
        return 1.0f;
    }
    context->GetValue<bool>("neglect threat")->Set(true);
    if (botAI->GetState() == BOT_STATE_COMBAT &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
         dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action)))
    {
        return 0.0f;
    }
    return 1.0f;
}

float KelthuzadGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
    {
        return 1.0f;
    }

    bool guardiansPresent = !helper.GetGuardians().empty();
    bool isOffTankForKT = botAI->IsTank(bot) && !botAI->IsMainTank(bot) &&
                          (botAI->IsAssistTank(bot) || botAI->HasStrategy("tank assist", BOT_STATE_COMBAT));

    if (dynamic_cast<PetAttackAction*>(action))
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (!target)
            return 0.0f;

        if (!helper.IsWithinRoom(target, KelthuzadBossHelper::ROOM_MAX_RADIUS))
            return 0.0f;

        if (Unit* pet = bot->GetPet())
            if (!helper.IsWithinRoom(pet, KelthuzadBossHelper::ROOM_MAX_RADIUS + 2.0f))
                return 0.0f;
        if (Unit* guardianPet = bot->GetGuardianPet())
            if (!helper.IsWithinRoom(guardianPet, KelthuzadBossHelper::ROOM_MAX_RADIUS + 2.0f))
                return 0.0f;

        return 1.0f;
    }

    if (helper.IsPhaseTwo() &&
        helper.IsBossCastingAny({NaxxSpellIds::FrostBoltSingle, NaxxSpellIds::FrostBoltSingle25}))
    {
        std::string const name = action->getName();
        if (name == "kick" || name == "pummel" || name == "shield bash" ||
            name == "mind freeze" || name == "strangulate" ||
            name == "counterspell" || name == "wind shear" ||
            name == "spell lock" || name == "silencing shot" ||
            name == "bash" || name == "hammer of justice")
        {
            return 5.0f;
        }
    }

    if (helper.HasChains(bot))
    {
        if (dynamic_cast<MovementAction*>(action))
        {
            return 1.0f;
        }
        return 0.0f;
    }
    if (botAI->IsHeal(bot))
    {
        if (helper.HasAuraInGroup(NaxxSpellIds::FrostBlast))
        {
            if (dynamic_cast<KelthuzadPositionAction*>(action) ||
                dynamic_cast<KelthuzadFleeShadowFissureAction*>(action) ||
                dynamic_cast<KelthuzadCycloneChainedAction*>(action) ||
                dynamic_cast<CastHealingSpellAction*>(action) ||
                dynamic_cast<HealPartyMemberAction*>(action) ||
                dynamic_cast<CastAoeHealSpellAction*>(action) ||
                dynamic_cast<CurePartyMemberAction*>(action))
            {
                return 1.0f;
            }
            return 0.0f;
        }
        if (helper.HasAuraInGroup(NaxxSpellIds::ChainsOfKelthuzad))
        {
            if (dynamic_cast<KelthuzadPositionAction*>(action) ||
                dynamic_cast<KelthuzadFleeShadowFissureAction*>(action) ||
                dynamic_cast<KelthuzadCycloneChainedAction*>(action) ||
                dynamic_cast<CastHealingSpellAction*>(action) ||
                dynamic_cast<HealPartyMemberAction*>(action) ||
                dynamic_cast<CastAoeHealSpellAction*>(action) ||
                dynamic_cast<CurePartyMemberAction*>(action))
            {
                return 1.0f;
            }
            return 0.0f;
        }
    }
    if (helper.HasDetonateMana(bot))
    {
        if (dynamic_cast<KelthuzadPositionAction*>(action) || dynamic_cast<MovementAction*>(action))
        {
            return 1.0f;
        }
        return 0.0f;
    }

    if (dynamic_cast<TankAssistAction*>(action))
    {
        if (isOffTankForKT && guardiansPresent)
        {
            return 2.0f;
        }
        return 1.0f;
    }

    if (isOffTankForKT && guardiansPresent)
    {
        if (dynamic_cast<CastDebuffSpellOnAttackerAction*>(action))
            return 1.0f;
    }

    if ((dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<FleeAction*>(action) ||
         dynamic_cast<CastDebuffSpellOnAttackerAction*>(action)))
    {
        return 0.0f;
    }
    if (helper.IsPhaseOne())
    {
        if (dynamic_cast<CastTotemAction*>(action) || dynamic_cast<CastShadowfiendAction*>(action) ||
            dynamic_cast<CastRaiseDeadAction*>(action) || dynamic_cast<CastFeignDeathAction*>(action) ||
            dynamic_cast<CastInvisibilityAction*>(action) || dynamic_cast<CastVanishAction*>(action) ||
            dynamic_cast<PetAttackAction*>(action))
        {
            return 0.0f;
        }
    }
    if (helper.IsPhaseTwo())
    {
        if (dynamic_cast<CastBlizzardAction*>(action) || dynamic_cast<CastFrostNovaAction*>(action))
        {
            return 0.0f;
        }
    }
    return 1.0f;
}

float NothGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
    {
        return 1.0f;
    }

    if (dynamic_cast<CombatFormationMoveAction*>(action))
    {
        return 0.0f;
    }

    // Targeting belongs to "noth choose target". The generic assists rank by attack range and
    // remaining lifetime, so a fresh Plagued Warrior always outranks the boss and the bot flips
    // between the two every tick instead of committing to either.
    if (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action))
    {
        return 0.0f;
    }

    // Nothing else in the encounter warrants holding the raid back; the curse is handled by giving
    // the dispel its own high-priority node rather than by muting three classes for 40% of the fight.
    // Blink's threat wipe only empties Noth's own table, so only the bots hitting him have to hold -
    // anyone on an add keeps going.
    Unit* boss = helper.GetBoss();
    if (!helper.IsBlinkWindow() || botAI->IsTank(bot) || (boss && AI_VALUE(Unit*, "current target") != boss))
    {
        return 1.0f;
    }

    if (dynamic_cast<NothDispelCurseAction*>(action) || dynamic_cast<NothPositionAction*>(action) ||
        dynamic_cast<CurePartyMemberAction*>(action) || dynamic_cast<CastHealingSpellAction*>(action))
    {
        return 1.0f;
    }

    // A redirect during the window is the fastest way back to a tank holding the boss.
    if (dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
        dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action))
    {
        return 1.0f;
    }

    if (dynamic_cast<MeleeAction*>(action) || dynamic_cast<CastDebuffSpellOnAttackerAction*>(action))
    {
        return 0.0f;
    }

    if (dynamic_cast<CastSpellAction*>(action))
    {
        return 0.0f;
    }
    return 1.0f;
}

float AnubrekhanGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
    {
        return 1.0f;
    }

    // The position action hands out slots the generic formation mover would pull everyone back out
    // of, into the one pile Impale punishes. Melee are the exception outside the swarm: they are not
    // positioned at all, so they keep the generic behaviour.
    bool meleeDps = !botAI->IsTank(bot) && !botAI->IsHeal(bot) && !botAI->IsRanged(bot);
    if (dynamic_cast<CombatFormationMoveAction*>(action) && !(meleeDps && !helper.IsSwarmFormation()))
    {
        return 0.0f;
    }

    if (helper.IsSwarmFormation())
    {
        // Nobody gains from a panic move during the swarm: the tank would drop the kite and everyone
        // else would leave the pile that keeps them out of the aura.
        if (dynamic_cast<FleeAction*>(action))
        {
            return 0.0f;
        }

        if (!botAI->IsTank(bot))
        {
            // Autoattack: there is nothing in reach from the pile anyway, and swinging means facing
            // whatever the bot last targeted.
            if (dynamic_cast<MeleeAction*>(action))
            {
                return 0.0f;
            }

            // The chases, all of which fire the moment the position action goes quiet and would walk
            // the bot straight back out: "reach melee" and "reach spell" onto a boss that is a whole
            // KiteRadius away by design, and "reach party member to heal" onto the kiting tank.
            if (dynamic_cast<ReachTargetAction*>(action))
            {
                return 0.0f;
            }
        }
    }
    return 1.0f;
}

float FourhorsemanGenericMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sir zeliek");
    if (!boss)
    {
        return 1.0f;
    }
    context->GetValue<bool>("neglect threat")->Set(true);
    if ((dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action)))
    {
        return 0.0f;
    }
    return 1.0f;
}

float GothikGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
    {
        return 1.0f;
    }

    // Targeting belongs to "gothik choose target"; the generic assist actions would copy whatever the
    // tank happens to be on, which loses the kill order the whole tactic rests on.
    context->GetValue<bool>("neglect threat")->Set(true);
    if (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action))
    {
        return 0.0f;
    }

    // On the balcony he is immune and out of reach, and behind the gate he is unreachable. Either way
    // nothing may spend a global or a step on him.
    if (!helper.IsBossAttackable() && AI_VALUE(Unit*, "current target") == helper.GetBoss())
    {
        if (dynamic_cast<CastHealingSpellAction*>(action))
        {
            return 1.0f;
        }
        if (action->getName() == "gothik choose target" || action->getName() == "gothik stay on living side")
        {
            return 1.0f;
        }
        return 0.0f;
    }

    return 1.0f;
}

float GluthGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
    {
        return 1.0f;
    }
    if ((dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<FleeAction*>(action) || dynamic_cast<CastDebuffSpellOnAttackerAction*>(action)))
    {
        return 0.0f;
    }

    if (botAI->IsMainTank(bot))
    {
        Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::MortalWound10, NaxxSpellIds::MortalWound25});
        if (!aura)
        {
            // Fallback to name for custom spell data.
            aura = botAI->GetAura("mortal wound", bot, false, true);
        }
        if (aura && aura->GetStackAmount() >= 5)
        {
            if (dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
                dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action))
            {
                return 0.0f;
            }
        }
    }
    if (dynamic_cast<PetAttackAction*>(action))
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (helper.IsZombieChow(target))
        {
            return 0.0f;
        }
    }
    return 1.0f;
}

float NaxxThreatRedirectMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastMisdirectionOnMainTankAction*>(action) &&
        !dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action))
    {
        return 1.0f;
    }

    // Encounters where the main tank is not the right threat sink: tank swaps on a debuff stack,
    // mind-controlled tanks, or one tank per boss. "find target" only sees creatures that already
    // have this bot on their threat list, so all four horsemen are listed - a melee bot parked on
    // Thane or the Baron never resolves Zeliek. Thaddius' pets are the same story on a smaller
    // scale: one tank per pet, and the main tank is only the right sink for one of them.
    static std::vector<std::string> const noRedirectBosses = {"gluth",
                                                              "instructor razuvious",
                                                              "gothik the harvester",
                                                              "sir zeliek",
                                                              "lady blaumeux",
                                                              "thane korth'azz",
                                                              "baron rivendare",
                                                              "highlord mograine",
                                                              "stalagg",
                                                              "feugen"};

    for (std::string const& name : noRedirectBosses)
    {
        if (AI_VALUE2(Unit*, "find target", name))
        {
            return 0.0f;
        }
    }

    // Heigan spends 45s of every cycle REACT_PASSIVE on his ledge, so a redirect fired there burns
    // its charges on nothing. Hold it for the arena return, where the tank has to rebuild while the
    // raid is already back on the boss.
    if (heigan.UpdateBossAI() && heigan.IsFastDance())
    {
        return 0.0f;
    }
    return 1.0f;
}

float NaxxBurstWindowMultiplier::GetValue(Action* action)
{
    if (!action || !IsBurstCooldownAction(action->getName()))
    {
        return 1.0f;
    }

    uint32 now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedValue = EvaluateWindow();
    }
    return cachedValue;
}

float NaxxBurstWindowMultiplier::EvaluateWindow()
{
    // Resolved up front rather than in encounter order, so the fight timer is cleared even when an
    // earlier boss's branch takes the return.
    bool const loathebUp = loatheb.UpdateBossAI();
    if (!loathebUp)
    {
        loathebFightStartMs = 0;
    }

    if (kelthuzad.UpdateBossAI())
    {
        if (kelthuzad.IsPhaseOne())
        {
            return 0.0f;
        }
        // Phase 2 below the Guardian threshold is the actual DPS race.
        Unit* boss = kelthuzad.GetBoss();
        return boss && boss->GetHealthPct() <= KELTHUZAD_GUARDIAN_PCT ? 1.0f : 0.0f;
    }

    if (sapphiron.UpdateBossAI())
    {
        return sapphiron.IsPhaseFlight() ? 0.0f : 1.0f;
    }

    if (thaddius.UpdateBossAI())
    {
        // Save everything for the boss himself - he has a 5 minute enrage.
        return thaddius.IsPhaseThaddius() ? 1.0f : 0.0f;
    }

    if (loathebUp)
    {
        uint32 now = getMSTime();
        if (!loathebFightStartMs)
        {
            loathebFightStartMs = now;
        }

        if (NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::FungalCreep}) || botAI->HasAura("fungal creep", bot))
        {
            return 1.0f;
        }
        return getMSTimeDiff(loathebFightStartMs, now) >= LOATHEB_FALLBACK_MS ? 1.0f : 0.0f;
    }

    if (noth.UpdateBossAI())
    {
        return noth.IsBalconyPhase() ? 0.0f : 1.0f;
    }

    // Cheaper than another helper member: his balcony phase is readable straight off the unit flag
    // the core script sets (boss_gothik.cpp:232), which is all this needs.
    if (Unit* gothik = AI_VALUE2(Unit*, "find target", "gothik the harvester"))
    {
        return gothik->HasUnitFlag(UNIT_FLAG_DISABLE_MOVE) ? 0.0f : 1.0f;
    }

    return 1.0f;
}
