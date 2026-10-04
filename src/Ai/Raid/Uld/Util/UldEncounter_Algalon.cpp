/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Algalon.h"

#include "AiObjectContext.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "InstanceScript.h"
#include "LastMovementValue.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidTankDefensive.h"
#include "ServerFacade.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellAuras.h"
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
#include <string>
#include <tuple>

using namespace EncounterHelpers;

// navprobe map 603: every point below is on mesh at 0.04, settled Z 417.321, out to 44 yd on the hold's
// bearing. Angles are from the hold's line to his landing spot.
const Position ULDUAR_ALGALON_ROOM_CENTER = Position(1632.668f, -302.7656f, 417.3211f);
const Position ULDUAR_ALGALON_PICKUP_SLOT = Position(1633.781f, -310.688f, 417.321f);
const Position ULDUAR_ALGALON_PULL_OFFTANK_SLOT = Position(1628.668f, -309.694f, 417.321f);
// 38 and 30 yd out at bearing -82, the middle of the gap between the two star spawns on that side.
const Position ULDUAR_ALGALON_TANK_SLOT = Position(1637.957f, -340.396f, 417.321f);
const Position ULDUAR_ALGALON_HOLD_SPOT = Position(1636.843f, -332.474f, 417.321f);
// 4.1-7.2 yd from him wherever he stops between 27 and 32 yd out.
const Position ULDUAR_ALGALON_OFFTANK_SLOT = Position(1633.300f, -336.001f, 417.321f);

const std::array<Position, ULDUAR_ALGALON_TOTAL_SLOTS> ULDUAR_ALGALON_RING_SLOTS = {{
    Position(1617.869f, -333.468f, 417.321f),  // healer, 19 yd from the hold at +85 deg
    Position(1655.356f, -328.200f, 417.321f),  // healer, 19 yd from the hold at -85 deg
    Position(1619.354f, -339.898f, 417.321f),  // healer, 19 yd from the hold at +105 deg
    Position(1655.702f, -334.789f, 417.321f),  // healer, 19 yd from the hold at -105 deg
    Position(1612.390f, -327.276f, 417.321f),  // ranged, 25 yd from the hold at +70 deg
    Position(1658.917f, -320.737f, 417.321f),  // ranged, 25 yd from the hold at -70 deg
    Position(1611.877f, -333.782f, 417.321f),  // ranged, 25 yd from the hold at +85 deg
    Position(1661.202f, -326.850f, 417.321f),  // ranged, 25 yd from the hold at -85 deg
    Position(1613.067f, -340.199f, 417.321f),  // ranged, 25 yd from the hold at +100 deg
    Position(1661.828f, -333.346f, 417.321f),  // ranged, 25 yd from the hold at -100 deg
    Position(1613.806f, -311.731f, 417.321f),  // ranged, 31 yd from the hold at +40 deg
    Position(1653.271f, -306.184f, 417.321f),  // ranged, 31 yd from the hold at -40 deg
    Position(1609.222f, -318.400f, 417.321f),  // ranged, 31 yd from the hold at +55 deg
    Position(1659.515f, -311.332f, 417.321f),  // ranged, 31 yd from the hold at -55 deg
    Position(1606.521f, -326.028f, 417.321f),  // ranged, 31 yd from the hold at +70 deg
    Position(1664.215f, -317.920f, 417.321f),  // ranged, 31 yd from the hold at -70 deg
    Position(1605.886f, -334.096f, 417.321f),  // ranged, 31 yd from the hold at +85 deg
    Position(1667.049f, -325.500f, 417.321f),  // ranged, 31 yd from the hold at -85 deg
}};

const std::array<Position, ULDUAR_ALGALON_MELEE_STAGE_SPOTS> ULDUAR_ALGALON_MELEE_STAGE_SLOTS = {{
    Position(1621.852f, -331.950f, 417.321f),  // 15 yd from the hold at +80 deg
    Position(1651.109f, -327.838f, 417.321f),  // 15 yd from the hold at -80 deg
    Position(1621.989f, -334.561f, 417.321f),  // 15 yd from the hold at +90 deg
    Position(1651.697f, -330.386f, 417.321f),  // 15 yd from the hold at -90 deg
    Position(1622.577f, -337.109f, 417.321f),  // 15 yd from the hold at +100 deg
    Position(1651.834f, -332.997f, 417.321f),  // 15 yd from the hold at -100 deg
    Position(1623.599f, -339.516f, 417.321f),  // 15 yd from the hold at +110 deg
    Position(1651.515f, -335.592f, 417.321f),  // 15 yd from the hold at -110 deg
    Position(1623.447f, -342.940f, 417.321f),  // 17 yd from the hold at +120 deg
    Position(1652.605f, -338.842f, 417.321f),  // 17 yd from the hold at -120 deg
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

    // The guid lookup has no liveness filter, and a dead boss would hold every gate open through loot.
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

// Alive and in the room's phase, which is all Big Bang and his evade check count.
bool CanStayOut(Player* member) { return InFight(member) && !IsAlgalonPhased(member); }

bool IsDamageDealer(Player* member) { return !PlayerbotAI::IsHeal(member) && !PlayerbotAI::IsTank(member); }

bool IsRangedDps(Player* member) { return PlayerbotAI::IsRanged(member) && IsDamageDealer(member); }

bool IsBot(Player* member) { return member && GET_PLAYERBOT_AI(member); }

// Nearest living, unphased raid member: who a new hole or an explosion lands on.
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

    // Signed, so a cast running late reads as due rather than a full cycle away.
    int32 const left = static_cast<int32>(state.nextBigBangMs - now);
    return left <= static_cast<int32>(seconds * IN_MILLISECONDS);
}

// Kept while a Big Bang is close: the raid needs one standing when it lands.
uint8 ReservedHoles(AlgalonEncounterState const& state, uint32 now)
{
    return state.bigBangCasting || BigBangDueWithin(state, ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS, now) ? 1 : 0;
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

// The sweep keeps only the 40 units nearest its anchor, and on 25-man the room holds more than that, so
// a meteor far from the anchor never reaches the trace unless it is written here.
void NoteMeteor(Creature* boss, Creature* marker)
{
    if (!RaidObs::Active() || marker->GetEntry() != NPC_ALGALON_ASTEROID_TARGET_2)
        return;

    uint32 const spell =
        boss->GetMap()->Is25ManRaid() ? SPELL_ALGALON_COSMIC_SMASH_DAMAGE_25 : SPELL_ALGALON_COSMIC_SMASH_DAMAGE;
    // The damage caster hangs 35 yd up; the impact is on the floor below it.
    Position const impact(marker->GetPositionX(), marker->GetPositionY(), ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
    RaidObs::NoteHazardCircle(boss->GetMap(), spell, impact, 6.0f, ULDUAR_ALGALON_MARKER_LIFETIME_MS);
}

// Appends to the star's health log and sets its loss rate from the slope. A short log reads as the drain, or
// as the dropped rate for a star the team left part-killed.
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
        star.lossPctPerSec = std::max(ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC,
                                      (log.front().second - star.healthPct) * IN_MILLISECONDS / static_cast<float>(span));
    else if (state.droppedStars.count(star.guid))
        star.lossPctPerSec = ULDUAR_ALGALON_STAR_DROPPED_PCT_PER_SEC;

    logs[star.guid] = std::move(log);
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

    // The searcher takes its phase mask from its first argument. His is the room's, while a bot's
    // turns to 16 inside a hole and would see nothing here.
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
                state.constellations.push_back(unit);
                break;
            case PB_NPC_UNLEASHED_DARK_MATTER:
                state.darkMatter.push_back(unit);
                break;
            default:
            {
                auto const seen = state.markerSeenMs.find(unit.guid);
                if (seen == state.markerSeenMs.end())
                    NoteMeteor(boss, creature);

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
    for (auto dropped = state.droppedStars.begin(); dropped != state.droppedStars.end();)
        dropped = state.starSeenMs.count(*dropped) ? std::next(dropped) : state.droppedStars.erase(dropped);

    bool const phaseTwoStarted = state.wormHolesSeen && !hadWormHoles;
    // By guid, not by count: a death in the same scan as the minute's refill leaves the count unchanged.
    auto const gone = [&state](AlgalonScanUnit const& star) { return !FindScanned(state.stars, star.guid); };
    bool const starDied = std::any_of(previousStars.begin(), previousStars.end(), gone);
    if (starDied && !phaseTwoStarted)
        state.lastStarDeathMs = now;

    // A kill or an Ascend despawns every summon in one go, which is no hole anyone lost.
    bool const despawnedAll = !previousHoles.empty() && state.holes.empty() && state.stars.empty() &&
                              state.constellations.empty() && (!previousStars.empty() || previousConstellations);
    if (!RaidObs::Active() || despawnedAll)
        return;

    // Nothing in the trace says who killed a star, and the damage stream only records hits on players. A
    // star nobody was sent to dies chewed by splash or drained out, and its health against its age says
    // which.
    if (!phaseTwoStarted)
    {
        for (AlgalonScanUnit const& star : previousStars)
        {
            if (FindScanned(state.stars, star.guid))
                continue;

            float const drainedTo = 100.0f - static_cast<float>(getMSTimeDiff(star.firstSeenMs, now)) / IN_MILLISECONDS;
            char const* cause = "chewed";
            if (star.guid == state.focusStar.Get())
                cause = "killed";
            else if (star.healthPct >= drainedTo - ULDUAR_ALGALON_STAR_DRAIN_SLACK_PCT)
                cause = "drained";

            RaidObs::Note(bot, "algalon.stardeath",
                          std::string(cause) + " " + DescribeYards(NearestRaidMember(bot->GetGroup(), star.position)));
        }
    }

    // Holes are the resource the whole fight turns on, and nothing else in the trace says why one went.
    for (AlgalonScanUnit const& hole : previousHoles)
    {
        if (FindScanned(state.holes, hole.guid))
            continue;

        char const* reason = phaseTwoStarted ? "phase2" : "stray";
        for (auto const& kite : state.kiteHoleAssignments)
            if (kite.second == hole.guid)
                reason = "kite";

        RaidObs::Note(bot, "algalon.holelost", reason);
    }
}

// Bots only: a human can't be handed a job.
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

// Two bots trade him on their own; with one, the human tank has to be told when to taunt.
std::vector<Player*> WantedSwapPair(Player* bot)
{
    std::vector<Player*> const tanks = OrderedTanks(bot);
    std::vector<Player*> bots;
    std::copy_if(tanks.begin(), tanks.end(), std::back_inserter(bots), IsBot);
    std::vector<Player*> pair = bots.size() >= 2 ? bots : tanks;
    if (pair.size() > 2)
        pair.resize(2);

    return pair;
}

// Re-derived until the pull, so a role change or a tank joining late still counts. During the pull a
// member who died or left is replaced, or the survivor eats the 5th Phase Punch with nobody to take him.
void EnsureSwapPair(Player* bot, Creature* boss, AlgalonEncounterState& state)
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

    if (!fighting)
    {
        std::vector<Player*> const wanted = WantedSwapPair(bot);
        for (ObjectGuid const& guid : state.swapPair)
        {
            auto const same = [&guid](Player* tank) { return tank->GetGUID() == guid; };
            if (std::none_of(wanted.begin(), wanted.end(), same))
                leaving.push_back(guid);
        }
    }

    for (ObjectGuid const& guid : leaving)
        state.swapPair.erase(guid);

    // OrderedTanks returns living tanks only, so a replacement is someone who can take him. Bots first:
    // a human has to be called.
    std::vector<Player*> candidates = WantedSwapPair(bot);
    if (fighting)
    {
        std::vector<Player*> const tanks = OrderedTanks(bot);
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

    // Latched before the pull opened a trace; a no-op insert writes the set into the new one.
    if (RaidObs::Active() && !state.swapPair.empty())
    {
        ObjectGuid const first = *state.swapPair.begin();
        state.swapPair.insert(first);
    }
}

Player* PairPartner(Group* group, AlgalonEncounterState const& state, Player* tank)
{
    for (ObjectGuid const& guid : state.swapPair)
        if (guid != tank->GetGUID())
            return FindGroupMember(group, guid);

    return nullptr;
}

// The holder has nothing physical left for the coming hit and the partner has. Divine Protection is a
// 2-3 minute cooldown against a 90.5 s cast, so whoever spent it last can't soak this one. Bots only: a
// human's buttons are theirs to spend.
bool DefensiveHandoverDue(AlgalonEncounterState const& state, Player* holder, Player* taker, uint32 now)
{
    if (!IsBot(holder) || !IsBot(taker) || GetAlgalonPhasePunchStacks(taker) > ULDUAR_ALGALON_HANDOVER_PARTNER_STACKS ||
        !BigBangDueWithin(state, ULDUAR_ALGALON_EARLY_SWAP_SECONDS, now))
        return false;

    // The soak defensive goes up with ULDUAR_ALGALON_SOAK_DEFENSIVE_MS of the cast left.
    int32 const untilNeeded = static_cast<int32>(state.nextBigBangMs - now) +
                              static_cast<int32>(ULDUAR_ALGALON_BIG_BANG_CAST_MS) - ULDUAR_ALGALON_SOAK_DEFENSIVE_MS;
    uint32 const within = static_cast<uint32>(std::max(0, untilNeeded));
    return !ReadyTankDefensive(holder, true, within) && ReadyTankDefensive(taker, true, within);
}

// Whether `taker` should take him off `holder`: the 4th stack, or a holder deep in stacks or out of
// defensives with a Big Bang close, since the holder at the cast soaks it and the Phase Punch queued
// behind the cast lands while the rest of the raid is phased.
bool SwapDue(AlgalonEncounterState const& state, Player* holder, Player* taker, uint32 now)
{
    uint8 const own = GetAlgalonPhasePunchStacks(taker);
    uint8 const theirs = GetAlgalonPhasePunchStacks(holder);
    if (own >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
        return false;

    if (theirs >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS || DefensiveHandoverDue(state, holder, taker, now))
        return true;

    return theirs >= ULDUAR_ALGALON_EARLY_SWAP_STACKS && own <= ULDUAR_ALGALON_EARLY_SWAP_PARTNER_STACKS &&
           BigBangDueWithin(state, ULDUAR_ALGALON_EARLY_SWAP_SECONDS, now);
}

// Raid warning to the humans only: bots read the encounter state, not chat.
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

// The swap and the pickup fire on the tank that takes him, and a human has no trigger to fire them.
void CallHumanTank(Group* group, PlayerbotAI* botAI, Creature* boss, AlgalonEncounterState& state, uint32 now)
{
    if (state.bigBangCasting ||
        (state.lastTankCallMs && getMSTimeDiff(state.lastTankCallMs, now) < ULDUAR_ALGALON_TANK_CALL_MS))
    {
        return;
    }

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

// Two calls, because a human sitting in the void for the whole 8 s cast is fed to the phase-16 Dark
// Matter, and one stepping out early lets the 10 s phase lapse before the hit.
void CallBigBang(Group* group, Creature* boss, AlgalonEncounterState& state, bool castStarted)
{
    Player* tracker = FindTracker(group);
    if (!tracker)
        return;

    Player* soaker = FindGroupMember(group, state.bigBangSoaker.Get());
    if (castStarted)
    {
        state.bigBangHideCalled = false;
        std::string stays =
            soaker ? soaker->GetName() + " stays out with a defensive. Everyone else" : std::string("Everyone");
        // A human holder handed the soak is told outright, or it stays out and takes him back.
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

// The moment he swings at a random player: anyone but a swap tank inside his reach is one hit away.
void NotePull(Group* group, Creature* boss, AlgalonEncounterState const& state)
{
    Player* tracker = RaidObs::Active() ? FindTracker(group) : nullptr;
    if (!tracker)
        return;

    uint32 inReach = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (CanStayOut(member) && !state.swapPair.count(member->GetGUID()) &&
            boss->GetExactDist2d(member) <= ULDUAR_ALGALON_MELEE_REACH)
        {
            ++inReach;
        }
    }

    Unit* victim = boss->GetVictim();
    RaidObs::Note(tracker, "algalon.pull",
                  "inreach=" + std::to_string(inReach) +
                      " victim=" + RaidObs::DescribeAssignment(victim ? victim->GetGUID() : ObjectGuid::Empty));
}

// Once per Big Bang cycle, on the holder handing him over for want of a defensive.
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

// What the soaker had for the hit when the cast began, so a death after it reads as kit or as healing.
void NoteSoakKit(Group* group, AlgalonEncounterState const& state)
{
    Player* soaker = RaidObs::Active() ? FindGroupMember(group, state.bigBangSoaker.Get()) : nullptr;
    if (!soaker)
        return;

    char const* ready =
        ReadyTankDefensive(soaker, true, ULDUAR_ALGALON_BIG_BANG_CAST_MS - ULDUAR_ALGALON_SOAK_DEFENSIVE_MS);
    RaidObs::Note(soaker, "algalon.soakkit",
                  std::string("def=") + (ready ? ready : "none") +
                      " forbearance=" + (soaker->HasAura(SPELL_ALGALON_FORBEARANCE) ? "1" : "0"));
}

// Not onto a loose boss, whose next swing lands on whoever is nearest, unless nobody ever picks him up.
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

// A physical button for a hit this far off: back off cooldown in time, or already up and lasting past it.
bool SoakCovered(Player* tank, uint32 hitMs)
{
    uint32 const castBy = hitMs > static_cast<uint32>(ULDUAR_ALGALON_SOAK_DEFENSIVE_MS)
                              ? hitMs - static_cast<uint32>(ULDUAR_ALGALON_SOAK_DEFENSIVE_MS)
                              : 0;
    return ReadyTankDefensive(tank, true, castBy) || RunningTankDefensive(tank, true, hitMs);
}

// A tank handed the soak mid run keeps going on its forced booking while its walk back waits behind it,
// and he may turn to it by threat, so no taunt action ever runs to stop it.
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

// The bot swap tank other than `tank`, able to stay out and take the Phase Punch behind the hit.
Player* SoakingPartner(Group* group, AlgalonEncounterState const& state, Player* tank, uint8 maxStacks)
{
    for (ObjectGuid const& guid : state.swapPair)
    {
        Player* partner = FindGroupMember(group, guid);
        if (partner && partner != tank && IsBot(partner) && CanStayOut(partner) &&
            GetAlgalonPhasePunchStacks(partner) <= maxStacks)
        {
            return partner;
        }
    }

    return nullptr;
}

void ElectBigBangSoakers(Player* bot, Creature* boss, AlgalonEncounterState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;

    Player* soaker = nullptr;
    if (CanStayOut(holder) && IsAlgalonSwapTank(holder))
    {
        soaker = holder;
        // A human with nothing for the hit hides or dies; a bot partner with a button takes it instead.
        if (!IsBot(holder) && !SoakCovered(holder, ULDUAR_ALGALON_BIG_BANG_CAST_MS))
        {
            Player* partner = SoakingPartner(group, state, holder, ULDUAR_ALGALON_HANDOVER_PARTNER_STACKS);
            if (partner && SoakCovered(partner, ULDUAR_ALGALON_BIG_BANG_CAST_MS))
                soaker = partner;
        }
    }

    if (!soaker && botAI)
        soaker = GetAlgalonPickupTank(botAI);

    // Nobody who should have him does, but whoever he is on still keeps him from evading.
    if (!soaker && CanStayOut(holder))
        soaker = holder;

    Player* backup = ElectBackup(group, soaker);
    state.bigBangSoaker = soaker ? soaker->GetGUID() : ObjectGuid::Empty;
    state.bigBangBackup = backup ? backup->GetGUID() : ObjectGuid::Empty;
}

// The soaker is the one thing standing between the raid and a reset. If it died or got phased mid
// cast, the bot swap tank takes the duty, else the backup, and someone else backs that up. The tank
// first: he stays on a tank once the raid is back, and a priest out of Dispersion dies to his next swing.
void KeepBigBangSoaker(Player* bot, AlgalonEncounterState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    Player* soaker = FindGroupMember(group, state.bigBangSoaker);
    Player* backup = FindGroupMember(group, state.bigBangBackup);
    if (!CanStayOut(soaker))
    {
        // One stack under the swap: the Phase Punch behind the hit takes it to the swap, not past it.
        if (Player* partner =
                SoakingPartner(group, state, soaker, ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS - 1))
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

    // Also retried every tick, so a priest whose Dispersion comes off cooldown mid cast still joins.
    if (!CanStayOut(backup))
    {
        backup = ElectBackup(group, soaker);
        state.bigBangBackup = backup ? backup->GetGUID() : ObjectGuid::Empty;
    }
}

// A bot tank outside the swap pair, else the pair bot not keeping him.
Player* ElectHandler(Player* bot, AlgalonEncounterState const& state)
{
    for (Player* tank : OrderedTanks(bot))
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

void ElectStarTeam(Player* bot, AlgalonEncounterState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    std::vector<ObjectGuid> leaving;
    for (ObjectGuid const& guid : state.starTeam)
    {
        Player* member = FindGroupMember(group, guid);
        if (!InFight(member) || !IsDamageDealer(member))
            leaving.push_back(guid);
    }

    for (ObjectGuid const& guid : leaving)
        state.starTeam.erase(guid);

    uint8 const size =
        bot->GetMap()->Is25ManRaid() ? ULDUAR_ALGALON_STAR_TEAM_25 : ULDUAR_ALGALON_STAR_TEAM_10;
    if (state.starTeam.size() >= size)
        return;

    std::vector<Player*> candidates;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (InFight(member) && IsDamageDealer(member) && GET_PLAYERBOT_AI(member) &&
            !state.starTeam.count(member->GetGUID()))
        {
            candidates.push_back(member);
        }
    }

    // Ranged first, since a star wanders; melee only fill a raid short on ranged, or no star ever dies.
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

        state.starTeam.insert(candidate->GetGUID());
    }
}

// The hole lands where the star dies. Under a swap tank or the handler it phases them and sets him or
// their adds loose; inside his melee ring (`bossRing`) it phases the melee.
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
    // Also clear of every raid member.
    Clear,
};

bool StarPassesFilter(AlgalonEncounterState const& state, Group* group, Unit* boss, AlgalonScanUnit const& star,
                      StarFilter filter)
{
    if (filter == StarFilter::Clear && NearestRaidMember(group, star.position) < ULDUAR_ALGALON_STAR_CLEAR_RADIUS)
        return false;

    return filter == StarFilter::Any || StarClearOfTanks(state, group, boss, star, filter != StarFilter::Tanks);
}

// For `urgent`, which wants a hole as fast as possible, wherever it lands.
AlgalonScanUnit const* LowestStar(AlgalonEncounterState const& state)
{
    AlgalonScanUnit const* lowest = nullptr;
    for (AlgalonScanUnit const& star : state.stars)
        if (!lowest || star.healthPct < lowest->healthPct)
            lowest = &star;

    return lowest;
}

// How long the team needs to walk into range of a star from where it stands.
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

// The planner works in ms from now.
struct StarSpan
{
    int32 from;
    int32 to;
};

int32 MsUntil(uint32 when, uint32 now) { return static_cast<int32>(when - now); }

int32 StarDeathAt(AlgalonScanUnit const& star, float lossPctPerSec)
{
    return static_cast<int32>(star.healthPct / lossPctPerSec * IN_MILLISECONDS);
}

struct StarBlackout
{
    StarSpan span;
    char const* label;
};

struct StarPlan
{
    // An explosion in one of these lands on a raid that can't take it.
    std::vector<StarBlackout> blackouts;
    bool hadDeath = false;
    int32 lastDeath = 0;
    // Each star's death at its measured loss, the team's damage included for the one it is on.
    std::vector<std::pair<AlgalonScanUnit const*, int32>> deaths;
};

StarPlan PlanStars(AlgalonEncounterState const& state, uint32 now)
{
    StarPlan plan;

    // nextBigBangMs moves on to the next cycle the moment a cast starts, so the last one's tail comes from
    // its end.
    if (state.lastBigBangEndMs)
    {
        int32 const tail = MsUntil(state.lastBigBangEndMs, now) + static_cast<int32>(ULDUAR_ALGALON_STAR_BIG_BANG_TAIL_MS);
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

    // Not `markers`: they drop out 5.5 s after the sighting, before the impact window ends.
    for (auto const& [marker, seenMs] : state.markerSeenMs)
    {
        int32 const impact = MsUntil(seenMs + ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_MS, now);
        if (impact + static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_TAIL_MS) < 0)
            continue;

        plan.blackouts.push_back({{impact - static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_LEAD_MS),
                                   impact + static_cast<int32>(ULDUAR_ALGALON_STAR_IMPACT_TAIL_MS)},
                                  "smash"});
    }

    plan.hadDeath = state.lastStarDeathMs != 0;
    plan.lastDeath = plan.hadDeath ? MsUntil(state.lastStarDeathMs, now) : 0;
    for (AlgalonScanUnit const& star : state.stars)
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

// Why an explosion somewhere in `when` would land badly, or nullptr. A span, because a kill's finish is only
// known to within the team's speed.
char const* StarCollision(StarPlan const& plan, StarSpan when, AlgalonScanUnit const* self, int32 separation)
{
    for (StarBlackout const& blackout : plan.blackouts)
        if (when.from <= blackout.span.to && when.to >= blackout.span.from)
            return blackout.label;

    if (plan.hadDeath && when.from < plan.lastDeath + separation)
        return "gap";

    for (auto const& [star, death] : plan.deaths)
        if (star != self && when.from < death + separation && when.to > death - separation)
            return "spacing";

    return nullptr;
}

// How far `when` stays from every other explosion, or -1 inside a blackout.
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

// When the team's kill would land: the walk, then its speed, which only the star already being hit can tell.
StarSpan FinishBand(AlgalonEncounterState const& state, Group* group, AlgalonScanUnit const& star)
{
    int32 const walk = static_cast<int32>(TeamWalkSeconds(state, group, star) * IN_MILLISECONDS);
    bool const measured = star.guid == state.focusStar.Get() && star.lossMeasured &&
                          star.lossPctPerSec >= ULDUAR_ALGALON_STAR_KILL_SLOW_PCT_PER_SEC;
    float const fast = measured ? star.lossPctPerSec : ULDUAR_ALGALON_STAR_KILL_FAST_PCT_PER_SEC;
    float const slow = measured ? star.lossPctPerSec : ULDUAR_ALGALON_STAR_KILL_SLOW_PCT_PER_SEC;
    return {walk + StarDeathAt(star, fast), walk + StarDeathAt(star, slow)};
}

// Spacing i explosions takes i gaps plus the kills, plus the walk and every blackout on the way, so the i-th
// soonest death sets a deadline. Stars of one wave spawn and drain together.
bool SerialDue(StarPlan const& plan, AlgalonEncounterState const& state, Group* group, AlgalonScanUnit const& star)
{
    int32 const death = ForecastDeath(plan, &star);
    uint32 index = 0;
    for (auto const& [other, otherDeath] : plan.deaths)
        if (other != &star && (otherDeath < death || (otherDeath == death && other < &star)))
            ++index;

    int32 budget = static_cast<int32>((index + 1) * ULDUAR_ALGALON_STAR_SERIAL_SECONDS * IN_MILLISECONDS) +
                   static_cast<int32>(TeamWalkSeconds(state, group, star) * IN_MILLISECONDS);
    for (StarBlackout const& blackout : plan.blackouts)
        budget += std::max(0, std::min(death, blackout.span.to) - std::max(0, blackout.span.from));

    return death <= budget;
}

// Unavoidable and raid wide, so the weakest member decides whether it can be paid for: everyone must
// survive its top roll. Phased members are out of its reach. Someone whose max health is below it can't
// do better than full.
bool RaidAffordsExplosion(Player* bot, Group* group)
{
    if (!group)
        return true;

    uint32 const blast =
        bot->GetMap()->Is25ManRaid() ? ULDUAR_ALGALON_STAR_EXPLOSION_MAX_25 : ULDUAR_ALGALON_STAR_EXPLOSION_MAX_10;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (CanStayOut(member) && member->GetHealth() < std::min(blast, member->GetMaxHealth()))
            return false;
    }

    return true;
}

// The star the team may kill now, or nullptr with the reason it may not. Every death is planned against the
// others' forecasts, Big Bang and the meteors.
char const* StarWindow(Player* bot, Unit* boss, AlgalonEncounterState const& state, uint32 now,
                       AlgalonScanUnit const*& star)
{
    star = nullptr;
    Group* group = bot->GetGroup();
    if (state.stars.empty())
        return "none";

    // No hole and a Big Bang coming: the one this star leaves behind is worth more than its damage.
    if (state.urgent)
    {
        star = LowestStar(state);
        return "urgent";
    }

    if (state.bigBangCasting)
        return "bigbang";

    StarPlan const plan = PlanStars(state, now);
    int32 const gap = static_cast<int32>(ULDUAR_ALGALON_STAR_PACING_GAP_MS);

    // Ahead of the gap, or any other death drops a kill half done. Dropping a star a few % from dead doesn't
    // move its explosion, since its DoTs and the drain finish it anyway, so that only counts when it would
    // then die clear.
    if (AlgalonScanUnit const* focus = FindScanned(state.stars, state.focusStar.Get()))
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
            int32 const dropped = StarDeathAt(*focus, ULDUAR_ALGALON_STAR_DROPPED_PCT_PER_SEC);
            if (!StarCollision(plan, finish, focus, keep) ||
                (StarCollision(plan, {dropped, dropped}, focus, keep) && finish.to < dropped))
            {
                star = focus;
                return state.focusByDeadline ? "deadline" : "open";
            }
        }
    }

    if (state.lastStarDeathMs && getMSTimeDiff(state.lastStarDeathMs, now) < ULDUAR_ALGALON_STAR_PACING_GAP_MS)
        return "gap";

    // Due: its own death would land badly, or the serial deadline says so. Past that, waiting for raid health
    // or clear ground means letting two go off at once. Never one under a swap tank or the handler: a hole
    // there sets him loose.
    // A serial deadline makes every star dying no later due too: of a tied wave only the last rank fires, and
    // the soonest killable one is what should go.
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
                char const* collision = StarCollision(plan, finish, candidate, gap);
                if (!collision)
                {
                    star = candidate;
                    return "deadline";
                }

                if (!std::strcmp(blocked, "near"))
                    blocked = collision;

                int32 const separation = StarSeparation(plan, finish, candidate);
                if (separation > fallbackSeparation)
                {
                    fallback = candidate;
                    fallbackSeparation = separation;
                }
            }
        }

        // Nothing lands clear: still kill the one that lands further from the rest than their own deaths do,
        // or two due stars 2 s apart both drain out together.
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

    if (!RaidAffordsExplosion(bot, group))
        return "raidhp";

    std::vector<AlgalonScanUnit const*> open;
    for (AlgalonScanUnit const& candidate : state.stars)
        if (StarPassesFilter(state, group, boss, candidate, StarFilter::Clear))
            open.push_back(&candidate);

    std::sort(open.begin(), open.end(), [&plan](AlgalonScanUnit const* a, AlgalonScanUnit const* b)
              { return ForecastDeath(plan, a) < ForecastDeath(plan, b); });

    char const* blocked = "near";
    for (AlgalonScanUnit const* candidate : open)
    {
        char const* collision = StarCollision(plan, FinishBand(state, group, *candidate), candidate, gap);
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
        {
            continue;
        }

        // Healers prefer the inner ring (heal range is 30 yd to the tank), but a raid with more healers
        // than inner slots still places everyone.
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

    // A slot assigned before the pull opened a trace was never written into it.
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

// Same order on every bot, so the arc fills in step. Past ten melee, two share a spot.
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
        {
            staged.push_back(member->GetGUID());
        }
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

// `distance` from `origin` towards `toward`, inside the room, or false if that is in his reach.
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

// A hole further from the bot's own spot than it can run in the cast: it waits on the way there instead.
// Measured from the spot, not the bot, or the bot would walk back the moment it got there.
bool FarHoleSpot(Player* bot, Unit* boss, AlgalonEncounterState const& state, Position const& anchor,
                 Position& spot)
{
    if (state.holes.empty() || state.swapPair.count(bot->GetGUID()) || state.handler.Get() == bot->GetGUID())
        return false;

    if (!state.bigBangCasting && !BigBangDueWithin(state, ULDUAR_ALGALON_FAR_HOLE_SECONDS, getMSTime()))
        return false;

    if (AlgalonStaysOut(bot))
        return false;

    AlgalonScanUnit const* nearest = nullptr;
    float best = std::numeric_limits<float>::max();
    for (AlgalonScanUnit const& hole : state.holes)
    {
        float const distance = hole.position.GetExactDist2d(anchor);
        if (distance < best)
        {
            nearest = &hole;
            best = distance;
        }
    }

    if (!nearest || best <= ULDUAR_ALGALON_FAR_HOLE_YARDS)
        return false;

    if (SpotOffFrom(boss, nearest->position, anchor, ULDUAR_ALGALON_NEAR_HOLE_STANDOFF, spot))
        return true;

    // Straight towards the spot is in his reach, as for the healers ringing him: the nearest point round the
    // hole that isn't.
    float closest = std::numeric_limits<float>::max();
    for (uint8 step = 0; step < 16; ++step)
    {
        float const angle = static_cast<float>(step) * static_cast<float>(M_PI) / 8.0f;
        float const x = nearest->position.GetPositionX() + std::cos(angle) * ULDUAR_ALGALON_NEAR_HOLE_STANDOFF;
        float const y = nearest->position.GetPositionY() + std::sin(angle) * ULDUAR_ALGALON_NEAR_HOLE_STANDOFF;
        float const distance = anchor.GetExactDist2d(x, y);
        if (distance >= closest || !AlgalonSpotInRoom(x, y) ||
            (boss && boss->GetExactDist2d(x, y) < ULDUAR_ALGALON_FORMATION_CLEARANCE))
        {
            continue;
        }

        closest = distance;
        spot = Position(x, y, ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
    }

    return closest != std::numeric_limits<float>::max();
}

// A ranged star-team member whose focus star is out of range of its ring slot works it from a spot of its
// own: with the slot as its spot, "reach spell" and the slot node pull it opposite ways.
bool StarTeamSpot(Player* bot, Unit* boss, AlgalonEncounterState const& state, Position const& anchor,
                  Position& spot)
{
    if (!IsAlgalonStarTeam(bot))
        return false;

    AlgalonScanUnit const* star = FindScanned(state.stars, state.focusStar.Get());
    return star && star->position.GetExactDist2d(anchor) > ULDUAR_ALGALON_STAR_TEAM_REACH &&
           SpotOffFrom(boss, star->position, anchor, ULDUAR_ALGALON_STAR_TEAM_STANDOFF, spot);
}

// Swap tanks wait inside his reach of the landing spot. Once a swap tank has him, the holder walks him out
// to the tank slot and the other waits in his reach there; until then the pickup tank keeps its spot,
// or a missed taunt would send it walking away from him while he is loose.
char const* SwapTankSlot(Player* bot, PlayerbotAI* botAI, Position& slot)
{
    Unit* boss = GetAlgalon(botAI);
    bool const selectable = boss && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) && AlgalonEngaged(botAI);
    Player* holder = selectable ? GetAlgalonBossTank(botAI) : nullptr;
    bool const pairHolds = holder && IsAlgalonSwapTank(holder);
    if (pairHolds && holder == bot)
    {
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
        // Held anywhere else, by a human or on the walk out: in his reach, on the tank slot's side.
        float const angle = boss->GetAngle(&ULDUAR_ALGALON_TANK_SLOT);
        slot = Position(boss->GetPositionX() + std::cos(angle) * ULDUAR_ALGALON_OFFTANK_STEP,
                        boss->GetPositionY() + std::sin(angle) * ULDUAR_ALGALON_OFFTANK_STEP,
                        ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
    }

    return "offtank";
}

// The undisplaced spot and what it is, or nullptr. A far hole before Big Bang beats a far star.
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

    // A released melee bot has no spot of its own; it belongs at him.
    Position const anchor = kind ? slot : boss->GetPosition();
    Position spot;
    if (FarHoleSpot(bot, boss, *state, anchor, spot))
    {
        slot = spot;
        return "nearhole";
    }

    if (kind && PlayerbotAI::IsRanged(bot) && StarTeamSpot(bot, boss, *state, anchor, spot))
    {
        slot = spot;
        return "star";
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

// The first hole a straight walk would pass within `clearance` of. A walker inside a field, or phased by
// one, is leaving that one and may cross it, but no other: a neighbour 8 yd off phases it all over again.
// Nobody may come closer to a field it already stands beside.
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

// Round the hole, preferring a point the slot is in clear sight of, so the next walk goes straight there.
// A point that brings the slot no closer is skipped: standing on it, the bot's own spot wins every
// later search and the walk stalls beside the hole.
bool FindHoleDetour(Player* bot, AlgalonEncounterState const& state, AlgalonScanUnit const& hole, Position const& to,
                    Position& waypoint)
{
    bool const phased = IsAlgalonPhased(bot);
    float const remaining = bot->GetExactDist2d(to);
    float best = std::numeric_limits<float>::max();
    bool bestThrough = false;
    for (uint8 step = 0; step < 16; ++step)
    {
        float const angle = static_cast<float>(step) * static_cast<float>(M_PI) / 8.0f;
        float const x = hole.position.GetPositionX() + std::cos(angle) * ULDUAR_ALGALON_HOLE_DETOUR_RADIUS;
        float const y = hole.position.GetPositionY() + std::sin(angle) * ULDUAR_ALGALON_HOLE_DETOUR_RADIUS;
        float const onward = to.GetExactDist2d(x, y);
        if (onward >= remaining - 1.0f || !AlgalonSpotInRoom(x, y) || !ClearOfHoles(state, x, y) ||
            !ClearOfMarkers(state, x, y) || HoleAcrossPath(state, bot->GetPosition(), x, y, phased))
        {
            continue;
        }

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

// Holes land wherever a star died, so a spot can end up buried. Step aside from the spot itself, nearest
// ring first, rather than abandon the formation.
bool DisplaceFromHoles(Player* bot, AlgalonEncounterState const& state, Position const& slot, Position& position)
{
    for (float radius = 2.0f; radius <= ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS; radius += 2.0f)
    {
        for (uint8 step = 0; step < 16; ++step)
        {
            float const angle = static_cast<float>(step) * static_cast<float>(M_PI) / 8.0f;
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

// Returns the traced branch; `found` says whether `position` is somewhere to walk to.
char const* DeriveSlot(Player* bot, Position& position, bool& found)
{
    found = false;
    Position slot;
    char const* const kind = RawSlot(bot, slot);
    if (!kind)
        return "none";

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    // Test the slot, not the bot: a bot that already dodged passes trivially and walks back under it.
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

    // A phased bot is already in the void; for everyone else the walk itself can clip a field.
    if (!IsAlgalonPhased(bot) && bot->GetExactDist2d(position) > ULDUAR_ALGALON_SLOT_TOLERANCE)
    {
        if (AlgalonScanUnit const* hole =
                HoleAcrossPath(*state, bot->GetPosition(), position.GetPositionX(), position.GetPositionY(), false))
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

}  // namespace

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

    // A wipe despawns him and summons a fresh one 2s later, and every bot is dead or phased while that
    // happens. Instance state is the only thing that notices, never one bot's view of the room.
    bool const wiped = !state->boss.IsEmpty() &&
                       (!boss || boss->GetGUID() != state->boss || (state->engagedSeen && !boss->IsInCombat()));
    if (wiped)
    {
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

    EnsureSwapPair(bot, boss, *state);

    if (!state->introEndMs && boss->IsInCombat() && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
    {
        state->introEndMs = now;
        state->nextBigBangMs = now + ULDUAR_ALGALON_FIRST_BIG_BANG_MS;
        NotePull(group, boss, *state);
    }

    EnsureAlgalonSlotAssignments(bot, *state);

    if (boss->IsInCombat())
        ScanRoom(bot, boss, *state, now);

    AlgalonPhase const phase = ReadPhase(boss, *state);
    state->phase = phase;
    state->engaged = phase == AlgalonPhase::Intro || phase == AlgalonPhase::One || phase == AlgalonPhase::Two;
    if (!state->engaged)
        return;

    ReleaseMelee(boss, *state, now);

    bool const casting = CurrentBigBang(boss) != nullptr;
    if (casting && !state->bigBangCasting)
    {
        ++state->bigBangCount;
        state->nextBigBangMs = now + ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS;
        state->shelterAssignments.clear();
        ElectBigBangSoakers(bot, boss, *state);
        NoteSoakKit(group, *state);
        CallBigBang(group, boss, *state, true);
    }
    else if (casting)
    {
        KeepBigBangSoaker(bot, *state);
        CallBigBang(group, boss, *state, false);
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
    state->urgent = phase == AlgalonPhase::One && state->holes.empty() &&
                    (casting || BigBangDueWithin(*state, ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS, now));

    Player* handler = ElectHandler(bot, *state);
    state->handler = handler ? handler->GetGUID() : ObjectGuid::Empty;

    if (phase != AlgalonPhase::Intro)
    {
        CallHumanTank(group, botAI, boss, *state, now);
        NoteTankRoles(group, boss, *state, now);
        NoteHandover(group, boss, *state, now);
    }

    if (phase != AlgalonPhase::One)
    {
        state->focusStar = ObjectGuid::Empty;
        return;
    }

    ElectStarTeam(bot, *state);

    AlgalonScanUnit const* star = nullptr;
    char const* window = StarWindow(bot, boss, *state, now, star);
    ObjectGuid const previous = state->focusStar.Get();
    ObjectGuid const next = star ? star->guid : ObjectGuid::Empty;
    if (next != previous)
    {
        if (FindScanned(state->stars, previous))
        {
            state->droppedStars.insert(previous);
            state->starHealthLog.erase(previous);
        }

        state->focusByDeadline = false;
    }

    // One way: a kill taken again at a deadline keeps the deadline's looser position test from then on.
    if (star && !std::strcmp(window, "deadline"))
        state->focusByDeadline = true;

    state->focusStar = next;

    if (RaidObs::Active())
        if (Player* tracker = FindTracker(group))
            RaidObs::NoteDerived(tracker, "algalon.starwindow", window);
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

    // Signed: a cast running late is imminent, not a cycle away.
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
    {
        return nullptr;
    }

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
            // One Earth Shield per target, whoever cast it.
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

    // Instants first: the next swing lands about a second after the raid is back.
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
    if (!state || state->handler.Get() != bot->GetGUID())
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
    return state->bigBangSoaker.Get() == guid || state->bigBangBackup.Get() == guid || AlgalonHoldsConstellation(bot);
}

bool AlgalonCanDisperse(Player* bot)
{
    return bot && bot->HasSpell(SPELL_ALGALON_DISPERSION) && !bot->HasSpellCooldown(SPELL_ALGALON_DISPERSION);
}

int32 AlgalonHideSlackMs(Player* bot)
{
    if (!AlgalonBigBangLatched(bot) || IsAlgalonPhased(bot) || AlgalonStaysOut(bot))
        return std::numeric_limits<int32>::max();

    AlgalonEncounterState const* state = FindState(bot);
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!state || !botAI || state->holes.empty())
        return std::numeric_limits<int32>::max();

    float nearest = std::numeric_limits<float>::max();
    for (AlgalonScanUnit const& hole : state->holes)
        nearest = std::min(nearest, bot->GetExactDist2d(hole.position));

    float const walk = std::max(0.0f, nearest - (ULDUAR_ALGALON_SHELTER_RADIUS - 1.5f));
    float const speed = std::max(1.0f, bot->GetSpeed(MOVE_RUN));
    int32 const walkMs = static_cast<int32>(walk / speed * IN_MILLISECONDS);
    return AlgalonBigBangRemainingMs(botAI) - walkMs - ULDUAR_ALGALON_HIDE_MARGIN_MS;
}

bool AlgalonShouldRunForShelter(Player* bot) { return AlgalonHideSlackMs(bot) <= 0; }

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
    else if (AlgalonHoldsConstellation(bot))
        role = AlgalonHideRole::Hold;
    else if (state->holes.empty())
        role = AlgalonHideRole::NoShelter;
    else if (!AlgalonShouldRunForShelter(bot))
        role = AlgalonHideRole::Wait;
    else
        role = GetAlgalonShelter(bot, shelter) ? AlgalonHideRole::Run : AlgalonHideRole::NoShelter;

    // Only once he has been seen, or every Ulduar trace before him carries a row of these per bot.
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

    auto const latched = state->shelterAssignments.find(bot->GetGUID());
    if (latched != state->shelterAssignments.end())
    {
        if (AlgalonScanUnit const* hole = FindScanned(state->holes, latched->second))
        {
            shelter = hole->position;
            return true;
        }
    }

    // Nearest, but not the hole a constellation is being walked into unless it is the only one.
    AlgalonScanUnit const* nearest = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    for (AlgalonScanUnit const& hole : state->holes)
    {
        bool kiteTarget = false;
        for (auto const& kite : state->kiteHoleAssignments)
            kiteTarget = kiteTarget || kite.second == hole.guid;

        float const distance = bot->GetExactDist2d(hole.position) + (kiteTarget ? 1000.0f : 0.0f);
        if (distance < bestDistance)
        {
            nearest = &hole;
            bestDistance = distance;
        }
    }

    if (!nearest)
        return false;

    state->shelterAssignments[bot->GetGUID()] = nearest->guid;
    shelter = nearest->position;
    return true;
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

    // A human isn't on a trigger, so a bot takes him whenever one can, unless it is already at the swap
    // stack count: a fifth Phase Punch phases it out again within seconds.
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

    // Mid cast the elected soaker is the one staying out, so it is the one to take him, off the other swap
    // tank too when that one was handed the soak.
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

    // The holder is this cast's soaker. Taking him now leaves the soaker hiding and the wrong tank out.
    if (AlgalonBigBangCasting(botAI))
        return false;

    return IsAlgalonPhased(holder) || SwapDue(*state, holder, bot, getMSTime());
}

bool AlgalonYieldsBoss(Player* bot)
{
    // The pair and the handler are the tanks there are; reading the bot's role instead walks its
    // strategies, and the damage guard asks this for every action every damage dealer pops.
    AlgalonEncounterState const* state = FindState(bot);
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI || !state || (!state->swapPair.count(bot->GetGUID()) && state->handler.Get() != bot->GetGUID()))
        return false;

    Player* holder = GetAlgalonBossTank(botAI);
    if (!holder || holder == bot || !IsAlgalonSwapTank(holder) || IsAlgalonPhased(holder))
        return false;

    return !AlgalonShouldPickUp(bot) && !AlgalonShouldSwap(bot);
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

    // Loose ones first: a constellation follows its victim into a hole during Big Bang and closes it on
    // the raid, so every one ends up on the handler, who stays out. Barrage hits a random raid member
    // anyway, so holding them costs nothing. Only a spare hole takes one away.
    Creature* loose = nullptr;
    Creature* held = nullptr;
    float looseDistance = std::numeric_limits<float>::max();
    for (AlgalonScanUnit const& constellation : state->constellations)
    {
        Creature* unit = constellation.active ? Resolve(bot, constellation.guid) : nullptr;
        if (!unit)
            continue;

        if (unit->GetVictim() == bot)
        {
            held = held ? held : unit;
            continue;
        }

        float const distance = bot->GetExactDist2d(unit->GetPositionX(), unit->GetPositionY());
        if (distance < looseDistance)
        {
            loose = unit;
            looseDistance = distance;
        }
    }

    char const* branch = "none";
    Unit* chosen = nullptr;
    if (loose)
    {
        chosen = loose;
        branch = "taunt";
    }
    else if (held && state->holes.size() > ReservedHoles(*state, getMSTime()))
        chosen = held;
    else if (held)
        branch = "kept";

    if (!chosen)
        state->kiteHoleAssignments.erase(bot->GetGUID());

    // Once it is on the handler the kite owns the key, as hole or parked.
    if (RaidObs::Active() && (!chosen || chosen->GetVictim() != bot))
        RaidObs::NoteDerived(bot, "algalon.kite", branch);

    return chosen;
}

bool GetAlgalonKiteSpot(Player* bot, Unit* constellation, Position& spot)
{
    AlgalonEncounterState* state = FindState(bot);
    if (!state || !constellation)
        return false;

    AlgalonScanUnit const* hole = nullptr;
    auto const latched = state->kiteHoleAssignments.find(bot->GetGUID());
    if (latched != state->kiteHoleAssignments.end())
        hole = FindScanned(state->holes, latched->second);

    if (!hole)
    {
        // Nearest to the constellation, not the handler: it is what has to arrive.
        float bestDistance = std::numeric_limits<float>::max();
        for (AlgalonScanUnit const& candidate : state->holes)
        {
            float const distance = constellation->GetExactDist2d(candidate.position);
            if (distance < bestDistance)
            {
                hole = &candidate;
                bestDistance = distance;
            }
        }

        if (!hole)
        {
            state->kiteHoleAssignments.erase(bot->GetGUID());
            return false;
        }

        state->kiteHoleAssignments[bot->GetGUID()] = hole->guid;
        state->kiteBearings.erase(bot->GetGUID());
    }

    // Past the hole on the far side of where the constellation was, so the chase drags it through the
    // field. Latched: the handler walks round the hole, and a bearing read off the constellation each tick
    // would turn with it.
    auto bearing = state->kiteBearings.find(bot->GetGUID());
    if (bearing == state->kiteBearings.end())
        bearing = state->kiteBearings
                      .emplace(bot->GetGUID(), Position::NormalizeOrientation(hole->position.GetAngle(constellation) +
                                                                              static_cast<float>(M_PI)))
                      .first;

    float const farAngle = bearing->second;
    float x = hole->position.GetPositionX() + std::cos(farAngle) * ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET;
    float y = hole->position.GetPositionY() + std::sin(farAngle) * ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET;

    // He evades if dragged past 47 yd, and so would the tank walking him out there on a swap.
    ClampIntoRoom(x, y);

    spot = ValidateFloorPoint(bot, Position(x, y, hole->position.GetPositionZ()));
    return true;
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

    for (AlgalonScanUnit const& star : state->stars)
        if (StarDying(star))
            hazards.emplace_back(star.position, ULDUAR_ALGALON_STAR_DYING_RADIUS);

    return hazards;
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
    // Rule predicates ask this for every action: answer the common case before deriving a spot.
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !state->engaged || state->holes.empty() ||
        (!state->bigBangCasting && !BigBangDueWithin(*state, ULDUAR_ALGALON_FAR_HOLE_SECONDS, getMSTime())))
    {
        return false;
    }

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

void AppendAlgalonTargetExclusions(PlayerbotAI* botAI, GuidSet& exclusions)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !state->engaged)
        return;

    if (AlgalonYieldsBoss(bot))
        exclusions.insert(state->boss);

    bool const starTeam = IsAlgalonStarTeam(bot);
    for (AlgalonScanUnit const& unit : state->stars)
        if (!starTeam || unit.guid != state->focusStar.Get())
            exclusions.insert(unit.guid);

    bool const handler = state->handler.Get() == bot->GetGUID();
    if (!handler)
        for (AlgalonScanUnit const& unit : state->constellations)
            exclusions.insert(unit.guid);

    // With no bot to collect them, Dark Matter is left to whoever it chases rather than to nobody.
    if (!handler && !state->handler.Get().IsEmpty())
        for (AlgalonScanUnit const& unit : state->darkMatter)
            exclusions.insert(unit.guid);
}

bool AlgalonMayDamage(Player* bot, Unit* target)
{
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !state->engaged || !target)
        return true;

    // Ahead of the skull, which raids put on him: a yielding tank hitting him takes him back by threat.
    if (target->GetEntry() == PB_NPC_ALGALON)
        return !AlgalonYieldsBoss(bot);

    // A skull a player put there is a call the raid made; "attack rti target" ignores the exclusions.
    ObjectGuid const guid = target->GetGUID();
    Group* group = bot->GetGroup();
    if (group && group->GetTargetIcon(RtiTargetValue::skullIndex) == guid)
        return true;

    bool const handler = state->handler.Get() == bot->GetGUID();
    switch (target->GetEntry())
    {
        case PB_NPC_COLLAPSING_STAR:
            return guid == state->focusStar.Get() && IsAlgalonStarTeam(bot);
        case PB_NPC_LIVING_CONSTELLATION:
            return handler;
        case PB_NPC_UNLEASHED_DARK_MATTER:
            return handler || state->handler.Get().IsEmpty();
        default:
            return true;
    }
}
