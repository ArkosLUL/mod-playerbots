#include "ToCHelpers_FactionChampions.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Map.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RtiTargetValue.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCEncounterGate.h"

namespace TrialOfTheCrusaderHelpers
{

namespace
{
struct KillOrderRow
{
    ToCFactionChampions alliance;
    ToCFactionChampions horde;
};

// Healers first, then the Warcraft Tavern Faction Champions guide's DPS danger table. CC goes the same way.
constexpr KillOrderRow KILL_ORDER[] = {
    {ToCFactionChampions::NPC_ALLIANCE_PALADIN_HOLY, ToCFactionChampions::NPC_HORDE_PALADIN_HOLY},
    {ToCFactionChampions::NPC_ALLIANCE_PRIEST_DISCIPLINE, ToCFactionChampions::NPC_HORDE_PRIEST_DISCIPLINE},
    {ToCFactionChampions::NPC_ALLIANCE_SHAMAN_RESTORATION, ToCFactionChampions::NPC_HORDE_SHAMAN_RESTORATION},
    {ToCFactionChampions::NPC_ALLIANCE_DRUID_RESTORATION, ToCFactionChampions::NPC_HORDE_DRUID_RESTORATION},
    {ToCFactionChampions::NPC_ALLIANCE_ROGUE, ToCFactionChampions::NPC_HORDE_ROGUE},
    {ToCFactionChampions::NPC_ALLIANCE_WARRIOR, ToCFactionChampions::NPC_HORDE_WARRIOR},
    {ToCFactionChampions::NPC_ALLIANCE_HUNTER, ToCFactionChampions::NPC_HORDE_HUNTER},
    {ToCFactionChampions::NPC_ALLIANCE_SHAMAN_ENHANCEMENT, ToCFactionChampions::NPC_HORDE_SHAMAN_ENHANCEMENT},
    {ToCFactionChampions::NPC_ALLIANCE_DEATH_KNIGHT, ToCFactionChampions::NPC_HORDE_DEATH_KNIGHT},
    {ToCFactionChampions::NPC_ALLIANCE_PALADIN_RETRIBUTION, ToCFactionChampions::NPC_HORDE_PALADIN_RETRIBUTION},
    {ToCFactionChampions::NPC_ALLIANCE_WARLOCK, ToCFactionChampions::NPC_HORDE_WARLOCK},
    {ToCFactionChampions::NPC_ALLIANCE_PRIEST_SHADOW, ToCFactionChampions::NPC_HORDE_PRIEST_SHADOW},
    {ToCFactionChampions::NPC_ALLIANCE_MAGE, ToCFactionChampions::NPC_HORDE_MAGE},
    {ToCFactionChampions::NPC_ALLIANCE_DRUID_BALANCE, ToCFactionChampions::NPC_HORDE_DRUID_BALANCE},
};

constexpr uint8 NOT_A_CHAMPION = static_cast<uint8>(std::size(KILL_ORDER));

// Same radius as the encounter gate, which sees the whole arena from anywhere on its floor
constexpr float CHAMPION_SEARCH_RADIUS = 200.0f;
constexpr float COUNTERSPELL_RANGE = 30.0f;

constexpr uint8 ICON_COUNT = 8;
constexpr uint8 NO_ICON = ICON_COUNT;
constexpr uint8 SKULL_ICON = static_cast<uint8>(RtiTargetValue::skullIndex);

// Handed out in this order, each CC bot keeping its own
constexpr std::array<uint8, 7> CC_ICONS = {
    static_cast<uint8>(RtiTargetValue::moonIndex),     static_cast<uint8>(RtiTargetValue::squareIndex),
    static_cast<uint8>(RtiTargetValue::triangleIndex), static_cast<uint8>(RtiTargetValue::diamondIndex),
    static_cast<uint8>(RtiTargetValue::circleIndex),   static_cast<uint8>(RtiTargetValue::starIndex),
    static_cast<uint8>(RtiTargetValue::crossIndex),
};

// Indexed like RtiTargetValue
char const* const ICON_NAMES[ICON_COUNT] = {"star", "circle", "diamond", "triangle",
                                            "moon", "square", "cross",   "skull"};

struct Champion
{
    ObjectGuid guid;
    uint32 entry = 0;
    uint8 rank = NOT_A_CHAMPION;
    bool inCombat = false;
    bool touchable = false;
    float healthPct = 0.0f;
};

struct FactionChampionsState
{
    RaidObs::ObsValue<ObjectGuid> killTarget{"fc.kill"};
    RaidObs::ObsGuidMap<ObjectGuid> ccTargets{"fc.cc"};

    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    bool live = false;
    std::vector<Champion> champions;  // alive, in kill order
    ObjectGuid suspended;
    std::unordered_map<ObjectGuid, uint8> ccIcons;
    // "rti cc" is saved to the bot's DB store, so whatever it held before the first assignment goes back
    std::unordered_map<ObjectGuid, std::string> savedRtiCc;
    std::array<ObjectGuid, ICON_COUNT> placedIcons{};
};

RaidInstanceState<FactionChampionsState> championStates;

uint8 KillRank(uint32 entry)
{
    for (uint8 rank = 0; rank < NOT_A_CHAMPION; ++rank)
        if (entry == static_cast<uint32>(KILL_ORDER[rank].alliance) ||
            entry == static_cast<uint32>(KILL_ORDER[rank].horde))
            return rank;

    return NOT_A_CHAMPION;
}

std::vector<uint32> const& ChampionEntries()
{
    static std::vector<uint32> const entries = []
    {
        std::vector<uint32> list;
        for (KillOrderRow const& row : KILL_ORDER)
        {
            list.push_back(static_cast<uint32>(row.alliance));
            list.push_back(static_cast<uint32>(row.horde));
        }

        return list;
    }();

    return entries;
}

// Fear 65809, Psychic Scream 65543 (disc and shadow), Intimidating Shout 65930
bool CastsFear(uint32 entry)
{
    switch (static_cast<ToCFactionChampions>(entry))
    {
        case ToCFactionChampions::NPC_ALLIANCE_WARLOCK:
        case ToCFactionChampions::NPC_HORDE_WARLOCK:
        case ToCFactionChampions::NPC_ALLIANCE_PRIEST_DISCIPLINE:
        case ToCFactionChampions::NPC_HORDE_PRIEST_DISCIPLINE:
        case ToCFactionChampions::NPC_ALLIANCE_PRIEST_SHADOW:
        case ToCFactionChampions::NPC_HORDE_PRIEST_SHADOW:
        case ToCFactionChampions::NPC_ALLIANCE_WARRIOR:
        case ToCFactionChampions::NPC_HORDE_WARRIOR:
            return true;
        default:
            return false;
    }
}

uint32 ToCInstanceId(Player* bot)
{
    Map* map = bot ? bot->FindMap() : nullptr;
    return map && map->GetId() == TRIAL_OF_THE_CRUSADER_MAP_ID ? bot->GetInstanceId() : 0;
}

// Null off a ToC instance.
FactionChampionsState* StateFor(Player* bot)
{
    uint32 const instanceId = ToCInstanceId(bot);
    return instanceId ? &championStates.For(instanceId) : nullptr;
}

// Null also when this instance never had one, so asking doesn't create it.
FactionChampionsState* FindState(Player* bot)
{
    uint32 const instanceId = ToCInstanceId(bot);
    return instanceId ? championStates.Find(instanceId) : nullptr;
}

void RestoreRtiCc(PlayerbotAI* botAI, FactionChampionsState& state)
{
    auto const saved = state.savedRtiCc.find(botAI->GetBot()->GetGUID());
    if (saved == state.savedRtiCc.end())
        return;

    botAI->GetAiObjectContext()->GetValue<std::string>("rti cc")->Set(saved->second);
    state.savedRtiCc.erase(saved);
}

Creature* AliveCreature(Player* bot, ObjectGuid guid)
{
    Map* map = bot->FindMap();
    if (!map || guid.IsEmpty())
        return nullptr;

    Creature* creature = map->GetCreature(guid);
    return creature && creature->IsAlive() ? creature : nullptr;
}

void EndPull(FactionChampionsState& state, Player* bot)
{
    state.champions.clear();
    state.suspended.Clear();

    if (!state.killTarget.Get().IsEmpty())
    {
        RaidObs::Note(bot, "fc.switch", "reset");
        state.killTarget = ObjectGuid::Empty;
    }

    std::vector<ObjectGuid> held;
    for (auto const& [ccBot, target] : state.ccTargets.Raw())
        held.push_back(ccBot);

    for (ObjectGuid const& ccBot : held)
        state.ccTargets.erase(ccBot);

    state.ccIcons.clear();
}

void ScanChampions(FactionChampionsState& state, Player* bot)
{
    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, ChampionEntries(), CHAMPION_SEARCH_RADIUS);

    state.champions.clear();
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

        Champion champion;
        champion.guid = creature->GetGUID();
        champion.entry = creature->GetEntry();
        champion.rank = KillRank(champion.entry);
        champion.inCombat = creature->IsInCombat();
        // Divine Shield, Ice Block or Cyclone, not Hand of Protection (physical only). Asked per half since
        // Ice Block is a physical aura plus a magic one, and IsImmunedToDamage wants one aura per mask
        champion.touchable = !(creature->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) &&
                               creature->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC));
        champion.healthPct = creature->GetHealthPct();
        state.champions.push_back(champion);
    }

    std::sort(state.champions.begin(), state.champions.end(),
              [](Champion const& a, Champion const& b)
              { return a.rank != b.rank ? a.rank < b.rank : a.guid < b.guid; });
}

Champion const* FindCandidate(FactionChampionsState const& state, ObjectGuid guid)
{
    if (guid.IsEmpty())
        return nullptr;

    for (Champion const& champion : state.champions)
        if (champion.guid == guid)
            return champion.inCombat ? &champion : nullptr;

    return nullptr;
}

Champion const* BestCandidate(FactionChampionsState const& state, bool touchable, ObjectGuid exclude)
{
    for (Champion const& champion : state.champions)
        if (champion.inCombat && champion.touchable == touchable && champion.guid != exclude)
            return &champion;

    return nullptr;
}

void UpdateKillLatch(FactionChampionsState& state, Player* bot)
{
    ObjectGuid const latched = state.killTarget.Get();
    Champion const* current = FindCandidate(state, latched);
    Champion const* suspended = FindCandidate(state, state.suspended);
    if (!suspended)
        state.suspended.Clear();

    ObjectGuid next = latched;
    char const* reason = nullptr;

    if (current && current->touchable)
    {
        if (suspended && suspended->touchable && suspended->healthPct <= current->healthPct)
        {
            next = suspended->guid;
            reason = "back";
            state.suspended.Clear();
        }
    }
    else if (current)
    {
        // With nobody else touchable there's nothing better to hit, so it stays
        if (Champion const* other = BestCandidate(state, true, current->guid))
        {
            state.suspended = current->guid;
            next = other->guid;
            reason = "immune";
        }
    }
    else
    {
        Champion const* best = BestCandidate(state, true, ObjectGuid::Empty);
        if (!best)
            best = BestCandidate(state, false, ObjectGuid::Empty);

        next = best ? best->guid : ObjectGuid::Empty;
        if (best)
        {
            reason = latched.IsEmpty() ? "first" : "dead";
            if (best->guid == state.suspended)
                state.suspended.Clear();
        }
    }

    if (reason)
        RaidObs::Note(bot, "fc.switch", reason);

    state.killTarget = next;
}

bool IsCcBot(Player* member, Map* map)
{
    if (!member || !member->IsInWorld() || !member->IsAlive() || member->FindMap() != map)
        return false;

    switch (member->getClass())
    {
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_DRUID:
            break;
        default:
            return false;
    }

    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    return memberAI && !PlayerbotAI::IsHeal(member) && memberAI->HasStrategy("cc", BOT_STATE_COMBAT);
}

uint8 FreeCcIcon(FactionChampionsState const& state)
{
    for (uint8 icon : CC_ICONS)
    {
        bool const taken = std::any_of(state.ccIcons.begin(), state.ccIcons.end(),
                                       [icon](auto const& held) { return held.second == icon; });
        if (!taken)
            return icon;
    }

    return NO_ICON;
}

void UpdateCcAssignments(FactionChampionsState& state, Player* bot)
{
    std::vector<ObjectGuid> ccBots;
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (IsCcBot(ref->GetSource(), bot->FindMap()))
                ccBots.push_back(ref->GetSource()->GetGUID());

    std::sort(ccBots.begin(), ccBots.end());

    ObjectGuid const killTarget = state.killTarget.Get();
    std::vector<ObjectGuid> candidates;
    // Not the suspended one either: the latch goes back to it, and CC on it would break or block that
    for (Champion const& champion : state.champions)
        if (champion.guid != killTarget && champion.guid != state.suspended)
            candidates.push_back(champion.guid);

    std::vector<ObjectGuid> released;
    for (auto const& [ccBot, target] : state.ccTargets.Raw())
        if (std::find(ccBots.begin(), ccBots.end(), ccBot) == ccBots.end() ||
            std::find(candidates.begin(), candidates.end(), target) == candidates.end())
            released.push_back(ccBot);

    for (ObjectGuid const& ccBot : released)
    {
        state.ccTargets.erase(ccBot);
        state.ccIcons.erase(ccBot);
    }

    auto const& assigned = state.ccTargets.Raw();
    for (ObjectGuid const& ccBot : ccBots)
    {
        if (assigned.count(ccBot))
            continue;

        auto const unassigned = std::find_if(candidates.begin(), candidates.end(),
                                             [&assigned](ObjectGuid const& candidate)
                                             {
                                                 return std::none_of(assigned.begin(), assigned.end(),
                                                                     [&candidate](auto const& held)
                                                                     { return held.second == candidate; });
                                             });
        uint8 const icon = FreeCcIcon(state);
        if (unassigned == candidates.end() || icon == NO_ICON)
            break;

        state.ccTargets.Set(ccBot, *unassigned);
        state.ccIcons[ccBot] = icon;
    }
}

FactionChampionsState* Refresh(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    FactionChampionsState* state = StateFor(bot);
    if (!state)
        return nullptr;

    // Every node and multiplier asks, and a refresh walks a 200 yd grid
    uint32 const now = getMSTime();
    if (state->memoValid && state->memoMs == now)
        return state;

    state->memoMs = now;
    state->memoValid = true;
    state->live = ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions);
    if (!state->live)
    {
        EndPull(*state, bot);
        return state;
    }

    ScanChampions(*state, bot);
    UpdateKillLatch(*state, bot);
    UpdateCcAssignments(*state, bot);
    return state;
}

struct MarkWork
{
    bool moveSkull = false;
    std::vector<uint8> clearIcons;
};

bool AssignmentGivesIcon(FactionChampionsState const& state, uint8 icon, ObjectGuid target)
{
    for (auto const& [ccBot, held] : state.ccIcons)
    {
        if (held != icon)
            continue;

        auto const assigned = state.ccTargets.Raw().find(ccBot);
        return assigned != state.ccTargets.Raw().end() && assigned->second == target;
    }

    return false;
}

bool IsChampionGuid(Map* map, ObjectGuid guid)
{
    if (!guid.IsCreature())
        return false;

    Creature* creature = map->GetCreature(guid);
    return creature && IsFactionChampion(creature->GetEntry());
}

// Marks on anything but a champion are left alone, and between pulls every mark is, unless it's
// still where this encounter put it.
MarkWork PendingMarks(FactionChampionsState const& state, Player* bot, Group* group)
{
    MarkWork work;
    if (state.live)
    {
        ObjectGuid const killTarget = state.killTarget.Get();
        work.moveSkull = !killTarget.IsEmpty() && group->GetTargetIcon(SKULL_ICON) != killTarget;

        Map* map = bot->FindMap();
        for (uint8 icon : CC_ICONS)
        {
            ObjectGuid const held = group->GetTargetIcon(icon);
            if (map && IsChampionGuid(map, held) && !AssignmentGivesIcon(state, icon, held))
                work.clearIcons.push_back(icon);
        }

        return work;
    }

    for (uint8 icon = 0; icon < ICON_COUNT; ++icon)
    {
        ObjectGuid const placed = state.placedIcons[icon];
        if (!placed.IsEmpty() && group->GetTargetIcon(icon) == placed)
            work.clearIcons.push_back(icon);
    }

    return work;
}

// Counterspell only interrupts a spell passing these checks (Spell::EffectInterruptCast), so the
// cooldown only goes on a heal it stops
bool CounterspellStopsHeal(Unit* champion)
{
    for (CurrentSpellTypes type : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
    {
        Spell const* spell = champion->GetCurrentSpell(type);
        if (!spell)
            continue;

        SpellInfo const* info = spell->GetSpellInfo();
        bool const casting = spell->getState() == SPELL_STATE_CASTING ||
                             (spell->getState() == SPELL_STATE_PREPARING && spell->GetCastTime() > 0);
        bool const interruptible = type == CURRENT_CHANNELED_SPELL
                                       ? (info->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT)
                                       : (info->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT);

        if (info->IsPositive() && casting && interruptible && info->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
            return true;
    }

    return false;
}

// No pacify check, only silence stops Counterspell (Hex sets both). A school lockout shows up as
// the spell's own cooldown.
bool CounterspellReady(Player* mage, Unit* target)
{
    return mage->HasSpell(SPELL_COUNTERSPELL) && !mage->HasSpellCooldown(SPELL_COUNTERSPELL) &&
           !mage->IsNonMeleeSpellCast(false) && !mage->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
           !mage->HasUnitFlag(UNIT_FLAG_SILENCED) && mage->IsWithinDistInMap(target, COUNTERSPELL_RANGE) &&
           !target->IsImmunedToSpell(sSpellMgr->GetSpellInfo(SPELL_COUNTERSPELL));
}

// Lowest guid among the ready mage bots, so one Counterspell answers each heal
Player* CounterspellMage(Player* bot, Unit* target)
{
    Group* group = bot->GetGroup();
    if (!group)
        return CounterspellReady(bot, target) ? bot : nullptr;

    Map* map = bot->FindMap();
    Player* chosen = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsInWorld() || member->FindMap() != map || !member->IsAlive() ||
            member->getClass() != CLASS_MAGE || !GET_PLAYERBOT_AI(member) || !CounterspellReady(member, target))
            continue;

        if (!chosen || member->GetGUID() < chosen->GetGUID())
            chosen = member;
    }

    return chosen;
}
}

bool IsFactionChampionHealer(uint32 entry)
{
    switch (entry)
    {
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_DRUID_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_SHAMAN_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PALADIN_HOLY):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PRIEST_DISCIPLINE):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_DRUID_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_SHAMAN_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PALADIN_HOLY):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PRIEST_DISCIPLINE):
            return true;
        default:
            return false;
    }
}

bool IsFactionChampion(uint32 entry) { return KillRank(entry) != NOT_A_CHAMPION; }

// These two check live first: after the kill only multipliers still ask, and a refresh from them
// would write the reset into the next encounter's trace.
Unit* FactionChampionsKillTarget(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live ? AliveCreature(botAI->GetBot(), state->killTarget.Get()) : nullptr;
}

Unit* FactionChampionsSuspendedTarget(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live ? AliveCreature(botAI->GetBot(), state->suspended) : nullptr;
}

Unit* FactionChampionsNextTarget(PlayerbotAI* botAI, std::function<bool(Unit*)> const& accept)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    if (!state || !state->live)
        return nullptr;

    ObjectGuid const killTarget = state->killTarget.Get();
    auto const& ccTargets = state->ccTargets.Raw();
    for (Champion const& champion : state->champions)
    {
        if (!champion.inCombat || champion.guid == killTarget || champion.guid == state->suspended)
            continue;

        bool const ccAssigned = std::any_of(ccTargets.begin(), ccTargets.end(),
                                            [&champion](auto const& held) { return held.second == champion.guid; });
        if (ccAssigned)
            continue;

        Creature* creature = AliveCreature(botAI->GetBot(), champion.guid);
        if (creature && (!accept || accept(creature)))
            return creature;
    }

    return nullptr;
}

bool FactionChampionsFocusBot(PlayerbotAI* botAI) { return botAI && !PlayerbotAI::IsHeal(botAI->GetBot()); }

bool FactionChampionsFearWindowActive(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return false;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live &&
           std::any_of(state->champions.begin(), state->champions.end(),
                       [](Champion const& champion) { return CastsFear(champion.entry); });
}

Unit* FactionChampionsCcTarget(PlayerbotAI* botAI, uint8& iconIndex)
{
    FactionChampionsState* state = Refresh(botAI);
    if (!state || !state->live)
        return nullptr;

    Player* bot = botAI->GetBot();
    auto const target = state->ccTargets.Raw().find(bot->GetGUID());
    auto const icon = state->ccIcons.find(bot->GetGUID());
    if (target == state->ccTargets.Raw().end() || icon == state->ccIcons.end())
        return nullptr;

    Creature* creature = AliveCreature(bot, target->second);
    if (!creature)
        return nullptr;

    iconIndex = icon->second;
    return creature;
}

bool FactionChampionsMarksPending(PlayerbotAI* botAI)
{
    FactionChampionsState* state = Refresh(botAI);
    Group* group = state ? botAI->GetBot()->GetGroup() : nullptr;
    if (!group)
        return false;

    MarkWork const work = PendingMarks(*state, botAI->GetBot(), group);
    return work.moveSkull || !work.clearIcons.empty();
}

void FactionChampionsApplyMarks(PlayerbotAI* botAI)
{
    FactionChampionsState* state = Refresh(botAI);
    Player* bot = botAI->GetBot();
    Group* group = state ? bot->GetGroup() : nullptr;
    if (!group)
        return;

    MarkWork const work = PendingMarks(*state, bot, group);
    if (work.moveSkull)
    {
        if (Creature* killTarget = AliveCreature(bot, state->killTarget.Get()))
        {
            // Group::SetTargetIcon also takes any CC icon off it
            EncounterHelpers::MarkTargetWithSkull(bot, killTarget);
            state->placedIcons[SKULL_ICON] = killTarget->GetGUID();
        }
    }

    for (uint8 icon : work.clearIcons)
    {
        EncounterHelpers::ClearTargetIcon(bot, icon);
        state->placedIcons[icon].Clear();
    }
}

bool FactionChampionsCcIconPending(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    uint8 icon = 0;
    if (Unit* target = FactionChampionsCcTarget(botAI, icon))
    {
        Group* group = bot->GetGroup();
        std::string const& rtiCc = botAI->GetAiObjectContext()->GetValue<std::string>("rti cc")->Get();
        return group && (rtiCc != ICON_NAMES[icon] || group->GetTargetIcon(icon) != target->GetGUID());
    }

    // FactionChampionsCcTarget just refreshed it
    FactionChampionsState* state = StateFor(bot);
    return state && state->live && state->savedRtiCc.count(bot->GetGUID());
}

void FactionChampionsApplyCcIcon(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    FactionChampionsState* state = StateFor(bot);
    if (!state)
        return;

    uint8 icon = 0;
    if (Unit* target = FactionChampionsCcTarget(botAI, icon))
    {
        // Only the first save counts, later ones would store this encounter's own icon
        state->savedRtiCc.try_emplace(bot->GetGUID(),
                                      botAI->GetAiObjectContext()->GetValue<std::string>("rti cc")->Get());
        EncounterHelpers::SetRtiCcTarget(botAI, ICON_NAMES[icon], target);
        state->placedIcons[icon] = target->GetGUID();
        return;
    }

    if (state->live)
        RestoreRtiCc(botAI, *state);
}

// No Refresh here: after the kill it would write a reset into the next encounter's trace
bool FactionChampionsRtiCcRestorePending(PlayerbotAI* botAI)
{
    FactionChampionsState* state = FindState(botAI->GetBot());
    return state && state->savedRtiCc.count(botAI->GetBot()->GetGUID()) &&
           !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions);
}

void FactionChampionsRestoreRtiCc(PlayerbotAI* botAI)
{
    if (FactionChampionsRtiCcRestorePending(botAI))
        RestoreRtiCc(botAI, *FindState(botAI->GetBot()));
}

bool FactionChampionsCounterspellDuty(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_MAGE)
        return false;

    Unit* killTarget = FactionChampionsKillTarget(botAI);
    return killTarget && CounterspellStopsHeal(killTarget) && CounterspellMage(bot, killTarget) == bot;
}

}
