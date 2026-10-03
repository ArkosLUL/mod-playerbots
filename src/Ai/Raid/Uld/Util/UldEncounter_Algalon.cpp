/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Algalon.h"

#include "CellImpl.h"
#include "Chat.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
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

// navprobe map 603: every point below and the ten melee staging spots are on mesh at 0.04, settled
// Z 417.321.
const Position ULDUAR_ALGALON_ROOM_CENTER = Position(1632.668f, -302.7656f, 417.3211f);
const Position ULDUAR_ALGALON_TANK_SLOT = Position(1632.668f, -310.766f, 417.321f);
const Position ULDUAR_ALGALON_OFFTANK_SLOT = Position(1628.668f, -309.694f, 417.321f);

const std::array<Position, ULDUAR_ALGALON_TOTAL_SLOTS> ULDUAR_ALGALON_RING_SLOTS = {{
    Position(1651.668f, -302.766f, 417.321f),  // healer, 19 yd at 0 deg
    Position(1613.740f, -304.422f, 417.321f),  // healer, 19 yd at 185 deg
    Position(1637.586f, -284.413f, 417.321f),  // healer, 19 yd at 75 deg
    Position(1627.750f, -284.413f, 417.321f),  // healer, 19 yd at 105 deg
    Position(1643.233f, -280.108f, 417.321f),  // ranged, 25 yd at 65 deg
    Position(1618.329f, -282.287f, 417.321f),  // ranged, 25 yd at 125 deg
    Position(1656.816f, -296.295f, 417.321f),  // ranged, 25 yd at 15 deg
    Position(1608.048f, -298.424f, 417.321f),  // ranged, 25 yd at 170 deg
    Position(1656.816f, -309.236f, 417.321f),  // ranged, 25 yd at 345 deg
    Position(1612.189f, -317.105f, 417.321f),  // ranged, 25 yd at 215 deg
    Position(1632.668f, -271.766f, 417.321f),  // ranged, 31 yd at 90 deg
    Position(1654.588f, -280.845f, 417.321f),  // ranged, 31 yd at 45 deg
    Position(1610.748f, -280.845f, 417.321f),  // ranged, 31 yd at 135 deg
    Position(1663.668f, -302.766f, 417.321f),  // ranged, 31 yd at 0 deg
    Position(1601.668f, -302.766f, 417.321f),  // ranged, 31 yd at 180 deg
    Position(1659.515f, -318.266f, 417.321f),  // ranged, 31 yd at 330 deg
    Position(1610.748f, -324.686f, 417.321f),  // ranged, 31 yd at 225 deg
    Position(1632.668f, -333.766f, 417.321f),  // ranged, 31 yd at 270 deg
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
                state.stars.push_back(unit);
                break;
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

    // Nothing in the trace says who killed a star, and the damage stream only records hits on players.
    if (!phaseTwoStarted)
    {
        for (AlgalonScanUnit const& star : previousStars)
        {
            if (FindScanned(state.stars, star.guid))
                continue;

            char const* cause = star.healthPct <= ULDUAR_ALGALON_STAR_COLLAPSED_HP_PCT ? "collapsed" : "killed";
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

// Whether `taker` should take him off `holder`: the 4th stack, or a holder deep in stacks with a Big Bang
// close, since the Phase Punch queued behind the cast lands while the rest of the raid is phased.
bool SwapDue(AlgalonEncounterState const& state, Player* holder, Player* taker, uint32 now)
{
    uint8 const own = GetAlgalonPhasePunchStacks(taker);
    uint8 const theirs = GetAlgalonPhasePunchStacks(holder);
    if (own >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
        return false;

    if (theirs >= ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS)
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

    if (castStarted)
    {
        state.bigBangHideCalled = false;
        Player* soaker = FindGroupMember(group, state.bigBangSoaker.Get());
        std::string const stays =
            soaker ? soaker->GetName() + " stays out with a defensive. Everyone else" : std::string("Everyone");
        SendAlgalonCall(tracker, "bigbang", "Big Bang in 8 s: " + stays + ", get next to a black hole");
        return;
    }

    Spell* cast = CurrentBigBang(boss);
    if (state.bigBangHideCalled || !cast || cast->GetCastTimeRemaining() > ULDUAR_ALGALON_HIDE_CALL_MS)
        return;

    state.bigBangHideCalled = true;
    SendAlgalonCall(tracker, "hide", "Big Bang: into a black hole NOW and stay in it until it lands");
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

void ReleaseMelee(Creature* boss, AlgalonEncounterState& state, uint32 now)
{
    if (state.meleeReleased || !state.introEndMs)
        return;

    Player* holder = BossHolder(boss);
    bool const held = holder && state.swapPair.count(holder->GetGUID());
    if (held || getMSTimeDiff(state.introEndMs, now) >= ULDUAR_ALGALON_MELEE_RELEASE_MS)
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
        soaker = holder;

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
// cast, the backup takes the duty and someone else backs that up.
void KeepBigBangSoaker(Player* bot, AlgalonEncounterState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    Player* soaker = FindGroupMember(group, state.bigBangSoaker);
    Player* backup = FindGroupMember(group, state.bigBangBackup);
    if (!CanStayOut(soaker) && CanStayOut(backup))
    {
        soaker = backup;
        backup = nullptr;
        state.bigBangSoaker = soaker->GetGUID();
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

// Lowest health first: Collapse drains 1% a second, so health is the star's remaining lifetime and
// killing the shortest-lived one keeps the explosions apart. `clearOnly` skips stars a raid member is
// standing under, since the hole lands where the star dies.
AlgalonScanUnit const* LowestStar(AlgalonEncounterState const& state, Group* group, bool clearOnly)
{
    AlgalonScanUnit const* lowest = nullptr;
    for (AlgalonScanUnit const& star : state.stars)
    {
        if (clearOnly && NearestRaidMember(group, star.position) < ULDUAR_ALGALON_STAR_CLEAR_RADIUS)
            continue;

        if (!lowest || star.healthPct < lowest->healthPct)
            lowest = &star;
    }

    return lowest;
}

// Spacing i explosions takes i gaps plus the kills, so the i-th shortest-lived star sets a deadline.
// Stars of one wave spawn and drain together; past the deadline, waiting for raid health or clear ground
// means letting two of them go off at once.
bool StarDeadlineDue(AlgalonEncounterState const& state)
{
    std::vector<float> lifetimes;
    for (AlgalonScanUnit const& star : state.stars)
        lifetimes.push_back(star.healthPct);

    std::sort(lifetimes.begin(), lifetimes.end());
    for (size_t index = 0; index < lifetimes.size(); ++index)
        if (lifetimes[index] <= static_cast<float>((index + 1) * ULDUAR_ALGALON_STAR_SERIAL_SECONDS))
            return true;

    return false;
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

// The star the team may kill now, or nullptr with the reason it may not.
char const* StarWindow(Player* bot, AlgalonEncounterState const& state, uint32 now, AlgalonScanUnit const*& star)
{
    star = nullptr;
    Group* group = bot->GetGroup();
    if (state.stars.empty())
        return "none";

    // No hole and a Big Bang coming: the one this star leaves behind is worth more than its damage.
    if (state.urgent)
    {
        star = LowestStar(state, group, false);
        return "urgent";
    }

    if (state.bigBangCasting)
        return "bigbang";

    if (!state.markers.empty())
        return "smash";

    if (state.lastStarDeathMs && getMSTimeDiff(state.lastStarDeathMs, now) < ULDUAR_ALGALON_STAR_PACING_GAP_MS)
        return "gap";

    if (StarDeadlineDue(state))
    {
        star = LowestStar(state, group, false);
        return "deadline";
    }

    if (!RaidAffordsExplosion(bot, group))
        return "raidhp";

    // The star the team is already on keeps it a little closer to the raid, or a drift past the edge
    // flips the whole team to another star and back.
    if (AlgalonScanUnit const* focus = FindScanned(state.stars, state.focusStar.Get()))
    {
        if (NearestRaidMember(group, focus->position) >= ULDUAR_ALGALON_STAR_CLEAR_RADIUS - 2.0f)
        {
            star = focus;
            return "open";
        }
    }

    star = LowestStar(state, group, true);
    return star ? "open" : "near";
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

    size_t const count = std::min<size_t>(staged.size(), ULDUAR_ALGALON_MELEE_STAGE_SPOTS);
    size_t const index = static_cast<size_t>(own - staged.begin()) % ULDUAR_ALGALON_MELEE_STAGE_SPOTS;
    float const bearing = static_cast<float>(M_PI) / 2.0f +
                          (static_cast<float>(index) - static_cast<float>(count - 1) / 2.0f) *
                              ULDUAR_ALGALON_MELEE_STAGE_STEP;
    spot = Position(ULDUAR_ALGALON_ROOM_CENTER.GetPositionX() + std::cos(bearing) * ULDUAR_ALGALON_MELEE_STAGE_RADIUS,
                    ULDUAR_ALGALON_ROOM_CENTER.GetPositionY() + std::sin(bearing) * ULDUAR_ALGALON_MELEE_STAGE_RADIUS,
                    ULDUAR_ALGALON_ROOM_CENTER.GetPositionZ());
    return true;
}

// The undisplaced spot and what it is, or nullptr. Swap tanks wait inside his reach, the one taking him
// at the end of the intro on the tank slot.
char const* RawSlot(Player* bot, Position& slot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    AlgalonEncounterState const* state = FindState(bot);
    if (!botAI || !state)
        return nullptr;

    if (IsAlgalonSwapTank(bot))
    {
        Unit* boss = GetAlgalon(botAI);
        bool const selectable = boss && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) && AlgalonEngaged(botAI);
        Player* holder = selectable ? GetAlgalonBossTank(botAI) : nullptr;
        bool const holds = holder && IsAlgalonSwapTank(holder) ? holder == bot : GetAlgalonPickupTank(botAI) == bot;
        slot = holds ? ULDUAR_ALGALON_TANK_SLOT : ULDUAR_ALGALON_OFFTANK_SLOT;
        return holds ? "tank" : "offtank";
    }

    if (AlgalonMeleeStaged(bot))
        return MeleeStageSpot(bot, *state, slot) ? "stage" : nullptr;

    if (!AlgalonTakesRingSlot(bot))
        return nullptr;

    auto const assignment = state->slotAssignments.find(bot->GetGUID());
    return assignment != state->slotAssignments.end() && TryGetAlgalonSlotPosition(assignment->second, slot)
               ? "slot"
               : nullptr;
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

// The first hole whose field a straight walk would clip. A walker inside a field, or phased by one, is
// leaving it and may cross it; anyone else may not come closer to a field it already stands beside.
AlgalonScanUnit const* HoleAcrossPath(AlgalonEncounterState const& state, Position const& from, float x, float y,
                                      bool phased)
{
    for (AlgalonScanUnit const& hole : state.holes)
    {
        float const start = hole.position.GetExactDist2d(from);
        bool const leaving = start <= ULDUAR_ALGALON_HOLE_EXIT_RADIUS &&
                             (phased || start <= ULDUAR_ALGALON_SHELTER_RADIUS);
        if (leaving)
            continue;

        float const limit =
            std::min(ULDUAR_ALGALON_HOLE_PATH_CLEARANCE, std::max(start - 0.5f, ULDUAR_ALGALON_SHELTER_RADIUS));
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
    if (AlgalonCosmicSmashMarkerNear(botAI, slot, ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE))
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
        CallBigBang(group, boss, *state, true);
    }
    else if (casting)
    {
        KeepBigBangSoaker(bot, *state);
        CallBigBang(group, boss, *state, false);
    }
    else if (state->bigBangCasting)
    {
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
    }

    if (phase != AlgalonPhase::One)
    {
        state->focusStar = ObjectGuid::Empty;
        return;
    }

    ElectStarTeam(bot, *state);

    AlgalonScanUnit const* star = nullptr;
    char const* window = StarWindow(bot, *state, now, star);
    state->focusStar = star ? star->guid : ObjectGuid::Empty;

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

bool AlgalonShouldRunForShelter(Player* bot)
{
    if (!AlgalonBigBangLatched(bot) || IsAlgalonPhased(bot) || AlgalonStaysOut(bot))
        return false;

    AlgalonEncounterState const* state = FindState(bot);
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!state || !botAI || state->holes.empty())
        return false;

    float nearest = std::numeric_limits<float>::max();
    for (AlgalonScanUnit const& hole : state->holes)
        nearest = std::min(nearest, bot->GetExactDist2d(hole.position));

    float const walk = std::max(0.0f, nearest - (ULDUAR_ALGALON_SHELTER_RADIUS - 1.5f));
    float const speed = std::max(1.0f, bot->GetSpeed(MOVE_RUN));
    int32 const walkMs = static_cast<int32>(walk / speed * IN_MILLISECONDS);
    return AlgalonBigBangRemainingMs(botAI) <= walkMs + ULDUAR_ALGALON_HIDE_MARGIN_MS;
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
    if (holder && IsAlgalonSwapTank(holder) && !IsAlgalonPhased(holder))
        return false;

    // Mid cast the elected soaker is the one staying out, so it is the one to take him.
    if (AlgalonBigBangLatched(bot))
        return GetAlgalonBigBangSoaker(botAI) == bot && holder != bot;

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
    }

    // Past the hole on the far side, so the chase drags it through the field. The spot converges as it
    // closes in, because the bearing it approaches from swings round to the handler's own.
    float const farAngle = Position::NormalizeOrientation(hole->position.GetAngle(constellation) + static_cast<float>(M_PI));
    float x = hole->position.GetPositionX() + std::cos(farAngle) * ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET;
    float y = hole->position.GetPositionY() + std::sin(farAngle) * ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET;

    // He evades if dragged past 47 yd, and so would the tank walking him out there on a swap.
    float const fromCenter = ULDUAR_ALGALON_ROOM_CENTER.GetExactDist2d(x, y);
    if (fromCenter > ULDUAR_ALGALON_ROOM_CLAMP)
    {
        float const scale = ULDUAR_ALGALON_ROOM_CLAMP / fromCenter;
        x = ULDUAR_ALGALON_ROOM_CENTER.GetPositionX() + (x - ULDUAR_ALGALON_ROOM_CENTER.GetPositionX()) * scale;
        y = ULDUAR_ALGALON_ROOM_CENTER.GetPositionY() + (y - ULDUAR_ALGALON_ROOM_CENTER.GetPositionY()) * scale;
    }

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
    return botAI && !IsAlgalonPhased(bot) &&
           AlgalonCosmicSmashMarkerNear(botAI, bot->GetPosition(), ULDUAR_ALGALON_COSMIC_SMASH_HOLD_RADIUS);
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

    return hazards;
}

bool AlgalonSpotInRoom(float x, float y)
{
    return ULDUAR_ALGALON_ROOM_CENTER.GetExactDist2d(x, y) <= ULDUAR_ALGALON_ROOM_CLAMP;
}

bool AlgalonPathCrossesHole(Player* bot, float x, float y)
{
    AlgalonEncounterState const* state = FindState(bot);
    return state && HoleAcrossPath(*state, bot->GetPosition(), x, y, IsAlgalonPhased(bot));
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
