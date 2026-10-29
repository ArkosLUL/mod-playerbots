/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Putricide.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Creature.h"
#include "CreatureAI.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "ICCScripts.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDefensive.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidResistAura.h"
#include "RaidTankDefensive.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "Vehicle.h"

using namespace EncounterHelpers;

namespace
{
constexpr float PI_F = static_cast<float>(M_PI);
// CanAIAttack keeps his targets within 80 yd of the room's centre
constexpr float LIVE_RANGE = 80.0f;
constexpr float SCAN_RANGE = 110.0f;
constexpr uint32 SCAN_MS = 200;
constexpr uint32 DUTY_REFRESH_MS = 200;
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;
// Dodge spots stay on the probed floor
constexpr float ROOM_RADIUS = 42.0f;
constexpr float PLAYER_REACH = 1.5f;
constexpr float MELEE_LEAD = 4.0f / 3.0f;

// Stand ids: 1-PP_SLOT_COUNT a ring slot (pp.slot), then the holder's drag spot
constexpr uint8 STAND_TANK = PP_SLOT_COUNT + 1;

// A spot is fouled by a puddle reaching within his own area, where the holder and melee stand, or by a bomb
// within its gas plus his reach of it. A new spot must clear both by SPOT_PICK_MARGIN more.
constexpr float SPOT_BOSS_CLEAR = 9.0f;
constexpr float SPOT_PUDDLE_MARGIN = 2.0f;
constexpr float SPOT_BOMB_CLEAR = 9.0f;
constexpr float SPOT_PICK_MARGIN = 1.0f;
// The holder stands this much inside his melee range past the spot, so he stops on it
constexpr float DRAG_SHORT = 1.5f;
// Held off the spot by anyone but a bot tank, he's out of the rings' reach past this: they let go, and
// hold again once he's back within RING_REHOLD
constexpr float RING_RELEASE = 15.0f;
constexpr float RING_REHOLD = 10.0f;

// A puddle gains a Grow stack every 1.5 s, half a yard of radius: this covers about three seconds of it
constexpr float PUDDLE_GROWTH = 1.0f;
constexpr float SLOT_MARGIN = 1.0f + PP_STAND_PARK;

// The bombs live 10 s and explode for 10 yd on a 10 s period, so the blast counts from 8 s
constexpr uint32 BOMB_LIFETIME_MS = 10000;
constexpr uint32 BLAST_ARMED_MS = 8000;

// Malleable Goo's missile flies 6 yd/s from him to where its target stood. The listener keeps an impact
// until the next cast, 25-30 s on, so a throw across the whole room (90 yd, 15 s) is still there.
constexpr float GOO_SPEED = 6.0f;
constexpr uint32 GOO_GRACE_MS = 400;
constexpr uint32 GOO_RETENTION_MS = 16000;
constexpr uint32 GOO_URGENT_MS = 2000;

// The dodge triggers within half a yard of a hazard's edge and clears it by DODGE_MARGIN: a bot parks up
// to a yard off its spot, and a step that only just clears re-triggers on the next spawn
constexpr float DODGE_TRIGGER = 0.5f;
constexpr float DODGE_MARGIN = 2.5f;
constexpr float DODGE_TIGHT = 0.75f;
constexpr float DODGE_SWEEP_RADIUS = 30.0f;
// Melee keep their dodge spots this far inside his melee range
constexpr float MELEE_INSIDE = 1.5f;

// A Gas Cloud whose victim is down to this many stacks re-picks a random player soon: melee leave it
constexpr uint8 CLOUD_REPICK_STACKS = 2;
constexpr float CLOUD_CLEAR = 15.0f;
// Ranged take an add only within their spell range of their slot: reach is held there
constexpr float RANGED_REACH = 30.0f;
constexpr float HUNTER_REACH = 35.0f;

constexpr float KITE_LEG = 10.0f;
constexpr float KITE_MIN_TRAVEL = 4.0f;
constexpr float KITE_HAZARD_MARGIN = 1.5f;
constexpr float KITE_CONTINUITY = 0.3f;
constexpr int KITE_TURNS_DEG[] = {0, 20, -20, 40, -40, 60, -60, 80, -80, 100, -100, 120, -120};
// A leg still heading this much away from the cloud stays worth walking
constexpr float KITE_KEEP_DOT = -0.2f;

// Stackers stand inside this of the rooted victim. The ooze erupts 5 yd from it and hits 10 yd round itself.
constexpr float STACK_RANGE = 3.5f;
constexpr float STACK_OFFSET = 1.5f;
// A victim carrying Unbound Plague is stacked on from past its jump, on the side the ooze comes from, which
// still puts the stack inside the eruption
constexpr float PLAGUE_STACK = PP_CARRIER_CLEAR + 1.5f;
constexpr float PLAGUE_STACK_PARK = 1.5f;

// A carrier passes from its 8th tick, or under half health, once its searcher has ticked twice
constexpr uint32 PASS_TICK = 8;
constexpr uint32 RECEIVER_ELECT_TICK = 6;
constexpr float PASS_HEALTH_PCT = 50.0f;
constexpr uint32 SEARCHER_TICKS = 2;
constexpr int32 PASS_MIN_LEFT_MS = 3000;
constexpr float PASS_TOUCH = 1.5f;

// Mutated Plague lands on his victim every 10 s
constexpr int32 PLAGUE_PERIOD_MS = 10000;
constexpr int32 SWAP_LEAD_MS = 1200;
// A sharer raises its button from here and taunts from SWAP_LEAD_MS
constexpr int32 SHARE_LEAD_MS = 2500;
constexpr uint8 HOLD_APPLICATIONS = 2;
// A hold that has taken one application ends here when a taker with fewer stacks is free: 3 and 3 cost the
// raid 31% less a tick than 4 and 2
constexpr uint8 STACK_CEILING = 3;
constexpr float TAUNT_RANGE = 30.0f;
constexpr float SHARER_MIN_HEALTH_PCT = 80.0f;
constexpr uint8 SHARER_MAX_STACKS = 1;
// A sharer holds him from its taunt until a tank's lands once the stack has: the lead, a scan and a taunt
constexpr uint32 SHARE_WINDOW_MS = static_cast<uint32>(SWAP_LEAD_MS) + 600;
constexpr uint32 SHARE_HEALTH_STEP = 5;

constexpr float BRAKE_CAP = 0.9f;
constexpr float BRAKE_RELEASE = 0.8f;

// A phase 3 holder spends its button this soon after it takes him
constexpr uint32 HOLD_DEFENSIVE_MS = 4000;

constexpr float DRINK_REACH = 2.0f;
// Regurgitated Ooze reaches 50 yd
constexpr float SPIT_RANGE = 45.0f;
// The abomination stands this far inside a puddle's edge, where its tick gives Eat Ooze its caster aura
constexpr float EAT_INSIDE = 1.5f;

struct Puddle
{
    ObjectGuid guid;
    Position at;
    float radius = 0.0f;
};

struct Bomb
{
    Position at;
    uint32 ageMs = 0;
};

struct Goo
{
    Position at;
    uint32 landsMs = 0;
};

// Where he stood when an impact first showed, which is within a scan of where he threw it
struct GooOrigin
{
    uint32 castMs = 0;
    Position at;
    Position from;
};

// A Volatile Ooze with its rooted victim, or a Gas Cloud with its bloated one and the stacks left
struct Add
{
    ObjectGuid guid;
    ObjectGuid victim;
    uint8 stacks = 0;
};

struct Hazard
{
    Position at;
    float radius = 0.0f;
    char const* kind = "";
    bool urgent = false;
};

struct Stand
{
    uint8 id = 0;
    Position spot;
    bool held = false;
    bool parked = false;
};

// Asked by a trigger, its action and several rules for every action a bot weighs, and a bot never ticks
// twice in one ms
struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct HazardMemo
{
    uint32 ms = 0;
    bool set = false;
    std::vector<Hazard> hazards;
};

struct PpState
{
    uint32 scanMs = 0;
    bool scanned = false;
    uint8 phase = 1;
    bool passive = false;
    std::vector<Add> oozes;
    std::vector<Add> clouds;
    std::vector<Puddle> puddles;
    std::vector<Bomb> bombs;
    std::vector<Goo> goo;
    std::vector<GooOrigin> gooOrigins;
    ObjectGuid abomination;
    std::vector<ObjectGuid> carriers;
    // Plague Sickness or the 5 s bounce protection: a pass would hurt, or can't land
    std::unordered_set<ObjectGuid> sick;
    std::unordered_map<ObjectGuid, uint8> mutated;
    // Since the last Mutated Plague, -1 before the first
    int32 sinceAppMs = -1;

    uint32 dutyMs = 0;
    bool dutiesRan = false;
    // 1-PP_SPOT_COUNT
    RaidObs::ObsValue<uint8> spot{"pp.spot"};
    Position spotPos;
    Position tankStand;
    RaidObs::ObsGuidMap<uint8> slot{"pp.slot"};
    // Held once he first reaches the spot: at the pull he is still at his table, 51 yd off
    bool arrived = false;
    RaidObs::ObsValue<bool> rings{"pp.rings"};
    RaidObs::ObsValue<ObjectGuid> driver{"pp.driver"};
    ObjectGuid eating;
    // Unbound Plague carrier -> the bot it walks onto
    RaidObs::ObsGuidMap<ObjectGuid> receiver{"pp.receiver"};
    RaidObs::ObsGuidSet brake{"pp.brake"};
    ObjectGuid holder;
    uint8 holderStart = 0;
    uint32 holderSinceMs = 0;
    uint32 phase3Ms = 0;

    uint32 humanCallMs = 0;
    bool humanCallRan = false;

    std::unordered_map<ObjectGuid, Stand> stands;
    std::unordered_map<ObjectGuid, Position> kiteDir;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
    std::unordered_map<ObjectGuid, HazardMemo> hazards;
};

RaidInstanceState<PpState> ppStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// A bot of its own, not a player, selfbot included: players hold no duty.
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool IsPpUnit(Unit* unit)
{
    uint32 const entry = unit ? unit->GetEntry() : 0;
    return entry == NPC_PP_PUTRICIDE || entry == NPC_PP_VOLATILE_OOZE || entry == NPC_PP_GAS_CLOUD;
}

bool IsHeroic(Player* bot)
{
    Difficulty const difficulty = GetRaidMapDifficulty(bot);
    return difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC;
}

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

std::string DescribeGuid(Unit* unit) { return unit ? RaidObs::DescribeAssignment(unit->GetGUID()) : "none"; }

uint8 PhaseOf(Unit* boss) { return boss->HealthAbovePct(80) ? 1 : (boss->HealthAbovePct(35) ? 2 : 3); }

std::vector<Player*> MembersOnMap(Player* bot)
{
    std::vector<Player*> members;
    Group* group = bot->GetGroup();
    if (!group)
    {
        members.push_back(bot);
        return members;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && IccSameInstance(bot, member))
            members.push_back(member);
    }

    return members;
}

Player* MemberOf(Player* bot, ObjectGuid guid)
{
    if (guid.IsEmpty())
        return nullptr;

    Player* member = ObjectAccessor::FindPlayer(guid);
    return member && member->IsAlive() && IccSameInstance(bot, member) ? member : nullptr;
}

Position RingPoint(Position const& centre, float radius, float bearingDeg)
{
    float const angle = bearingDeg * PI_F / 180.0f;
    return Position(centre.GetPositionX() + radius * std::cos(angle), centre.GetPositionY() + radius * std::sin(angle),
                    ICC_PP_CENTER.GetPositionZ());
}

Position SpotPos(uint8 id)
{
    if (id <= 1)
        return ICC_PP_CENTER;

    if (id <= 9)
        return RingPoint(ICC_PP_CENTER, PP_SPOT_RING_NEAR, 45.0f * (id - 2));

    return RingPoint(ICC_PP_CENTER, PP_SPOT_RING_FAR, 22.5f + 45.0f * (id - 10));
}

Position SlotPos(Position const& spot, uint8 id)
{
    if (id <= PP_INNER_SLOTS)
        return RingPoint(spot, PP_INNER_RADIUS, (id - 1) * 360.0f / PP_INNER_SLOTS);

    return RingPoint(spot, PP_OUTER_RADIUS, PP_OUTER_OFFSET_DEG + (id - 1 - PP_INNER_SLOTS) * 360.0f / PP_OUTER_SLOTS);
}

bool IsInnerSlot(uint8 id) { return id >= 1 && id <= PP_INNER_SLOTS; }

float MeleeRange(Creature* boss) { return boss->GetCombatReach() + PLAYER_REACH + MELEE_LEAD; }

Creature* PutricideUnit(Player* bot)
{
    InstanceScript* instance = bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
    return instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(ICC_BOSS_PROFESSOR_PUTRICIDE)) : nullptr;
}

PpState* LiveState(PlayerbotAI* botAI, Creature** boss = nullptr)
{
    Creature* putricide = GetPpBoss(botAI);
    if (boss)
        *boss = putricide;

    return putricide ? ppStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

std::vector<Creature*> Resolve(Player* bot, std::vector<Add> const& adds)
{
    std::vector<Creature*> out;
    for (Add const& add : adds)
        if (Creature* unit = bot->GetMap()->GetCreature(add.guid))
            if (unit->IsAlive() && unit->IsInCombat())
                out.push_back(unit);

    return out;
}

Add const* AddOf(std::vector<Add> const& adds, ObjectGuid guid)
{
    for (Add const& add : adds)
        if (add.guid == guid)
            return &add;

    return nullptr;
}

// Of two of one entry the one closest to dead goes first
Creature* KillPick(Player* bot, std::vector<Add> const& adds)
{
    Creature* pick = nullptr;
    for (Creature* unit : Resolve(bot, adds))
        if (!pick || std::make_tuple(unit->GetHealthPct(), unit->GetGUID()) <
                         std::make_tuple(pick->GetHealthPct(), pick->GetGUID()))
            pick = unit;

    return pick;
}

bool IsVictimOf(std::vector<Add> const& adds, ObjectGuid guid)
{
    return std::any_of(adds.begin(), adds.end(), [guid](Add const& add) { return add.victim == guid; });
}

Creature* RiddenAbomination(Player* bot)
{
    Unit* base = bot->GetVehicleBase();
    Creature* abo = base ? base->ToCreature() : nullptr;
    uint32 const entry = abo ? abo->GetEntry() : 0;
    return entry == NPC_PP_ABOMINATION_10 || entry == NPC_PP_ABOMINATION_25 ? abo : nullptr;
}

bool IsHolder(Player* bot, PpState const& st) { return !st.holder.IsEmpty() && st.holder == bot->GetGUID(); }

// A damage dealer that took him for one application keeps its own place
bool IsTankHolder(Player* bot, PpState const& st) { return IsHolder(bot, st) && IsTankPlayer(bot); }

bool IsMeleeDps(Player* bot) { return IsRaidDpsBot(bot) && !PlayerbotAI::IsRanged(bot); }

// The cloud re-picks a random player once its victim's stacks run out, or is picking one now
bool CloudRepicking(Add const& cloud) { return cloud.victim.IsEmpty() || cloud.stacks <= CLOUD_REPICK_STACKS; }

Unit* ResolveTarget(PlayerbotAI* botAI, PpState const& st, Creature* boss)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if ((!tank && !IsRaidDpsBot(bot)) || RiddenAbomination(bot) || bot->GetGUID() == st.driver.Get())
        return nullptr;

    bool const holder = IsHolder(bot, st);
    bool const braked = st.brake.count(bot->GetGUID()) != 0;
    if (holder && !st.passive)
    {
        NoteDerived(bot, "pp.target", "boss");
        return boss;
    }

    if (!holder)
        if (Unit* skull = GetRaidHumanSkull(botAI, &IsPpUnit))
        {
            if (!braked || skull != boss)
            {
                NoteDerived(bot, "pp.target", "skull");
                return skull;
            }
        }

    Creature* ooze = KillPick(bot, st.oozes);
    Creature* cloud = KillPick(bot, st.clouds);
    // Out of reach from its slot, a ranged bot leaves the add to the others
    bool const ranged = PlayerbotAI::IsRanged(bot);
    float const reach = bot->getClass() == CLASS_HUNTER ? HUNTER_REACH : RANGED_REACH;
    if (ranged && ooze && bot->GetExactDist(ooze) > reach)
        ooze = nullptr;
    if (ranged && cloud && bot->GetExactDist(cloud) > reach)
        cloud = nullptr;

    Unit* fallback = braked ? nullptr : boss;
    // 25H intermissions: each half of the raid can hurt one add only
    if (bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_OOZE_VARIABLE, bot)))
    {
        NoteDerived(bot, "pp.target", ooze ? "variable ooze" : "variable boss");
        return ooze ? ooze : fallback;
    }

    if (bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_GAS_VARIABLE, bot)))
    {
        NoteDerived(bot, "pp.target", cloud ? "variable cloud" : "variable boss");
        return cloud ? static_cast<Unit*>(cloud) : fallback;
    }

    if (ooze)
    {
        NoteDerived(bot, "pp.target", "ooze");
        return ooze;
    }

    if (cloud)
    {
        Add const* add = AddOf(st.clouds, cloud->GetGUID());
        bool const leave = !ranged && add && CloudRepicking(*add) && add->victim != bot->GetGUID();
        if (!leave)
        {
            NoteDerived(bot, "pp.target", "cloud");
            return cloud;
        }
    }

    NoteDerived(bot, "pp.target", braked ? "brake" : "boss");
    return fallback;
}

void Scan(PpState& st, Player* bot, Creature* boss, uint32 now)
{
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    st.phase = PhaseOf(boss);
    st.passive = boss->GetReactState() == REACT_PASSIVE;
    st.oozes.clear();
    st.clouds.clear();
    st.puddles.clear();
    st.bombs.clear();
    st.goo.clear();
    st.carriers.clear();
    st.sick.clear();
    st.mutated.clear();
    st.sinceAppMs = -1;
    st.abomination.Clear();

    uint32 const adhesive = sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_ADHESIVE, boss);
    uint32 const bloat = sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_GASEOUS_BLOAT, boss);
    uint32 const mutated = sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_MUTATED_PLAGUE, boss);
    uint32 const unbound = sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_UNBOUND_PLAGUE, boss);
    uint32 const sickness = sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_PLAGUE_SICKNESS, boss);
    bool const heroic = IsHeroic(bot);

    // The ooze roots its victim and the cloud bloats its own, and each aura names the add that cast it
    std::unordered_map<ObjectGuid, std::pair<ObjectGuid, uint8>> victims;
    for (Player* member : MembersOnMap(bot))
    {
        ObjectGuid const guid = member->GetGUID();
        if (Aura const* aura = member->GetAura(adhesive))
            victims[aura->GetCasterGUID()] = {guid, 0};
        if (Aura const* aura = member->GetAura(bloat))
            victims[aura->GetCasterGUID()] = {guid, aura->GetStackAmount()};

        if (Aura const* aura = member->GetAura(mutated, boss->GetGUID()))
        {
            st.mutated[guid] = aura->GetStackAmount();
            int32 const since = aura->GetMaxDuration() - aura->GetDuration();
            st.sinceAppMs = st.sinceAppMs < 0 ? since : std::min(st.sinceAppMs, since);
        }

        if (!heroic)
            continue;

        if (member->HasAura(unbound))
            st.carriers.push_back(guid);
        if (member->HasAura(sickness) || member->HasAura(SPELL_PP_BOUNCE_PROTECTION))
            st.sick.insert(guid);
    }

    for (Creature* ooze : IccGetCreaturesByEntries(boss, {NPC_PP_VOLATILE_OOZE}, SCAN_RANGE))
    {
        auto const hit = victims.find(ooze->GetGUID());
        st.oozes.push_back({ooze->GetGUID(), hit != victims.end() ? hit->second.first : ObjectGuid::Empty, 0});
    }

    for (Creature* cloud : IccGetCreaturesByEntries(boss, {NPC_PP_GAS_CLOUD}, SCAN_RANGE))
    {
        auto const hit = victims.find(cloud->GetGUID());
        Add add{cloud->GetGUID(), ObjectGuid::Empty, 0};
        if (hit != victims.end())
        {
            add.victim = hit->second.first;
            add.stacks = hit->second.second;
        }

        st.clouds.push_back(add);
    }

    // spell_putricide_slime_puddle hits within 2.5 yd times the puddle's scale, which every Grow stack raises
    for (Creature* puddle : IccGetCreaturesByEntries(boss, {NPC_PP_PUDDLE}, SCAN_RANGE))
        st.puddles.push_back(
            {puddle->GetGUID(), puddle->GetPosition(), PP_PUDDLE_SCALE_RADIUS * puddle->GetObjectScale()});

    for (Creature* bomb : IccGetCreaturesByEntries(boss, {NPC_PP_GAS_BOMB}, SCAN_RANGE))
    {
        TempSummon* summon = bomb->ToTempSummon();
        uint32 const timer = summon ? summon->GetTimer() : BOMB_LIFETIME_MS;
        st.bombs.push_back({bomb->GetPosition(), timer < BOMB_LIFETIME_MS ? BOMB_LIFETIME_MS - timer : 0});
    }

    uint32 const instanceId = bot->GetInstanceId();
    st.gooOrigins.erase(std::remove_if(st.gooOrigins.begin(), st.gooOrigins.end(), [now](GooOrigin const& origin)
                                       { return getMSTimeDiff(origin.castMs, now) > GOO_RETENTION_MS; }),
                        st.gooOrigins.end());
    if (!IcecrownHelpers::ActiveGooPositions(instanceId, GOO_RETENTION_MS).empty())
        for (IcecrownHelpers::MalleableGooImpact const& impact : IcecrownHelpers::IccState(instanceId).malleableGoo)
        {
            if (getMSTimeDiff(impact.castTime, now) > GOO_RETENTION_MS)
                continue;

            auto origin = std::find_if(st.gooOrigins.begin(), st.gooOrigins.end(),
                                       [&impact](GooOrigin const& seen)
                                       {
                                           return seen.castMs == impact.castTime &&
                                                  seen.at.GetExactDist2d(impact.position) < 0.1f;
                                       });
            if (origin == st.gooOrigins.end())
                origin = st.gooOrigins.insert(st.gooOrigins.end(),
                                              {impact.castTime, impact.position, boss->GetPosition()});

            uint32 const flightMs =
                static_cast<uint32>(origin->from.GetExactDist2d(impact.position) / GOO_SPEED * 1000.0f);
            uint32 const landsMs = impact.castTime + flightMs + GOO_GRACE_MS;
            if (static_cast<int32>(landsMs - now) > 0)
                st.goo.push_back({impact.position, landsMs});
        }

    for (Creature* abo : IccGetCreaturesByEntries(boss, {NPC_PP_ABOMINATION_10, NPC_PP_ABOMINATION_25}, SCAN_RANGE))
        st.abomination = abo->GetGUID();
}

bool SpotFouled(PpState const& st, Position const& at, float extra)
{
    for (Puddle const& puddle : st.puddles)
        if (puddle.at.GetExactDist2d(at) < puddle.radius + SPOT_BOSS_CLEAR + SPOT_PUDDLE_MARGIN + extra)
            return true;

    for (Bomb const& bomb : st.bombs)
        if (bomb.at.GetExactDist2d(at) < SPOT_BOMB_CLEAR + extra)
            return true;

    return false;
}

float SpotClearance(PpState const& st, Position const& at)
{
    float clearance = 1000.0f;
    for (Puddle const& puddle : st.puddles)
        clearance = std::min(clearance, puddle.at.GetExactDist2d(at) - puddle.radius);
    for (Bomb const& bomb : st.bombs)
        clearance = std::min(clearance, bomb.at.GetExactDist2d(at));

    return clearance;
}

// He walks at his victim and stops once it is in his melee range, so the holder stands that far past the
// spot on the line he comes in along
Position TankStandFor(Position const& spot, Creature* boss)
{
    float dx = spot.GetPositionX() - boss->GetPositionX();
    float dy = spot.GetPositionY() - boss->GetPositionY();
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.0f)
    {
        dx = spot.GetPositionX() - ICC_PP_CENTER.GetPositionX();
        dy = spot.GetPositionY() - ICC_PP_CENTER.GetPositionY();
        length = std::sqrt(dx * dx + dy * dy);
    }

    if (length < 1.0f)
    {
        dx = 0.0f;
        dy = -1.0f;
        length = 1.0f;
    }

    float const reach = MeleeRange(boss) - DRAG_SHORT;
    return Position(spot.GetPositionX() + dx / length * reach, spot.GetPositionY() + dy / length * reach,
                    ICC_PP_CENTER.GetPositionZ());
}

void UpdateSpot(PpState& st, Creature* boss)
{
    uint8 id = st.spot;
    bool moved = false;
    if (!id)
    {
        id = 1;
        moved = true;
    }
    else if (SpotFouled(st, SpotPos(id), 0.0f))
    {
        Position const from = SpotPos(id);
        uint8 best = 0;
        float bestTravel = 0.0f;
        for (uint8 candidate = 1; candidate <= PP_SPOT_COUNT; ++candidate)
        {
            Position const at = SpotPos(candidate);
            if (SpotFouled(st, at, SPOT_PICK_MARGIN))
                continue;

            float const travel = at.GetExactDist2d(from);
            if (!best || travel < bestTravel)
            {
                best = candidate;
                bestTravel = travel;
            }
        }

        // Late in phase 3 the puddles cover every spot: take the one they leave widest
        if (!best)
        {
            float bestClearance = 0.0f;
            for (uint8 candidate = 1; candidate <= PP_SPOT_COUNT; ++candidate)
            {
                float const clearance = SpotClearance(st, SpotPos(candidate));
                if (!best || clearance > bestClearance)
                {
                    best = candidate;
                    bestClearance = clearance;
                }
            }
        }

        moved = best != id;
        id = best;
    }

    if (moved)
    {
        st.spotPos = SpotPos(id);
        st.tankStand = TankStandFor(st.spotPos, boss);
    }

    st.spot = id;
}

bool SlotUsable(PpState const& st, uint8 id, uint32 now)
{
    Position const spot = SlotPos(st.spotPos, id);
    for (Puddle const& puddle : st.puddles)
        if (puddle.at.GetExactDist2d(spot) < puddle.radius + PUDDLE_GROWTH + SLOT_MARGIN)
            return false;

    for (Bomb const& bomb : st.bombs)
    {
        float const radius = bomb.ageMs >= BLAST_ARMED_MS ? PP_BLAST_RADIUS : PP_GAS_RADIUS;
        if (bomb.at.GetExactDist2d(spot) < radius + SLOT_MARGIN)
            return false;
    }

    for (Goo const& goo : st.goo)
        if (static_cast<int32>(goo.landsMs - now) > 0 && goo.at.GetExactDist2d(spot) < PP_GOO_RADIUS + SLOT_MARGIN)
            return false;

    return true;
}

// Healers take the inner ring first, then casters. Hunters and whoever finds no inner slot take the outer.
void AssignSlots(PpState& st, std::vector<Player*> const& members, uint32 now)
{
    std::vector<Player*> healers;
    std::vector<Player*> casters;
    std::vector<Player*> hunters;
    for (Player* member : members)
    {
        // A damage dealer driving the abomination stands by the table, then rides
        if (!IsDutyBot(member) || IsTankPlayer(member) || member->GetGUID() == st.driver.Get() ||
            RiddenAbomination(member))
            continue;

        if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsRanged(member))
            (member->getClass() == CLASS_HUNTER ? hunters : casters).push_back(member);
    }

    std::sort(healers.begin(), healers.end(), IccGuidLess());
    std::sort(casters.begin(), casters.end(), IccGuidLess());
    std::sort(hunters.begin(), hunters.end(), IccGuidLess());

    std::unordered_set<ObjectGuid> roster;
    for (std::vector<Player*> const* group : {&healers, &casters, &hunters})
        for (Player* member : *group)
            roster.insert(member->GetGUID());

    for (auto itr = st.slot.begin(); itr != st.slot.end();)
        itr = roster.count(itr->first) ? std::next(itr) : st.slot.erase(itr);

    bool used[PP_SLOT_COUNT + 1] = {};
    bool usable[PP_SLOT_COUNT + 1] = {};
    for (uint8 id = 1; id <= PP_SLOT_COUNT; ++id)
        usable[id] = SlotUsable(st, id, now);

    std::vector<Player*> unplaced;
    auto const keep = [&](Player* member)
    {
        auto const held = st.slot.find(member->GetGUID());
        uint8 const id = held != st.slot.end() ? held->second : 0;
        bool const hunterInner = member->getClass() == CLASS_HUNTER && IsInnerSlot(id);
        if (id >= 1 && id <= PP_SLOT_COUNT && !used[id] && usable[id] && !hunterInner)
            used[id] = true;
        else
            unplaced.push_back(member);
    };
    for (std::vector<Player*> const* group : {&healers, &casters, &hunters})
        for (Player* member : *group)
            keep(member);

    for (Player* member : unplaced)
    {
        bool const hunter = member->getClass() == CLASS_HUNTER;
        uint8 best = 0;
        float bestScore = 0.0f;
        for (uint8 id = 1; id <= PP_SLOT_COUNT; ++id)
        {
            if (used[id] || !usable[id] || (hunter && IsInnerSlot(id)))
                continue;

            // The inner ring first for all but hunters, then the nearest slot
            float const score =
                member->GetExactDist2d(SlotPos(st.spotPos, id)) + (IsInnerSlot(id) || hunter ? 0.0f : 1000.0f);
            if (!best || score < bestScore)
            {
                best = id;
                bestScore = score;
            }
        }

        if (best)
        {
            used[best] = true;
            st.slot[member->GetGUID()] = best;
        }
        else
            st.slot.erase(member->GetGUID());
    }
}

bool DriverCandidate(Player* member, PpState const& st, Unit* victim)
{
    return IsDutyBot(member) && !PlayerbotAI::IsHeal(member) && member != victim &&
           !IsVictimOf(st.oozes, member->GetGUID()) && !IsVictimOf(st.clouds, member->GetGUID());
}

bool IsTauntDps(Player* member)
{
    return IsRaidDpsBot(member) &&
           (member->getClass() == CLASS_DEATH_KNIGHT || member->getClass() == CLASS_PALADIN);
}

// Assist tank, then any free tank, then a damage dealer: melee first (Mutated Slash replaces their swings),
// Death Knights and paladins last of those since their taunt can pick him up if the holder dies, ranged after
uint8 DriverRank(Player* member)
{
    if (IsTankPlayer(member))
        return PlayerbotAI::IsAssistTank(member) ? 0 : 1;

    if (PlayerbotAI::IsRanged(member))
        return 4;

    return IsTauntDps(member) ? 3 : 2;
}

// He is off every tank and no other tank stands on foot: a riding tank leaves the abomination to take him.
// Its driver owns the summon, so the exit dismisses it and the table serves the next driver.
bool RiderExits(PpState const& st, Player* bot, Creature* boss)
{
    if (st.passive || st.phase == 3 || !IsTankPlayer(bot))
        return false;

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (holder && IsTankPlayer(holder))
        return false;

    std::vector<Player*> const members = MembersOnMap(bot);
    return std::none_of(members.begin(), members.end(), [bot](Player* member)
                        { return member != bot && IsTankPlayer(member) && !RiddenAbomination(member); });
}

// One bot drinks while a tank holds him: a bot already riding keeps it, else the best ranked candidate
void UpdateDriver(PpState& st, Creature* boss, std::vector<Player*> const& members)
{
    if (st.phase == 3)
    {
        st.driver = ObjectGuid::Empty;
        st.eating.Clear();
        return;
    }

    for (Player* member : members)
        if (IsDutyBot(member) && RiddenAbomination(member))
        {
            st.driver = member->GetGUID();
            return;
        }

    // A player drives it, and the table refuses a second
    if (!st.abomination.IsEmpty())
    {
        st.driver = ObjectGuid::Empty;
        return;
    }

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    ObjectGuid const current = st.driver.Get();
    if (!holder || !IsTankPlayer(holder))
    {
        // A tank on foot has to pick him up, not walk to the table, and a driver he's hitting stays to hold him.
        // At his own table he holds nobody.
        auto const driver = std::find_if(members.begin(), members.end(),
                                         [current](Player* member) { return member->GetGUID() == current; });
        bool const keep = driver != members.end() && *driver != holder && (st.passive || !IsTankPlayer(*driver));
        st.driver = keep ? current : ObjectGuid::Empty;
        return;
    }

    for (Player* member : members)
        if (member->GetGUID() == current && DriverCandidate(member, st, victim))
        {
            st.driver = current;
            return;
        }

    Player* pick = nullptr;
    for (Player* member : members)
    {
        if (!DriverCandidate(member, st, victim))
            continue;

        auto const rank = [](Player* player) { return std::make_tuple(DriverRank(player), player->GetGUID()); };
        if (!pick || rank(member) < rank(pick))
            pick = member;
    }

    st.driver = pick ? pick->GetGUID() : ObjectGuid::Empty;
}

// The puddle crowding the boss spot most, latched until it is gone
void UpdateEating(PpState& st)
{
    if (st.driver.Get().IsEmpty())
    {
        st.eating.Clear();
        return;
    }

    bool const alive = std::any_of(st.puddles.begin(), st.puddles.end(),
                                   [&st](Puddle const& puddle) { return puddle.guid == st.eating; });
    if (alive)
        return;

    st.eating.Clear();
    float best = 0.0f;
    for (Puddle const& puddle : st.puddles)
    {
        float const crowd = puddle.at.GetExactDist2d(st.spotPos) - puddle.radius;
        if (st.eating.IsEmpty() || crowd < best)
        {
            st.eating = puddle.guid;
            best = crowd;
        }
    }
}

uint32 PlagueTick(Player* carrier, Creature* boss)
{
    Aura const* aura = carrier->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_UNBOUND_PLAGUE, boss));
    AuraEffect const* effect = aura ? aura->GetEffect(EFFECT_0) : nullptr;
    return effect ? effect->GetTickNumber() : 0;
}

bool ReceiverEligible(Player* member, PpState const& st, Unit* victim)
{
    ObjectGuid const guid = member->GetGUID();
    return IsDutyBot(member) && member != victim && !st.sick.count(guid) &&
           std::find(st.carriers.begin(), st.carriers.end(), guid) == st.carriers.end() &&
           !RiddenAbomination(member) && !IsVictimOf(st.clouds, guid) && !IsVictimOf(st.oozes, guid);
}

// Ranged damage dealers take the plague first, healers last
uint8 ReceiverRank(Player* member)
{
    if (PlayerbotAI::IsHeal(member))
        return 2;

    if (IsTankPlayer(member))
        return 3;

    return PlayerbotAI::IsRanged(member) ? 0 : 1;
}

void UpdateReceivers(PpState& st, Creature* boss, std::vector<Player*> const& members)
{
    for (auto itr = st.receiver.begin(); itr != st.receiver.end();)
    {
        bool const carrier = std::find(st.carriers.begin(), st.carriers.end(), itr->first) != st.carriers.end();
        itr = carrier ? std::next(itr) : st.receiver.erase(itr);
    }

    Unit* victim = boss->GetVictim();
    for (ObjectGuid const& guid : st.carriers)
    {
        Player* carrier = nullptr;
        for (Player* member : members)
            if (member->GetGUID() == guid)
                carrier = member;

        if (!carrier || !IsDutyBot(carrier) || PlagueTick(carrier, boss) < RECEIVER_ELECT_TICK)
        {
            st.receiver.erase(guid);
            continue;
        }

        auto const held = st.receiver.find(guid);
        if (held != st.receiver.end())
        {
            bool valid = false;
            for (Player* member : members)
                if (member->GetGUID() == held->second && ReceiverEligible(member, st, victim))
                    valid = true;

            if (valid)
                continue;
        }

        Player* pick = nullptr;
        for (Player* member : members)
        {
            if (member == carrier || !ReceiverEligible(member, st, victim))
                continue;

            auto const rank = [carrier](Player* player)
            { return std::make_tuple(ReceiverRank(player), player->GetExactDist2d(carrier), player->GetGUID()); };
            if (!pick || rank(member) < rank(pick))
                pick = member;
        }

        if (pick)
            st.receiver[guid] = pick->GetGUID();
        else
            st.receiver.erase(guid);
    }
}

void UpdateHolder(PpState& st, Creature* boss, uint32 now)
{
    Unit* victim = boss->GetVictim();
    ObjectGuid const guid = victim && victim->IsPlayer() ? victim->GetGUID() : ObjectGuid::Empty;
    if (guid == st.holder)
        return;

    st.holder = guid;
    auto const stacks = st.mutated.find(guid);
    st.holderStart = stacks != st.mutated.end() ? stacks->second : 0;
    st.holderSinceMs = now;
}

// A bot tank drags him onto the spot, and at his table he comes back to it. Until he first gets there the
// raid's own reach brings it to him.
void UpdateRings(PpState& st, Creature* boss)
{
    float const distance = boss->GetExactDist2d(st.spotPos);
    st.arrived = st.arrived || distance <= RING_REHOLD;
    if (!st.arrived)
    {
        st.rings = false;
        return;
    }

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    bool const dragged = st.passive || (holder && IsDutyBot(holder) && IsTankPlayer(holder));
    float const limit = st.rings.Get() ? RING_RELEASE : RING_REHOLD;
    st.rings = dragged || distance <= limit;
}

void RefreshDuties(PpState& st, Player* bot, Creature* boss, uint32 now)
{
    if (st.dutiesRan && getMSTimeDiff(st.dutyMs, now) < DUTY_REFRESH_MS)
        return;

    st.dutiesRan = true;
    st.dutyMs = now;
    if (st.phase == 3 && !st.phase3Ms)
        st.phase3Ms = now;

    std::vector<Player*> const members = MembersOnMap(bot);
    UpdateSpot(st, boss);
    AssignSlots(st, members, now);
    UpdateDriver(st, boss, members);
    UpdateEating(st);
    UpdateReceivers(st, boss, members);
    UpdateHolder(st, boss, now);
    UpdateRings(st, boss);
}

// Past BRAKE_CAP of the holder's threat until under BRAKE_RELEASE. Read from the threat list: the "threat"
// value is a uint8 percentage and wraps past 255%.
void UpdateBrake(PpState& st, Player* bot, Creature* boss)
{
    bool hold = false;
    Unit* holder = boss->GetVictim();
    if (IsDutyBot(bot) && !PlayerbotAI::IsHeal(bot) && holder && holder != bot && !RiddenAbomination(bot))
    {
        float const top = boss->GetThreatMgr().GetThreat(holder);
        float const own = boss->GetThreatMgr().GetThreat(bot);
        float const share = st.brake.count(bot->GetGUID()) ? BRAKE_RELEASE : BRAKE_CAP;
        hold = top > 0.0f && own > 0.0f && own >= share * top;
    }

    if (hold)
        st.brake.insert(bot->GetGUID());
    else
        st.brake.erase(bot->GetGUID());
}

Creature* KiteCloud(PpState const& st, Player* bot)
{
    for (Add const& cloud : st.clouds)
        if (cloud.victim == bot->GetGUID())
            if (Creature* unit = bot->GetMap()->GetCreature(cloud.guid))
                if (unit->IsAlive())
                    return unit;

    return nullptr;
}

Player* StackVictim(PpState const& st, Player* bot)
{
    Player* pick = nullptr;
    for (Add const& ooze : st.oozes)
    {
        Player* victim = MemberOf(bot, ooze.victim);
        if (victim && (!pick || victim->GetExactDist2d(bot) < pick->GetExactDist2d(bot)))
            pick = victim;
    }

    return pick;
}

bool IsCarrier(PpState const& st, Player* bot)
{
    return std::find(st.carriers.begin(), st.carriers.end(), bot->GetGUID()) != st.carriers.end();
}

// A carrier that can't walk onto its receiver: his holder would drag him along, the ooze's victim is rooted
// and a rider moves only with the drive
bool IsAnchoredCarrier(PpState const& st, Player* carrier)
{
    return IsTankHolder(carrier, st) || IsVictimOf(st.oozes, carrier->GetGUID()) || RiddenAbomination(carrier);
}

// From its 8th tick or under half health, once its searcher has ticked twice and with 3 s of plague left
bool PassDue(Player* carrier, Creature* boss)
{
    Aura const* plague = carrier->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_UNBOUND_PLAGUE, boss));
    AuraEffect const* searcher = carrier->GetAuraEffect(SPELL_PP_UNBOUND_SEARCHER, EFFECT_0);
    bool const due = PlagueTick(carrier, boss) >= PASS_TICK || carrier->GetHealthPct() < PASS_HEALTH_PCT;
    return due && plague && plague->GetDuration() >= PASS_MIN_LEFT_MS && searcher &&
           searcher->GetTickNumber() >= SEARCHER_TICKS;
}

// The anchored carrier this bot is due to walk onto, nullptr for none
Player* ReceiveFrom(PpState const& st, Player* bot, Creature* boss)
{
    for (auto const& [carrierGuid, receiverGuid] : st.receiver)
        if (receiverGuid == bot->GetGUID())
            if (Player* carrier = MemberOf(bot, carrierGuid))
                if (IsAnchoredCarrier(st, carrier) && PassDue(carrier, boss))
                    return carrier;

    return nullptr;
}

bool StackDuty(PpState const& st, Player* bot)
{
    if (!bot->IsAlive() || !IsDutyBot(bot) || IsTankHolder(bot, st) || RiddenAbomination(bot) || KiteCloud(st, bot) ||
        IsCarrier(st, bot) || bot->GetGUID() == st.driver.Get())
        return false;

    bool const role = PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsRanged(bot) || IsTankPlayer(bot);
    Player* victim = role ? StackVictim(st, bot) : nullptr;
    return victim && victim != bot;
}

void UpdateStand(PpState& st, Player* bot, Creature* boss)
{
    Stand& stand = st.stands[bot->GetGUID()];
    uint8 id = 0;
    Position spot;
    // His holder keeps dragging him while its receiver walks in
    bool const free = bot->IsAlive() && IsDutyBot(bot) && !RiddenAbomination(bot) && !KiteCloud(st, bot) &&
                      !StackDuty(st, bot) && (!IsCarrier(st, bot) || IsTankHolder(bot, st)) &&
                      bot->GetGUID() != st.driver.Get() && !ReceiveFrom(st, bot, boss);
    if (free)
    {
        if (IsTankHolder(bot, st))
        {
            if (!st.passive)
            {
                id = STAND_TANK;
                spot = st.tankStand;
            }
        }
        else if (st.rings.Get())
        {
            auto const held = st.slot.find(bot->GetGUID());
            if (held != st.slot.end())
            {
                id = held->second;
                spot = SlotPos(st.spotPos, id);
            }
        }
    }

    if (!id)
    {
        stand.held = false;
        return;
    }

    if (!stand.held || stand.id != id || stand.spot.GetExactDist2d(spot) > 0.5f)
    {
        stand.id = id;
        stand.spot = spot;
        stand.held = true;
        stand.parked = false;
    }

    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= PP_STAND_UNPARK : distance <= PP_STAND_PARK;
}

Stand const* HeldStand(PlayerbotAI* botAI)
{
    PpState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    auto const itr = st->stands.find(botAI->GetBot()->GetGUID());
    return itr != st->stands.end() && itr->second.held ? &itr->second : nullptr;
}

// The floor hazards: puddles, bomb gas and blasts, goo still in flight
void FloorHazards(PpState const& st, uint32 now, std::vector<Hazard>& out)
{
    for (Puddle const& puddle : st.puddles)
        out.push_back({puddle.at, puddle.radius + PUDDLE_GROWTH, "puddle", true});

    for (Bomb const& bomb : st.bombs)
    {
        out.push_back({bomb.at, PP_GAS_RADIUS, "gas", true});
        if (bomb.ageMs >= BLAST_ARMED_MS)
            out.push_back({bomb.at, PP_BLAST_RADIUS, "blast", true});
    }

    for (Goo const& goo : st.goo)
    {
        int32 const left = static_cast<int32>(goo.landsMs - now);
        if (left > 0)
            out.push_back({goo.at, PP_GOO_RADIUS, "goo", left < static_cast<int32>(GOO_URGENT_MS)});
    }
}

std::vector<Hazard> const& HazardsFor(PlayerbotAI* botAI, PpState& st)
{
    Player* bot = botAI->GetBot();
    HazardMemo& memo = st.hazards[bot->GetGUID()];
    uint32 const now = getMSTime();
    if (memo.set && memo.ms == now)
        return memo.hazards;

    memo.set = true;
    memo.ms = now;
    memo.hazards.clear();
    FloorHazards(st, now, memo.hazards);

    if (IsMeleeDps(bot))
        for (Add const& cloud : st.clouds)
            if (CloudRepicking(cloud) && cloud.victim != bot->GetGUID())
                if (Creature* unit = bot->GetMap()->GetCreature(cloud.guid))
                    memo.hazards.push_back({unit->GetPosition(), CLOUD_CLEAR, "cloud", false});

    for (ObjectGuid const& guid : st.carriers)
    {
        auto const receiver = st.receiver.find(guid);
        if (guid == bot->GetGUID() || (receiver != st.receiver.end() && receiver->second == bot->GetGUID()))
            continue;

        if (Player* carrier = MemberOf(bot, guid))
            memo.hazards.push_back({carrier->GetPosition(), PP_CARRIER_CLEAR, "carrier", false});
    }

    return memo.hazards;
}

bool Dodges(PpState const& st, Player* bot)
{
    return bot->IsAlive() && !IsTankHolder(bot, st) && !RiddenAbomination(bot) && !KiteCloud(st, bot) &&
           !StackDuty(st, bot) && !IsVictimOf(st.oozes, bot->GetGUID());
}

bool Covered(std::vector<Hazard> const& hazards, Position const& at, float margin)
{
    return std::any_of(hazards.begin(), hazards.end(),
                       [&](Hazard const& hazard) { return hazard.at.GetExactDist2d(at) < hazard.radius + margin; });
}

bool LegCrosses(std::vector<Hazard> const& hazards, Position const& from, Position const& to, float margin)
{
    return std::any_of(hazards.begin(), hazards.end(),
                       [&](Hazard const& hazard)
                       {
                           return hazard.at.GetExactDist2d(from) >= hazard.radius + margin &&
                                  SegmentCrossesCircle(from, to, {hazard.at, hazard.radius + margin});
                       });
}

// The rotation's next taunter, nullptr for none. `kind` says why.
Player* SwapTaunter(PpState const& st, Player* bot, Creature* boss, char const*& kind);

bool TauntUsable(Player* member)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    char const* taunt = nullptr;
    switch (member->getClass())
    {
        case CLASS_PALADIN:
            taunt = "hand of reckoning";
            break;
        case CLASS_DEATH_KNIGHT:
            taunt = "dark command";
            break;
        case CLASS_DRUID:
            taunt = "growl";
            break;
        case CLASS_WARRIOR:
            taunt = "taunt";
            break;
        default:
            break;
    }

    uint32 const spellId =
        memberAI && taunt ? memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", taunt)->Get() : 0;
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    // Growl needs bear form and Taunt Defensive Stance, so a tank elected without them would never land it
    return info && !member->HasSpellCooldown(spellId) &&
           info->CheckShapeshift(member->GetShapeshiftForm()) == SPELL_CAST_OK;
}

uint8 Stacks(PpState const& st, Player* member)
{
    auto const itr = st.mutated.find(member->GetGUID());
    return itr != st.mutated.end() ? itr->second : 0;
}

bool CanTaunt(Player* member, Creature* boss)
{
    return IsDutyBot(member) && !RiddenAbomination(member) && member->GetExactDist(boss) <= TAUNT_RANGE &&
           member->IsWithinLOSInMap(boss) && TauntUsable(member);
}

// What a button takes off his swings, read off its own damage taken aura
uint8 PhysicalCutPct(uint32 spellId)
{
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info)
        return 0;

    int32 cut = 0;
    for (SpellEffectInfo const& effect : info->Effects)
        if (effect.ApplyAuraName == SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN &&
            (effect.MiscValue & SPELL_SCHOOL_MASK_NORMAL))
            cut -= effect.CalcValue();

    return static_cast<uint8>(std::clamp(cut, 0, 100));
}

// The share of its health a sharer keeps after his swings for one application: his own damage between its
// min and max, through its armor and damage taken auras, with `cutPct` off for a button not up yet. Notes
// pp.sharehp when `noter` is the sharer itself.
float ShareHealthLeftPct(Player* member, Creature* boss, uint8 cutPct, Player* noter)
{
    float const swing =
        (boss->GetFloatValue(UNIT_FIELD_MINDAMAGE) + boss->GetFloatValue(UNIT_FIELD_MAXDAMAGE)) / 2.0f;
    uint32 const armored =
        Unit::CalcArmorReducedDamage(boss, member, static_cast<uint32>(swing), nullptr, 0, BASE_ATTACK);
    float const taken =
        member->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELL_SCHOOL_MASK_NORMAL);
    float const perSwing = armored * taken * (100 - cutPct) / 100.0f;
    uint32 const attackMs = std::max<uint32>(boss->GetAttackTime(BASE_ATTACK), 500);
    float const swings = std::ceil(static_cast<float>(SHARE_WINDOW_MS) / attackMs);
    float const left = static_cast<float>(member->GetHealth()) - swings * perSwing;
    float const pct = std::max(0.0f, left * 100.0f / std::max<uint32>(member->GetMaxHealth(), 1));
    if (noter == member)
    {
        uint32 const step = static_cast<uint32>(pct) / SHARE_HEALTH_STEP * SHARE_HEALTH_STEP;
        NoteDerived(member, "pp.sharehp", std::to_string(step));
    }

    return pct;
}

// A damage dealer whose taunt needs no form or stance and who lives through one application: a Death Knight's
// Dark Command or a Retribution paladin's Hand of Reckoning. A button it needs must be up by the taunt.
bool CanShare(Player* member, PpState const& st, Creature* boss, int32 nextMs, Player* noter)
{
    if (!GET_PLAYERBOT_AI(member) || !IsTauntDps(member) || member->GetHealthPct() < SHARER_MIN_HEALTH_PCT ||
        Stacks(st, member) > SHARER_MAX_STACKS || IsCarrier(st, member))
        return false;

    bool const running = RunningTankDefensive(member, true, 0) != nullptr;
    uint32 const button = running ? 0 : ReadyTankDefensiveSpell(member, true, 0, 0);
    uint8 const cut = running || nextMs <= SWAP_LEAD_MS ? 0 : PhysicalCutPct(button);
    return ShareHealthLeftPct(member, boss, cut, noter) >= RAID_DEFENSIVE_FLOOR_PCT;
}

// A hold ends after two applications, or at the ceiling once it has taken one while his taunt DR stays low
bool HoldDue(PpState const& st, Player* holder, Creature* boss)
{
    uint8 const held = Stacks(st, holder);
    if (held >= st.holderStart + HOLD_APPLICATIONS)
        return true;

    return held >= STACK_CEILING && held > st.holderStart &&
           boss->GetDiminishing(DIMINISHING_TAUNT) < DIMINISHING_LEVEL_3;
}

// A taunt from anyone he isn't hitting yet just landed, his victim catches up next update. His victim's own
// aura doesn't hold him, the latest taunt wins, so a tank takes him back off a sharer while that still runs.
bool TauntPending(Creature* boss, Unit* victim)
{
    Unit::AuraEffectList const& taunts = boss->GetAuraEffectsByType(SPELL_AURA_MOD_TAUNT);
    return std::any_of(taunts.begin(), taunts.end(), [victim](AuraEffect const* effect)
                       { return effect->GetCasterGUID() != victim->GetGUID(); });
}

Player* SwapTaunter(PpState const& st, Player* bot, Creature* boss, char const*& kind)
{
    kind = nullptr;
    Unit* victim = boss->GetVictim();
    if (!victim || st.passive || TauntPending(boss, victim) ||
        boss->GetDiminishing(DIMINISHING_TAUNT) >= DIMINISHING_LEVEL_TAUNT_IMMUNE)
        return nullptr;

    std::vector<Player*> const members = MembersOnMap(bot);
    Player* holder = victim->ToPlayer();
    int32 const nextMs = st.sinceAppMs < 0 ? PLAGUE_PERIOD_MS : PLAGUE_PERIOD_MS - st.sinceAppMs;
    auto const tankPick = [&](uint8 below) -> Player*
    {
        Player* pick = nullptr;
        for (Player* member : members)
        {
            if (member == holder || !IsTankPlayer(member) || !CanTaunt(member, boss) || Stacks(st, member) >= below)
                continue;

            // The driver last: it walks to the table, so it picks him up only when nobody else can
            auto const rank = [&st](Player* player)
            {
                return std::make_tuple(player->GetGUID() == st.driver.Get(), Stacks(st, player),
                                       !PlayerbotAI::IsMainTank(player), player->GetGUID());
            };
            if (!pick || rank(member) < rank(pick))
                pick = member;
        }

        return pick;
    };

    if (!holder || !IsTankPlayer(holder))
    {
        // A sharer took him for the next application: once it has landed, a tank takes him back
        bool const share = holder && st.phase == 3 && IsRaidDpsBot(holder);
        if (share && Stacks(st, holder) > st.holderStart)
        {
            kind = "back";
            return tankPick(255);
        }

        if (share && st.sinceAppMs >= 0 && nextMs <= SHARE_LEAD_MS + SWAP_LEAD_MS)
            return nullptr;

        // Anyone else holding him, at any phase, loses him to a tank at once
        kind = "pickup";
        if (Player* tank = tankPick(255))
            return tank;

        // With no tank left standing, a Death Knight or paladin damage dealer keeps him off the healers. A
        // riding tank leaves the abomination for him instead.
        bool const tankAlive =
            std::any_of(members.begin(), members.end(), [](Player* member) { return IsTankPlayer(member); });
        if (tankAlive || (holder && IsDutyBot(holder) && IsTauntDps(holder)))
            return nullptr;

        Player* guard = nullptr;
        for (Player* member : members)
        {
            if (member == holder || !IsTauntDps(member) || !CanTaunt(member, boss))
                continue;

            auto const rank = [&st](Player* player)
            {
                return std::make_tuple(player->GetGUID() == st.driver.Get(), player->getClass() != CLASS_DEATH_KNIGHT,
                                       -player->GetHealthPct(), player->GetGUID());
            };
            if (!guard || rank(member) < rank(guard))
                guard = member;
        }

        return guard;
    }

    if (st.phase != 3 || st.sinceAppMs < 0 || !HoldDue(st, holder, boss) || nextMs > SHARE_LEAD_MS)
        return nullptr;

    uint8 const held = Stacks(st, holder);
    Player* tank = tankPick(held);
    uint8 const tankStacks = tank ? Stacks(st, tank) : held;
    Player* sharer = nullptr;
    for (Player* member : members)
    {
        if (member == holder || IsTankPlayer(member) || Stacks(st, member) >= tankStacks ||
            !CanShare(member, st, boss, nextMs, bot) || !CanTaunt(member, boss))
            continue;

        auto const rank = [&st](Player* player) { return std::make_tuple(Stacks(st, player), player->GetGUID()); };
        if (!sharer || rank(member) < rank(sharer))
            sharer = member;
    }

    if (sharer)
    {
        kind = "share";
        return sharer;
    }

    if (tank && nextMs <= SWAP_LEAD_MS)
    {
        kind = "take";
        return tank;
    }

    return nullptr;
}

std::vector<Player*> OthersNear(Player* bot, float radius)
{
    std::vector<Player*> out;
    for (Player* member : MembersOnMap(bot))
        if (member != bot && member->GetExactDist2d(bot) < radius)
            out.push_back(member);

    return out;
}
}  // namespace

uint8 IccPutricidePhase(Player* bot, Unit* boss)
{
    uint8 const phase = PhaseOf(boss);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "pp.phase", std::to_string(phase));

    return phase;
}

Creature* GetPpBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetMapId() != ICC_MAP_ID || bot->GetExactDist2d(ICC_PP_CENTER) > LIVE_RANGE)
        return nullptr;

    Creature* boss = PutricideUnit(bot);
    return boss && boss->IsAlive() && boss->IsInCombat() ? boss : nullptr;
}

bool IsPpLive(PlayerbotAI* botAI) { return GetPpBoss(botAI) != nullptr; }

Unit* GetPpTarget(PlayerbotAI* botAI)
{
    Creature* boss = nullptr;
    PpState* st = LiveState(botAI, &boss);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, *st, boss);
    }

    return memo.pick;
}

char const* GetPpHazardKind(PlayerbotAI* botAI)
{
    PpState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !Dodges(*st, bot))
        return nullptr;

    for (Hazard const& hazard : HazardsFor(botAI, *st))
        if (hazard.at.GetExactDist2d(bot) < hazard.radius + DODGE_TRIGGER)
            return hazard.kind;

    return nullptr;
}

bool FindPpDodgeSpot(PlayerbotAI* botAI, Position& spot, bool& interrupt, std::string& branch)
{
    Creature* boss = nullptr;
    PpState* st = LiveState(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    std::vector<Hazard> const& hazards = HazardsFor(botAI, *st);
    Position const from = bot->GetPosition();
    interrupt = false;
    std::vector<HazardCircle> crossing;
    for (Hazard const& hazard : hazards)
    {
        bool const inside = hazard.at.GetExactDist2d(from) < hazard.radius + DODGE_TRIGGER;
        interrupt = interrupt || (inside && hazard.urgent);
        if (!inside)
            crossing.emplace_back(hazard.at, hazard.radius);
    }

    bool const melee = IsMeleeDps(bot) || IsTankPlayer(bot);
    float const meleeKeep = MeleeRange(boss) - MELEE_INSIDE;
    Position anchor;
    Position const* preferNear = nullptr;
    if (melee)
    {
        anchor = boss->GetPosition();
        preferNear = &anchor;
    }
    else if (Stand const* stand = HeldStand(botAI))
    {
        anchor = stand->spot;
        preferNear = &anchor;
    }

    HazardSweepCache sweep;
    auto const findSpot = [&](float margin, bool keepMelee, bool checkWalk)
    {
        std::vector<HazardCircle> circles;
        circles.reserve(hazards.size() + 1);
        for (Hazard const& hazard : hazards)
            circles.emplace_back(hazard.at, hazard.radius + margin);

        if (circles.empty())
            circles.emplace_back(from, 1.0f);

        auto const accept = [&](float x, float y)
        {
            Position const to(x, y, from.GetPositionZ());
            if (to.GetExactDist2d(ICC_PP_CENTER) > ROOM_RADIUS)
                return false;

            if (keepMelee && boss->GetExactDist2d(x, y) > meleeKeep)
                return false;

            return !checkWalk || std::none_of(crossing.begin(), crossing.end(), [&](HazardCircle const& circle)
                                              { return SegmentCrossesCircle(from, to, circle); });
        };

        return FindNearestPositionClearOfHazards(bot, circles, DODGE_SWEEP_RADIUS, 2.0f, PI_F / 8.0f, preferNear,
                                                 accept, &sweep);
    };

    struct Attempt
    {
        float margin;
        bool keepMelee;
        bool checkWalk;
        char const* branch;
    };
    Attempt const attempts[] = {
        {DODGE_MARGIN, true, true, "move"},  {DODGE_MARGIN, false, true, "move"},
        {DODGE_TIGHT, false, true, "tight"}, {DODGE_TIGHT, false, false, "cross"},
    };

    for (Attempt const& attempt : attempts)
    {
        if (attempt.keepMelee && !melee)
            continue;

        spot = findSpot(attempt.margin, attempt.keepMelee, attempt.checkWalk);
        if (spot != Position())
        {
            branch = attempt.branch;
            return true;
        }
    }

    branch = "none";
    return false;
}

bool IsPpDodgeSpotClear(PlayerbotAI* botAI, Position const& spot)
{
    PpState* st = LiveState(botAI);
    return st && !Covered(HazardsFor(botAI, *st), spot, DODGE_TIGHT);
}

bool IsPpKiter(PlayerbotAI* botAI)
{
    PpState* st = LiveState(botAI);
    return st && KiteCloud(*st, botAI->GetBot()) != nullptr;
}

bool GetPpKiteLeg(PlayerbotAI* botAI, Position& spot, char const*& branch)
{
    PpState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    Creature* cloud = st ? KiteCloud(*st, bot) : nullptr;
    if (!cloud)
        return false;

    Position const here = bot->GetPosition();
    float ax = here.GetPositionX() - cloud->GetPositionX();
    float ay = here.GetPositionY() - cloud->GetPositionY();
    float length = std::sqrt(ax * ax + ay * ay);
    if (length < 0.5f)
    {
        ax = here.GetPositionX() - ICC_PP_CENTER.GetPositionX();
        ay = here.GetPositionY() - ICC_PP_CENTER.GetPositionY();
        length = std::max(0.5f, std::sqrt(ax * ax + ay * ay));
    }

    ax /= length;
    ay /= length;

    Position last;
    auto const dir = st->kiteDir.find(bot->GetGUID());
    if (dir != st->kiteDir.end())
        last = dir->second;

    std::vector<Hazard> hazards;
    FloorHazards(*st, getMSTime(), hazards);

    float const cx = ICC_PP_CENTER.GetPositionX();
    float const cy = ICC_PP_CENTER.GetPositionY();
    for (bool const strict : {true, false})
    {
        bool found = false;
        float bestScore = 0.0f;
        for (int const turn : KITE_TURNS_DEG)
        {
            float const angle = turn * PI_F / 180.0f;
            float const dx = ax * std::cos(angle) - ay * std::sin(angle);
            float const dy = ax * std::sin(angle) + ay * std::cos(angle);
            float px = here.GetPositionX() + dx * KITE_LEG;
            float py = here.GetPositionY() + dy * KITE_LEG;
            bool clamped = false;
            float const out = std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
            if (out > PP_KITE_RADIUS)
            {
                px = cx + (px - cx) * PP_KITE_RADIUS / out;
                py = cy + (py - cy) * PP_KITE_RADIUS / out;
                clamped = true;
            }

            Position const to(px, py, ICC_PP_CENTER.GetPositionZ());
            float const travel = here.GetExactDist2d(to);
            if (travel < KITE_MIN_TRAVEL)
                continue;

            if (strict &&
                (Covered(hazards, to, KITE_HAZARD_MARGIN) || LegCrosses(hazards, here, to, KITE_HAZARD_MARGIN)))
                continue;

            float const lx = (px - here.GetPositionX()) / travel;
            float const ly = (py - here.GetPositionY()) / travel;
            float const continuity = lx * last.GetPositionX() + ly * last.GetPositionY();
            float const score = lx * ax + ly * ay + KITE_CONTINUITY * continuity;
            if (!found || score > bestScore)
            {
                found = true;
                bestScore = score;
                spot = to;
                branch = strict ? (clamped ? "wall" : "away") : "cross";
            }
        }

        if (found)
            return true;
    }

    branch = "none";
    return false;
}

bool IsPpKiteLegSafe(PlayerbotAI* botAI, Position const& spot)
{
    PpState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    Creature* cloud = st ? KiteCloud(*st, bot) : nullptr;
    if (!cloud)
        return false;

    std::vector<Hazard> hazards;
    FloorHazards(*st, getMSTime(), hazards);
    if (Covered(hazards, spot, 0.0f))
        return false;

    float const ax = bot->GetPositionX() - cloud->GetPositionX();
    float const ay = bot->GetPositionY() - cloud->GetPositionY();
    float const lx = spot.GetPositionX() - bot->GetPositionX();
    float const ly = spot.GetPositionY() - bot->GetPositionY();
    float const norm = std::sqrt(ax * ax + ay * ay) * std::sqrt(lx * lx + ly * ly);
    return norm < 0.01f || (ax * lx + ay * ly) / norm > KITE_KEEP_DOT;
}

void CommitPpKiteLeg(PlayerbotAI* botAI, Position const& to)
{
    PpState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return;

    float const dx = to.GetPositionX() - bot->GetPositionX();
    float const dy = to.GetPositionY() - bot->GetPositionY();
    float const length = std::sqrt(dx * dx + dy * dy);
    if (length > 0.01f)
        st->kiteDir[bot->GetGUID()] = Position(dx / length, dy / length, 0.0f);
}

bool GetPpStackWalk(PlayerbotAI* botAI, Position& spot)
{
    PpState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !StackDuty(*st, bot))
        return false;

    Player* victim = StackVictim(*st, bot);
    if (IsCarrier(*st, victim))
    {
        Creature* ooze = nullptr;
        for (Add const& add : st->oozes)
            if (add.victim == victim->GetGUID())
                ooze = bot->GetMap()->GetCreature(add.guid);

        // Toward the ooze, else toward the bot, else toward the centre
        Position side = ooze && ooze->GetExactDist2d(victim) > 1.0f ? ooze->GetPosition() : bot->GetPosition();
        if (side.GetExactDist2d(victim) < 0.5f)
            side = ICC_PP_CENTER;

        float dx = side.GetPositionX() - victim->GetPositionX();
        float dy = side.GetPositionY() - victim->GetPositionY();
        float const length = std::sqrt(dx * dx + dy * dy);
        dx = length > 0.01f ? dx / length : 1.0f;
        dy = length > 0.01f ? dy / length : 0.0f;
        float x = victim->GetPositionX() + dx * PLAGUE_STACK;
        float y = victim->GetPositionY() + dy * PLAGUE_STACK;
        // Kept on the probed floor
        float const out = std::sqrt((x - ICC_PP_CENTER.GetPositionX()) * (x - ICC_PP_CENTER.GetPositionX()) +
                                    (y - ICC_PP_CENTER.GetPositionY()) * (y - ICC_PP_CENTER.GetPositionY()));
        if (out > ROOM_RADIUS)
        {
            x = ICC_PP_CENTER.GetPositionX() + (x - ICC_PP_CENTER.GetPositionX()) * ROOM_RADIUS / out;
            y = ICC_PP_CENTER.GetPositionY() + (y - ICC_PP_CENTER.GetPositionY()) * ROOM_RADIUS / out;
        }

        Position const at(x, y, victim->GetPositionZ());
        if (bot->GetExactDist2d(at) <= PLAGUE_STACK_PARK)
            return false;

        spot = at;
        return true;
    }

    float const distance = victim->GetExactDist2d(bot);
    if (distance <= STACK_RANGE)
        return false;

    float const dx = (bot->GetPositionX() - victim->GetPositionX()) / distance;
    float const dy = (bot->GetPositionY() - victim->GetPositionY()) / distance;
    spot = Position(victim->GetPositionX() + dx * STACK_OFFSET, victim->GetPositionY() + dy * STACK_OFFSET,
                    victim->GetPositionZ());
    return true;
}

bool IsPpStacking(PlayerbotAI* botAI)
{
    PpState* st = LiveState(botAI);
    return st && StackDuty(*st, botAI->GetBot());
}

bool IsPpPlagueCarrier(PlayerbotAI* botAI)
{
    PpState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    return st && IsDutyBot(bot) && IsCarrier(*st, bot) && !RiddenAbomination(bot) && !KiteCloud(*st, bot) &&
           !IsTankHolder(bot, *st);
}

bool IsPpPlagueReceiving(PlayerbotAI* botAI)
{
    Creature* boss = nullptr;
    PpState* st = LiveState(botAI, &boss);
    return st && ReceiveFrom(*st, botAI->GetBot(), boss);
}

bool GetPpPlagueWalk(PlayerbotAI* botAI, Position& spot, char const*& branch)
{
    Creature* boss = nullptr;
    PpState* st = LiveState(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    if (!IsPpPlagueCarrier(botAI))
    {
        Player* carrier = ReceiveFrom(*st, bot, boss);
        if (!carrier || carrier->GetExactDist2d(bot) <= PASS_TOUCH)
            return false;

        branch = "receive";
        spot = carrier->GetPosition();
        return true;
    }

    // Its receiver comes to it
    if (IsAnchoredCarrier(*st, bot))
        return false;

    auto const held = st->receiver.find(bot->GetGUID());
    Player* receiver = held != st->receiver.end() ? MemberOf(bot, held->second) : nullptr;
    if (receiver && PassDue(bot, boss))
    {
        float const distance = receiver->GetExactDist2d(bot);
        if (distance <= PASS_TOUCH)
            return false;

        branch = "pass";
        spot = receiver->GetPosition();
        return true;
    }

    std::vector<Player*> const near = OthersNear(bot, PP_CARRIER_CLEAR);
    if (near.empty())
        return false;

    std::vector<HazardCircle> circles;
    for (Player* member : MembersOnMap(bot))
        if (member != bot)
            circles.emplace_back(member->GetPosition(), PP_CARRIER_CLEAR + DODGE_MARGIN);

    std::vector<Hazard> hazards;
    FloorHazards(*st, getMSTime(), hazards);
    for (Hazard const& hazard : hazards)
        circles.emplace_back(hazard.at, hazard.radius + DODGE_TIGHT);

    Position anchor = boss->GetPosition();
    if (Stand const* stand = HeldStand(botAI))
        anchor = stand->spot;

    auto const accept = [](float x, float y)
    { return Position(x, y, ICC_PP_CENTER.GetPositionZ()).GetExactDist2d(ICC_PP_CENTER) <= ROOM_RADIUS; };
    spot = FindNearestPositionClearOfHazards(bot, circles, DODGE_SWEEP_RADIUS, 2.0f, PI_F / 8.0f, &anchor, accept);
    if (spot == Position())
        return false;

    branch = "clear";
    return true;
}

bool IsPpDriver(PlayerbotAI* botAI)
{
    PpState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    return st && IsDutyBot(bot) && (RiddenAbomination(bot) || bot->GetGUID() == st->driver.Get());
}

bool IsPpDriving(PlayerbotAI* botAI) { return IsPpLive(botAI) && RiddenAbomination(botAI->GetBot()); }

bool GetPpAbomPlan(PlayerbotAI* botAI, PpAbomPlan& plan)
{
    Creature* boss = nullptr;
    PpState* st = LiveState(botAI, &boss);
    Player* bot = botAI->GetBot();
    plan = PpAbomPlan();
    if (!st || !IsPpDriver(botAI))
        return false;

    Creature* abo = RiddenAbomination(bot);
    if (!abo)
    {
        // The table refuses a second abomination and any drink once he is in phase 3
        if (st->phase == 3 || !st->abomination.IsEmpty())
            return false;

        GameObject* table = bot->FindNearestGameObject(GO_PP_DRINK_ME, 100.0f);
        if (!table || !table->isSpawned())
            return false;

        plan.duty = PpAbomDuty::Drink;
        plan.table = table;
        plan.spot = ICC_PP_DRINK_STAND;
        plan.walk = bot->GetExactDist2d(ICC_PP_DRINK_STAND) > DRINK_REACH;
        // The drink's 5 s aura summons the abomination when it fades, and another drink would only restart it
        plan.cast = !plan.walk && !bot->HasAura(SPELL_PP_MUTATION_INIT);
        return true;
    }

    if (RiderExits(*st, bot, boss))
    {
        plan.duty = PpAbomDuty::Exit;
        plan.cast = true;
        return true;
    }

    AiObjectContext* context = botAI->GetAiObjectContext();
    uint32 const spitId = context->GetValue<uint32>("vehicle spell id", "Regurgitated Ooze")->Get();
    SpellInfo const* spit = spitId ? sSpellMgr->GetSpellInfo(spitId) : nullptr;
    if (spit && !abo->HasSpellCooldown(spitId) && spit->PowerType == POWER_ENERGY)
    {
        int32 const cost = spit->CalcPowerCost(abo, spit->GetSchoolMask());
        int32 const energy = static_cast<int32>(abo->GetPower(POWER_ENERGY));
        uint32 const slowId = sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_REGURGITATED_OOZE, abo);
        Creature* cloud = nullptr;
        bool cloudSlowed = false;
        for (Creature* unit : Resolve(bot, st->clouds))
        {
            if (unit->HasAura(slowId))
                cloudSlowed = true;
            else if (!cloud && abo->GetExactDist(unit) <= SPIT_RANGE && abo->IsWithinLOSInMap(unit))
                cloud = unit;
        }

        Creature* ooze = nullptr;
        for (Creature* unit : Resolve(bot, st->oozes))
            if (!ooze && !unit->HasAura(slowId) && abo->GetExactDist(unit) <= SPIT_RANGE && abo->IsWithinLOSInMap(unit))
                ooze = unit;

        // Keep a cast for the next cloud unless one is already slowed or there is energy for both
        Unit* pick = cloud && energy >= cost ? cloud : nullptr;
        if (!pick && ooze && (energy >= 2 * cost || (cloudSlowed && energy >= cost)))
            pick = ooze;

        if (pick)
        {
            plan.duty = PpAbomDuty::Spit;
            plan.target = pick;
            plan.cast = true;
            return true;
        }
    }

    // A rider whose Unbound Plague is due leaves the puddles alone: its receiver would have to stand in one
    bool const passing = IsCarrier(*st, bot) && PassDue(bot, boss);
    for (Puddle const& puddle : st->puddles)
    {
        if (passing || puddle.guid != st->eating)
            continue;

        Creature* unit = bot->GetMap()->GetCreature(puddle.guid);
        if (!unit || !unit->IsAlive())
            break;

        uint32 const eatId = context->GetValue<uint32>("vehicle spell id", "Eat Ooze")->Get();
        float const inside = std::max(1.0f, puddle.radius - EAT_INSIDE);
        plan.duty = PpAbomDuty::Eat;
        plan.target = unit;
        plan.spot = puddle.at;
        plan.walk = abo->GetExactDist2d(puddle.at) > inside;
        // Eat Ooze needs the aura the puddle's own tick leaves on whatever stands in it
        plan.cast = eatId && !abo->HasSpellCooldown(eatId) &&
                    abo->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_PP_SLIME_PUDDLE, abo));
        return true;
    }

    Unit* target = KillPick(bot, st->oozes);
    if (!target)
        target = KillPick(bot, st->clouds);
    if (!target && !st->passive)
        target = boss;
    if (!target)
        return false;

    uint32 const slashId = context->GetValue<uint32>("vehicle spell id", "Mutated Slash")->Get();
    plan.duty = PpAbomDuty::Slash;
    plan.target = target;
    plan.spot = target->GetPosition();
    plan.walk = !abo->IsWithinMeleeRange(target);
    plan.cast = !plan.walk && slashId && !abo->HasSpellCooldown(slashId);
    return true;
}

char const* PpAbomDutyName(PpAbomDuty duty)
{
    switch (duty)
    {
        case PpAbomDuty::Drink:
            return "drink";
        case PpAbomDuty::Spit:
            return "spit";
        case PpAbomDuty::Eat:
            return "eat";
        case PpAbomDuty::Slash:
            return "slash";
        case PpAbomDuty::Exit:
            return "exit";
        default:
            return "none";
    }
}

bool IsPpSwapTaunter(PlayerbotAI* botAI, uint32& defensive, bool& taunt)
{
    Creature* boss = nullptr;
    PpState* st = LiveState(botAI, &boss);
    Player* bot = botAI->GetBot();
    defensive = 0;
    taunt = false;
    if (!st || !IsDutyBot(bot))
        return false;

    // Only a tank, a would-be sharer or a guard is ever elected, and the election walks the raid
    if (!IsTankPlayer(bot) && !IsTauntDps(bot))
        return false;

    char const* kind = nullptr;
    bool const elected = SwapTaunter(*st, bot, boss, kind) == bot;
    NoteDerived(bot, "pp.swap", elected ? kind : "none");
    if (!elected)
        return false;

    taunt = true;
    if (std::string(kind) != "share")
        return true;

    // Its button goes up from SHARE_LEAD_MS, the taunt only within SWAP_LEAD_MS of the stack
    if (!RunningTankDefensive(bot, true, 0))
        defensive = NextPhysicalDefensive(botAI, bot, "pp.sharedefensive", 0);

    taunt = st->sinceAppMs >= 0 && PLAGUE_PERIOD_MS - st->sinceAppMs <= SWAP_LEAD_MS;
    return true;
}

bool IsPpBraked(PlayerbotAI* botAI)
{
    PpState* st = LiveState(botAI);
    return st && st->brake.count(botAI->GetBot()->GetGUID());
}

uint32 GetPpTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    PpState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !bot->IsAlive() || !IsDutyBot(bot) || !IsTankPlayer(bot) || RiddenAbomination(bot))
        return 0;

    uint32 const now = getMSTime();
    uint32 const since = std::max(st->holderSinceMs, st->phase3Ms);
    bool const low = bot->GetHealthPct() < PP_TANK_DEFENSIVE_PCT;
    bool const hold =
        st->phase == 3 && st->phase3Ms && IsHolder(bot, *st) && getMSTimeDiff(since, now) < HOLD_DEFENSIVE_MS;
    if (!low && !hold)
    {
        // Change-latched, so the way out of a window is written too
        if (noteKind)
            NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    // Mutated Strength's swings are physical, so a hold alone gets no Anti-Magic Shell
    return NextTankDefensive(botAI, bot, noteKind, !low);
}

bool IsPpTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsPpLive(botAI) && botAI->GetBot()->GetHealthPct() >= PP_TANK_LAST_RESORT_PCT;
}

bool IsPpHoldingStand(PlayerbotAI* botAI) { return HeldStand(botAI) != nullptr; }

bool GetPpStandWalk(PlayerbotAI* botAI, Position& spot)
{
    PpState* st = LiveState(botAI);
    Stand const* stand = st ? HeldStand(botAI) : nullptr;
    if (!stand || stand->parked)
        return false;

    // Back only once nothing the dodge would flee covers the spot
    if (stand->id != STAND_TANK && Covered(HazardsFor(botAI, *st), stand->spot, DODGE_TRIGGER))
        return false;

    spot = stand->spot;
    return true;
}

bool IsPpMovementOwned(PlayerbotAI* botAI)
{
    PpState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    return st && (HeldStand(botAI) || KiteCloud(*st, bot) || StackDuty(*st, bot) || IsPpPlagueCarrier(botAI) ||
                  IsPpPlagueReceiving(botAI) || RiddenAbomination(bot) || bot->GetGUID() == st->driver.Get());
}

RaidBurstWindow IccPutricideBurstWindow(PlayerbotAI* botAI)
{
    RaidBurstWindow window;
    Creature* boss = GetPpBoss(botAI);
    window.allowLust = boss && PhaseOf(boss) == 3;
    return window;
}

std::vector<uint32> const& GetPpOpenerBosses(PlayerbotAI* /*botAI*/)
{
    static std::vector<uint32> const bosses = {NPC_PP_PUTRICIDE};
    return bosses;
}

uint8 IccPutricideResistSchools(PlayerbotAI* /*botAI*/, uint8 /*previous*/) { return RAID_RESIST_SHADOW; }

namespace
{
// A human tank has no trigger, so it gets told: a button at the start of a phase 3 hold or when low, and the
// taunt when the bot holding him is due to hand him over with no bot to take him
void CallHumanTank(PpState& st, Player* sender, Creature* boss, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (!holder || !IsTankPlayer(holder))
        return;

    if (!IsDutyBot(holder))
    {
        uint32 const since = std::max(st.holderSinceMs, st.phase3Ms);
        bool const hold = st.phase == 3 && st.phase3Ms && getMSTimeDiff(since, now) < HOLD_DEFENSIVE_MS;
        bool const low = holder->GetHealthPct() < PP_TANK_DEFENSIVE_PCT;
        if (!hold && !low)
            return;

        uint32 const spellId = ReadyTankDefensiveSpell(holder, true, 0, 0);
        if (spellId)
            CallHumanTankDefensive(sender, holder, spellId, 0, low ? "you are low" : "Mutated Strength hits hard",
                                   "pp.humancall");
        return;
    }

    char const* kind = nullptr;
    if (st.phase != 3 || st.sinceAppMs < 0 || PLAGUE_PERIOD_MS - st.sinceAppMs > SWAP_LEAD_MS ||
        !HoldDue(st, holder, boss) || SwapTaunter(st, sender, boss, kind))
        return;

    for (Player* member : MembersOnMap(sender))
    {
        if (IsDutyBot(member) || member == holder || !IsTankPlayer(member) || Stacks(st, member) >= Stacks(st, holder))
            continue;

        uint32 taunt = 0;
        switch (member->getClass())
        {
            case CLASS_PALADIN:
                taunt = SPELL_TAUNT_PALADIN;
                break;
            case CLASS_DEATH_KNIGHT:
                taunt = SPELL_TAUNT_DK;
                break;
            case CLASS_DRUID:
                taunt = SPELL_TAUNT_DRUID;
                break;
            case CLASS_WARRIOR:
                taunt = SPELL_TAUNT_WARRIOR;
                break;
            default:
                break;
        }

        if (taunt && member->HasSpell(taunt) && !member->HasSpellCooldown(taunt))
        {
            CallHumanTankDefensive(sender, member, taunt, static_cast<uint32>(PLAGUE_PERIOD_MS - st.sinceAppMs),
                                   "take him before the next Mutated Plague", "pp.humancall");
            return;
        }
    }
}

void NoteDuties(PpState const& st, PlayerbotAI* botAI)
{
    if (!RaidObs::Active())
        return;

    Player* bot = botAI->GetBot();
    if (bot->GetGUID() == st.driver.Get() || RiddenAbomination(bot))
    {
        PpAbomPlan plan;
        GetPpAbomPlan(botAI, plan);
        NoteDerived(bot, "pp.abom",
                    plan.target ? std::string(PpAbomDutyName(plan.duty)) + " " + DescribeGuid(plan.target)
                                : PpAbomDutyName(plan.duty));
    }

    Player* victim = StackDuty(st, bot) ? StackVictim(st, bot) : nullptr;
    NoteDerived(bot, "pp.stack", DescribeGuid(victim));

    Position spot;
    char const* branch = IsCarrier(st, bot) ? "hold" : "none";
    GetPpPlagueWalk(botAI, spot, branch);
    NoteDerived(bot, "pp.plague", branch);
}
}  // namespace

void TickPpEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // A wipe or a kill ends the pull: the next one rebuilds every duty
    Creature* putricide = PutricideUnit(bot);
    if (!putricide || !putricide->IsAlive() || !putricide->IsInCombat())
    {
        if (ppStates.Find(bot->GetInstanceId()))
            ppStates.Reset(bot->GetInstanceId());
        return;
    }

    Creature* boss = GetPpBoss(botAI);
    if (!boss)
        return;

    PpState& st = ppStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    IccPutricidePhase(bot, boss);
    Scan(st, bot, boss, now);
    RefreshDuties(st, bot, boss, now);
    UpdateBrake(st, bot, boss);
    UpdateStand(st, bot, boss);
    CallHumanTank(st, bot, boss, now);
    NoteDuties(st, botAI);
}
