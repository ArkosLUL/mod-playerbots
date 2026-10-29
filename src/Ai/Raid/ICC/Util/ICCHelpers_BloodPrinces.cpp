/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_BloodPrinces.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCData.h"
#include "ICCHelpers_Shared.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidResistAura.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr float LIVE_RANGE = 80.0f;
constexpr float SCAN_RANGE = 100.0f;
constexpr uint32 SCAN_MS = 200;
constexpr uint32 DUTY_MS = 250;
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;

// A healer node reaches every tank spot in use within HEAL_TAKE when taken and HEAL_KEEP while held: heals
// pick targets out to 38.5 yd
constexpr float HEAL_TAKE = 36.0f;
constexpr float HEAL_KEEP = 38.0f;
// A ranged node is taken within the first figure of the empowered prince and held to the second: 30 yd spells
// (hunters 35) plus both combat reaches, 1.5 and the princes' 1.75, less what a prince drifts off his spot
constexpr float RANGED_TAKE = 28.0f;
constexpr float RANGED_KEEP = 31.0f;
constexpr float HUNTER_TAKE = 32.0f;
constexpr float HUNTER_KEEP = 35.0f;
// Nodes this close to Valanar's spot are where his ranged stand, so a healer with the choice leaves them
constexpr float VALANAR_RANGED_ZONE = 28.0f;

// Empowered Shock Vortex is a 4.5 s cast: a bot reaches a node this far off before it lands
constexpr float SCATTER_REACH = 28.0f;
constexpr uint32 SCATTER_TAIL_MS = 500;

// Shock Vortex hits 12 yd round itself, centre to centre, every 1.5 s from 4.5 s after it appears
constexpr float VORTEX_RADIUS = 12.0f;
constexpr float VORTEX_TRIGGER = VORTEX_RADIUS + 0.5f;
constexpr float VORTEX_CLEAR = VORTEX_RADIUS + BPC_STAND_PARK + 1.5f;
constexpr float DODGE_SWEEP = 20.0f;
// A melee bot's dodge spot stays this close to its target's edge
constexpr float MELEE_KEEP = 4.0f;

// Flames hits 15 yd round the ball where it catches its victim
constexpr float BALL_CLEAR = 15.0f + 1.5f;
constexpr float BALL_STEP = 15.0f;
// Empowered Flare hits 10 yd round a Ball of Inferno Flame hovering 5 yd up, so 8.6 yd on the floor. Each
// player it hits takes a stack off the ball, and each stack adds 3520 fire to the explosion.
constexpr float FLARE_REACH = 8.0f;
constexpr uint8 SOAKERS = 3;
// The balls run 11 yd/s, a player 7
constexpr float BALL_SPEED = 11.0f;
constexpr float BOT_SPEED = 7.0f;
constexpr float SOAK_MAX_WALK = 20.0f;
constexpr float BOMB_DUTY_RANGE = 60.0f;
// A bomb falls 2 yd/s from 20 yd up and explodes within 2 yd of its landing; a hit lifts it 2 yd/s for 3 s
constexpr float BOMB_SHOOT_HEIGHT = 10.0f;

constexpr float TAUNT_RANGE = 30.0f;
constexpr float NUCLEUS_TAUNT_FROM = 28.0f;
// Keleseth casts from 10 yd: once he stands this close to his spot his tank fights him where he is, and
// drags him back once he drifts past KELESETH_DRIFTED
constexpr float KELESETH_PARKED = 12.0f;
constexpr float KELESETH_DRIFTED = 16.0f;
constexpr float SPLASH_GUARD = 12.0f;

enum class StandKind : uint8
{
    None,
    Node,
    Tank
};

struct Stand
{
    StandKind kind = StandKind::None;
    Position spot;
    bool parked = false;
};

// Asked by a trigger, its action and a rule for every action a bot weighs, and a bot never ticks twice in
// one ms
struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct BpcState
{
    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<ObjectGuid> bombs;
    std::vector<ObjectGuid> nuclei;
    std::vector<ObjectGuid> balls;
    std::vector<Position> vortices;

    uint32 dutyMs = 0;
    bool dutiesRan = false;
    // Prince bits per tank
    RaidObs::ObsGuidMap<uint8> tank{"bpc.tank"};
    // A node 1-BPC_NODE_COUNT per healer, ranged bot and, during the cast, scattering bot
    RaidObs::ObsGuidMap<uint8> slot{"bpc.slot"};
    // Duty bot -> the Kinetic Bomb it juggles
    RaidObs::ObsGuidMap<ObjectGuid> bomb{"bpc.bomb"};
    RaidObs::ObsGuidSet soak{"bpc.soak"};
    ObjectGuid soakBall;
    std::unordered_set<ObjectGuid> scatter;
    uint32 scatterUntilMs = 0;
    bool kelesethParked = false;

    uint32 humanCallMs = 0;
    bool humanCallRan = false;

    std::unordered_map<ObjectGuid, Stand> stands;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<BpcState> bpcStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// A bot of its own, not a player, selfbot included: players hold no duty.
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

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

bool IsPrinceUnit(Unit* unit)
{
    uint32 const entry = unit ? unit->GetEntry() : 0;
    return entry == NPC_BPC_VALANAR || entry == NPC_BPC_KELESETH || entry == NPC_BPC_TALDARAM;
}

uint8 PrinceBit(Unit* prince)
{
    switch (prince ? prince->GetEntry() : 0)
    {
        case NPC_BPC_VALANAR:
            return BPC_PRINCE_VALANAR;
        case NPC_BPC_KELESETH:
            return BPC_PRINCE_KELESETH;
        case NPC_BPC_TALDARAM:
            return BPC_PRINCE_TALDARAM;
        default:
            return 0;
    }
}

uint32 InvocationOf(uint8 bit)
{
    return bit == BPC_PRINCE_VALANAR    ? SPELL_BPC_INVOCATION_VALANAR
           : bit == BPC_PRINCE_KELESETH ? SPELL_BPC_INVOCATION_KELESETH
                                        : SPELL_BPC_INVOCATION_TALDARAM;
}

Creature* PrinceUnit(Player* bot, uint8 bit)
{
    InstanceScript* instance = bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
    if (!instance)
        return nullptr;

    uint32 const data = bit == BPC_PRINCE_VALANAR    ? BPC_DATA_VALANAR_GUID
                        : bit == BPC_PRINCE_KELESETH ? BPC_DATA_KELESETH_GUID
                                                     : BPC_DATA_TALDARAM_GUID;
    return ObjectAccessor::GetCreature(*bot, instance->GetGuidData(data));
}

Creature* LivePrince(Player* bot, uint8 bit)
{
    Creature* prince = PrinceUnit(bot, bit);
    return prince && prince->IsAlive() && prince->IsInCombat() ? prince : nullptr;
}

// Pure read: the publishing wrapper is GetBpcEmpoweredPrince
Creature* EmpoweredPrince(Player* bot)
{
    for (uint8 bit : {BPC_PRINCE_VALANAR, BPC_PRINCE_KELESETH, BPC_PRINCE_TALDARAM})
        if (Creature* prince = LivePrince(bot, bit))
            if (prince->HasAura(InvocationOf(bit)))
                return prince;

    return nullptr;
}

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

std::vector<Creature*> Resolve(Player* bot, std::vector<ObjectGuid> const& guids)
{
    std::vector<Creature*> units;
    for (ObjectGuid const& guid : guids)
        if (Creature* unit = bot->GetMap()->GetCreature(guid))
            if (unit->IsAlive())
                units.push_back(unit);

    return units;
}

Position const& NodeSpot(uint8 id) { return ICC_BPC_NODES[id - 1]; }

bool InVortex(BpcState const& st, Position const& pos, float reach)
{
    return std::any_of(st.vortices.begin(), st.vortices.end(),
                       [&](Position const& vortex) { return vortex.GetExactDist2d(pos) < reach; });
}

BpcState* LiveState(PlayerbotAI* botAI, Creature** valanar = nullptr)
{
    Creature* boss = GetBpcValanar(botAI);
    if (valanar)
        *valanar = boss;

    return boss ? bpcStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

void Scan(BpcState& st, Player* bot, uint32 now)
{
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    auto const collect = [bot](uint32 entry, std::vector<ObjectGuid>& out)
    {
        out.clear();
        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, entry, SCAN_RANGE);
        for (Creature* unit : found)
            if (unit->IsAlive())
                out.push_back(unit->GetGUID());
    };

    collect(NPC_BPC_KINETIC_BOMB, st.bombs);
    collect(NPC_BPC_DARK_NUCLEUS, st.nuclei);

    std::vector<ObjectGuid> plain;
    collect(NPC_BPC_BALL_OF_FLAME, plain);
    collect(NPC_BPC_BALL_OF_INFERNO_FLAME, st.balls);
    st.balls.insert(st.balls.end(), plain.begin(), plain.end());

    std::vector<ObjectGuid> vortices;
    collect(NPC_BPC_SHOCK_VORTEX, vortices);
    st.vortices.clear();
    for (Creature* vortex : Resolve(bot, vortices))
        st.vortices.push_back(vortex->GetPosition());
}

uint8 OwnedBy(BpcState const& st, Player* bot)
{
    auto const itr = st.tank.find(bot->GetGUID());
    return itr != st.tank.end() ? itr->second : 0;
}

bool TaldaramOwnSpot(BpcState const& st)
{
    return std::any_of(st.tank.begin(), st.tank.end(),
                       [](auto const& entry) { return entry.second == BPC_PRINCE_TALDARAM; });
}

// Main tank, then the assist tanks in their order, then guid: Valanar to the first, Keleseth to the second,
// Taldaram to the third or else to the first with Valanar
void AssignTanks(BpcState& st, std::vector<Player*> const& members)
{
    std::vector<std::pair<uint8, Player*>> ranked;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || !IsTankPlayer(member))
            continue;

        uint8 rank = PlayerbotAI::IsMainTank(member) ? 0 : 10;
        for (uint8 index = 0; rank == 10 && index < 4; ++index)
            if (PlayerbotAI::IsAssistTankOfIndex(member, index, true))
                rank = index + 1;

        ranked.emplace_back(rank, member);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](auto const& a, auto const& b)
              { return a.first != b.first ? a.first < b.first : a.second->GetGUID() < b.second->GetGUID(); });

    std::unordered_map<ObjectGuid, uint8> masks;
    if (ranked.size() >= 3)
    {
        masks[ranked[0].second->GetGUID()] = BPC_PRINCE_VALANAR;
        masks[ranked[1].second->GetGUID()] = BPC_PRINCE_KELESETH;
        masks[ranked[2].second->GetGUID()] = BPC_PRINCE_TALDARAM;
    }
    else if (ranked.size() == 2)
    {
        masks[ranked[0].second->GetGUID()] = BPC_PRINCE_VALANAR | BPC_PRINCE_TALDARAM;
        masks[ranked[1].second->GetGUID()] = BPC_PRINCE_KELESETH;
    }
    else if (ranked.size() == 1)
        masks[ranked[0].second->GetGUID()] = BPC_PRINCE_VALANAR | BPC_PRINCE_TALDARAM;

    std::vector<ObjectGuid> dropped;
    for (auto const& entry : st.tank)
        if (!masks.count(entry.first))
            dropped.push_back(entry.first);

    for (ObjectGuid const& guid : dropped)
        st.tank.erase(guid);

    for (auto const& [guid, mask] : masks)
        st.tank[guid] = mask;
}

std::vector<Position> TankSpotsInUse(BpcState const& st)
{
    uint8 owned = 0;
    for (auto const& entry : st.tank)
        owned |= entry.second;

    std::vector<Position> spots;
    if (owned & BPC_PRINCE_VALANAR)
        spots.push_back(ICC_BPC_VALANAR_SPOT);
    if (owned & BPC_PRINCE_KELESETH)
        spots.push_back(ICC_BPC_KELESETH_SPOT);
    if (TaldaramOwnSpot(st))
        spots.push_back(ICC_BPC_TALDARAM_SPOT);

    return spots;
}

bool NodeUsable(BpcState const& st, uint8 id)
{
    if (id < 1 || id > BPC_NODE_COUNT)
        return false;

    if (id == BPC_TALDARAM_NODE && TaldaramOwnSpot(st))
        return false;

    return !InVortex(st, NodeSpot(id), VORTEX_CLEAR);
}

uint8 HealerCoverage(std::vector<Position> const& spots, uint8 id, float reach)
{
    uint8 covered = 0;
    for (Position const& spot : spots)
        covered += NodeSpot(id).GetExactDist2d(spot) <= reach ? 1 : 0;

    return covered;
}

float RangedReach(Player* member, bool keep)
{
    bool const hunter = member->getClass() == CLASS_HUNTER;
    return keep ? (hunter ? HUNTER_KEEP : RANGED_KEEP) : (hunter ? HUNTER_TAKE : RANGED_TAKE);
}

// Healers keep a node covering every tank spot in use; the ranged follow the empowered prince's side. A
// healer is placed first, so a phase change never moves one.
void AssignSlots(BpcState& st, std::vector<Player*> const& members, Unit* anchor)
{
    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || IsTankPlayer(member))
            continue;

        if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
    }

    std::sort(healers.begin(), healers.end(), IccGuidLess());
    std::sort(ranged.begin(), ranged.end(), IccGuidLess());

    std::unordered_set<ObjectGuid> roster;
    for (Player* member : healers)
        roster.insert(member->GetGUID());
    for (Player* member : ranged)
        roster.insert(member->GetGUID());
    for (ObjectGuid const& guid : st.scatter)
        roster.insert(guid);

    for (auto itr = st.slot.begin(); itr != st.slot.end();)
        itr = roster.count(itr->first) ? std::next(itr) : st.slot.erase(itr);

    std::vector<Position> const spots = TankSpotsInUse(st);
    uint8 const wantedCover = static_cast<uint8>(spots.size());
    bool used[BPC_NODE_COUNT + 1] = {};

    auto const held = [&](Player* member) -> uint8
    {
        auto const itr = st.slot.find(member->GetGUID());
        return itr != st.slot.end() ? itr->second : 0;
    };

    // Scattering bots keep their node until the cast lands
    for (ObjectGuid const& guid : st.scatter)
    {
        auto const itr = st.slot.find(guid);
        if (itr != st.slot.end() && itr->second >= 1 && itr->second <= BPC_NODE_COUNT)
            used[itr->second] = true;
    }

    for (Player* healer : healers)
    {
        if (st.scatter.count(healer->GetGUID()))
            continue;

        uint8 const id = held(healer);
        if (NodeUsable(st, id) && !used[id] && HealerCoverage(spots, id, HEAL_KEEP) == wantedCover)
        {
            used[id] = true;
            continue;
        }

        uint8 best = 0;
        float bestScore = 0.0f;
        for (uint8 node = 1; node <= BPC_NODE_COUNT; ++node)
        {
            if (used[node] || !NodeUsable(st, node))
                continue;

            float score = 1000.0f * (wantedCover - HealerCoverage(spots, node, HEAL_TAKE)) +
                          healer->GetExactDist2d(NodeSpot(node));
            if (NodeSpot(node).GetExactDist2d(ICC_BPC_VALANAR_SPOT) <= VALANAR_RANGED_ZONE)
                score += 100.0f;

            if (!best || score < bestScore)
            {
                best = node;
                bestScore = score;
            }
        }

        if (best)
        {
            used[best] = true;
            st.slot[healer->GetGUID()] = best;
        }
        else
            st.slot.erase(healer->GetGUID());
    }

    for (Player* member : ranged)
    {
        if (st.scatter.count(member->GetGUID()))
            continue;

        uint8 const id = held(member);
        if (NodeUsable(st, id) && !used[id] && anchor &&
            NodeSpot(id).GetExactDist2d(anchor) <= RangedReach(member, true))
        {
            used[id] = true;
            continue;
        }

        uint8 best = 0;
        float bestDistance = 0.0f;
        for (uint8 node = 1; node <= BPC_NODE_COUNT; ++node)
        {
            if (used[node] || !NodeUsable(st, node) || !anchor ||
                NodeSpot(node).GetExactDist2d(anchor) > RangedReach(member, false))
                continue;

            float const distance = member->GetExactDist2d(NodeSpot(node));
            if (!best || distance < bestDistance)
            {
                best = node;
                bestDistance = distance;
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

// Everyone without a node runs to the nearest free one while the cast is on, and gives it back after
void UpdateScatter(BpcState& st, std::vector<Player*> const& members, Creature* valanar, uint32 now)
{
    uint32 const vortexId = sSpellMgr->GetSpellIdForDifficulty(SPELL_BPC_EMPOWERED_SHOCK_VORTEX, valanar);
    if (Spell* cast = valanar->FindCurrentSpellBySpellId(vortexId))
        st.scatterUntilMs = now + static_cast<uint32>(std::max(0, cast->GetCastTimeRemaining())) + SCATTER_TAIL_MS;

    bool const on = st.scatterUntilMs && static_cast<int32>(st.scatterUntilMs - now) > 0;
    if (!on)
    {
        st.scatterUntilMs = 0;
        for (ObjectGuid const& guid : st.scatter)
            st.slot.erase(guid);

        st.scatter.clear();
        return;
    }

    bool used[BPC_NODE_COUNT + 1] = {};
    for (auto const& entry : st.slot)
        if (entry.second >= 1 && entry.second <= BPC_NODE_COUNT)
            used[entry.second] = true;

    std::vector<Player*> runners;
    for (Player* member : members)
        if (IsDutyBot(member) && !IsTankPlayer(member) && !st.slot.count(member->GetGUID()))
            runners.push_back(member);

    std::sort(runners.begin(), runners.end(), IccGuidLess());
    for (Player* member : runners)
    {
        uint8 best = 0;
        float bestDistance = 0.0f;
        for (uint8 node = 1; node <= BPC_NODE_COUNT; ++node)
        {
            if (used[node] || !NodeUsable(st, node))
                continue;

            float const distance = member->GetExactDist2d(NodeSpot(node));
            if (distance <= SCATTER_REACH && (!best || distance < bestDistance))
            {
                best = node;
                bestDistance = distance;
            }
        }

        if (!best)
            continue;

        used[best] = true;
        st.slot[member->GetGUID()] = best;
        st.scatter.insert(member->GetGUID());
    }
}

float BombHeight(Creature* bomb)
{
    float const ground = bomb->GetMapHeight(bomb->GetPositionX(), bomb->GetPositionY(), bomb->GetPositionZ());
    return ground > INVALID_HEIGHT ? bomb->GetPositionZ() - ground : 0.0f;
}

// Only a direct hit procs the bomb's Unstable, never periodic damage: hunters fire instants and Auto Shot,
// mages open on direct spells, a warlock or a shadow priest often on a DoT
uint8 BombTier(Player* member)
{
    switch (member->getClass())
    {
        case CLASS_HUNTER:
            return 0;
        case CLASS_MAGE:
            return 1;
        default:
            return 2;
    }
}

void AssignBombs(BpcState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<Creature*> const bombs = Resolve(bot, st.bombs);
    std::unordered_set<ObjectGuid> live;
    for (Creature* bomb : bombs)
        live.insert(bomb->GetGUID());

    std::unordered_set<ObjectGuid> roster;
    std::vector<Player*> shooters;
    for (Player* member : members)
        if (IsDutyBot(member) && PlayerbotAI::IsRangedDps(member) && !IsTankPlayer(member))
        {
            shooters.push_back(member);
            roster.insert(member->GetGUID());
        }

    // Erased by guid, so the trace sees the duty end
    std::unordered_set<ObjectGuid> covered;
    std::vector<ObjectGuid> ended;
    for (auto const& [shooter, bomb] : st.bomb)
    {
        if (!roster.count(shooter) || !live.count(bomb))
            ended.push_back(shooter);
        else
            covered.insert(bomb);
    }

    for (ObjectGuid const& shooter : ended)
        st.bomb.erase(shooter);

    for (Creature* bomb : bombs)
    {
        if (covered.count(bomb->GetGUID()))
            continue;

        Player* best = nullptr;
        uint8 bestTier = 0;
        float bestDistance = 0.0f;
        for (Player* member : shooters)
        {
            if (st.bomb.count(member->GetGUID()))
                continue;

            float const distance = member->GetExactDist2d(bomb);
            uint8 const tier = BombTier(member);
            if (distance > BOMB_DUTY_RANGE)
                continue;

            if (!best || tier < bestTier || (tier == bestTier && distance < bestDistance))
            {
                best = member;
                bestTier = tier;
                bestDistance = distance;
            }
        }

        if (best)
            st.bomb[best->GetGUID()] = bomb->GetGUID();
    }
}

Creature* InfernoBall(BpcState const& st, Player* bot, Player*& victim)
{
    victim = nullptr;
    for (Creature* ball : Resolve(bot, st.balls))
    {
        if (ball->GetEntry() != NPC_BPC_BALL_OF_INFERNO_FLAME)
            continue;

        Unit* chased = ball->GetVictim();
        if (!chased || !chased->IsPlayer() ||
            !ball->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_BPC_BALL_OF_FLAMES_PROC, ball)))
            continue;

        victim = chased->ToPlayer();
        return ball;
    }

    return nullptr;
}

float DistanceToSegment(Position const& from, Position const& to, Position const& pos)
{
    float const dx = to.GetPositionX() - from.GetPositionX();
    float const dy = to.GetPositionY() - from.GetPositionY();
    float const length2 = dx * dx + dy * dy;
    float t = 0.0f;
    if (length2 > 0.001f)
        t = std::clamp(((pos.GetPositionX() - from.GetPositionX()) * dx + (pos.GetPositionY() - from.GetPositionY()) * dy) /
                           length2,
                       0.0f, 1.0f);

    return std::hypot(from.GetPositionX() + t * dx - pos.GetPositionX(), from.GetPositionY() + t * dy - pos.GetPositionY());
}

// The melee nearest its path first, then the rest by the same distance
void ReleaseSoakers(BpcState& st)
{
    std::vector<ObjectGuid> const soakers(st.soak.begin(), st.soak.end());
    for (ObjectGuid const& guid : soakers)
        st.soak.erase(guid);
}

void AssignSoak(BpcState& st, Player* bot, std::vector<Player*> const& members)
{
    Player* victim = nullptr;
    Creature* ball = InfernoBall(st, bot, victim);
    if (!ball)
    {
        ReleaseSoakers(st);
        st.soakBall = ObjectGuid::Empty;
        return;
    }

    if (st.soakBall == ball->GetGUID())
        return;

    ReleaseSoakers(st);
    st.soakBall = ball->GetGUID();
    std::vector<std::pair<float, Player*>> ranked;
    for (Player* member : members)
    {
        if (member == victim || !IsDutyBot(member) || IsTankPlayer(member) || st.bomb.count(member->GetGUID()))
            continue;

        float const away = DistanceToSegment(ball->GetPosition(), victim->GetPosition(), member->GetPosition());
        ranked.emplace_back(away + (PlayerbotAI::IsRanged(member) ? 1000.0f : 0.0f), member);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](auto const& a, auto const& b)
              { return a.first != b.first ? a.first < b.first : a.second->GetGUID() < b.second->GetGUID(); });
    for (size_t i = 0; i < ranked.size() && i < SOAKERS; ++i)
        st.soak.insert(ranked[i].second->GetGUID());
}

void RefreshDuties(BpcState& st, Player* bot, Creature* valanar, uint32 now)
{
    if (st.dutiesRan && getMSTimeDiff(st.dutyMs, now) < DUTY_MS)
        return;

    st.dutiesRan = true;
    st.dutyMs = now;
    std::vector<Player*> const members = MembersOnMap(bot);
    AssignTanks(st, members);
    UpdateScatter(st, members, valanar, now);

    Creature* empowered = EmpoweredPrince(bot);
    AssignSlots(st, members, empowered ? empowered : valanar);
    AssignBombs(st, bot, members);
    AssignSoak(st, bot, members);

    if (Creature* keleseth = LivePrince(bot, BPC_PRINCE_KELESETH))
    {
        float const off = keleseth->GetExactDist2d(ICC_BPC_KELESETH_SPOT);
        st.kelesethParked = st.kelesethParked ? off <= KELESETH_DRIFTED : off <= KELESETH_PARKED;
    }
}

bool OnSideDuty(BpcState const& st, Player* bot);

void UpdateStand(BpcState& st, Player* bot)
{
    Stand& stand = st.stands[bot->GetGUID()];
    StandKind kind = StandKind::None;
    Position spot;
    if (uint8 const owned = OwnedBy(st, bot))
    {
        bool holdingAll = true;
        for (uint8 bit : {BPC_PRINCE_VALANAR, BPC_PRINCE_KELESETH, BPC_PRINCE_TALDARAM})
            if (owned & bit)
                if (Creature* prince = LivePrince(bot, bit))
                    holdingAll = holdingAll && prince->GetVictim() == bot;

        spot = (owned & BPC_PRINCE_VALANAR)    ? ICC_BPC_VALANAR_SPOT
               : (owned & BPC_PRINCE_KELESETH) ? ICC_BPC_KELESETH_SPOT
                                               : ICC_BPC_TALDARAM_SPOT;
        bool const kelesethOnly = owned == BPC_PRINCE_KELESETH;
        if (holdingAll && !(kelesethOnly && st.kelesethParked) && !InVortex(st, spot, VORTEX_CLEAR))
            kind = StandKind::Tank;
    }
    else if (!OnSideDuty(st, bot))
    {
        auto const held = st.slot.find(bot->GetGUID());
        if (held != st.slot.end() && held->second >= 1 && held->second <= BPC_NODE_COUNT)
        {
            kind = StandKind::Node;
            spot = NodeSpot(held->second);
        }
    }

    if (kind == StandKind::None)
    {
        stand.kind = StandKind::None;
        return;
    }

    if (stand.kind != kind || stand.spot.GetExactDist2d(spot) > 0.5f)
    {
        stand.kind = kind;
        stand.spot = spot;
        stand.parked = false;
    }

    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= BPC_STAND_UNPARK : distance <= BPC_STAND_PARK;
}

Stand const* HeldStand(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    auto const itr = st->stands.find(botAI->GetBot()->GetGUID());
    return itr != st->stands.end() && itr->second.kind != StandKind::None ? &itr->second : nullptr;
}

Creature* BallChasing(BpcState const& st, Player* bot)
{
    for (Creature* ball : Resolve(bot, st.balls))
        if (ball->GetVictim() == bot)
            return ball;

    return nullptr;
}

Creature* ShotFor(BpcState const& st, Player* bot)
{
    auto const itr = st.bomb.find(bot->GetGUID());
    Creature* bomb = itr != st.bomb.end() ? bot->GetMap()->GetCreature(itr->second) : nullptr;
    if (!bomb || !bomb->IsAlive() || bomb->HasAura(SPELL_BPC_KINETIC_BOMB_KNOCKBACK))
        return nullptr;

    return BombHeight(bomb) <= BOMB_SHOOT_HEIGHT ? bomb : nullptr;
}

// A bomb its node is out of range of stays the bot's to stand by until it goes
bool BombAway(BpcState const& st, Player* bot)
{
    auto const duty = st.bomb.find(bot->GetGUID());
    auto const node = st.slot.find(bot->GetGUID());
    if (duty == st.bomb.end() || node == st.slot.end() || node->second < 1 || node->second > BPC_NODE_COUNT)
        return false;

    Creature* bomb = bot->GetMap()->GetCreature(duty->second);
    return bomb && bomb->IsAlive() && NodeSpot(node->second).GetExactDist2d(bomb) > RangedReach(bot, true);
}

// A bomb shot, a ball on the bot or a soak takes it off its node for the while
bool OnSideDuty(BpcState const& st, Player* bot)
{
    return ShotFor(st, bot) || BombAway(st, bot) || BallChasing(st, bot) || st.soak.count(bot->GetGUID());
}

// The prince of its own that somebody else holds first, so threat brings it back, then the empowered one,
// then the one it is on
Creature* OwnedPick(PlayerbotAI* botAI, uint8 owned)
{
    Player* bot = botAI->GetBot();
    std::vector<Creature*> mine;
    for (uint8 bit : {BPC_PRINCE_VALANAR, BPC_PRINCE_TALDARAM, BPC_PRINCE_KELESETH})
        if (owned & bit)
            if (Creature* prince = LivePrince(bot, bit))
                mine.push_back(prince);

    if (mine.empty())
        return nullptr;

    for (Creature* prince : mine)
        if (prince->GetVictim() != bot)
            return prince;

    Creature* empowered = EmpoweredPrince(bot);
    if (std::find(mine.begin(), mine.end(), empowered) != mine.end())
        return empowered;

    Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    for (Creature* prince : mine)
        if (prince == current)
            return prince;

    return mine.front();
}

Unit* ResolveTarget(PlayerbotAI* botAI, BpcState& st)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    if (uint8 const owned = OwnedBy(st, bot))
        if (Creature* prince = OwnedPick(botAI, owned))
        {
            NoteDerived(bot, "bpc.target", prince->GetVictim() == bot ? "owned" : "recover");
            return prince;
        }

    if (Creature* bomb = ShotFor(st, bot))
    {
        NoteDerived(bot, "bpc.target", "bomb");
        return bomb;
    }

    if (Unit* skull = GetRaidHumanSkull(botAI, &IsPrinceUnit))
    {
        NoteDerived(bot, "bpc.target", "skull");
        return skull;
    }

    Creature* empowered = EmpoweredPrince(bot);
    NoteDerived(bot, "bpc.target", empowered ? "empowered" : "none");
    return empowered;
}

bool IsNucleusOwner(BpcState const& st, Player* bot) { return (OwnedBy(st, bot) & BPC_PRINCE_KELESETH) != 0; }

Creature* NearestLooseNucleus(BpcState const& st, Player* bot)
{
    Creature* nearest = nullptr;
    for (Creature* nucleus : Resolve(bot, st.nuclei))
        if (nucleus->GetVictim() != bot &&
            nucleus->GetExactDist2d(ICC_BPC_ROOM_CENTER) <= BPC_ROOM_RADIUS + 10.0f &&
            (!nearest || bot->GetExactDist2d(nucleus) < bot->GetExactDist2d(nearest)))
            nearest = nucleus;

    return nearest;
}

// Keleseth's tank walks toward a nucleus out of taunt range while short of stacks, outside his own phase:
// that is the phase the stacks are for, and he stays put on his tank through it. Only with the taunt ready,
// or the tank arrives, can't taunt and its stand walks it straight back.
Creature* NucleusToWalkTo(BpcState const& st, PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsNucleusOwner(st, bot) || bot->GetAuraCount(SPELL_BPC_SHADOW_RESONANCE) >= BPC_NUCLEUS_STACKS ||
        !ClassTauntReady(botAI))
        return nullptr;

    Creature* empowered = EmpoweredPrince(bot);
    if (empowered && empowered->GetEntry() == NPC_BPC_KELESETH)
        return nullptr;

    Creature* nucleus = NearestLooseNucleus(st, bot);
    return nucleus && bot->GetExactDist2d(nucleus) > NUCLEUS_TAUNT_FROM ? nucleus : nullptr;
}

// Holders of a node get a fresh one from the tick instead
bool InVortexWithoutNode(BpcState const& st, Player* bot)
{
    auto const stand = st.stands.find(bot->GetGUID());
    if (stand != st.stands.end() && stand->second.kind == StandKind::Node)
        return false;

    return InVortex(st, bot->GetPosition(), VORTEX_TRIGGER);
}
}  // namespace

bool IsBpcLive(PlayerbotAI* botAI) { return GetBpcValanar(botAI) != nullptr; }

Creature* GetBpcValanar(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetExactDist2d(ICC_BPC_ROOM_CENTER) > LIVE_RANGE)
        return nullptr;

    return LivePrince(bot, BPC_PRINCE_VALANAR);
}

Creature* GetBpcEmpoweredPrince(PlayerbotAI* botAI)
{
    if (!IsBpcLive(botAI))
        return nullptr;

    Player* bot = botAI->GetBot();
    Creature* empowered = EmpoweredPrince(bot);
    char const* phase = "none";
    switch (PrinceBit(empowered))
    {
        case BPC_PRINCE_VALANAR:
            phase = "valanar";
            break;
        case BPC_PRINCE_KELESETH:
            phase = "keleseth";
            break;
        case BPC_PRINCE_TALDARAM:
            phase = "taldaram";
            break;
        default:
            break;
    }

    NoteDerived(bot, "bpc.phase", phase);
    return empowered;
}

Unit* GetBpcTarget(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, *st);
    }

    return memo.pick;
}

Unit* GetBpcPrinceToTaunt(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    uint8 const owned = st ? OwnedBy(*st, bot) : 0;
    if (!owned || !ClassTauntReady(botAI))
        return nullptr;

    for (uint8 bit : {BPC_PRINCE_VALANAR, BPC_PRINCE_TALDARAM, BPC_PRINCE_KELESETH})
    {
        Creature* prince = (owned & bit) ? LivePrince(bot, bit) : nullptr;
        if (!prince || !prince->GetVictim() || prince->GetVictim() == bot)
            continue;

        // The princes obey taunt diminishing returns: past two taunts in a row threat has to bring one back
        if (prince->GetDiminishing(DIMINISHING_TAUNT) >= DIMINISHING_LEVEL_3)
            continue;

        if (bot->GetExactDist(prince) <= TAUNT_RANGE && bot->IsWithinLOSInMap(prince))
            return prince;
    }

    return nullptr;
}

Unit* GetBpcNucleusToTaunt(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsNucleusOwner(*st, bot))
        return nullptr;

    uint32 const stacks = bot->GetAuraCount(SPELL_BPC_SHADOW_RESONANCE);
    if (stacks >= BPC_NUCLEUS_STACKS)
    {
        NoteDerived(bot, "bpc.nucleus", "full");
        return nullptr;
    }

    Creature* nucleus = NearestLooseNucleus(*st, bot);
    if (!nucleus)
    {
        NoteDerived(bot, "bpc.nucleus", "none");
        return nullptr;
    }

    // Taunts on a nucleus never diminish
    if (bot->GetExactDist(nucleus) > TAUNT_RANGE || !bot->IsWithinLOSInMap(nucleus))
        return nullptr;

    if (!ClassTauntReady(botAI))
    {
        NoteDerived(bot, "bpc.nucleus", "cooldown");
        return nullptr;
    }

    NoteDerived(bot, "bpc.nucleus", "taunt " + std::to_string(stacks));
    return nucleus;
}

bool GetBpcNucleusWalk(PlayerbotAI* botAI, Position& spot)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    Creature* nucleus = st ? NucleusToWalkTo(*st, botAI) : nullptr;
    if (!nucleus)
        return false;

    float const step = bot->GetExactDist2d(nucleus) - NUCLEUS_TAUNT_FROM + 1.0f;
    float const angle = bot->GetAngle(nucleus);
    spot.Relocate(bot->GetPositionX() + step * std::cos(angle), bot->GetPositionY() + step * std::sin(angle),
                  bot->GetPositionZ());
    NoteDerived(bot, "bpc.nucleus", "walk");
    return true;
}

Unit* GetBpcBombShot(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    return st ? ShotFor(*st, botAI->GetBot()) : nullptr;
}

bool IsBpcBombShooting(PlayerbotAI* botAI) { return GetBpcBombShot(botAI) != nullptr; }

bool GetBpcBallWalk(PlayerbotAI* botAI, Position& spot)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    Creature* ball = st ? BallChasing(*st, bot) : nullptr;
    if (!ball)
        return false;

    char const* kind = ball->GetEntry() == NPC_BPC_BALL_OF_INFERNO_FLAME ? "inferno" : "plain";
    std::vector<HazardCircle> hazards;
    for (Player* member : MembersOnMap(bot))
        if (member != bot)
            hazards.emplace_back(member->GetPosition(), BALL_CLEAR);
    for (Position const& vortex : st->vortices)
        hazards.emplace_back(vortex, VORTEX_CLEAR);

    bool const clear = std::none_of(hazards.begin(), hazards.end(), [bot](HazardCircle const& hazard)
                                    { return hazard.first.GetExactDist2d(bot) < hazard.second; });
    if (clear)
    {
        NoteDerived(bot, "bpc.ball", std::string(kind) + " clear");
        return false;
    }

    Position const found = FindNearestPositionClearOfHazards(bot, hazards, BALL_STEP, nullptr);
    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        NoteDerived(bot, "bpc.ball", std::string(kind) + " none");
        return false;
    }

    NoteDerived(bot, "bpc.ball", std::string(kind) + " step");
    spot = found;
    return true;
}

bool GetBpcSoakWalk(PlayerbotAI* botAI, Position& spot)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !st->soak.count(bot->GetGUID()))
        return false;

    Player* victim = nullptr;
    Creature* ball = InfernoBall(*st, bot, victim);
    if (!ball || ball->GetGUID() != st->soakBall)
        return false;

    // Already where it passes
    Position const from = ball->GetPosition();
    Position const to = victim->GetPosition();
    if (DistanceToSegment(from, to, bot->GetPosition()) <= FLARE_REACH - 2.0f)
        return false;

    // The first point of its path the bot reaches before the ball does, short of the explosion round the victim
    float const length = from.GetExactDist2d(to);
    float const dx = (to.GetPositionX() - from.GetPositionX()) / std::max(length, 0.001f);
    float const dy = (to.GetPositionY() - from.GetPositionY()) / std::max(length, 0.001f);
    for (float along = 0.0f; along <= length - BALL_CLEAR; along += 1.0f)
    {
        Position const point(from.GetPositionX() + dx * along, from.GetPositionY() + dy * along, bot->GetPositionZ());
        float const walk = bot->GetExactDist2d(point);
        if (walk <= SOAK_MAX_WALK && walk / BOT_SPEED <= along / BALL_SPEED)
        {
            spot = point;
            return true;
        }
    }

    return false;
}

bool IsBpcInVortex(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    bool const inside = InVortexWithoutNode(*st, bot);
    if (!inside)
        NoteDerived(bot, "bpc.dodge", "none");

    return inside;
}

bool FindBpcDodgeSpot(PlayerbotAI* botAI, Position& spot)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    std::vector<HazardCircle> hazards;
    for (Position const& vortex : st->vortices)
        hazards.emplace_back(vortex, VORTEX_CLEAR);

    // Melee and tanks keep their target in reach, so the fight goes on from the far side
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    bool const melee = target && !PlayerbotAI::IsRanged(bot);
    Position found;
    if (melee)
        found = FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP, 2.0f, static_cast<float>(M_PI) / 8.0f,
                                                  nullptr,
                                                  [target](float x, float y)
                                                  {
                                                      return target->GetExactDist2d(x, y) <=
                                                             target->GetCombatReach() + MELEE_KEEP;
                                                  });

    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
        found = FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP, nullptr);

    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        NoteDerived(bot, "bpc.dodge", "blocked");
        return false;
    }

    NoteDerived(bot, "bpc.dodge", melee ? "melee" : "clear");
    spot = found;
    return true;
}

bool IsBpcHoldingStand(PlayerbotAI* botAI)
{
    Stand const* stand = HeldStand(botAI);
    return stand && stand->kind == StandKind::Node;
}

bool IsBpcHoldingTankStand(PlayerbotAI* botAI)
{
    Stand const* stand = HeldStand(botAI);
    return stand && stand->kind == StandKind::Tank;
}

bool GetBpcStandWalk(PlayerbotAI* botAI, Position& spot)
{
    BpcState* st = LiveState(botAI);
    Stand const* stand = st ? HeldStand(botAI) : nullptr;
    if (!stand || stand->parked || InVortex(*st, stand->spot, VORTEX_TRIGGER))
        return false;

    spot = stand->spot;
    return true;
}

bool IsBpcScattering(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    return st && st->scatter.count(botAI->GetBot()->GetGUID());
}

bool IsBpcMovementOwned(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    return HeldStand(botAI) || st->soak.count(bot->GetGUID()) || BallChasing(*st, bot) ||
           InVortexWithoutNode(*st, bot) || NucleusToWalkTo(*st, botAI);
}

bool IsBpcPrisonHeld(PlayerbotAI* botAI) { return IsHeroic(botAI->GetBot()) && IsBpcLive(botAI); }

uint32 GetBpcTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !bot->IsAlive() || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return 0;

    Creature* keleseth = LivePrince(bot, BPC_PRINCE_KELESETH);
    bool const lance = keleseth && keleseth->GetVictim() == bot && keleseth->HasAura(SPELL_BPC_INVOCATION_KELESETH);
    if (!lance && bot->GetHealthPct() >= BPC_TANK_DEFENSIVE_PCT)
    {
        // Change-latched, so the way out of a window is written too
        if (noteKind)
            NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    // Empowered Shadow Lance is shadow; the princes swinging at a low tank are physical
    return NextTankDefensive(botAI, bot, noteKind, !lance);
}

bool IsBpcTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsBpcLive(botAI) && botAI->GetBot()->GetHealthPct() >= BPC_TANK_LAST_RESORT_PCT;
}

bool IsBpcAreaDamageHeld(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || IsTankPlayer(bot))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    auto const close = [&](Unit* unit)
    { return unit->GetExactDist2d(bot) < SPLASH_GUARD || (target && unit->GetExactDist2d(target) < SPLASH_GUARD); };

    Creature* empowered = EmpoweredPrince(bot);
    for (uint8 bit : {BPC_PRINCE_VALANAR, BPC_PRINCE_KELESETH, BPC_PRINCE_TALDARAM})
        if (Creature* prince = LivePrince(bot, bit))
            if (prince != empowered && close(prince))
                return true;

    for (Creature* nucleus : Resolve(bot, st->nuclei))
        if (close(nucleus))
            return true;

    return false;
}

namespace
{
// A human tank has no trigger, so it gets told: a button through Keleseth's empowered Shadow Lance, or when low
void CallHumanTank(BpcState& st, Player* sender, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;

    Creature* keleseth = LivePrince(sender, BPC_PRINCE_KELESETH);
    bool const lancing = keleseth && keleseth->HasAura(SPELL_BPC_INVOCATION_KELESETH);
    for (Player* member : MembersOnMap(sender))
    {
        if (IsDutyBot(member) || !IsTankPlayer(member))
            continue;

        bool const lanced = lancing && keleseth->GetVictim() == member;
        bool hit = lanced;
        for (uint8 bit : {BPC_PRINCE_VALANAR, BPC_PRINCE_TALDARAM})
            if (Creature* prince = LivePrince(sender, bit))
                hit = hit || prince->GetVictim() == member;

        bool const low = hit && member->GetHealthPct() < BPC_TANK_DEFENSIVE_PCT;
        if (!lanced && !low)
            continue;

        uint32 const spellId = ReadyTankDefensiveSpell(member, !lanced, 0, 0);
        if (spellId)
            CallHumanTankDefensive(sender, member, spellId, 0,
                                   lanced ? "Keleseth's Shadow Lance is empowered" : "you are low", "bpc.humancall");
    }
}

void NoteDuties(BpcState const& st, Player* bot)
{
    if (!RaidObs::Active() || !st.bomb.count(bot->GetGUID()))
        return;

    NoteDerived(bot, "bpc.juggle", ShotFor(st, bot) ? "shoot" : "wait");
}
}  // namespace

void TickBpcEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // A wipe or a kill ends the pull: the next one rebuilds every duty
    Creature* valanar = PrinceUnit(bot, BPC_PRINCE_VALANAR);
    if (!valanar || !valanar->IsAlive() || !valanar->IsInCombat())
    {
        if (bpcStates.Find(bot->GetInstanceId()))
            bpcStates.Reset(bot->GetInstanceId());
        return;
    }

    Creature* boss = GetBpcValanar(botAI);
    if (!boss)
        return;

    BpcState& st = bpcStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    Scan(st, bot, now);
    RefreshDuties(st, bot, boss, now);
    CallHumanTank(st, bot, now);
    GetBpcEmpoweredPrince(botAI);
    NoteDuties(st, bot);
    UpdateStand(st, bot);
}

RaidBurstWindow IccBloodPrincesBurstWindow(PlayerbotAI* botAI)
{
    RaidBurstWindow window;
    Creature* empowered = IsBpcLive(botAI) ? EmpoweredPrince(botAI->GetBot()) : nullptr;
    window.allowLust = empowered && empowered->GetEntry() == NPC_BPC_KELESETH;
    return window;
}

std::vector<uint32> const& GetBpcOpenerBosses(PlayerbotAI* /*botAI*/)
{
    static std::vector<uint32> const bosses = {NPC_BPC_VALANAR};
    return bosses;
}

uint8 IccBloodPrincesResistSchools(PlayerbotAI* /*botAI*/, uint8 /*previous*/)
{
    return RAID_RESIST_SHADOW | RAID_RESIST_FIRE;
}
