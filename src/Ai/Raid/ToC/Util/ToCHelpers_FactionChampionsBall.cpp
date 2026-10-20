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

// Champions this close to the pack centre are the frontline's
constexpr float FRONT_REACH = 20.0f;

// Off to the side of the pack, not toward the pulled healer: a caster champion stops 18.5 yd short of the
// healer it picks, which keeps champion healers past their 40 yd heal reach of one pulled 70 yd out.
// At 28 yd cloth still scores under plate in melee for the melee champions.
constexpr float FRONT_HEALER_DIST = 28.0f;
constexpr float FRONT_HEALER_MIN = 20.0f;
// a held heal stand stays while this close to the pack centre, inside heal reach of the far side
constexpr float FRONT_HEALER_KEEP = 34.0f;
constexpr std::array<float, 6> FRONT_HEALER_SPOKES_DEG = {60.0f, -60.0f, 45.0f, -45.0f, 75.0f, -75.0f};

// Inside spell reach 28.5, which counts both combat reaches on top
constexpr std::array<float, 2> SIDE_RINGS = {26.0f, 20.0f};
// With a healer left and the kill target still in the pack, ranged hit it from out here or not at all
constexpr std::array<float, 2> PACK_RINGS = {32.0f, 30.0f};
constexpr float SIDE_STEP_DEG = 15.0f;
constexpr uint32 SIDE_STEPS = 5;
// Layouts move only past this, so stands don't creep every tick
constexpr float SIDE_REANCHOR = 6.0f;
// a held stand this close to a slot of the current layout keeps it
constexpr float SAME_SLOT = 0.5f;
constexpr float SIDE_MIN_SPREAD = 5.0f;
// Slack inside Cyclone's reach for the healer's drift
constexpr float CYCLONE_STAND_MARGIN = 2.0f;
// The Cyclone stand's rings round the kill target. 30 yd still sits inside Wrath's 33 with Nature's Reach.
constexpr std::array<float, 3> CYCLONE_RINGS = {20.0f, 26.0f, 30.0f};

// a shorter walk isn't worth Nitro Boosts' 3 min cooldown
constexpr float NITRO_MIN_WALK = 15.0f;
constexpr float CHASED_REACH = 8.0f;
constexpr float RUN_DIST = 25.0f;
constexpr float RUN_ARRIVE = 2.0f;

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

struct Vec
{
    float x;
    float y;
};

struct Layout
{
    bool valid = false;
    ObjectGuid key;
    bool mode = false;
    Position at;
    Vec dir{1.0f, 0.0f};
};

struct Run
{
    uint32 sinceMs = 0;
    Position spot;
};

// Kept until either unit moves SIDE_REANCHOR or the spot stops being clear
struct CycloneStand
{
    ObjectGuid kill;
    ObjectGuid healer;
    Position killAt;
    Position healerAt;
    Position spot;
};

struct BallState
{
    RaidObs::ObsGuidMap<ObjectGuid> front{"fc.front"};
    RaidObs::ObsGuidMap<uint32> opener{"fc.opener"};
    RaidObs::ObsGuidMap<ObjectGuid> cyclone{"fc.cyclone"};  // druid -> the pack healer it covers

    uint32 memoMs = 0;
    bool memoValid = false;
    uint32 pullStartMs = 0;
    bool openerDone = false;
    // Each layout moves only when its reference does by SIDE_REANCHOR or its key changes, and a stand
    // stays with its bot while its slot is still in the layout, so nobody walks for a small shift
    Layout side;
    Layout heal;
    std::unordered_map<ObjectGuid, Position> stands;
    std::unordered_map<ObjectGuid, Run> runs;  // chased bot -> its boosted run
    std::unordered_map<ObjectGuid, size_t> healSpokes;
    std::unordered_map<ObjectGuid, CycloneStand> cycloneStands;
};

RaidInstanceState<BallState> ballStates;

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
    float fromCentre;
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
            float const fromCentre = champion->GetExactDist2d(&centre);
            bool const untouchable = champion->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) &&
                                     champion->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC);
            if (fromCentre > FRONT_REACH || untouchable || FactionChampionsIsPulled(botAI, champion) ||
                FactionChampionsCcProtected(botAI, champion))
                continue;

            candidates.push_back(
                {champion, FrontTier(champion, front, members), IsFactionChampionMelee(champion->GetEntry()),
                 fromCentre});
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [](FrontCandidate const& a, FrontCandidate const& b)
                     {
                         if (a.tier != b.tier)
                             return a.tier < b.tier;

                         if (a.melee != b.melee)
                             return a.melee;

                         return a.fromCentre < b.fromCentre;
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

// Moves the layout only when its key or mode changes or its reference walked past SIDE_REANCHOR. True when
// it moved, so the stands dealt on it go too.
bool Reanchor(Layout& layout, ObjectGuid key, bool mode, Position const& at, Vec dir)
{
    if (layout.valid && layout.key == key && layout.mode == mode && layout.at.GetExactDist2d(&at) <= SIDE_REANCHOR)
        return false;

    layout.valid = true;
    layout.key = key;
    layout.mode = mode;
    layout.at = at;
    layout.dir = dir;
    return true;
}

bool Clear(Position const& spot, std::vector<EncounterHelpers::HazardCircle> const& hazards)
{
    return OnFloor(spot) && std::none_of(hazards.begin(), hazards.end(),
                                         [&spot](EncounterHelpers::HazardCircle const& hazard)
                                         { return spot.GetExactDist2d(hazard.first) < hazard.second; });
}

// On the rings round the kill target, so the druid still hits it. First a spot that stays in Cyclone reach
// of the pack healer while it drifts up to SIDE_REANCHOR, then one in reach, each farthest from the pack
// then nearest the druid; with none in reach, nearest the healer. False when no ring spot is clear.
bool CycloneSpot(BallState& state, Player* druid, Unit* kill, Unit* healer, Position const& centre,
                 std::vector<EncounterHelpers::HazardCircle> const& hazards, Position& spot)
{
    auto const held = state.cycloneStands.find(druid->GetGUID());
    if (held != state.cycloneStands.end() && held->second.kill == kill->GetGUID() &&
        held->second.healer == healer->GetGUID() && kill->GetExactDist2d(&held->second.killAt) <= SIDE_REANCHOR &&
        healer->GetExactDist2d(&held->second.healerAt) <= SIDE_REANCHOR && Clear(held->second.spot, hazards))
    {
        spot = held->second.spot;
        return true;
    }

    SpellInfo const* info = sSpellMgr->GetSpellInfo(SPELL_CYCLONE);
    float const reach = (info ? info->GetMaxRange(false, druid) : 0.0f) + druid->GetCombatReach() +
                        healer->GetCombatReach() - CYCLONE_STAND_MARGIN;
    Vec const out = Unit2(kill->GetPositionX() - centre.GetPositionX(), kill->GetPositionY() - centre.GetPositionY());
    uint32 const steps = static_cast<uint32>(360.0f / SIDE_STEP_DEG);

    // 0 holds through the healer's drift, 1 reaches it now, 2 doesn't
    auto const tier = [&](Position const& candidate)
    {
        float const toHealer = healer->GetExactDist2d(&candidate);
        return toHealer <= reach - SIDE_REANCHOR ? 0 : (toHealer <= reach ? 1 : 2);
    };

    bool found = false;
    int bestTier = 2;
    Position best;
    for (float ring : CYCLONE_RINGS)
    {
        for (uint32 step = 0; step < steps; ++step)
        {
            Position const candidate = Along(kill->GetPosition(), Rotate(out, SIDE_STEP_DEG * step), ring);
            if (!Clear(candidate, hazards))
                continue;

            int const candidateTier = tier(candidate);
            bool better = !found || candidateTier < bestTier;
            if (found && candidateTier == bestTier)
            {
                if (candidateTier == 2)
                    better = healer->GetExactDist2d(&candidate) < healer->GetExactDist2d(&best);
                else if (centre.GetExactDist2d(&candidate) != centre.GetExactDist2d(&best))
                    better = centre.GetExactDist2d(&candidate) > centre.GetExactDist2d(&best);
                else
                    better = druid->GetExactDist2d(&candidate) < druid->GetExactDist2d(&best);
            }

            if (better)
            {
                found = true;
                bestTier = candidateTier;
                best = candidate;
            }
        }
    }

    if (!found)
        return false;

    state.cycloneStands[druid->GetGUID()] = {kill->GetGUID(), healer->GetGUID(), kill->GetPosition(),
                                             healer->GetPosition(), best};
    spot = best;
    return true;
}

void UpdateStands(BallState& state, PlayerbotAI* botAI, std::vector<Player*> const& members)
{
    Position anchor;
    Position centre;
    Unit* killTarget = FactionChampionsKillTarget(botAI);
    if (!killTarget || !FactionChampionsPack(botAI, anchor, centre))
    {
        state.stands.clear();
        state.side.valid = false;
        state.heal.valid = false;
        std::vector<ObjectGuid> druids;
        for (auto const& held : state.cyclone.Raw())
            druids.push_back(held.first);

        for (ObjectGuid const& guid : druids)
            state.cyclone.erase(guid);

        state.cycloneStands.clear();
        return;
    }

    // latched rather than IsPulled, which flips each time a champion steps near the kill target
    bool const out = FactionChampionsKillTargetOut(botAI);
    bool const waitOut = !out && FactionChampionsHealerLeft(botAI);
    Vec const toArena = Unit2(ARENA_CENTER.GetPositionX() - centre.GetPositionX(),
                              ARENA_CENTER.GetPositionY() - centre.GetPositionY());

    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    for (Player* member : members)
    {
        if (!GET_PLAYERBOT_AI(member) || PlayerbotAI::IsTank(member) || state.opener.count(member->GetGUID()))
            continue;

        if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
    }

    std::vector<ObjectGuid> gone;
    for (auto const& held : state.stands)
    {
        auto const is = [&held](Player* member) { return member->GetGUID() == held.first; };
        if (std::none_of(healers.begin(), healers.end(), is) && std::none_of(ranged.begin(), ranged.end(), is))
            gone.push_back(held.first);
    }

    for (ObjectGuid const& guid : gone)
        state.stands.erase(guid);

    // The gripped champion healer can't kill anyone in the kill squad, so every healer heals the frontline.
    // Each keeps its spoke while it lives, so a death moves nobody else.
    std::vector<ObjectGuid> lost;
    for (auto const& held : state.healSpokes)
        if (std::none_of(healers.begin(), healers.end(),
                         [&held](Player* member) { return member->GetGUID() == held.first; }))
            lost.push_back(held.first);

    for (ObjectGuid const& guid : lost)
        state.healSpokes.erase(guid);

    Vec const toPulled =
        out ? Unit2(killTarget->GetPositionX() - centre.GetPositionX(), killTarget->GetPositionY() - centre.GetPositionY())
            : toArena;
    std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
    bool const sameHealLayout = state.heal.valid && state.heal.key == killTarget->GetGUID() && state.heal.mode == out;
    Reanchor(state.heal, killTarget->GetGUID(), out, centre, toPulled);
    std::vector<Position> spokes;
    for (float degrees : FRONT_HEALER_SPOKES_DEG)
    {
        Vec const spoke = Rotate(state.heal.dir, degrees);
        float distance = FRONT_HEALER_DIST;
        Position spot = Along(state.heal.at, spoke, distance);
        while (!OnFloor(spot) && distance > FRONT_HEALER_MIN)
        {
            distance -= 2.0f;
            spot = Along(state.heal.at, spoke, distance);
        }

        spokes.push_back(spot);
    }

    std::vector<bool> spokeTaken(spokes.size(), false);
    for (auto const& held : state.healSpokes)
        spokeTaken[held.second] = true;

    for (Player* healer : healers)
    {
        // the pack drifts 25-50 yd a pull, a fresh spoke every 6 yd of it keeps healers walking. Same kill
        // target and still in heal reach of the pack: the stand stays.
        auto const stand = state.stands.find(healer->GetGUID());
        if (sameHealLayout && stand != state.stands.end() && state.healSpokes.count(healer->GetGUID()))
        {
            float const fromCentre = centre.GetExactDist2d(&stand->second);
            if (fromCentre >= FRONT_HEALER_MIN && fromCentre <= FRONT_HEALER_KEEP && Clear(stand->second, hazards))
                continue;
        }

        auto held = state.healSpokes.find(healer->GetGUID());
        if (held == state.healSpokes.end())
        {
            size_t best = spokes.size();
            for (size_t s = 0; s < spokes.size(); ++s)
                if (!spokeTaken[s] &&
                    (best == spokes.size() || healer->GetExactDist2d(&spokes[s]) < healer->GetExactDist2d(&spokes[best])))
                    best = s;

            if (best == spokes.size())
            {
                state.stands.erase(healer->GetGUID());
                continue;
            }

            spokeTaken[best] = true;
            held = state.healSpokes.emplace(healer->GetGUID(), best).first;
        }

        state.stands[healer->GetGUID()] = spokes[held->second];
    }

    // With the kill target still in the pack the pack's own direction means nothing
    Position const sideAt = waitOut ? centre : killTarget->GetPosition();
    Vec sideDir = toArena;
    if (!waitOut && sideAt.GetExactDist2d(&centre) >= SIDE_MIN_SPREAD)
        sideDir = Unit2(sideAt.GetPositionX() - centre.GetPositionX(), sideAt.GetPositionY() - centre.GetPositionY());

    Unit* packHealer = out ? FactionChampionsBlobHealer(botAI) : nullptr;
    std::vector<Player*> squad;
    std::vector<ObjectGuid> covering;
    for (Player* member : ranged)
    {
        Position spot;
        if (packHealer && FactionChampionsCycloneBot(member) &&
            CycloneSpot(state, member, killTarget, packHealer, centre, hazards, spot))
        {
            state.stands[member->GetGUID()] = spot;
            state.cyclone.Set(member->GetGUID(), packHealer->GetGUID());
            covering.push_back(member->GetGUID());
        }
        else
            squad.push_back(member);
    }

    std::vector<ObjectGuid> released;
    for (auto const& held : state.cyclone.Raw())
        if (std::find(covering.begin(), covering.end(), held.first) == covering.end())
            released.push_back(held.first);

    for (ObjectGuid const& guid : released)
    {
        state.cyclone.erase(guid);
        state.cycloneStands.erase(guid);
    }

    if (Reanchor(state.side, killTarget->GetGUID(), waitOut, sideAt, sideDir))
        for (Player* member : squad)
            state.stands.erase(member->GetGUID());
    std::vector<Position> slots;
    for (float ring : waitOut ? PACK_RINGS : SIDE_RINGS)
    {
        for (uint32 step = 0; step <= SIDE_STEPS; ++step)
        {
            for (float sign : {1.0f, -1.0f})
            {
                if (!step && sign < 0.0f)
                    continue;

                Position const spot = Along(state.side.at, Rotate(state.side.dir, sign * SIDE_STEP_DEG * step), ring);
                if (Clear(spot, hazards))
                    slots.push_back(spot);
            }
        }
    }

    std::vector<bool> taken(slots.size(), false);
    auto const slotOf = [&slots](Position const& spot)
    {
        for (size_t s = 0; s < slots.size(); ++s)
            if (slots[s].GetExactDist2d(&spot) < SAME_SLOT)
                return s;

        return slots.size();
    };

    // a bot keeps its slot while the layout still has it, free ones take the nearest left
    std::vector<Player*> unplaced;
    for (Player* member : squad)
    {
        auto const held = state.stands.find(member->GetGUID());
        size_t const s = held != state.stands.end() ? slotOf(held->second) : slots.size();
        if (s < slots.size() && !taken[s])
            taken[s] = true;
        else
        {
            state.stands.erase(member->GetGUID());
            unplaced.push_back(member);
        }
    }

    for (Player* member : unplaced)
    {
        size_t best = slots.size();
        for (size_t s = 0; s < slots.size(); ++s)
            if (!taken[s] && (best == slots.size() ||
                              member->GetExactDist2d(&slots[s]) < member->GetExactDist2d(&slots[best])))
                best = s;

        if (best == slots.size())
            continue;

        taken[best] = true;
        state.stands[member->GetGUID()] = slots[best];
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

bool FactionChampionsFrontlineHeld(PlayerbotAI* botAI, Position& centre)
{
    Position anchor;
    return botAI && FactionChampionsIsFrontline(botAI->GetBot()) && botAI->GetBot()->IsAlive() &&
           FactionChampionsPack(botAI, anchor, centre);
}

Unit* FactionChampionsFrontlineTarget(PlayerbotAI* botAI)
{
    Position centre;
    BallState* state = FactionChampionsFrontlineHeld(botAI, centre) ? Refresh(botAI) : nullptr;
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

    // a CC duty may walk it into spell range, its slot waits for it
    ObjectGuid const guid = botAI->GetBot()->GetGUID();
    auto const stand = state->stands.find(guid);
    if (stand == state->stands.end() || FactionChampionsHasCcDuty(botAI, guid))
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

void FactionChampionsBoostWalk(Player* bot, float distance)
{
    if (distance >= NITRO_MIN_WALK && !bot->HasAura(EncounterHelpers::NITRO_BOOSTS_SPEED_SPELL))
        EncounterHelpers::UseNitroBoosts(bot);
}

bool FactionChampionsChased(PlayerbotAI* botAI, Position& spot)
{
    BallState* state = Refresh(botAI);
    if (!state)
        return false;

    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();
    auto const run = state->runs.find(bot->GetGUID());
    if (run != state->runs.end())
    {
        if (getMSTimeDiff(run->second.sinceMs, now) < EncounterHelpers::NITRO_BOOSTS_DURATION_MS &&
            bot->GetExactDist2d(&run->second.spot) > RUN_ARRIVE)
        {
            spot = run->second.spot;
            return true;
        }

        state->runs.erase(run);
    }

    if (PlayerbotAI::IsTank(bot) || FactionChampionsIsFrontline(bot) ||
        (!PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot)) || bot->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
        bot->HasAuraType(SPELL_AURA_MOD_ROOT) || FactionChampionsHasCcDuty(botAI, bot->GetGUID()) ||
        !EncounterHelpers::NitroBoostsReady(bot))
        return false;

    Unit* chaser = nullptr;
    for (Unit* champion : FactionChampionsInCombat(botAI))
        if (IsFactionChampionMelee(champion->GetEntry()) && champion->GetVictim() == bot &&
            champion->GetExactDist2d(bot) <= CHASED_REACH)
            chaser = champion;

    if (!chaser)
        return false;

    Vec const away = Unit2(bot->GetPositionX() - chaser->GetPositionX(), bot->GetPositionY() - chaser->GetPositionY());
    for (float turn : {0.0f, 45.0f, -45.0f, 90.0f, -90.0f})
    {
        Position const target = Along(bot->GetPosition(), Rotate(away, turn), RUN_DIST);
        if (!OnFloor(target))
            continue;

        state->runs[bot->GetGUID()] = {now, target};
        spot = target;
        return true;
    }

    return false;
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
