#include "ToCDefinitions.h"

#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsBall.h"
#include "ToCHelpers_FactionChampionsDefence.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool FactionChampionsPackTankOwned(PlayerbotAI* botAI)
{
    Position anchor;
    return FactionChampionsPackTankHeld(botAI, anchor);
}

// The focus trigger's test, so the rule and the node cover the same window. The pack tank picks his
// own.
bool FactionChampionsTargetOwned(PlayerbotAI* botAI)
{
    return (FactionChampionsFocusBot(botAI) && FactionChampionsKillTarget(botAI)) ||
           FactionChampionsPackTankOwned(botAI);
}

bool FactionChampionsStandHeld(PlayerbotAI* botAI)
{
    Position spot;
    return FactionChampionsBallStand(botAI, spot);
}

// Melee wait outside the pack until the grip has the kill target out of it
bool FactionChampionsGripWait(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return PlayerbotAI::IsMelee(bot) && !PlayerbotAI::IsTank(bot) && FactionChampionsGripPending(botAI);
}

bool FactionChampionsTotemHeld(PlayerbotAI* botAI) { return FactionChampionsTotemTarget(botAI) != nullptr; }

bool FactionChampionsGripHeld(PlayerbotAI* botAI)
{
    Position stand;
    return FactionChampionsGripTarget(botAI, stand) != nullptr;
}

bool FactionChampionsLive(PlayerbotAI* botAI) { return ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions); }

bool FactionChampionsHeroicLive(PlayerbotAI* botAI)
{
    Map* map = botAI->GetBot()->FindMap();
    return map && map->IsHeroic() && FactionChampionsLive(botAI);
}

void DefineFactionChampions(EncounterBuilder& e)
{
    // Assist makes its own pick, so on any tick the settled focus node returns false it would take the
    // bot off the kill target. The name is what the reader counts vetoes under.
    e.OwnTargeting("faction champions target guard multiplier", Role::Any, FactionChampionsTargetOwned,
                   Family::DpsAssist | Family::TankAssist);

    // once the bot is on its totem the kill totem node returns false, and either of these would pull it off
    e.Block("faction champions totem guard multiplier", Role::Any, FactionChampionsTotemHeld, 0,
            {"faction champions focus priority", "faction champions physical switch"});

    // the DK walks out of melee to its grip stand, and these would walk it straight back
    e.Block("faction champions grip hold", Role::Any, FactionChampionsGripHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach melee", "reach spell"});

    // the pack follows its tank, so he stays where it landed
    e.Block("faction champions pack hold", Role::Any, FactionChampionsPackTankOwned,
            Family::CombatFormationMove | Family::Follow | Family::Flee | Family::RunAway | Family::MoveRandom |
                Family::Charge);

    // ranged and healers keep the kill target between them and the pack, and the reaches would walk
    // them back in toward the pack tank or a target past the slot
    e.Block("faction champions stand hold", Role::Any, FactionChampionsStandHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach spell", "reach party member to heal"});

    e.Block("faction champions grip wait", Role::Any, FactionChampionsGripWait, Family::Charge, {"reach melee"});

    // the class Rebirth raises the master first and a corpse inside the pack too, faction champions battle
    // res picks the corpse instead
    e.Block("faction champions battle res guard", Role::Any, FactionChampionsLive, 0, {"rebirth"});

    // heroic champions are immune to taunt, and the generic Death Grip would spend the grip duty's cooldown
    e.Block("faction champions heroic taunt guard", Role::Any, FactionChampionsHeroicLive, Family::Taunt);
}
}  // namespace

EncounterDefinition const& ToCFactionChampionsDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::FactionChampions), ToCGate,
                                                "faction-champions", &DefineFactionChampions);
    return definition;
}
