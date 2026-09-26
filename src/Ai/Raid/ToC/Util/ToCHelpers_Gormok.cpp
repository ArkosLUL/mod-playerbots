#include "ToCHelpers_Gormok.h"

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "Creature.h"
#include "Map.h"
#include "Player.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellMgr.h"
#include "Timer.h"
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

}
