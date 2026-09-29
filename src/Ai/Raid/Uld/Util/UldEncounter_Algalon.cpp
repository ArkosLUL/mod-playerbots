/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Algalon.h"

#include "CellImpl.h"
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
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Timer.h"
#include "UldEncounterGate.h"
#include "Unit.h"
#include <RtiTargetValue.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>

using namespace EncounterHelpers;

// navprobe map 603: both on mesh at 0.04, settled Z 417.321.
const Position ULDUAR_ALGALON_ROOM_CENTER = Position(1632.668f, -302.7656f, 417.3211f);
const Position ULDUAR_ALGALON_TANK_SLOT = Position(1632.7f, -321.5f, 417.321f);

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

void ScanRoom(Player* bot, Creature* boss, AlgalonEncounterState& state, uint32 now)
{
    std::vector<AlgalonScanUnit> const previousHoles = std::move(state.holes);
    size_t const previousStars = state.stars.size();
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
    if (state.stars.size() < previousStars && !phaseTwoStarted)
        state.lastStarDeathMs = now;

    // A kill or an Ascend despawns every summon in one go, which is no hole anyone lost.
    bool const despawnedAll = !previousHoles.empty() && state.holes.empty() && state.stars.empty() &&
                              state.constellations.empty() && (previousStars || previousConstellations);
    if (!RaidObs::Active() || despawnedAll)
        return;

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

Player* ElectHandler(Player* bot)
{
    Player* third = GetGroupAssistTank(bot, 1);
    if (InFight(third) && GET_PLAYERBOT_AI(third))
        return third;

    Player* mainTank = GetGroupMainTank(bot);
    Player* offTank = GetGroupAssistTank(bot, 0);
    if (!InFight(mainTank) || !InFight(offTank) || mainTank == offTank)
        return nullptr;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    Player* holder = botAI ? GetAlgalonBossTank(botAI) : nullptr;
    Player* handler = holder == offTank ? mainTank : offTank;
    return GET_PLAYERBOT_AI(handler) ? handler : nullptr;
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
// killing the shortest-lived one keeps the explosions apart.
AlgalonScanUnit const* LowestStar(AlgalonEncounterState const& state)
{
    AlgalonScanUnit const* lowest = nullptr;
    for (AlgalonScanUnit const& star : state.stars)
        if (!lowest || star.healthPct < lowest->healthPct)
            lowest = &star;

    return lowest;
}

char const* StarWindow(Player* bot, AlgalonEncounterState const& state, AlgalonScanUnit const* star, uint32 now,
                       bool& open)
{
    open = star != nullptr;
    if (!star)
        return "none";

    // No hole and a Big Bang coming: the one this star leaves behind is worth more than its damage.
    if (state.urgent)
        return "urgent";

    if (star->healthPct <= ULDUAR_ALGALON_STAR_FINISH_HP_PCT)
        return "finishing";

    open = false;

    if (state.lastStarDeathMs && getMSTimeDiff(state.lastStarDeathMs, now) < ULDUAR_ALGALON_STAR_PACING_GAP_MS)
        return "gap";

    // Unavoidable and raid wide, so the weakest member decides whether it can be paid for.
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (InFight(member) && member->GetHealthPct() < ULDUAR_ALGALON_STAR_PACING_RAID_HP_PCT)
                return "raidhp";
        }
    }

    open = true;
    return "open";
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

// Slot 0 sits on the arc centre and later slots alternate outwards, so an under-filled ring stays
// centred and keeps its spacing.
float ArcSlotAngleOffset(uint8 slotIndex, uint8 slotCount, float arcWidth)
{
    if (slotCount <= 1)
        return 0.0f;

    float const angleStep = arcWidth / static_cast<float>(slotCount - 1);

    if (slotCount % 2 == 1)
    {
        if (slotIndex == 0)
            return 0.0f;

        float const angleOffset = angleStep * static_cast<float>((slotIndex + 1) / 2);
        return slotIndex % 2 == 0 ? -angleOffset : angleOffset;
    }

    float const angleOffset = angleStep / 2.0f + angleStep * static_cast<float>(slotIndex / 2);
    return slotIndex % 2 == 1 ? -angleOffset : angleOffset;
}

// The undisplaced spot: the tank slot for whoever holds him, else the bot's ring slot.
bool RawSlot(Player* bot, Position& slot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return false;

    Player* holder = AlgalonEngaged(botAI) ? GetAlgalonBossTank(botAI) : nullptr;
    bool const tankSlot = holder && IsAlgalonSwapTank(holder) ? bot == holder : PlayerbotAI::IsMainTank(bot);
    if (tankSlot)
    {
        slot = ULDUAR_ALGALON_TANK_SLOT;
        return true;
    }

    if (!AlgalonTakesRingSlot(bot))
        return false;

    AlgalonEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    auto const assignment = state->slotAssignments.find(bot->GetGUID());
    return assignment != state->slotAssignments.end() && TryGetAlgalonSlotPosition(assignment->second, slot);
}

bool ClearOfHoles(AlgalonEncounterState const& state, float x, float y)
{
    for (AlgalonScanUnit const& hole : state.holes)
        if (hole.position.GetExactDist2d(x, y) < ULDUAR_ALGALON_HOLE_EXIT_RADIUS)
            return false;

    return true;
}

// Returns the traced branch; `found` says whether `position` is somewhere to stand.
char const* DeriveSlot(Player* bot, Position& position, bool& found)
{
    found = false;
    Position slot;
    if (!RawSlot(bot, slot))
        return "none";

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    // Test the slot, not the bot: a bot that already dodged passes trivially and walks back under it.
    if (AlgalonCosmicSmashMarkerNear(botAI, slot, ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE))
        return "smash";

    found = true;
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || ClearOfHoles(*state, slot.GetPositionX(), slot.GetPositionY()))
    {
        position = slot;
        return "slot";
    }

    // Holes land wherever a star died, so a slot can end up buried. Step aside from the slot itself,
    // nearest ring first, rather than abandon the formation.
    for (float radius = 2.0f; radius <= ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS; radius += 2.0f)
    {
        for (uint8 step = 0; step < 16; ++step)
        {
            float const angle = static_cast<float>(step) * static_cast<float>(M_PI) / 8.0f;
            float const x = slot.GetPositionX() + std::cos(angle) * radius;
            float const y = slot.GetPositionY() + std::sin(angle) * radius;
            if (!ClearOfHoles(*state, x, y) || !AlgalonSpotInRoom(x, y))
                continue;

            position = ValidateFloorPoint(bot, Position(x, y, slot.GetPositionZ()));
            return "buried";
        }
    }

    found = false;
    return "none";
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

    if (!state->introEndMs && boss->IsInCombat() && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
    {
        state->introEndMs = now;
        state->nextBigBangMs = now + ULDUAR_ALGALON_FIRST_BIG_BANG_MS;
    }

    EnsureAlgalonSlotAssignments(bot, *state);

    if (boss->IsInCombat())
        ScanRoom(bot, boss, *state, now);

    AlgalonPhase const phase = ReadPhase(boss, *state);
    state->phase = phase;
    state->engaged = phase == AlgalonPhase::Intro || phase == AlgalonPhase::One || phase == AlgalonPhase::Two;
    if (!state->engaged)
        return;

    bool const casting = CurrentBigBang(boss) != nullptr;
    if (casting && !state->bigBangCasting)
    {
        ++state->bigBangCount;
        state->nextBigBangMs = now + ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS;
        state->shelterAssignments.clear();
        ElectBigBangSoakers(bot, boss, *state);
    }
    else if (casting)
        KeepBigBangSoaker(bot, *state);
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

    Player* handler = ElectHandler(bot);
    state->handler = handler ? handler->GetGUID() : ObjectGuid::Empty;

    if (phase != AlgalonPhase::One)
    {
        state->focusStar = ObjectGuid::Empty;
        return;
    }

    ElectStarTeam(bot, *state);

    AlgalonScanUnit const* star = LowestStar(*state);
    bool open = false;
    char const* window = StarWindow(bot, *state, star, now, open);
    state->focusStar = open ? star->guid : ObjectGuid::Empty;

    if (RaidObs::Active())
        if (Group* group = bot->GetGroup())
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
    return player && (player == GetGroupMainTank(player) || player == GetGroupAssistTank(player, 0));
}

uint8 GetAlgalonPhasePunchStacks(Player* player)
{
    Aura* aura = player ? player->GetAura(SPELL_ALGALON_PHASE_PUNCH) : nullptr;
    return aura ? aura->GetStackAmount() : 0;
}

Player* GetAlgalonPickupTank(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return nullptr;

    Player* best = nullptr;
    for (Player* tank : {GetGroupMainTank(bot), GetGroupAssistTank(bot, 0)})
    {
        if (!CanStayOut(tank))
            continue;

        if (!best || GetAlgalonPhasePunchStacks(tank) < GetAlgalonPhasePunchStacks(best))
            best = tank;
    }

    return best;
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

bool AlgalonTakesRingSlot(Player* bot)
{
    return bot && PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsMainTank(bot) &&
           !PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
}

bool TryGetAlgalonSlotPosition(uint8 slotIndex, Position& position)
{
    if (slotIndex >= ULDUAR_ALGALON_TOTAL_SLOTS)
        return false;

    float radius = ULDUAR_ALGALON_HEALER_RADIUS;
    float arcCenter = ULDUAR_ALGALON_HEALER_ARC_CENTER;
    float arcWidth = ULDUAR_ALGALON_HEALER_ARC_WIDTH;
    uint8 slotCount = ULDUAR_ALGALON_HEALER_SLOTS;
    uint8 localIndex = slotIndex;

    if (slotIndex >= ULDUAR_ALGALON_HEALER_SLOTS + ULDUAR_ALGALON_RANGED_INNER_SLOTS)
    {
        radius = ULDUAR_ALGALON_RANGED_OUTER_RADIUS;
        arcCenter = ULDUAR_ALGALON_RANGED_OUTER_ARC_CENTER;
        arcWidth = ULDUAR_ALGALON_RANGED_OUTER_ARC_WIDTH;
        slotCount = ULDUAR_ALGALON_RANGED_OUTER_SLOTS;
        localIndex = slotIndex - ULDUAR_ALGALON_HEALER_SLOTS - ULDUAR_ALGALON_RANGED_INNER_SLOTS;
    }
    else if (slotIndex >= ULDUAR_ALGALON_HEALER_SLOTS)
    {
        radius = ULDUAR_ALGALON_RANGED_INNER_RADIUS;
        arcCenter = ULDUAR_ALGALON_RANGED_INNER_ARC_CENTER;
        arcWidth = ULDUAR_ALGALON_RANGED_INNER_ARC_WIDTH;
        slotCount = ULDUAR_ALGALON_RANGED_INNER_SLOTS;
        localIndex = slotIndex - ULDUAR_ALGALON_HEALER_SLOTS;
    }

    float const bearing =
        Position::NormalizeOrientation(arcCenter + ArcSlotAngleOffset(localIndex, slotCount, arcWidth));

    position = Position(ULDUAR_ALGALON_TANK_SLOT.GetPositionX() + std::cos(bearing) * radius,
                        ULDUAR_ALGALON_TANK_SLOT.GetPositionY() + std::sin(bearing) * radius,
                        ULDUAR_ALGALON_TANK_SLOT.GetPositionZ());
    return true;
}

bool TryGetAlgalonSlot(Player* bot, Position& position)
{
    if (!bot)
        return false;

    bool found = false;
    char const* branch = DeriveSlot(bot, position, found);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "algalon.spot", branch);

    return found;
}

bool GetAlgalonSlotAnchor(Player* bot, Position& slot) { return bot && RawSlot(bot, slot); }

void AppendAlgalonTargetExclusions(PlayerbotAI* botAI, GuidSet& exclusions)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AlgalonEncounterState const* state = FindState(bot);
    if (!state || !state->engaged)
        return;

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
