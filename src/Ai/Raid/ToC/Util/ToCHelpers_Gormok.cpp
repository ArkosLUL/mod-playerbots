#include "ToCHelpers_Gormok.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "Creature.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "Unit.h"

namespace TrialOfTheCrusaderHelpers
{

namespace
{
// Whole arena from anywhere on its floor
constexpr float SNOBOLD_SEARCH_RADIUS = 200.0f;

// Rank order: a lower value is killed first
enum class SnoboldRider : uint8
{
    Heal,
    Ranged,
    Melee,
    Tank
};

// Every snobold in the arena, seated, in hand or riding. Guids, so one that despawns inside the window
// just stops resolving.
struct SnoboldCache
{
    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    std::vector<ObjectGuid> guids;
};

RaidInstanceState<SnoboldCache> snoboldCaches;

std::vector<std::pair<Creature*, Player*>> GetRidingSnobolds(Player* bot)
{
    std::vector<std::pair<Creature*, Player*>> riding;
    Map* map = bot->FindMap();
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return riding;

    SnoboldCache& cache = snoboldCaches.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (!cache.memoValid || cache.memoMs != now)
    {
        cache.memoMs = now;
        cache.memoValid = true;
        cache.guids.clear();

        std::list<Creature*> snobolds;
        bot->GetCreatureListWithEntryInGrid(snobolds, static_cast<uint32>(ToCNpcs::NPC_SNOBOLD_VASSAL),
                                            SNOBOLD_SEARCH_RADIUS);
        for (Creature const* snobold : snobolds)
            cache.guids.push_back(snobold->GetGUID());
    }

    for (ObjectGuid const& guid : cache.guids)
    {
        Creature* snobold = map->GetCreature(guid);
        if (!snobold || !snobold->IsAlive())
            continue;

        // Seated and in-hand ones ride Gormok
        Unit* base = snobold->GetVehicleBase();
        Player* rider = base ? base->ToPlayer() : nullptr;
        if (rider && rider->IsAlive())
            riding.emplace_back(snobold, rider);
    }

    return riding;
}

SnoboldRider ClassifyRider(Player* rider)
{
    if (IsBeastsTank(rider))
        return SnoboldRider::Tank;
    if (PlayerbotAI::IsHeal(rider))
        return SnoboldRider::Heal;
    return PlayerbotAI::IsRanged(rider) ? SnoboldRider::Ranged : SnoboldRider::Melee;
}

char const* RiderName(SnoboldRider rider)
{
    switch (rider)
    {
        case SnoboldRider::Heal:
            return "heal";
        case SnoboldRider::Ranged:
            return "ranged";
        case SnoboldRider::Melee:
            return "melee";
        default:
            return "tank";
    }
}

bool IsCarryingSnobold(Player* bot)
{
    // The aura lands 1.5 s after the seat, so the seat itself covers the gap
    if (bot->HasAura(SPELL_SNOBOLLED))
        return true;

    for (auto const& [snobold, rider] : GetRidingSnobolds(bot))
        if (rider == bot)
            return true;

    return false;
}

bool IsRangedDpsOrHealer(PlayerbotAI* botAI, Player* bot)
{
    return !IsBeastsTank(bot) && (botAI->IsHeal(bot) || botAI->IsRanged(bot));
}

// Same reach as the snobold search: bombs land anywhere on the arena floor
constexpr float FIRE_BOMB_SEARCH_RADIUS = 200.0f;

constexpr std::array<NorthrendBeast, 4> ALL_BEASTS = {
    NorthrendBeast::Gormok, NorthrendBeast::Acidmaw, NorthrendBeast::Dreadscale, NorthrendBeast::Icehowl};

struct FireBomb
{
    ObjectGuid guid;
    // Age at which 66317 hits
    uint32 impactAgeMs = 0;
    // Ages a bomb that isn't a TempSummon
    uint32 firstSeenMs = 0;
    bool noted = false;
};

struct FireBombCache
{
    uint32 scanMs = 0;
    bool scanned = false;  // 0 is a real getMSTime value
    std::vector<FireBomb> bombs;
};

RaidInstanceState<FireBombCache> fireBombCaches;

uint32 FireBombAge(Creature* bomb, FireBomb const& entry, uint32 now)
{
    if (TempSummon* summon = bomb->ToTempSummon())
    {
        uint32 const left = summon->GetTimer();
        return left < FIRE_BOMB_LIFETIME_MS ? FIRE_BOMB_LIFETIME_MS - left : 0;
    }

    return getMSTimeDiff(entry.firstSeenMs, now);
}

// Measured from the snobold that threw it, else from Gormok, whose seat it threw from
uint32 ModelFireBombImpact(Map* map, Creature* bomb)
{
    Unit* source = nullptr;
    if (TempSummon* summon = bomb->ToTempSummon())
    {
        ObjectGuid const summoner = summon->GetSummonerGUID();
        if (!summoner.IsEmpty())
            source = ObjectAccessor::GetUnit(*bomb, summoner);
    }

    if (!source)
    {
        if (InstanceScript* instance = bomb->GetInstanceScript())
        {
            ObjectGuid const gormok = instance->GetGuidData(TOC_DATA_GORMOK);
            if (!gormok.IsEmpty())
                source = map->GetCreature(gormok);
        }
    }

    if (!source)
        return FIRE_BOMB_FALLBACK_IMPACT_MS;

    float const flightMs = 1000.0f * source->GetExactDist2d(bomb) / FIRE_BOMB_MISSILE_SPEED;
    return FIRE_BOMB_CAST_MS + static_cast<uint32>(std::lround(flightMs)) + FIRE_BOMB_IMPACT_SLACK_MS;
}

bool IsEngagedBeastVictim(PlayerbotAI* botAI, Player* bot)
{
    return std::any_of(ALL_BEASTS.begin(), ALL_BEASTS.end(),
                       [botAI, bot](NorthrendBeast beast)
                       {
                           Unit* engaged = GetEngagedBeast(botAI, beast);
                           return engaged && engaged->GetVictim() == bot;
                       });
}

bool IsYoungFireBomb(Creature* bomb, FireBomb const& entry, uint32 now)
{
    return FireBombAge(bomb, entry, now) < entry.impactAgeMs;
}

// Null off a ToC instance. Drops bombs that stopped resolving, and rescans the grid only while one
// can still be thrown or is still in flight.
FireBombCache* ReadFireBombs(PlayerbotAI* botAI, Map*& map)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    FireBombCache& cache = fireBombCaches.For(bot->GetInstanceId());
    uint32 const now = getMSTime();

    bool anyYoung = false;
    std::vector<FireBomb>& bombs = cache.bombs;
    for (auto itr = bombs.begin(); itr != bombs.end();)
    {
        Creature* bomb = map->GetCreature(itr->guid);
        if (!bomb)
        {
            itr = bombs.erase(itr);
            continue;
        }

        anyYoung = anyYoung || IsYoungFireBomb(bomb, *itr, now);
        ++itr;
    }

    bool const scanDue = !cache.scanned || getMSTimeDiff(cache.scanMs, now) >= FIRE_BOMB_SCAN_MS;
    if (scanDue && (anyYoung || GetEngagedBeast(botAI, NorthrendBeast::Gormok)))
    {
        cache.scanMs = now;
        cache.scanned = true;

        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, NPC_FIRE_BOMB, FIRE_BOMB_SEARCH_RADIUS);
        for (Creature* bomb : found)
        {
            ObjectGuid const guid = bomb->GetGUID();
            bool const known = std::any_of(bombs.begin(), bombs.end(),
                                           [&guid](FireBomb const& entry) { return entry.guid == guid; });
            if (known)
                continue;

            FireBomb entry;
            entry.guid = guid;
            entry.firstSeenMs = now;
            entry.impactAgeMs = ModelFireBombImpact(map, bomb);
            bombs.push_back(entry);
        }
    }

    // Once per bomb, as soon as a trace can take it, with what's left of the flight as its ttl
    for (FireBomb& entry : bombs)
    {
        if (entry.noted)
            continue;

        Creature* bomb = map->GetCreature(entry.guid);
        if (!bomb)
            continue;

        uint32 const age = FireBombAge(bomb, entry, now);
        if (age >= entry.impactAgeMs)
        {
            entry.noted = true;
            continue;
        }

        if (RaidObs::Active())
        {
            RaidObs::NoteHazardCircle(map, SPELL_FIRE_BOMB_IMPACT, bomb->GetPosition(), FIRE_BOMB_IMPACT_RADIUS,
                                      entry.impactAgeMs - age);
            entry.noted = true;
        }
    }

    return &cache;
}
}  // namespace

uint32 GetGormokImpaleStacks(Unit* unit)
{
    return unit ? unit->GetAuraCount(sSpellMgr->GetSpellIdForDifficulty(SPELL_IMPALE, unit)) : 0;
}

Unit* GetGormokSwapTauntTarget(PlayerbotAI* botAI)
{
    if (GetBeastsTankDuty(botAI) != BeastsTankDuty::GormokSwap)
        return nullptr;

    Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok);
    Player* holder = GetBeastsDutyHolder(botAI, BeastsTankDuty::Gormok);
    if (!gormok || !holder || gormok->GetVictim() != holder)
        return nullptr;

    Player* bot = botAI->GetBot();
    uint32 const holderStacks = GetGormokImpaleStacks(holder);
    uint32 const ownStacks = GetGormokImpaleStacks(bot);
    bool const go = holderStacks >= GORMOK_IMPALE_SWAP_STACKS && ownStacks == 0;

    if (RaidObs::Active())
    {
        std::string note;
        if (go)
            note = "go v=" + std::to_string(holderStacks);
        else if (holderStacks >= GORMOK_IMPALE_SWAP_STACKS)
            note = "wait mine=" + std::to_string(ownStacks);
        else
            note = "wait v=" + std::to_string(holderStacks);
        RaidObs::NoteDerived(bot, "nb.swap", note);
    }

    return go ? gormok : nullptr;
}

bool GormokTankNeedsDefensive(PlayerbotAI* botAI)
{
    if (GetBeastsTankDuty(botAI) != BeastsTankDuty::Gormok)
        return false;

    Player* bot = botAI->GetBot();
    Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok);
    if (!gormok || gormok->GetVictim() != bot)
        return false;

    uint32 const stacks = GetGormokImpaleStacks(bot);
    if (stacks >= GORMOK_IMPALE_DEFENSIVE_STACKS)
        return true;

    return stacks >= GORMOK_IMPALE_SWAP_STACKS && !GetBeastsDutyHolder(botAI, BeastsTankDuty::GormokSwap);
}

Unit* GetGormokSnoboldPick(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (IsBeastsTank(bot) || botAI->IsHeal(bot))
        return nullptr;

    // Melee and tank riders are already next to the melee, so ranged stay on Gormok while he's up
    bool const melee = !botAI->IsRanged(bot);
    bool const gormokEngaged = GetEngagedBeast(botAI, NorthrendBeast::Gormok) != nullptr;

    Creature* pick = nullptr;
    Player* pickRider = nullptr;
    SnoboldRider pickRank = SnoboldRider::Tank;
    for (auto const& [snobold, rider] : GetRidingSnobolds(bot))
    {
        SnoboldRider const rank = ClassifyRider(rider);
        bool const meleeOnly = rank == SnoboldRider::Melee || rank == SnoboldRider::Tank;
        if (meleeOnly && gormokEngaged && !melee)
            continue;

        if (!pick || rank < pickRank || (rank == pickRank && snobold->GetGUID() < pick->GetGUID()))
        {
            pick = snobold;
            pickRider = rider;
            pickRank = rank;
        }
    }

    if (RaidObs::Active())
    {
        std::string const note =
            pick ? RaidObs::DescribeAssignment(pickRider->GetGUID()) + " " + RiderName(pickRank) : "none";
        RaidObs::NoteDerived(bot, "nb.snobold", note);
    }

    return pick;
}

Unit* GetGormokForSnoboldCarrier(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsRangedDpsOrHealer(botAI, bot))
        return nullptr;

    Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok);
    return gormok && IsCarryingSnobold(bot) ? gormok : nullptr;
}

Unit* GetGormokStompThreat(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsRangedDpsOrHealer(botAI, bot))
        return nullptr;

    Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok);
    if (!gormok || bot->GetExactDist2d(gormok) >= GORMOK_STOMP_TRIGGER)
        return nullptr;

    // A carrier belongs in his melee until its snobold dies
    return IsCarryingSnobold(bot) ? nullptr : gormok;
}

Unit* GetGormokForStompCaster(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsRangedDpsOrHealer(botAI, bot))
        return nullptr;

    Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok);
    return gormok && !IsCarryingSnobold(bot) ? gormok : nullptr;
}

bool GetFireBombDodgeAnchor(PlayerbotAI* botAI, Position& anchor)
{
    Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok);
    if (!gormok)
        return false;

    Player* bot = botAI->GetBot();
    bool const meleeDps = !IsBeastsTank(bot) && !botAI->IsHeal(bot) && !botAI->IsRanged(bot);
    if (meleeDps || IsCarryingSnobold(bot))
    {
        anchor = gormok->GetPosition();
        return true;
    }

    // Healers and casters stay in reach of whoever he's hitting
    Unit* victim = gormok->GetVictim();
    if (!victim)
        return false;

    anchor = victim->GetPosition();
    return true;
}

void GetFireBombs(PlayerbotAI* botAI, std::vector<Position>& young, std::vector<Position>& old)
{
    young.clear();
    old.clear();

    Map* map = nullptr;
    FireBombCache* cache = ReadFireBombs(botAI, map);
    if (!cache)
        return;

    uint32 const now = getMSTime();
    for (FireBomb const& entry : cache->bombs)
    {
        Creature* bomb = map->GetCreature(entry.guid);
        if (!bomb)
            continue;

        if (IsYoungFireBomb(bomb, entry, now))
            young.push_back(bomb->GetPosition());
        else
            old.push_back(bomb->GetPosition());
    }
}

bool IsInFireBombImpact(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->IsAlive())
        return false;

    std::vector<Position> young;
    std::vector<Position> old;
    GetFireBombs(botAI, young, old);

    bool const inside = std::any_of(young.begin(), young.end(), [bot](Position const& bomb)
                                    { return bot->GetExactDist2d(bomb) <= FIRE_BOMB_TRIGGER; });
    bool const dodging = inside && !IsEngagedBeastVictim(botAI, bot);

    // nb.bomb is change-only and the dodge only runs inside a trigger, so its end is written here.
    // Without it a bomb whose dodge repeats the last one's branch writes nothing.
    if (!dodging && RaidObs::Active())
        RaidObs::NoteDerived(bot, "nb.bomb", "clear");

    return dodging;
}

}
