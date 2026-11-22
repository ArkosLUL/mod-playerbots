#include "ToCDefinitions.h"

#include "Strategy.h"
#include "ToCActions_FactionChampionsDefence.h"
#include "ToCEncounterGate.h"
#include "ToCMultipliers_FactionChampionsDefence.h"
#include "ToCTriggers_FactionChampionsDefence.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineFactionChampionsDefence(EncounterBuilder& e)
{
    e.Node<FactionChampionsPersonalDefensiveTrigger, FactionChampionsPersonalDefensiveAction>(ACTION_EMERGENCY + 8.4f);
    e.Node<FactionChampionsAvoidAoeTrigger, FactionChampionsAvoidAoeAction>(ACTION_EMERGENCY + 6);
    e.Node<FactionChampionsChannelInterruptTrigger, FactionChampionsChannelInterruptAction>(ACTION_INTERRUPT + 4);
    e.Node<FactionChampionsProtectTrigger, FactionChampionsProtectAction>(ACTION_EMERGENCY + 4);
    e.Node<FactionChampionsFreedomTrigger, FactionChampionsFreedomAction>(ACTION_EMERGENCY + 3);

    e.Node<FactionChampionsChainGripTrigger, FactionChampionsChainGripAction>(ACTION_RAID + 4.6f);
    e.Node<FactionChampionsDeathGripTrigger, FactionChampionsDeathGripAction>(ACTION_RAID + 4.5f);
    e.Node<FactionChampionsBattleResTrigger, FactionChampionsBattleResAction>(ACTION_RAID + 4.2f);
    e.Node<FactionChampionsMassDispelTrigger, FactionChampionsMassDispelAction>(ACTION_RAID + 5);
    e.Node<FactionChampionsDispelCcTrigger, FactionChampionsDispelCcAction>(ACTION_RAID + 3.5f);
    e.Node<FactionChampionsCrowdControlTrigger, FactionChampionsCrowdControlAction>(ACTION_RAID + 3.4f);
    e.Node<FactionChampionsPeelTrigger, FactionChampionsPeelAction>(ACTION_RAID + 3.3f);
    e.Node<FactionChampionsSnareTrigger, FactionChampionsSnareAction>(ACTION_RAID + 3.2f);
    e.Node<FactionChampionsKillTotemTrigger, FactionChampionsKillTotemAction>(ACTION_RAID + 2.7f);
    e.Node<FactionChampionsPhysicalSwitchTrigger, FactionChampionsPhysicalSwitchAction>(ACTION_RAID + 2.5f);

    e.Node<FactionChampionsWallGripTrigger, FactionChampionsWallGripAction>(ACTION_RAID + 2.3f);
    e.Node<FactionChampionsPurgeKillTargetTrigger, FactionChampionsPurgeKillTargetAction>(ACTION_RAID);

    e.LiveMultiplier<FactionChampionsGripGuardMultiplier>(Family::AnyAction);
    e.LiveMultiplier<FactionChampionsAoeGuardMultiplier>(Family::AnyMovement);
    e.LiveMultiplier<FactionChampionsPhysicalSwitchGuardMultiplier>(Family::AnyMovement);
    e.LiveMultiplier<FactionChampionsDispelGuardMultiplier>(Family::Spell);
    e.LiveMultiplier<FactionChampionsPurgeGuardMultiplier>(Family::Spell);
    e.LiveMultiplier<FactionChampionsIceGuardMultiplier>(Family::Spell);
}
}

EncounterDefinition const& ToCFactionChampionsDefenceDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::FactionChampions), ToCGate,
                                                "faction-champions", &DefineFactionChampionsDefence);
    return definition;
}
