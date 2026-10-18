#include "ToCHelpers_FactionChampionsBall.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "EncounterHelpers.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsDefence.h"

namespace TrialOfTheCrusaderHelpers
{

namespace
{
// Same floor the grip stands and the Beasts spots keep to
constexpr float ARENA_FLOOR_RADIUS = 44.0f;

constexpr float PACK_TANK_REACH = 12.0f;

// Inside HealDistance 38.5 of the pack tank, with room for his own step about
constexpr float TANK_HEALER_DIST = 34.0f;
constexpr float TANK_HEALER_MIN = 20.0f;
constexpr float TANK_HEALER_SPREAD_DEG = 15.0f;
constexpr uint32 TANK_HEALERS_MAX = 2;

// Inside spell reach 28.5, which counts both combat reaches on top
constexpr std::array<float, 2> SIDE_RINGS = {26.0f, 20.0f};
constexpr float SIDE_STEP_DEG = 15.0f;
constexpr uint32 SIDE_STEPS = 5;
// Slots move with the kill target only past this, so stands don't creep every tick
constexpr float SIDE_REANCHOR = 6.0f;
// With the kill target still in the pack the pack's own direction means nothing
constexpr float SIDE_MIN_SPREAD = 5.0f;

constexpr uint32 OPENER_FROM_MS = 2500;
constexpr uint32 OPENER_UNTIL_MS = 8000;
constexpr uint32 OPENER_MIN_CHAMPIONS = 3;

struct OpenerSpell
{
    std::array<uint32, 6> ranks;  // highest first, 0 pads
};

// Frozen and feared both count in the champions' CC test, which pops the trinket
constexpr std::array<OpenerSpell, 5> OPENER_SPELLS = {{
    {{42917, 27088, 10230, 6131, 865, 122}},  // Frost Nova
    {{10890, 10888, 8124, 8122, 0, 0}},       // Psychic Scream
    {{5246, 0, 0, 0, 0, 0}},                  // Intimidating Shout
    {{49203, 0, 0, 0, 0, 0}},                 // Hungering Cold
    {{17928, 5484, 0, 0, 0, 0}},              // Howl of Terror, only once talented instant
}};

struct BallState
{
    RaidObs::ObsGuidMap<uint32> packTanks{"fc.packtank"};
    RaidObs::ObsGuidMap<uint32> opener{"fc.opener"};

    uint32 memoMs = 0;
    bool memoValid = false;
    uint32 pullStartMs = 0;
    bool openerDone = false;
    ObjectGuid sideAnchorTarget;
    Position sideAnchor;
    std::unordered_map<ObjectGuid, Position> stands;
};

RaidInstanceState<BallState> ballStates;

struct Vec
{
    float x;
    float y;
};

Vec Rotate(Vec v, float degrees)
{
    float const r = degrees * static_cast<float>(M_PI) / 180.0f;
    return {v.x * std::cos(r) - v.y * std::sin(r), v.x * std::sin(r) + v.y * std::cos(r)};
}

Vec Unit2(float x, float y)
{
    float const length = std::hypot(x, y);
    return length > 0.0f ? Vec{x / length, y / length} : Vec{1.0f, 0.0f};
}

bool OnFloor(Position const& spot) { return ARENA_CENTER.GetExactDist2d(spot) <= ARENA_FLOOR_RADIUS; }

Position Along(Position const& from, Vec dir, float distance)
{
    return Position(from.GetPositionX() + dir.x * distance, from.GetPositionY() + dir.y * distance,
                    from.GetPositionZ());
}

std::vector<Player*> GroupOnMap(Player* bot)
{
    std::vector<Player*> members;
    Map* map = bot->FindMap();
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsInWorld() && member->IsAlive() && member->FindMap() == map)
                members.push_back(member);
        }
    }
    else
        members.push_back(bot);

    std::sort(members.begin(), members.end(),
              [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });
    return members;
}

uint32 KnownOpenerRank(Player* player, OpenerSpell const& spell)
{
    for (uint32 rank : spell.ranks)
        if (rank && player->HasSpell(rank))
            return rank;

    return 0;
}

uint32 ReadyOpener(Player* player)
{
    for (OpenerSpell const& spell : OPENER_SPELLS)
    {
        uint32 const rank = KnownOpenerRank(player, spell);
        SpellInfo const* info = rank ? sSpellMgr->GetSpellInfo(rank) : nullptr;
        if (!info || player->HasSpellCooldown(rank) || info->CalcCastTime(player))
            continue;

        return rank;
    }

    return 0;
}

bool IsGripDk(Player* player)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(player);
    Position stand;
    return memberAI && FactionChampionsGripTarget(memberAI, stand) != nullptr;
}

void UpdateOpener(BallState& state, PlayerbotAI* botAI, std::vector<Player*> const& members)
{
    Player* bot = botAI->GetBot();
    Map* map = bot->FindMap();
    uint32 const liveMs = FactionChampionsLiveMs(botAI);
    bool const window = map && map->IsHeroic() && liveMs >= OPENER_FROM_MS && liveMs <= OPENER_UNTIL_MS;

    // The holder's spell going on cooldown is the cast
    std::vector<ObjectGuid> released;
    for (auto const& held : state.opener)
    {
        Player* holder = ObjectAccessor::GetPlayer(*bot, held.first);
        bool const cast = holder && holder->HasSpellCooldown(held.second);
        if (cast)
            state.openerDone = true;

        if (!window || cast || !holder || !holder->IsAlive())
            released.push_back(held.first);
    }

    for (ObjectGuid const& guid : released)
        state.opener.erase(guid);

    if (!window || state.openerDone || !state.opener.empty())
        return;

    for (Player* member : members)
    {
        if (!GET_PLAYERBOT_AI(member) || PlayerbotAI::IsTank(member) || PlayerbotAI::IsHeal(member) ||
            member->HasUnitState(UNIT_STATE_LOST_CONTROL) || IsGripDk(member))
            continue;

        if (uint32 const spellId = ReadyOpener(member))
        {
            state.opener.Set(member->GetGUID(), spellId);
            return;
        }
    }
}

void UpdateStands(BallState& state, PlayerbotAI* botAI, std::vector<Player*> const& members)
{
    state.stands.clear();

    Position anchor;
    Position centre;
    Unit* killTarget = FactionChampionsKillTarget(botAI);
    if (!killTarget || !FactionChampionsPack(botAI, anchor, centre))
        return;

    if (state.sideAnchorTarget != killTarget->GetGUID() ||
        state.sideAnchor.GetExactDist2d(killTarget) > SIDE_REANCHOR)
    {
        state.sideAnchorTarget = killTarget->GetGUID();
        state.sideAnchor = killTarget->GetPosition();
    }

    Position const& side = state.sideAnchor;
    Vec dir = Unit2(side.GetPositionX() - centre.GetPositionX(), side.GetPositionY() - centre.GetPositionY());
    if (side.GetExactDist2d(&centre) < SIDE_MIN_SPREAD)
        dir = Unit2(ARENA_CENTER.GetPositionX() - anchor.GetPositionX(),
                    ARENA_CENTER.GetPositionY() - anchor.GetPositionY());

    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    for (Player* member : members)
    {
        if (!GET_PLAYERBOT_AI(member) || PlayerbotAI::IsTank(member))
            continue;

        if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
    }

    // The melee champions weigh armour, so a plate healer is the one they leave alone at heal range
    std::vector<Player*> byArmour = healers;
    std::stable_sort(byArmour.begin(), byArmour.end(),
                     [](Player* a, Player* b) { return a->GetArmor() > b->GetArmor(); });
    uint32 const tankHealers = std::min<uint32>(TANK_HEALERS_MAX, (static_cast<uint32>(healers.size()) + 1) / 2);
    for (uint32 i = 0; i < tankHealers && i < byArmour.size(); ++i)
    {
        Vec const spoke = Rotate(dir, i % 2 ? -TANK_HEALER_SPREAD_DEG : TANK_HEALER_SPREAD_DEG);
        float distance = TANK_HEALER_DIST;
        Position spot = Along(anchor, spoke, distance);
        while (!OnFloor(spot) && distance > TANK_HEALER_MIN)
        {
            distance -= 2.0f;
            spot = Along(anchor, spoke, distance);
        }

        state.stands[byArmour[i]->GetGUID()] = spot;
    }

    std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
    std::vector<Position> slots;
    for (float ring : SIDE_RINGS)
    {
        for (uint32 step = 0; step <= SIDE_STEPS; ++step)
        {
            for (float sign : {1.0f, -1.0f})
            {
                if (!step && sign < 0.0f)
                    continue;

                Position const spot = Along(side, Rotate(dir, sign * SIDE_STEP_DEG * step), ring);
                bool const inHazard =
                    std::any_of(hazards.begin(), hazards.end(), [&spot](EncounterHelpers::HazardCircle const& hazard)
                                { return spot.GetExactDist2d(hazard.first) < hazard.second; });
                if (OnFloor(spot) && !inHazard)
                    slots.push_back(spot);
            }
        }
    }

    std::stable_sort(slots.begin(), slots.end(), [&centre](Position const& a, Position const& b)
                     { return centre.GetExactDist2d(a) > centre.GetExactDist2d(b); });

    size_t next = 0;
    for (std::vector<Player*> const* group : {&healers, &ranged})
    {
        for (Player* member : *group)
        {
            if (state.stands.count(member->GetGUID()) || state.opener.count(member->GetGUID()))
                continue;

            if (next >= slots.size())
                return;

            state.stands[member->GetGUID()] = slots[next++];
        }
    }
}

// Null unless live: a refresh after the kill would write into the next encounter's trace
BallState* Refresh(PlayerbotAI* botAI)
{
    if (!botAI || !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    Player* bot = botAI->GetBot();
    if (!bot->GetInstanceId())
        return nullptr;

    BallState& state = ballStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;

    // A new pull starts the opener over
    uint32 const pullStart = now - FactionChampionsLiveMs(botAI);
    if (getMSTimeDiff(state.pullStartMs, pullStart) > 1000 && getMSTimeDiff(pullStart, state.pullStartMs) > 1000)
    {
        state.pullStartMs = pullStart;
        state.openerDone = false;
    }

    std::vector<Player*> const members = GroupOnMap(bot);
    for (Player* member : members)
        if (FactionChampionsIsPackTank(member))
            state.packTanks.Set(member->GetGUID(), 1);

    UpdateOpener(state, botAI, members);
    UpdateStands(state, botAI, members);
    return &state;
}
}  // namespace

bool FactionChampionsPackTankHeld(PlayerbotAI* botAI, Position& anchor)
{
    Position centre;
    return botAI && FactionChampionsIsPackTank(botAI->GetBot()) && botAI->GetBot()->IsAlive() &&
           FactionChampionsPack(botAI, anchor, centre);
}

Unit* FactionChampionsPackTankTarget(PlayerbotAI* botAI)
{
    Position anchor;
    if (!FactionChampionsPackTankHeld(botAI, anchor))
        return nullptr;

    Player* bot = botAI->GetBot();
    Unit* killTarget = FactionChampionsKillTarget(botAI);
    Unit* nearest = nullptr;
    for (Unit* champion : FactionChampionsInCombat(botAI))
    {
        if (champion == killTarget || FactionChampionsIsPulled(botAI, champion) ||
            bot->GetExactDist2d(champion) > PACK_TANK_REACH)
            continue;

        if (!nearest || bot->GetExactDist2d(champion) < bot->GetExactDist2d(nearest))
            nearest = champion;
    }

    return nearest;
}

bool FactionChampionsBallStand(PlayerbotAI* botAI, Position& spot)
{
    BallState* state = Refresh(botAI);
    if (!state)
        return false;

    auto const stand = state->stands.find(botAI->GetBot()->GetGUID());
    if (stand == state->stands.end())
        return false;

    spot = stand->second;
    return true;
}

bool FactionChampionsOpener(PlayerbotAI* botAI, uint32& spellId, Position& spot)
{
    BallState* state = Refresh(botAI);
    if (!state)
        return false;

    auto const held = state->opener.find(botAI->GetBot()->GetGUID());
    Position anchor;
    if (held == state->opener.end() || !FactionChampionsPack(botAI, anchor, spot))
        return false;

    spellId = held->second;
    return true;
}

uint32 FactionChampionsPackAround(PlayerbotAI* botAI, float radius)
{
    Player* bot = botAI->GetBot();
    Unit* killTarget = FactionChampionsKillTarget(botAI);
    uint32 count = 0;
    for (Unit* champion : FactionChampionsInCombat(botAI))
        if (champion != killTarget && !FactionChampionsIsPulled(botAI, champion) &&
            bot->GetExactDist2d(champion) <= radius)
            ++count;

    return count;
}

}
