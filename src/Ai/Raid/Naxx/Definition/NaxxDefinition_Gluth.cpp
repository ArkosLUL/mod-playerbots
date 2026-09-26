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
void DefineGluth(EncounterBuilder& e)
{
    e.Node<GluthTrigger, GluthChooseTargetAction>(ACTION_RAID + 1);
    e.Node<GluthTrigger, GluthPositionAction>(ACTION_RAID + 1);
    e.Node<GluthTrigger, GluthSlowdownAction>(ACTION_RAID);

    e.Node<GluthMainTankMortalWoundTrigger>("taunt spell", ACTION_RAID + 1);
    e.Node<GluthRedirectThreatTrigger, GluthRedirectThreatAction>(ACTION_RAID + 2);
    e.Node<GluthFrenzyTrigger, GluthTranquilizingShotAction>(ACTION_RAID + 4);

    e.Node<GluthLowHealthZombieAoeTrigger>("starfall", ACTION_RAID + 1);
    e.Node<GluthLowHealthZombieAoeTrigger>("blizzard", ACTION_RAID + 1);
    e.Node<GluthLowHealthZombieAoeTrigger>("volley", ACTION_RAID + 1);
    e.Node<GluthLowHealthZombieAoeTrigger>("rain of fire", ACTION_RAID + 1);

    e.Multiplier<GluthGenericMultiplier>(Family::AnyMovement | Family::Spell | Family::PetAttack);
}
}  // namespace

EncounterDefinition const& NaxxGluthDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_GLUTH, BossStateGate, "gluth", &DefineGluth);
    return definition;
}
