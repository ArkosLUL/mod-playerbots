#include "ToCHelpers_FactionChampionsBall.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "ToCHelpers_Staging.h"

namespace TrialOfTheCrusaderHelpers
{

namespace
{
// Same floor the grip stands and the Beasts spots keep to
constexpr float ARENA_FLOOR_RADIUS = 44.0f;

constexpr float SIDE_STEP_DEG = 15.0f;
// a fresh Cyclone stand prefers one still in reach after the pack healer drifts this far
constexpr float SIDE_REANCHOR = 6.0f;
// Slack inside Cyclone's reach for the healer's drift
constexpr float CYCLONE_STAND_MARGIN = 2.0f;
// The Cyclone stand's rings round the kill target. 30 yd still sits inside Wrath's 33 with Nature's Reach.
constexpr std::array<float, 3> CYCLONE_RINGS = {20.0f, 26.0f, 30.0f};

// a healer steps off melee champions this close, this far, then stands again for a while so one that
// follows doesn't turn it into a kite
constexpr float STEP_OFF_TRIGGER = 8.0f;
constexpr float STEP_OFF_DIST = 10.0f;
constexpr uint32 STEP_OFF_MS = 2500;
constexpr uint32 STEP_OFF_REST_MS = 3000;

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

struct OpenerSpell
{
    std::array<uint32, 6> ranks;  // highest first, 0 pads
};

constexpr OpenerSpell INTIMIDATING_SHOUT{{5246, 0, 0, 0, 0, 0}};
constexpr OpenerSpell PSYCHIC_SCREAM{{10890, 10888, 8124, 8122, 0, 0}};
constexpr OpenerSpell HUNGERING_COLD{{49203, 0, 0, 0, 0, 0}};
// only once talented instant
constexpr OpenerSpell HOWL_OF_TERROR{{17928, 5484, 0, 0, 0, 0}};
constexpr OpenerSpell FROST_NOVA{{42917, 27088, 10230, 6131, 865, 122}};
constexpr OpenerSpell DRAGONS_BREATH{{42950, 42949, 33043, 33042, 33041, 31661}};
constexpr OpenerSpell SHADOWFURY{{47847, 47846, 30414, 30413, 30283, 0}};

// Feared and frozen both pass the champions' CC test, which pops the trinket
constexpr std::array<OpenerSpell, 5> OPENER_SPELLS = {INTIMIDATING_SHOUT, PSYCHIC_SCREAM, HUNGERING_COLD,
                                                      HOWL_OF_TERROR, FROST_NOVA};

// A healer or ranged bot runs or CCs its way out once a melee champion on it is this close
constexpr float ESCAPE_MELEE_REACH = 8.0f;
// the circle has to put this much more ground between the bot and the melee on it
constexpr float ESCAPE_MIN_GAIN = 12.0f;
constexpr float BLINK_DIST = 20.0f;
// an AoE's search counts both ends' size on top of its radius
constexpr float ESCAPE_AOE_SLACK = 2.0f;
constexpr uint32 SPELL_BLINK = 1953;
constexpr uint32 SPELL_DEMONIC_CIRCLE_TELEPORT = 48020;
// on the warlock while its circle is within the teleport's 40 yd
constexpr uint32 SPELL_DEMONIC_CIRCLE_IN_RANGE = 62388;
// the circle lasts 6 min, a long wait between pulls gets a fresh one before it runs out
constexpr int32 CIRCLE_REFRESH_MS = 60000;
// two melee champions on a cloth caster kill it in 3-8 s, well before a third arrives
constexpr uint32 NITRO_MELEE_FOCUS = 2;
// Blink flees every melee champion this close, and a landing nearer one than this isn't worth the cooldown
constexpr float BLINK_FLEE_RADIUS = 15.0f;
constexpr float BLINK_MIN_CLEAR = 12.0f;
constexpr uint32 SPELL_DISENGAGE = 781;
constexpr float DISENGAGE_REACH = 5.0f;
constexpr float DISENGAGE_DIST = 15.0f;

// cloth beats a plate DPS in melee for the re-pick up to ~27 yd, plus the 110% switch margin
constexpr float KITE_CLEAR = 30.0f;
// closer than this a free chaser keeps hitting while it follows, so running gains nothing
constexpr float KITE_CONTACT = 6.0f;
constexpr float KITE_STEP = 15.0f;
constexpr uint32 KITE_STEP_MS = 2500;
// a chaser dragged onto a healer would just re-pick the healer
constexpr float KITE_HEALER_GAP = 10.0f;
constexpr float KITE_HOLD = 45.0f;

struct Vec
{
    float x;
    float y;
};

struct Run
{
    uint32 sinceMs = 0;
    Position spot;
};

// Kept while the healer and the kill target stay in reach and the spot stays clear
struct CycloneStand
{
    ObjectGuid kill;
    ObjectGuid healer;
    Position spot;
};

struct BallState
{
    RaidObs::ObsGuidMap<uint32> opener{"fc.opener"};
    RaidObs::ObsGuidMap<ObjectGuid> cyclone{"fc.cyclone"};  // druid -> the pack healer it covers

    uint32 memoMs = 0;
    bool memoValid = false;
    std::unordered_map<ObjectGuid, Run> runs;   // chased bot -> its boosted run
    std::unordered_map<ObjectGuid, Run> steps;  // healer -> its step off the melee
    std::unordered_map<ObjectGuid, Run> kites;  // ranged dps -> its current hop
    std::unordered_map<ObjectGuid, uint32> stepEndMs;
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

bool Clear(Position const& spot, std::vector<EncounterHelpers::HazardCircle> const& hazards)
{
    return OnFloor(spot) && std::none_of(hazards.begin(), hazards.end(),
                                         [&spot](EncounterHelpers::HazardCircle const& hazard)
                                         { return spot.GetExactDist2d(hazard.first) < hazard.second; });
}

// On the rings round the kill target, so the druid still hits it. First a spot that stays in Cyclone reach
// of the pack healer while it drifts up to SIDE_REANCHOR, then one in reach, each farthest from the pack
// then nearest the druid; with none in reach, nearest the healer. False when no ring spot is clear. A spot
// already held stays while both are still in reach of it, so the druid doesn't walk for every drift.
bool CycloneSpot(BallState& state, Player* druid, Unit* kill, Unit* healer, Position const& centre,
                 std::vector<EncounterHelpers::HazardCircle> const& hazards, Position& spot)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(SPELL_CYCLONE);
    float const reach = (info ? info->GetMaxRange(false, druid) : 0.0f) + druid->GetCombatReach() +
                        healer->GetCombatReach() - CYCLONE_STAND_MARGIN;
    auto const held = state.cycloneStands.find(druid->GetGUID());
    if (held != state.cycloneStands.end() && held->second.kill == kill->GetGUID() &&
        held->second.healer == healer->GetGUID() && healer->GetExactDist2d(&held->second.spot) <= reach &&
        kill->GetExactDist2d(&held->second.spot) <= CYCLONE_RINGS.back() + SIDE_REANCHOR &&
        Clear(held->second.spot, hazards))
    {
        spot = held->second.spot;
        return true;
    }

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

    state.cycloneStands[druid->GetGUID()] = {kill->GetGUID(), healer->GetGUID(), best};
    spot = best;
    return true;
}

// Healers and ranged stand where they are. Only the Cyclone druid gets a stand, once the kill target has been
// out and a champion healer is left in the pack.
void UpdateCycloneStands(BallState& state, PlayerbotAI* botAI, std::vector<Player*> const& members)
{
    Position anchor;
    Position centre;
    Unit* killTarget = FactionChampionsKillTarget(botAI);
    // latched rather than IsPulled, which flips each time a champion steps near the kill target
    Unit* packHealer = killTarget && FactionChampionsPack(botAI, anchor, centre) && FactionChampionsKillTargetOut(botAI)
                           ? FactionChampionsBlobHealer(botAI)
                           : nullptr;
    std::vector<ObjectGuid> covering;
    if (packHealer)
    {
        std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
        for (Player* member : members)
        {
            Position spot;
            if (GET_PLAYERBOT_AI(member) && !PlayerbotAI::IsTank(member) && !PlayerbotAI::IsHeal(member) &&
                PlayerbotAI::IsRanged(member) && FactionChampionsCycloneBot(member) &&
                CycloneSpot(state, member, killTarget, packHealer, centre, hazards, spot))
            {
                state.cyclone.Set(member->GetGUID(), packHealer->GetGUID());
                covering.push_back(member->GetGUID());
            }
        }
    }

    auto const covers = [&covering](ObjectGuid guid)
    { return std::find(covering.begin(), covering.end(), guid) != covering.end(); };
    std::vector<ObjectGuid> released;
    for (auto const& held : state.cyclone.Raw())
        if (!covers(held.first))
            released.push_back(held.first);

    for (auto const& held : state.cycloneStands)
        if (!covers(held.first))
            released.push_back(held.first);

    for (ObjectGuid const& guid : released)
    {
        state.cyclone.erase(guid);
        state.cycloneStands.erase(guid);
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
    UpdateCycloneStands(state, botAI, members);
    return &state;
}
}  // namespace

bool FactionChampionsCycloneStand(PlayerbotAI* botAI, Position& spot)
{
    BallState* state = Refresh(botAI);
    if (!state)
        return false;

    // a CC duty may walk it into spell range, its stand waits for it
    ObjectGuid const guid = botAI->GetBot()->GetGUID();
    auto const stand = state->cycloneStands.find(guid);
    if (stand == state->cycloneStands.end() || FactionChampionsHasCcDuty(botAI, guid))
        return false;

    spot = stand->second.spot;
    return true;
}

bool FactionChampionsStaysPut(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    return bot && Refresh(botAI) && !PlayerbotAI::IsTank(bot) &&
           (PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsRanged(bot)) && !FactionChampionsIsHolder(bot);
}

bool FactionChampionsStepOff(PlayerbotAI* botAI, Position& spot)
{
    BallState* state = Refresh(botAI);
    if (!state)
        return false;

    Player* bot = botAI->GetBot();
    ObjectGuid const guid = bot->GetGUID();
    uint32 const now = getMSTime();
    auto const step = state->steps.find(guid);
    if (step != state->steps.end())
    {
        if (getMSTimeDiff(step->second.sinceMs, now) < STEP_OFF_MS &&
            bot->GetExactDist2d(&step->second.spot) > RUN_ARRIVE)
        {
            spot = step->second.spot;
            return true;
        }

        state->steps.erase(step);
        state->stepEndMs[guid] = now;
    }

    auto const rested = state->stepEndMs.find(guid);
    if (!PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsTank(bot) || FactionChampionsIsHolder(bot) ||
        bot->HasUnitState(UNIT_STATE_LOST_CONTROL) || bot->HasAuraType(SPELL_AURA_MOD_ROOT) ||
        bot->IsNonMeleeSpellCast(false) || state->runs.count(guid) ||
        (rested != state->stepEndMs.end() && getMSTimeDiff(rested->second, now) < STEP_OFF_REST_MS))
        return false;

    uint32 near = 0;
    float sumX = 0.0f;
    float sumY = 0.0f;
    for (Unit* champion : FactionChampionsInCombat(botAI))
    {
        if (!IsFactionChampionMelee(champion->GetEntry()) || champion->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
            bot->GetExactDist2d(champion) > STEP_OFF_TRIGGER)
            continue;

        ++near;
        sumX += champion->GetPositionX();
        sumY += champion->GetPositionY();
    }

    if (!near)
        return false;

    std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
    Vec const away = Unit2(bot->GetPositionX() - sumX / near, bot->GetPositionY() - sumY / near);
    for (float turn : {0.0f, 45.0f, -45.0f, 90.0f, -90.0f})
    {
        Position const target = Along(bot->GetPosition(), Rotate(away, turn), STEP_OFF_DIST);
        if (!Clear(target, hazards))
            continue;

        state->steps[guid] = {now, target};
        spot = target;
        return true;
    }

    return false;
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
    uint32 meleeOn = 0;
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
            meleeOn += IsFactionChampionMelee(champion->GetEntry()) ? 1 : 0;
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
    bool const melee = meleeOn >= NITRO_MELEE_FOCUS;
    if (on < NITRO_FOCUS && !melee && !next)
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
        RaidObs::Note(bot, "fc.nitro", on >= NITRO_FOCUS ? "focus " + std::to_string(on)
                                       : melee         ? "melee " + std::to_string(meleeOn)
                                                       : "next");
        spot = target;
        return true;
    }

    return false;
}

namespace
{
SpellInfo const* KnownReady(Player* bot, uint32 spellId)
{
    return spellId && bot->HasSpell(spellId) ? ReadyInstant(bot, spellId) : nullptr;
}

// Nothing a hit of ours would free, nothing a pull needs, and no fear on the kill target, which would run the
// kill squad around the arena
bool AoeEscapeClear(PlayerbotAI* botAI, Position const& centre, float radius, bool fear)
{
    Unit* kill = fear ? FactionChampionsKillTarget(botAI) : nullptr;
    for (Unit* champion : FactionChampionsInCombat(botAI))
    {
        if (champion->GetExactDist2d(&centre) > radius + ESCAPE_AOE_SLACK)
            continue;

        // a duty still casting or a root peel loses nothing to it, a landed Polymorph or Fear would
        if (champion == kill || FactionChampionsHeldByBreakableCc(champion) ||
            FactionChampionsGripGuarded(botAI, champion))
            return false;
    }

    return true;
}

// Distance from the spot to the nearest melee champion, max with none
float MeleeClearance(PlayerbotAI* botAI, Position const& spot)
{
    float nearest = std::numeric_limits<float>::max();
    for (Unit* champion : FactionChampionsInCombat(botAI))
        if (IsFactionChampionMelee(champion->GetEntry()))
            nearest = std::min(nearest, champion->GetExactDist2d(&spot));

    return nearest;
}

std::vector<Unit*> MeleeOn(PlayerbotAI* botAI, Player* bot, float radius)
{
    std::vector<Unit*> on;
    for (Unit* champion : FactionChampionsInCombat(botAI))
        if (IsFactionChampionMelee(champion->GetEntry()) && champion->GetVictim() == bot &&
            bot->GetExactDist2d(champion) <= radius)
            on.push_back(champion);

    return on;
}

// Healers keep their step-off, only ranged dps kites
bool KiteBot(PlayerbotAI* botAI)
{
    return FactionChampionsStaysPut(botAI) && !PlayerbotAI::IsHeal(botAI->GetBot()) && botAI->GetBot()->IsAlive();
}

bool AoeEscape(PlayerbotAI* botAI, OpenerSpell const& spell, Unit* at, bool fear, FactionChampionsEscapeCast& cast)
{
    Player* bot = botAI->GetBot();
    uint32 const rank = KnownOpenerRank(bot, spell);
    SpellInfo const* info = KnownReady(bot, rank);
    if (!info || !AoeEscapeClear(botAI, at->GetPosition(), SpellRadius(info, bot), fear))
        return false;

    cast.spellId = rank;
    cast.target = at;
    return true;
}
}  // namespace

bool FactionChampionsEscape(PlayerbotAI* botAI, FactionChampionsEscapeCast& cast)
{
    if (!FactionChampionsStaysPut(botAI))
        return false;

    Player* bot = botAI->GetBot();
    if (bot->HasUnitState(UNIT_STATE_LOST_CONTROL) || bot->HasUnitFlag(UNIT_FLAG_SILENCED))
        return false;

    std::vector<Unit*> melee;
    float sumX = 0.0f;
    float sumY = 0.0f;
    Unit* nearest = nullptr;
    for (Unit* champion : FactionChampionsInCombat(botAI))
    {
        if (!IsFactionChampionMelee(champion->GetEntry()) || champion->GetVictim() != bot ||
            champion->HasUnitState(UNIT_STATE_LOST_CONTROL) || bot->GetExactDist2d(champion) > ESCAPE_MELEE_REACH)
            continue;

        melee.push_back(champion);
        sumX += champion->GetPositionX();
        sumY += champion->GetPositionY();
        if (!nearest || bot->GetExactDist2d(champion) < bot->GetExactDist2d(nearest))
            nearest = champion;
    }

    if (!nearest)
        return false;

    Position const mean(sumX / melee.size(), sumY / melee.size(), bot->GetPositionZ());
    Vec const away = Unit2(bot->GetPositionX() - mean.GetPositionX(), bot->GetPositionY() - mean.GetPositionY());
    cast = FactionChampionsEscapeCast();
    switch (bot->getClass())
    {
        case CLASS_MAGE:
        {
            // rooted first, so the blink after it leaves them behind
            if (AoeEscape(botAI, FROST_NOVA, bot, false, cast))
                return true;

            if (KnownReady(bot, SPELL_BLINK))
            {
                // off the whole pack, not just the ones on it, or it lands on the far side of the others
                float fleeX = 0.0f;
                float fleeY = 0.0f;
                uint32 flee = 0;
                for (Unit* champion : FactionChampionsInCombat(botAI))
                {
                    if (!IsFactionChampionMelee(champion->GetEntry()) ||
                        bot->GetExactDist2d(champion) > BLINK_FLEE_RADIUS)
                        continue;

                    ++flee;
                    fleeX += champion->GetPositionX();
                    fleeY += champion->GetPositionY();
                }

                Vec const from =
                    flee ? Unit2(bot->GetPositionX() - fleeX / flee, bot->GetPositionY() - fleeY / flee) : away;
                std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
                float bestClear = BLINK_MIN_CLEAR;
                bool found = false;
                Vec best = from;
                for (float turn : {0.0f, 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f})
                {
                    Vec const dir = Rotate(from, turn);
                    Position const landing = Along(bot->GetPosition(), dir, BLINK_DIST);
                    if (!Clear(landing, hazards))
                        continue;

                    float const clear = MeleeClearance(botAI, landing);
                    if (clear >= bestClear)
                    {
                        bestClear = clear;
                        best = dir;
                        found = true;
                    }
                }

                if (found)
                {
                    cast.spellId = SPELL_BLINK;
                    cast.target = bot;
                    cast.face = true;
                    cast.facing = Position::NormalizeOrientation(std::atan2(best.y, best.x));
                    return true;
                }
            }

            if (AoeEscape(botAI, DRAGONS_BREATH, bot, true, cast))
            {
                cast.face = true;
                cast.facing = bot->GetAngle(nearest);
                return true;
            }

            return false;
        }
        case CLASS_WARLOCK:
        {
            if (AoeEscape(botAI, SHADOWFURY, nearest, false, cast))
                return true;

            GameObject* circle = bot->GetGameObject(SPELL_DEMONIC_CIRCLE_SUMMON);
            if (circle && bot->HasAura(SPELL_DEMONIC_CIRCLE_IN_RANGE) &&
                KnownReady(bot, SPELL_DEMONIC_CIRCLE_TELEPORT) &&
                circle->GetExactDist2d(&mean) >= bot->GetExactDist2d(&mean) + ESCAPE_MIN_GAIN &&
                Clear(circle->GetPosition(), FactionChampionsAoeClearances(botAI)))
            {
                cast.spellId = SPELL_DEMONIC_CIRCLE_TELEPORT;
                cast.target = bot;
                return true;
            }

            if (AoeEscape(botAI, HOWL_OF_TERROR, bot, true, cast))
                return true;

            auto const known = std::find_if(DEATH_COIL_RANKS.begin(), DEATH_COIL_RANKS.end(),
                                            [bot](uint32 rank) { return bot->HasSpell(rank); });
            uint32 const coil = known != DEATH_COIL_RANKS.end() ? *known : 0;
            if (KnownReady(bot, coil) && nearest != FactionChampionsKillTarget(botAI) &&
                !FactionChampionsGripGuarded(botAI, nearest))
            {
                cast.spellId = coil;
                cast.target = nearest;
                return true;
            }

            return false;
        }
        case CLASS_PRIEST:
            return AoeEscape(botAI, PSYCHIC_SCREAM, bot, true, cast);
        case CLASS_HUNTER:
        {
            if (bot->GetExactDist2d(nearest) > DISENGAGE_REACH || !KnownReady(bot, SPELL_DISENGAGE))
                return false;

            // leaps backwards off its facing, so face the nearest first
            float const facing = bot->GetAngle(nearest);
            Position const landing(bot->GetPositionX() - std::cos(facing) * DISENGAGE_DIST,
                                   bot->GetPositionY() - std::sin(facing) * DISENGAGE_DIST, bot->GetPositionZ());
            if (!Clear(landing, FactionChampionsAoeClearances(botAI)))
                return false;

            cast.spellId = SPELL_DISENGAGE;
            cast.target = bot;
            cast.face = true;
            cast.facing = facing;
            return true;
        }
        default:
            return false;
    }
}

bool FactionChampionsKite(PlayerbotAI* botAI, Position& spot)
{
    BallState* state = botAI && KiteBot(botAI) ? Refresh(botAI) : nullptr;
    if (!state)
        return false;

    Player* bot = botAI->GetBot();
    ObjectGuid const guid = bot->GetGUID();
    std::vector<Unit*> const chasers = MeleeOn(botAI, bot, KITE_CLEAR);
    auto const kite = state->kites.find(guid);
    if (chasers.empty() || bot->HasUnitState(UNIT_STATE_LOST_CONTROL) || bot->HasAuraType(SPELL_AURA_MOD_ROOT) ||
        state->runs.count(guid) || FactionChampionsHasCcDuty(botAI, guid))
    {
        if (kite != state->kites.end())
            state->kites.erase(kite);

        return false;
    }

    uint32 const now = getMSTime();
    if (kite != state->kites.end())
    {
        if (getMSTimeDiff(kite->second.sinceMs, now) < KITE_STEP_MS &&
            bot->GetExactDist2d(&kite->second.spot) > RUN_ARRIVE)
        {
            spot = kite->second.spot;
            return true;
        }

        state->kites.erase(kite);
    }

    bool gains = false;
    float sumX = 0.0f;
    float sumY = 0.0f;
    float nearest = std::numeric_limits<float>::max();
    for (Unit* chaser : chasers)
    {
        float const dist = bot->GetExactDist2d(chaser);
        nearest = std::min(nearest, dist);
        sumX += chaser->GetPositionX();
        sumY += chaser->GetPositionY();
        if (dist > KITE_CONTACT || chaser->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
            chaser->HasAuraType(SPELL_AURA_MOD_ROOT) || chaser->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
            gains = true;
    }

    if (!gains)
        return false;

    std::vector<Player*> healers;
    for (Player* member : GroupOnMap(bot))
        if (member != bot && member->IsAlive() && PlayerbotAI::IsHeal(member))
            healers.push_back(member);

    std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
    Vec const away = Unit2(bot->GetPositionX() - sumX / chasers.size(), bot->GetPositionY() - sumY / chasers.size());
    float bestClear = -1.0f;
    Position best;
    for (float turn : {0.0f, 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f, 120.0f, -120.0f})
    {
        Position const target = Along(bot->GetPosition(), Rotate(away, turn), KITE_STEP);
        if (!Clear(target, hazards) ||
            std::any_of(healers.begin(), healers.end(),
                        [&target](Player* healer) { return healer->GetExactDist2d(&target) < KITE_HEALER_GAP; }))
            continue;

        float const clear = MeleeClearance(botAI, target);
        if (clear > bestClear)
        {
            bestClear = clear;
            best = target;
        }
    }

    if (bestClear < 0.0f)
        return false;

    state->kites[guid] = {now, best};
    RaidObs::Note(bot, "fc.kite",
                  std::to_string(chasers.size()) + " melee, " + std::to_string(static_cast<int>(nearest)) + " yd");
    spot = best;
    return true;
}

bool FactionChampionsKiteHold(PlayerbotAI* botAI)
{
    return botAI && KiteBot(botAI) && !MeleeOn(botAI, botAI->GetBot(), KITE_HOLD).empty();
}

bool FactionChampionsCircleDue(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->getClass() != CLASS_WARLOCK || !bot->IsAlive() || bot->IsInCombat() ||
        !bot->HasSpell(SPELL_DEMONIC_CIRCLE_SUMMON) || GetToCStagingBoss(botAI) != ToCStagingBoss::Champions ||
        !IsToCStagingHeld(botAI) || IsToCStagingRestHeld(botAI))
        return false;

    Aura const* circle = bot->GetAura(SPELL_DEMONIC_CIRCLE_SUMMON, bot->GetGUID());
    return !circle || (circle->GetDuration() >= 0 && circle->GetDuration() < CIRCLE_REFRESH_MS);
}

}
