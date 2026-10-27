/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_BloodPrinces.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_BloodPrinces.h"
#include "ICCTriggers_BloodPrinces.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineBloodPrinces(EncounterBuilder& e)
{
    e.Node<IccBpcKelesethTankTrigger, IccBpcKelesethTankAction>(ACTION_RAID + 1);
    e.Node<IccBpcMainTankTrigger, IccBpcMainTankAction>(ACTION_RAID + 3);
    e.Node<IccBpcEmpoweredVortexTrigger, IccBpcEmpoweredVortexAction>(ACTION_RAID + 4);
    e.Node<IccBpcKineticBombTrigger, IccBpcKineticBombAction>(ACTION_RAID + 6);
    e.Node<IccBpcBallOfFlameTrigger, IccBpcBallOfFlameAction>(ACTION_RAID + 7);

    e.Multiplier<IccBpcAssistMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccBloodPrincesDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_BLOOD_PRINCE_COUNCIL, IccPassThroughGate,
                                                "blood-prince-council", &DefineBloodPrinces);
    return definition;
}
