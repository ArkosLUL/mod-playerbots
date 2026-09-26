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
void DefineFaerlina(EncounterBuilder& e)
{
    e.Node<FaerlinaTrigger>("avoid aoe", ACTION_RAID + 1);

    e.Node<FaerlinaFrenzyTrigger>("tranquilizing shot", ACTION_RAID + 4);
    e.Node<FaerlinaFrenzyTrigger, FaerlinaSacrificeWorshipperAction>(ACTION_RAID + 3);
}
}  // namespace

EncounterDefinition const& NaxxFaerlinaDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_FAERLINA, BossStateGate, "grand widow faerlina",
                                                &DefineFaerlina);
    return definition;
}
