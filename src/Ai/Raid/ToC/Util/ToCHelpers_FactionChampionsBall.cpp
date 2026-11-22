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
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "ToCHelpers_Shared.h"
#include "ToCHelpers_Staging.h"

namespace TrialOfTheCrusaderHelpers
{

namespace
{
constexpr float ARENA_FLOOR_RADIUS = 44.0f;

constexpr float SIDE_STEP_DEG = 15.0f;
constexpr float SIDE_REANCHOR = 6.0f;
constexpr float CYCLONE_STAND_MARGIN = 2.0f;
constexpr std::array<float, 3> CYCLONE_RINGS = {20.0f, 26.0f, 30.0f};

constexpr float STEP_OFF_DIST = 10.0f;
constexpr uint32 STEP_OFF_MS = 2500;
constexpr uint32 STEP_OFF_REST_MS = 3000;

constexpr uint32 NITRO_FOCUS = 3;
constexpr float NITRO_MIN_HEALTH_PCT = 30.0f;
constexpr float NITRO_STEP_BACK = 5.0f;
constexpr float NITRO_MIN_RUN = 15.0f;
constexpr std::array<float, 9> RUN_TURNS = {0.0f, 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f, 120.0f, -120.0f};
constexpr float RUN_ARRIVE = 2.0f;
constexpr float NEXT_FOCUS_RADIUS = 30.0f;

constexpr uint32 OPENER_MIN_CHAMPIONS = 3;
constexpr float OPENER_MARGIN = 1.0f;

struct OpenerSpell
{
    std::array<uint32, 6> ranks;
};

constexpr OpenerSpell INTIMIDATING_SHOUT{{5246, 0, 0, 0, 0, 0}};
constexpr OpenerSpell PSYCHIC_SCREAM{{10890, 10888, 8124, 8122, 0, 0}};
constexpr OpenerSpell HUNGERING_COLD{{49203, 0, 0, 0, 0, 0}};
constexpr OpenerSpell HOWL_OF_TERROR{{17928, 5484, 0, 0, 0, 0}};
constexpr OpenerSpell FROST_NOVA{{42917, 27088, 10230, 6131, 865, 122}};
constexpr OpenerSpell DRAGONS_BREATH{{42950, 42949, 33043, 33042, 33041, 31661}};
constexpr OpenerSpell SHADOWFURY{{47847, 47846, 30414, 30413, 30283, 0}};

constexpr std::array<OpenerSpell, 5> OPENER_SPELLS = {INTIMIDATING_SHOUT, PSYCHIC_SCREAM, HUNGERING_COLD,
                                                      HOWL_OF_TERROR, FROST_NOVA};

constexpr OpenerSpell SPRINT{{11305, 8696, 2983, 0, 0, 0}};
constexpr OpenerSpell DASH{{33357, 9821, 1850, 0, 0, 0}};

constexpr float ESCAPE_MIN_GAIN = 12.0f;
constexpr float BLINK_DIST = 20.0f;
constexpr uint32 SPELL_BLINK = 1953;
constexpr uint32 SPELL_DEMONIC_CIRCLE_TELEPORT = 48020;
constexpr uint32 SPELL_DEMONIC_CIRCLE_IN_RANGE = 62388;
constexpr int32 CIRCLE_REFRESH_MS = 60000;
constexpr uint32 NITRO_MELEE_FOCUS = 2;
constexpr float BLINK_FLEE_RADIUS = 15.0f;
constexpr float BLINK_MIN_CLEAR = 12.0f;
constexpr uint32 SPELL_DISENGAGE = 781;
constexpr uint32 SPELL_FROST_TRAP = 13809;
constexpr float FROST_TRAP_SLICK = 10.0f;
constexpr float DISENGAGE_DIST = 15.0f;

constexpr float KITE_CLEAR = 30.0f;
constexpr float KITE_STEP = 15.0f;
constexpr uint32 KITE_STEP_MS = 2500;
constexpr float KITE_HEALER_GAP = 10.0f;
constexpr float KITE_HOLD = 45.0f;
constexpr float KITE_CRAWL_RATE = 0.6f;
constexpr float KITE_CRAWL_MARGIN = 2.25f;
constexpr float KITE_CRAWL_REACH_RET = 17.0f;
constexpr float KITE_HEAL_REACH = 30.0f;
constexpr uint32 CAST_BREAK_HOLD_MS = 1000;

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

struct CycloneStand
{
    ObjectGuid kill;
    ObjectGuid healer;
    Position spot;
};

struct BallState
{
    RaidObs::ObsGuidMap<uint32> opener{"fc.opener"};
    RaidObs::ObsGuidMap<ObjectGuid> cyclone{"fc.cyclone"};

    uint32 memoMs = 0;
    bool memoValid = false;
    std::unordered_map<ObjectGuid, Run> runs;
    std::unordered_map<ObjectGuid, Run> steps;
    std::unordered_map<ObjectGuid, Run> kites;
    std::unordered_map<ObjectGuid, uint32> stepEndMs;
    std::unordered_map<ObjectGuid, CycloneStand> cycloneStands;
    std::unordered_map<ObjectGuid, uint32> castBreaks;
    ToCGroupMemo<std::vector<ObjectGuid>> healers;
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

std::vector<Player*> HealersOnMap(Player* bot, BallState const& state)
{
    std::vector<ObjectGuid> const& guids = state.healers.Get(bot, [bot](std::vector<ObjectGuid>& healers)
    {
        for (Player* member : GroupOnMap(bot))
            if (PlayerbotAI::IsHeal(member))
                healers.push_back(member->GetGUID());
    });

    std::vector<Player*> healers;
    for (ObjectGuid const& guid : guids)
        if (Player* healer = ObjectAccessor::GetPlayer(*bot, guid))
            healers.push_back(healer);

    return healers;
}

uint32 KnownOpenerRank(Player* player, OpenerSpell const& spell)
{
    for (uint32 rank : spell.ranks)
        if (rank && player->HasSpell(rank))
            return rank;

    return 0;
}

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

size_t CollectTrinketsInReach(PlayerbotAI* botAI, Player* caster, SpellInfo const* info,
                              std::vector<Unit*> const& champions, std::vector<Unit*>* inReach)
{
    size_t count = 0;
    float const radius = SpellRadius(info, caster);
    for (Unit* champion : champions)
    {
        float const distance = caster->GetExactDist2d(champion);
        if (distance <= radius + OPENER_MARGIN && FactionChampionsGripGuarded(botAI, champion))
        {
            if (inReach)
                inReach->clear();

            return 0;
        }

        if (distance <= radius - OPENER_MARGIN && FactionChampionsTrinketReady(champion) &&
            !champion->IsImmunedToSpell(info))
        {
            ++count;
            if (inReach)
                inReach->push_back(champion);
        }
    }

    return count;
}

std::vector<Unit*> TrinketsInReach(PlayerbotAI* botAI, Player* caster, SpellInfo const* info,
                                   std::vector<Unit*> const& champions)
{
    std::vector<Unit*> inReach;
    CollectTrinketsInReach(botAI, caster, info, champions, &inReach);
    return inReach;
}

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

            size_t const count = CollectTrinketsInReach(botAI, member, info, champions, nullptr);
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
        if (FactionChampionsKitesFrom(bot, champion) && bot->GetExactDist2d(champion) <= radius)
            on.push_back(champion);

    return on;
}

bool Unhindered(Unit* champion)
{
    return !champion->HasUnitState(UNIT_STATE_LOST_CONTROL) && !champion->HasAuraType(SPELL_AURA_MOD_ROOT) &&
           !champion->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED);
}

bool InContact(Unit* champion, Player* bot)
{
    return Unhindered(champion) && bot->GetExactDist2d(champion) <= FactionChampionsSwingReach(champion, bot);
}

bool Crawling(Unit* champion)
{
    return champion->HasUnitState(UNIT_STATE_LOST_CONTROL) || champion->HasAuraType(SPELL_AURA_MOD_ROOT) ||
           champion->GetSpeedRate(MOVE_RUN) <= KITE_CRAWL_RATE;
}

float CrawlReach(Unit* champion, Player* bot)
{
    uint32 const entry = champion->GetEntry();
    bool const ret = entry == static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PALADIN_RETRIBUTION) ||
                     entry == static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PALADIN_RETRIBUTION);
    float const reach = FactionChampionsSwingReach(champion, bot) + KITE_CRAWL_MARGIN;
    return ret ? std::max(reach, KITE_CRAWL_REACH_RET) : reach;
}

std::vector<Unit*> KiteChasers(PlayerbotAI* botAI, Player* bot)
{
    std::vector<Unit*> chasers = MeleeOn(botAI, bot, KITE_CLEAR);
    chasers.erase(std::remove_if(chasers.begin(), chasers.end(), [bot](Unit* chaser)
                                 { return Crawling(chaser) && bot->GetExactDist2d(chaser) > CrawlReach(chaser, bot); }),
                  chasers.end());
    return chasers;
}

bool KiteFree(BallState const& state, PlayerbotAI* botAI, Player* bot)
{
    return !bot->HasUnitState(UNIT_STATE_LOST_CONTROL) && !bot->HasAuraType(SPELL_AURA_MOD_ROOT) &&
           !state.runs.count(bot->GetGUID()) && !FactionChampionsHasCcDuty(botAI, bot->GetGUID());
}

bool NearHealer(Player* bot, std::vector<Player*> const& members, Position const& spot)
{
    return std::any_of(members.begin(), members.end(),
                       [bot, &spot](Player* member)
                       {
                           return member != bot && PlayerbotAI::IsHeal(member) &&
                                  member->GetExactDist2d(&spot) < KITE_HEALER_GAP;
                       });
}

float HealerGap(Player* bot, std::vector<Player*> const& members, Position const& spot)
{
    float nearest = std::numeric_limits<float>::max();
    for (Player* member : members)
        if (member != bot && member->IsAlive() && PlayerbotAI::IsHeal(member))
            nearest = std::min(nearest, member->GetExactDist2d(&spot));

    return nearest;
}

bool Healed(float gap, float nowGap)
{
    return gap <= KITE_HEAL_REACH || gap < nowGap || nowGap == std::numeric_limits<float>::max();
}

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

void UpdateCycloneStands(BallState& state, PlayerbotAI* botAI, std::vector<Player*> const& members)
{
    Position anchor;
    Position centre;
    Unit* killTarget = FactionChampionsKillTarget(botAI);
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

void BreakChasedCasts(BallState& state, PlayerbotAI* botAI, std::vector<Player*> const& members, uint32 now)
{
    for (Player* member : members)
    {
        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        Spell* cast = member->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!memberAI || !cast || cast->getState() != SPELL_STATE_PREPARING || !member->IsMovementPreventedByCasting())
            continue;

        auto const asked = state.castBreaks.find(member->GetGUID());
        if (asked != state.castBreaks.end() && getMSTimeDiff(asked->second, now) < CAST_BREAK_HOLD_MS)
            continue;

        float const leftSec = static_cast<float>(std::max(cast->GetCastTimeRemaining(), 0)) / 1000.0f;
        std::vector<Unit*> const chasers = KiteChasers(botAI, member);
        bool const caught = std::any_of(chasers.begin(), chasers.end(), [member, leftSec](Unit* chaser)
                                        {
                                            return member->GetExactDist2d(chaser) -
                                                       FactionChampionsSwingReach(chaser, member) <=
                                                   chaser->GetSpeed(MOVE_RUN) * leftSec;
                                        });
        if (!caught || !KiteFree(state, botAI, member))
            continue;

        state.castBreaks[member->GetGUID()] = now;
        memberAI->RequestSpellInterrupt();
        RaidObs::Note(member, "fc.castbreak", std::string(cast->GetSpellInfo()->SpellName[0]));
    }
}

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
    BreakChasedCasts(state, botAI, members, now);
    return &state;
}
}

bool FactionChampionsCycloneStand(PlayerbotAI* botAI, Position& spot)
{
    BallState* state = Refresh(botAI);
    if (!state)
        return false;

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
           (PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsRanged(bot));
}

bool FactionChampionsKitesFrom(Player* member, Unit* champion)
{
    if (!member || !champion || !member->IsAlive() || !IsFactionChampionMelee(champion->GetEntry()) ||
        champion->GetVictim() != member)
        return false;

    return !PlayerbotAI::IsTank(member) || FactionChampionsIceDutyOf(member) == champion->GetGUID();
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
    if (!PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsTank(bot) || bot->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
        bot->HasAuraType(SPELL_AURA_MOD_ROOT) || bot->IsNonMeleeSpellCast(false) || state->runs.count(guid) ||
        state->kites.count(guid) ||
        (rested != state->stepEndMs.end() && getMSTimeDiff(rested->second, now) < STEP_OFF_REST_MS))
        return false;

    uint32 nearby = 0;
    float sumX = 0.0f;
    float sumY = 0.0f;
    for (Unit* champion : FactionChampionsInCombat(botAI))
    {
        if (!IsFactionChampionMelee(champion->GetEntry()) || champion->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
            bot->GetExactDist2d(champion) > FactionChampionsSwingReach(champion, bot))
            continue;

        ++nearby;
        sumX += champion->GetPositionX();
        sumY += champion->GetPositionY();
    }

    if (!nearby)
        return false;

    std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
    Vec const away = Unit2(bot->GetPositionX() - sumX / nearby, bot->GetPositionY() - sumY / nearby);
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

    if (PlayerbotAI::IsTank(bot) || bot->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
        bot->HasAuraType(SPELL_AURA_MOD_ROOT) || FactionChampionsHasCcDuty(botAI, bot->GetGUID()) ||
        bot->GetHealthPct() < NITRO_MIN_HEALTH_PCT || !EncounterHelpers::NitroBoostsReady(bot))
        return false;

    uint32 on = 0;
    uint32 meleeOn = 0;
    uint32 contact = 0;
    std::array<float, 2> onSum = {0.0f, 0.0f};
    std::array<float, 2> meleeSum = {0.0f, 0.0f};
    std::array<float, 2> nearSum = {0.0f, 0.0f};
    uint32 nearby = 0;
    for (Unit* champion : FactionChampionsInCombat(botAI))
    {
        if (champion->HasUnitState(UNIT_STATE_LOST_CONTROL))
            continue;

        if (champion->GetVictim() == bot)
        {
            ++on;
            onSum[0] += champion->GetPositionX();
            onSum[1] += champion->GetPositionY();
            if (IsFactionChampionMelee(champion->GetEntry()))
            {
                ++meleeOn;
                meleeSum[0] += champion->GetPositionX();
                meleeSum[1] += champion->GetPositionY();
                if (InContact(champion, bot))
                    ++contact;
            }
        }

        if (bot->GetExactDist2d(champion) <= NEXT_FOCUS_RADIUS)
        {
            ++nearby;
            nearSum[0] += champion->GetPositionX();
            nearSum[1] += champion->GetPositionY();
        }
    }

    bool const next = FactionChampionsNextFocus(botAI) == bot->GetGUID();
    bool const melee = meleeOn >= NITRO_MELEE_FOCUS;
    if (on < NITRO_FOCUS && !melee && !next && !contact)
        return false;

    uint32 const from = meleeOn ? meleeOn : (on ? on : nearby);
    std::array<float, 2> const& sum = meleeOn ? meleeSum : (on ? onSum : nearSum);
    if (!from)
        return false;

    Vec const away = Unit2(bot->GetPositionX() - sum[0] / from, bot->GetPositionY() - sum[1] / from);
    float const reach = bot->GetSpeed(MOVE_RUN) * EncounterHelpers::NITRO_BOOSTS_SPEED_FACTOR *
                        EncounterHelpers::NITRO_BOOSTS_DURATION_MS / IN_MILLISECONDS;
    std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
    std::vector<Player*> const healers = HealersOnMap(bot, *state);
    std::vector<Player*> const members = PlayerbotAI::IsHeal(bot) ? std::vector<Player*>() : healers;
    float const nowGap = HealerGap(bot, healers, bot->GetPosition());
    bool found = false;
    float bestClear = 0.0f;
    float bestRun = 0.0f;
    Position best;
    for (float turn : RUN_TURNS)
    {
        Vec const dir = Rotate(away, turn);
        for (float run = reach; run >= NITRO_MIN_RUN; run -= NITRO_STEP_BACK)
        {
            Position const target = Along(bot->GetPosition(), dir, run);
            if (!Clear(target, hazards) || NearHealer(bot, members, target) ||
                !Healed(HealerGap(bot, healers, target), nowGap))
                continue;

            float const clear = MeleeClearance(botAI, target);
            if (!found || clear > bestClear || (clear == bestClear && run > bestRun))
            {
                found = true;
                bestClear = clear;
                bestRun = run;
                best = target;
            }

            break;
        }
    }

    if (!found)
        return false;

    state->runs[bot->GetGUID()] = {now, best};
    RaidObs::Note(bot, "fc.nitro", on >= NITRO_FOCUS ? "focus " + std::to_string(on)
                                   : melee         ? "melee " + std::to_string(meleeOn)
                                   : contact       ? "contact " + std::to_string(contact)
                                                   : "next");
    spot = best;
    return true;
}

namespace
{
SpellInfo const* KnownReady(Player* bot, uint32 spellId)
{
    return spellId && bot->HasSpell(spellId) ? ReadyInstant(bot, spellId) : nullptr;
}

bool InAoe(Unit* champion, Position const& centre, float radius)
{
    return champion->GetExactDist2d(&centre) <= radius + champion->GetCombatReach();
}

bool AoeEscapeClear(PlayerbotAI* botAI, Position const& centre, float radius, bool fear)
{
    Unit* kill = fear ? FactionChampionsKillTarget(botAI) : nullptr;
    for (Unit* champion : FactionChampionsInCombat(botAI))
    {
        if (!InAoe(champion, centre, radius))
            continue;

        if (champion == kill || FactionChampionsHeldByBreakableCc(champion) ||
            FactionChampionsGripGuarded(botAI, champion))
            return false;
    }

    return true;
}

bool FrostTrapClear(PlayerbotAI* botAI, Position const& centre)
{
    for (Unit* champion : FactionChampionsInCombat(botAI))
        if (InAoe(champion, centre, FROST_TRAP_SLICK + DEFAULT_WORLD_OBJECT_SIZE) &&
            (FactionChampionsIced(botAI, champion) || FactionChampionsGripGuarded(botAI, champion)))
            return false;

    return true;
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
}

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
    float nearestGap = 0.0f;
    for (Unit* champion : FactionChampionsInCombat(botAI))
    {
        if (!IsFactionChampionMelee(champion->GetEntry()) || champion->GetVictim() != bot ||
            champion->HasUnitState(UNIT_STATE_LOST_CONTROL))
            continue;

        float const gap = bot->GetExactDist2d(champion) - FactionChampionsSwingReach(champion, bot);
        if (gap > 0.0f)
            continue;

        melee.push_back(champion);
        sumX += champion->GetPositionX();
        sumY += champion->GetPositionY();
        if (!nearest || gap < nearestGap)
        {
            nearest = champion;
            nearestGap = gap;
        }
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
            if (AoeEscape(botAI, FROST_NOVA, bot, false, cast))
                return true;

            if (KnownReady(bot, SPELL_BLINK))
            {
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
            if (KnownReady(bot, SPELL_FROST_TRAP) && FrostTrapClear(botAI, bot->GetPosition()))
            {
                cast.spellId = SPELL_FROST_TRAP;
                cast.target = bot;
                return true;
            }

            if (bot->GetExactDist2d(nearest) > nearest->GetMeleeRange(bot) || !KnownReady(bot, SPELL_DISENGAGE))
                return false;

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
    BallState* state = Refresh(botAI);
    if (!state)
        return false;

    Player* bot = botAI->GetBot();
    ObjectGuid const guid = bot->GetGUID();
    std::vector<Unit*> const chasers = KiteChasers(botAI, bot);
    auto const kite = state->kites.find(guid);
    if (chasers.empty() || !KiteFree(*state, botAI, bot))
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

    float sumX = 0.0f;
    float sumY = 0.0f;
    float nearest = std::numeric_limits<float>::max();
    for (Unit* chaser : chasers)
    {
        nearest = std::min(nearest, bot->GetExactDist2d(chaser));
        sumX += chaser->GetPositionX();
        sumY += chaser->GetPositionY();
    }

    std::vector<Player*> const healers = HealersOnMap(bot, *state);
    std::vector<EncounterHelpers::HazardCircle> const hazards = FactionChampionsAoeClearances(botAI);
    Vec const away = Unit2(bot->GetPositionX() - sumX / chasers.size(), bot->GetPositionY() - sumY / chasers.size());
    float const nowGap = HealerGap(bot, healers, bot->GetPosition());
    bool found = false;
    bool bestHealed = false;
    float bestClear = 0.0f;
    float bestGap = 0.0f;
    Position best;
    for (float turn : RUN_TURNS)
    {
        Position const target = Along(bot->GetPosition(), Rotate(away, turn), KITE_STEP);
        if (!Clear(target, hazards) || NearHealer(bot, healers, target))
            continue;

        float const gap = HealerGap(bot, healers, target);
        bool const healed = Healed(gap, nowGap);
        float const clear = MeleeClearance(botAI, target);
        bool const better = !found || (healed && !bestHealed) ||
                            (healed == bestHealed && (healed ? clear > bestClear : gap < bestGap));
        if (better)
        {
            found = true;
            bestHealed = healed;
            bestClear = clear;
            bestGap = gap;
            best = target;
        }
    }

    if (!found)
        return false;

    state->kites[guid] = {now, best};
    RaidObs::Note(bot, "fc.kite",
                  std::to_string(chasers.size()) + " melee, " + std::to_string(static_cast<int>(nearest)) + " yd");
    spot = best;
    return true;
}

bool FactionChampionsKiteHold(PlayerbotAI* botAI)
{
    return Refresh(botAI) && !MeleeOn(botAI, botAI->GetBot(), KITE_HOLD).empty();
}

uint32 FactionChampionsSpeedBoost(PlayerbotAI* botAI)
{
    BallState* state = Refresh(botAI);
    if (!state)
        return 0;

    Player* bot = botAI->GetBot();
    uint32 rank = 0;
    if (bot->getClass() == CLASS_ROGUE)
        rank = KnownOpenerRank(bot, SPRINT);
    else if (bot->getClass() == CLASS_DRUID && bot->GetShapeshiftForm() == FORM_CAT)
        rank = KnownOpenerRank(bot, DASH);

    SpellInfo const* info = rank ? ReadyInstant(bot, rank) : nullptr;
    if (!info ||
        bot->GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SPEED) >= info->Effects[EFFECT_0].CalcValue(bot) ||
        bot->HasUnitState(UNIT_STATE_LOST_CONTROL) || bot->HasAuraType(SPELL_AURA_MOD_ROOT) ||
        state->runs.count(bot->GetGUID()) || FactionChampionsHasCcDuty(botAI, bot->GetGUID()))
        return 0;

    std::vector<Unit*> const chasers = MeleeOn(botAI, bot, KITE_CLEAR);
    return std::any_of(chasers.begin(), chasers.end(), [bot](Unit* chaser) { return InContact(chaser, bot); })
               ? rank
               : 0;
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
