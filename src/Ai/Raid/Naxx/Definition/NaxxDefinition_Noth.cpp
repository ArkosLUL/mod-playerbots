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
void DefineNoth(EncounterBuilder& e)
{
    e.Node<NothTrigger, NothPositionAction>(ACTION_RAID + 2);
    e.Node<NothTrigger, NothChooseTargetAction>(ACTION_RAID + 1);

    // 25-man only, and only for the few seconds after EVENT_BLINK empties the threat table.
    e.Node<NothBlinkTrigger>("taunt spell", ACTION_RAID + 4);

    // Above the positioning nodes: 25-man curses 10 players at once against a ~10s window, so a
    // dispel that loses a GCD to a repositioning move is a dispel that does not happen.
    e.Node<NothCurseTrigger, NothDispelCurseAction>(ACTION_RAID + 5);

    e.Multiplier<NothGenericMultiplier>(Family::AnyMovement | Family::Spell);
}
}  // namespace

EncounterDefinition const& NaxxNothDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_NOTH, BossStateGate, "noth the plaguebringer", &DefineNoth);
    return definition;
}
