/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Saurfang.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_Saurfang.h"
#include "ICCTriggers_Saurfang.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineSaurfang(EncounterBuilder& e)
{
    e.Node<IccDbsTrigger, IccDbsTankPositionAction>(ACTION_RAID + 3);
    e.Node<IccDbsTrigger, IccAddsDbsAction>(ACTION_RAID + 5);

    // Boss taunt on Rune of Blood is handled inside icc dbs tank position action

    e.Multiplier<IccAddsDbsMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccSaurfangDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_DEATHBRINGER_SAURFANG, IccPassThroughGate,
                                                "deathbringer-saurfang", &DefineSaurfang);
    return definition;
}
