/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxDefinitions.h"

#include "NaxxActions.h"
#include "NaxxTriggers.h"
#include "Strategy.h"

namespace
{
void DefineMaexxna(EncounterBuilder& e)
{
    e.Node<MaexxnaWebWrapTrigger, MaexxnaAttackWebWrapAction>(ACTION_RAID + 5);
    e.Node<MaexxnaSpiderlingsTrigger, MaexxnaTankSpiderlingsAction>(ACTION_RAID + 2);

    e.Node<MaexxnaTrigger>("rear flank", ACTION_RAID + 1);
    e.Node<MaexxnaTrigger>("avoid aoe", ACTION_RAID + 1);
}
}  // namespace

EncounterDefinition const& NaxxMaexxnaDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_MAEXXNA, BossStateGate, "maexxna", &DefineMaexxna);
    return definition;
}
