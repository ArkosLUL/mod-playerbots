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
bool FactionChampionsHolding(PlayerbotAI* botAI) { return FactionChampionsHoldOf(botAI).champion != nullptr; }

// The focus trigger's test, so the rule and the node cover the same window. A holder hits its own champion.
bool FactionChampionsTargetOwned(PlayerbotAI* botAI)
{
    return (FactionChampionsFocusBot(botAI) && FactionChampionsKillTarget(botAI)) || FactionChampionsHolding(botAI);
}

bool FactionChampionsStandHeld(PlayerbotAI* botAI)
{
    Position spot;
    return FactionChampionsBallStand(botAI, spot);
}

// Kill squad melee, tanks included, wait outside the pack while the player's pull is still flying, its healer
// takes the kill latch once it lands, or while a DK is on its way to grip the kill target out of the pack
bool FactionChampionsKillSquadWait(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsMelee(bot) || FactionChampionsIsHolder(bot))
        return false;

    return !FactionChampionsChainTarget(bot).IsEmpty() || FactionChampionsGripPending(botAI);
}

bool FactionChampionsTotemHeld(PlayerbotAI* botAI) { return FactionChampionsTotemTarget(botAI) != nullptr; }

bool FactionChampionsGripHeld(PlayerbotAI* botAI)
{
    Position stand;
    return FactionChampionsGripTarget(botAI, stand) != nullptr;
}

bool FactionChampionsLive(PlayerbotAI* botAI) { return ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions); }

// Its 30 s marker blocks Divine Shield, Divine Protection and Hand of Protection, and these paladins are
// the ones the champions focus
bool FactionChampionsWrathHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return bot->getClass() == CLASS_PALADIN &&
           (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsHeal(bot) || FactionChampionsIsHolder(bot)) &&
           FactionChampionsLive(botAI);
}

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

    // its champion follows wherever it goes, and these would walk it toward the raid
    e.Block("faction champions holder hold", Role::Any, FactionChampionsHolding,
            Family::CombatFormationMove | Family::Follow | Family::Flee | Family::RunAway | Family::MoveRandom);

    // ranged and healers hold their slots, and the reaches would walk them in toward a target past the slot
    e.Block("faction champions stand hold", Role::Any, FactionChampionsStandHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach spell", "reach party member to heal"});

    e.Block("faction champions kill squad wait", Role::Any, FactionChampionsKillSquadWait, Family::Charge,
            {"reach melee"});

    // the class Rebirth raises the master first and a corpse inside the pack too, faction champions battle
    // res picks the corpse instead
    e.Block("faction champions battle res guard", Role::Any, FactionChampionsLive, 0, {"rebirth"});

    // every paladin's class node fires at one dying bot at once; faction champions protect deals one
    e.Block("faction champions lay on hands guard", Role::Any, FactionChampionsLive, 0, {"lay on hands on party"});
    e.Block("faction champions avenging wrath hold", Role::Any, FactionChampionsWrathHeld, 0, {"avenging wrath"});
    // faction champions protect spends it on a focused bot
    e.Block("faction champions divine sacrifice hold", Role::Any, FactionChampionsLive, 0, {"divine sacrifice"});

    // heroic champions are immune to taunt, and the generic Death Grip would spend the grip duty's cooldown
    e.Block("faction champions heroic taunt guard", Role::Any, FactionChampionsHeroicLive, Family::Taunt);

    // the DK tank's grip opens every chain the player pulls
    e.Block("faction champions chain grip reserve", Role::Tank, FactionChampionsIsChainOpener, 0, {"death grip"});
}
}  // namespace

EncounterDefinition const& ToCFactionChampionsDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::FactionChampions), ToCGate,
                                                "faction-champions", &DefineFactionChampions);
    return definition;
}
