#include "ToCDefinitions.h"

#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Strategy.h"
#include "ToCActions_FactionChampions.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsBall.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "ToCMultipliers_FactionChampions.h"
#include "ToCTriggers_FactionChampions.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
constexpr uint32 PLAYER_DIVINE_SHIELD = 642;
constexpr uint32 FORBEARANCE = 25771;

bool FactionChampionsTargetOwned(PlayerbotAI* botAI)
{
    return (FactionChampionsFocusBot(botAI) && FactionChampionsKillTarget(botAI)) ||
           FactionChampionsHasIceDuty(botAI);
}

bool FactionChampionsStayPut(PlayerbotAI* botAI) { return FactionChampionsStaysPut(botAI); }

bool FactionChampionsKillSquadWait(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsMelee(bot) || FactionChampionsHasIceDuty(botAI))
        return false;

    return !FactionChampionsChainTarget(bot).IsEmpty() || FactionChampionsGripPending(botAI);
}

bool FactionChampionsTotemHeld(PlayerbotAI* botAI) { return FactionChampionsTotemTarget(botAI) != nullptr; }

bool FactionChampionsGripHeld(PlayerbotAI* botAI)
{
    Position stand;
    return FactionChampionsGripTarget(botAI, stand) != nullptr || FactionChampionsChainLink(botAI);
}

bool FactionChampionsWallGripHeld(PlayerbotAI* botAI)
{
    Position stand;
    uint32 spellId = 0;
    bool gripped = false;
    return FactionChampionsWallGrip(botAI, stand, spellId, &gripped) && !gripped;
}

bool FactionChampionsLive(PlayerbotAI* botAI) { return ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions); }

bool FactionChampionsWrathHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return bot->getClass() == CLASS_PALADIN && (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsHeal(bot)) &&
           FactionChampionsLive(botAI);
}

bool FactionChampionsBubbleReady(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_PALADIN || !FactionChampionsLive(botAI) || !bot->HasSpell(PLAYER_DIVINE_SHIELD) ||
        bot->HasSpellCooldown(PLAYER_DIVINE_SHIELD) || bot->HasAura(FORBEARANCE))
        return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(PLAYER_DIVINE_SHIELD);
    return info && !(info->ExcludeCasterAuraSpell && bot->HasAura(info->ExcludeCasterAuraSpell));
}

bool FactionChampionsHeroicLive(PlayerbotAI* botAI)
{
    Map* map = botAI->GetBot()->FindMap();
    return map && map->IsHeroic() && FactionChampionsLive(botAI);
}

void DefineFactionChampions(EncounterBuilder& e)
{
    e.Node<FactionChampionsAntiFearTrigger, FactionChampionsAntiFearAction>(ACTION_RAID + 3);
    e.Node<FactionChampionsShouldFocusTrigger, FactionChampionsFocusPriorityAction>(ACTION_RAID + 2);
    e.Node<FactionChampionsKillTargetHealingTrigger,
           FactionChampionsCounterspellKillTargetAction>(ACTION_INTERRUPT + 3);
    e.Node<FactionChampionsPetAutocastTrigger, FactionChampionsPetAutocastAction>(ACTION_RAID + 6);

    e.LiveMultiplier<FactionChampionsLateInterruptMultiplier>(Family::Spell);
    e.LiveMultiplier<FactionChampionsSuppressAoeMultiplier>(Family::AnyAction);
    e.LiveMultiplier<FactionChampionsThreatRedirectVetoMultiplier>(Family::Spell);
    e.LiveMultiplier<FactionChampionsAntiFearTotemGuardMultiplier>(Family::Spell);
    e.LiveMultiplier<FactionChampionsCcGuardMultiplier>(Family::Spell);

    e.OwnTargeting("faction champions target guard multiplier", Role::Any, FactionChampionsTargetOwned,
                   Family::DpsAssist | Family::TankAssist);
    e.OwnTargeting("faction champions healer target guard multiplier", Role::Heal, FactionChampionsLive,
                   Family::DpsAssist);

    e.Block("faction champions totem guard multiplier", Role::Any, FactionChampionsTotemHeld, 0,
            {"faction champions focus priority", "faction champions physical switch"});

    e.Block("faction champions grip hold", Role::Any, FactionChampionsGripHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach melee", "reach spell"});

    e.Block("faction champions wall grip hold", Role::Any, FactionChampionsWallGripHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach melee", "reach spell"});

    e.Block("faction champions ice hold", Role::Any, FactionChampionsHasIceDuty,
            Family::CombatFormationMove | Family::Follow | Family::Flee | Family::RunAway | Family::MoveRandom);

    e.Block("faction champions ice rune reserve", Role::Any, FactionChampionsHasIceDuty, 0,
            {"icy touch", "icy touch on attacker", "obliterate", "howling blast", "death strike", "scourge strike",
             "death and decay", "unbreakable armor", "army of the dead", "chains of ice"});

    e.Block("faction champions stay put", Role::Any, FactionChampionsStayPut,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage);

    e.Block("faction champions kite hold", Role::Any, FactionChampionsKiteHold, Family::Charge,
            {"reach spell", "reach melee", "reach party member to heal"});

    e.Block("faction champions kill squad wait", Role::Any, FactionChampionsKillSquadWait, Family::Charge,
            {"reach melee"});

    e.Block("faction champions battle res guard", Role::Any, FactionChampionsLive, 0, {"rebirth"});

    e.Block("faction champions lay on hands guard", Role::Any, FactionChampionsLive, 0, {"lay on hands on party"});
    e.Block("faction champions self lay on hands hold", Role::Any, FactionChampionsBubbleReady, 0, {"lay on hands"});
    e.Block("faction champions avenging wrath hold", Role::Any, FactionChampionsWrathHeld, 0, {"avenging wrath"});
    e.Block("faction champions divine sacrifice hold", Role::Any, FactionChampionsLive, 0, {"divine sacrifice"});
    e.Block("faction champions freedom guard", Role::Any, FactionChampionsLive, 0, {"hand of freedom on party"});
    e.Block("faction champions speed boost guard", Role::Any, FactionChampionsLive, 0, {"sprint", "dash"});
    e.Block("faction champions concussive guard", Role::Any, FactionChampionsLive, 0,
            {"concussive shot on snare target"});
    e.Block("faction champions personal reserve", Role::Any, FactionChampionsLive, 0, {"shamanistic rage"});

    e.Block("faction champions heroic taunt guard", Role::Any, FactionChampionsHeroicLive, Family::Taunt);

    e.Block("faction champions chain grip reserve", Role::Any, FactionChampionsChainLink, 0, {"death grip"});
}
}

EncounterDefinition const& ToCFactionChampionsDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::FactionChampions), ToCGate,
                                                "faction-champions", &DefineFactionChampions);
    return definition;
}
