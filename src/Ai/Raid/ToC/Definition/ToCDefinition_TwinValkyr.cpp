#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Shared.h"
#include "ToCHelpers_TwinValkyr.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
// The dps target trigger's test, so the rule and the node cover the same window
bool TwinValkyrDpsTargetOwned(PlayerbotAI* botAI)
{
    return IsToCDpsBot(botAI->GetBot()) && ToCEncounterIsLive(botAI, ToCEncounter::TwinValkyr) &&
           GetTwinDpsTarget(botAI);
}

// The two hold triggers' twin tests, so a tank keeps tank assist only while no hold covers it
bool TwinValkyrTankTargetOwned(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Unit* fjola = GetFjola(botAI);
    if (IsTwinAttackable(fjola) && IsTwinTank(bot, fjola))
        return true;

    Unit* eydis = GetEydis(botAI);
    return IsTwinAttackable(eydis) && IsTwinTank(bot, eydis);
}

void DefineTwinValkyr(EncounterBuilder& e)
{
    e.OwnTargeting("twin valkyr dps target guard", Role::Any, TwinValkyrDpsTargetOwned, Family::DpsAssist);
    e.OwnTargeting("twin valkyr tank target guard", Role::Any, TwinValkyrTankTargetOwned, Family::TankAssist);
}
}  // namespace

EncounterDefinition const& ToCTwinValkyrDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::TwinValkyr), ToCGate,
                                                "val-kyr-twins", &DefineTwinValkyr);
    return definition;
}
