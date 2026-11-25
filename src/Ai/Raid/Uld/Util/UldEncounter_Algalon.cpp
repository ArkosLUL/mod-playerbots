/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Algalon.h"

#include "Action.h"
#include "AiFactory.h"
#include "AiObjectContext.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Item.h"
#include "LastMovementValue.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "PaladinHelper.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidTankDefensive.h"
#include "ServerFacade.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldEncounterGate.h"
#include "Unit.h"
#include "WorldPacket.h"
#include <RtiTargetValue.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>
#include <list>
#include <optional>
#include <set>
#include <string>
#include <tuple>

using namespace EncounterHelpers;

const Position ULDUAR_ALGALON_ROOM_CENTER = Position(1632.668f, -302.7656f, 417.3211f);
const Position ULDUAR_ALGALON_PICKUP_SLOT = Position(1633.781f, -310.688f, 417.321f);
const Position ULDUAR_ALGALON_PULL_OFFTANK_SLOT = Position(1628.668f, -309.694f, 417.321f);
const Position ULDUAR_ALGALON_TANK_SLOT = Position(1637.957f, -340.396f, 417.321f);
const Position ULDUAR_ALGALON_HOLD_SPOT = Position(1636.843f, -332.474f, 417.321f);
const Position ULDUAR_ALGALON_OFFTANK_SLOT = Position(1633.300f, -336.001f, 417.321f);

const std::array<Position, ULDUAR_ALGALON_TOTAL_SLOTS> ULDUAR_ALGALON_RING_SLOTS = {{
    Position(1617.869f, -333.468f, 417.321f),
    Position(1655.356f, -328.200f, 417.321f),
    Position(1619.354f, -339.898f, 417.321f),
    Position(1655.702f, -334.789f, 417.321f),
    Position(1612.390f, -327.276f, 417.321f),
    Position(1658.917f, -320.737f, 417.321f),
    Position(1611.877f, -333.782f, 417.321f),
    Position(1661.202f, -326.850f, 417.321f),
    Position(1613.067f, -340.199f, 417.321f),
    Position(1661.828f, -333.346f, 417.321f),
    Position(1613.806f, -311.731f, 417.321f),
    Position(1653.271f, -306.184f, 417.321f),
    Position(1609.222f, -318.400f, 417.321f),
    Position(1659.515f, -311.332f, 417.321f),
    Position(1606.521f, -326.028f, 417.321f),
    Position(1664.215f, -317.920f, 417.321f),
    Position(1605.886f, -334.096f, 417.321f),
    Position(1667.049f, -325.500f, 417.321f),
}};

const std::array<Position, ULDUAR_ALGALON_MELEE_STAGE_SPOTS> ULDUAR_ALGALON_MELEE_STAGE_SLOTS = {{
    Position(1621.852f, -331.950f, 417.321f),
    Position(1651.109f, -327.838f, 417.321f),
    Position(1621.989f, -334.561f, 417.321f),
    Position(1651.697f, -330.386f, 417.321f),
    Position(1622.577f, -337.109f, 417.321f),
    Position(1651.834f, -332.997f, 417.321f),
    Position(1623.599f, -339.516f, 417.321f),
    Position(1651.515f, -335.592f, 417.321f),
    Position(1623.447f, -342.940f, 417.321f),
    Position(1652.605f, -338.842f, 417.321f),
}};

static RaidInstanceState<AlgalonEncounterState> algalonEncounterStates;

namespace
{

Creature* FindAlgalon(Player* bot)
{
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return nullptr;

    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance)
        return nullptr;

    Creature* algalon = instance->GetCreature(ULD_BOSS_ALGALON);
    return algalon && algalon->IsAlive() ? algalon : nullptr;
}

AlgalonEncounterState* FindState(Player* bot)
{
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return nullptr;

    return algalonEncounterStates.Find(bot->GetInstanceId());
}

Spell* CurrentBigBang(Unit* boss)
{
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return nullptr;

    if (Spell* spell = boss->FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG))
        return spell;

    return boss->FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG_25);
}

Player* FindGroupMember(Group* group, ObjectGuid const& guid)
{
    if (!group || guid.IsEmpty())
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->GetGUID() == guid)
            return member;
    }

    return nullptr;
}

bool InFight(Player* member)
{
    return member && member->IsInWorld() && member->IsAlive() && member->GetMapId() == ULDUAR_MAP_ID;
}

bool CanStayOut(Player* member) { return InFight(member) && !IsAlgalonPhased(member); }

bool IsDamageDealer(Player* member) { return !PlayerbotAI::IsHeal(member) && !PlayerbotAI::IsTank(member); }

bool IsRangedDps(Player* member) { return PlayerbotAI::IsRanged(member) && IsDamageDealer(member); }

bool IsBot(Player* member) { return member && GET_PLAYERBOT_AI(member); }

ObjectGuid StarFocusOf(AlgalonEncounterState const& state, ObjectGuid const& member)
{
    ObjectGuid const finish = state.finishStar.Get();
    return !finish.IsEmpty() && state.finisher.Get() == member ? finish : state.focusStar.Get();
}

float NearestRaidMember(Group* group, Position const& spot)
{
    float nearest = std::numeric_limits<float>::max();
    if (!group)
        return nearest;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (CanStayOut(member))
            nearest = std::min(nearest, member->GetExactDist2d(spot));
    }

    return nearest;
}

std::string DescribeYards(float yards)
{
    if (yards == std::numeric_limits<float>::max())
        return "-";

    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%.1f", yards);
    return buffer;
}

float SegmentDistance(Position const& from, float toX, float toY, Position const& point)
{
    float const dx = toX - from.GetPositionX();
    float const dy = toY - from.GetPositionY();
    float const lengthSq = dx * dx + dy * dy;
    float t = 0.0f;
    if (lengthSq > 0.0f)
        t = ((point.GetPositionX() - from.GetPositionX()) * dx + (point.GetPositionY() - from.GetPositionY()) * dy) /
            lengthSq;

    t = std::clamp(t, 0.0f, 1.0f);
    return std::hypot(from.GetPositionX() + t * dx - point.GetPositionX(),
                      from.GetPositionY() + t * dy - point.GetPositionY());
}

bool BigBangDueWithin(AlgalonEncounterState const& state, uint32 seconds, uint32 now)
{
    if (!state.nextBigBangMs || state.bigBangCasting)
        return false;

    int32 const left = static_cast<int32>(state.nextBigBangMs - now);
    return left <= static_cast<int32>(seconds * IN_MILLISECONDS);
}

uint8 ReservedHoles(AlgalonEncounterState const& state, uint32 now)
{
    return state.bigBangCasting || BigBangDueWithin(state, ULDUAR_ALGALON_KITE_RESERVE_SECONDS, now)
               ? ULDUAR_ALGALON_KITE_RESERVED_HOLES
               : 0;
}

AlgalonScanUnit const* FindScanned(std::vector<AlgalonScanUnit> const& units, ObjectGuid const& guid)
{
    for (AlgalonScanUnit const& unit : units)
        if (unit.guid == guid)
            return &unit;

    return nullptr;
}

Creature* Resolve(Player* bot, ObjectGuid const& guid)
{
    Creature* creature = guid.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(*bot, guid);
    return creature && creature->IsAlive() ? creature : nullptr;
}

bool IsKiteHole(AlgalonEncounterState const& state, AlgalonScanUnit const& hole)
{
    for (auto const& kite : state.kiteHoleAssignments)
        if (kite.second == hole.guid)
            return true;

    return false;
}

AlgalonScanUnit const* NearestShelter(AlgalonEncounterState const& state, Position const& from)
{
    constexpr float kiteHolePenalty = 1000.0f;
    AlgalonScanUnit const* nearest = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    for (AlgalonScanUnit const& hole : state.holes)
    {
        float const distance = from.GetExactDist2d(hole.position) + (IsKiteHole(state, hole) ? kiteHolePenalty : 0.0f);
        if (distance < bestDistance)
        {
            nearest = &hole;
            bestDistance = distance;
        }
    }

    return nearest;
}

AlgalonScanUnit const* ShelterHole(Player* bot, AlgalonEncounterState const& state)
{
    if (state.handler.Get() == bot->GetGUID() && AlgalonHoldsConstellation(bot))
    {
        auto const kite = state.kiteHoleAssignments.find(bot->GetGUID());
        if (kite != state.kiteHoleAssignments.end())
            if (AlgalonScanUnit const* hole = FindScanned(state.holes, kite->second))
                return hole;
    }

    auto const latched = state.shelterAssignments.find(bot->GetGUID());
    if (latched != state.shelterAssignments.end())
        if (AlgalonScanUnit const* hole = FindScanned(state.holes, latched->second))
            return hole;

    return NearestShelter(state, bot->GetPosition());
}

bool IsRaidShelterHole(AlgalonEncounterState const& state, ObjectGuid const& guid, uint32 now)
{
    uint8 const reserved = ReservedHoles(state, now);
    AlgalonScanUnit const* hole = reserved ? FindScanned(state.holes, guid) : nullptr;
    if (!hole)
        return false;

    float const distance = hole->position.GetExactDist2d(ULDUAR_ALGALON_HOLD_SPOT);
    uint8 nearer = 0;
    for (AlgalonScanUnit const& other : state.holes)
    {
        float const otherDistance = other.position.GetExactDist2d(ULDUAR_ALGALON_HOLD_SPOT);
        if (other.guid != guid && (otherDistance < distance || (otherDistance == distance && other.guid < guid)))
            ++nearer;
    }

    return nearer < reserved;
}

struct AlgalonRoomCreatureCheck
{
    bool operator()(Creature* creature) const
    {
        switch (creature->GetEntry())
        {
            case PB_NPC_COLLAPSING_STAR:
            case PB_NPC_BLACK_HOLE:
            case PB_NPC_WORM_HOLE:
            case PB_NPC_LIVING_CONSTELLATION:
            case NPC_ALGALON_ASTEROID_TARGET_1:
            case NPC_ALGALON_ASTEROID_TARGET_2:
            case PB_NPC_UNLEASHED_DARK_MATTER:
                break;
            default:
                return false;
        }

        return creature->IsAlive() &&
               creature->GetExactDist2d(ULDUAR_ALGALON_ROOM_CENTER.GetPositionX(),
                                        ULDUAR_ALGALON_ROOM_CENTER.GetPositionY()) <= ULDUAR_ALGALON_SCAN_RADIUS;
    }
};

void NoteMeteor(Creature* boss, Creature* marker)
{
    if (!RaidObs::Active() || marker->GetEntry() != NPC_ALGALON_ASTEROID_TARGET_2)
        return;

    uint32 const spell =
        boss->GetMap()->Is25ManRaid() ? SPELL_ALGALON_COSMIC_SMASH_DAMAGE_25 : SPELL_ALGALON_COSMIC_SMASH_DAMAGE;
    Position const impact(marker->GetPositionX(), marker->GetPositionY(), ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
    RaidObs::NoteHazardCircle(boss->GetMap(), spell, impact, 6.0f, ULDUAR_ALGALON_MARKER_LIFETIME_MS);
}

void LogStarHealth(AlgalonEncounterState const& state, AlgalonScanUnit& star, uint32 now,
                   std::unordered_map<ObjectGuid, std::vector<std::pair<uint32, float>>>& logs)
{
    std::vector<std::pair<uint32, float>> log;
    auto const previous = state.starHealthLog.find(star.guid);
    if (previous != state.starHealthLog.end())
        log = previous->second;

    if (log.empty() || getMSTimeDiff(log.back().first, now) >= ULDUAR_ALGALON_STAR_LOG_STEP_MS)
        log.emplace_back(now, star.healthPct);

    while (log.size() > 1 && getMSTimeDiff(log.front().first, now) > ULDUAR_ALGALON_STAR_LOG_MS)
        log.erase(log.begin());

    uint32 const span = getMSTimeDiff(log.front().first, now);
    star.lossMeasured = span >= ULDUAR_ALGALON_STAR_LOG_MIN_MS;
    if (star.lossMeasured)
        star.lossPctPerSec =
            std::max(ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC,
                     (log.front().second - star.healthPct) * IN_MILLISECONDS / static_cast<float>(span));

    logs[star.guid] = std::move(log);
}

uint32 PendingPeriodicHp(Creature* star)
{
    uint64 pending = 0;
    for (AuraType const type :
         {SPELL_AURA_PERIODIC_DAMAGE, SPELL_AURA_PERIODIC_LEECH, SPELL_AURA_PERIODIC_DAMAGE_PERCENT})
    {
        for (AuraEffect const* effect : star->GetAuraEffectsByType(type))
        {
            int32 const left = effect->GetTotalTicks() - static_cast<int32>(effect->GetTickNumber());
            if (effect->GetId() == SPELL_ALGALON_COLLAPSE || left <= 0 || effect->GetAmount() <= 0)
                continue;

            uint32 const tick = type == SPELL_AURA_PERIODIC_DAMAGE_PERCENT
                                    ? star->CountPctFromMaxHealth(effect->GetAmount())
                                    : static_cast<uint32>(effect->GetAmount());
            pending += static_cast<uint64>(tick) * left;
        }
    }

    SpellInfo const* explosive = sSpellMgr->GetSpellInfo(SPELL_ALGALON_EXPLOSIVE_SHOT_TICK);
    for (AuraEffect const* effect : star->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DUMMY))
    {
        SpellInfo const* info = effect->GetSpellInfo();
        int32 const left = effect->GetTotalTicks() - static_cast<int32>(effect->GetTickNumber());
        Unit* caster = effect->GetCaster();
        if (!explosive || !caster || left <= 0 || effect->GetAmount() <= 0 ||
            info->SpellFamilyName != SPELLFAMILY_HUNTER ||
            !(info->SpellFamilyFlags[1] & ULDUAR_ALGALON_EXPLOSIVE_SHOT_FAMILY_FLAG_1))
            continue;

        uint32 const done = caster->SpellDamageBonusDone(star, explosive, static_cast<uint32>(effect->GetAmount()),
                                                         SPELL_DIRECT_DAMAGE, EFFECT_0);
        uint32 const tick = star->SpellDamageBonusTaken(caster, explosive, done, SPELL_DIRECT_DAMAGE);
        pending += static_cast<uint64>(tick) * left;
    }

    return static_cast<uint32>(std::min<uint64>(pending, star->GetHealth()));
}

void ScanRoom(Player* bot, Creature* boss, AlgalonEncounterState& state, uint32 now)
{
    std::vector<AlgalonScanUnit> const previousHoles = std::move(state.holes);
    std::vector<AlgalonScanUnit> const previousStars = std::move(state.stars);
    size_t const previousConstellations = state.constellations.size();
    bool const hadWormHoles = state.wormHolesSeen;

    state.stars.clear();
    state.holes.clear();
    state.constellations.clear();
    state.markers.clear();
    state.darkMatter.clear();
    std::unordered_map<ObjectGuid, uint32> markerSeenMs;
    std::unordered_map<ObjectGuid, uint32> starSeenMs;
    std::unordered_map<ObjectGuid, std::vector<std::pair<uint32, float>>> starHealthLog;

    std::list<Creature*> found;
    AlgalonRoomCreatureCheck check;
    Acore::CreatureListSearcher<AlgalonRoomCreatureCheck> searcher(boss, found, check);
    Cell::VisitObjects(ULDUAR_ALGALON_ROOM_CENTER.GetPositionX(), ULDUAR_ALGALON_ROOM_CENTER.GetPositionY(),
                       boss->GetMap(), searcher, ULDUAR_ALGALON_SCAN_RADIUS);

    for (Creature* creature : found)
    {
        AlgalonScanUnit unit;
        unit.guid = creature->GetGUID();
        unit.position = creature->GetPosition();
        Unit* victim = creature->GetVictim();
        unit.victim = victim ? victim->GetGUID() : ObjectGuid::Empty;
        unit.healthPct = creature->GetHealthPct();
        unit.active = !creature->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);

        switch (creature->GetEntry())
        {
            case PB_NPC_COLLAPSING_STAR:
            {
                auto const seen = state.starSeenMs.find(unit.guid);
                unit.firstSeenMs = seen != state.starSeenMs.end() ? seen->second : now;
                starSeenMs[unit.guid] = unit.firstSeenMs;
                unit.health = creature->GetHealth();
                unit.pendingHp = PendingPeriodicHp(creature);
                unit.tickHp = std::max<uint32>(1, creature->CountPctFromMaxHealth(1));
                if (AuraEffect const* drain = creature->GetAuraEffect(SPELL_ALGALON_COLLAPSE, EFFECT_0))
                    unit.nextTickMs = std::max(0, drain->GetPeriodicTimer());
                unit.under = state.underStars.count(unit.guid) > 0;
                LogStarHealth(state, unit, now, starHealthLog);
                state.stars.push_back(unit);
                break;
            }
            case PB_NPC_WORM_HOLE:
                state.wormHolesSeen = true;
                [[fallthrough]];
            case PB_NPC_BLACK_HOLE:
                state.holes.push_back(unit);
                break;
            case PB_NPC_LIVING_CONSTELLATION:
                unit.active = unit.active && creature->AI() && creature->AI()->GetData(0);
                state.constellations.push_back(unit);
                break;
            case PB_NPC_UNLEASHED_DARK_MATTER:
                state.darkMatter.push_back(unit);
                break;
            default:
            {
                auto const seen = state.markerSeenMs.find(unit.guid);
                if (seen == state.markerSeenMs.end())
                {
                    NoteMeteor(boss, creature);
                    state.nextSmashImpactMs =
                        now + ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_MS + ULDUAR_ALGALON_COSMIC_SMASH_INTERVAL_MS;
                }

                uint32 const seenMs = seen != state.markerSeenMs.end() ? seen->second : now;
                markerSeenMs[unit.guid] = seenMs;
                if (getMSTimeDiff(seenMs, now) <= ULDUAR_ALGALON_MARKER_LIFETIME_MS)
                    state.markers.push_back(unit);
                break;
            }
        }
    }

    state.markerSeenMs = std::move(markerSeenMs);
    state.starSeenMs = std::move(starSeenMs);
    state.starHealthLog = std::move(starHealthLog);

    bool const phaseTwoStarted = state.wormHolesSeen && !hadWormHoles;
    auto const gone = [&state](AlgalonScanUnit const& star) { return !FindScanned(state.stars, star.guid); };
    bool const starDied = std::any_of(previousStars.begin(), previousStars.end(), gone);
    if (starDied && !phaseTwoStarted)
        state.lastStarDeathMs = now;

    bool const despawnedAll = !previousHoles.empty() && state.holes.empty() && state.stars.empty() &&
                              state.constellations.empty() && (!previousStars.empty() || previousConstellations);
    if (!RaidObs::Active() || despawnedAll)
        return;

    if (!phaseTwoStarted)
    {
        for (AlgalonScanUnit const& star : previousStars)
        {
            if (FindScanned(state.stars, star.guid))
                continue;

            float const drainedTo = 100.0f - static_cast<float>(getMSTimeDiff(star.firstSeenMs, now)) / IN_MILLISECONDS;
            char const* cause = "chewed";
            if (star.under)
            {
                int32 const sinceFrom = static_cast<int32>(now - state.underFromMs);
                int32 const sinceTo = static_cast<int32>(now - state.underToMs);
                cause = sinceFrom < 0 ? "early" : (sinceTo > 0 ? "late" : "under");
            }
            else if (star.guid == state.focusStar.Get())
                cause = "killed";
            else if (star.healthPct >= drainedTo - ULDUAR_ALGALON_STAR_DRAIN_SLACK_PCT)
                cause = "drained";

            RaidObs::Note(bot, "algalon.stardeath",
                          std::string(cause) + " " + DescribeYards(NearestRaidMember(bot->GetGroup(), star.position)));
        }
    }

    for (AlgalonScanUnit const& hole : previousHoles)
    {
        if (FindScanned(state.holes, hole.guid))
            continue;

        char const* reason = "stray";
        if (IsKiteHole(state, hole))
            reason = "kite";
        else if (phaseTwoStarted)
            reason = "phase2";
        else if (state.bigBangCasting)
            reason = "bigbang";

        RaidObs::Note(bot, "algalon.holelost", reason);
    }
}

Player* FindTracker(Group* group)
{
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (InFight(member) && GET_PLAYERBOT_AI(member))
            return member;
    }

    return nullptr;
}

std::vector<Player*> OrderedTanks(Player* bot)
{
    std::vector<Player*> tanks;
    auto add = [&tanks](Player* tank)
    {
        if (tank && std::find(tanks.begin(), tanks.end(), tank) == tanks.end())
            tanks.push_back(tank);
    };

    add(GetGroupMainTank(bot));
    for (uint8 index = 0; index < 3; ++index)
        add(GetGroupAssistTank(bot, index));

    return tanks;
}

std::vector<Player*> WantedSwapPair(std::vector<Player*> const& tanks)
{
    std::vector<Player*> bots;
    std::copy_if(tanks.begin(), tanks.end(), std::back_inserter(bots), IsBot);
    std::vector<Player*> pair = bots.size() >= 2 ? bots : tanks;
    if (pair.size() > 2)
        pair.resize(2);

    return pair;
}

void EnsureSwapPair(Player* bot, Creature* boss, AlgalonEncounterState& state, std::vector<Player*> const& tanks)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    bool const fighting = boss->IsInCombat();
    std::vector<ObjectGuid> leaving;
    for (ObjectGuid const& guid : state.swapPair)
    {
        Player* member = FindGroupMember(group, guid);
        if (!member || (fighting && !InFight(member)))
            leaving.push_back(guid);
    }

    std::vector<Player*> const wanted = WantedSwapPair(tanks);
    if (!fighting)
    {
        for (ObjectGuid const& guid : state.swapPair)
        {
            auto const same = [&guid](Player* tank) { return tank->GetGUID() == guid; };
            if (std::none_of(wanted.begin(), wanted.end(), same))
                leaving.push_back(guid);
        }
    }

    for (ObjectGuid const& guid : leaving)
        state.swapPair.erase(guid);

    std::vector<Player*> candidates = wanted;
    if (fighting)
    {
        candidates.clear();
        std::copy_if(tanks.begin(), tanks.end(), std::back_inserter(candidates), IsBot);
        std::copy_if(tanks.begin(), tanks.end(), std::back_inserter(candidates),
                     [](Player* tank) { return !IsBot(tank); });
    }

    for (Player* tank : candidates)
    {
        if (state.swapPair.size() >= 2)
            break;

        if (InFight(tank))
            state.swapPair.insert(tank->GetGUID());
    }

    if (RaidObs::Active() && !state.swapPair.empty())
    {
        ObjectGuid const first = *state.swapPair.begin();
        state.swapPair.insert(first);
    }
}

uint8 TankHealerRank(Player* member)
{
    switch (member->getClass())
    {
        case CLASS_PALADIN:
            return 0;
        case CLASS_PRIEST:
            return AiFactory::GetPlayerSpecTab(member) == PRIEST_TAB_DISCIPLINE ? 1 : 4;
        case CLASS_DRUID:
            return 2;
        case CLASS_SHAMAN:
            return 3;
        default:
            return 5;
    }
}

bool BetterTankHealer(Player* a, Player* b)
{
    uint8 const aRank = TankHealerRank(a);
    uint8 const bRank = TankHealerRank(b);
    return aRank != bRank ? aRank < bRank : a->GetGUID() < b->GetGUID();
}

uint8 TankHealerSlots(Player* bot, Group* group, bool stars)
{
    uint8 healers = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (InFight(member) && PlayerbotAI::IsHeal(member))
            ++healers;
    }

    uint8 const wanted =
        bot->GetMap()->Is25ManRaid() ? ULDUAR_ALGALON_TANK_HEALERS_25 : ULDUAR_ALGALON_TANK_HEALERS_10;
    return std::min<uint8>(wanted, stars ? healers / 2 : std::max(1, healers - 1));
}

void EnsureTankHealers(Player* bot, Creature* boss, AlgalonEncounterState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    uint8 const slots = TankHealerSlots(bot, group, state.phase.Get() != AlgalonPhase::Two);
    state.tankHealerSlots = slots;

    std::vector<Player*> candidates;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (InFight(member) && IsBot(member) && PlayerbotAI::IsHeal(member))
            candidates.push_back(member);
    }

    std::sort(candidates.begin(), candidates.end(), BetterTankHealer);
    if (!boss->IsInCombat() && candidates.size() > slots)
        candidates.resize(slots);

    std::vector<ObjectGuid> leaving;
    for (ObjectGuid const& guid : state.tankHealers)
    {
        auto const same = [&guid](Player* healer) { return healer->GetGUID() == guid; };
        if (std::none_of(candidates.begin(), candidates.end(), same))
            leaving.push_back(guid);
    }

    for (ObjectGuid const& guid : leaving)
        state.tankHealers.erase(guid);

    for (Player* candidate : candidates)
    {
        if (state.tankHealers.size() >= slots)
            break;

        state.tankHealers.insert(candidate->GetGUID());
    }

    while (state.tankHealers.size() > slots)
    {
        Player* worst = nullptr;
        for (ObjectGuid const& guid : state.tankHealers)
        {
            Player* healer = FindGroupMember(group, guid);
            if (healer && (!worst || BetterTankHealer(worst, healer)))
                worst = healer;
        }

        if (!worst)
            break;

        state.tankHealers.erase(worst->GetGUID());
    }

    if (RaidObs::Active() && !state.tankHealers.empty())
    {
        ObjectGuid const first = *state.tankHealers.begin();
        state.tankHealers.insert(first);
    }
}

bool TankHealReachable(Player* bot, Player* target)
{
    return CanStayOut(target) && bot->GetDistance2d(target) <= sPlayerbotAIConfig.HealDistance &&
           bot->IsWithinLOSInMap(target);
}

uint32 CastTimeMs(PlayerbotAI* botAI, char const* spell)
{
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return info ? info->CalcCastTime(botAI->GetBot()) : 0;
}

Player* PairPartner(Group* group, AlgalonEncounterState const& state, Player* tank)
{
    for (ObjectGuid const& guid : state.swapPair)
        if (guid != tank->GetGUID())
            return FindGroupMember(group, guid);

    return nullptr;
}

bool DefensiveHandoverDue(AlgalonEncounterState const& state, Player* holder, Player* taker, uint32 now)
{
    if (!state.disperser.Get().IsEmpty() || !IsBot(holder) || !IsBot(taker) ||
        GetAlgalonPhasePunchStacks(taker) > ULDUAR_ALGALON_HANDOVER_PARTNER_STACKS ||
        !BigBangDueWithin(state, ULDUAR_ALGALON_EARLY_SWAP_SECONDS, now))
        return false;

    int32 const untilNeeded = static_cast<int32>(state.nextBigBangMs - now) +
                              static_cast<int32>(ULDUAR_ALGALON_BIG_BANG_CAST_MS) - ULDUAR_ALGALON_SOAK_DEFENSIVE_MS;
    uint32 const within = static_cast<uint32>(std::max(0, untilNeeded));
    return !ReadyTankDefensive(holder, true, within) && ReadyTankDefensive(taker, true, within);
}

bool SwapDue(AlgalonEncounterState const& state, Player* holder, Player* taker, uint32 now)
{
    uint8 const own = GetAlgalonPhasePunchStacks(taker);
    uint8 const theirs = GetAlgalonPhasePunchStacks(holder);
    if (own >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
        return false;

    if (theirs >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS || DefensiveHandoverDue(state, holder, taker, now))
        return true;

    return state.disperser.Get().IsEmpty() && theirs >= ULDUAR_ALGALON_EARLY_SWAP_STACKS &&
           own <= ULDUAR_ALGALON_EARLY_SWAP_PARTNER_STACKS &&
           BigBangDueWithin(state, ULDUAR_ALGALON_EARLY_SWAP_SECONDS, now);
}

void SendAlgalonCall(Player* sender, char const* kind, std::string const& text)
{
    PlayerbotAI* senderAI = GET_PLAYERBOT_AI(sender);
    if (!senderAI)
        return;

    std::vector<Player*> const humans = senderAI->GetRealPlayersInGroup();
    if (humans.empty())
        return;

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_WARNING, LANG_UNIVERSAL, sender->GetGUID(), {}, text,
                                 CHAT_TAG_NONE, sender->GetName());
    for (Player* human : humans)
        ServerFacade::instance().SendPacket(human, &data);

    if (RaidObs::Active())
        RaidObs::Note(sender, "algalon.call", kind);
}

Player* BossHolder(Creature* boss)
{
    Unit* victim = boss->GetVictim();
    return victim ? victim->ToPlayer() : nullptr;
}

void CallHumanTank(Group* group, PlayerbotAI* botAI, Creature* boss, AlgalonEncounterState& state, uint32 now)
{
    if (state.bigBangCasting ||
        (state.lastTankCallMs && getMSTimeDiff(state.lastTankCallMs, now) < ULDUAR_ALGALON_TANK_CALL_MS))
        return;

    Player* holder = BossHolder(boss);
    if (!holder || !state.swapPair.count(holder->GetGUID()))
    {
        Player* pickup = GetAlgalonPickupTank(botAI);
        Player* sender = FindTracker(group);
        if (!pickup || IsBot(pickup) || !sender)
            return;

        std::string const on = holder ? " he is on " + holder->GetName() : " he is loose";
        SendAlgalonCall(sender, "loose", pickup->GetName() + ", taunt Algalon now:" + on);
        state.lastTankCallMs = now;
        return;
    }

    if (!IsBot(holder) || !CanStayOut(holder))
        return;

    Player* partner = PairPartner(group, state, holder);
    if (!partner || IsBot(partner) || !CanStayOut(partner) || !SwapDue(state, holder, partner, now))
        return;

    SendAlgalonCall(holder, "swap",
                    partner->GetName() + ", taunt Algalon now: " + holder->GetName() + " has " +
                        std::to_string(GetAlgalonPhasePunchStacks(holder)) + " Phase Punch stacks");
    state.lastTankCallMs = now;
}

void CallBigBang(Group* group, Creature* boss, AlgalonEncounterState& state, bool castStarted)
{
    Player* tracker = FindTracker(group);
    if (!tracker)
        return;

    Player* soaker = FindGroupMember(group, state.bigBangSoaker.Get());
    if (castStarted)
    {
        state.bigBangHideCalled = false;
        char const* const kit =
            soaker && soaker->HasSpell(SPELL_ALGALON_DISPERSION) ? " with Dispersion" : " with a defensive";
        std::string stays =
            soaker ? soaker->GetName() + " stays out" + kit + ". Everyone else" : std::string("Everyone");
        Player* holder = BossHolder(boss);
        if (soaker && holder && holder != soaker && !IsBot(holder) && state.swapPair.count(holder->GetGUID()))
            stays = holder->GetName() + ", hide too: " + soaker->GetName() + " takes it. Everyone else";

        SendAlgalonCall(tracker, "bigbang", "Big Bang in 8 s: " + stays + ", get next to a black hole");
        return;
    }

    Spell* cast = CurrentBigBang(boss);
    if (state.bigBangHideCalled || !cast || cast->GetCastTimeRemaining() > ULDUAR_ALGALON_HIDE_CALL_MS)
        return;

    state.bigBangHideCalled = true;
    std::string const who = soaker ? "everyone but " + soaker->GetName() + " into" : std::string("into");
    SendAlgalonCall(tracker, "hide", "Big Bang: " + who + " a black hole NOW and stay in it until it lands");
}

void NoteTankRoles(Group* group, Creature* boss, AlgalonEncounterState const& state, uint32 now)
{
    if (!RaidObs::Active())
        return;

    Player* holder = BossHolder(boss);
    for (ObjectGuid const& guid : state.swapPair)
    {
        Player* tank = FindGroupMember(group, guid);
        if (!IsBot(tank))
            continue;

        char const* role = "none";
        if (!CanStayOut(tank))
            role = "out";
        else if (tank == holder)
        {
            Player* partner = PairPartner(group, state, tank);
            bool const calling =
                partner && !IsBot(partner) && CanStayOut(partner) && SwapDue(state, tank, partner, now);
            role = calling ? "call" : "hold";
        }
        else if (AlgalonShouldPickUp(tank) || AlgalonShouldSwap(tank))
            role = "take";
        else if (AlgalonYieldsBoss(tank))
            role = "yield";

        RaidObs::NoteDerived(tank, "algalon.tankrole", role);
    }
}

void NotePull(Group* group, Creature* boss, AlgalonEncounterState const& state)
{
    Player* tracker = RaidObs::Active() ? FindTracker(group) : nullptr;
    if (!tracker)
        return;

    RaidObs::LogHitsOn(tracker, PB_NPC_COLLAPSING_STAR);

    uint32 inReach = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (CanStayOut(member) && !state.swapPair.count(member->GetGUID()) &&
            boss->GetExactDist2d(member) <= ULDUAR_ALGALON_MELEE_REACH)
            ++inReach;
    }

    Unit* victim = boss->GetVictim();
    RaidObs::Note(tracker, "algalon.pull",
                  "inreach=" + std::to_string(inReach) +
                      " victim=" + RaidObs::DescribeAssignment(victim ? victim->GetGUID() : ObjectGuid::Empty));
}

void NoteHandover(Group* group, Creature* boss, AlgalonEncounterState& state, uint32 now)
{
    Player* holder = BossHolder(boss);
    uint32 const cycle = state.bigBangCount + 1;
    if (!RaidObs::Active() || !holder || !state.swapPair.count(holder->GetGUID()) || state.handoverNoted == cycle)
        return;

    Player* partner = PairPartner(group, state, holder);
    if (!partner || !DefensiveHandoverDue(state, holder, partner, now))
        return;

    RaidObs::Note(holder, "algalon.handover", "nodef");
    state.handoverNoted = cycle;
}

void NoteSoakKit(Group* group, AlgalonEncounterState const& state)
{
    Player* soaker = RaidObs::Active() ? FindGroupMember(group, state.bigBangSoaker.Get()) : nullptr;
    if (!soaker)
        return;

    char const* ready =
        soaker->HasSpell(SPELL_ALGALON_DISPERSION) &&
                soaker->GetSpellCooldownDelay(SPELL_ALGALON_DISPERSION) <=
                    static_cast<uint32>(ULDUAR_ALGALON_BIG_BANG_CAST_MS - ULDUAR_ALGALON_SOAK_DISPERSION_MS)
            ? "dispersion"
            : ReadyTankDefensive(soaker, true, ULDUAR_ALGALON_BIG_BANG_CAST_MS - ULDUAR_ALGALON_SOAK_DEFENSIVE_MS);
    RaidObs::Note(soaker, "algalon.soakkit",
                  std::string("def=") + (ready ? ready : "none") +
                      " forbearance=" + (soaker->HasAura(SPELL_ALGALON_FORBEARANCE) ? "1" : "0"));
}

void ReleaseMelee(Creature* boss, AlgalonEncounterState& state, uint32 now)
{
    if (state.meleeReleased || !state.introEndMs)
        return;

    uint32 const sinceIntro = getMSTimeDiff(state.introEndMs, now);
    Player* holder = BossHolder(boss);
    bool const held = holder && state.swapPair.count(holder->GetGUID());
    bool const onSpot =
        held && holder->GetExactDist2d(ULDUAR_ALGALON_TANK_SLOT) <= ULDUAR_ALGALON_HOLDER_ON_SPOT_RADIUS;
    if (onSpot || (held && sinceIntro >= ULDUAR_ALGALON_MELEE_RELEASE_MS) ||
        sinceIntro >= ULDUAR_ALGALON_MELEE_RELEASE_ANYWAY_MS)
        state.meleeReleased = true;
}

Player* ElectDisperser(Group* group, int32 castByMs, Player* except)
{
    Player* best = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member == except || !CanStayOut(member) || !IsBot(member) || !member->HasSpell(SPELL_ALGALON_DISPERSION) ||
            static_cast<int64>(member->GetSpellCooldownDelay(SPELL_ALGALON_DISPERSION)) > castByMs)
            continue;

        if (!best || member->GetGUID() < best->GetGUID())
            best = member;
    }

    return best;
}

Player* ElectBackup(Group* group, Player* soaker)
{
    Player* best = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member == soaker || !CanStayOut(member) || !GET_PLAYERBOT_AI(member) || !AlgalonCanDisperse(member))
            continue;

        if (!best || member->GetGUID() < best->GetGUID())
            best = member;
    }

    return best;
}

bool SoakCovered(Player* tank, uint32 hitMs)
{
    uint32 const castBy = hitMs > static_cast<uint32>(ULDUAR_ALGALON_SOAK_DEFENSIVE_MS)
                              ? hitMs - static_cast<uint32>(ULDUAR_ALGALON_SOAK_DEFENSIVE_MS)
                              : 0;
    return ReadyTankDefensive(tank, true, castBy) || RunningTankDefensive(tank, true, hitMs);
}

void StopRunIntoHole(Player* tank, AlgalonEncounterState const& state)
{
    PlayerbotAI* tankAI = GET_PLAYERBOT_AI(tank);
    if (!tankAI)
        return;

    LastMovement& last = tankAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (!last.msTime)
        return;

    for (AlgalonScanUnit const& hole : state.holes)
    {
        if (hole.position.GetExactDist2d(last.lastMoveToX, last.lastMoveToY) <= ULDUAR_ALGALON_SHELTER_RADIUS)
        {
            tank->StopMoving();
            last.clear();
            return;
        }
    }
}

Player* SoakingPartner(Group* group, AlgalonEncounterState const& state, Player* tank, uint8 maxStacks)
{
    for (ObjectGuid const& guid : state.swapPair)
    {
        Player* partner = FindGroupMember(group, guid);
        if (partner && partner != tank && IsBot(partner) && CanStayOut(partner) &&
            GetAlgalonPhasePunchStacks(partner) <= maxStacks)
            return partner;
    }

    return nullptr;
}

void ElectBigBangSoakers(Player* bot, Creature* boss, AlgalonEncounterState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    Spell* cast = CurrentBigBang(boss);
    int32 const remaining =
        cast ? static_cast<int32>(cast->GetCastTimeRemaining()) : static_cast<int32>(ULDUAR_ALGALON_BIG_BANG_CAST_MS);
    int32 const castBy = remaining - ULDUAR_ALGALON_SOAK_DISPERSION_MS;
    if (Player* disperser = ElectDisperser(group, castBy, nullptr))
    {
        Player* backup = ElectDisperser(group, castBy, disperser);
        state.bigBangSoaker = disperser->GetGUID();
        state.bigBangBackup = backup ? backup->GetGUID() : ObjectGuid::Empty;
        return;
    }

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;

    Player* soaker = nullptr;
    if (CanStayOut(holder) && IsAlgalonSwapTank(holder))
    {
        soaker = holder;
        if (!IsBot(holder) && !SoakCovered(holder, ULDUAR_ALGALON_BIG_BANG_CAST_MS))
        {
            Player* partner = SoakingPartner(group, state, holder, ULDUAR_ALGALON_HANDOVER_PARTNER_STACKS);
            if (partner && SoakCovered(partner, ULDUAR_ALGALON_BIG_BANG_CAST_MS))
                soaker = partner;
        }
    }

    if (!soaker && botAI)
        soaker = GetAlgalonPickupTank(botAI);

    if (!soaker && CanStayOut(holder))
        soaker = holder;

    Player* backup = ElectBackup(group, soaker);
    state.bigBangSoaker = soaker ? soaker->GetGUID() : ObjectGuid::Empty;
    state.bigBangBackup = backup ? backup->GetGUID() : ObjectGuid::Empty;
}

void KeepBigBangSoaker(Player* bot, AlgalonEncounterState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    Player* soaker = FindGroupMember(group, state.bigBangSoaker);
    Player* backup = FindGroupMember(group, state.bigBangBackup);
    if (!CanStayOut(soaker))
    {
        bool const priestLost =
            soaker ? soaker->HasSpell(SPELL_ALGALON_DISPERSION) : !state.disperser.Get().IsEmpty();
        bool const backupFirst = priestLost && CanStayOut(backup);
        if (Player* partner = backupFirst ? nullptr
                                          : SoakingPartner(group, state, soaker,
                                                           ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS - 1))
        {
            soaker = partner;
            state.bigBangSoaker = soaker->GetGUID();
            StopRunIntoHole(partner, state);
        }
        else if (CanStayOut(backup))
        {
            soaker = backup;
            backup = nullptr;
            state.bigBangSoaker = soaker->GetGUID();
        }
    }

    if (!CanStayOut(backup))
    {
        backup = ElectBackup(group, soaker);
        state.bigBangBackup = backup ? backup->GetGUID() : ObjectGuid::Empty;
    }
}

Player* ElectHandler(Player* bot, AlgalonEncounterState const& state, std::vector<Player*> const& tanks)
{
    for (Player* tank : tanks)
        if (InFight(tank) && IsBot(tank) && !state.swapPair.count(tank->GetGUID()))
            return tank;

    Group* group = bot->GetGroup();
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!group || !botAI)
        return nullptr;

    Player* holder = GetAlgalonBossTank(botAI);
    Player* keeper = holder && state.swapPair.count(holder->GetGUID()) ? holder : GetAlgalonPickupTank(botAI);
    for (ObjectGuid const& guid : state.swapPair)
    {
        Player* tank = FindGroupMember(group, guid);
        if (tank != keeper && InFight(tank) && IsBot(tank))
            return tank;
    }

    return nullptr;
}

bool StarTeamReinforced(Player* bot, AlgalonEncounterState const& state)
{
    if (!bot->GetMap()->Is25ManRaid() || state.bigBangCasting || !state.lastBigBangEndMs ||
        state.bigBangCount + 1 != ULDUAR_ALGALON_STAR_TEAM_EXTRA_BIG_BANG)
        return false;

    return std::any_of(state.stars.begin(), state.stars.end(), [&state](AlgalonScanUnit const& star)
                       { return static_cast<int32>(star.firstSeenMs - state.lastBigBangEndMs) > 0; });
}

void ElectStarTeam(Player* bot, AlgalonEncounterState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    bool const reinforced = StarTeamReinforced(bot, state);
    std::vector<ObjectGuid> leaving;
    for (ObjectGuid const& guid : state.starTeam)
    {
        Player* member = FindGroupMember(group, guid);
        if (!InFight(member) || !IsDamageDealer(member) || (!reinforced && state.starTeamExtras.count(guid)))
            leaving.push_back(guid);
    }

    for (ObjectGuid const& guid : leaving)
    {
        state.starTeam.erase(guid);
        state.starTeamExtras.erase(guid);
    }

    size_t const core =
        bot->GetMap()->Is25ManRaid() ? ULDUAR_ALGALON_STAR_TEAM_25 : ULDUAR_ALGALON_STAR_TEAM_10;
    size_t const extras = reinforced ? ULDUAR_ALGALON_STAR_TEAM_EXTRA_25 : 0;
    size_t const size = core + extras;
    if (state.starTeam.size() >= size)
        return;

    std::vector<Player*> candidates;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (InFight(member) && IsDamageDealer(member) && GET_PLAYERBOT_AI(member) &&
            !state.starTeam.count(member->GetGUID()))
            candidates.push_back(member);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](Player* a, Player* b)
              {
                  bool const aRanged = IsRangedDps(a);
                  bool const bRanged = IsRangedDps(b);
                  return aRanged != bRanged ? aRanged : a->GetGUID() < b->GetGUID();
              });

    for (Player* candidate : candidates)
    {
        if (state.starTeam.size() >= size)
            break;

        if (state.starTeam.size() - state.starTeamExtras.size() < core)
            state.starTeam.insert(candidate->GetGUID());
        else if (state.starTeamExtras.size() < extras && IsRangedDps(candidate))
        {
            state.starTeam.insert(candidate->GetGUID());
            state.starTeamExtras.insert(candidate->GetGUID());
        }
    }
}

float TeamSpeed(AlgalonEncounterState const& state, float measured, uint32 measuredTeam)
{
    if (measured <= ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC || !measuredTeam || state.starTeam.empty())
        return measured;

    return ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC + (measured - ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC) *
                                                       static_cast<float>(state.starTeam.size()) /
                                                       static_cast<float>(measuredTeam);
}

bool StarClearOfTanks(AlgalonEncounterState const& state, Group* group, Unit* boss, AlgalonScanUnit const& star,
                      bool bossRing)
{
    if (bossRing && boss && boss->GetExactDist2d(star.position) < ULDUAR_ALGALON_STAR_BOSS_CLEARANCE)
        return false;

    for (ObjectGuid const& guid : state.swapPair)
    {
        Player* tank = FindGroupMember(group, guid);
        if (CanStayOut(tank) && tank->GetExactDist2d(star.position) < ULDUAR_ALGALON_STAR_TANK_CLEARANCE)
            return false;
    }

    Player* handler = FindGroupMember(group, state.handler.Get());
    return !CanStayOut(handler) || handler->GetExactDist2d(star.position) >= ULDUAR_ALGALON_STAR_TANK_CLEARANCE;
}

enum class StarFilter
{
    Any,
    Tanks,
    TanksAndRing,
    Clear,
};

bool StarPassesFilter(AlgalonEncounterState const& state, Group* group, Unit* boss, AlgalonScanUnit const& star,
                      StarFilter filter)
{
    if (filter == StarFilter::Clear && NearestRaidMember(group, star.position) < ULDUAR_ALGALON_STAR_CLEAR_RADIUS)
        return false;

    return filter == StarFilter::Any || StarClearOfTanks(state, group, boss, star, filter != StarFilter::Tanks);
}

AlgalonScanUnit const* LowestStar(AlgalonEncounterState const& state)
{
    AlgalonScanUnit const* lowest = nullptr;
    for (AlgalonScanUnit const& star : state.stars)
        if (!lowest || lowest->under > star.under ||
            (lowest->under == star.under && star.healthPct < lowest->healthPct))
            lowest = &star;

    return lowest;
}

float TeamWalkSeconds(AlgalonEncounterState const& state, Group* group, AlgalonScanUnit const& star)
{
    float total = 0.0f;
    uint32 count = 0;
    for (ObjectGuid const& guid : state.starTeam)
    {
        Player* member = FindGroupMember(group, guid);
        if (!CanStayOut(member))
            continue;

        total += member->GetExactDist2d(star.position);
        ++count;
    }

    if (!count)
        return 0.0f;

    float const mean = total / static_cast<float>(count);
    return std::max(0.0f, mean - ULDUAR_ALGALON_STAR_TEAM_REACH) / ULDUAR_ALGALON_RUN_SPEED;
}

struct StarSpan
{
    int32 from;
    int32 to;
};

int32 MsUntil(uint32 when, uint32 now) { return static_cast<int32>(when - now); }

int32 DrainDeathMs(AlgalonScanUnit const& star)
{
    uint32 const ticks = (star.health + star.tickHp - 1) / star.tickHp;
    return star.nextTickMs + (static_cast<int32>(ticks) - 1) * static_cast<int32>(IN_MILLISECONDS);
}

int32 StarDeathAt(AlgalonScanUnit const& star, float lossPctPerSec)
{
    return static_cast<int32>(star.healthPct / lossPctPerSec * IN_MILLISECONDS);
}

int32 DroppedDeathMs(AlgalonScanUnit const& star)
{
    float const carry = std::min(star.lossPctPerSec, ULDUAR_ALGALON_STAR_DROPPED_PCT_PER_SEC);
    float const carried = carry * ULDUAR_ALGALON_STAR_DROP_CARRY_MS / IN_MILLISECONDS;
    if (star.healthPct <= carried)
        return StarDeathAt(star, carry);

    return static_cast<int32>(ULDUAR_ALGALON_STAR_DROP_CARRY_MS) +
           static_cast<int32>((star.healthPct - carried) / ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC * IN_MILLISECONDS);
}

struct StarBlackout
{
    StarSpan span;
    char const* label;
};

struct MergedBlackout
{
    StarSpan span;
    std::string label;
};

struct StarPlan
{
    std::vector<StarBlackout> blackouts;
    std::vector<MergedBlackout> merged;
    bool hadDeath = false;
    int32 lastDeath = 0;
    std::vector<std::pair<AlgalonScanUnit const*, int32>> deaths;
};

StarPlan PlanStars(AlgalonEncounterState const& state, uint32 now)
{
    StarPlan plan;

    if (state.lastBigBangEndMs)
    {
        int32 const tail =
            MsUntil(state.lastBigBangEndMs, now) + static_cast<int32>(ULDUAR_ALGALON_STAR_BIG_BANG_TAIL_MS);
        if (tail > 0)
            plan.blackouts.push_back({{0, tail}, "prebang"});
    }

    if (state.nextBigBangMs && !state.bigBangCasting)
    {
        int32 const cast = MsUntil(state.nextBigBangMs, now);
        plan.blackouts.push_back({{cast - static_cast<int32>(ULDUAR_ALGALON_STAR_BIG_BANG_LEAD_MS),
                                   cast + static_cast<int32>(ULDUAR_ALGALON_BIG_BANG_CAST_MS +
                                                             ULDUAR_ALGALON_STAR_BIG_BANG_TAIL_MS)},
                                  "prebang"});
    }

    for (auto const& [marker, seenMs] : state.markerSeenMs)
    {
        int32 const impact = MsUntil(seenMs + ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_MS, now);
        if (impact + static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_TAIL_MS) < 0)
            continue;

        plan.blackouts.push_back({{impact - static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_LEAD_MS),
                                   impact + static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_TAIL_MS)},
                                  "smash"});
    }

    if (state.nextSmashImpactMs)
    {
        for (uint8 i = 0; i < ULDUAR_ALGALON_SMASH_FORECAST; ++i)
        {
            int32 const impact = MsUntil(state.nextSmashImpactMs, now) +
                                 i * static_cast<int32>(ULDUAR_ALGALON_COSMIC_SMASH_INTERVAL_MS);
            plan.blackouts.push_back({{impact - static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_LEAD_MS),
                                       impact + static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_TAIL_MS)},
                                      "smash"});
        }
    }

    std::vector<StarBlackout> sorted = plan.blackouts;
    std::sort(sorted.begin(), sorted.end(),
              [](StarBlackout const& a, StarBlackout const& b) { return a.span.from < b.span.from; });
    for (StarBlackout const& blackout : sorted)
    {
        if (blackout.span.to < 0)
            continue;

        if (plan.merged.empty() || blackout.span.from > plan.merged.back().span.to)
        {
            plan.merged.push_back({blackout.span, blackout.label});
            continue;
        }

        MergedBlackout& last = plan.merged.back();
        last.span.to = std::max(last.span.to, blackout.span.to);
        if (last.label.find(blackout.label) == std::string::npos)
            last.label += std::string("+") + blackout.label;
    }

    plan.hadDeath = state.lastStarDeathMs != 0;
    plan.lastDeath = plan.hadDeath ? MsUntil(state.lastStarDeathMs, now) : 0;
    for (AlgalonScanUnit const& star : state.stars)
        if (!star.under)
            plan.deaths.emplace_back(&star, StarDeathAt(star, star.lossPctPerSec));

    return plan;
}

int32 ForecastDeath(StarPlan const& plan, AlgalonScanUnit const* star)
{
    for (auto const& [candidate, death] : plan.deaths)
        if (candidate == star)
            return death;

    return 0;
}

char const* StarCollision(StarPlan const& plan, StarSpan when, AlgalonScanUnit const* self, int32 separation,
                          bool meteors = true)
{
    for (StarBlackout const& blackout : plan.blackouts)
        if ((meteors || std::strcmp(blackout.label, "smash")) && when.from <= blackout.span.to &&
            when.to >= blackout.span.from)
            return blackout.label;

    if (plan.hadDeath && when.from < plan.lastDeath + separation)
        return "gap";

    for (auto const& [star, death] : plan.deaths)
        if (star != self && when.from < death + separation && when.to > death - separation)
            return "spacing";

    return nullptr;
}

bool InSmashRecovery(StarPlan const& plan, AlgalonEncounterState const& state, uint32 now, bool raidLow, int32 finish)
{
    int32 const recovery = static_cast<int32>(ULDUAR_ALGALON_STAR_SMASH_RECOVERY_MS);
    if (raidLow && state.lastSmashImpactMs && finish < MsUntil(state.lastSmashImpactMs, now) + recovery)
        return true;

    int32 const tail = static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_TAIL_MS);
    int32 const landing = static_cast<int32>(ULDUAR_ALGALON_MARKER_LIFETIME_MS - ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_MS);
    for (StarBlackout const& blackout : plan.blackouts)
    {
        int32 const impact = blackout.span.to - tail;
        if (!std::strcmp(blackout.label, "smash") && impact > -landing && finish >= blackout.span.to &&
            finish < impact + recovery)
            return true;
    }

    return false;
}

int32 StarSeparation(StarPlan const& plan, StarSpan when, AlgalonScanUnit const* self)
{
    for (StarBlackout const& blackout : plan.blackouts)
        if (when.from <= blackout.span.to && when.to >= blackout.span.from)
            return -1;

    auto const away = [&when](int32 death)
    { return death < when.from ? when.from - death : (death > when.to ? death - when.to : 0); };

    int32 nearest = std::numeric_limits<int32>::max();
    if (plan.hadDeath)
        nearest = away(plan.lastDeath);

    for (auto const& [star, death] : plan.deaths)
        if (star != self)
            nearest = std::min(nearest, away(death));

    return nearest;
}

StarSpan FinishBand(AlgalonEncounterState const& state, Group* group, AlgalonScanUnit const& star)
{
    int32 const walk = static_cast<int32>(TeamWalkSeconds(state, group, star) * IN_MILLISECONDS);
    bool const hit = star.lossMeasured && star.lossPctPerSec >= ULDUAR_ALGALON_STAR_KILL_MEASURED_PCT_PER_SEC;
    if (hit && star.guid == state.focusStar.Get())
    {
        int32 const finish = walk + StarDeathAt(star, star.lossPctPerSec);
        return {finish, finish};
    }

    int32 const start = walk + (hit ? 0 : static_cast<int32>(ULDUAR_ALGALON_STAR_KILL_START_MS));
    float const fast = std::max(ULDUAR_ALGALON_STAR_KILL_FAST_PCT_PER_SEC,
                                TeamSpeed(state, state.teamKillPctPerSec, state.teamKillTeam));
    return {start + StarDeathAt(star, fast), start + StarDeathAt(star, ULDUAR_ALGALON_STAR_KILL_SLOW_PCT_PER_SEC)};
}

bool SerialDue(StarPlan const& plan, AlgalonEncounterState const& state, Group* group, AlgalonScanUnit const& star)
{
    int32 const death = ForecastDeath(plan, &star);
    uint32 index = 0;
    for (auto const& [other, otherDeath] : plan.deaths)
        if (other != &star && (otherDeath < death || (otherDeath == death && other < &star)))
            ++index;

    int32 budget = static_cast<int32>((index + 1) * ULDUAR_ALGALON_STAR_SERIAL_SECONDS * IN_MILLISECONDS) +
                   static_cast<int32>(TeamWalkSeconds(state, group, star) * IN_MILLISECONDS);
    for (MergedBlackout const& blackout : plan.merged)
        budget += std::max(0, std::min(death, blackout.span.to) - std::max(0, blackout.span.from));

    return death <= budget;
}

template <typename Hit>
uint32 RaidUnderExplosionLine(Player* bot, Group* group, Hit hit)
{
    if (!group)
        return 0;

    uint32 const blast =
        bot->GetMap()->Is25ManRaid() ? ULDUAR_ALGALON_STAR_EXPLOSION_MAX_25 : ULDUAR_ALGALON_STAR_EXPLOSION_MAX_10;
    uint32 under = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (hit(member) && member->GetHealth() < std::min(blast, member->GetMaxHealth()))
            ++under;
    }

    return under;
}

bool RaidAffordsExplosion(Player* bot, Group* group) { return !RaidUnderExplosionLine(bot, group, InFight); }

std::string DescribeStarPlan(AlgalonEncounterState const& state, uint32 now)
{
    StarPlan const plan = PlanStars(state, now);
    std::vector<std::pair<int32, AlgalonScanUnit const*>> deaths;
    for (auto const& [star, death] : plan.deaths)
        deaths.emplace_back(death, star);

    for (AlgalonScanUnit const& star : state.stars)
        if (star.under)
            deaths.emplace_back(star.tickHp ? DrainDeathMs(star) : 0, &star);

    std::sort(deaths.begin(), deaths.end(),
              [](auto const& a, auto const& b) { return a.first < b.first; });
    std::string stars;
    for (auto const& [death, star] : deaths)
    {
        if (!stars.empty())
            stars += ", ";

        int32 const shown = star->under && star->tickHp ? DrainDeathMs(*star) : death;
        stars += std::to_string(star->guid.GetCounter()) + " " + std::to_string(static_cast<int32>(star->healthPct)) +
                 "% " + std::to_string(shown / IN_MILLISECONDS) + "s";
        if (star->under)
            stars += " u";
        else if (star->guid == state.shelterStar)
            stars += " s";
    }

    std::vector<std::string> parts;
    if (!stars.empty())
        parts.push_back(stars);

    for (MergedBlackout const& blackout : plan.merged)
        parts.push_back(blackout.label + " " + std::to_string(blackout.span.from / IN_MILLISECONDS) + ".." +
                        std::to_string(blackout.span.to / IN_MILLISECONDS));

    if (!state.underStars.empty())
        parts.push_back("under " + std::to_string(MsUntil(state.underFromMs, now) / IN_MILLISECONDS) + ".." +
                        std::to_string(MsUntil(state.underToMs, now) / IN_MILLISECONDS));

    std::string text;
    for (std::string const& part : parts)
        text += (text.empty() ? "" : " | ") + part;

    return text;
}

char const* StarWindow(Player* bot, Unit* boss, AlgalonEncounterState const& state, uint32 now,
                       AlgalonScanUnit const*& star)
{
    star = nullptr;
    Group* group = bot->GetGroup();
    if (state.stars.empty())
        return "none";

    if (state.urgent)
    {
        star = LowestStar(state);
        return "urgent";
    }

    if (state.bigBangCasting)
        return "bigbang";

    StarPlan const plan = PlanStars(state, now);
    int32 const gap = static_cast<int32>(ULDUAR_ALGALON_STAR_PACING_GAP_MS);
    int32 const recovery =
        state.lastBigBangEndMs
            ? MsUntil(state.lastBigBangEndMs, now) + static_cast<int32>(ULDUAR_ALGALON_STAR_VOID_RECOVERY_MS)
            : 0;
    bool const raidLow = !RaidAffordsExplosion(bot, group);
    bool const drained = recovery > 0 && raidLow;

    AlgalonScanUnit const* focus = FindScanned(state.stars, state.focusStar.Get());
    if (focus && !focus->under)
    {
        bool const placed =
            state.focusByDeadline
                ? StarClearOfTanks(state, group, boss, *focus, false)
                : NearestRaidMember(group, focus->position) >= ULDUAR_ALGALON_STAR_CLEAR_RADIUS - 2.0f &&
                      StarClearOfTanks(state, group, boss, *focus, true);
        if (placed)
        {
            int32 const keep = static_cast<int32>(ULDUAR_ALGALON_STAR_KEEP_GAP_MS);
            StarSpan const finish = FinishBand(state, group, *focus);
            int32 const fast = DroppedDeathMs(*focus);
            int32 const slow =
                std::max(fast, focus->tickHp ? DrainDeathMs(*focus)
                                             : StarDeathAt(*focus, ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC));
            bool const meteors = !StarCollision(plan, finish, focus, keep, false);
            bool const carried =
                drained && MsUntil(state.focusSinceMs, state.lastBigBangEndMs) < 0 && finish.from < recovery;
            if (!carried && (!StarCollision(plan, finish, focus, keep) ||
                             (StarCollision(plan, {fast, slow}, focus, keep, meteors) && finish.to <= slow)))
            {
                star = focus;
                return state.focusByDeadline ? "deadline" : "open";
            }
        }
    }

    if (state.lastStarDeathMs && getMSTimeDiff(state.lastStarDeathMs, now) < ULDUAR_ALGALON_STAR_PACING_GAP_MS)
        return "gap";

    int32 serialLine = std::numeric_limits<int32>::min();
    for (auto const& [candidate, death] : plan.deaths)
        if (SerialDue(plan, state, group, *candidate))
            serialLine = std::max(serialLine, death);

    std::vector<AlgalonScanUnit const*> due;
    for (auto const& [candidate, death] : plan.deaths)
    {
        bool const collides = death <= static_cast<int32>(ULDUAR_ALGALON_STAR_DUE_HORIZON_MS) &&
                              StarCollision(plan, {death, death}, candidate, gap);
        if (collides || death <= serialLine)
            due.push_back(candidate);
    }

    std::sort(due.begin(), due.end(), [&plan](AlgalonScanUnit const* a, AlgalonScanUnit const* b)
              { return ForecastDeath(plan, a) < ForecastDeath(plan, b); });

    if (!due.empty())
    {
        char const* blocked = "near";
        AlgalonScanUnit const* fallback = nullptr;
        int32 fallbackSeparation = -1;
        for (StarFilter filter : {StarFilter::TanksAndRing, StarFilter::Tanks})
        {
            for (AlgalonScanUnit const* candidate : due)
            {
                if (!StarPassesFilter(state, group, boss, *candidate, filter))
                    continue;

                StarSpan const finish = FinishBand(state, group, *candidate);
                char const* held = drained && finish.from < recovery ? "recovery"
                                   : InSmashRecovery(plan, state, now, raidLow, finish.from) ? "smashrecovery"
                                                                                             : nullptr;
                char const* collision = held ? held : StarCollision(plan, finish, candidate, gap);
                if (!collision)
                {
                    star = candidate;
                    return "deadline";
                }

                if (!std::strcmp(blocked, "near"))
                    blocked = collision;

                if (held)
                    continue;

                int32 const separation = StarSeparation(plan, finish, candidate);
                if (separation > fallbackSeparation)
                {
                    fallback = candidate;
                    fallbackSeparation = separation;
                }
            }
        }

        int32 natural = std::numeric_limits<int32>::max();
        for (AlgalonScanUnit const* candidate : due)
        {
            int32 const death = ForecastDeath(plan, candidate);
            natural = std::min(natural, std::max(0, StarSeparation(plan, {death, death}, candidate)));
        }

        if (fallback && fallbackSeparation > natural)
        {
            star = fallback;
            return "deadline";
        }

        return blocked;
    }

    if (raidLow)
        return "raidhp";

    std::vector<AlgalonScanUnit const*> open;
    for (AlgalonScanUnit const& candidate : state.stars)
        if (!candidate.under && StarPassesFilter(state, group, boss, candidate, StarFilter::Clear))
            open.push_back(&candidate);

    std::sort(open.begin(), open.end(), [&plan](AlgalonScanUnit const* a, AlgalonScanUnit const* b)
              { return ForecastDeath(plan, a) < ForecastDeath(plan, b); });

    char const* blocked = "near";
    for (AlgalonScanUnit const* candidate : open)
    {
        StarSpan const finish = FinishBand(state, group, *candidate);
        char const* collision = StarCollision(plan, finish, candidate, gap);
        if (!collision && InSmashRecovery(plan, state, now, raidLow, finish.from))
            collision = "smashrecovery";

        if (!collision)
        {
            star = candidate;
            return "open";
        }

        if (!std::strcmp(blocked, "near"))
            blocked = collision;
    }

    return blocked;
}

void EnsureAlgalonSlotAssignments(Player* bot, AlgalonEncounterState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    for (auto it = state.slotAssignments.begin(); it != state.slotAssignments.end();)
    {
        Player* holder = FindGroupMember(group, it->first);
        if (!holder || holder->GetMapId() != ULDUAR_MAP_ID || !AlgalonTakesRingSlot(holder))
            it = state.slotAssignments.erase(it);
        else
            ++it;
    }

    std::array<bool, ULDUAR_ALGALON_TOTAL_SLOTS> used = {};
    for (auto const& assignment : state.slotAssignments)
        if (assignment.second < ULDUAR_ALGALON_TOTAL_SLOTS)
            used[assignment.second] = true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMapId() != ULDUAR_MAP_ID || !AlgalonTakesRingSlot(member) ||
            state.slotAssignments.count(member->GetGUID()))
            continue;

        bool const wantsHealerSlot = PlayerbotAI::IsHeal(member);
        bool placed = false;
        for (uint8 pass = 0; pass < 2 && !placed; ++pass)
        {
            for (uint8 slotIndex = 0; slotIndex < ULDUAR_ALGALON_TOTAL_SLOTS; ++slotIndex)
            {
                bool const healerSlot = slotIndex < ULDUAR_ALGALON_HEALER_SLOTS;
                if (used[slotIndex] || (pass == 0 && healerSlot != wantsHealerSlot))
                    continue;

                state.slotAssignments[member->GetGUID()] = slotIndex;
                used[slotIndex] = true;
                placed = true;
                break;
            }
        }
    }

    if (RaidObs::Active() && !state.slotAssignments.empty())
    {
        auto const first = *state.slotAssignments.begin();
        state.slotAssignments.Set(first.first, first.second);
    }
}

AlgalonPhase ReadPhase(Creature* boss, AlgalonEncounterState const& state)
{
    if (state.engagedSeen && boss->GetFaction() == FACTION_FRIENDLY)
        return AlgalonPhase::Won;

    if (!boss->IsInCombat())
        return AlgalonPhase::Idle;

    if (boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return AlgalonPhase::Intro;

    return state.wormHolesSeen ? AlgalonPhase::Two : AlgalonPhase::One;
}

bool MeleeStageSpot(Player* bot, AlgalonEncounterState const& state, Position& spot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<ObjectGuid> staged;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (IsBot(member) && member->GetMapId() == ULDUAR_MAP_ID && !PlayerbotAI::IsRanged(member) &&
            !state.swapPair.count(member->GetGUID()))
            staged.push_back(member->GetGUID());
    }

    std::sort(staged.begin(), staged.end());
    auto const own = std::find(staged.begin(), staged.end(), bot->GetGUID());
    if (own == staged.end())
        return false;

    spot = ULDUAR_ALGALON_MELEE_STAGE_SLOTS[static_cast<size_t>(own - staged.begin()) %
                                            ULDUAR_ALGALON_MELEE_STAGE_SPOTS];
    return true;
}

void ClampIntoRoom(float& x, float& y)
{
    float const fromCenter = ULDUAR_ALGALON_ROOM_CENTER.GetExactDist2d(x, y);
    if (fromCenter <= ULDUAR_ALGALON_ROOM_CLAMP)
        return;

    float const scale = ULDUAR_ALGALON_ROOM_CLAMP / fromCenter;
    x = ULDUAR_ALGALON_ROOM_CENTER.GetPositionX() + (x - ULDUAR_ALGALON_ROOM_CENTER.GetPositionX()) * scale;
    y = ULDUAR_ALGALON_ROOM_CENTER.GetPositionY() + (y - ULDUAR_ALGALON_ROOM_CENTER.GetPositionY()) * scale;
}

bool SpotOffFrom(Unit* boss, Position const& origin, Position const& toward, float distance, Position& spot)
{
    float const angle = origin.GetAngle(&toward);
    float x = origin.GetPositionX() + std::cos(angle) * distance;
    float y = origin.GetPositionY() + std::sin(angle) * distance;
    ClampIntoRoom(x, y);
    if (boss && boss->GetExactDist2d(x, y) < ULDUAR_ALGALON_FORMATION_CLEARANCE)
        return false;

    spot = Position(x, y, ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
    return true;
}

bool FarHoleSpot(Player* bot, Unit* boss, AlgalonEncounterState const& state, Position const& anchor,
                 Position& spot)
{
    if (state.holes.empty() || state.swapPair.count(bot->GetGUID()) || state.handler.Get() == bot->GetGUID() ||
        state.disperser.Get() == bot->GetGUID())
        return false;

    if (!state.bigBangCasting && !BigBangDueWithin(state, ULDUAR_ALGALON_FAR_HOLE_SECONDS, getMSTime()))
        return false;

    if (AlgalonStaysOut(bot))
        return false;

    if (state.bigBangCasting ? state.nitroRunners.count(bot->GetGUID()) > 0 : NitroBoostsReady(bot))
        return false;

    bool const under = !state.underStars.empty();
    float const farYards = under ? ULDUAR_ALGALON_UNDER_HOLE_REACH : ULDUAR_ALGALON_FAR_HOLE_YARDS;
    float const standoff = under ? ULDUAR_ALGALON_UNDER_HOLE_REACH : ULDUAR_ALGALON_NEAR_HOLE_STANDOFF;
    AlgalonScanUnit const* nearest = NearestShelter(state, anchor);
    if (!nearest || nearest->position.GetExactDist2d(anchor) <= farYards)
        return false;

    if (SpotOffFrom(boss, nearest->position, anchor, standoff, spot))
        return true;

    float closest = std::numeric_limits<float>::max();
    for (uint8 step = 0; step < ULDUAR_ALGALON_SWEEP_BEARINGS; ++step)
    {
        float const angle = static_cast<float>(step) * ULDUAR_ALGALON_SWEEP_ANGLE_STEP;
        float const x = nearest->position.GetPositionX() + std::cos(angle) * standoff;
        float const y = nearest->position.GetPositionY() + std::sin(angle) * standoff;
        float const distance = anchor.GetExactDist2d(x, y);
        if (distance >= closest || !AlgalonSpotInRoom(x, y) ||
            (boss && boss->GetExactDist2d(x, y) < ULDUAR_ALGALON_FORMATION_CLEARANCE))
            continue;

        closest = distance;
        spot = Position(x, y, ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
    }

    return closest != std::numeric_limits<float>::max();
}

char const* StarTeamSpot(Player* bot, Unit* boss, AlgalonEncounterState const& state, Position const& anchor,
                         Position& spot)
{
    if (!IsAlgalonStarTeam(bot))
        return nullptr;

    AlgalonScanUnit const* star = FindScanned(state.stars, StarFocusOf(state, bot->GetGUID()));
    if (!star)
        return nullptr;

    float const reach = bot->GetExactDist2d(star->position);
    bool const inReach = reach >= ULDUAR_ALGALON_STAR_TEAM_HOLD_MIN && reach <= ULDUAR_ALGALON_STAR_TEAM_REACH;
    if (inReach && !IsAlgalonPhased(bot) && AlgalonSpotInRoom(bot->GetPositionX(), bot->GetPositionY()) &&
        (!boss || boss->GetExactDist2d(bot) >= ULDUAR_ALGALON_FORMATION_CLEARANCE))
    {
        spot = Position(bot->GetPositionX(), bot->GetPositionY(), ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
        return "starhold";
    }

    if (star->position.GetExactDist2d(anchor) > ULDUAR_ALGALON_STAR_TEAM_REACH &&
        SpotOffFrom(boss, star->position, anchor, ULDUAR_ALGALON_STAR_TEAM_STANDOFF, spot))
        return "star";

    return nullptr;
}

bool MeleeHoleSpot(Player* bot, Unit* boss, AlgalonEncounterState const& state, Position& spot);

char const* SwapTankSlot(Player* bot, PlayerbotAI* botAI, Position& slot)
{
    Unit* boss = GetAlgalon(botAI);
    bool const selectable = boss && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) && AlgalonEngaged(botAI);
    Player* holder = selectable ? GetAlgalonBossTank(botAI) : nullptr;
    bool const pairHolds = holder && IsAlgalonSwapTank(holder);
    if (pairHolds && holder == bot)
    {
        AlgalonEncounterState const* state = FindState(bot);
        if (state && state->firstHolder == bot->GetGUID() &&
            getMSTimeDiff(state->firstHoldMs, getMSTime()) < ULDUAR_ALGALON_PULL_HOLD_MS)
        {
            slot = ULDUAR_ALGALON_PICKUP_SLOT;
            return "pickup";
        }

        slot = ULDUAR_ALGALON_TANK_SLOT;
        return "tank";
    }

    if (!pairHolds && GetAlgalonPickupTank(botAI) == bot)
    {
        slot = ULDUAR_ALGALON_PICKUP_SLOT;
        return "pickup";
    }

    if (!selectable)
        slot = ULDUAR_ALGALON_PULL_OFFTANK_SLOT;
    else if (!pairHolds || holder->GetExactDist2d(ULDUAR_ALGALON_TANK_SLOT) <= ULDUAR_ALGALON_HOLDER_ON_SPOT_RADIUS)
        slot = ULDUAR_ALGALON_OFFTANK_SLOT;
    else
    {
        float const angle = boss->GetAngle(&ULDUAR_ALGALON_TANK_SLOT);
        slot = Position(boss->GetPositionX() + std::cos(angle) * ULDUAR_ALGALON_OFFTANK_STEP,
                        boss->GetPositionY() + std::sin(angle) * ULDUAR_ALGALON_OFFTANK_STEP,
                        ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
    }

    return "offtank";
}

char const* RawSlot(Player* bot, Position& slot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    AlgalonEncounterState const* state = FindState(bot);
    if (!botAI || !state)
        return nullptr;

    if (IsAlgalonSwapTank(bot))
        return SwapTankSlot(bot, botAI, slot);

    Unit* boss = GetAlgalon(botAI);
    char const* kind = nullptr;
    if (AlgalonMeleeStaged(bot))
        kind = MeleeStageSpot(bot, *state, slot) ? "stage" : nullptr;
    else if (AlgalonTakesRingSlot(bot))
    {
        auto const assignment = state->slotAssignments.find(bot->GetGUID());
        if (assignment != state->slotAssignments.end() && TryGetAlgalonSlotPosition(assignment->second, slot))
            kind = "slot";
    }

    if (!state->engaged || !boss)
        return kind;

    Position const anchor = kind ? slot : boss->GetPosition();
    Position spot;
    if (FarHoleSpot(bot, boss, *state, anchor, spot))
    {
        slot = spot;
        return "nearhole";
    }

    if (char const* team =
            (kind && PlayerbotAI::IsRanged(bot)) ? StarTeamSpot(bot, boss, *state, anchor, spot) : nullptr)
    {
        slot = spot;
        return team;
    }

    if (!kind && MeleeHoleSpot(bot, boss, *state, spot))
    {
        slot = spot;
        return "melee";
    }

    return kind;
}

bool StarDying(AlgalonScanUnit const& star)
{
    return star.healthPct <= ULDUAR_ALGALON_STAR_DYING_HP_PCT ||
           star.healthPct / star.lossPctPerSec * IN_MILLISECONDS <= ULDUAR_ALGALON_STAR_DYING_MS;
}

bool DyingStarNear(AlgalonEncounterState const& state, Position const& spot, float radius)
{
    for (AlgalonScanUnit const& star : state.stars)
        if (StarDying(star) && spot.GetExactDist2d(star.position) < radius)
            return true;

    return false;
}

bool ClearOfHoles(AlgalonEncounterState const& state, float x, float y)
{
    for (AlgalonScanUnit const& hole : state.holes)
        if (hole.position.GetExactDist2d(x, y) < ULDUAR_ALGALON_HOLE_EXIT_RADIUS)
            return false;

    return true;
}

bool ClearOfMarkers(AlgalonEncounterState const& state, float x, float y)
{
    for (AlgalonScanUnit const& marker : state.markers)
        if (marker.position.GetExactDist2d(x, y) < ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE)
            return false;

    return true;
}

AlgalonScanUnit const* HoleAcrossPath(AlgalonEncounterState const& state, Position const& from, float x, float y,
                                      bool phased, float clearance = ULDUAR_ALGALON_HOLE_PATH_CLEARANCE)
{
    float const inside = phased ? ULDUAR_ALGALON_HOLE_EXIT_RADIUS : ULDUAR_ALGALON_SHELTER_RADIUS;
    AlgalonScanUnit const* leaving = nullptr;
    float nearest = std::numeric_limits<float>::max();
    for (AlgalonScanUnit const& hole : state.holes)
    {
        float const start = hole.position.GetExactDist2d(from);
        if (start <= inside && start < nearest)
        {
            leaving = &hole;
            nearest = start;
        }
    }

    for (AlgalonScanUnit const& hole : state.holes)
    {
        if (&hole == leaving)
            continue;

        float const start = hole.position.GetExactDist2d(from);
        float const limit = std::min(clearance, std::max(start - 0.5f, ULDUAR_ALGALON_SHELTER_RADIUS));
        if (SegmentDistance(from, x, y, hole.position) < limit)
            return &hole;
    }

    return nullptr;
}

bool MeleeHoleSpot(Player* bot, Unit* boss, AlgalonEncounterState const& state, Position& spot)
{
    if (state.holes.empty() || PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsTank(bot) || IsAlgalonPhased(bot))
        return false;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI || botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() != boss)
        return false;

    float const reach = bot->GetMeleeRange(boss);
    if (bot->GetExactDist2d(boss) <= reach)
        return false;

    float const bearing = boss->GetAngle(bot);
    if (!AlgalonWalkEntersField(bot, boss->GetPositionX() + std::cos(bearing) * reach,
                                boss->GetPositionY() + std::sin(bearing) * reach))
        return false;

    float const radius = reach - ULDUAR_ALGALON_MELEE_SPOT_INSET;
    float closest = std::numeric_limits<float>::max();
    bool closestClear = false;
    for (uint8 step = 0; step < ULDUAR_ALGALON_SWEEP_BEARINGS; ++step)
    {
        float const angle = static_cast<float>(step) * ULDUAR_ALGALON_SWEEP_ANGLE_STEP;
        float const x = boss->GetPositionX() + std::cos(angle) * radius;
        float const y = boss->GetPositionY() + std::sin(angle) * radius;
        if (!AlgalonSpotInRoom(x, y) || !ClearOfHoles(state, x, y))
            continue;

        bool const clear = !HoleAcrossPath(state, bot->GetPosition(), x, y, false);
        float const distance = bot->GetExactDist2d(x, y);
        if ((clear && !closestClear) || (clear == closestClear && distance < closest))
        {
            closest = distance;
            closestClear = clear;
            spot = Position(x, y, ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
        }
    }

    return closest != std::numeric_limits<float>::max();
}

bool FindHoleDetour(Player* bot, AlgalonEncounterState const& state, AlgalonScanUnit const& hole, Position const& to,
                    Position& waypoint)
{
    bool const phased = IsAlgalonPhased(bot);
    float const remaining = bot->GetExactDist2d(to);
    float best = std::numeric_limits<float>::max();
    bool bestThrough = false;
    for (uint8 step = 0; step < ULDUAR_ALGALON_SWEEP_BEARINGS; ++step)
    {
        float const angle = static_cast<float>(step) * ULDUAR_ALGALON_SWEEP_ANGLE_STEP;
        float const x = hole.position.GetPositionX() + std::cos(angle) * ULDUAR_ALGALON_HOLE_DETOUR_RADIUS;
        float const y = hole.position.GetPositionY() + std::sin(angle) * ULDUAR_ALGALON_HOLE_DETOUR_RADIUS;
        float const onward = to.GetExactDist2d(x, y);
        if (onward >= remaining - 1.0f || !AlgalonSpotInRoom(x, y) || !ClearOfHoles(state, x, y) ||
            !ClearOfMarkers(state, x, y) || HoleAcrossPath(state, bot->GetPosition(), x, y, phased))
            continue;

        Position const point(x, y, to.GetPositionZ());
        bool const through = !HoleAcrossPath(state, point, to.GetPositionX(), to.GetPositionY(), false);
        float const legs = bot->GetExactDist2d(x, y) + onward;
        if ((through && !bestThrough) || (through == bestThrough && legs < best))
        {
            best = legs;
            bestThrough = through;
            waypoint = point;
        }
    }

    if (best == std::numeric_limits<float>::max())
        return false;

    waypoint = ValidateFloorPoint(bot, waypoint);
    return true;
}

bool DisplaceFromHoles(Player* bot, AlgalonEncounterState const& state, Position const& slot, Position& position)
{
    for (float radius = ULDUAR_ALGALON_SWEEP_STEP; radius <= ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS;
         radius += ULDUAR_ALGALON_SWEEP_STEP)
    {
        for (uint8 step = 0; step < ULDUAR_ALGALON_SWEEP_BEARINGS; ++step)
        {
            float const angle = static_cast<float>(step) * ULDUAR_ALGALON_SWEEP_ANGLE_STEP;
            float const x = slot.GetPositionX() + std::cos(angle) * radius;
            float const y = slot.GetPositionY() + std::sin(angle) * radius;
            if (!ClearOfHoles(state, x, y) || !ClearOfMarkers(state, x, y) || !AlgalonSpotInRoom(x, y))
                continue;

            position = ValidateFloorPoint(bot, Position(x, y, slot.GetPositionZ()));
            return true;
        }
    }

    return false;
}

char const* DeriveSlot(Player* bot, Position& position, bool& found)
{
    found = false;
    Position slot;
    char const* const kind = RawSlot(bot, slot);
    if (!kind)
        return "none";

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (AlgalonCosmicSmashMarkerNear(botAI, slot, ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE) ||
        DyingStarNear(*FindState(bot), slot, ULDUAR_ALGALON_STAR_DYING_RADIUS))
        return "smash";

    AlgalonEncounterState const* state = FindState(bot);
    char const* branch = kind;
    position = slot;
    if (!ClearOfHoles(*state, slot.GetPositionX(), slot.GetPositionY()))
    {
        if (!DisplaceFromHoles(bot, *state, slot, position))
            return "none";

        branch = "buried";
    }

    if (bot->GetExactDist2d(position) > ULDUAR_ALGALON_SLOT_TOLERANCE)
    {
        if (AlgalonScanUnit const* hole = HoleAcrossPath(*state, bot->GetPosition(), position.GetPositionX(),
                                                         position.GetPositionY(), IsAlgalonPhased(bot)))
        {
            Position waypoint;
            found = FindHoleDetour(bot, *state, *hole, position, waypoint);
            position = waypoint;
            return "blocked";
        }
    }

    found = true;
    return branch;
}

int32 ExplosionDueMs(AlgalonEncounterState const& state, AlgalonScanUnit const& star, float lowPct)
{
    if (star.healthPct <= lowPct)
        return 0;

    float loss = star.lossPctPerSec;
    auto const left = state.starLeft.find(star.guid);
    if (left != state.starLeft.end())
    {
        uint32 const since = getMSTimeDiff(left->second.first, state.lastTickMs);
        if (since >= ULDUAR_ALGALON_STAR_LOG_STEP_MS)
            loss = std::max(ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC,
                            (left->second.second - star.healthPct) * IN_MILLISECONDS / static_cast<float>(since));
        else
            loss = std::min(loss, ULDUAR_ALGALON_STAR_DROPPED_PCT_PER_SEC);
    }

    return StarDeathAt(star, loss);
}

bool DiesUnder(AlgalonEncounterState const& state, AlgalonScanUnit const& star)
{
    return star.under && star.tickHp && state.lastTickMs &&
           DrainDeathMs(star) <= MsUntil(state.underToMs, state.lastTickMs);
}

AlgalonScanUnit const* ExplodingStar(AlgalonEncounterState const& state, uint32 leadMs,
                                     float lowPct = ULDUAR_ALGALON_EXPLOSION_LOW_PCT, Player* bot = nullptr,
                                     bool underToo = false)
{
    AlgalonScanUnit const* soonest = nullptr;
    int32 best = std::numeric_limits<int32>::max();
    for (AlgalonScanUnit const& star : state.stars)
    {
        int32 const death = ExplosionDueMs(state, star, lowPct);
        if (death <= static_cast<int32>(leadMs) && death < best && (underToo || !DiesUnder(state, star)) &&
            (!bot || Resolve(bot, star.guid)))
        {
            soonest = &star;
            best = death;
        }
    }

    return soonest;
}

bool CaughtOutside(AlgalonEncounterState const& state, Player* member, int32 deathMs);

float StackedHit(AlgalonEncounterState const& state, Player* bot, AlgalonScanUnit const& first, float topRoll,
                 int32 coverMs, bool underToo = false)
{
    int32 const from = ExplosionDueMs(state, first, ULDUAR_ALGALON_EXPLOSION_LOW_PCT);
    std::vector<int32> deaths;
    for (AlgalonScanUnit const& star : state.stars)
    {
        int32 const death = ExplosionDueMs(state, star, ULDUAR_ALGALON_EXPLOSION_LOW_PCT);
        if (death <= from + static_cast<int32>(ULDUAR_ALGALON_EXPLOSION_STACK_MS) &&
            (underToo || !DiesUnder(state, star)) && Resolve(bot, star.guid) &&
            (!underToo || star.guid == first.guid || CaughtOutside(state, bot, death)))
            deaths.push_back(death);
    }

    if (deaths.empty())
        deaths.push_back(from);

    std::sort(deaths.begin(), deaths.end());
    deaths.resize(std::min<size_t>(deaths.size(), ULDUAR_ALGALON_EXPLOSION_STACK_MAX));
    float hit = 0.0f;
    for (int32 const death : deaths)
        hit += topRoll * (death <= coverMs ? ULDUAR_ALGALON_COVERED_EXPLOSION_FACTOR : 1.0f);

    return hit;
}

bool RaidHitWindow(AlgalonEncounterState const& state, uint32 now)
{
    for (uint32 const hit : {state.lastStarDeathMs, state.lastSmashImpactMs})
        if (hit && getMSTimeDiff(hit, now) <= ULDUAR_ALGALON_TANK_WINDOW_TAIL_MS)
            return true;

    if (ExplodingStar(state, ULDUAR_ALGALON_TANK_WINDOW_LEAD_MS))
        return true;

    for (auto const& [marker, seenMs] : state.markerSeenMs)
    {
        int32 const impact = MsUntil(seenMs + ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_MS, now);
        if (impact >= 0 && impact <= static_cast<int32>(ULDUAR_ALGALON_TANK_WINDOW_LEAD_MS))
            return true;
    }

    return false;
}

bool TankCutRunning(Player* holder)
{
    return RunningTankDefensive(holder, true, 0) || holder->HasAura(SPELL_ALGALON_PAIN_SUPPRESSION) ||
           holder->HasAura(SPELL_ALGALON_HAND_OF_SACRIFICE) || holder->HasAura(SPELL_ALGALON_GUARDIAN_SPIRIT);
}

char const* TankCutReason(AlgalonEncounterState const& state, Player* holder, Player* partner, uint32 now)
{
    bool const swapping = partner && CanStayOut(partner) && SwapDue(state, holder, partner, now);
    if (RaidHitWindow(state, now) && !swapping && holder->GetHealthPct() < ULDUAR_ALGALON_TANK_WINDOW_PCT)
        return "window";

    return holder->GetHealthPct() < ULDUAR_ALGALON_TANK_DEFENSIVE_PCT ? "low" : nullptr;
}

void CallHumanTankCut(Group* group, PlayerbotAI* botAI, Creature* boss, AlgalonEncounterState const& state,
                      uint32 now)
{
    Player* holder = BossHolder(boss);
    if (state.bigBangCasting || !holder || IsBot(holder) || !state.swapPair.count(holder->GetGUID()) ||
        !CanStayOut(holder) || TankCutRunning(holder))
        return;

    if (state.lastBigBangEndMs && getMSTimeDiff(state.lastBigBangEndMs, now) < ULDUAR_ALGALON_BIG_BANG_STILL_MS &&
        state.lastSoaker != holder->GetGUID())
        return;

    char const* why = TankCutReason(state, holder, PairPartner(group, state, holder), now);
    uint32 const spellId = why ? ReadyTankDefensiveSpell(holder, true, 0, 0) : 0;
    if (!spellId)
        return;

    bool const reserved = state.disperser.Get().IsEmpty() && AlgalonLoneHolder(botAI) != holder;
    if (reserved && SpellCooldownMs(holder, spellId) > AlgalonMsToBigBangHit(botAI))
        return;

    if (Player* sender = FindTracker(group))
        CallHumanTankDefensive(sender, holder, spellId, 0,
                               std::strcmp(why, "window") ? "you are low" : "stars or meteors are hitting the raid",
                               "algalon.humancall");
}

void CallHumanSoak(Group* group, Creature* boss, AlgalonEncounterState& state)
{
    Player* soaker = FindGroupMember(group, state.bigBangSoaker.Get());
    Spell* cast = CurrentBigBang(boss);
    if (state.humanSoakCalled || !soaker || IsBot(soaker) || !CanStayOut(soaker) || !cast)
        return;

    int32 const remaining = cast->GetCastTimeRemaining();
    Player* sender = FindTracker(group);
    if (remaining < 0 || remaining > ULDUAR_ALGALON_HUMAN_SOAK_CALL_MS || !sender)
        return;

    state.humanSoakCalled = true;
    uint32 const hitInMs = static_cast<uint32>(remaining);
    CallHumanTankDefensive(sender, soaker, ReadyTankDefensiveSpell(soaker, true, 0, hitInMs), hitInMs,
                           "Big Bang hits you", "algalon.humancall", 0);
}

bool CanPay(Player* caster, SpellInfo const* info)
{
    if (info->PowerType >= MAX_POWERS || info->PowerType == POWER_RUNE)
        return true;

    return static_cast<int32>(caster->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(caster, info->GetSchoolMask());
}

uint32 ExternalSpell(Player* provider, char const* spell)
{
    PlayerbotAI* providerAI = GET_PLAYERBOT_AI(provider);
    if (!providerAI || !CanStayOut(provider))
        return 0;

    uint32 const spellId = providerAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return info && CanPay(provider, info) && !provider->HasSpellCooldown(spellId) ? spellId : 0;
}

bool ExternalReaches(Player* provider, uint32 spellId, Player* target)
{
    PlayerbotAI* providerAI = GET_PLAYERBOT_AI(provider);
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!providerAI || !info)
        return false;

    if (provider != target && (!provider->IsWithinDistInMap(target, info->GetMaxRange(true, provider)) ||
                               !provider->IsWithinLOSInMap(target)))
        return false;

    return providerAI->CanCastSpell(spellId, target);
}

uint32 ExternalReady(Player* provider, char const* spell, Player* target)
{
    uint32 const spellId = ExternalSpell(provider, spell);
    return spellId && ExternalReaches(provider, spellId, target) ? spellId : 0;
}

uint32 SacrificeSpell(Player* paladin, Player* holder)
{
    if (paladin == holder || paladin->getClass() != CLASS_PALADIN ||
        paladin->GetHealthPct() < ULDUAR_ALGALON_SACRIFICE_MIN_PCT ||
        ai::paladin::HasAnyPaladinHandFromCaster(holder, paladin))
        return 0;

    return ExternalSpell(paladin, "hand of sacrifice");
}

bool HasDivineGuardian(Player* paladin)
{
    uint8 const spec = paladin->GetActiveSpec();
    return paladin->HasTalent(SPELL_ALGALON_DIVINE_GUARDIAN_RANK_1, spec) ||
           paladin->HasTalent(SPELL_ALGALON_DIVINE_GUARDIAN_RANK_2, spec);
}

bool ExplosionCovered(Group* group, AlgalonEncounterState const& state)
{
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (InFight(member) && member->HasAura(SPELL_ALGALON_DIVINE_GUARDIAN))
            return true;
    }

    Player* auraPaladin = FindGroupMember(group, state.shadowAuraPaladin.Get());
    return auraPaladin && auraPaladin->HasAura(SPELL_ALGALON_AURA_MASTERY);
}

Player* ElectShadowAuraPaladin(Group* group, ObjectGuid const& current)
{
    Player* kept = nullptr;
    Player* ready = nullptr;
    Player* lowest = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !InFight(member) || member->getClass() != CLASS_PALADIN ||
            !member->HasActiveSpell(SPELL_ALGALON_AURA_MASTERY) ||
            !memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", "shadow resistance aura")->Get())
            continue;

        if (member->GetGUID() == current)
            kept = member;

        if (!lowest || member->GetGUID() < lowest->GetGUID())
            lowest = member;

        if (!member->HasSpellCooldown(SPELL_ALGALON_AURA_MASTERY) && (!ready || member->GetGUID() < ready->GetGUID()))
            ready = member;
    }

    if (kept && (kept->HasAura(SPELL_ALGALON_AURA_MASTERY) || !kept->HasSpellCooldown(SPELL_ALGALON_AURA_MASTERY)))
        return kept;

    if (ready)
        return ready;

    return kept ? kept : lowest;
}

Player* ElectFireTotemShaman(Group* group, ObjectGuid const& current)
{
    static char const* const waters[] = {"mana spring", "healing stream", "cleansing"};
    std::vector<std::pair<Player*, char const*>> shamans;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !InFight(member) || member->getClass() != CLASS_SHAMAN ||
            !memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", "fire resistance totem")->Get())
            continue;

        for (char const* water : waters)
        {
            if (memberAI->HasStrategy(water, BOT_STATE_COMBAT))
            {
                shamans.emplace_back(member, water);
                break;
            }
        }
    }

    Player* best = nullptr;
    for (auto const& [shaman, water] : shamans)
    {
        bool const duplicate = std::any_of(shamans.begin(), shamans.end(), [&](auto const& other)
                                           { return other.first != shaman && std::strcmp(other.second, water) == 0; });
        if (!duplicate)
            continue;

        if (shaman->GetGUID() == current)
            return shaman;

        auto const rank = [](Player* candidate)
        { return std::make_pair(PlayerbotAI::IsHeal(candidate), candidate->GetGUID()); };
        if (!best || rank(shaman) < rank(best))
            best = shaman;
    }

    return best;
}

void CountHidersOut(Group* group, AlgalonEncounterState& state)
{
    auto const nearest = [&state](Player* human)
    {
        AlgalonScanUnit const* best = nullptr;
        for (AlgalonScanUnit const& hole : state.holes)
            if (!best || human->GetExactDist2d(hole.position) < human->GetExactDist2d(best->position))
                best = &hole;

        return best;
    };

    state.hidersOut.clear();
    if (!state.bigBangCasting)
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!CanStayOut(member))
            continue;

        ObjectGuid const guid = member->GetGUID();
        if (guid == state.bigBangSoaker.Get() || guid == state.bigBangBackup.Get() || guid == state.handler.Get() ||
            AlgalonHoldsConstellation(member))
            continue;

        AlgalonScanUnit const* hole = IsBot(member) ? ShelterHole(member, state) : nearest(member);
        if (hole)
            ++state.hidersOut[hole->guid];
    }
}

}

Unit* GetAlgalon(PlayerbotAI* botAI) { return botAI ? FindAlgalon(botAI->GetBot()) : nullptr; }

bool AlgalonInRoom(Player* bot)
{
    return bot && bot->GetMapId() == ULDUAR_MAP_ID &&
           bot->GetExactDist2d(ULDUAR_ALGALON_ROOM_CENTER.GetPositionX(), ULDUAR_ALGALON_ROOM_CENTER.GetPositionY()) <=
               ULDUAR_ALGALON_ROOM_RADIUS &&
           bot->GetPositionZ() >= ULDUAR_ALGALON_ROOM_Z_MIN && bot->GetPositionZ() <= ULDUAR_ALGALON_ROOM_Z_MAX;
}

bool AlgalonPresent(PlayerbotAI* botAI)
{
    Unit* boss = GetAlgalon(botAI);
    return boss && boss->GetFaction() != FACTION_FRIENDLY && AlgalonInRoom(botAI->GetBot());
}

bool AlgalonEngaged(PlayerbotAI* botAI)
{
    Unit* boss = GetAlgalon(botAI);
    return boss && boss->IsInCombat() && boss->GetFaction() != FACTION_FRIENDLY;
}

bool AlgalonInPhaseOne(PlayerbotAI* botAI)
{
    AlgalonEncounterState const* state = FindState(botAI ? botAI->GetBot() : nullptr);
    return state && state->phase.Get() == AlgalonPhase::One;
}

bool IsAlgalonPhased(Unit const* unit) { return unit && !(unit->GetPhaseMask() & PHASEMASK_NORMAL); }

bool AlgalonInOpener(PlayerbotAI* botAI)
{
    AlgalonEncounterState const* state = FindState(botAI ? botAI->GetBot() : nullptr);
    if (!state)
        return false;

    return state->phase.Get() == AlgalonPhase::One && state->introEndMs &&
           getMSTimeDiff(state->introEndMs, getMSTime()) <= ULDUAR_ALGALON_OPENER_MS;
}

Player* GetAlgalonOpenerTank(PlayerbotAI* botAI)
{
    Player* holder = GetAlgalonBossTank(botAI);
    return holder && IsAlgalonSwapTank(holder) ? holder : nullptr;
}

char const* AlgalonThreatRedirectSpell(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_HUNTER:
            return "misdirection";
        case CLASS_ROGUE:
            return "tricks of the trade";
        default:
            return nullptr;
    }
}

namespace
{
bool UnderBand(uint32 tickHp, int32 nextTickMs, int32 fromMs, int32 toMs, uint32& lo, uint32& hi)
{
    int32 const first = fromMs - nextTickMs;
    int32 const last = toMs - nextTickMs;
    if (last < 0)
        return false;

    int32 const second = static_cast<int32>(IN_MILLISECONDS);
    int32 const kLo = first <= 0 ? 1 : (first + second - 1) / second + 1;
    int32 const kHi = last / second + 1;
    if (kHi < kLo)
        return false;

    lo = static_cast<uint32>(kLo - 1) * tickHp;
    hi = static_cast<uint32>(kHi) * tickHp;
    return true;
}

uint32 StopLineHp(AlgalonScanUnit const& star)
{
    uint32 const half = star.tickHp / 2;
    return std::max(star.underLo + 1, star.underHi > half ? star.underHi - half : 0);
}

uint32 BulkLeftHp(AlgalonScanUnit const& star)
{
    uint32 const stop = StopLineHp(star);
    uint32 const due = star.health > star.pendingHp ? star.health - star.pendingHp : 0;
    return due > stop ? due - stop : 0;
}

int32 BulkWorkMs(AlgalonEncounterState const& state, AlgalonScanUnit const& star)
{
    uint32 const left = BulkLeftHp(star);
    if (!left)
        return 0;

    float const measured = state.bulkPctPerSec > 0.0f ? TeamSpeed(state, state.bulkPctPerSec, state.bulkTeam)
                                                      : TeamSpeed(state, state.teamKillPctPerSec, state.teamKillTeam);
    float const pctPerSec = measured > 0.0f ? std::min(measured, ULDUAR_ALGALON_UNDER_TEAM_PCT_PER_SEC)
                                            : ULDUAR_ALGALON_UNDER_TEAM_PCT_PER_SEC;
    float const closing = std::max(1.0f, pctPerSec - ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC);
    return static_cast<int32>(left / (star.tickHp * closing) * IN_MILLISECONDS) + ULDUAR_ALGALON_UNDER_SWITCH_MS;
}

bool UnderFrozen(AlgalonEncounterState const& state, uint32 now)
{
    return state.bigBangCasting ||
           (state.lastBigBangEndMs && getMSTimeDiff(state.lastBigBangEndMs, now) < ULDUAR_ALGALON_UNDER_KEEP_MS);
}

void MarkUnder(AlgalonEncounterState& state, uint32 now)
{
    int32 const from = MsUntil(state.underFromMs, now);
    int32 const to = MsUntil(state.underToMs, now);
    bool const frozen = UnderFrozen(state, now);
    for (AlgalonScanUnit& star : state.stars)
    {
        star.under = state.underStars.count(star.guid) > 0;
        star.underLo = 0;
        star.underHi = 0;
        if (star.under && star.tickHp)
            UnderBand(star.tickHp, star.nextTickMs, from, to, star.underLo, star.underHi);

        star.underDying =
            star.under && frozen && star.tickHp && DrainDeathMs(star) <= ULDUAR_ALGALON_UNDER_DYING_MS;
    }
}

void PlanShelterStar(AlgalonEncounterState& state)
{
    if (!state.holes.empty())
    {
        state.shelterStar = ObjectGuid::Empty;
        return;
    }

    if (FindScanned(state.stars, state.shelterStar))
        return;

    AlgalonScanUnit const* nearest = nullptr;
    for (AlgalonScanUnit const& star : state.stars)
        if (!nearest || star.position.GetExactDist2d(ULDUAR_ALGALON_HOLD_SPOT) <
                            nearest->position.GetExactDist2d(ULDUAR_ALGALON_HOLD_SPOT))
            nearest = &star;

    state.shelterStar = nearest ? nearest->guid : ObjectGuid::Empty;
}

void ReleaseUnderStar(Group* group, AlgalonEncounterState& state, AlgalonScanUnit const& star, char const* why,
                      int32 toCastMs)
{
    state.underReleased.insert(star.guid);
    if (!RaidObs::Active())
        return;

    if (Player* tracker = FindTracker(group))
        RaidObs::Note(tracker, "algalon.underdrop",
                      std::string(why) + " " + std::to_string(star.guid.GetCounter()) + " " +
                          std::to_string(static_cast<int32>(star.healthPct)) + " " + std::to_string(toCastMs));
}

void PlanUnderStars(Group* group, AlgalonEncounterState& state, uint32 now)
{
    if (UnderFrozen(state, now))
    {
        MarkUnder(state, now);
        return;
    }

    if (state.underPlanCycle != state.bigBangCount)
    {
        if (state.bulkMeasuredCycle != state.underPlanCycle)
            state.bulkPctPerSec = 0.0f;

        state.underPlanCycle = state.bigBangCount;
        state.underStars.clear();
        state.underReleased.clear();
        state.underLateMs.clear();
        state.bulkStar = ObjectGuid::Empty;
        state.finishLatch = ObjectGuid::Empty;
        state.finisherLatch = ObjectGuid::Empty;
    }

    PlanShelterStar(state);
    if (!state.nextBigBangMs || state.phase.Get() != AlgalonPhase::One)
    {
        state.underStars.clear();
        MarkUnder(state, now);
        return;
    }

    int32 const toCast = MsUntil(state.nextBigBangMs, now);

    if (state.disperser.Get().IsEmpty())
    {
        if (!state.underNoSoakMs)
            state.underNoSoakMs = now;

        if (getMSTimeDiff(state.underNoSoakMs, now) >= ULDUAR_ALGALON_UNDER_SOAKER_GRACE_MS)
        {
            for (AlgalonScanUnit const& star : state.stars)
                if (state.underStars.count(star.guid))
                    ReleaseUnderStar(group, state, star, "nosoak", toCast);

            state.underStars.clear();
        }

        MarkUnder(state, now);
        return;
    }

    state.underNoSoakMs = 0;
    int32 const toHit = toCast + static_cast<int32>(ULDUAR_ALGALON_BIG_BANG_CAST_MS);
    int32 const from = toHit - ULDUAR_ALGALON_UNDER_LEAD_MS;
    int32 const to = toHit + ULDUAR_ALGALON_UNDER_TAIL_MS;
    int32 const stretchFrom = toHit - (ULDUAR_ALGALON_HIDE_MARGIN_MS + ULDUAR_ALGALON_UNDER_HIDE_STRETCH_MS -
                                       ULDUAR_ALGALON_UNDER_DEATH_LEAD_MS);
    state.underFromMs = now + static_cast<uint32>(std::max(0, from));
    state.underToMs = now + static_cast<uint32>(std::max(0, to));

    std::unordered_set<ObjectGuid> const planned = std::move(state.underStars);
    state.underStars.clear();
    bool const releasing =
        toCast <= ULDUAR_ALGALON_UNDER_RELEASE_FROM_MS && toCast > ULDUAR_ALGALON_UNDER_RELEASE_TO_MS;
    std::vector<AlgalonScanUnit*> kept;
    std::vector<AlgalonScanUnit*> candidates;
    for (AlgalonScanUnit& star : state.stars)
    {
        star.underLo = 0;
        star.underHi = 0;
        if (state.underReleased.count(star.guid) || star.guid == state.shelterStar || !star.tickHp ||
            !UnderBand(star.tickHp, star.nextTickMs, from, to, star.underLo, star.underHi))
            continue;

        if (!planned.count(star.guid))
        {
            if (DrainDeathMs(star) >= from)
                candidates.push_back(&star);

            continue;
        }

        if (DrainDeathMs(star) < stretchFrom)
        {
            ReleaseUnderStar(group, state, star, "early", toCast);
            continue;
        }

        kept.push_back(&star);
    }

    std::sort(kept.begin(), kept.end(), [&state](AlgalonScanUnit const* a, AlgalonScanUnit const* b)
              {
                  bool const aTaken = a->guid == state.bulkStar || a->guid == state.finishLatch;
                  bool const bTaken = b->guid == state.bulkStar || b->guid == state.finishLatch;
                  if (aTaken != bTaken)
                      return aTaken;

                  return BulkLeftHp(*a) > BulkLeftHp(*b);
              });

    int32 work = 0;
    for (AlgalonScanUnit* star : kept)
    {
        int32 const own = BulkWorkMs(state, *star);
        if (releasing && work + own > toCast)
        {
            auto const since = state.underLateMs.try_emplace(star->guid, now).first;
            if (getMSTimeDiff(since->second, now) >= ULDUAR_ALGALON_UNDER_LATE_HOLD_MS)
            {
                ReleaseUnderStar(group, state, *star, "late", toCast);
                continue;
            }
        }
        else
            state.underLateMs.erase(star->guid);

        work += own;
        state.underStars.insert(star->guid);
    }

    int32 const available = toCast - ULDUAR_ALGALON_UNDER_FINISH_MS;
    std::sort(candidates.begin(), candidates.end(), [&state](AlgalonScanUnit const* a, AlgalonScanUnit const* b)
              { return BulkWorkMs(state, *a) < BulkWorkMs(state, *b); });

    for (AlgalonScanUnit const* star : candidates)
    {
        int32 const own = BulkWorkMs(state, *star);
        if (work + own > available)
            break;

        work += own;
        state.underStars.insert(star->guid);
    }

    MarkUnder(state, now);
}

uint8 FinisherRank(Player* member)
{
    switch (member->getClass())
    {
        case CLASS_HUNTER:
            return 0;
        case CLASS_PRIEST:
            return 1;
        case CLASS_SHAMAN:
            return 2;
        case CLASS_DRUID:
            return 3;
        case CLASS_WARLOCK:
            return 4;
        case CLASS_MAGE:
            return 5;
        default:
            return 6;
    }
}

Player* ElectFinisher(Group* group, AlgalonEncounterState const& state)
{
    Player* best = nullptr;
    uint32 able = 0;
    for (ObjectGuid const& guid : state.starTeam)
    {
        Player* member = FindGroupMember(group, guid);
        if (!IsBot(member) || !CanStayOut(member))
            continue;

        ++able;
        if (!best || FinisherRank(member) < FinisherRank(best) ||
            (FinisherRank(member) == FinisherRank(best) && member->GetGUID() < best->GetGUID()))
            best = member;
    }

    return able >= 2 ? best : nullptr;
}

void ValidateUnderFinish(Group* group, AlgalonEncounterState& state)
{
    AlgalonScanUnit const* finish = FindScanned(state.stars, state.finishLatch);
    if (finish && finish->under && BulkLeftHp(*finish) && state.starTeam.count(state.finisherLatch) &&
        CanStayOut(FindGroupMember(group, state.finisherLatch)))
        return;

    state.finishLatch = ObjectGuid::Empty;
    state.finisherLatch = ObjectGuid::Empty;
}

uint32 FinishHandOverHp(AlgalonEncounterState const& state, AlgalonScanUnit const& star)
{
    uint32 const pair = ULDUAR_ALGALON_STAR_TEAM_10;
    uint32 const members = std::max(static_cast<uint32>(state.starTeam.size()), pair);
    return star.tickHp * ULDUAR_ALGALON_UNDER_FINISH_TICKS * members / pair;
}

bool HandOverFinish(Group* group, AlgalonEncounterState& state, AlgalonScanUnit const& star)
{
    uint32 const left = BulkLeftHp(star);
    if (!state.finishLatch.IsEmpty() || !left || left > FinishHandOverHp(state, star))
        return false;

    Player* finisher = ElectFinisher(group, state);
    if (!finisher)
        return false;

    state.finishLatch = star.guid;
    state.finisherLatch = finisher->GetGUID();
    if (state.bulkStar == star.guid)
        state.bulkStar = ObjectGuid::Empty;

    return true;
}

AlgalonScanUnit const* PickBulkStar(AlgalonEncounterState const& state)
{
    AlgalonScanUnit const* next = nullptr;
    for (AlgalonScanUnit const& star : state.stars)
        if (star.under && star.guid != state.finishLatch && BulkLeftHp(star) &&
            (!next || BulkLeftHp(star) > BulkLeftHp(*next)))
            next = &star;

    return next;
}

AlgalonScanUnit const* UnderBulkStar(Group* group, AlgalonEncounterState& state, uint32 now)
{
    ValidateUnderFinish(group, state);
    AlgalonScanUnit const* latched = FindScanned(state.stars, state.bulkStar);
    if (!latched || !latched->under || !BulkLeftHp(*latched))
    {
        state.bulkStar = ObjectGuid::Empty;
        latched = nullptr;
    }

    if (state.bigBangCasting || !state.nextBigBangMs)
        return nullptr;

    if (latched && !HandOverFinish(group, state, *latched))
        return latched;

    int32 work = 0;
    for (AlgalonScanUnit const& star : state.stars)
        if (star.under && BulkLeftHp(star))
            work += BulkWorkMs(state, star);

    if (!latched && MsUntil(state.nextBigBangMs, now) > work + ULDUAR_ALGALON_UNDER_BULK_SLACK_MS)
        return nullptr;

    AlgalonScanUnit const* next = PickBulkStar(state);
    if (next && HandOverFinish(group, state, *next))
        next = PickBulkStar(state);

    if (!next)
        return nullptr;

    state.bulkStar = next->guid;
    return next;
}

void StopCastsAtLine(Player* bot, Group* group, AlgalonEncounterState const& state, char const* window)
{
    ObjectGuid const skull = group->GetTargetIcon(RtiTargetValue::skullIndex);
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!IsBot(member))
            continue;

        ObjectGuid const focus = StarFocusOf(state, member->GetGUID());
        for (CurrentSpellTypes const type : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL, CURRENT_AUTOREPEAT_SPELL})
        {
            Spell* spell = member->GetCurrentSpell(type);
            ObjectGuid const target = spell ? spell->m_targets.GetUnitTargetGUID() : ObjectGuid::Empty;
            AlgalonScanUnit const* scanned = target.IsEmpty() ? nullptr : FindScanned(state.stars, target);
            if (!scanned || !scanned->under ||
                (target == focus && target == state.focusStar.Get() && std::strcmp(window, "under")))
                continue;

            bool const left = target != focus && target != skull;
            Creature* star = Resolve(bot, target);
            if (star && (left || star->GetHealth() <= StopLineHp(*scanned) + scanned->pendingHp))
                member->InterruptSpell(type);
        }
    }
}

void CommitBulkSpeed(AlgalonEncounterState& state, uint32 now)
{
    AlgalonScanUnit const* star = FindScanned(state.stars, state.bulkFrom);
    uint32 const span = getMSTimeDiff(state.bulkFromMs, now);
    if (star && span >= ULDUAR_ALGALON_STAR_LOG_MS && star->healthPct < state.bulkFromPct)
    {
        float const speed = (state.bulkFromPct - star->healthPct) * IN_MILLISECONDS / span;
        state.bulkPctPerSec = std::min(speed, ULDUAR_ALGALON_STAR_KILL_PEAK_PCT_PER_SEC);
        state.bulkTeam = state.bulkFromTeam;
        state.bulkMeasuredCycle = state.bigBangCount;
    }

    state.bulkFrom = ObjectGuid::Empty;
}

int32 CastStarDeathMs(AlgalonEncounterState const& state, int32 remaining, uint32 now)
{
    int32 earliest = std::numeric_limits<int32>::max();
    if (!state.bigBangCasting)
        return earliest;

    int32 const age = state.lastTickMs ? static_cast<int32>(getMSTimeDiff(state.lastTickMs, now)) : 0;
    for (AlgalonScanUnit const& star : state.stars)
    {
        if (!star.tickHp)
            continue;

        int32 const death = DrainDeathMs(star) - age;
        if (star.under || death <= remaining + ULDUAR_ALGALON_UNDER_TAIL_MS)
            earliest = std::min(earliest, death);
    }

    return earliest;
}

int32 UnderHideExtraMs(AlgalonEncounterState const& state, int32 remaining, uint32 now)
{
    int32 const earliest = CastStarDeathMs(state, remaining, now);
    if (earliest == std::numeric_limits<int32>::max())
        return state.bigBangCasting && !state.underStars.empty() ? ULDUAR_ALGALON_UNDER_HIDE_EXTRA_MS : 0;

    int32 const extra =
        std::max(ULDUAR_ALGALON_UNDER_HIDE_EXTRA_MS,
                 remaining - earliest + ULDUAR_ALGALON_UNDER_DEATH_LEAD_MS - ULDUAR_ALGALON_HIDE_MARGIN_MS);
    return std::min(extra, ULDUAR_ALGALON_UNDER_HIDE_STRETCH_MS);
}

bool PetSpellSplashes(SpellInfo const* info)
{
    if (!info || info->IsPositive())
        return false;

    if (info->IsTargetingArea() || info->IsAffectingArea())
        return true;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (info->Effects[i].ChainTarget > 1)
            return true;

    return false;
}

bool StarNearPet(AlgalonEncounterState const& state, Pet* pet)
{
    Unit* victim = pet->GetVictim();
    ObjectGuid const except = victim ? victim->GetGUID() : ObjectGuid::Empty;
    float const aroundVictim =
        victim ? std::max(ULDUAR_ALGALON_SPLASH_RADIUS, ULDUAR_ALGALON_SPLASH_PAST_REACH + victim->GetCombatReach())
               : 0.0f;
    for (AlgalonScanUnit const& star : state.stars)
    {
        if (star.guid == except)
            continue;

        if (pet->GetExactDist2d(star.position) <= ULDUAR_ALGALON_SPLASH_RADIUS ||
            (victim && victim->GetExactDist2d(star.position) <= aroundVictim))
            return true;
    }

    return false;
}

void HoldPetSplash(Group* group, AlgalonEncounterState& state, bool active)
{
    if (!group)
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        Pet* pet = IsBot(member) && InFight(member) ? member->GetPet() : nullptr;
        if (!pet || !pet->IsAlive())
            continue;

        auto held = state.petSplashHeld.find(member->GetGUID());
        if (active && StarNearPet(state, pet))
        {
            std::vector<uint32> splash;
            for (uint8 pos = 0; pos < pet->GetPetAutoSpellSize(); ++pos)
            {
                uint32 const spellId = pet->GetPetAutoSpellOnPos(pos);
                if (pet->HasSpell(spellId) && PetSpellSplashes(sSpellMgr->GetSpellInfo(spellId)))
                    splash.push_back(spellId);
            }

            for (uint32 const spellId : splash)
            {
                pet->ToggleAutocast(sSpellMgr->GetSpellInfo(spellId), false);
                std::vector<uint32>& list = state.petSplashHeld[member->GetGUID()];
                if (std::find(list.begin(), list.end(), spellId) == list.end())
                    list.push_back(spellId);

                if (RaidObs::Active())
                    RaidObs::Note(member, "algalon.petsplash", "off " + std::to_string(spellId));
            }

            continue;
        }

        if (held == state.petSplashHeld.end())
            continue;

        for (uint32 const spellId : held->second)
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
            if (!info || !pet->HasSpell(spellId))
                continue;

            pet->ToggleAutocast(info, true);
            if (RaidObs::Active())
                RaidObs::Note(member, "algalon.petsplash", "on " + std::to_string(spellId));
        }

        state.petSplashHeld.erase(held);
    }
}

void BreakSpentIceBlocks(Group* group, AlgalonEncounterState& state)
{
    for (auto pressed = state.personalBlocks.begin(); pressed != state.personalBlocks.end();)
    {
        Player* member = FindGroupMember(group, pressed->first);
        if (!member || !member->HasAura(SPELL_ALGALON_ICE_BLOCK))
        {
            pressed = state.personalBlocks.erase(pressed);
            continue;
        }

        if (state.bigBangCasting || IsAlgalonPhased(member) || FindScanned(state.stars, pressed->second) ||
            ExplodingStar(state, ULDUAR_ALGALON_EXPLOSION_PERSONAL_LEAD_MS + ULDUAR_ALGALON_EXPLOSION_STACK_MS,
                          ULDUAR_ALGALON_EXPLOSION_LOW_PCT, member))
        {
            ++pressed;
            continue;
        }

        member->RemoveAurasDueToSpell(SPELL_ALGALON_ICE_BLOCK);
        if (RaidObs::Active())
            RaidObs::Note(member, "algalon.unblock", "Ice Block");

        pressed = state.personalBlocks.erase(pressed);
    }
}
}

void AlgalonTickEncounterState(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return;

    uint32 const instanceId = bot->GetInstanceId();
    AlgalonEncounterState* state = &algalonEncounterStates.For(instanceId);
    uint32 const now = getMSTime();
    if (state->lastTickMs && getMSTimeDiff(state->lastTickMs, now) < ULDUAR_ALGALON_STATE_TICK_MS)
        return;

    state->lastTickMs = now;

    Creature* boss = FindAlgalon(bot);

    bool const wiped = !state->boss.IsEmpty() &&
                       (!boss || boss->GetGUID() != state->boss || (state->engagedSeen && !boss->IsInCombat()));
    if (wiped)
    {
        HoldPetSplash(bot->GetGroup(), *state, false);
        algalonEncounterStates.Reset(instanceId);
        state = &algalonEncounterStates.For(instanceId);
        state->lastTickMs = now;
    }

    if (!boss)
        return;

    state->boss = boss->GetGUID();
    if (boss->IsInCombat())
        state->engagedSeen = true;

    Group* group = bot->GetGroup();
    if (!group)
        return;

    std::vector<Player*> const tanks = OrderedTanks(bot);
    EnsureSwapPair(bot, boss, *state, tanks);
    EnsureTankHealers(bot, boss, *state);

    if (!state->introEndMs && boss->IsInCombat() && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
    {
        state->introEndMs = now;
        state->nextBigBangMs = now + ULDUAR_ALGALON_FIRST_BIG_BANG_MS;
        state->nextSmashImpactMs = now + ULDUAR_ALGALON_FIRST_SMASH_IMPACT_MS;
        NotePull(group, boss, *state);
    }

    EnsureAlgalonSlotAssignments(bot, *state);

    if (boss->IsInCombat())
        ScanRoom(bot, boss, *state, now);

    AlgalonPhase const phase = ReadPhase(boss, *state);
    state->phase = phase;
    state->engaged = phase == AlgalonPhase::Intro || phase == AlgalonPhase::One || phase == AlgalonPhase::Two;
    HoldPetSplash(group, *state, phase == AlgalonPhase::One);
    if (!state->engaged)
        return;

    ReleaseMelee(boss, *state, now);

    if (!state->firstHoldMs && state->introEndMs)
    {
        Player* holder = BossHolder(boss);
        if (holder && state->swapPair.count(holder->GetGUID()))
        {
            state->firstHoldMs = now;
            if (getMSTimeDiff(state->introEndMs, now) <= ULDUAR_ALGALON_PULL_HOLD_MS)
                state->firstHolder = holder->GetGUID();
        }
    }

    while (state->nextSmashImpactMs && MsUntil(state->nextSmashImpactMs, now) +
                                               static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_TAIL_MS) < 0)
        state->nextSmashImpactMs += ULDUAR_ALGALON_COSMIC_SMASH_INTERVAL_MS;

    bool const casting = CurrentBigBang(boss) != nullptr;
    if (casting && !state->bigBangCasting)
    {
        ++state->bigBangCount;
        state->nextBigBangMs = now + ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS;
        state->shelterAssignments.clear();
        if (Spell* bang = CurrentBigBang(boss))
        {
            uint32 const hit = now + static_cast<uint32>(std::max(0, bang->GetCastTimeRemaining()));
            state->underFromMs = hit - ULDUAR_ALGALON_UNDER_LEAD_MS;
            state->underToMs = hit + ULDUAR_ALGALON_UNDER_TAIL_MS;
        }

        state->nitroRunners.clear();
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (IsBot(member) && NitroBoostsReady(member))
                state->nitroRunners.insert(member->GetGUID());
        }
        ElectBigBangSoakers(bot, boss, *state);
        NoteSoakKit(group, *state);
        CallBigBang(group, boss, *state, true);
        state->humanSoakCalled = false;
    }
    else if (casting)
    {
        KeepBigBangSoaker(bot, *state);
        CallBigBang(group, boss, *state, false);
        CallHumanSoak(group, boss, *state);
    }
    else if (state->bigBangCasting)
    {
        state->lastBigBangEndMs = now;
        state->lastSoaker = state->bigBangSoaker.Get();
        state->bigBangSoaker = ObjectGuid::Empty;
        state->bigBangBackup = ObjectGuid::Empty;
    }

    state->bigBangCasting = casting;
    state->bigBang = casting ? state->bigBangCount : 0;
    state->holeCount = static_cast<uint32>(state->holes.size());
    CountHidersOut(group, *state);

    if (!casting)
    {
        int32 const castBy = state->nextBigBangMs ? MsUntil(state->nextBigBangMs, now) +
                                                        static_cast<int32>(ULDUAR_ALGALON_BIG_BANG_CAST_MS) -
                                                        ULDUAR_ALGALON_SOAK_DISPERSION_MS
                                                  : std::numeric_limits<int32>::max();
        Player* disperser = ElectDisperser(group, castBy, nullptr);
        state->disperser = disperser ? disperser->GetGUID() : ObjectGuid::Empty;
    }

    for (auto const& [marker, seenMs] : state->markerSeenMs)
    {
        uint32 const impact = seenMs + ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_MS;
        if (MsUntil(impact, now) <= 0 && (!state->lastSmashImpactMs || MsUntil(impact, state->lastSmashImpactMs) > 0))
            state->lastSmashImpactMs = impact;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (IsBot(member) && member->HasAura(SPELL_ALGALON_DIVINE_SACRIFICE, member->GetGUID()))
            member->RemoveOwnedAura(SPELL_ALGALON_DIVINE_SACRIFICE);
    }

    BreakSpentIceBlocks(group, *state);

    state->urgent = phase == AlgalonPhase::One && state->holes.empty() &&
                    (casting || BigBangDueWithin(*state, ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS, now));

    Player* handler = ElectHandler(bot, *state, tanks);
    state->handler = handler ? handler->GetGUID() : ObjectGuid::Empty;

    Player* fireTotem = ElectFireTotemShaman(group, state->fireTotemShaman.Get());
    state->fireTotemShaman = fireTotem ? fireTotem->GetGUID() : ObjectGuid::Empty;

    if (phase != AlgalonPhase::Intro)
    {
        CallHumanTank(group, botAI, boss, *state, now);
        CallHumanTankCut(group, botAI, boss, *state, now);
        NoteTankRoles(group, boss, *state, now);
        NoteHandover(group, boss, *state, now);
    }

    if (phase != AlgalonPhase::One)
    {
        state->focusStar = ObjectGuid::Empty;
        state->finishStar = ObjectGuid::Empty;
        state->finisher = ObjectGuid::Empty;
        state->finishLatch = ObjectGuid::Empty;
        state->finisherLatch = ObjectGuid::Empty;
        state->shadowAuraPaladin = ObjectGuid::Empty;
        return;
    }

    Player* auraPaladin = ElectShadowAuraPaladin(group, state->shadowAuraPaladin.Get());
    state->shadowAuraPaladin = auraPaladin ? auraPaladin->GetGUID() : ObjectGuid::Empty;

    if (AlgalonScanUnit const* focus = FindScanned(state->stars, state->focusStar.Get()))
        if (focus->lossMeasured && focus->lossPctPerSec >= ULDUAR_ALGALON_STAR_KILL_MEASURED_PCT_PER_SEC)
        {
            state->teamKillPctPerSec = std::min(focus->lossPctPerSec, ULDUAR_ALGALON_STAR_KILL_PEAK_PCT_PER_SEC);
            state->teamKillTeam = static_cast<uint32>(state->starTeam.size());
        }

    ElectStarTeam(bot, *state);
    PlanUnderStars(group, *state, now);

    AlgalonScanUnit const* star = nullptr;
    char const* window = StarWindow(bot, boss, *state, now, star);
    AlgalonScanUnit const* finish = nullptr;
    if (!star && !state->bigBangCasting)
    {
        if (AlgalonScanUnit const* bulk = UnderBulkStar(group, *state, now))
        {
            star = bulk;
            window = "under";
        }

        finish = FindScanned(state->stars, state->finishLatch);
    }
    ObjectGuid const previous = state->focusStar.Get();
    ObjectGuid const next = star ? star->guid : ObjectGuid::Empty;
    for (auto left = state->starLeft.begin(); left != state->starLeft.end();)
        left = getMSTimeDiff(left->second.first, now) > ULDUAR_ALGALON_STAR_LOG_MS ? state->starLeft.erase(left)
                                                                                    : std::next(left);

    if (next != previous)
    {
        state->focusByDeadline = false;
        state->focusSinceMs = now;
        CommitBulkSpeed(*state, now);
        if (AlgalonScanUnit const* left = FindScanned(state->stars, previous))
            state->starLeft[previous] = {now, left->healthPct};

        state->starLeft.erase(next);
    }

    if (star && !std::strcmp(window, "deadline"))
        state->focusByDeadline = true;

    state->focusStar = next;
    state->finishStar = finish ? finish->guid : ObjectGuid::Empty;
    state->finisher = finish ? state->finisherLatch : ObjectGuid::Empty;
    StopCastsAtLine(bot, group, *state, window);

    if (star && !std::strcmp(window, "under") && state->bulkFrom.IsEmpty() &&
        getMSTimeDiff(state->focusSinceMs, now) >= static_cast<uint32>(ULDUAR_ALGALON_UNDER_SWITCH_MS))
    {
        state->bulkFrom = star->guid;
        state->bulkFromMs = now;
        state->bulkFromPct = star->healthPct;
        state->bulkFromTeam = static_cast<uint32>(state->starTeam.size());
    }

    if (RaidObs::Active())
    {
        if (Player* tracker = FindTracker(group))
        {
            RaidObs::NoteDerived(tracker, "algalon.starwindow", window);
            if (state->lastStarWindow != window)
                RaidObs::NoteDerived(tracker, "algalon.starplan", DescribeStarPlan(*state, now));
        }

        state->lastStarWindow = window;
    }
}

bool AlgalonBigBangCasting(PlayerbotAI* botAI) { return CurrentBigBang(GetAlgalon(botAI)) != nullptr; }

int32 AlgalonBigBangRemainingMs(PlayerbotAI* botAI)
{
    Spell* spell = CurrentBigBang(GetAlgalon(botAI));
    return spell ? spell->GetCastTimeRemaining() : -1;
}

bool AlgalonBigBangWithin(PlayerbotAI* botAI, uint32 seconds)
{
    AlgalonEncounterState const* state = FindState(botAI ? botAI->GetBot() : nullptr);
    return state && BigBangDueWithin(*state, seconds, getMSTime());
}

bool AlgalonBigBangLatched(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    return state && state->engaged && state->bigBangCasting;
}

int32 AlgalonMsToBigBangHit(PlayerbotAI* botAI)
{
    AlgalonEncounterState const* state = FindState(botAI ? botAI->GetBot() : nullptr);
    if (!state || !state->engaged || !state->nextBigBangMs)
        return std::numeric_limits<int32>::max();

    if (state->bigBangCasting)
        return 0;

    int32 const left = static_cast<int32>(state->nextBigBangMs - getMSTime()) +
                       static_cast<int32>(ULDUAR_ALGALON_BIG_BANG_CAST_MS);
    return std::max(0, left);
}

Player* GetAlgalonRescueTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !state->engaged || !state->lastBigBangEndMs ||
        getMSTimeDiff(state->lastBigBangEndMs, getMSTime()) > ULDUAR_ALGALON_RESCUE_WINDOW_MS)
        return nullptr;

    Player* soaker = FindGroupMember(bot->GetGroup(), state->lastSoaker);
    return CanStayOut(soaker) && soaker->GetHealthPct() < ULDUAR_ALGALON_RESCUE_HP_PCT ? soaker : nullptr;
}

char const* GetAlgalonSoakerExternal(PlayerbotAI* botAI, Player* soaker)
{
    if (!botAI || !soaker)
        return nullptr;

    if (!soaker->HasAura(SPELL_ALGALON_PAIN_SUPPRESSION) && botAI->CanCastSpell("pain suppression", soaker))
        return "pain suppression";

    if (!soaker->HasAura(SPELL_ALGALON_GUARDIAN_SPIRIT) && botAI->CanCastSpell("guardian spirit", soaker))
        return "guardian spirit";

    if (!soaker->HasAura(SPELL_ALGALON_HAND_OF_SACRIFICE) && botAI->CanCastSpell("hand of sacrifice", soaker))
        return "hand of sacrifice";

    return nullptr;
}

char const* GetAlgalonSoakerPrep(PlayerbotAI* botAI, Player* soaker)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !soaker)
        return nullptr;

    std::vector<char const*> layers;
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            if (!soaker->HasAura(SPELL_ALGALON_WEAKENED_SOUL))
                layers.push_back("power word: shield");
            layers.push_back("renew");
            break;
        case CLASS_DRUID:
            layers = {"rejuvenation", "lifebloom"};
            break;
        case CLASS_SHAMAN:
            layers.push_back("riptide");
            if (!botAI->HasAura("earth shield", soaker))
                layers.push_back("earth shield");
            break;
        case CLASS_PALADIN:
            layers.push_back("sacred shield");
            break;
        default:
            break;
    }

    for (char const* layer : layers)
        if (!botAI->HasAura(layer, soaker, false, true) && botAI->CanCastSpell(layer, soaker))
            return layer;

    return nullptr;
}

char const* GetAlgalonRescueSpell(PlayerbotAI* botAI, Player* soaker)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !soaker)
        return nullptr;

    std::vector<char const*> heals;
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
            if (soaker->GetHealthPct() < ULDUAR_ALGALON_RESCUE_LAY_ON_HANDS_PCT)
                heals.push_back("lay on hands");
            heals.push_back("holy shock");
            heals.push_back("flash of light");
            break;
        case CLASS_SHAMAN:
            heals = {"riptide", "lesser healing wave"};
            break;
        case CLASS_DRUID:
            heals = {"swiftmend", "nourish"};
            break;
        case CLASS_PRIEST:
            heals.push_back("penance");
            if (!soaker->HasAura(SPELL_ALGALON_WEAKENED_SOUL))
                heals.push_back("power word: shield");
            heals.push_back("flash heal");
            break;
        default:
            break;
    }

    for (char const* heal : heals)
        if (botAI->CanCastSpell(heal, soaker))
            return heal;

    return nullptr;
}

bool IsAlgalonTankHealer(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    return state && state->engaged && state->tankHealers.count(bot->GetGUID());
}

AlgalonTankHealPick GetAlgalonTankHealPick(PlayerbotAI* botAI)
{
    AlgalonTankHealPick pick;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!state || !group || !state->engaged || !state->tankHealers.count(bot->GetGUID()) || IsAlgalonPhased(bot))
        return pick;

    std::vector<Player*> pair;
    for (ObjectGuid const& guid : state->swapPair)
        if (Player* tank = FindGroupMember(group, guid))
            pair.push_back(tank);

    Player* holder = GetAlgalonBossTank(botAI);
    if (std::find(pair.begin(), pair.end(), holder) == pair.end())
    {
        holder = nullptr;
        for (Player* tank : pair)
            if (CanStayOut(tank) && (!holder || tank->GetHealthPct() < holder->GetHealthPct()))
                holder = tank;
    }

    Player* partner = nullptr;
    for (Player* tank : pair)
        if (tank != holder)
            partner = tank;

    std::optional<bool> holderReach;
    std::optional<bool> partnerReach;
    auto const reachable = [&](Player* tank)
    {
        if (!tank)
            return false;

        std::optional<bool>& reach = tank == holder ? holderReach : partnerReach;
        if (!reach)
            reach = TankHealReachable(bot, tank);

        return *reach;
    };
    auto const below = [](Player* unit, float pct) { return unit && unit->GetHealthPct() < pct; };

    char const* branch = "full";
    if (below(holder, ULDUAR_ALGALON_TANK_CRITICAL_PCT) && reachable(holder))
    {
        pick = {holder, true};
        branch = "holder";
    }
    else if (below(bot, ULDUAR_ALGALON_TANK_HEALER_SELF_PCT))
    {
        pick = {bot, true};
        branch = "self";
    }
    else if (below(partner, ULDUAR_ALGALON_TANK_CRITICAL_PCT) && reachable(partner))
    {
        pick = {partner, true};
        branch = "offtank";
    }
    else if (below(holder, ULDUAR_ALGALON_HOLDER_URGENT_PCT) && reachable(holder))
    {
        pick = {holder, true};
        branch = "holder";
    }
    else
    {
        bool const holderNeeds = below(holder, ULDUAR_ALGALON_HOLDER_TOPOFF_PCT) && reachable(holder);
        bool const partnerNeeds = below(partner, ULDUAR_ALGALON_OFFTANK_TOPOFF_PCT) && reachable(partner);
        bool const partnerFirst = partnerNeeds && holderNeeds &&
                                  partner->GetHealthPct() + ULDUAR_ALGALON_OFFTANK_MARGIN_PCT <= holder->GetHealthPct();
        if (holderNeeds && !partnerFirst)
        {
            pick.target = holder;
            branch = "holder";
        }
        else if (partnerNeeds)
        {
            pick.target = partner;
            branch = "offtank";
        }
        else if ((below(holder, ULDUAR_ALGALON_HOLDER_TOPOFF_PCT) && !reachable(holder)) ||
                 (below(partner, ULDUAR_ALGALON_OFFTANK_TOPOFF_PCT) && !reachable(partner)))
        {
            branch = "range";
        }
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "algalon.tankheal", branch);

    return pick;
}

char const* GetAlgalonTankHealSpell(PlayerbotAI* botAI, AlgalonTankHealPick const& pick)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Player* target = pick.target;
    if (!bot || !target)
        return nullptr;

    bool const onHolder = target != bot && GetAlgalonBossTank(botAI) == target;
    bool const shieldable = !target->HasAura(SPELL_ALGALON_WEAKENED_SOUL);
    std::vector<char const*> spells;
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
            if (pick.critical)
            {
                bool const instantFlash = !CastTimeMs(botAI, "flash of light");
                spells = {"holy shock", instantFlash ? "flash of light" : "holy light",
                          instantFlash ? "holy light" : "flash of light"};
            }
            else
            {
                if (onHolder)
                    spells.push_back("sacred shield");
                spells.insert(spells.end(), {"holy light", "flash of light"});
            }
            break;
        case CLASS_PRIEST:
            if (shieldable)
                spells.push_back("power word: shield");
            if (pick.critical)
                spells.insert(spells.end(), {"penance", "flash heal"});
            else
                spells.insert(spells.end(), {"prayer of mending", "renew", "penance", "greater heal", "flash heal"});
            break;
        case CLASS_DRUID:
            if (pick.critical)
                spells = {"swiftmend", "nourish", "regrowth"};
            else
                spells = {"lifebloom", "rejuvenation", "nourish", "regrowth"};
            break;
        case CLASS_SHAMAN:
            if (pick.critical)
                spells = {"riptide", "lesser healing wave", "healing wave"};
            else
            {
                if (onHolder && !botAI->HasAura("earth shield", target))
                    spells.push_back("earth shield");
                spells.insert(spells.end(), {"riptide", "healing wave", "lesser healing wave"});
            }
            break;
        default:
            break;
    }

    static std::set<std::string> const layers = {"sacred shield", "prayer of mending", "renew", "rejuvenation",
                                                 "riptide"};
    int32 const slack = AlgalonHideSlackMs(bot);
    for (char const* spell : spells)
    {
        if (layers.count(spell) && botAI->HasAura(spell, target, false, true))
            continue;

        if (!std::strcmp(spell, "lifebloom") && botAI->HasAura(spell, target, true, true))
            continue;

        if (slack != std::numeric_limits<int32>::max() &&
            static_cast<int32>(CastTimeMs(botAI, spell)) + ULDUAR_ALGALON_PREP_GCD_MS > slack)
            continue;

        if (botAI->CanCastSpell(spell, target))
            return spell;
    }

    return nullptr;
}

Player* GetAlgalonBeaconTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!state || !group || bot->getClass() != CLASS_PALADIN || !IsAlgalonTankHealer(bot))
        return nullptr;

    Player* holder = GetAlgalonBossTank(botAI);
    if (!holder || !state->swapPair.count(holder->GetGUID()))
        holder = GetAlgalonPickupTank(botAI);

    Player* target = nullptr;
    for (ObjectGuid const& guid : state->swapPair)
    {
        Player* tank = FindGroupMember(group, guid);
        if (tank && tank != holder && TankHealReachable(bot, tank))
            target = tank;
    }

    if (!target && holder && TankHealReachable(bot, holder))
        target = holder;

    return target && !botAI->HasAura("beacon of light", target, false, true) ? target : nullptr;
}

bool AlgalonDispersionSoak(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    return state && state->engaged && !state->disperser.Get().IsEmpty();
}

namespace
{
Player* ExplosionCoverProvider(PlayerbotAI* botAI, char const*& cover);
}

AlgalonTankCut GetAlgalonTankCut(PlayerbotAI* botAI)
{
    AlgalonTankCut const none;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!state || !group || !state->engaged || IsAlgalonPhased(bot))
        return none;

    Player* holder = GetAlgalonBossTank(botAI);
    if (!holder || !IsAlgalonSwapTank(holder) || !CanStayOut(holder) ||
        (bot != holder && bot->getClass() != CLASS_PALADIN && bot->getClass() != CLASS_PRIEST))
        return none;

    uint32 const now = getMSTime();
    if (state->bigBangCasting ||
        (state->lastBigBangEndMs && getMSTimeDiff(state->lastBigBangEndMs, now) < ULDUAR_ALGALON_BIG_BANG_STILL_MS &&
         state->lastSoaker != holder->GetGUID()))
        return none;

    auto const claimed = [now](uint32 claimMs)
    { return claimMs && getMSTimeDiff(claimMs, now) < ULDUAR_ALGALON_TANK_DEFENSIVE_CLAIM_MS; };

    int32 const untilHit = AlgalonMsToBigBangHit(botAI);
    bool const reserved = state->disperser.Get().IsEmpty() && AlgalonLoneHolder(botAI) != holder;
    auto const usable = [&](Player* provider, uint32 spellId)
    { return spellId && (!reserved || SpellCooldownMs(provider, spellId) <= untilHit); };

    auto const ready = [&](Player* provider, uint32 spellId, Player* target)
    { return usable(provider, spellId) && ExternalReaches(provider, spellId, target) ? spellId : 0; };

    if (holder->GetHealthPct() < ULDUAR_ALGALON_TANK_LAY_ON_HANDS_PCT && !claimed(state->layOnHandsClaimMs))
    {
        if (IsBot(holder) && !TankCutRunning(holder) && !claimed(state->tankCutClaimMs))
        {
            if (uint32 const spellId = ready(holder, ExternalSpell(holder, "divine protection"), holder))
                return holder == bot ? AlgalonTankCut{holder, spellId, "low"} : none;
        }

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* paladin = ref->GetSource();
            if (paladin == holder || !IsBot(paladin) || paladin->getClass() != CLASS_PALADIN)
                continue;

            if (uint32 const spellId = ready(paladin, ExternalSpell(paladin, "lay on hands"), holder))
                return paladin == bot ? AlgalonTankCut{holder, spellId, "loh"} : none;
        }

        if (IsBot(holder))
        {
            if (uint32 const spellId = ready(holder, ExternalSpell(holder, "lay on hands"), holder))
                return holder == bot ? AlgalonTankCut{holder, spellId, "loh"} : none;
        }
    }

    if (TankCutRunning(holder) || claimed(state->tankCutClaimMs))
        return none;

    Player* partner = PairPartner(group, *state, holder);
    char const* why = TankCutReason(*state, holder, partner, now);
    if (!why)
        return none;

    auto const turn = [&](Player* provider, uint32 spellId)
    { return provider == bot ? AlgalonTankCut{holder, spellId, why} : none; };

    char const* cover = nullptr;
    Player* coverer = !std::strcmp(why, "window") ? ExplosionCoverProvider(botAI, cover) : nullptr;

    if (IsBot(partner) && partner != coverer)
    {
        if (uint32 const spellId = ready(partner, SacrificeSpell(partner, holder), holder))
            return turn(partner, spellId);
    }

    if (IsBot(holder) && holder != coverer)
    {
        if (char const* own = ReadyTankDefensive(holder, true, ULDUAR_ALGALON_PREP_GCD_MS))
        {
            PlayerbotAI* holderAI = GET_PLAYERBOT_AI(holder);
            uint32 const ownId = holderAI->GetAiObjectContext()->GetValue<uint32>("spell id", own)->Get();
            SpellInfo const* info = ownId ? sSpellMgr->GetSpellInfo(ownId) : nullptr;
            if (usable(holder, ownId) && info && CanPay(holder, info))
            {
                uint32 const spellId = holder == bot ? NextTankDefensive(botAI, bot, nullptr, true) : 0;
                return spellId ? AlgalonTankCut{holder, spellId, why} : none;
            }
        }
    }

    for (bool const healers : {false, true})
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* paladin = ref->GetSource();
            if (!paladin || paladin->getClass() != CLASS_PALADIN || paladin == partner || paladin == coverer ||
                !IsBot(paladin) || PlayerbotAI::IsHeal(paladin) != healers)
                continue;

            if (uint32 const spellId = ready(paladin, SacrificeSpell(paladin, holder), holder))
                return turn(paladin, spellId);
        }
    }

    for (char const* spell : {"pain suppression", "guardian spirit"})
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* priest = ref->GetSource();
            if (!priest || priest->getClass() != CLASS_PRIEST || !IsBot(priest))
                continue;

            if (uint32 const spellId = ready(priest, ExternalSpell(priest, spell), holder))
                return turn(priest, spellId);
        }
    }

    return none;
}

void ClaimAlgalonTankCut(Player* bot, AlgalonTankCut const& cut)
{
    AlgalonEncounterState* state = FindState(bot);
    if (!state || !cut.target)
        return;

    bool const layOnHands = !std::strcmp(cut.why, "loh");
    (layOnHands ? state->layOnHandsClaimMs : state->tankCutClaimMs) = getMSTime();

    if (RaidObs::Active())
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(cut.spellId);
        char pct[16];
        std::snprintf(pct, sizeof(pct), "%.0f", cut.target->GetHealthPct());
        RaidObs::Note(bot, "algalon.tankdef",
                      std::string(cut.why) + " " + pct + " " + RaidObs::DescribeAssignment(cut.target->GetGUID()) +
                          " " + (info ? info->SpellName[0] : "?"));
    }
}

namespace
{
int32 StarDeathMs(AlgalonEncounterState const& state, AlgalonScanUnit const& star)
{
    return star.under && star.tickHp ? DrainDeathMs(star)
                                     : ExplosionDueMs(state, star, ULDUAR_ALGALON_EXPLOSION_LOW_PCT);
}

Player* ExplosionCoverProvider(PlayerbotAI* botAI, char const*& cover)
{
    cover = nullptr;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!state || !group || state->phase.Get() != AlgalonPhase::One)
        return nullptr;

    bool const casting = state->bigBangCasting;
    AlgalonScanUnit const* star = ExplodingStar(*state, ULDUAR_ALGALON_EXPLOSION_COVER_LEAD_MS,
                                                ULDUAR_ALGALON_EXPLOSION_COVER_HP_PCT, bot, casting);
    if (!star || ExplosionCovered(group, *state) ||
        (star->guid == state->coverStar &&
         getMSTimeDiff(state->coverClaimMs, getMSTime()) < ULDUAR_ALGALON_EXPLOSION_COVER_MS))
        return nullptr;

    int32 const death = StarDeathMs(*state, *star);
    auto const hit = [state, casting, death](Player* member)
    { return casting ? CaughtOutside(*state, member, death) : CanStayOut(member); };
    if (RaidUnderExplosionLine(bot, group, hit) < ULDUAR_ALGALON_EXPLOSION_COVER_MIN_UNDER)
        return nullptr;

    Player* auraPaladin = FindGroupMember(group, state->shadowAuraPaladin.Get());
    PlayerbotAI* auraAI = auraPaladin ? GET_PLAYERBOT_AI(auraPaladin) : nullptr;
    if (auraAI && !IsAlgalonPhased(auraPaladin) &&
        auraAI->HasAura("shadow resistance aura", auraPaladin, false, true) &&
        ExternalReady(auraPaladin, "aura mastery", auraPaladin))
    {
        cover = "aura mastery";
        return auraPaladin;
    }

    Player* holder = GetAlgalonBossTank(botAI);
    for (bool const holderTurn : {false, true})
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* paladin = ref->GetSource();
            if ((paladin == holder) != holderTurn || !IsBot(paladin) || paladin->getClass() != CLASS_PALADIN ||
                IsAlgalonPhased(paladin) || !HasDivineGuardian(paladin) ||
                paladin->GetHealthPct() < ULDUAR_ALGALON_SACRIFICE_MIN_PCT)
                continue;

            if (ExternalReady(paladin, "divine sacrifice", paladin))
            {
                cover = "divine sacrifice";
                return paladin;
            }
        }
    }

    return nullptr;
}
}

char const* GetAlgalonExplosionCover(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->getClass() != CLASS_PALADIN)
        return nullptr;

    char const* cover = nullptr;
    return ExplosionCoverProvider(botAI, cover) == bot ? cover : nullptr;
}

void ClaimAlgalonExplosionCover(Player* bot, char const* cover)
{
    AlgalonEncounterState* state = FindState(bot);
    AlgalonScanUnit const* star = state ? ExplodingStar(*state, ULDUAR_ALGALON_EXPLOSION_COVER_LEAD_MS,
                                                        ULDUAR_ALGALON_EXPLOSION_COVER_HP_PCT, bot,
                                                        state->bigBangCasting)
                                        : nullptr;
    if (!star)
        return;

    state->coverStar = star->guid;
    state->coverClaimMs = getMSTime();
    if (RaidObs::Active())
        RaidObs::Note(bot, "algalon.cover", cover);
}

namespace
{
struct PersonalButton
{
    char const* spell;
    uint32 spellId;
    float factor;
    uint32 absorb;
    bool immunity = false;
};

PersonalButton const PERSONAL_BUTTONS[] = {
    {"shadow ward", 0, 1.0f, 3300},
    {"anti magic shell", SPELL_ALGALON_ANTI_MAGIC_SHELL, 0.25f, 0, true},
    {"barkskin", 0, 0.8f, 0},
    {"shamanistic rage", 0, 0.7f, 0},
    {"cloak of shadows", 0, 0.1f, 0, true},
    {"icebound fortitude", 0, 0.7f, 0},
    {"dispersion", 0, 0.1f, 0},
    {"divine protection", 0, 0.5f, 0},
    {"ice block", 0, 0.0f, 0, true},
    {"divine shield", 0, 0.0f, 0, true},
};

uint32 PersonalButtonId(Player* bot, PersonalButton const& button)
{
    if (button.spellId)
        return bot->HasSpell(button.spellId) ? button.spellId : 0;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    return botAI ? botAI->GetAiObjectContext()->GetValue<uint32>("spell id", button.spell)->Get() : 0;
}

int32 ExplosionCoverMs(Player* bot, Group* group, AlgalonEncounterState const& state)
{
    int32 left = 0;
    if (Aura const* guardian = bot->GetAura(SPELL_ALGALON_DIVINE_GUARDIAN))
        left = guardian->GetDuration();

    Player* auraPaladin = FindGroupMember(group, state.shadowAuraPaladin.Get());
    Aura const* mastery = auraPaladin ? auraPaladin->GetAura(SPELL_ALGALON_AURA_MASTERY) : nullptr;
    if (mastery && bot->IsWithinDistInMap(auraPaladin, ULDUAR_ALGALON_AURA_RANGE))
        left = std::max(left, mastery->GetDuration());

    return left;
}

bool ItemCoolingDown(Player* bot, Item* item)
{
    ItemTemplate const* proto = item->GetTemplate();
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        if (proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE && proto->Spells[i].SpellId > 0 &&
            bot->HasSpellCooldown(static_cast<uint32>(proto->Spells[i].SpellId)))
            return true;

    return false;
}

char const* ReadyConsumable(PlayerbotAI* botAI)
{
    AiObjectContext* context = botAI->GetAiObjectContext();
    bool const moving = botAI->GetBot()->isMoving();
    for (char const* name : {"healthstone", "healing potion"})
    {
        if (moving && !std::strcmp(name, "healthstone"))
            continue;

        Action* action = context->GetAction(name);
        if (!action || !action->isPossible() || !action->isUseful())
            continue;

        std::vector<Item*> const items = context->GetValue<std::vector<Item*>>("inventory items", name)->Get();
        if (!items.empty() && !ItemCoolingDown(botAI->GetBot(), items.front()))
            return name;
    }

    return nullptr;
}
}

AlgalonPersonalPick GetAlgalonExplosionPersonal(PlayerbotAI* botAI)
{
    AlgalonPersonalPick pick;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!state || !group || state->phase.Get() != AlgalonPhase::One || IsAlgalonPhased(bot) ||
        state->swapPair.count(bot->GetGUID()))
        return pick;

    bool const casting = state->bigBangCasting;
    AlgalonScanUnit const* star = ExplodingStar(*state, ULDUAR_ALGALON_EXPLOSION_PERSONAL_LEAD_MS,
                                                ULDUAR_ALGALON_EXPLOSION_LOW_PCT, bot, casting);
    if (!star || (casting && !CaughtOutside(*state, bot, StarDeathMs(*state, *star))))
        return pick;

    auto const pressed = state->personalStars.find(bot->GetGUID());
    if (pressed != state->personalStars.end() && pressed->second == star->guid)
        return pick;

    float const topRoll = static_cast<float>(
        bot->GetMap()->Is25ManRaid() ? ULDUAR_ALGALON_STAR_EXPLOSION_MAX_25 : ULDUAR_ALGALON_STAR_EXPLOSION_MAX_10);
    float const hit = StackedHit(*state, bot, *star, topRoll, ExplosionCoverMs(bot, group, *state), casting);
    float const health = static_cast<float>(bot->GetHealth());
    if (health > hit)
        return pick;

    std::array<uint32, std::size(PERSONAL_BUTTONS)> spellIds;
    for (size_t i = 0; i < spellIds.size(); ++i)
    {
        spellIds[i] = PersonalButtonId(bot, PERSONAL_BUTTONS[i]);
        if (spellIds[i] && bot->HasAura(spellIds[i]))
            return pick;
    }

    int32 const untilHit = AlgalonMsToBigBangHit(botAI);
    uint32 strongest = 0;
    float strongestLeft = std::numeric_limits<float>::max();
    for (size_t i = 0; i < spellIds.size(); ++i)
    {
        PersonalButton const& button = PERSONAL_BUTTONS[i];
        float const left = hit * button.factor - button.absorb;
        if ((left >= health && left >= strongestLeft) || (casting && button.immunity))
            continue;

        uint32 const spellId = spellIds[i];
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info || !CanPay(bot, info) || !botAI->CanCastSpell(spellId, bot))
            continue;

        if (!std::strcmp(button.spell, "dispersion") &&
            (state->disperser.Get() == bot->GetGUID() || untilHit == 0 ||
             SpellCooldownMs(bot, spellId) > untilHit - ULDUAR_ALGALON_SOAK_DISPERSION_MS))
            continue;

        if (left < health)
        {
            pick.spellId = spellId;
            return pick;
        }

        strongest = spellId;
        strongestLeft = left;
    }

    float const heal = static_cast<float>(ULDUAR_ALGALON_EXPLOSION_CONSUMABLE_HEAL);
    if (static_cast<float>(bot->GetMaxHealth()) - health >= heal && health + heal > hit)
        pick.consumable = ReadyConsumable(botAI);

    if (!pick.consumable)
        pick.spellId = strongest;

    return pick;
}

void ClaimAlgalonExplosionPersonal(Player* bot, AlgalonPersonalPick const& pick)
{
    AlgalonEncounterState* state = FindState(bot);
    AlgalonScanUnit const* star = state ? ExplodingStar(*state, ULDUAR_ALGALON_EXPLOSION_PERSONAL_LEAD_MS,
                                                        ULDUAR_ALGALON_EXPLOSION_LOW_PCT, bot, state->bigBangCasting)
                                        : nullptr;
    if (!star)
        return;

    state->personalStars[bot->GetGUID()] = star->guid;
    if (pick.spellId == SPELL_ALGALON_ICE_BLOCK)
        state->personalBlocks[bot->GetGUID()] = star->guid;

    if (!RaidObs::Active())
        return;

    SpellInfo const* info = pick.spellId ? sSpellMgr->GetSpellInfo(pick.spellId) : nullptr;
    RaidObs::Note(bot, "algalon.personal", pick.consumable ? pick.consumable : (info ? info->SpellName[0] : "?"));
}

bool IsAlgalonShadowAuraPaladin(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    return state && state->engaged && state->shadowAuraPaladin.Get() == bot->GetGUID();
}

bool IsAlgalonFireTotemShaman(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    return state && state->engaged && state->fireTotemShaman.Get() == bot->GetGUID();
}

Player* GetAlgalonBigBangSoaker(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    return state ? FindGroupMember(bot->GetGroup(), state->bigBangSoaker) : nullptr;
}

Player* GetAlgalonBigBangBackup(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    return state ? FindGroupMember(bot->GetGroup(), state->bigBangBackup) : nullptr;
}

bool AlgalonHoldsConstellation(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    for (AlgalonScanUnit const& constellation : state->constellations)
        if (constellation.active && constellation.victim == bot->GetGUID())
            return true;

    return false;
}

bool AlgalonStaysOut(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    ObjectGuid const guid = bot->GetGUID();
    return state->bigBangSoaker.Get() == guid || state->bigBangBackup.Get() == guid;
}

bool AlgalonTankCaughtOut(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || (!state->swapPair.count(bot->GetGUID()) && state->handler.Get() != bot->GetGUID()))
        return false;

    return GetAlgalonHideRole(bot) == AlgalonHideRole::NoShelter;
}

bool AlgalonCanDisperse(Player* bot)
{
    return bot && bot->HasSpell(SPELL_ALGALON_DISPERSION) && !bot->HasSpellCooldown(SPELL_ALGALON_DISPERSION);
}

namespace
{
int32 HideWalkMs(AlgalonEncounterState const& state, Player* bot, float walk)
{
    float const speed = std::max(1.0f, bot->GetSpeed(MOVE_RUN));
    int32 walkMs = static_cast<int32>(walk / speed * IN_MILLISECONDS);
    if (state.nitroRunners.count(bot->GetGUID()) && !bot->HasAura(NITRO_BOOSTS_SPEED_SPELL) &&
        walk > ULDUAR_ALGALON_NITRO_MIN_WALK)
    {
        float const fast = speed * NITRO_BOOSTS_SPEED_FACTOR;
        float const boosted = fast * NITRO_BOOSTS_DURATION_MS / IN_MILLISECONDS;
        walkMs = walk <= boosted
                     ? static_cast<int32>(walk / fast * IN_MILLISECONDS)
                     : static_cast<int32>(NITRO_BOOSTS_DURATION_MS + (walk - boosted) / speed * IN_MILLISECONDS);
        walkMs = std::max(walkMs, static_cast<int32>(ULDUAR_ALGALON_NITRO_MIN_WALK / speed * IN_MILLISECONDS));
    }

    return walkMs;
}

int32 SmashImpactMs(AlgalonEncounterState const& state, AlgalonScanUnit const& marker, uint32 now)
{
    auto const seen = state.markerSeenMs.find(marker.guid);
    return seen == state.markerSeenMs.end() ? -static_cast<int32>(ULDUAR_ALGALON_MARKER_LIFETIME_MS)
                                            : MsUntil(seen->second + ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_MS, now);
}

Position WalkPoint(Position const& from, Position const& to, float walk, int32 ms, int32 walkMs)
{
    float const length = from.GetExactDist2d(to);
    float const share = walkMs > 0 ? std::clamp(static_cast<float>(ms) / walkMs, 0.0f, 1.0f) : 1.0f;
    float const t = length > 0.0f ? std::min(1.0f, walk * share / length) : 0.0f;
    return Position(from.GetPositionX() + (to.GetPositionX() - from.GetPositionX()) * t,
                    from.GetPositionY() + (to.GetPositionY() - from.GetPositionY()) * t, to.GetPositionZ());
}

AlgalonScanUnit const* SmashAcrossPath(AlgalonEncounterState const& state, Position const& from, Position const& to,
                                       float walk, int32 startMs, int32 walkMs, int32 tailMs, uint32 now)
{
    for (AlgalonScanUnit const& marker : state.markers)
    {
        int32 const into = SmashImpactMs(state, marker, now) - startMs;
        if (into <= -ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_SPREAD_MS || into >= walkMs + tailMs)
            continue;

        Position const at = WalkPoint(from, to, walk, into, walkMs);
        Position const late = WalkPoint(from, to, walk, into + ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_SPREAD_MS, walkMs);
        if (SegmentDistance(at, late.GetPositionX(), late.GetPositionY(), marker.position) <
            ULDUAR_ALGALON_COSMIC_SMASH_MIN_CLEARANCE)
            return &marker;
    }

    return nullptr;
}

bool ClipsOtherHole(AlgalonEncounterState const& state, Position const& from, Position const& via, Position const& to)
{
    for (AlgalonScanUnit const& hole : state.holes)
        if (hole.position.GetExactDist2d(to) > 0.5f &&
            SegmentDistance(from, via.GetPositionX(), via.GetPositionY(), hole.position) <
                ULDUAR_ALGALON_HOLE_PATH_CLEARANCE)
            return true;

    return false;
}

bool FindSmashDetour(Player* bot, AlgalonEncounterState const& state, AlgalonScanUnit const& smash, Position const& to,
                     int32 startMs, uint32 now, Position& waypoint, float& length)
{
    Position const from = bot->GetPosition();
    float best = std::numeric_limits<float>::max();
    for (uint8 step = 0; step < ULDUAR_ALGALON_SWEEP_BEARINGS; ++step)
    {
        float const angle = static_cast<float>(step) * ULDUAR_ALGALON_SWEEP_ANGLE_STEP;
        float const x = smash.position.GetPositionX() + std::cos(angle) * ULDUAR_ALGALON_SMASH_DETOUR_RADIUS;
        float const y = smash.position.GetPositionY() + std::sin(angle) * ULDUAR_ALGALON_SMASH_DETOUR_RADIUS;
        if (!AlgalonSpotInRoom(x, y) || !ClearOfHoles(state, x, y))
            continue;

        Position const point(x, y, to.GetPositionZ());
        if (ClipsOtherHole(state, from, point, to) || ClipsOtherHole(state, point, to, to))
            continue;

        float const first = from.GetExactDist2d(point);
        float const second = std::max(0.0f, point.GetExactDist2d(to) - ULDUAR_ALGALON_SHELTER_IN_RADIUS);
        int32 const walkMs = HideWalkMs(state, bot, first + second);
        int32 const firstMs = first + second > 0.0f ? static_cast<int32>(walkMs * first / (first + second)) : 0;
        if (SmashAcrossPath(state, from, point, first, startMs, firstMs, 0, now) ||
            SmashAcrossPath(state, point, to, second, startMs + firstMs, walkMs - firstMs,
                            ULDUAR_ALGALON_FIELD_PULSE_MS, now))
            continue;

        float const legs = from.GetExactDist2d(x, y) + to.GetExactDist2d(x, y);
        if (legs < best)
        {
            best = legs;
            waypoint = point;
        }
    }

    if (best == std::numeric_limits<float>::max())
        return false;

    length = best;
    return true;
}

float HideRoute(AlgalonEncounterState const& state, Player* bot, Position const& shelter, int32 startMs, Position& next,
                bool& clear)
{
    next = shelter;
    clear = true;
    float const straight = bot->GetExactDist2d(shelter);
    if (state.markers.empty())
        return straight;

    uint32 const now = getMSTime();
    float const walk = std::max(0.0f, straight - ULDUAR_ALGALON_SHELTER_IN_RADIUS);
    AlgalonScanUnit const* smash = SmashAcrossPath(state, bot->GetPosition(), shelter, walk, startMs,
                                                   HideWalkMs(state, bot, walk), ULDUAR_ALGALON_FIELD_PULSE_MS, now);
    if (!smash)
        return straight;

    float length = straight;
    if (!FindSmashDetour(bot, state, *smash, shelter, startMs, now, next, length))
    {
        next = shelter;
        clear = false;
        return straight;
    }

    return length;
}

bool SmashLandsInCast(AlgalonEncounterState const& state, uint32 now)
{
    if (state.markers.empty())
        return false;

    if (state.bigBangCasting)
        return true;

    if (!state.nextBigBangMs)
        return false;

    int32 const castIn = MsUntil(state.nextBigBangMs, now);
    for (AlgalonScanUnit const& marker : state.markers)
        if (SmashImpactMs(state, marker, now) >= castIn)
            return true;

    return false;
}

bool CaughtOutside(AlgalonEncounterState const& state, Player* member, int32 deathMs)
{
    if (!CanStayOut(member) || member->GetGUID() == state.bigBangSoaker.Get())
        return false;

    if (!IsBot(member))
    {
        AlgalonScanUnit const* hole = NearestShelter(state, member->GetPosition());
        if (!hole)
            return true;

        float const walk = std::max(0.0f, member->GetExactDist2d(hole->position) - ULDUAR_ALGALON_SHELTER_IN_RADIUS);
        float const speed = std::max(1.0f, member->GetSpeed(MOVE_RUN));
        return static_cast<int32>(walk / speed * IN_MILLISECONDS) + ULDUAR_ALGALON_LAST_IN_MARGIN_MS > deathMs;
    }

    AlgalonScanUnit const* hole = ShelterHole(member, state);
    int32 const slack = AlgalonHideSlackMs(member);
    if (!hole || slack == std::numeric_limits<int32>::max())
        return true;

    Position next;
    bool clear = true;
    float const route = HideRoute(state, member, hole->position, std::max(0, slack), next, clear);
    int32 const walkMs = HideWalkMs(state, member, std::max(0.0f, route - ULDUAR_ALGALON_SHELTER_IN_RADIUS));
    return std::max(0, slack) + walkMs + ULDUAR_ALGALON_LAST_IN_MARGIN_MS > deathMs;
}
}

int32 AlgalonHideSlackMs(Player* bot)
{
    if (!AlgalonBigBangLatched(bot) || IsAlgalonPhased(bot) || AlgalonStaysOut(bot))
        return std::numeric_limits<int32>::max();

    AlgalonEncounterState const* state = FindState(bot);
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!state || !botAI || state->holes.empty())
        return std::numeric_limits<int32>::max();

    AlgalonScanUnit const* shelter = ShelterHole(bot, *state);
    if (!shelter)
        return std::numeric_limits<int32>::max();

    Position const& to = shelter->position;
    float const straight = std::max(0.0f, bot->GetExactDist2d(to) - ULDUAR_ALGALON_SHELTER_IN_RADIUS);
    int32 const straightMs = HideWalkMs(*state, bot, straight);
    int32 const remaining = AlgalonBigBangRemainingMs(botAI);
    uint32 const now = getMSTime();

    bool const tank = state->swapPair.count(bot->GetGUID()) || state->handler.Get() == bot->GetGUID();
    if (tank && remaining < straightMs + ULDUAR_ALGALON_LAST_IN_MARGIN_MS)
        return std::numeric_limits<int32>::max();

    int32 const underExtra = UnderHideExtraMs(*state, remaining, now);
    int32 margin = ULDUAR_ALGALON_HIDE_MARGIN_MS + underExtra;
    uint32 const explosion =
        bot->GetMap()->Is25ManRaid() ? ULDUAR_ALGALON_STAR_EXPLOSION_MAX_25 : ULDUAR_ALGALON_STAR_EXPLOSION_MAX_10;
    bool last = false;
    if (AlgalonHoldsConstellation(bot) && (!underExtra || (tank && bot->GetHealth() > explosion)))
    {
        auto const out = state->hidersOut.find(shelter->guid);
        last = out != state->hidersOut.end() && out->second;
        if (last)
            margin = ULDUAR_ALGALON_LAST_IN_MARGIN_MS + ULDUAR_ALGALON_HOLD_LEAVE_LEAD_MS;
    }

    int32 const slack = remaining - straightMs - margin;
    int32 const start = std::max(0, slack);
    auto const crossed = [&](int32 from)
    {
        return SmashAcrossPath(*state, bot->GetPosition(), to, straight, from, straightMs,
                               ULDUAR_ALGALON_FIELD_PULSE_MS, now);
    };
    AlgalonScanUnit const* const smash = crossed(start);
    if (!smash)
        return slack;

    if (!last)
    {
        if (start > 0 && state->shelterAssignments.count(bot->GetGUID()) && !crossed(0))
            return 0;

        int32 early = start;
        for (AlgalonScanUnit const* across = smash; across; across = early >= 0 ? crossed(early) : nullptr)
            early = SmashImpactMs(*state, *across, now) - straightMs - ULDUAR_ALGALON_FIELD_PULSE_MS -
                    ULDUAR_ALGALON_SMASH_IN_LEAD_MS;

        if (early >= 0)
            return early;
    }

    Position next;
    bool round = true;
    float const route = HideRoute(*state, bot, to, start, next, round);
    int32 const walkMs = HideWalkMs(*state, bot, std::max(0.0f, route - ULDUAR_ALGALON_SHELTER_IN_RADIUS));
    int32 const routeSlack = remaining - walkMs - margin;

    if (!round || routeSlack > 0 || !state->shelterAssignments.count(bot->GetGUID()))
    {
        int32 late = start;
        for (AlgalonScanUnit const* across = smash; across; across = crossed(late))
            late = SmashImpactMs(*state, *across, now) + ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_SPREAD_MS;

        int32 const waitIn = late + straightMs;
        int32 const go = std::max(0, routeSlack);
        int32 const starDeath = last ? std::numeric_limits<int32>::max() : CastStarDeathMs(*state, remaining, now);
        bool const outUnderStar = waitIn + ULDUAR_ALGALON_LAST_IN_MARGIN_MS > starDeath;
        bool const inFirst =
            round ? go + walkMs < waitIn
                  : go + walkMs + ULDUAR_ALGALON_FIELD_PULSE_MS <= starDeath &&
                        !SmashAcrossPath(*state, bot->GetPosition(), to, straight, go, walkMs, 0, now);
        if (waitIn + ULDUAR_ALGALON_LAST_IN_MARGIN_MS <= remaining && !(outUnderStar && inFirst))
            return late;
    }

    if (tank && remaining < walkMs + ULDUAR_ALGALON_LAST_IN_MARGIN_MS)
        return std::numeric_limits<int32>::max();

    return routeSlack;
}

bool AlgalonShouldRunForShelter(Player* bot) { return AlgalonHideSlackMs(bot) <= 0; }

void AlgalonBoostHideRun(Player* bot, float walk)
{
    AlgalonEncounterState* state = FindState(bot);
    if (!state || walk <= ULDUAR_ALGALON_NITRO_MIN_WALK || bot->HasAura(NITRO_BOOSTS_SPEED_SPELL))
        return;

    if (state->nitroRunners.count(bot->GetGUID()))
    {
        if (NitroBoostsReady(bot))
        {
            if (UseNitroBoosts(bot) && RaidObs::Active())
                RaidObs::Note(bot, "algalon.nitro", "hide");

            return;
        }

        state->nitroRunners.erase(bot->GetGUID());
    }

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI ||
        AlgalonBigBangRemainingMs(botAI) >= HideWalkMs(*state, bot, walk) + ULDUAR_ALGALON_LAST_IN_MARGIN_MS)
        return;

    char const* const speed = bot->getClass() == CLASS_HUNTER  ? "aspect of the cheetah"
                              : bot->getClass() == CLASS_ROGUE ? "sprint"
                                                               : nullptr;
    if (!speed || botAI->HasAura(speed, bot) || !botAI->CanCastSpell(speed, bot) || !botAI->CastSpell(speed, bot))
        return;

    if (RaidObs::Active())
        RaidObs::Note(bot, "algalon.speed", speed);
}

AlgalonHideRole GetAlgalonHideRole(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    ObjectGuid const guid = bot ? bot->GetGUID() : ObjectGuid::Empty;

    AlgalonHideRole role = AlgalonHideRole::None;
    Position shelter;
    if (!state || !state->engaged)
        role = AlgalonHideRole::None;
    else if (IsAlgalonPhased(bot))
        role = AlgalonHoleNear(bot, ULDUAR_ALGALON_HOLE_EXIT_RADIUS) ? AlgalonHideRole::In : AlgalonHideRole::Exit;
    else if (!state->bigBangCasting)
        role = AlgalonHideRole::None;
    else if (state->bigBangSoaker.Get() == guid)
        role = AlgalonHideRole::Soak;
    else if (state->bigBangBackup.Get() == guid)
        role = AlgalonHideRole::Backup;
    else
    {
        int32 const slack = AlgalonHideSlackMs(bot);
        if (slack == std::numeric_limits<int32>::max())
            role = AlgalonHideRole::NoShelter;
        else if (slack > 0)
            role = AlgalonHoldsConstellation(bot) ? AlgalonHideRole::Hold : AlgalonHideRole::Wait;
        else
            role = GetAlgalonShelter(bot, shelter) ? AlgalonHideRole::Run : AlgalonHideRole::NoShelter;
    }

    if (RaidObs::Active() && state && !state->boss.IsEmpty())
    {
        static char const* const names[] = {"none", "soak", "backup", "run", "in", "exit", "noshelter", "wait", "hold"};
        RaidObs::NoteDerived(bot, "algalon.hide", names[static_cast<uint8>(role)]);
    }

    return role;
}

bool GetAlgalonShelter(Player* bot, Position& shelter)
{
    AlgalonEncounterState* state = FindState(bot);
    if (!state || state->holes.empty())
        return false;

    AlgalonScanUnit const* hole = ShelterHole(bot, *state);
    if (!hole)
        return false;

    auto const latched = state->shelterAssignments.find(bot->GetGUID());
    if (latched == state->shelterAssignments.end() || latched->second != hole->guid)
        state->shelterAssignments[bot->GetGUID()] = hole->guid;

    shelter = hole->position;
    return true;
}

bool AlgalonHideRoute(Player* bot, Position const& shelter, Position& next)
{
    next = shelter;
    AlgalonEncounterState const* state = FindState(bot);
    if (!state)
        return true;

    bool clear = true;
    HideRoute(*state, bot, shelter, 0, next, clear);
    if (next != shelter)
        next = ValidateFloorPoint(bot, next);

    return clear;
}

bool AlgalonHoleNear(Player* bot, float radius, Position* hole)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    for (AlgalonScanUnit const& candidate : state->holes)
    {
        if (bot->GetExactDist2d(candidate.position) > radius)
            continue;

        if (hole)
            *hole = candidate.position;

        return true;
    }

    return false;
}

Player* GetAlgalonBossTank(PlayerbotAI* botAI)
{
    Unit* boss = GetAlgalon(botAI);
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    return victim ? victim->ToPlayer() : nullptr;
}

bool IsAlgalonSwapTank(Player* player)
{
    AlgalonEncounterState const* state = FindState(player);
    return state && state->swapPair.count(player->GetGUID());
}

uint8 GetAlgalonPhasePunchStacks(Player* player)
{
    Aura* aura = player ? player->GetAura(SPELL_ALGALON_PHASE_PUNCH) : nullptr;
    return aura ? aura->GetStackAmount() : 0;
}

Player* GetAlgalonPickupTank(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!state || !group)
        return nullptr;

    auto rank = [](Player* tank)
    {
        uint8 const stacks = GetAlgalonPhasePunchStacks(tank);
        return std::make_tuple(stacks >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS ? 1 : 0, IsBot(tank) ? 0 : 1, stacks,
                               tank->GetGUID());
    };

    Player* best = nullptr;
    for (ObjectGuid const& guid : state->swapPair)
    {
        Player* tank = FindGroupMember(group, guid);
        if (CanStayOut(tank) && (!best || rank(tank) < rank(best)))
            best = tank;
    }

    return best;
}

bool AlgalonShouldPickUp(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI || IsAlgalonPhased(bot) || !IsAlgalonSwapTank(bot) || !AlgalonEngaged(botAI))
        return false;

    Unit* boss = GetAlgalon(botAI);
    if (!boss || boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;

    if (AlgalonBigBangLatched(bot))
        return GetAlgalonBigBangSoaker(botAI) == bot && holder != bot;

    if (holder && IsAlgalonSwapTank(holder) && !IsAlgalonPhased(holder))
        return false;

    return GetAlgalonPickupTank(botAI) == bot;
}

bool AlgalonShouldSwap(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    if (!botAI || !state || IsAlgalonPhased(bot) || !IsAlgalonSwapTank(bot) || !AlgalonEngaged(botAI))
        return false;

    Player* holder = GetAlgalonBossTank(botAI);
    if (!holder || holder == bot || !IsAlgalonSwapTank(holder))
        return false;

    if (AlgalonBigBangCasting(botAI))
        return false;

    return IsAlgalonPhased(holder) || SwapDue(*state, holder, bot, getMSTime());
}

bool AlgalonTakesBossNow(Player* bot) { return AlgalonShouldPickUp(bot) || AlgalonShouldSwap(bot); }

bool AlgalonYieldsBoss(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || (!state->swapPair.count(bot->GetGUID()) && state->handler.Get() != bot->GetGUID()))
        return false;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return false;

    Player* holder = GetAlgalonBossTank(botAI);
    if (!holder || holder == bot || !IsAlgalonSwapTank(holder) || IsAlgalonPhased(holder))
        return false;

    return !AlgalonShouldPickUp(bot) && !AlgalonShouldSwap(bot);
}

Player* AlgalonLoneHolder(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    Player* holder = GetAlgalonBossTank(botAI);
    if (!state || !holder || !IsAlgalonSwapTank(holder) || !CanStayOut(holder))
        return nullptr;

    for (ObjectGuid const& guid : state->swapPair)
    {
        Player* partner = FindGroupMember(bot->GetGroup(), guid);
        if (partner && partner != holder && CanStayOut(partner))
            return nullptr;
    }

    return holder;
}

bool AlgalonOffTankWorking(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI || !IsAlgalonSwapTank(bot))
        return false;

    Player* holder = GetAlgalonBossTank(botAI);
    if (!holder || holder == bot || !IsAlgalonSwapTank(holder))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && target != GetAlgalon(botAI);
}

Player* GetAlgalonHandler(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    return state ? FindGroupMember(bot->GetGroup(), state->handler) : nullptr;
}

bool IsAlgalonHandler(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    return state && bot->GetGroup() && state->handler.Get() == bot->GetGUID();
}

namespace
{
bool ConstellationLow(Unit const* unit)
{
    return unit->GetPositionZ() - ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ() <= ULDUAR_ALGALON_CONSTELLATION_LOW_Z;
}

bool HeldKitePossible(AlgalonEncounterState const& state, Player* bot, uint32 now);
AlgalonTauntLine TauntLine(AlgalonEncounterState const& state, Player* bot, Unit const* constellation, uint32 now,
                           Position& spot);
bool KeepSpot(AlgalonEncounterState const& state, Player* bot, uint32 now, Position& spot);
}

Unit* GetAlgalonHandlerConstellation(Player* bot)
{
    AlgalonEncounterState* state = FindState(bot);
    if (!state)
        return nullptr;

    if (!state->engaged || state->handler.Get() != bot->GetGUID())
    {
        state->kiteHoleAssignments.erase(bot->GetGUID());
        return nullptr;
    }

    uint32 const now = getMSTime();
    bool const gathering =
        state->bigBangCasting || BigBangDueWithin(*state, ULDUAR_ALGALON_GATHER_LEAD_SECONDS, now);
    Creature* loose = nullptr;
    Creature* held = nullptr;
    Creature* heldLow = nullptr;
    float looseDistance = std::numeric_limits<float>::max();
    for (AlgalonScanUnit const& constellation : state->constellations)
    {
        Creature* unit = constellation.active ? Resolve(bot, constellation.guid) : nullptr;
        if (!unit)
            continue;

        if (unit->GetVictim() == bot)
        {
            held = held ? held : unit;
            heldLow = heldLow || !ConstellationLow(unit) ? heldLow : unit;
            continue;
        }

        if (!gathering && !ConstellationLow(unit))
            continue;

        float const distance = bot->GetExactDist2d(unit->GetPositionX(), unit->GetPositionY());
        if (distance < looseDistance)
        {
            loose = unit;
            looseDistance = distance;
        }
    }

    bool const spare = state->holes.size() > ReservedHoles(*state, now);
    bool const kiteFirst = loose && heldLow && !gathering && spare && HeldKitePossible(*state, bot, now);
    char const* branch = "none";
    Unit* chosen = nullptr;
    bool keeping = false;
    Position spot;
    if (loose && !kiteFirst)
    {
        chosen = loose;
        if (AlgalonKitingBehind(bot, loose))
            branch = "behind";
        else
            switch (TauntLine(*state, bot, loose, now, spot))
            {
                case AlgalonTauntLine::Side:
                    branch = "side";
                    break;
                case AlgalonTauntLine::Blocked:
                    branch = "blocked";
                    break;
                default:
                    branch = "taunt";
                    break;
            }
    }
    else if (held && (gathering || !heldLow || !spare) && KeepSpot(*state, bot, now, spot))
    {
        chosen = held;
        keeping = true;
    }
    else if (held && gathering)
        branch = "gather";
    else if (heldLow && spare)
        chosen = heldLow;
    else if (held)
        branch = "kept";

    auto const kite = state->kiteHoleAssignments.find(bot->GetGUID());
    bool const keepKite = held && gathering &&
                          !(kite != state->kiteHoleAssignments.end() && IsRaidShelterHole(*state, kite->second, now));
    if ((!chosen || keeping) && !keepKite)
        state->kiteHoleAssignments.erase(bot->GetGUID());

    if (RaidObs::Active() && (!chosen || chosen->GetVictim() != bot))
        RaidObs::NoteDerived(bot, "algalon.kite", branch);

    return chosen;
}

namespace
{
bool OffTankTauntSafe(AlgalonEncounterState const& state, PlayerbotAI* botAI, Player* bot, uint32 now)
{
    if (!BigBangDueWithin(state, ULDUAR_ALGALON_OFFTANK_TAUNT_SECONDS, now) || !ClassTauntReady(botAI) ||
        AlgalonStaysOut(bot) || AlgalonTakesBossNow(bot))
        return false;

    Player* holder = GetAlgalonBossTank(botAI);
    if (!holder || holder == bot || !state.swapPair.count(holder->GetGUID()) || IsAlgalonPhased(holder) ||
        holder->GetHealthPct() < ULDUAR_ALGALON_HOLDER_URGENT_PCT ||
        GetAlgalonPhasePunchStacks(holder) + 1 >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
        return false;

    return !state.disperser.Get().IsEmpty() ||
           !BigBangDueWithin(state, ULDUAR_ALGALON_EARLY_SWAP_SECONDS, now + ULDUAR_ALGALON_TAUNT_COOLDOWN_MS);
}

bool ConstellationPathClear(AlgalonEncounterState const& state, Unit const* constellation, Player* bot)
{
    Position const from = constellation->GetPosition();
    return std::none_of(state.holes.begin(), state.holes.end(),
                        [&](AlgalonScanUnit const& hole)
                        {
                            return SegmentDistance(from, bot->GetPositionX(), bot->GetPositionY(), hole.position) <
                                   ULDUAR_ALGALON_HOLE_PATH_CLEARANCE;
                        });
}
}

Unit* GetAlgalonOffTankConstellation(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!state || !botAI || !state->engaged || state->constellations.empty() ||
        !state->swapPair.count(bot->GetGUID()) || state->handler.Get() == bot->GetGUID() || IsAlgalonPhased(bot) ||
        !OffTankTauntSafe(*state, botAI, bot, getMSTime()))
        return nullptr;

    Player* handler = FindGroupMember(bot->GetGroup(), state->handler.Get());
    ObjectGuid const handlerPick = handler ? handler->GetTarget() : ObjectGuid::Empty;
    Creature* chosen = nullptr;
    float nearest = ULDUAR_ALGALON_TAUNT_REACH;
    for (AlgalonScanUnit const& constellation : state->constellations)
    {
        Creature* unit =
            constellation.active && constellation.guid != handlerPick ? Resolve(bot, constellation.guid) : nullptr;
        Player* victim = unit && unit->GetVictim() ? unit->GetVictim()->ToPlayer() : nullptr;
        if (!victim || !PlayerbotAI::IsHeal(victim))
            continue;

        float const distance = bot->GetExactDist(unit);
        if (distance <= nearest && ConstellationPathClear(*state, unit, bot))
        {
            chosen = unit;
            nearest = distance;
        }
    }

    return chosen;
}

namespace
{
bool KiteSpotNear(AlgalonEncounterState const& state, AlgalonScanUnit const& hole, float angle, float offset,
                  Position& spot)
{
    for (float const swing : {0.0f, ULDUAR_ALGALON_KITE_SWING, -ULDUAR_ALGALON_KITE_SWING})
    {
        float const bearing = angle + swing;
        float x = hole.position.GetPositionX() + std::cos(bearing) * offset;
        float y = hole.position.GetPositionY() + std::sin(bearing) * offset;
        ClampIntoRoom(x, y);

        float const dx = x - hole.position.GetPositionX();
        float const dy = y - hole.position.GetPositionY();
        if (std::hypot(dx, dy) < ULDUAR_ALGALON_KITE_MIN_OFFSET || dx * std::cos(angle) + dy * std::sin(angle) <= 0.0f)
            continue;

        bool const clear =
            std::none_of(state.holes.begin(), state.holes.end(), [&](AlgalonScanUnit const& other)
                         { return other.guid != hole.guid &&
                                  other.position.GetExactDist2d(x, y) < ULDUAR_ALGALON_HOLE_EXIT_RADIUS; });
        if (!clear)
            continue;

        spot = Position(x, y, hole.position.GetPositionZ());
        return true;
    }

    return false;
}

bool KiteableHole(AlgalonEncounterState const& state, AlgalonScanUnit const& hole, uint32 now)
{
    if (state.kiteRefusedHoles.count(hole.guid) || IsRaidShelterHole(state, hole.guid, now))
        return false;

    if (hole.position.GetExactDist2d(ULDUAR_ALGALON_HOLD_SPOT) > ULDUAR_ALGALON_TANK_SHELTER_REACH)
        return true;

    return std::any_of(state.holes.begin(), state.holes.end(), [&hole](AlgalonScanUnit const& other)
        { return other.guid != hole.guid &&
                 other.position.GetExactDist2d(ULDUAR_ALGALON_HOLD_SPOT) <= ULDUAR_ALGALON_TANK_SHELTER_REACH; });
}

bool KiteHoleFits(AlgalonEncounterState const& state, AlgalonScanUnit const& hole, Position const& from, bool loose)
{
    Position spot;
    float const toward = hole.position.GetAngle(&from);
    float const away = Position::NormalizeOrientation(toward + static_cast<float>(M_PI));
    return KiteSpotNear(state, hole, away, ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET, spot) &&
           (loose || KiteSpotNear(state, hole, toward, ULDUAR_ALGALON_KITE_STAGE_OFFSET, spot));
}

void DropKite(AlgalonEncounterState& state, ObjectGuid const& guid)
{
    state.kiteHoleAssignments.erase(guid);
    state.kites.erase(guid);
}

bool HeldKitePossible(AlgalonEncounterState const& state, Player* bot, uint32 now)
{
    float qx = 0.0f;
    float qy = 0.0f;
    uint32 count = 0;
    for (AlgalonScanUnit const& scanned : state.constellations)
    {
        Creature* unit = scanned.active ? Resolve(bot, scanned.guid) : nullptr;
        if (!unit || unit->GetVictim() != bot || !ConstellationLow(unit))
            continue;

        qx += unit->GetPositionX();
        qy += unit->GetPositionY();
        ++count;
    }

    if (!count)
        return false;

    auto const latched = state.kiteHoleAssignments.find(bot->GetGUID());
    if (latched != state.kiteHoleAssignments.end())
        if (AlgalonScanUnit const* hole = FindScanned(state.holes, latched->second))
            if (KiteableHole(state, *hole, now))
                return true;

    Position const from(qx / count, qy / count, bot->GetPositionZ());
    return std::any_of(state.holes.begin(), state.holes.end(), [&](AlgalonScanUnit const& hole)
                       { return KiteableHole(state, hole, now) && KiteHoleFits(state, hole, from, false); });
}

bool BehindAllowed(AlgalonEncounterState const& state, uint32 now)
{
    return !state.bigBangCasting && !BigBangDueWithin(state, ULDUAR_ALGALON_GATHER_LEAD_SECONDS, now) &&
           state.holes.size() > ReservedHoles(state, now);
}

std::vector<Position> RaidShelterPoints(AlgalonEncounterState const& state, uint32 now)
{
    std::vector<Position> points;
    for (AlgalonScanUnit const& hole : state.holes)
        if (IsRaidShelterHole(state, hole.guid, now))
            points.push_back(hole.position);

    if (state.urgent.Get() && state.holes.empty())
        if (AlgalonScanUnit const* star = LowestStar(state))
            points.push_back(star->position);

    return points;
}

bool LineNearPoints(std::vector<Position> const& points, Position const& from, float x, float y, float clearance)
{
    return std::any_of(points.begin(), points.end(), [&](Position const& point)
                       { return SegmentDistance(from, x, y, point) < clearance; });
}

bool HandlerSpotClear(AlgalonEncounterState const& state, std::vector<Position> const& points, Unit const* boss,
                      float x, float y, float keep)
{
    if (!AlgalonSpotInRoom(x, y) || !ClearOfHoles(state, x, y) || !ClearOfMarkers(state, x, y) ||
        (boss && boss->GetExactDist2d(x, y) < ULDUAR_ALGALON_FORMATION_CLEARANCE))
        return false;

    return std::none_of(points.begin(), points.end(),
                        [x, y, keep](Position const& point) { return point.GetExactDist2d(x, y) < keep; });
}

AlgalonTauntLine TauntLine(AlgalonEncounterState const& state, Player* bot, Unit const* constellation, uint32 now,
                           Position& spot)
{
    std::vector<Position> const points = RaidShelterPoints(state, now);
    Position const from = constellation->GetPosition();
    if (!LineNearPoints(points, from, bot->GetPositionX(), bot->GetPositionY(), ULDUAR_ALGALON_HOLE_PATH_CLEARANCE))
        return AlgalonTauntLine::Clear;

    float closest = std::numeric_limits<float>::max();
    for (Position const& point : points)
        closest = std::min(closest, point.GetExactDist2d(from));

    float const lineKeep = std::clamp(closest - ULDUAR_ALGALON_TAUNT_LINE_SLACK,
                                      ULDUAR_ALGALON_HOLE_PATH_CLEARANCE + ULDUAR_ALGALON_TAUNT_LINE_SLACK,
                                      ULDUAR_ALGALON_HOLE_DETOUR_RADIUS);
    Unit* boss = Resolve(bot, state.boss);
    float best = std::numeric_limits<float>::max();
    for (float const reach : {ULDUAR_ALGALON_TAUNT_SPOT_NEAR, ULDUAR_ALGALON_TAUNT_SPOT_FAR})
    {
        for (uint8 step = 0; step < ULDUAR_ALGALON_SWEEP_BEARINGS; ++step)
        {
            float const angle = static_cast<float>(step) * ULDUAR_ALGALON_SWEEP_ANGLE_STEP;
            float const x = from.GetPositionX() + std::cos(angle) * reach;
            float const y = from.GetPositionY() + std::sin(angle) * reach;
            float const distance = bot->GetExactDist2d(x, y);
            if (distance >= best ||
                !HandlerSpotClear(state, points, boss, x, y, ULDUAR_ALGALON_HANDLER_HOLE_CLEARANCE) ||
                LineNearPoints(points, from, x, y, lineKeep))
                continue;

            best = distance;
            spot = Position(x, y, ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
        }
    }

    return best == std::numeric_limits<float>::max() ? AlgalonTauntLine::Blocked : AlgalonTauntLine::Side;
}

bool KeepSpot(AlgalonEncounterState const& state, Player* bot, uint32 now, Position& spot)
{
    std::vector<Position> const points = RaidShelterPoints(state, now);
    Position const from = bot->GetPosition();
    std::vector<Position> crowding;
    for (Position const& point : points)
        if (point.GetExactDist2d(from) < ULDUAR_ALGALON_HANDLER_HOLE_CLEARANCE)
            crowding.push_back(point);

    if (crowding.empty())
        return false;

    Unit* boss = Resolve(bot, state.boss);
    float const keep = ULDUAR_ALGALON_HANDLER_HOLE_CLEARANCE + ULDUAR_ALGALON_HANDLER_KEEP_MARGIN;
    for (float radius = ULDUAR_ALGALON_SWEEP_STEP; radius <= keep * 2.0f; radius += ULDUAR_ALGALON_SWEEP_STEP)
    {
        float widest = -1.0f;
        for (uint8 step = 0; step < ULDUAR_ALGALON_SWEEP_BEARINGS; ++step)
        {
            float const angle = static_cast<float>(step) * ULDUAR_ALGALON_SWEEP_ANGLE_STEP;
            float const x = from.GetPositionX() + std::cos(angle) * radius;
            float const y = from.GetPositionY() + std::sin(angle) * radius;
            float gap = std::numeric_limits<float>::max();
            bool away = true;
            for (Position const& point : crowding)
            {
                gap = std::min(gap, point.GetExactDist2d(x, y));
                away = away && (x - from.GetPositionX()) * (from.GetPositionX() - point.GetPositionX()) +
                                       (y - from.GetPositionY()) * (from.GetPositionY() - point.GetPositionY()) >=
                                   0.0f;
            }

            if (!away || gap <= widest || !HandlerSpotClear(state, points, boss, x, y, keep))
                continue;

            widest = gap;
            spot = Position(x, y, ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
        }

        if (widest >= 0.0f)
            return true;
    }

    return false;
}
}

bool GetAlgalonKiteSpot(Player* bot, Unit* constellation, Position& spot, char const*& stage)
{
    AlgalonEncounterState* state = FindState(bot);
    if (!state || !constellation)
        return false;

    uint32 const now = getMSTime();
    ObjectGuid const guid = bot->GetGUID();

    std::vector<Creature*> held;
    bool allCasting = true;
    bool anyCasting = false;
    float qx = 0.0f;
    float qy = 0.0f;
    for (AlgalonScanUnit const& scanned : state->constellations)
    {
        Creature* unit = scanned.active ? Resolve(bot, scanned.guid) : nullptr;
        if (!unit || unit->GetVictim() != bot || !ConstellationLow(unit))
            continue;

        held.push_back(unit);
        qx += unit->GetPositionX();
        qy += unit->GetPositionY();
        bool const casting = unit->HasUnitState(UNIT_STATE_CASTING);
        allCasting = allCasting && casting;
        anyCasting = anyCasting || casting;
    }

    bool const loose = constellation->GetVictim() != bot;
    Position const from = loose || held.empty()
                              ? constellation->GetPosition()
                              : Position(qx / held.size(), qy / held.size(), constellation->GetPositionZ());

    AlgalonScanUnit const* hole = nullptr;
    auto const latched = state->kiteHoleAssignments.find(guid);
    if (latched != state->kiteHoleAssignments.end())
        hole = FindScanned(state->holes, latched->second);

    auto const previous = state->kites.find(guid);
    bool const otherTarget =
        loose && (previous == state->kites.end() || previous->second.looseTarget != constellation->GetGUID());
    if (hole && (!KiteableHole(*state, *hole, now) || otherTarget))
    {
        hole = nullptr;
        DropKite(*state, guid);
    }

    if (!hole)
    {
        std::vector<AlgalonScanUnit const*> candidates;
        for (AlgalonScanUnit const& candidate : state->holes)
            if (KiteableHole(*state, candidate, now))
                candidates.push_back(&candidate);

        std::sort(candidates.begin(), candidates.end(), [&from](AlgalonScanUnit const* a, AlgalonScanUnit const* b)
                  { return from.GetExactDist2d(a->position) < from.GetExactDist2d(b->position); });

        for (AlgalonScanUnit const* candidate : candidates)
        {
            if (KiteHoleFits(*state, *candidate, from, loose))
            {
                hole = candidate;
                break;
            }
        }

        if (!hole)
        {
            DropKite(*state, guid);
            return false;
        }

        state->kiteHoleAssignments[guid] = hole->guid;
        AlgalonKite fresh;
        fresh.sinceMs = now;
        if (loose)
            fresh.looseTarget = constellation->GetGUID();
        state->kites[guid] = fresh;
    }

    AlgalonKite& kite = state->kites[guid];
    float const toward = hole->position.GetAngle(&from);
    float const away = Position::NormalizeOrientation(toward + static_cast<float>(M_PI));

    auto const enter = [&kite, now](AlgalonKiteStage next)
    {
        kite.stage = next;
        kite.sinceMs = now;
        kite.hasSpot = false;
    };

    if (loose)
    {
        stage = "behind";
        if (!kite.hasSpot || constellation->GetExactDist2d(kite.spot) > ULDUAR_ALGALON_TAUNT_REACH)
            kite.hasSpot = KiteSpotNear(*state, *hole, away, ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET, kite.spot);
    }
    else
    {
        if (!kite.looseTarget.IsEmpty())
        {
            kite.looseTarget = ObjectGuid::Empty;
            kite.hasSpot = false;
        }

        if (kite.stage == AlgalonKiteStage::Stage)
        {
            float const dx = bot->GetPositionX() - hole->position.GetPositionX();
            float const dy = bot->GetPositionY() - hole->position.GetPositionY();
            float const out = std::hypot(dx, dy);
            if (out >= ULDUAR_ALGALON_KITE_MIN_OFFSET && out <= ULDUAR_ALGALON_KITE_STAGE_OFFSET &&
                dx * std::cos(away) + dy * std::sin(away) >= out * 0.5f)
                enter(AlgalonKiteStage::Cross);
        }

        if (kite.stage == AlgalonKiteStage::Parked &&
            getMSTimeDiff(kite.sinceMs, now) >= ULDUAR_ALGALON_KITE_RETRY_MS &&
            std::any_of(held.begin(), held.end(), [bot, hole](Creature* unit)
                        { return bot->GetExactDist2d(unit) <= ULDUAR_ALGALON_KITE_BESIDE &&
                                 hole->position.GetExactDist2d(unit) > ULDUAR_ALGALON_CONSTELLATION_EAT_RADIUS; }))
            enter(AlgalonKiteStage::Stage);

        if ((kite.stage == AlgalonKiteStage::Stage || kite.stage == AlgalonKiteStage::Cross) &&
            getMSTimeDiff(kite.sinceMs, now) >= ULDUAR_ALGALON_KITE_STALL_MS)
        {
            DropKite(*state, guid);
            return false;
        }

        if (kite.stage == AlgalonKiteStage::Wait &&
            (allCasting || (anyCasting && getMSTimeDiff(kite.sinceMs, now) >= ULDUAR_ALGALON_KITE_WAIT_MS)))
            enter(AlgalonKiteStage::Cross);

        if (!kite.hasSpot)
        {
            bool const staging = kite.stage == AlgalonKiteStage::Stage || kite.stage == AlgalonKiteStage::Wait;
            kite.hasSpot = KiteSpotNear(*state, *hole, staging ? toward : away,
                                        staging ? ULDUAR_ALGALON_KITE_STAGE_OFFSET : ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET,
                                        kite.spot);
        }

        if (kite.hasSpot && bot->GetExactDist2d(kite.spot) <= ULDUAR_ALGALON_SLOT_TOLERANCE &&
            (kite.stage == AlgalonKiteStage::Stage || kite.stage == AlgalonKiteStage::Cross))
        {
            Position const reached = kite.spot;
            enter(kite.stage == AlgalonKiteStage::Stage ? AlgalonKiteStage::Wait : AlgalonKiteStage::Parked);
            kite.spot = reached;
            kite.hasSpot = true;
        }

        static char const* const names[] = {"stage", "wait", "cross", "parked"};
        stage = names[static_cast<uint8>(kite.stage)];
    }

    if (!kite.hasSpot)
    {
        DropKite(*state, guid);
        return false;
    }

    spot = ValidateFloorPoint(bot, kite.spot);
    if (!loose && (kite.stage == AlgalonKiteStage::Stage || kite.stage == AlgalonKiteStage::Cross) &&
        spot.GetExactDist2d(kite.spot) > ULDUAR_ALGALON_SLOT_TOLERANCE)
    {
        state->kiteRefusedHoles.insert(hole->guid);
        DropKite(*state, guid);
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "algalon.kite", "stuck");

        return false;
    }

    return true;
}

bool AlgalonKiteBehind(Player* bot, Unit* constellation, Position& spot)
{
    AlgalonEncounterState* state = FindState(bot);
    if (!state || !constellation || constellation->GetVictim() == bot || !BehindAllowed(*state, getMSTime()))
        return false;

    char const* stage = nullptr;
    if (!GetAlgalonKiteSpot(bot, constellation, spot, stage))
        return false;

    if (constellation->GetExactDist2d(spot) > ULDUAR_ALGALON_TAUNT_REACH)
    {
        DropKite(*state, bot->GetGUID());
        return false;
    }

    return true;
}

bool AlgalonKitingBehind(Player* bot, Unit* constellation)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !constellation || constellation->GetVictim() == bot || !BehindAllowed(*state, getMSTime()))
        return false;

    auto const kite = state->kites.find(bot->GetGUID());
    return kite != state->kites.end() && kite->second.hasSpot &&
           kite->second.looseTarget == constellation->GetGUID();
}

bool GetAlgalonKeepSpot(Player* bot, Position& spot)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || state->handler.Get() != bot->GetGUID())
        return false;

    uint32 const now = getMSTime();
    bool const gathering =
        state->bigBangCasting || BigBangDueWithin(*state, ULDUAR_ALGALON_GATHER_LEAD_SECONDS, now);
    bool const spare = state->holes.size() > ReservedHoles(*state, now);
    bool held = false;
    bool heldLow = false;
    for (AlgalonScanUnit const& constellation : state->constellations)
    {
        Creature* unit = constellation.active ? Resolve(bot, constellation.guid) : nullptr;
        if (!unit || unit->GetVictim() != bot)
            continue;

        held = true;
        heldLow = heldLow || ConstellationLow(unit);
    }

    return held && (gathering || !heldLow || !spare) && KeepSpot(*state, bot, now, spot);
}

AlgalonTauntLine GetAlgalonTauntLine(Player* bot, Unit* constellation, Position& spot)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !constellation || constellation->GetVictim() == bot)
        return AlgalonTauntLine::Clear;

    return TauntLine(*state, bot, constellation, getMSTime(), spot);
}

Unit* GetAlgalonLooseDarkMatter(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !state->engaged || state->handler.Get() != bot->GetGUID())
        return nullptr;

    Unit* loosest = nullptr;
    float lowestVictimHealth = std::numeric_limits<float>::max();
    for (AlgalonScanUnit const& scanned : state->darkMatter)
    {
        Creature* darkMatter = Resolve(bot, scanned.guid);
        Unit* victim = darkMatter ? darkMatter->GetVictim() : nullptr;
        if (!darkMatter || victim == bot)
            continue;

        float const victimHealth = victim ? victim->GetHealthPct() : 100.0f;
        if (victimHealth < lowestVictimHealth)
        {
            loosest = darkMatter;
            lowestVictimHealth = victimHealth;
        }
    }

    return loosest;
}

bool IsAlgalonStarTeam(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    return state->starTeam.count(bot->GetGUID()) || (state->urgent.Get() && IsRangedDps(bot));
}

Unit* GetAlgalonFocusStar(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    return state ? Resolve(bot, state->focusStar.Get()) : nullptr;
}

Unit* GetAlgalonStarTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    return state ? Resolve(bot, StarFocusOf(*state, bot->GetGUID())) : nullptr;
}

uint8 AlgalonAliveStarCount(PlayerbotAI* botAI)
{
    AlgalonEncounterState const* state = FindState(botAI ? botAI->GetBot() : nullptr);
    return state ? static_cast<uint8>(state->stars.size()) : 0;
}

bool AlgalonCosmicSmashThreatens(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    return botAI && !IsAlgalonPhased(bot) &&
           AlgalonCosmicSmashMarkerNear(botAI, bot->GetPosition(), ULDUAR_ALGALON_COSMIC_SMASH_TRIGGER_RADIUS);
}

bool AlgalonDyingStarThreatens(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    return state && state->engaged && !IsAlgalonPhased(bot) &&
           DyingStarNear(*state, bot->GetPosition(), ULDUAR_ALGALON_STAR_DYING_RADIUS);
}

bool AlgalonCosmicSmashMarkerNear(PlayerbotAI* botAI, Position const& spot, float radius)
{
    AlgalonEncounterState const* state = FindState(botAI ? botAI->GetBot() : nullptr);
    if (!state)
        return false;

    for (AlgalonScanUnit const& marker : state->markers)
        if (spot.GetExactDist2d(marker.position.GetPositionX(), marker.position.GetPositionY()) <= radius)
            return true;

    return false;
}

bool AlgalonSmashHoldsGround(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    return botAI && state && !IsAlgalonPhased(bot) &&
           (AlgalonCosmicSmashMarkerNear(botAI, bot->GetPosition(), ULDUAR_ALGALON_COSMIC_SMASH_HOLD_RADIUS) ||
            DyingStarNear(*state, bot->GetPosition(), ULDUAR_ALGALON_STAR_DYING_HOLD_RADIUS));
}

std::vector<HazardCircle> GetAlgalonDodgeHazards(Player* bot, float markerClearance)
{
    std::vector<HazardCircle> hazards;
    AlgalonEncounterState const* state = FindState(bot);
    if (!state)
        return hazards;

    for (AlgalonScanUnit const& marker : state->markers)
        hazards.emplace_back(marker.position, markerClearance);

    for (AlgalonScanUnit const& hole : state->holes)
        hazards.emplace_back(hole.position, ULDUAR_ALGALON_HOLE_EXIT_RADIUS);

    bool const phased = IsAlgalonPhased(bot);
    for (AlgalonScanUnit const& star : state->stars)
        if (StarDying(star) || (phased && star.underDying))
            hazards.emplace_back(star.position, ULDUAR_ALGALON_STAR_DYING_RADIUS);

    return hazards;
}

bool AlgalonUnderStarNear(Player* bot, float radius)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    return std::any_of(state->stars.begin(), state->stars.end(), [bot, radius](AlgalonScanUnit const& star)
                       { return star.underDying && bot->GetExactDist2d(star.position) < radius; });
}

bool AlgalonSpotInRoom(float x, float y)
{
    return ULDUAR_ALGALON_ROOM_CENTER.GetExactDist2d(x, y) <= ULDUAR_ALGALON_ROOM_CLAMP;
}

bool AlgalonPathCrossesHole(Player* bot, float x, float y, float clearance)
{
    AlgalonEncounterState const* state = FindState(bot);
    return state && HoleAcrossPath(*state, bot->GetPosition(), x, y, IsAlgalonPhased(bot), clearance);
}

bool AlgalonWalkEntersField(Player* bot, float x, float y, float standClearance)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || state->holes.empty())
        return false;

    for (AlgalonScanUnit const& hole : state->holes)
        if (hole.position.GetExactDist2d(x, y) < standClearance)
            return true;

    return HoleAcrossPath(*state, bot->GetPosition(), x, y, false, ULDUAR_ALGALON_FIELD_PATH_CLEARANCE) != nullptr;
}

bool AlgalonWalkAround(Player* bot, Position const& to, Position& next)
{
    next = to;
    AlgalonEncounterState const* state = FindState(bot);
    if (!state)
        return true;

    AlgalonScanUnit const* hole =
        HoleAcrossPath(*state, bot->GetPosition(), to.GetPositionX(), to.GetPositionY(), IsAlgalonPhased(bot));
    return !hole || FindHoleDetour(bot, *state, *hole, to, next);
}

bool AlgalonPhasedInField(Player* bot)
{
    return IsAlgalonPhased(bot) && AlgalonHoleNear(bot, ULDUAR_ALGALON_HOLE_EXIT_RADIUS);
}

bool AlgalonStarWithin(PlayerbotAI* botAI, Position const& spot, float radius, ObjectGuid const& except)
{
    AlgalonEncounterState const* state = FindState(botAI ? botAI->GetBot() : nullptr);
    if (!state)
        return false;

    for (AlgalonScanUnit const& star : state->stars)
        if (star.guid != except && spot.GetExactDist2d(star.position) <= radius)
            return true;

    return false;
}

bool AlgalonHeadsForFarHole(Player* bot)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !state->engaged || state->holes.empty() ||
        (!state->bigBangCasting && !BigBangDueWithin(*state, ULDUAR_ALGALON_FAR_HOLE_SECONDS, getMSTime())))
        return false;

    Position spot;
    char const* kind = RawSlot(bot, spot);
    return kind && std::strcmp(kind, "nearhole") == 0;
}

bool AlgalonTakesRingSlot(Player* bot)
{
    return bot && PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsMainTank(bot) &&
           !PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
}

bool AlgalonMeleeStaged(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    return botAI && state && !state->meleeReleased && !PlayerbotAI::IsRanged(bot) && !IsAlgalonSwapTank(bot) &&
           AlgalonPresent(botAI);
}

bool TryGetAlgalonSlotPosition(uint8 slotIndex, Position& position)
{
    if (slotIndex >= ULDUAR_ALGALON_TOTAL_SLOTS)
        return false;

    position = ULDUAR_ALGALON_RING_SLOTS[slotIndex];
    return true;
}

bool TryGetAlgalonSlot(Player* bot, Position& position, bool* waypoint)
{
    if (!bot)
        return false;

    bool found = false;
    char const* branch = DeriveSlot(bot, position, found);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "algalon.spot", branch);

    if (waypoint)
        *waypoint = std::strcmp(branch, "blocked") == 0;

    return found;
}

bool GetAlgalonSlotAnchor(Player* bot, Position& slot) { return bot && RawSlot(bot, slot); }

bool GetAlgalonDodgeAnchor(Player* bot, Position& anchor)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (state && state->engaged && !IsAlgalonPhased(bot) && !AlgalonStaysOut(bot) &&
        SmashLandsInCast(*state, getMSTime()))
    {
        AlgalonScanUnit const* hole =
            state->bigBangCasting ? ShelterHole(bot, *state) : NearestShelter(*state, bot->GetPosition());
        if (hole)
        {
            anchor = hole->position;
            return true;
        }
    }

    return GetAlgalonSlotAnchor(bot, anchor);
}

bool AlgalonHiderInCast(Player* bot)
{
    return AlgalonBigBangLatched(bot) && !IsAlgalonPhased(bot) && !AlgalonStaysOut(bot);
}

void AppendAlgalonTargetExclusions(PlayerbotAI* botAI, GuidSet& exclusions)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !state->engaged)
        return;

    if (AlgalonYieldsBoss(bot))
        exclusions.insert(state->boss);

    bool const starTeam = IsAlgalonStarTeam(bot);
    ObjectGuid const focus = StarFocusOf(*state, bot->GetGUID());
    for (AlgalonScanUnit const& unit : state->stars)
        if (!starTeam || unit.guid != focus)
            exclusions.insert(unit.guid);

    bool const handler = state->handler.Get() == bot->GetGUID();
    if (!handler)
        for (AlgalonScanUnit const& unit : state->constellations)
            exclusions.insert(unit.guid);

    if (!handler && !state->handler.Get().IsEmpty())
        for (AlgalonScanUnit const& unit : state->darkMatter)
            exclusions.insert(unit.guid);
}

bool AlgalonMayDamage(Player* bot, Unit* target)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !state->engaged || !target)
        return true;

    if (target->GetEntry() == PB_NPC_ALGALON)
        return !AlgalonYieldsBoss(bot);

    ObjectGuid const guid = target->GetGUID();
    Group* group = bot->GetGroup();
    if (group && group->GetTargetIcon(RtiTargetValue::skullIndex) == guid)
        return true;

    bool const handler = state->handler.Get() == bot->GetGUID();
    switch (target->GetEntry())
    {
        case PB_NPC_COLLAPSING_STAR:
            return guid == StarFocusOf(*state, bot->GetGUID()) && IsAlgalonStarTeam(bot);
        case PB_NPC_LIVING_CONSTELLATION:
        {
            Position side;
            return handler && (target->GetVictim() == bot ||
                               TauntLine(*state, bot, target, getMSTime(), side) == AlgalonTauntLine::Clear);
        }
        case PB_NPC_UNLEASHED_DARK_MATTER:
            return handler || state->handler.Get().IsEmpty();
        default:
            return true;
    }
}
