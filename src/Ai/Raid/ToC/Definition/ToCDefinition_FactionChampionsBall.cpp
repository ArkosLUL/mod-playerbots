#include "ToCDefinitions.h"

#include "Strategy.h"
#include "ToCActions_FactionChampionsBall.h"
#include "ToCEncounterGate.h"
#include "ToCTriggers_FactionChampionsBall.h"

namespace
{
void DefineFactionChampionsBall(EncounterBuilder& e)
{
    e.Node<FactionChampionsOpenerTrigger, FactionChampionsOpenerAction>(ACTION_RAID + 4.4f);

    e.Node<FactionChampionsEscapeTrigger, FactionChampionsEscapeAction>(ACTION_RAID + 2.6f);
    e.Node<FactionChampionsChasedTrigger, FactionChampionsChasedAction>(ACTION_RAID + 2.5f);

    e.Node<FactionChampionsSpeedBoostTrigger, FactionChampionsSpeedBoostAction>(ACTION_RAID + 2.48f);

    e.Node<FactionChampionsChainsOfIceTrigger, FactionChampionsChainsOfIceAction>(ACTION_RAID + 2.47f);
    e.Node<FactionChampionsKiteTrigger, FactionChampionsKiteAction>(ACTION_RAID + 2.45f);
    e.Node<FactionChampionsIceReachTrigger, FactionChampionsIceReachAction>(ACTION_RAID + 2.44f);
    e.Node<FactionChampionsStepOffTrigger, FactionChampionsStepOffAction>(ACTION_RAID + 2.4f);
    e.Node<FactionChampionsIceTargetTrigger, FactionChampionsIceTargetAction>(ACTION_RAID + 2);
    e.Node<FactionChampionsCycloneStandTrigger, FactionChampionsCycloneStandAction>(ACTION_RAID + 1.9f);

    e.Node<FactionChampionsDemonicCircleTrigger, FactionChampionsDemonicCircleAction>(ACTION_RAID + 1);
}
}

EncounterDefinition const& ToCFactionChampionsBallDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::FactionChampions), ToCGate,
                                                "faction-champions", &DefineFactionChampionsBall);
    return definition;
}
