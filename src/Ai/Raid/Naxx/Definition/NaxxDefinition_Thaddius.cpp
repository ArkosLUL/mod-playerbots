/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxDefinitions.h"

#include "NaxxActions.h"
#include "NaxxMultipliers.h"
#include "NaxxTriggers.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineThaddius(EncounterBuilder& e)
{
    // Pre-pull only. Splits the raid onto the two adds on room entry so the pull does not start
    // with everyone stacked in one blob - phase 1 is on a 5 minute enrage.
    e.Node<ThaddiusPrepullSplitTrigger, ThaddiusPrepullSplitAction>(ACTION_RAID);

    e.Node<ThaddiusPhasePetTrigger, ThaddiusAttackNearestPetAction>(ACTION_RAID + 6);
    e.Node<ThaddiusPhasePetLoseAggroTrigger>("taunt spell", ACTION_RAID + 7);
    e.Node<ThaddiusPhaseTransitionTrigger, ThaddiusMoveToPlatformAction>(ACTION_RAID + 1);
    e.Node<ThaddiusPhaseThaddiusTrigger, ThaddiusMovePolarityAction>(ACTION_RAID + 1);

    // Below the pet-phase taunt. It only ever casts the redirect buff, so outranking the
    // positioning nodes costs a GCD, not a Polarity Shift.
    e.Node<ThaddiusRedirectThreatTrigger, ThaddiusRedirectThreatAction>(ACTION_RAID + 3);

    e.Multiplier<ThaddiusPrepullMultiplier>(Family::Follow);
    e.Multiplier<ThaddiusGenericMultiplier>(Family::AnyMovement | Family::Spell);
}
}  // namespace

EncounterDefinition const& NaxxThaddiusDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_THADDIUS, BossStateGate, "thaddius", &DefineThaddius);
    return definition;
}
