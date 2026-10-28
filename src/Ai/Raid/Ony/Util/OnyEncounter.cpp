/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyEncounter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <list>
#include <string>
#include <unordered_map>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "GroupReference.h"
#include "InstanceScript.h"
#include "MoveSpline.h"
#include "ObjectAccessor.h"
#include "OnyGeometry.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidRedirectThreat.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "ThreatManager.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace OnyHelpers
{
namespace
{
// The bots poll this several times a second during a pull, so a gap this long means combat stopped.
constexpr uint32 STALE_STATE_MS = 15000;
constexpr uint32 EGG_REFRESH_MS = 1000;
constexpr uint32 ADD_REFRESH_MS = 500;
constexpr float ADD_SEARCH_RADIUS = 200.0f;
constexpr uint32 EGG_HAZARD_TTL_MS = 600000;
// A breath is treated as live this long past the last tick she was seen casting it.
constexpr uint32 BREATH_CAST_TAIL_MS = 1500;
constexpr float MAIN_TANK_WHELP_RANGE = 30.0f;
constexpr float CLASS_TAUNT_RANGE = 30.0f;
constexpr float TANK_COOLDOWN_PANIC_PCT = 35.0f;
// Whelps on a player who cannot tank them count as this many yards nearer.
constexpr float LOOSE_WHELP_BONUS = 20.0f;
constexpr float FLANK_FISSURE_STEP_DEG = 10.0f;
constexpr float FLANK_FISSURE_STEP_DEPTH = 3.0f;
constexpr uint8 MAX_SLOT_RINGS = 8;

struct SlotCache
{
    uint32 anchorVersion = 0;
    bool fissures = false;
    Position point;
};

struct OnyxiaState
{
    ObjectGuid bossGuid;
    uint32 lastSeenMs = 0;

    RaidObs::ObsValue<uint8> phase{"onyxia.phase"};
    OnyxiaPhase lastPhase = OnyxiaPhase::None;
    bool landingSeen = false;
    uint32 touchDownMs = 0;

    // Her victim while it is the main tank, and since when.
    ObjectGuid heldBy;
    uint32 heldSinceMs = 0;
    // A bot main tank has built his lead in this hold and may walk her off.
    bool secured = false;

    // The main tank reached the tank spot since she last landed or was pulled.
    bool tankArrived = false;
    uint32 tankWalkMs = 0;

    RaidObs::ObsValue<Position> anchor{"onyxia.anchor"};
    float anchorFacing = 0.0f;
    bool anchorSet = false;
    uint32 anchorVersion = 0;

    RaidObs::ObsGuidMap<uint8> threat{"onyxia.threat"};
    RaidObs::ObsGuidMap<uint8> slot{"onyxia.slot"};
    bool slotsAssigned = false;
    std::unordered_map<ObjectGuid, SlotCache> slotCache;

    RaidObs::ObsValue<uint8> breath{"onyxia.breath"};
    uint32 breathStartMs = 0;
    uint32 breathSeenMs = 0;
    RaidObs::ObsValue<ObjectGuid> fireball{"onyxia.fireball"};
    RaidObs::ObsValue<bool> lust{"onyxia.lust"};

    bool airBuilt = false;
    std::array<std::vector<Position>, 8> airCells;
    // She has reached her first hover point, so her spline's destination names the next waypoint.
    bool reachedHover = false;
    uint8 airWaypoint = 0;
    std::unordered_map<ObjectGuid, size_t> airSpot;

    std::vector<ObjectGuid> whelps;
    std::vector<ObjectGuid> guards;
    uint32 addsRefreshedMs = 0;

    std::unordered_map<ObjectGuid, Position> dodgeDestination;
    std::unordered_map<ObjectGuid, int8> flankSide;
    std::unordered_map<ObjectGuid, Position> flankIssued;

    bool eggsTraced = false;
    std::unordered_map<ObjectGuid, bool> novaTraced;
};

// Kept apart from the pull's state, which resets with combat: the eggs are there before the pull
// and after the wipe.
struct EggCache
{
    std::vector<Position> live;
    std::vector<Position> known;
    uint32 refreshedMs = 0;
    RaidObs::ObsValue<uint32> count{"onyxia.egg"};
};

RaidInstanceState<OnyxiaState> onyxiaStates;
RaidInstanceState<EggCache> eggCaches;

template <typename T>
struct TickCache
{
    PlayerbotAI* owner = nullptr;
    uint32 atMs = 0;
    T value{};

    bool Fresh(PlayerbotAI* botAI, uint32 now) const { return atMs && owner == botAI && atMs == now; }

    T const& Store(PlayerbotAI* botAI, uint32 now, T result)
    {
        owner = botAI;
        atMs = now;
        value = result;
        return value;
    }
};

Creature* GetOnyxia(Player* bot)
{
    if (!bot || bot->GetMapId() != ONYXIA_MAP_ID)
        return nullptr;

    InstanceScript* instance = bot->GetInstanceScript();
    Creature* boss = instance ? instance->GetCreature(ONYXIA_BOSS_ONYXIA) : nullptr;
    return boss && boss->IsAlive() && boss->GetEntry() == NpcId::Onyxia ? boss : nullptr;
}

OnyxiaState& StateFor(Creature* boss)
{
    OnyxiaState& state = onyxiaStates.For(boss->GetInstanceId());
    uint32 const now = getMSTime();

    // Member-wise, so the traced latches snap back without writing a note: a reset is not an
    // assignment, and this runs on every read while she is out of combat.
    if (state.bossGuid != boss->GetGUID() || !boss->IsInCombat() ||
        (state.lastSeenMs && getMSTimeDiff(state.lastSeenMs, now) > STALE_STATE_MS))
    {
        state = OnyxiaState();
        state.bossGuid = boss->GetGUID();
    }

    state.lastSeenMs = now;
    return state;
}

OnyxiaPhase DerivePhase(Creature* boss, OnyxiaState const& state)
{
    if (!boss->IsInCombat())
        return OnyxiaPhase::None;

    bool const low = boss->GetHealthPct() <= ONYXIA_PHASE_3_HEALTH_PCT;
    if (boss->HasReactState(REACT_PASSIVE))
    {
        if (low)
            return OnyxiaPhase::Landing;

        bool const airborne = boss->IsFlying() || boss->GetPositionZ() > ONYXIA_AIR_Z;
        return airborne ? OnyxiaPhase::Air : OnyxiaPhase::Liftoff;
    }

    return state.landingSeen || low ? OnyxiaPhase::Final : OnyxiaPhase::Ground;
}

template <typename Visit>
void ForEachMember(Player* bot, Visit&& visit)
{
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group)
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsInWorld() && member->GetMapId() == bot->GetMapId())
            visit(member);
    }
}

bool IsLayoutBot(Player* member)
{
    return !IsRealPlayer(member) && !PlayerbotAI::IsTank(member) &&
           (PlayerbotAI::IsHeal(member) || PlayerbotAI::IsRanged(member));
}

std::vector<Player*> LayoutBots(Player* bot)
{
    std::vector<Player*> members;
    ForEachMember(bot,
                  [&](Player* member)
                  {
                      if (IsLayoutBot(member))
                          members.push_back(member);
                  });

    std::sort(members.begin(), members.end(),
              [](Player const* a, Player const* b) { return a->GetGUID() < b->GetGUID(); });
    return members;
}

EggCache* EggCacheFor(Player* bot)
{
    return bot && bot->GetInstanceId() ? &eggCaches.For(bot->GetInstanceId()) : nullptr;
}

void RefreshEggs(Player* bot, uint32 now)
{
    EggCache* cache = EggCacheFor(bot);
    if (!cache || (cache->refreshedMs && getMSTimeDiff(cache->refreshedMs, now) < EGG_REFRESH_MS))
        return;

    cache->refreshedMs = now;

    std::list<GameObject*> eggs;
    bot->GetGameObjectListWithEntryInGrid(eggs, GoId::Egg, ONYXIA_EGG_SEARCH_RADIUS);

    cache->live.clear();
    cache->known.clear();
    for (GameObject* egg : eggs)
    {
        if (!egg)
            continue;

        cache->known.push_back(egg->GetPosition());
        if (egg->isSpawned())
            cache->live.push_back(egg->GetPosition());
    }

    cache->count = static_cast<uint32>(cache->live.size());
}

void RefreshAdds(Player* bot, OnyxiaState& state, uint32 now)
{
    if (state.addsRefreshedMs && getMSTimeDiff(state.addsRefreshedMs, now) < ADD_REFRESH_MS)
        return;

    state.addsRefreshedMs = now;
    state.whelps.clear();
    state.guards.clear();

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, std::vector<uint32>{NpcId::Whelp, NpcId::LairGuard}, ADD_SEARCH_RADIUS);
    for (Creature* add : found)
    {
        if (!add || !add->IsAlive())
            continue;

        if (add->GetEntry() == NpcId::Whelp)
            state.whelps.push_back(add->GetGUID());
        else if (add->IsInCombat() && add->GetPositionX() > ONYXIA_TUNNEL_GUARD_MAX_X)
            state.guards.push_back(add->GetGUID());
    }
}

bool EggDanger(Player* bot)
{
    for (Position const& egg : LiveEggs(bot))
        if (bot->GetExactDist(&egg) <= ONYXIA_EGG_DANGER)
            return true;

    return false;
}

bool LaneLive(OnyxiaState const& state, uint32 now)
{
    if (!state.breath.Get())
        return false;

    return getMSTimeDiff(state.breathStartMs, now) <= ONYXIA_BREATH_LIVE_MS ||
           getMSTimeDiff(state.breathSeenMs, now) <= BREATH_CAST_TAIL_MS;
}

Spell* CastingBlastNova(Creature* guard)
{
    Spell* spell = guard ? guard->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
    return spell && spell->m_spellInfo && spell->m_spellInfo->Id == SpellId::BlastNova ? spell : nullptr;
}

bool NovaThreatens(Player* bot, OnyxiaState const& state)
{
    for (ObjectGuid const& guid : state.guards)
    {
        Creature* guard = ObjectAccessor::GetCreature(*bot, guid);
        Spell* nova = CastingBlastNova(guard);
        if (nova && nova->GetCastTimeRemaining() <= static_cast<int32>(ONYXIA_BLAST_NOVA_LEAVE_MS) &&
            bot->GetExactDist2d(guard) <= ONYXIA_BLAST_NOVA_CLEARANCE)
            return true;
    }

    return false;
}

Unit* FireballTarget(Creature* boss)
{
    Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell || !spell->m_spellInfo || !IsFireballSpell(spell->m_spellInfo->Id))
        return nullptr;

    return spell->m_targets.GetUnitTarget();
}

std::string DescribeReasons(uint8 reasons)
{
    if (!reasons)
        return "none";

    std::string text;
    auto add = [&](uint8 bit, char const* name)
    {
        if (!(reasons & bit))
            return;

        if (!text.empty())
            text += "+";

        text += name;
    };

    add(DodgeReason::Egg, "egg");
    add(DodgeReason::Breath, "breath");
    add(DodgeReason::Nova, "nova");
    add(DodgeReason::Fireball, "fireball");
    return text;
}

char const* DescribeBranch(TankBranch branch)
{
    switch (branch)
    {
        case TankBranch::Pickup:
            return "pickup";
        case TankBranch::Taunt:
            return "taunt";
        case TankBranch::Recover:
            return "recover";
        case TankBranch::Secure:
            return "secure";
        case TankBranch::Drag:
            return "drag";
        case TankBranch::Return:
            return "return";
        case TankBranch::Hold:
            return "hold";
        default:
            return "none";
    }
}

TankBranch ComputeTankBranch(PlayerbotAI* botAI, OnyxiaSnapshot const& snap)
{
    Player* bot = botAI->GetBot();
    if (!snap.boss || EffectiveMainTank(bot, snap.boss) != bot)
        return TankBranch::None;

    if (snap.phase == OnyxiaPhase::Landing)
        return TankBranch::Pickup;

    if (!IsGrounded(snap.phase))
        return TankBranch::None;

    if (snap.boss->GetVictim() != bot)
    {
        if (ClassTauntReady(botAI) && bot->GetDistance(snap.boss) <= CLASS_TAUNT_RANGE)
            return TankBranch::Taunt;

        return TankBranch::Recover;
    }

    if (bot->GetExactDist2d(&ONYXIA_TANK_SPOT) <= ONYXIA_TANK_SPOT_TOLERANCE)
        return TankBranch::Hold;

    OnyxiaState const& state = StateFor(snap.boss);
    if (!state.secured)
        return TankBranch::Secure;

    return state.tankArrived ? TankBranch::Return : TankBranch::Drag;
}

// His threat over the highest anyone else has on her list, pets included.
float ThreatLead(Creature* boss, Unit const* tank)
{
    float own = 0.0f;
    float next = 0.0f;
    for (ThreatReference const* ref : boss->GetThreatMgr().GetUnsortedThreatList())
    {
        if (ref->GetVictim() == tank)
            own = ref->GetThreat();
        else
            next = std::max(next, ref->GetThreat());
    }

    return next > 0.0f ? own / next : std::numeric_limits<float>::max();
}

uint8 ComputeThreatLatch(Player* bot, Creature* boss, OnyxiaPhase phase, Player* mainTank, bool secured,
                         uint8 previous)
{
    if (PlayerbotAI::IsHeal(bot) || mainTank == bot)
        return 0;

    if (phase == OnyxiaPhase::Landing)
        return 3;

    if (!IsGrounded(phase) || !mainTank)
        return 0;

    if (bot->HasAura(SPELL_MISDIRECTION_PROC) || bot->HasAura(SpellId::TricksOfTheTrade) ||
        bot->HasAura(SpellId::TricksOfTheTradeActive))
        return 0;

    if (boss->GetVictim() != mainTank || MainTankHeldMs(bot) < ONYXIA_THREAT_DWELL_MS)
        return 1;

    // A bot tank walks her off once he has his lead, and damage before that is what takes her off him
    // on the way. A human tank only gets the dwell.
    if (!secured && !IsRealPlayer(mainTank))
        return 1;

    // Straight off the threat manager: the "threat" AI value is a uint8 percentage and wraps past 255.
    ThreatManager& threat = boss->GetThreatMgr();
    float const tankThreat = threat.GetThreat(mainTank);
    if (tankThreat <= 0.0f)
        return 1;

    float const ratio = threat.GetThreat(bot) / tankThreat;
    if (ratio >= ONYXIA_THREAT_HOLD_RATIO || (previous == 2 && ratio > ONYXIA_THREAT_RELEASE_RATIO))
        return 2;

    return 0;
}

void UpdatePhase(OnyxiaState& state, OnyxiaPhase phase, uint32 now)
{
    state.phase = static_cast<uint8>(phase);

    if (phase == OnyxiaPhase::Landing)
        state.landingSeen = true;

    if (phase == OnyxiaPhase::Final && !state.touchDownMs)
        state.touchDownMs = now ? now : 1;

    if (phase == state.lastPhase)
        return;

    if (phase == OnyxiaPhase::Liftoff || phase == OnyxiaPhase::Landing)
        state.tankArrived = false;

    state.dodgeDestination.clear();
    state.lastPhase = phase;
}

// Her threat is wiped on the way up and on the way down, so a hold only counts on the ground.
void UpdateHold(OnyxiaState& state, Creature* boss, OnyxiaPhase phase, Player* mainTank, uint32 now)
{
    if (mainTank && IsGrounded(phase) && boss->GetVictim() == mainTank)
    {
        if (state.heldBy != mainTank->GetGUID())
        {
            state.heldBy = mainTank->GetGUID();
            state.heldSinceMs = now ? now : 1;
            state.secured = false;
        }
        return;
    }

    state.heldBy.Clear();
    state.heldSinceMs = 0;
    state.secured = false;
}

// Latched for the rest of the hold: a lead that dips on the walk doesn't stop him halfway.
void UpdateSecured(OnyxiaState& state, Creature* boss, Player* mainTank, uint32 now)
{
    if (state.secured || !mainTank || IsRealPlayer(mainTank) || !state.heldSinceMs ||
        state.heldBy != mainTank->GetGUID())
        return;

    uint32 const held = getMSTimeDiff(state.heldSinceMs, now);
    if (held < ONYXIA_SECURE_DWELL_MS)
        return;

    if (held >= ONYXIA_SECURE_MAX_MS || ThreatLead(boss, mainTank) >= ONYXIA_SECURE_LEAD)
        state.secured = true;
}

void NoteLane(Player* bot, BreathLane const& lane)
{
    if (!RaidObs::Active())
        return;

    char params[80];
    snprintf(params, sizeof(params), "\"ex\":%.1f,\"ey\":%.1f,\"half\":%.0f", lane.to.GetPositionX(),
             lane.to.GetPositionY(), ONYXIA_BREATH_TRIGGER);
    RaidObs::NoteHazard(bot->GetMap(), lane.spellId, lane.from, "lane", params, ONYXIA_BREATH_LIVE_MS);
}

BreathLane const* BreathBeingCast(Creature* boss)
{
    for (CurrentSpellTypes type : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
    {
        Spell* spell = boss->GetCurrentSpell(type);
        if (spell && spell->m_spellInfo)
            if (BreathLane const* lane = BreathLaneForSpell(spell->m_spellInfo->Id))
                return lane;
    }

    for (BreathLane const& lane : BREATH_LANES)
        if (boss->HasAura(lane.spellId))
            return &lane;

    return nullptr;
}

void UpdateBreath(Player* bot, Creature* boss, OnyxiaState& state, uint32 now)
{
    BreathLane const* lane = BreathBeingCast(boss);
    if (lane)
    {
        if (state.breath.Get() != lane->waypoint || !LaneLive(state, now))
        {
            state.breathStartMs = now;
            state.breath = lane->waypoint;
            NoteLane(bot, *lane);
        }

        state.breathSeenMs = now;
        return;
    }

    if (state.breath.Get() && !LaneLive(state, now))
        state.breath = 0;
}

void UpdateAnchor(OnyxiaState& state, Creature* boss, OnyxiaPhase phase, Player* mainTank)
{
    if (!IsGroundLayout(phase))
        return;

    Position wanted;
    float facing = 0.0f;

    // A bot tank walks her to the hold spot, so the raid sets up there before she arrives. A human
    // tank goes wherever he likes, so the layout follows her instead.
    if (mainTank && !IsRealPlayer(mainTank))
        wanted = ONYXIA_BOSS_HOLD_SPOT;
    else
    {
        wanted = boss->GetPosition();
        facing = boss->GetOrientation();
        if (state.anchorSet)
        {
            float turn = std::fabs(facing - state.anchorFacing);
            if (turn > static_cast<float>(M_PI))
                turn = 2.0f * static_cast<float>(M_PI) - turn;

            if (state.anchor.Get().GetExactDist2d(&wanted) < ONYXIA_ANCHOR_MOVE &&
                turn * 180.0f / static_cast<float>(M_PI) < ONYXIA_ANCHOR_TURN_DEG)
                return;
        }
    }

    Position const& current = state.anchor.Get();
    if (state.anchorSet && current.GetPositionX() == wanted.GetPositionX() &&
        current.GetPositionY() == wanted.GetPositionY() && state.anchorFacing == facing)
        return;

    state.anchor = Position(wanted.GetPositionX(), wanted.GetPositionY(), wanted.GetPositionZ());
    state.anchorFacing = facing;
    state.anchorSet = true;
    ++state.anchorVersion;
}

// Once per pull, by GUID so every bot agrees. Healers fill the inner ring and spill into the
// casters', hunters keep the 36yd one, and whoever is left goes further out.
void AssignGroundSlots(OnyxiaState& state, Player* bot)
{
    if (state.slotsAssigned)
        return;

    std::array<uint8, MAX_SLOT_RINGS> used{};
    auto place = [&](Player* member, std::initializer_list<uint8> rings)
    {
        auto take = [&](uint8 ring)
        {
            if (used[ring] >= ONYXIA_SLOTS_PER_RING)
                return false;

            state.slot[member->GetGUID()] = static_cast<uint8>(ring * ONYXIA_SLOTS_PER_RING + used[ring]++);
            return true;
        };

        for (uint8 ring : rings)
            if (take(ring))
                return;

        for (uint8 ring = 3; ring < MAX_SLOT_RINGS; ++ring)
            if (take(ring))
                return;
    };

    std::vector<Player*> const members = LayoutBots(bot);
    for (Player* member : members)
        if (PlayerbotAI::IsHeal(member))
            place(member, {0, 1});

    for (Player* member : members)
        if (!PlayerbotAI::IsHeal(member) && member->getClass() == CLASS_HUNTER)
            place(member, {2});

    for (Player* member : members)
        if (!PlayerbotAI::IsHeal(member) && member->getClass() != CLASS_HUNTER)
            place(member, {1});

    state.slotsAssigned = true;
}

void BuildAirCellsOnce(OnyxiaState& state, Player* bot)
{
    if (state.airBuilt)
        return;

    EggCache* cache = EggCacheFor(bot);
    std::vector<Position> const eggs = cache ? cache->known : std::vector<Position>();
    for (BreathLane const& lane : BREATH_LANES)
        state.airCells[lane.waypoint - 1] = BuildAirCells(bot, lane, eggs);

    state.airBuilt = true;
}

// Nearest free cell per bot, in GUID order. A bot left without one shares the nearest.
void AssignAirSpots(OnyxiaState& state, Player* bot, uint8 waypoint)
{
    state.airSpot.clear();
    std::vector<Position> const& cells = state.airCells[waypoint - 1];
    if (cells.empty())
        return;

    std::vector<bool> taken(cells.size(), false);
    for (Player* member : LayoutBots(bot))
    {
        size_t best = cells.size();
        float bestDistance = std::numeric_limits<float>::max();
        for (bool allowTaken : {false, true})
        {
            for (size_t i = 0; i < cells.size(); ++i)
            {
                if (taken[i] && !allowTaken)
                    continue;

                float const distance = member->GetExactDist2d(&cells[i]);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = i;
                }
            }

            if (best < cells.size())
                break;
        }

        taken[best] = true;
        state.airSpot[member->GetGUID()] = best;
    }
}

void UpdateAirWaypoint(OnyxiaState& state, Player* bot, Creature* boss, OnyxiaPhase phase)
{
    uint8 waypoint = ONYXIA_FIRST_HOVER_WAYPOINT;
    if (phase == OnyxiaPhase::Air)
    {
        bool const settled = boss->movespline->Finalized();
        if (!state.reachedHover && settled)
        {
            BreathLane const& nearest = BreathLaneForWaypoint(NearestWaypoint(boss->GetPositionX(), boss->GetPositionY()));
            state.reachedHover = nearest.hover.GetExactDist2d(boss) <= ONYXIA_AIR_WAYPOINT_SNAP;
        }

        // Until then her spline still points at the takeoff, which is not where she breathes from.
        if (state.reachedHover)
        {
            if (settled)
                waypoint = NearestWaypoint(boss->GetPositionX(), boss->GetPositionY());
            else
            {
                G3D::Vector3 const destination = boss->movespline->FinalDestination();
                waypoint = NearestWaypoint(destination.x, destination.y);
            }
        }
    }

    if (waypoint == state.airWaypoint)
        return;

    state.airWaypoint = waypoint;
    AssignAirSpots(state, bot, waypoint);
}

char const* AirRoleOf(Player* bot, Player* mainTank)
{
    if (mainTank == bot)
        return "guard";

    if (PlayerbotAI::IsTank(bot))
        return "whelps";

    if (PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsRangedDps(bot))
        return "spot";

    return "guard";
}

void TraceHazards(Player* bot, OnyxiaState& state)
{
    if (!state.eggsTraced)
    {
        for (Position const& egg : LiveEggs(bot))
            RaidObs::NoteHazardCircle(bot->GetMap(), SpellId::EggTrap, egg, ONYXIA_EGG_DANGER, EGG_HAZARD_TTL_MS);

        state.eggsTraced = true;
    }

    for (ObjectGuid const& guid : state.guards)
    {
        Creature* guard = ObjectAccessor::GetCreature(*bot, guid);
        Spell* nova = CastingBlastNova(guard);
        bool& traced = state.novaTraced[guid];
        if (!nova)
        {
            traced = false;
            continue;
        }

        if (traced)
            continue;

        traced = true;
        RaidObs::NoteHazardCircle(bot->GetMap(), SpellId::BlastNova, guard->GetPosition(), 15.0f,
                                  static_cast<uint32>(std::max(0, nova->GetCastTimeRemaining())));
    }
}

Unit* LiveLairGuard(Player* bot, OnyxiaState const& state)
{
    Unit* nearest = nullptr;
    float best = std::numeric_limits<float>::max();
    for (ObjectGuid const& guid : state.guards)
    {
        Creature* guard = ObjectAccessor::GetCreature(*bot, guid);
        if (!guard || !guard->IsAlive())
            continue;

        float const distance = bot->GetExactDist2d(guard);
        if (distance < best)
        {
            best = distance;
            nearest = guard;
        }
    }

    return nearest;
}

bool WhelpClearOfEggs(Player* bot, Unit const* whelp)
{
    return !NearLiveEgg2d(bot, whelp->GetPositionX(), whelp->GetPositionY(), ONYXIA_WHELP_EGG_EXCLUSION);
}

// A whelp nobody has to walk into the egg pit for. Loose whelps, on a player who cannot tank them,
// go first.
Unit* EggClearWhelp(Player* bot, OnyxiaState const& state, bool sticky, float maxRange)
{
    Unit* victim = bot->GetVictim();
    if (sticky && victim && victim->IsAlive() && victim->GetEntry() == NpcId::Whelp && WhelpClearOfEggs(bot, victim))
        return victim;

    Unit* best = nullptr;
    float bestScore = std::numeric_limits<float>::max();
    for (ObjectGuid const& guid : state.whelps)
    {
        Creature* whelp = ObjectAccessor::GetCreature(*bot, guid);
        if (!whelp || !whelp->IsAlive() || !WhelpClearOfEggs(bot, whelp))
            continue;

        float const distance = bot->GetExactDist2d(whelp);
        if (maxRange > 0.0f && distance > maxRange)
            continue;

        Unit* whelpVictim = whelp->GetVictim();
        Player* victimPlayer = whelpVictim ? whelpVictim->ToPlayer() : nullptr;
        float const score =
            distance - (victimPlayer && !PlayerbotAI::IsTank(victimPlayer) ? LOOSE_WHELP_BONUS : 0.0f);
        if (score < bestScore)
        {
            bestScore = score;
            best = whelp;
        }
    }

    return best;
}

Unit* PickTarget(PlayerbotAI* botAI, OnyxiaSnapshot const& snap)
{
    Player* bot = botAI->GetBot();
    Creature* boss = snap.boss;
    if (!boss || snap.phase == OnyxiaPhase::None)
        return nullptr;

    bool const tank = PlayerbotAI::IsTank(bot);
    if (!tank && PlayerbotAI::IsHeal(bot))
        return nullptr;

    OnyxiaState const& state = StateFor(boss);
    Player* mainTank = EffectiveMainTank(bot, boss);
    bool const isMainTank = mainTank == bot;

    if (!IsAirborne(snap.phase))
    {
        if (snap.phase == OnyxiaPhase::Landing || isMainTank || !tank)
            return boss;

        if (Unit* whelp = EggClearWhelp(bot, state, true, 0.0f))
            return whelp;

        return boss;
    }

    if (!tank && PlayerbotAI::IsRangedDps(bot))
        return boss;

    if (isMainTank)
    {
        if (Unit* guard = LiveLairGuard(bot, state))
            return guard;

        if (Unit* whelp = EggClearWhelp(bot, state, false, MAIN_TANK_WHELP_RANGE))
            return whelp;

        return boss;
    }

    if (tank)
    {
        if (Unit* whelp = EggClearWhelp(bot, state, true, 0.0f))
            return whelp;

        if (!mainTank)
            if (Unit* guard = LiveLairGuard(bot, state))
                return guard;

        return boss;
    }

    if (Unit* guard = LiveLairGuard(bot, state))
        return guard;

    if (Unit* whelp = EggClearWhelp(bot, state, true, 0.0f))
        return whelp;

    return boss;
}

bool SpotClear(Position const& spot, std::vector<HazardCircle> const& hazards)
{
    for (HazardCircle const& hazard : hazards)
        if (spot.GetExactDist2d(&hazard.first) < hazard.second)
            return false;

    return true;
}
}  // namespace

OnyxiaSnapshot const& OnyxiaSnapshotFor(PlayerbotAI* botAI)
{
    thread_local TickCache<OnyxiaSnapshot> cache;
    uint32 const now = getMSTime();
    if (cache.Fresh(botAI, now))
        return cache.value;

    OnyxiaSnapshot snap;
    Creature* boss = GetOnyxia(botAI->GetBot());
    if (boss && boss->IsInCombat())
    {
        snap.boss = boss;
        snap.phase = DerivePhase(boss, StateFor(boss));
    }

    return cache.Store(botAI, now, snap);
}

Player* EffectiveMainTank(Player* bot, Unit* boss)
{
    if (Player* mainTank = GetGroupMainTank(bot))
        return mainTank;

    if (!boss)
        return nullptr;

    Player* best = nullptr;
    float bestThreat = -1.0f;
    ForEachMember(bot,
                  [&](Player* member)
                  {
                      if (!member->IsAlive() || !PlayerbotAI::IsTank(member))
                          return;

                      float const threat = boss->GetThreatMgr().GetThreat(member);
                      if (threat > bestThreat)
                      {
                          bestThreat = threat;
                          best = member;
                      }
                  });

    return best;
}

bool MainTankIsBot(Player* bot, Unit* boss)
{
    Player* mainTank = EffectiveMainTank(bot, boss);
    return mainTank && !IsRealPlayer(mainTank);
}

uint32 MainTankHeldMs(Player* bot)
{
    Creature* boss = GetOnyxia(bot);
    if (!boss || !boss->IsInCombat())
        return 0;

    OnyxiaState const& state = StateFor(boss);
    Player* mainTank = EffectiveMainTank(bot, boss);
    if (!mainTank || boss->GetVictim() != mainTank || state.heldBy != mainTank->GetGUID() || !state.heldSinceMs)
        return 0;

    return getMSTimeDiff(state.heldSinceMs, getMSTime());
}

TankBranch MainTankBranch(PlayerbotAI* botAI)
{
    thread_local TickCache<TankBranch> cache;
    uint32 const now = getMSTime();
    if (cache.Fresh(botAI, now))
        return cache.value;

    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    TankBranch const branch = ComputeTankBranch(botAI, snap);

    Player* bot = botAI->GetBot();
    if (RaidObs::Active() && snap.boss && EffectiveMainTank(bot, snap.boss) == bot)
        RaidObs::NoteDerived(bot, "onyxia.tank", DescribeBranch(branch));

    return cache.Store(botAI, now, branch);
}

bool MainTankOwnsMovement(PlayerbotAI* botAI)
{
    switch (MainTankBranch(botAI))
    {
        case TankBranch::Pickup:
        case TankBranch::Drag:
        case TankBranch::Return:
        case TankBranch::Hold:
            return true;
        default:
            return false;
    }
}

Position const* MainTankDestination(TankBranch branch)
{
    switch (branch)
    {
        case TankBranch::Pickup:
            return &ONYXIA_P3_PICKUP;
        case TankBranch::Drag:
        case TankBranch::Return:
        case TankBranch::Hold:
            return &ONYXIA_TANK_SPOT;
        default:
            return nullptr;
    }
}

bool MainTankWalkDue(Player* bot)
{
    Creature* boss = GetOnyxia(bot);
    if (!boss)
        return false;

    OnyxiaState const& state = StateFor(boss);
    if (!state.tankWalkMs)
        return true;

    // A walk still under way is left alone, so the tank's own threat buttons get the ticks between.
    uint32 const elapsed = getMSTimeDiff(state.tankWalkMs, getMSTime());
    return elapsed >= 4 * ONYXIA_TANK_WALK_REISSUE_MS || (elapsed >= ONYXIA_TANK_WALK_REISSUE_MS && !bot->isMoving());
}

void MarkMainTankWalk(Player* bot)
{
    if (Creature* boss = GetOnyxia(bot))
    {
        uint32 const now = getMSTime();
        StateFor(boss).tankWalkMs = now ? now : 1;
    }
}

uint8 ThreatHoldLatch(Player* bot)
{
    // Asked for nearly every action the bot weighs.
    thread_local Player* cachedFor = nullptr;
    thread_local uint32 cachedAtMs = 0;
    thread_local uint8 cached = 0;
    uint32 const now = getMSTime();
    if (cachedAtMs && cachedFor == bot && cachedAtMs == now)
        return cached;

    cachedFor = bot;
    cachedAtMs = now;
    cached = 0;

    Creature* boss = GetOnyxia(bot);
    if (!boss || !boss->IsInCombat())
        return cached;

    OnyxiaState const& state = StateFor(boss);
    auto const it = state.threat.find(bot->GetGUID());
    cached = it == state.threat.end() ? 0 : it->second;
    return cached;
}

bool BurstAllowed(PlayerbotAI* botAI, bool lust)
{
    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    if (!snap.boss)
        return true;

    if (snap.phase == OnyxiaPhase::Landing)
        return false;

    if (IsAirborne(snap.phase))
        return !lust;

    if (snap.phase == OnyxiaPhase::Ground && lust)
        return false;

    if (!IsGrounded(snap.phase))
        return true;

    Player* bot = botAI->GetBot();
    if (!EffectiveMainTank(bot, snap.boss))
        return true;

    if (MainTankHeldMs(bot) < ONYXIA_BURST_DWELL_MS)
        return false;

    if (snap.phase == OnyxiaPhase::Final && !MainTankIsBot(bot, snap.boss))
    {
        OnyxiaState const& state = StateFor(snap.boss);
        return state.touchDownMs && getMSTimeDiff(state.touchDownMs, getMSTime()) >= ONYXIA_HUMAN_TANK_LUST_MS;
    }

    return true;
}

Player* RedirectTankFor(PlayerbotAI* botAI)
{
    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    if (snap.phase != OnyxiaPhase::Final)
        return nullptr;

    OnyxiaState const& state = StateFor(snap.boss);
    if (!state.touchDownMs || getMSTimeDiff(state.touchDownMs, getMSTime()) > ONYXIA_REDIRECT_WINDOW_MS)
        return nullptr;

    Player* bot = botAI->GetBot();
    Player* mainTank = EffectiveMainTank(bot, snap.boss);
    return mainTank && mainTank != bot ? mainTank : nullptr;
}

bool FearWindowActive(PlayerbotAI* botAI)
{
    OnyxiaPhase const phase = OnyxiaSnapshotFor(botAI).phase;
    return phase == OnyxiaPhase::Landing || phase == OnyxiaPhase::Final;
}

std::vector<Position> const& LiveEggs(Player* bot)
{
    static std::vector<Position> const none;
    EggCache* cache = EggCacheFor(bot);
    return cache ? cache->live : none;
}

bool NearLiveEgg2d(Player* bot, float x, float y, float radius)
{
    for (Position const& egg : LiveEggs(bot))
        if (egg.GetExactDist2d(x, y) < radius)
            return true;

    return false;
}

Unit* OnyxiaPickTarget(PlayerbotAI* botAI)
{
    thread_local TickCache<Unit*> cache;
    uint32 const now = getMSTime();
    if (cache.Fresh(botAI, now))
        return cache.value;

    return cache.Store(botAI, now, PickTarget(botAI, OnyxiaSnapshotFor(botAI)));
}

bool GroundSlotFor(PlayerbotAI* botAI, Position& slot)
{
    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    Player* bot = botAI->GetBot();
    if (!snap.boss || !IsGroundLayout(snap.phase) || !IsLayoutBot(bot))
        return false;

    OnyxiaState& state = StateFor(snap.boss);
    auto const assigned = state.slot.find(bot->GetGUID());
    if (!state.anchorSet || assigned == state.slot.end())
        return false;

    // Eruption only bursts in phase 3, and she is on her way down for it.
    bool const fissures = snap.phase != OnyxiaPhase::Ground;
    SlotCache& cache = state.slotCache[bot->GetGUID()];
    if (cache.anchorVersion == state.anchorVersion && cache.fissures == fissures)
    {
        slot = cache.point;
        return true;
    }

    // Radius and bearing nudges, tried in order when the slot itself is off the floor, by an egg or,
    // in phase 3, on a fissure.
    static constexpr std::array<std::pair<float, float>, 7> nudges = {
        {{0.0f, 0.0f}, {-3.0f, 0.0f}, {3.0f, 0.0f}, {0.0f, 6.0f}, {0.0f, -6.0f}, {-3.0f, 6.0f}, {-3.0f, -6.0f}}};

    uint8 const index = assigned->second;
    Position const& anchor = state.anchor.Get();
    Position chosen;
    bool found = false;
    for (auto const& [radiusDelta, bearingDelta] : nudges)
    {
        Position const raw = GroundSlotPoint(anchor, state.anchorFacing, index, radiusDelta, bearingDelta);
        Position const floor = ValidateStaticFloorPoint(bot, raw);
        if (floor.GetExactDist2d(&raw) > ONYXIA_SLOT_MAX_CLIP)
            continue;

        if (NearLiveEgg2d(bot, floor.GetPositionX(), floor.GetPositionY(), ONYXIA_EGG_CLEARANCE))
            continue;

        if (fissures && NearestFissureDistance(floor.GetPositionX(), floor.GetPositionY()) < ONYXIA_FISSURE_CLEARANCE)
            continue;

        chosen = floor;
        found = true;
        break;
    }

    if (!found)
        chosen = ValidateStaticFloorPoint(bot, GroundSlotPoint(anchor, state.anchorFacing, index));

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "onyxia.slotfail", found ? "none" : std::to_string(index));

    cache.anchorVersion = state.anchorVersion;
    cache.fissures = fissures;
    cache.point = chosen;
    slot = chosen;
    return true;
}

bool AirSpotFor(PlayerbotAI* botAI, Position& spot)
{
    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    Player* bot = botAI->GetBot();
    if (!snap.boss || !IsAirborne(snap.phase) || !IsLayoutBot(bot))
        return false;

    OnyxiaState const& state = StateFor(snap.boss);
    bool found = false;
    if (state.airBuilt && state.airWaypoint)
    {
        std::vector<Position> const& cells = state.airCells[state.airWaypoint - 1];
        auto const assigned = state.airSpot.find(bot->GetGUID());
        if (assigned != state.airSpot.end() && assigned->second < cells.size())
        {
            spot = cells[assigned->second];
            found = true;
        }
    }

    if (RaidObs::Active())
    {
        RaidObs::NoteDerived(bot, "onyxia.p2spot",
                             found ? std::to_string(state.airWaypoint) + ":" + RaidObs::DescribeDerived(spot)
                                   : std::string("none"));
    }

    return found;
}

bool RearFlankPointFor(PlayerbotAI* botAI, Position& point, bool forTrigger)
{
    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    Player* bot = botAI->GetBot();
    Creature* boss = snap.boss;
    if (!boss || !IsGrounded(snap.phase) || boss->IsFlying())
        return false;

    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsTank(bot) || bot->GetVictim() != boss)
        return false;

    float const facing = boss->GetOrientation();
    float const meleeRange = bot->GetMeleeRange(boss);
    float const bearing = BearingOffFacingDeg(boss->GetPosition(), facing, bot->GetPositionX(), bot->GetPositionY());
    float const magnitude = std::fabs(bearing);
    bool const inBand = magnitude >= ONYXIA_FLANK_MIN_DEG - ONYXIA_FLANK_SLACK_DEG &&
                        magnitude <= ONYXIA_FLANK_MAX_DEG + ONYXIA_FLANK_SLACK_DEG &&
                        bot->GetExactDist2d(boss) <= meleeRange;
    if (inBand)
        return false;

    if (forTrigger)
        return true;

    // The angle stays put for the pull, so melee spread across the band instead of all picking one
    // random spot each tick. The side is the one the bot stands on, so when she turns it goes round
    // on its own side instead of across her nose. Only just past her tail does it keep the old one,
    // so a bot there doesn't dither.
    OnyxiaState& state = StateFor(boss);
    auto const side = state.flankSide.try_emplace(bot->GetGUID(), static_cast<int8>(bearing >= 0.0f ? 1 : -1)).first;
    if (side->second * bearing < 0.0f && magnitude <= 180.0f - ONYXIA_FLANK_SIDE_SWITCH_DEG)
        side->second = static_cast<int8>(-side->second);
    float const baseDeg = ONYXIA_FLANK_MIN_DEG + 5.0f * (bot->GetGUID().GetCounter() % 5);
    float const baseDepth = meleeRange * ONYXIA_FLANK_DEPTH;

    static constexpr std::array<std::pair<float, float>, 5> nudges = {
        {{0.0f, 0.0f}, {-FLANK_FISSURE_STEP_DEG, 0.0f}, {FLANK_FISSURE_STEP_DEG, 0.0f},
         {0.0f, -FLANK_FISSURE_STEP_DEPTH}, {0.0f, FLANK_FISSURE_STEP_DEPTH}}};

    bool const fissures = snap.phase == OnyxiaPhase::Final;
    Position fallback;
    bool haveFallback = false;
    for (auto const& [degDelta, depthDelta] : nudges)
    {
        float const deg = std::clamp(baseDeg + degDelta, ONYXIA_FLANK_MIN_DEG, ONYXIA_FLANK_MAX_DEG);
        float const angle = facing + side->second * deg * static_cast<float>(M_PI) / 180.0f;
        float const depth = baseDepth + depthDelta;
        Position const raw(boss->GetPositionX() + depth * std::cos(angle), boss->GetPositionY() + depth * std::sin(angle),
                           boss->GetPositionZ());
        Position const floor = ValidateStaticFloorPoint(bot, raw);
        if (NearLiveEgg2d(bot, floor.GetPositionX(), floor.GetPositionY(), ONYXIA_EGG_CLEARANCE))
            continue;

        if (!haveFallback)
        {
            fallback = floor;
            haveFallback = true;
        }

        if (!fissures || NearestFissureDistance(floor.GetPositionX(), floor.GetPositionY()) >= ONYXIA_FISSURE_CLEARANCE)
        {
            point = floor;
            return true;
        }
    }

    if (!haveFallback)
        return false;

    point = fallback;
    return true;
}

bool FlankPointMoved(Player* bot, Position const& point)
{
    Creature* boss = GetOnyxia(bot);
    if (!boss)
        return false;

    OnyxiaState const& state = StateFor(boss);
    auto const issued = state.flankIssued.find(bot->GetGUID());
    return issued == state.flankIssued.end() || issued->second.GetExactDist2d(&point) > ONYXIA_FLANK_REAIM;
}

void MarkFlankIssued(Player* bot, Position const& point)
{
    if (Creature* boss = GetOnyxia(bot))
        StateFor(boss).flankIssued[bot->GetGUID()] = point;
}

uint8 DodgeReasons(PlayerbotAI* botAI)
{
    thread_local TickCache<uint8> cache;
    uint32 const now = getMSTime();
    if (cache.Fresh(botAI, now))
        return cache.value;

    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    Player* bot = botAI->GetBot();
    uint8 reasons = 0;
    if (snap.boss && bot->IsAlive())
    {
        OnyxiaState const& state = StateFor(snap.boss);

        if (EggDanger(bot))
            reasons |= DodgeReason::Egg;

        if (LaneLive(state, now))
        {
            BreathLane const& lane = BreathLaneForWaypoint(state.breath.Get());
            if (DistanceToSegment2d(bot->GetPositionX(), bot->GetPositionY(), lane.from, lane.to) <= ONYXIA_BREATH_TRIGGER)
                reasons |= DodgeReason::Breath;
        }

        // A tank keeps the guard where it is.
        if (!PlayerbotAI::IsTank(bot) && NovaThreatens(bot, state))
            reasons |= DodgeReason::Nova;

        if (FireballTarget(snap.boss) == bot && FireballNeighbours(bot))
            reasons |= DodgeReason::Fireball;
    }

    if (RaidObs::Active() && snap.boss)
        RaidObs::NoteDerived(bot, "onyxia.dodge", DescribeReasons(reasons));

    return cache.Store(botAI, now, reasons);
}

bool DodgeDestination(PlayerbotAI* botAI, uint8 reasons, Position& destination)
{
    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    Player* bot = botAI->GetBot();
    if (!snap.boss || !reasons)
        return false;

    OnyxiaState& state = StateFor(snap.boss);
    uint32 const now = getMSTime();

    // Every live hazard, not only the ones that fired: a breath dodge must not end in the egg pit.
    std::vector<HazardCircle> hazards;
    for (Position const& egg : LiveEggs(bot))
        if (bot->GetExactDist2d(&egg) <= ONYXIA_DODGE_MAX_RADIUS + ONYXIA_EGG_CLEARANCE)
            hazards.emplace_back(egg, ONYXIA_EGG_CLEARANCE);

    if (LaneLive(state, now))
        AppendLaneHazards(BreathLaneForWaypoint(state.breath.Get()), ONYXIA_BREATH_CLEARANCE, hazards);

    for (ObjectGuid const& guid : state.guards)
    {
        Creature* guard = ObjectAccessor::GetCreature(*bot, guid);
        if (CastingBlastNova(guard))
            hazards.emplace_back(guard->GetPosition(), ONYXIA_BLAST_NOVA_CLEARANCE);
    }

    if (reasons & DodgeReason::Fireball)
    {
        ForEachMember(bot,
                      [&](Player* member)
                      {
                          if (member != bot && member->IsAlive() &&
                              bot->GetExactDist2d(member) <= ONYXIA_DODGE_MAX_RADIUS + ONYXIA_FIREBALL_SPACING)
                              hazards.emplace_back(member->GetPosition(), ONYXIA_FIREBALL_SPACING);
                      });
    }

    auto const latched = state.dodgeDestination.find(bot->GetGUID());
    if (latched != state.dodgeDestination.end() && SpotClear(latched->second, hazards) &&
        bot->GetExactDist2d(&latched->second) <= ONYXIA_DODGE_MAX_RADIUS)
    {
        destination = latched->second;
        return true;
    }

    HazardSweepCache sweepCache;
    Position const found = FindNearestPositionClearOfHazards(bot, hazards, ONYXIA_DODGE_MAX_RADIUS, 2.0f,
                                                             static_cast<float>(M_PI) / 12.0f, nullptr, {},
                                                             &sweepCache);
    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        state.dodgeDestination.erase(bot->GetGUID());
        return false;
    }

    state.dodgeDestination[bot->GetGUID()] = found;
    destination = found;
    return true;
}

uint32 FireballNeighbours(Player* bot)
{
    uint32 count = 0;
    ForEachMember(bot,
                  [&](Player* member)
                  {
                      if (member != bot && member->IsAlive() && bot->GetExactDist(member) < ONYXIA_FIREBALL_SPACING)
                          ++count;
                  });
    return count;
}

bool BreathLive(Player* bot)
{
    Creature* boss = GetOnyxia(bot);
    return boss && boss->IsInCombat() && LaneLive(StateFor(boss), getMSTime());
}

bool MainTankCooldownWanted(PlayerbotAI* botAI)
{
    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    Player* bot = botAI->GetBot();
    if (!snap.boss || !IsGrounded(snap.phase) || EffectiveMainTank(bot, snap.boss) != bot)
        return false;

    if (bot->GetHealthPct() < TANK_COOLDOWN_PANIC_PCT)
        return true;

    Spell* spell = snap.boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    return spell && spell->m_spellInfo && IsFlameBreathSpell(spell->m_spellInfo->Id);
}

void TickOnyxia(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ONYXIA_MAP_ID || !bot->GetInstanceId())
        return;

    uint32 const now = getMSTime();
    RefreshEggs(bot, now);

    Creature* boss = GetOnyxia(bot);
    if (!boss || !boss->IsInCombat())
        return;

    OnyxiaState& state = StateFor(boss);
    OnyxiaPhase const phase = DerivePhase(boss, state);
    UpdatePhase(state, phase, now);
    RefreshAdds(bot, state, now);

    Player* mainTank = EffectiveMainTank(bot, boss);
    UpdateHold(state, boss, phase, mainTank, now);
    UpdateSecured(state, boss, mainTank, now);
    UpdateBreath(bot, boss, state, now);

    Unit* fireballTarget = FireballTarget(boss);
    state.fireball = fireballTarget ? fireballTarget->GetGUID() : ObjectGuid::Empty;

    UpdateAnchor(state, boss, phase, mainTank);
    if (IsGroundLayout(phase))
        AssignGroundSlots(state, bot);

    if (IsAirborne(phase))
    {
        BuildAirCellsOnce(state, bot);
        UpdateAirWaypoint(state, bot, boss, phase);
    }

    if (bot->IsAlive())
    {
        auto const previous = state.threat.find(bot->GetGUID());
        uint8 const latch = ComputeThreatLatch(bot, boss, phase, mainTank, state.secured,
                                               previous == state.threat.end() ? 0 : previous->second);
        state.threat[bot->GetGUID()] = latch;

        if (mainTank == bot && MainTankBranch(botAI) == TankBranch::Hold)
            state.tankArrived = true;

        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "onyxia.p2role", IsAirborne(phase) ? AirRoleOf(bot, mainTank) : "none");
    }

    state.lust = phase == OnyxiaPhase::Final && BurstAllowed(botAI, true);

    if (RaidObs::Active() && IsMechanicTrackerBot(bot, ONYXIA_MAP_ID))
        TraceHazards(bot, state);
}
}  // namespace OnyHelpers
