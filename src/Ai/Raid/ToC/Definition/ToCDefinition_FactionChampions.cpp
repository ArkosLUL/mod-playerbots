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
bool FactionChampionsFrontlineOwned(PlayerbotAI* botAI)
{
    Position centre;
    return FactionChampionsFrontlineHeld(botAI, centre);
}

// The focus trigger's test, so the rule and the node cover the same window. The frontline picks its
// own.
bool FactionChampionsTargetOwned(PlayerbotAI* botAI)
{
    return (FactionChampionsFocusBot(botAI) && FactionChampionsKillTarget(botAI)) ||
           FactionChampionsFrontlineOwned(botAI);
}

bool FactionChampionsStandHeld(PlayerbotAI* botAI)
{
    Position spot;
    return FactionChampionsBallStand(botAI, spot);
}

// Kill squad melee wait outside the pack until a grip has the kill target out of it. Once it's been out
// they stay on it even if the pack walks over, and with no champion healer left nothing heals it in there
bool FactionChampionsKillSquadWait(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsTank(bot) || FactionChampionsIsFrontline(bot))
        return false;

    if (FactionChampionsGripPending(botAI))
        return true;

    return FactionChampionsKillTarget(botAI) && !FactionChampionsKillTargetOut(botAI) &&
           FactionChampionsHealerLeft(botAI);
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

    // the pack follows whoever it picks, so the frontline stays with the pack
    e.Block("faction champions frontline hold", Role::Any, FactionChampionsFrontlineOwned,
            Family::CombatFormationMove | Family::Follow | Family::Flee | Family::RunAway | Family::MoveRandom |
                Family::Charge);

    // ranged and healers hold their slots, and the reaches would walk them in toward the frontline or a
    // target past the slot
    e.Block("faction champions stand hold", Role::Any, FactionChampionsStandHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach spell", "reach party member to heal"});

    e.Block("faction champions kill squad wait", Role::Any, FactionChampionsKillSquadWait, Family::Charge,
            {"reach melee"});

    // the class Rebirth raises the master first and a corpse inside the pack too, faction champions battle
    // res picks the corpse instead
    e.Block("faction champions battle res guard", Role::Any, FactionChampionsLive, 0, {"rebirth"});

    // heroic champions are immune to taunt, and the generic Death Grip would spend the grip duty's cooldown
    e.Block("faction champions heroic taunt guard", Role::Any, FactionChampionsHeroicLive, Family::Taunt);

    // the DK tank's grip opens every chain the player calls
    e.Block("faction champions chain grip reserve", Role::Tank, FactionChampionsIsChainOpener, 0, {"death grip"});
}
}  // namespace

EncounterDefinition const& ToCFactionChampionsDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::FactionChampions), ToCGate,
                                                "faction-champions", &DefineFactionChampions);
    return definition;
}
