#include "ToCDefinitions.h"

#include "Strategy.h"
#include "ToCActions_Icehowl.h"
#include "ToCActions_NorthrendBeasts.h"
#include "ToCEncounterGate.h"
#include "ToCMultipliers_Icehowl.h"
#include "ToCTriggers_Icehowl.h"
#include "ToCTriggers_NorthrendBeasts.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineIcehowl(EncounterBuilder& e)
{
    e.Node<IcehowlTankDutyTrigger, IcehowlTankHoldBossAction>(ACTION_RAID + 1);
    e.Node<IcehowlChargeIncomingTrigger, IcehowlClearChargePathAction>(ACTION_EMERGENCY + 8);
    e.Node<IcehowlFrothingRageTrigger, IcehowlTankDefensiveAction>(ACTION_RAID + 6);

    e.Node<IcehowlDazeNitroTrigger, IcehowlDazeNitroAction>(ACTION_RAID + 3);

    e.Node(
        "icehowl dps target",
        [](PlayerbotAI* botAI) -> Trigger*
        { return new NorthrendBeastsDpsTargetTrigger(botAI, "icehowl dps target", NorthrendBeastsStem::Icehowl); },
        "icehowl dps target",
        [](PlayerbotAI* botAI) -> Action* { return new NorthrendBeastsDpsTargetAction(botAI, "icehowl dps target"); },
        ACTION_RAID + 2);
    e.Node<IcehowlBreathSpreadTrigger, IcehowlMoveToBreathStandAction>(ACTION_RAID + 2);

    e.LiveMultiplier<IcehowlChargeGuardMultiplier>(Family::AnyMovement | Family::Spell);
    e.LiveMultiplier<IcehowlBreathSpreadGuardMultiplier>(Family::AnyMovement);
}
}

EncounterDefinition const& ToCIcehowlDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::NorthrendBeasts), ToCGate,
                                                "northrend-beasts", &DefineIcehowl);
    return definition;
}
