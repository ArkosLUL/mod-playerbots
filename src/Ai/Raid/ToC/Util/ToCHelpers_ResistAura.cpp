#include "ToCHelpers_ResistAura.h"

#include <array>
#include <tuple>
#include <vector>

#include "BossAuraTriggers.h"
#include "GenericBuffUtils.h"
#include "Group.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Anubarak.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "ToCHelpers_Staging.h"

namespace TrialOfTheCrusaderHelpers
{
namespace
{
constexpr size_t PALADIN_SCHOOL_COUNT = 3;

// Election order: with one free paladin, fire wins
constexpr std::array<ToCResistSchool, PALADIN_SCHOOL_COUNT> PALADIN_SCHOOLS = {
    ToCResistSchool::Fire, ToCResistSchool::Frost, ToCResistSchool::Shadow};

constexpr uint32 ELECT_INTERVAL_MS = 1000;

constexpr uint8 Bit(ToCResistSchool school) { return static_cast<uint8>(1u << static_cast<uint8>(school)); }

constexpr uint8 FIRE = Bit(ToCResistSchool::Fire);
constexpr uint8 FROST = Bit(ToCResistSchool::Frost);
constexpr uint8 SHADOW = Bit(ToCResistSchool::Shadow);
constexpr uint8 NATURE = Bit(ToCResistSchool::Nature);
constexpr uint8 PALADIN_MASK = FIRE | FROST | SHADOW;

constexpr size_t SchoolIndex(ToCResistSchool school) { return static_cast<size_t>(school) - 1; }

// GetAuraOfRankedSpell takes any rank of the chain
uint32 RankOneAura(ToCResistSchool school)
{
    switch (school)
    {
        case ToCResistSchool::Fire:
            return SPELL_FIRE_RESISTANCE_AURA_RANK_1;
        case ToCResistSchool::Frost:
            return SPELL_FROST_RESISTANCE_AURA_RANK_1;
        case ToCResistSchool::Shadow:
            return SPELL_SHADOW_RESISTANCE_AURA_RANK_1;
        default:
            return 0;
    }
}

char const* SchoolNote(ToCResistSchool school)
{
    switch (school)
    {
        case ToCResistSchool::Fire:
            return "fire";
        case ToCResistSchool::Frost:
            return "frost";
        case ToCResistSchool::Shadow:
            return "shadow";
        case ToCResistSchool::Nature:
            return "nature";
        default:
            return "none";
    }
}

// The paladin's own aura strategy. They're siblings, so a bot runs one at most.
enum class AuraStrategy : uint8
{
    None,
    Fire,
    Frost,
    Shadow,
    Speed,
    Aoe,
    Cast,
    Armor
};

struct AuraStrategyName
{
    char const* name;
    AuraStrategy strategy;
};

constexpr AuraStrategyName AURA_STRATEGIES[] = {
    {"rfire", AuraStrategy::Fire}, {"rfrost", AuraStrategy::Frost}, {"rshadow", AuraStrategy::Shadow},
    {"bspeed", AuraStrategy::Speed}, {"baoe", AuraStrategy::Aoe},    {"bcast", AuraStrategy::Cast},
    {"barmor", AuraStrategy::Armor},
};

AuraStrategy AuraStrategyOf(PlayerbotAI* memberAI)
{
    for (AuraStrategyName const& entry : AURA_STRATEGIES)
        if (memberAI->HasStrategy(entry.name, BOT_STATE_COMBAT))
            return entry.strategy;

    return AuraStrategy::None;
}

AuraStrategy ResistStrategyOf(ToCResistSchool school)
{
    switch (school)
    {
        case ToCResistSchool::Fire:
            return AuraStrategy::Fire;
        case ToCResistSchool::Frost:
            return AuraStrategy::Frost;
        case ToCResistSchool::Shadow:
            return AuraStrategy::Shadow;
        default:
            return AuraStrategy::None;
    }
}

// What the raid loses when this paladin swaps auras. Sanctified and Swift Retribution work under any
// of its auras, Improved Devotion only under Devotion.
uint8 AuraCost(AuraStrategy strategy)
{
    switch (strategy)
    {
        case AuraStrategy::Fire:
        case AuraStrategy::Frost:
        case AuraStrategy::Shadow:
        case AuraStrategy::Speed:
            return 0;
        case AuraStrategy::Cast:
            return 2;
        case AuraStrategy::Armor:
            return 3;
        default:
            return 1;
    }
}

struct Paladin
{
    Player* player = nullptr;
    AuraStrategy strategy = AuraStrategy::None;
    std::array<bool, PALADIN_SCHOOL_COUNT> knows{};
    bool mastery = false;
};

struct ResistAuraState
{
    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    uint8 wanted = 0;
    // Jaraxxus and the Twins: Aura Mastery is spent on Burning Inferno or a normal Light Vortex, and
    // doubles only its caster's own aura
    bool fireWithMastery = false;

    bool elected = false;
    uint32 electMs = 0;
    uint8 electedFor = 0;
    bool electedWithMastery = false;
    std::array<ObjectGuid, PALADIN_SCHOOL_COUNT> holder;
};

RaidInstanceState<ResistAuraState> resistStates;

uint8 SchoolsOfArrival(ToCStagingBoss arrival)
{
    switch (arrival)
    {
        case ToCStagingBoss::Gormok:
            return FIRE;
        case ToCStagingBoss::Worms:
            return FIRE | NATURE;
        case ToCStagingBoss::Icehowl:
            return FROST;
        case ToCStagingBoss::Jaraxxus:
            return FIRE | NATURE;  // Fel Lightning is fire + nature and resists by the lower one
        case ToCStagingBoss::Twins:
            return FIRE | SHADOW;
        case ToCStagingBoss::Anubarak:
            return FROST;  // nature joins from phase 2: scarab Mandibles, then Leeching Swarm
        case ToCStagingBoss::Champions:
            return FIRE;  // mostly physical, but the hunter, warlock and enhancement shaman burn
        default:
            return 0;
    }
}

// A heroic overlap has two beasts up at once, so their sets add up
uint8 SchoolsOfBeastsStage(uint32 mask)
{
    uint8 schools = 0;
    if (mask & BEASTS_STAGE_GORMOK)
        schools |= SchoolsOfArrival(ToCStagingBoss::Gormok);
    if (mask & BEASTS_STAGE_WORMS)
        schools |= SchoolsOfArrival(ToCStagingBoss::Worms);
    if (mask & BEASTS_STAGE_ICEHOWL)
        schools |= SchoolsOfArrival(ToCStagingBoss::Icehowl);

    return schools;
}

bool FireWantsMastery(PlayerbotAI* botAI)
{
    ToCEncounter const live = ToCLiveEncounter(botAI);
    if (live != ToCEncounter::None)
        return live == ToCEncounter::Jaraxxus || live == ToCEncounter::TwinValkyr;

    ToCStagingBoss const staged = GetToCStagedArrival(botAI);
    return staged == ToCStagingBoss::Jaraxxus || staged == ToCStagingBoss::Twins;
}

uint8 WantedSchools(PlayerbotAI* botAI, uint8 previous)
{
    switch (ToCLiveEncounter(botAI))
    {
        case ToCEncounter::NorthrendBeasts:
        {
            // The gate finds Icehowl by its own search, so for a moment the stage mask can trail it.
            // An empty set here would drop every holder for that tick.
            uint8 const schools = SchoolsOfBeastsStage(GetBeastsStageMask(botAI));
            return schools ? schools : previous;
        }
        case ToCEncounter::Jaraxxus:
            return SchoolsOfArrival(ToCStagingBoss::Jaraxxus);
        case ToCEncounter::FactionChampions:
            return SchoolsOfArrival(ToCStagingBoss::Champions);
        case ToCEncounter::TwinValkyr:
            return SchoolsOfArrival(ToCStagingBoss::Twins);
        case ToCEncounter::Anubarak:
        {
            AnubarakPhase const phase = GetAnubarakPhase(botAI);
            bool const nature = phase == AnubarakPhase::Submerged || phase == AnubarakPhase::Swarm;
            return static_cast<uint8>(SchoolsOfArrival(ToCStagingBoss::Anubarak) | (nature ? NATURE : 0));
        }
        default:
            return SchoolsOfArrival(GetToCStagedArrival(botAI));
    }
}

// Bot paladins only: a human can't be handed the aura. A human already running one on himself covers
// the raid for that school, so it lands in humanHeld instead.
std::vector<Paladin> GatherPaladins(Player* bot, uint8& humanHeld)
{
    std::vector<Paladin> paladins;
    uint32 const instanceId = bot->GetInstanceId();

    auto const consider = [&](Player* member)
    {
        if (!member || !member->IsInWorld() || !member->IsAlive() || member->getClass() != CLASS_PALADIN ||
            member->GetMapId() != TRIAL_OF_THE_CRUSADER_MAP_ID || member->GetInstanceId() != instanceId)
            return;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI)
        {
            for (ToCResistSchool school : PALADIN_SCHOOLS)
                if (member->GetAuraOfRankedSpell(RankOneAura(school), member->GetGUID()))
                    humanHeld |= Bit(school);

            return;
        }

        Paladin paladin;
        paladin.player = member;
        paladin.strategy = AuraStrategyOf(memberAI);
        for (ToCResistSchool school : PALADIN_SCHOOLS)
            paladin.knows[SchoolIndex(school)] =
                memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", ToCResistAuraName(school))->Get() != 0;
        paladin.mastery = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", "aura mastery")->Get() != 0;

        paladins.push_back(paladin);
    };

    Group* group = bot->GetGroup();
    if (!group)
    {
        consider(bot);
        return paladins;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        consider(ref->GetSource());

    return paladins;
}

// Another paladin left on the same aura keeps it up for the raid. Only those not already holding a
// school this round, since a holder gives its own aura up.
bool AuraDuplicated(std::vector<Paladin> const& paladins, std::vector<bool> const& used, size_t self)
{
    if (paladins[self].strategy == AuraStrategy::None)
        return false;

    for (size_t i = 0; i < paladins.size(); ++i)
        if (i != self && !used[i] && paladins[i].strategy == paladins[self].strategy)
            return true;

    return false;
}

void Elect(Player* bot, ResistAuraState& state, uint32 now)
{
    state.elected = true;
    state.electMs = now;
    state.electedFor = state.wanted;
    state.electedWithMastery = state.fireWithMastery;

    std::array<ObjectGuid, PALADIN_SCHOOL_COUNT> next{};
    if (!(state.wanted & PALADIN_MASK))
    {
        state.holder = next;
        return;
    }

    uint8 humanHeld = 0;
    std::vector<Paladin> const paladins = GatherPaladins(bot, humanHeld);
    std::vector<bool> used(paladins.size(), false);

    auto const open = [&](ToCResistSchool school)
    {
        return (state.wanted & Bit(school)) && !(humanHeld & Bit(school));
    };

    auto const wantsMastery = [&](ToCResistSchool school)
    { return state.fireWithMastery && school == ToCResistSchool::Fire; };
    // a kept fire holder without Aura Mastery gives way to a free paladin with it
    auto const masteryFree = [&](size_t keeper)
    {
        for (size_t i = 0; i < paladins.size(); ++i)
            if (i != keeper && paladins[i].mastery && paladins[i].knows[SchoolIndex(ToCResistSchool::Fire)])
                return true;

        return false;
    };

    // Kept holders first, so a dead fire holder can't pull the frost holder over to fire
    for (ToCResistSchool school : PALADIN_SCHOOLS)
    {
        size_t const index = SchoolIndex(school);
        if (!open(school) || state.holder[index].IsEmpty())
            continue;

        for (size_t i = 0; i < paladins.size(); ++i)
        {
            if (!used[i] && paladins[i].knows[index] && paladins[i].player->GetGUID() == state.holder[index] &&
                (!wantsMastery(school) || paladins[i].mastery || !masteryFree(i)))
            {
                next[index] = state.holder[index];
                used[i] = true;
                break;
            }
        }
    }

    for (ToCResistSchool school : PALADIN_SCHOOLS)
    {
        size_t const index = SchoolIndex(school);
        if (!open(school) || !next[index].IsEmpty())
            continue;

        auto const rank = [&](size_t i)
        {
            Paladin const& paladin = paladins[i];
            return std::make_tuple(wantsMastery(school) && !paladin.mastery ? 1 : 0,
                                   paladin.strategy == ResistStrategyOf(school) ? 0 : 1,
                                   AuraDuplicated(paladins, used, i) ? 0 : 1, AuraCost(paladin.strategy),
                                   paladin.player->GetGUID());
        };

        size_t best = paladins.size();
        for (size_t i = 0; i < paladins.size(); ++i)
        {
            if (used[i] || !paladins[i].knows[index])
                continue;

            if (best == paladins.size() || rank(i) < rank(best))
                best = i;
        }

        if (best == paladins.size())
            continue;

        next[index] = paladins[best].player->GetGUID();
        used[best] = true;
    }

    state.holder = next;
}

// One read per instance per ms: every paladin and hunter asks each tick, and the rules ask per action.
ResistAuraState const* ReadResist(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    ResistAuraState& state = resistStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;
    state.wanted = WantedSchools(botAI, state.wanted);
    state.fireWithMastery = (state.wanted & FIRE) && FireWantsMastery(botAI);

    if (!state.elected || state.wanted != state.electedFor || state.fireWithMastery != state.electedWithMastery ||
        getMSTimeDiff(state.electMs, now) >= ELECT_INTERVAL_MS)
        Elect(bot, state, now);

    return &state;
}

// tracked: a paladin or hunter on map 649, so toc.aura gets noted
ToCResistSchool ReadDuty(PlayerbotAI* botAI, bool& tracked)
{
    tracked = false;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || (bot->getClass() != CLASS_PALADIN && bot->getClass() != CLASS_HUNTER))
        return ToCResistSchool::None;

    ResistAuraState const* state = ReadResist(botAI);
    if (!state)
        return ToCResistSchool::None;

    tracked = true;
    if (bot->getClass() == CLASS_PALADIN)
    {
        for (ToCResistSchool school : PALADIN_SCHOOLS)
            if (state->holder[SchoolIndex(school)] == bot->GetGUID())
                return school;

        return ToCResistSchool::None;
    }

    // The generic pick hands the role on by mana, through Viper
    if ((state->wanted & NATURE) && ai::buff::GetNatureResistanceHunter(botAI, bot) == bot)
        return ToCResistSchool::Nature;

    return ToCResistSchool::None;
}
}  // namespace

ToCResistSchool GetToCResistDuty(PlayerbotAI* botAI)
{
    bool tracked = false;
    ToCResistSchool const duty = ReadDuty(botAI, tracked);
    if (tracked && RaidObs::Active())
        RaidObs::NoteDerived(botAI->GetBot(), "toc.aura", SchoolNote(duty));

    return duty;
}

char const* ToCResistAuraName(ToCResistSchool school)
{
    switch (school)
    {
        case ToCResistSchool::Fire:
            return "fire resistance aura";
        case ToCResistSchool::Frost:
            return "frost resistance aura";
        case ToCResistSchool::Shadow:
            return "shadow resistance aura";
        case ToCResistSchool::Nature:
            return "aspect of the wild";
        default:
            return nullptr;
    }
}

bool ToCResistAuraMissing(PlayerbotAI* botAI)
{
    ToCResistSchool const duty = GetToCResistDuty(botAI);
    if (duty == ToCResistSchool::None)
        return false;

    Player* bot = botAI->GetBot();
    if (!bot->IsAlive())
        return false;

    if (duty == ToCResistSchool::Nature)
        return !botAI->HasAura("aspect of the wild", bot);

    return !bot->GetAuraOfRankedSpell(RankOneAura(duty), bot->GetGUID());
}

bool IsToCResistPaladinHolding(PlayerbotAI* botAI)
{
    bool tracked = false;
    ToCResistSchool const duty = ReadDuty(botAI, tracked);
    return duty != ToCResistSchool::None && duty != ToCResistSchool::Nature;
}

bool IsToCResistHunterHolding(PlayerbotAI* botAI)
{
    bool tracked = false;
    return ReadDuty(botAI, tracked) == ToCResistSchool::Nature;
}

}
