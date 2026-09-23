#include "ToCEncounterGate.h"

#include <list>

#include "Creature.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCHelpers_FactionChampions.h"

namespace
{
using TrialOfTheCrusaderHelpers::ToCFactionChampions;
using TrialOfTheCrusaderHelpers::ToCNpcs;

struct EncounterPrefix
{
    char const* prefix;
    ToCEncounter encounter;
};

// No prefix here is a prefix of another, so first match wins.
constexpr EncounterPrefix ENCOUNTER_PREFIXES[] = {
    {"gormok", ToCEncounter::NorthrendBeasts},
    {"northrend worms", ToCEncounter::NorthrendBeasts},
    {"icehowl", ToCEncounter::NorthrendBeasts},
    {"jaraxxus", ToCEncounter::Jaraxxus},
    {"faction champions", ToCEncounter::FactionChampions},
    {"twin valkyr", ToCEncounter::TwinValkyr},
    {"anubarak", ToCEncounter::Anubarak},
};

// The arena reaches about 80 yd from ARENA_CENTER (gate 77, stands 64), so from anywhere on the
// floor this sees all of it.
constexpr float ENTRY_SEARCH_RADIUS = 200.0f;

std::vector<uint32> const CHAMPION_ENTRIES = {
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_DRUID_RESTORATION),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_SHAMAN_RESTORATION),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PALADIN_HOLY),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PRIEST_DISCIPLINE),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_DRUID_RESTORATION),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_SHAMAN_RESTORATION),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PALADIN_HOLY),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PRIEST_DISCIPLINE),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_DEATH_KNIGHT),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_DRUID_BALANCE),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_HUNTER),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_MAGE),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PALADIN_RETRIBUTION),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PRIEST_SHADOW),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_ROGUE),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_SHAMAN_ENHANCEMENT),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_WARLOCK),
    static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_WARRIOR),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_DEATH_KNIGHT),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_DRUID_BALANCE),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_HUNTER),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_MAGE),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PALADIN_RETRIBUTION),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PRIEST_SHADOW),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_ROGUE),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_SHAMAN_ENHANCEMENT),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_WARLOCK),
    static_cast<uint32>(ToCFactionChampions::NPC_HORDE_WARRIOR),
};

// One read per instance per ms: every gated trigger and multiplier asks, and a read can walk a
// 200 yd grid.
struct ToCGateState
{
    RaidObs::ObsValue<uint32> progress{"toc.progress"};

    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    uint32 stage = 0;
    ToCEncounter live = ToCEncounter::None;
};

RaidInstanceState<ToCGateState> gateStates;

ToCEncounter EncounterOfStage(uint32 stage)
{
    switch (stage)
    {
        case TOC_PROGRESS_INITIAL:
        case TOC_PROGRESS_INTRO_DONE:
            return ToCEncounter::NorthrendBeasts;
        case TOC_PROGRESS_BEASTS_DEAD:
        case TOC_PROGRESS_JARAXXUS_INTRO_DONE:
            return ToCEncounter::Jaraxxus;
        case TOC_PROGRESS_JARAXXUS_DEAD:
            return ToCEncounter::FactionChampions;
        case TOC_PROGRESS_FACTION_CHAMPIONS_DEAD:
            return ToCEncounter::TwinValkyr;
        case TOC_PROGRESS_ANUB_ARAK:
            return ToCEncounter::Anubarak;
        default:
            return ToCEncounter::None;
    }
}

// The kill moves the stage on before the adds are gone: snobolds riding a player outlive Gormok, who
// can die last on heroic, and Jaraxxus's death leaves the Mistress and Infernals his portals and
// volcanoes summoned.
ToCEncounter EncounterBeforeStage(uint32 stage)
{
    switch (stage)
    {
        case TOC_PROGRESS_BEASTS_DEAD:
            return ToCEncounter::NorthrendBeasts;
        case TOC_PROGRESS_JARAXXUS_DEAD:
            return ToCEncounter::Jaraxxus;
        default:
            return ToCEncounter::None;
    }
}

bool IsEngaged(Creature const* creature) { return creature && creature->IsAlive() && creature->IsInCombat(); }

// A guid resolves map-wide, hence the combat test on top.
bool GuidEngaged(Map* map, InstanceScript* instance, uint32 type)
{
    ObjectGuid const guid = instance->GetGuidData(type);
    return !guid.IsEmpty() && IsEngaged(map->GetCreature(guid));
}

bool AnyEngaged(std::list<Creature*> const& creatures)
{
    for (Creature const* creature : creatures)
        if (IsEngaged(creature))
            return true;

    return false;
}

bool EntryEngaged(Player* bot, uint32 entry)
{
    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, entry, ENTRY_SEARCH_RADIUS);
    return AnyEngaged(creatures);
}

// Icehowl, Jaraxxus and the champions have no guid slot in the script, so those go by entry.
bool EncounterEngaged(Player* bot, Map* map, InstanceScript* instance, ToCEncounter encounter)
{
    switch (encounter)
    {
        case ToCEncounter::NorthrendBeasts:
            return GuidEngaged(map, instance, TOC_DATA_GORMOK) || GuidEngaged(map, instance, TOC_DATA_DREADSCALE) ||
                   GuidEngaged(map, instance, TOC_DATA_ACIDMAW) ||
                   EntryEngaged(bot, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
        case ToCEncounter::Jaraxxus:
            return EntryEngaged(bot, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS));
        case ToCEncounter::FactionChampions:
        {
            std::list<Creature*> champions;
            bot->GetCreatureListWithEntryInGrid(champions, CHAMPION_ENTRIES, ENTRY_SEARCH_RADIUS);
            return AnyEngaged(champions);
        }
        case ToCEncounter::TwinValkyr:
            return GuidEngaged(map, instance, TOC_DATA_FJOLA) || GuidEngaged(map, instance, TOC_DATA_EYDIS);
        case ToCEncounter::Anubarak:
            return GuidEngaged(map, instance, TOC_DATA_ANUBARAK);
        default:
            return false;
    }
}

// Null off a ToC instance.
ToCGateState const* ReadGate(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TrialOfTheCrusaderHelpers::TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance)
        return nullptr;

    ToCGateState& state = gateStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;
    state.stage = instance->GetData(TOC_DATA_INSTANCE_PROGRESS);
    state.progress = state.stage;

    ToCEncounter const encounter = EncounterOfStage(state.stage);
    bool const live = encounter != ToCEncounter::None && instance->IsEncounterInProgress() &&
                      EncounterEngaged(bot, map, instance, encounter);
    state.live = live ? encounter : ToCEncounter::None;

    return &state;
}
}

bool ToCEncounterGateOpen(PlayerbotAI* botAI, ToCEncounter encounter)
{
    ToCGateState const* state = ReadGate(botAI);
    if (!state || EncounterOfStage(state->stage) == encounter)
        return true;

    return state->live == ToCEncounter::None && EncounterBeforeStage(state->stage) == encounter;
}

bool ToCEncounterIsLive(PlayerbotAI* botAI, ToCEncounter encounter)
{
    return encounter != ToCEncounter::None && ToCLiveEncounter(botAI) == encounter;
}

ToCEncounter ToCLiveEncounter(PlayerbotAI* botAI)
{
    ToCGateState const* state = ReadGate(botAI);
    return state ? state->live : ToCEncounter::None;
}

bool ToCEncounterOfTrigger(std::string const& triggerName, ToCEncounter& encounter)
{
    for (EncounterPrefix const& entry : ENCOUNTER_PREFIXES)
    {
        if (triggerName.rfind(entry.prefix, 0) == 0)
        {
            encounter = entry.encounter;
            return true;
        }
    }

    return false;
}

char const* ToCEncounterSlug(ToCEncounter encounter)
{
    switch (encounter)
    {
        case ToCEncounter::NorthrendBeasts:
            return "northrend-beasts";
        case ToCEncounter::Jaraxxus:
            return "lord-jaraxxus";
        case ToCEncounter::FactionChampions:
            return "faction-champions";
        case ToCEncounter::TwinValkyr:
            return "val-kyr-twins";
        case ToCEncounter::Anubarak:
            return "anub-arak";
        default:
            return nullptr;
    }
}

// Interval copied off the inner trigger: the engine calls needCheck on the wrapper, and the default
// of 1 would run a throttled trigger every tick.
ToCGatedTrigger::ToCGatedTrigger(PlayerbotAI* botAI, Trigger* inner, ToCEncounter encounter)
    : Trigger(botAI, inner ? inner->getName() : "trigger", inner ? inner->getCheckInterval() : 1),
      inner(inner),
      encounter(encounter)
{
}

ToCGatedTrigger::~ToCGatedTrigger() { delete inner; }

Event ToCGatedTrigger::Check()
{
    if (!inner || !ToCEncounterGateOpen(botAI, encounter))
        return Event();

    Event event = inner->Check();
    if (!event)
        return event;

    // NamePull only renames a trace still filed under the map name, which the engage hook
    // normally beats it to.
    if (RaidObs::Active() && ToCEncounterIsLive(botAI, encounter))
        if (Map* map = bot ? bot->FindMap() : nullptr)
            RaidObs::NamePull(map, ToCEncounterSlug(encounter));

    return event;
}

bool ToCGatedTrigger::IsActive() { return inner && ToCEncounterGateOpen(botAI, encounter) && inner->IsActive(); }

bool ToCGatedTrigger::IsBuffTrigger() { return inner && inner->IsBuffTrigger(); }

bool ToCGatedTrigger::IsDebuffTrigger() { return inner && inner->IsDebuffTrigger(); }

std::vector<NextAction> ToCGatedTrigger::getHandlers()
{
    return inner ? inner->getHandlers() : std::vector<NextAction>();
}

void ToCGatedTrigger::Reset()
{
    if (inner)
        inner->Reset();
}

Unit* ToCGatedTrigger::GetTarget() { return inner ? inner->GetTarget() : Trigger::GetTarget(); }

Value<Unit*>* ToCGatedTrigger::GetTargetValue()
{
    return inner ? inner->GetTargetValue() : Trigger::GetTargetValue();
}

std::string const ToCGatedTrigger::GetTargetName()
{
    return inner ? inner->GetTargetName() : Trigger::GetTargetName();
}

void ToCGatedTrigger::ExternalEvent(std::string const param, Player* owner)
{
    if (inner)
        inner->ExternalEvent(param, owner);
}

void ToCGatedTrigger::ExternalEvent(WorldPacket& packet, Player* owner)
{
    if (inner)
        inner->ExternalEvent(packet, owner);
}
