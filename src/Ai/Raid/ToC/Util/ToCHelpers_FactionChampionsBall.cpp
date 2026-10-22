#include "ToCHelpers_FactionChampionsBall.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
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

// A held champion's pick weighs distance past 5 yd, so a hurt cloth healer ~25 yd off can outscore its plate
// holder. A stand already held keeps until one walks inside the lower figure.
constexpr float HEAL_FROM_HELD = 30.0f;
constexpr float HEAL_KEEP_FROM_HELD = 26.0f;
// heal range 40, less a margin for the target's own steps
constexpr float HEAL_STAND_REACH = 36.0f;
// a stand held while what it heals drifts this much past the reach
constexpr float HEAL_KEEP_SLACK = 6.0f;
// champion crowd AoE needs 3+ of the raid within 8-10 yd of the caster
constexpr float HEAL_SPACING = 6.0f;
constexpr float HEAL_LATTICE_STEP = 4.0f;
// ranged hold no threat on a held champion, this only keeps them out of its crowd AoE
constexpr float RANGED_FROM_HELD = 15.0f;

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

// Nitro Boosts are for running from a focus: this many champions on the bot, or about to be
constexpr uint32 NITRO_FOCUS = 3;
// lower, the boots no longer outrun the hits
constexpr float NITRO_MIN_HEALTH_PCT = 30.0f;
constexpr float RUN_DIST = 25.0f;
constexpr float RUN_ARRIVE = 2.0f;
// the focus on a bot fearing to be picked next, read off the champions around it
constexpr float NEXT_FOCUS_RADIUS = 30.0f;

constexpr uint32 OPENER_MIN_CHAMPIONS = 3;
// inside the AoE by this much, so a step by the champions doesn't leave them out
constexpr float OPENER_MARGIN = 1.0f;

// farther off, the champion is on its way to another pick rather than chasing its holder
constexpr float HOLD_FOLLOW = 10.0f;

struct OpenerSpell
{
    std::array<uint32, 6> ranks;  // highest first, 0 pads
};

// Feared and frozen both pass the champions' CC test, which pops the trinket
constexpr std::array<OpenerSpell, 5> OPENER_SPELLS = {{
    {{5246, 0, 0, 0, 0, 0}},                  // Intimidating Shout
    {{10890, 10888, 8124, 8122, 0, 0}},       // Psychic Scream
    {{49203, 0, 0, 0, 0, 0}},                 // Hungering Cold
    {{17928, 5484, 0, 0, 0, 0}},              // Howl of Terror, only once talented instant
    {{42917, 27088, 10230, 6131, 865, 122}},  // Frost Nova
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
    RaidObs::ObsGuidMap<uint32> opener{"fc.opener"};
    RaidObs::ObsGuidMap<ObjectGuid> cyclone{"fc.cyclone"};  // druid -> the pack healer it covers

    uint32 memoMs = 0;
    bool memoValid = false;
    // Each layout moves only when its reference does by SIDE_REANCHOR or its key changes, and a stand
    // stays with its bot while its slot is still in the layout, so nobody walks for a small shift
    Layout side;
    std::unordered_map<ObjectGuid, Position> stands;
    std::unordered_map<ObjectGuid, Run> runs;  // chased bot -> its boosted run
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

// An instant the bot knows, off cooldown and affordable
SpellInfo const* ReadyInstant(Player* player, uint32 rank)
{
    SpellInfo const* info = rank ? sSpellMgr->GetSpellInfo(rank) : nullptr;
    if (!info || player->HasSpellCooldown(rank) || info->CalcCastTime(player) ||
        info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return nullptr;

    return info->CalcPowerCost(player, info->GetSchoolMask()) <=
                   static_cast<int32>(player->GetPower(Powers(info->PowerType)))
               ? info
               : nullptr;
}

float SpellRadius(SpellInfo const* info, Unit* caster)
{
    float radius = 0.0f;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (info->Effects[i].IsEffect())
            radius = std::max(radius, info->Effects[i].CalcRadius(caster));

    return radius;
}

bool IsGripDk(Player* player)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(player);
    Position stand;
    return memberAI && FactionChampionsGripTarget(memberAI, stand) != nullptr;
}

// Champions with their trinket up inside the spell's reach of the caster. None while one the grip guard holds
// is anywhere near it: a fear or freeze ends a pull where it catches the champion.
std::vector<Unit*> TrinketsInReach(PlayerbotAI* botAI, Player* caster, SpellInfo const* info,
                                   std::vector<Unit*> const& champions)
{
    std::vector<Unit*> inReach;
    float const radius = SpellRadius(info, caster);
    for (Unit* champion : champions)
    {
        float const distance = caster->GetExactDist2d(champion);
        if (distance <= radius + OPENER_MARGIN && FactionChampionsGripGuarded(botAI, champion))
            return {};

        if (distance <= radius - OPENER_MARGIN && FactionChampionsTrinketReady(champion) &&
            !champion->IsImmunedToSpell(info))
            inReach.push_back(champion);
    }

    return inReach;
}

// Heroic: the bot that reaches the most champions with their trinket up, three or more, with a ready AoE
// fear or Frost Nova, casts it where it stands. A trinket used comes back in 2 min, and so does this.
void UpdateOpener(BallState& state, PlayerbotAI* botAI, std::vector<Player*> const& members)
{
    Map* map = botAI->GetBot()->FindMap();
    std::vector<Unit*> champions;
    if (map && map->IsHeroic())
        champions = FactionChampionsInCombat(botAI);

    ObjectGuid const holding = state.opener.empty() ? ObjectGuid::Empty : state.opener.begin()->first;
    Player* best = nullptr;
    uint32 bestSpell = 0;
    size_t bestCount = 0;
    for (Player* member : members)
    {
        if (champions.size() < OPENER_MIN_CHAMPIONS || !GET_PLAYERBOT_AI(member) || PlayerbotAI::IsHeal(member) ||
            member->HasUnitState(UNIT_STATE_LOST_CONTROL) || member->HasUnitFlag(UNIT_FLAG_SILENCED) ||
            IsGripDk(member))
            continue;

        for (OpenerSpell const& spell : OPENER_SPELLS)
        {
            uint32 const rank = KnownOpenerRank(member, spell);
            SpellInfo const* info = ReadyInstant(member, rank);
            if (!info)
                continue;

            size_t const count = TrinketsInReach(botAI, member, info, champions).size();
            // the holder keeps a tie, so the duty doesn't hop between two bots
            bool const better = count > bestCount || (count == bestCount && member->GetGUID() == holding);
            if (count >= OPENER_MIN_CHAMPIONS && better)
            {
                best = member;
                bestSpell = rank;
                bestCount = count;
            }
        }
    }

    if (!holding.IsEmpty() && (!best || best->GetGUID() != holding))
        state.opener.erase(holding);

    if (best)
        state.opener.Set(best->GetGUID(), bestSpell);
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

float NearestOf(std::vector<Position> const& points, Position const& spot)
{
    float nearest = std::numeric_limits<float>::max();
    for (Position const& point : points)
        nearest = std::min(nearest, point.GetExactDist2d(&spot));

    return nearest;
}

// Healers stand 30+ yd from every held champion and holder spot, 6+ yd apart, and within heal reach of
// every holder and the kill target where they can: the fewest out of reach, then the shortest walk. With
// no spot that far from the held ones, the farthest. A stand is kept while it stays clear, 26+ yd from them
// and what it heals stays within reach plus 6 yd.
void UpdateHealStands(BallState& state, PlayerbotAI* botAI, std::vector<Player*> const& healers,
                      std::vector<Player*> const& members, Unit* killTarget,
                      std::vector<EncounterHelpers::HazardCircle> const& hazards)
{
    std::vector<Position> const held = FactionChampionsHeldPoints(botAI);
    std::vector<Position> heals;
    for (Player* member : members)
        if (FactionChampionsIsHolder(member))
            heals.push_back(member->GetPosition());

    heals.push_back(killTarget->GetPosition());
    auto const outOfReach = [&heals](Position const& spot, float reach)
    {
        return static_cast<uint32>(std::count_if(heals.begin(), heals.end(), [&spot, reach](Position const& point)
                                                 { return point.GetExactDist2d(&spot) > reach; }));
    };

    std::vector<Position> placed;
    auto const spaced = [&placed](Position const& spot) { return NearestOf(placed, spot) >= HEAL_SPACING; };

    int32 const steps = static_cast<int32>(ARENA_FLOOR_RADIUS / HEAL_LATTICE_STEP);
    for (Player* healer : healers)
    {
        auto const stand = state.stands.find(healer->GetGUID());
        if (stand != state.stands.end() && Clear(stand->second, hazards) &&
            NearestOf(held, stand->second) >= HEAL_KEEP_FROM_HELD && spaced(stand->second) &&
            !outOfReach(stand->second, HEAL_STAND_REACH + HEAL_KEEP_SLACK))
        {
            placed.push_back(stand->second);
            continue;
        }

        bool found = false;
        bool bestFar = false;
        uint32 bestMisses = 0;
        float bestKey = 0.0f;
        Position best;
        for (int32 i = -steps; i <= steps; ++i)
        {
            for (int32 j = -steps; j <= steps; ++j)
            {
                Position const spot(ARENA_CENTER.GetPositionX() + i * HEAL_LATTICE_STEP,
                                    ARENA_CENTER.GetPositionY() + j * HEAL_LATTICE_STEP, healer->GetPositionZ());
                if (!Clear(spot, hazards) || !spaced(spot))
                    continue;

                float const fromHeld = NearestOf(held, spot);
                bool const far = fromHeld >= HEAL_FROM_HELD;
                uint32 const misses = outOfReach(spot, HEAL_STAND_REACH);
                // far ones by reach, then walk; with none far yet, the farthest
                float const key = far ? healer->GetExactDist2d(&spot) : -fromHeld;
                bool better = !found || (far && !bestFar);
                if (found && far == bestFar)
                    better = far ? (misses < bestMisses || (misses == bestMisses && key < bestKey)) : key < bestKey;

                if (better)
                {
                    found = true;
                    bestFar = far;
                    bestMisses = misses;
                    bestKey = key;
                    best = spot;
                }
            }
        }

        if (!found)
        {
            state.stands.erase(healer->GetGUID());
            continue;
        }

        state.stands[healer->GetGUID()] = best;
        placed.push_back(best);
    }
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
        if (!GET_PLAYERBOT_AI(member) || PlayerbotAI::IsTank(member))
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

    std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
    UpdateHealStands(state, botAI, healers, members, killTarget, hazards);

    std::vector<EncounterHelpers::HazardCircle> rangedHazards = hazards;
    for (Position const& point : FactionChampionsHeldPoints(botAI))
        rangedHazards.emplace_back(point, RANGED_FROM_HELD);

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
            CycloneSpot(state, member, killTarget, packHealer, centre, rangedHazards, spot))
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
                if (Clear(spot, rangedHazards))
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

    std::vector<Player*> const members = GroupOnMap(bot);
    UpdateOpener(state, botAI, members);
    UpdateStands(state, botAI, members);
    return &state;
}
}  // namespace

bool FactionChampionsHoldWalk(PlayerbotAI* botAI, Position& spot)
{
    FactionChampionsHold const hold = FactionChampionsHoldOf(botAI);
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!hold.placed || hold.champion->GetVictim() != bot || hold.champion->GetExactDist2d(bot) > HOLD_FOLLOW)
        return false;

    spot = hold.spot;
    return true;
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

Unit* FactionChampionsOpener(PlayerbotAI* botAI, uint32& spellId)
{
    BallState* state = Refresh(botAI);
    if (!state)
        return nullptr;

    Player* bot = botAI->GetBot();
    auto const held = state->opener.find(bot->GetGUID());
    SpellInfo const* info = held != state->opener.end() ? sSpellMgr->GetSpellInfo(held->second) : nullptr;
    if (!info)
        return nullptr;

    spellId = held->second;
    if (!info->NeedsExplicitUnitTarget())
        return bot;

    // Intimidating Shout stuns its target and fears the rest round the warrior
    Unit* nearest = nullptr;
    for (Unit* champion : TrinketsInReach(botAI, bot, info, FactionChampionsInCombat(botAI)))
        if (!nearest || bot->GetExactDist2d(champion) < bot->GetExactDist2d(nearest))
            nearest = champion;

    return nearest;
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

    if (PlayerbotAI::IsTank(bot) || FactionChampionsIsHolder(bot) || bot->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
        bot->HasAuraType(SPELL_AURA_MOD_ROOT) || FactionChampionsHasCcDuty(botAI, bot->GetGUID()) ||
        bot->GetHealthPct() < NITRO_MIN_HEALTH_PCT || !EncounterHelpers::NitroBoostsReady(bot))
        return false;

    uint32 on = 0;
    float sumX = 0.0f;
    float sumY = 0.0f;
    float nearX = 0.0f;
    float nearY = 0.0f;
    uint32 near = 0;
    for (Unit* champion : FactionChampionsInCombat(botAI))
    {
        if (champion->HasUnitState(UNIT_STATE_LOST_CONTROL))
            continue;

        if (champion->GetVictim() == bot)
        {
            ++on;
            sumX += champion->GetPositionX();
            sumY += champion->GetPositionY();
        }

        if (bot->GetExactDist2d(champion) <= NEXT_FOCUS_RADIUS)
        {
            ++near;
            nearX += champion->GetPositionX();
            nearY += champion->GetPositionY();
        }
    }

    bool const next = FactionChampionsNextFocus(botAI) == bot->GetGUID();
    if (on < NITRO_FOCUS && !next)
        return false;

    // about to be picked with nobody on it yet: away from the champions round it
    uint32 const from = on ? on : near;
    if (!from)
        return false;

    float const fromX = (on ? sumX : nearX) / from;
    float const fromY = (on ? sumY : nearY) / from;
    Vec const away = Unit2(bot->GetPositionX() - fromX, bot->GetPositionY() - fromY);
    for (float turn : {0.0f, 45.0f, -45.0f, 90.0f, -90.0f})
    {
        Position const target = Along(bot->GetPosition(), Rotate(away, turn), RUN_DIST);
        if (!OnFloor(target))
            continue;

        state->runs[bot->GetGUID()] = {now, target};
        RaidObs::Note(bot, "fc.nitro", on >= NITRO_FOCUS ? "focus " + std::to_string(on) : "next");
        spot = target;
        return true;
    }

    return false;
}

}
