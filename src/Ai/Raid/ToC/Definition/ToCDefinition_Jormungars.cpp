#include "ToCDefinitions.h"

#include "Strategy.h"
#include "ToCActions_Jormungars.h"
#include "ToCActions_NorthrendBeasts.h"
#include "ToCEncounterGate.h"
#include "ToCMultipliers_Jormungars.h"
#include "ToCTriggers_Jormungars.h"
#include "ToCTriggers_NorthrendBeasts.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineJormungars(EncounterBuilder& e)
{
    e.Node<NorthrendWormsMobileTankDutyTrigger, NorthrendWormsTankHoldMobileWormAction>(ACTION_RAID + 1);
    e.Node<NorthrendWormsStationaryTankDutyTrigger, NorthrendWormsTankHoldStationaryWormAction>(ACTION_RAID + 1);
    e.Node<NorthrendWormsRedirectThreatTrigger, NorthrendWormsRedirectThreatAction>(ACTION_RAID + 1);
    e.Node(
        "northrend worms dps target",
        [](PlayerbotAI* botAI) -> Trigger*
        { return new NorthrendBeastsDpsTargetTrigger(botAI, "northrend worms dps target", NorthrendBeastsStem::Worms); },
        "northrend worms dps target",
        [](PlayerbotAI* botAI) -> Action*
        { return new NorthrendBeastsDpsTargetAction(botAI, "northrend worms dps target"); },
        ACTION_RAID + 2);
    e.Node<NorthrendWormsMisplacedTrigger, NorthrendWormsRepositionAction>(ACTION_EMERGENCY + 6);
    e.Node<NorthrendWormsPoolAfterWormsTrigger, NorthrendWormsLeavePoolAction>(ACTION_EMERGENCY + 6);
    e.Node<NorthrendWormsToxinStuckTrigger, NorthrendWormsPurgeToxinAction>(ACTION_EMERGENCY + 5);

    e.Node<NorthrendWormsToxinCarrierHurtTrigger, NorthrendWormsHealToxinCarrierAction>(ACTION_RAID + 3);

    e.LiveMultiplier<NorthrendWormsMoveGuardMultiplier>(Family::AnyMovement | Family::Spell);
    e.LiveMultiplier<NorthrendWormsBileReachGuardMultiplier>(Family::AnyMovement | Family::Spell);
    e.LiveMultiplier<NorthrendWormsRedirectGuardMultiplier>(Family::Spell);
    e.LiveMultiplier<NorthrendWormsThreatBrakeMultiplier>(Family::Spell | Family::Melee);
}
}

EncounterDefinition const& ToCJormungarsDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::NorthrendBeasts), ToCGate,
                                                "northrend-beasts", &DefineJormungars);
    return definition;
}
