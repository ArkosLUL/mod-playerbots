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
#include "ThreatManager.h"
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

// Champions this close to where they landed are the frontline's
constexpr float FRONT_REACH = 20.0f;

// Off to the side of the pack, not toward the pulled healer: a caster champion stops 18.5 yd short of the
// healer it picks, which keeps champion healers past their 40 yd heal reach of one pulled 70 yd out.
// At 28 yd cloth still scores under plate in melee for the melee champions.
constexpr float FRONT_HEALER_DIST = 28.0f;
constexpr float FRONT_HEALER_MIN = 20.0f;
constexpr std::array<float, 6> FRONT_HEALER_SPOKES_DEG = {60.0f, -60.0f, 45.0f, -45.0f, 75.0f, -75.0f};

// Inside spell reach 28.5, which counts both combat reaches on top
constexpr std::array<float, 2> SIDE_RINGS = {26.0f, 20.0f};
// With a healer left and the kill target still in the pack, ranged hit it from out here or not at all
constexpr std::array<float, 2> PACK_RINGS = {32.0f, 30.0f};
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
    RaidObs::ObsGuidMap<ObjectGuid> front{"fc.front"};
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

struct FrontCandidate
{
    Unit* champion;
    uint32 tier;
    bool melee;
    float fromAnchor;
};

// 0: no frontline DPS has threat on it yet, so the next recount can't pick one; 1: it is on a raider off
// the frontline; 2: the rest. Threat once seeded is reset every recount but never cleared, so one hit
// keeps a frontline DPS on its list for the fight.
uint32 FrontTier(Unit* champion, std::vector<Player*> const& front, std::vector<Player*> const& members)
{
    bool const tagged = std::any_of(front.begin(), front.end(),
                                    [champion](Player* member)
                                    {
                                        return !FactionChampionsIsPackTank(member) &&
                                               champion->GetThreatMgr().GetThreat(member) > 0.0f;
                                    });
    if (!tagged)
        return 0;

    Unit* victim = champion->GetVictim();
    Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
    bool const straying = victimPlayer && !FactionChampionsIsFrontline(victimPlayer) &&
                          std::find(members.begin(), members.end(), victimPlayer) != members.end();
    return straying ? 1 : 2;
}

void UpdateFrontline(BallState& state, PlayerbotAI* botAI, std::vector<Player*> const& members)
{
    std::vector<Player*> front;
    for (Player* member : members)
        if (FactionChampionsIsFrontline(member) && !member->HasUnitState(UNIT_STATE_LOST_CONTROL))
            front.push_back(member);

    Position anchor;
    Position centre;
    std::vector<FrontCandidate> candidates;
    if (!front.empty() && FactionChampionsPack(botAI, anchor, centre))
    {
        for (Unit* champion : FactionChampionsInCombat(botAI))
        {
            float const fromAnchor = champion->GetExactDist2d(&anchor);
            bool const untouchable = champion->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) &&
                                     champion->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC);
            if (fromAnchor > FRONT_REACH || untouchable || FactionChampionsIsPulled(botAI, champion) ||
                FactionChampionsCcProtected(botAI, champion))
                continue;

            candidates.push_back(
                {champion, FrontTier(champion, front, members), IsFactionChampionMelee(champion->GetEntry()),
                 fromAnchor});
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [](FrontCandidate const& a, FrontCandidate const& b)
                     {
                         if (a.tier != b.tier)
                             return a.tier < b.tier;

                         if (a.melee != b.melee)
                             return a.melee;

                         return a.fromAnchor < b.fromAnchor;
                     });

    constexpr int NONE = -1;
    std::vector<int> pick(front.size(), NONE);
    std::vector<uint32> load(candidates.size(), 0);
    for (size_t i = 0; i < front.size(); ++i)
    {
        auto const held = state.front.find(front[i]->GetGUID());
        if (held == state.front.end())
            continue;

        for (size_t c = 0; c < candidates.size(); ++c)
        {
            if (candidates[c].champion->GetGUID() == held->second)
            {
                pick[i] = static_cast<int>(c);
                ++load[c];
                break;
            }
        }
    }

    // A free member first, else one on a lower tier, the nearest either way
    auto const nearest = [&](size_t c, bool steal)
    {
        int best = NONE;
        for (size_t i = 0; i < front.size(); ++i)
        {
            bool const free = pick[i] == NONE;
            bool const lower = !free && candidates[pick[i]].tier > candidates[c].tier;
            if (steal ? !lower : !free)
                continue;

            if (best == NONE ||
                front[i]->GetExactDist2d(candidates[c].champion) < front[best]->GetExactDist2d(candidates[c].champion))
                best = static_cast<int>(i);
        }

        return best;
    };

    for (size_t c = 0; c < candidates.size(); ++c)
    {
        if (load[c])
            continue;

        int member = nearest(c, false);
        if (member == NONE)
            member = nearest(c, true);

        if (member == NONE)
            continue;

        if (pick[member] != NONE)
            --load[pick[member]];

        pick[member] = static_cast<int>(c);
        ++load[c];
    }

    bool placed = !candidates.empty();
    while (placed)
    {
        placed = false;
        for (size_t c = 0; c < candidates.size(); ++c)
        {
            int const member = nearest(c, false);
            if (member == NONE)
                break;

            pick[member] = static_cast<int>(c);
            ++load[c];
            placed = true;
        }
    }

    std::vector<ObjectGuid> gone;
    for (auto const& held : state.front)
        if (std::none_of(front.begin(), front.end(),
                         [&held](Player* member) { return member->GetGUID() == held.first; }))
            gone.push_back(held.first);

    for (ObjectGuid const& guid : gone)
        state.front.erase(guid);

    for (size_t i = 0; i < front.size(); ++i)
    {
        if (pick[i] == NONE)
            state.front.erase(front[i]->GetGUID());
        else
            state.front.Set(front[i]->GetGUID(), candidates[pick[i]].champion->GetGUID());
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

    bool const pulled = FactionChampionsIsPulled(botAI, killTarget);
    bool const waitOut = !pulled && FactionChampionsHealerLeft(botAI);
    Vec const toCentre = Unit2(ARENA_CENTER.GetPositionX() - anchor.GetPositionX(),
                               ARENA_CENTER.GetPositionY() - anchor.GetPositionY());

    Position side;
    Vec dir = toCentre;
    if (waitOut)
        side = centre;
    else
    {
        if (state.sideAnchorTarget != killTarget->GetGUID() ||
            state.sideAnchor.GetExactDist2d(killTarget) > SIDE_REANCHOR)
        {
            state.sideAnchorTarget = killTarget->GetGUID();
            state.sideAnchor = killTarget->GetPosition();
        }

        side = state.sideAnchor;
        if (side.GetExactDist2d(&centre) >= SIDE_MIN_SPREAD)
            dir = Unit2(side.GetPositionX() - centre.GetPositionX(), side.GetPositionY() - centre.GetPositionY());
    }

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

    // The melee champions weigh armour, so plate heals the frontline and the least armour goes with the
    // kill squad
    std::vector<Player*> byArmour = healers;
    std::stable_sort(byArmour.begin(), byArmour.end(),
                     [](Player* a, Player* b) { return a->GetArmor() > b->GetArmor(); });
    Vec const toPulled = pulled ? Unit2(killTarget->GetPositionX() - anchor.GetPositionX(),
                                        killTarget->GetPositionY() - anchor.GetPositionY())
                                : toCentre;
    size_t const frontHealers = byArmour.size() > 1 ? byArmour.size() - 1 : byArmour.size();
    for (size_t i = 0; i < frontHealers; ++i)
    {
        Vec const spoke = Rotate(toPulled, FRONT_HEALER_SPOKES_DEG[i % FRONT_HEALER_SPOKES_DEG.size()]);
        float distance = FRONT_HEALER_DIST;
        Position spot = Along(anchor, spoke, distance);
        while (!OnFloor(spot) && distance > FRONT_HEALER_MIN)
        {
            distance -= 2.0f;
            spot = Along(anchor, spoke, distance);
        }

        state.stands[byArmour[i]->GetGUID()] = spot;
    }

    std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
    std::vector<Position> slots;
    for (float ring : waitOut ? PACK_RINGS : SIDE_RINGS)
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
    UpdateOpener(state, botAI, members);
    UpdateFrontline(state, botAI, members);
    UpdateStands(state, botAI, members);
    return &state;
}
}  // namespace

bool FactionChampionsFrontlineHeld(PlayerbotAI* botAI, Position& anchor)
{
    Position centre;
    return botAI && FactionChampionsIsFrontline(botAI->GetBot()) && botAI->GetBot()->IsAlive() &&
           FactionChampionsPack(botAI, anchor, centre);
}

Unit* FactionChampionsFrontlineTarget(PlayerbotAI* botAI)
{
    Position anchor;
    BallState* state = FactionChampionsFrontlineHeld(botAI, anchor) ? Refresh(botAI) : nullptr;
    if (!state)
        return nullptr;

    Player* bot = botAI->GetBot();
    auto const held = state->front.find(bot->GetGUID());
    if (held == state->front.end())
        return nullptr;

    Creature* champion = ObjectAccessor::GetCreature(*bot, held->second);
    return champion && champion->IsAlive() ? champion : nullptr;
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
