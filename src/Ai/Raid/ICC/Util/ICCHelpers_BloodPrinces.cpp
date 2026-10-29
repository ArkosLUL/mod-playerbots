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
#include "Pet.h"
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
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
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
// Nodes this close to Valanar's spot are where his ranged stand, so a healer leaves them while it can
// still reach a tank spot from outside. 25-man fits only 14 nodes in reach of him.
constexpr float VALANAR_RANGED_ZONE = 28.0f;
constexpr float HEALER_ZONE_COST = 1500.0f;
constexpr float HEALER_SHORT_COST = 1000.0f;
// The dais rises east of here. Its nodes in reach of Valanar's spot stay free of ranged in his phase, for
// the melee to scatter onto.
constexpr float DAIS_X = 4662.0f;

// Empowered Shock Vortex is a 4.5 s cast, 4 s on heroic: a bot takes a node no farther than it runs in
// what is left of the cast less SCATTER_LAG_MS, and never past SCATTER_REACH
constexpr float SCATTER_REACH = 28.0f;
constexpr uint32 SCATTER_LAG_MS = 250;
constexpr uint32 SCATTER_TAIL_MS = 500;

// Shock Vortex hits 12 yd round itself, centre to centre, every 1.5 s from 4.5 s after it appears
constexpr float VORTEX_RADIUS = 12.0f;
constexpr float VORTEX_TRIGGER = VORTEX_RADIUS + 0.5f;
constexpr float VORTEX_CLEAR = VORTEX_RADIUS + BPC_STAND_PARK + 1.5f;
// Empowered Shock Vortex puts a vortex on every player within 30 yd of Valanar, and each one hits everyone
// within 12 yd of that player. Lattice neighbours sit 13.499 yd apart.
constexpr float VORTEX_TARGETS = 30.0f + 1.5f;
constexpr float VORTEX_SHARED = VORTEX_RADIUS + BPC_STAND_PARK;
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
// A victim with no clear spot walks this far at most to a less crowded node
constexpr float BALL_CROWD_WALK = 20.0f;
// The Inferno victim's button goes up this long before the ball reaches it, if the explosion would leave
// it under FLAME_FLOOR of its health
constexpr float FLAME_LEAD_S = 1.5f;
constexpr float FLAME_FLOOR = 0.25f;
// The button still runs this long past the catch, the ball's approach being an estimate
constexpr uint32 FLAME_TAIL_MS = 500;
constexpr uint32 SPELL_DISPERSION = 47585;
// A Shadow Lance lands once its bolt has flown
constexpr uint32 LANCE_FLIGHT_MS = 1000;
constexpr float BOMB_DUTY_RANGE = 60.0f;
// A bomb is in a node's reach while this close, its height included
constexpr float NODE_SHOT_REACH = 30.0f;
constexpr float HUNTER_SHOT_REACH = 35.0f;
constexpr float DISPEL_RANGE = 30.0f;
// The threat brake holds a bot off the empowered prince past BRAKE_CAP of his owner's threat, until it
// falls under BRAKE_RELEASE
constexpr float BRAKE_CAP = 0.9f;
constexpr float BRAKE_RELEASE = 0.8f;
// A bomb falls at its walk speed, 2 yd/s or 3 on heroic, from 20 yd up and explodes within 2 yd of its
// landing; a hit lifts it 2 yd/s for 3 s. It is shot once it is this close to landing.
constexpr float BOMB_SHOOT_LEAD_S = 5.0f;

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
    // Pet owner -> the Kinetic Bomb its pet juggles
    RaidObs::ObsGuidMap<ObjectGuid> petBomb{"bpc.petbomb"};
    RaidObs::ObsGuidSet soak{"bpc.soak"};
    ObjectGuid soakBall;
    // Glittering Sparks carrier -> the bot that dispels it
    RaidObs::ObsGuidMap<ObjectGuid> dispel{"bpc.dispel"};
    RaidObs::ObsGuidSet brake{"bpc.brake"};
    std::unordered_set<ObjectGuid> scatter;
    // Scattering bots on a node somebody else holds, which leaves that holder its node
    std::unordered_set<ObjectGuid> scatterShared;
    // Vortex hits each scattering bot takes where it runs, its own included
    std::unordered_map<ObjectGuid, uint8> scatterHits;
    uint32 scatterUntilMs = 0;
    bool kelesethParked = false;

    uint32 humanCallMs = 0;
    bool humanCallRan = false;

    std::unordered_map<ObjectGuid, Stand> stands;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<BpcState> bpcStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// Players hold no duty, and a selfbot counts as a player here.
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

// The living owner of a prince, a bot on the map
Player* OwnerOf(BpcState const& st, Player* bot, uint8 bit)
{
    for (auto const& [guid, mask] : st.tank)
        if (mask & bit)
            if (Player* owner = ObjectAccessor::FindPlayer(guid))
                if (owner->IsAlive() && IccSameInstance(bot, owner))
                    return owner;

    return nullptr;
}

// Tanks and a DK or paladin stand-in take nuclei and Keleseth back with the class taunt. Any other
// stand-in has to hit a nucleus to take it.
bool GrabsByTaunt(Player* bot)
{
    return IsTankPlayer(bot) || bot->getClass() == CLASS_DEATH_KNIGHT || bot->getClass() == CLASS_PALADIN;
}

// Keleseth casts from 10 yd and turns every hit into threat outside his phase. Guides tank him with a
// warlock, a DK or a paladin can taunt, any other ranged dealer holds him on threat alone. Melee can't
// reach him from his spot.
int8 StandInTier(Player* member)
{
    if (!IsDutyBot(member) || IsTankPlayer(member) || !IsRaidDpsBot(member))
        return -1;

    switch (member->getClass())
    {
        case CLASS_WARLOCK:
            return 0;
        case CLASS_DEATH_KNIGHT:
        case CLASS_PALADIN:
            return 1;
        default:
            return PlayerbotAI::IsRanged(member) ? 2 : -1;
    }
}

Player* PickStandIn(BpcState const& st, std::vector<Player*> const& members)
{
    Player* best = nullptr;
    int8 bestTier = 0;
    for (Player* member : members)
    {
        int8 const tier = StandInTier(member);
        if (tier < 0)
            continue;

        if (OwnedBy(st, member) == BPC_PRINCE_KELESETH)
            return member;

        if (!best || tier < bestTier || (tier == bestTier && member->GetGUID() < best->GetGUID()))
        {
            best = member;
            bestTier = tier;
        }
    }

    return best;
}

// Main tank, then the assist tanks in their order, then guid: Valanar to the first, Keleseth to the second,
// Taldaram to the third or else to the first with Valanar. With one tank in the raid, human or bot,
// Keleseth goes to a stand-in.
void AssignTanks(BpcState& st, std::vector<Player*> const& members)
{
    size_t const tankPlayers = std::count_if(members.begin(), members.end(), &IsTankPlayer);
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
    {
        masks[ranked[0].second->GetGUID()] = BPC_PRINCE_VALANAR | BPC_PRINCE_TALDARAM;
        if (tankPlayers == 1)
            if (Player* standIn = PickStandIn(st, members))
                masks[standIn->GetGUID()] = BPC_PRINCE_KELESETH;
    }

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

bool ReachesSpot(uint8 id, Position const& spot, float reach) { return NodeSpot(id).GetExactDist2d(spot) <= reach; }

bool InValanarZone(uint8 id) { return ReachesSpot(id, ICC_BPC_VALANAR_SPOT, VALANAR_RANGED_ZONE); }

bool InMeleeField(uint8 id) { return NodeSpot(id).GetPositionX() > DAIS_X && InValanarZone(id); }

float RangedReach(Player* member, bool keep)
{
    bool const hunter = member->getClass() == CLASS_HUNTER;
    return keep ? (hunter ? HUNTER_KEEP : RANGED_KEEP) : (hunter ? HUNTER_TAKE : RANGED_TAKE);
}

// Every tank spot in use is reached by half the healers, rounded up, and every healer node by one spot at
// least, from outside Valanar's zone while that holds. With no bot tank any node will do. A healer is
// placed before the ranged and keeps its node, so a phase change never moves one.
void AssignHealers(BpcState& st, std::vector<Player*> const& healers, bool (&used)[BPC_NODE_COUNT + 1])
{
    std::vector<Position> const spots = TankSpotsInUse(st);
    uint8 const needed = static_cast<uint8>((healers.size() + 1) / 2);
    std::vector<uint8> reached(spots.size(), 0);
    auto const reachesAny = [&](uint8 id, float reach)
    {
        return spots.empty() || std::any_of(spots.begin(), spots.end(), [&](Position const& spot)
                                            { return ReachesSpot(id, spot, reach); });
    };
    auto const take = [&](uint8 id)
    {
        used[id] = true;
        for (size_t i = 0; i < spots.size(); ++i)
            reached[i] += ReachesSpot(id, spots[i], HEAL_TAKE) ? 1 : 0;
    };

    std::vector<Player*> open;
    for (Player* healer : healers)
    {
        if (st.scatter.count(healer->GetGUID()))
            continue;

        auto const held = st.slot.find(healer->GetGUID());
        uint8 const id = held != st.slot.end() ? held->second : 0;
        if (NodeUsable(st, id) && !used[id] && !InMeleeField(id) && reachesAny(id, HEAL_KEEP))
            take(id);
        else
            open.push_back(healer);
    }

    for (Player* healer : open)
    {
        uint8 best = 0;
        float bestScore = 0.0f;
        for (uint8 node = 1; node <= BPC_NODE_COUNT; ++node)
        {
            if (used[node] || !NodeUsable(st, node) || InMeleeField(node) || !reachesAny(node, HEAL_TAKE))
                continue;

            float score = healer->GetExactDist2d(NodeSpot(node)) + (InValanarZone(node) ? HEALER_ZONE_COST : 0.0f);
            for (size_t i = 0; i < spots.size(); ++i)
                if (reached[i] < needed && !ReachesSpot(node, spots[i], HEAL_TAKE))
                    score += HEALER_SHORT_COST;

            if (!best || score < bestScore)
            {
                best = node;
                bestScore = score;
            }
        }

        if (best)
        {
            take(best);
            st.slot[healer->GetGUID()] = best;
        }
        else
            st.slot.erase(healer->GetGUID());
    }
}

// The ranged follow the empowered prince's side, off the dais nodes in Valanar's phase
void AssignSlots(BpcState& st, std::vector<Player*> const& members, Unit* anchor)
{
    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || IsTankPlayer(member) || OwnedBy(st, member))
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
        if (itr != st.slot.end() && itr->second >= 1 && itr->second <= BPC_NODE_COUNT && !st.scatterShared.count(guid))
            used[itr->second] = true;
    }

    AssignHealers(st, healers, used);

    bool const valanarSide = anchor && anchor->GetEntry() == NPC_BPC_VALANAR;
    auto const rangedNode = [&](uint8 id) { return NodeUsable(st, id) && !(valanarSide && InMeleeField(id)); };
    for (Player* member : ranged)
    {
        if (st.scatter.count(member->GetGUID()))
            continue;

        uint8 const id = held(member);
        if (rangedNode(id) && !used[id] && anchor && NodeSpot(id).GetExactDist2d(anchor) <= RangedReach(member, true))
        {
            used[id] = true;
            continue;
        }

        uint8 best = 0;
        float bestDistance = 0.0f;
        for (uint8 node = 1; node <= BPC_NODE_COUNT; ++node)
        {
            if (used[node] || !rangedNode(node) || !anchor ||
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
        st.scatterShared.clear();
        st.scatterHits.clear();
        return;
    }

    bool used[BPC_NODE_COUNT + 1] = {};
    for (auto const& entry : st.slot)
        if (entry.second >= 1 && entry.second <= BPC_NODE_COUNT)
            used[entry.second] = true;

    // Where each player will stand when the vortices land, if Valanar's cast reaches it: a node holder on its
    // node, everyone else where it stands now
    std::vector<Position> carriers;
    std::vector<Player*> runners;
    auto const carry = [&](Position const& pos)
    {
        if (valanar->GetExactDist2d(pos) <= VORTEX_TARGETS)
            carriers.push_back(pos);
    };
    for (Player* member : members)
    {
        auto const held = st.slot.find(member->GetGUID());
        if (held != st.slot.end() && held->second >= 1 && held->second <= BPC_NODE_COUNT)
            carry(NodeSpot(held->second));
        else if (IsDutyBot(member) && !IsTankPlayer(member) && !OwnedBy(st, member))
            runners.push_back(member);
        else
            carry(member->GetPosition());
    }

    // The bot's own vortex and its neighbours', plus the ones it adds to them, with `extra` carriers on top
    auto const hits = [&](Position const& pos, std::vector<Position> const& extra)
    {
        bool const own = valanar->GetExactDist2d(pos) <= VORTEX_TARGETS;
        auto const near = [&](std::vector<Position> const& list)
        {
            return std::count_if(list.begin(), list.end(),
                                 [&](Position const& other) { return other.GetExactDist2d(pos) < VORTEX_SHARED; });
        };
        uint8 const count = static_cast<uint8>(near(carriers) + near(extra));
        return std::pair<uint8, uint8>(static_cast<uint8>(count + (own ? 1 : 0)), own ? count : 0);
    };

    int32 const castLeftMs = static_cast<int32>(st.scatterUntilMs - now - SCATTER_TAIL_MS - SCATTER_LAG_MS);
    float const reach = std::min(SCATTER_REACH, BOT_SPEED * static_cast<float>(std::max(0, castLeftMs)) / 1000.0f);

    std::sort(runners.begin(), runners.end(), IccGuidLess());
    std::vector<Position> const none;
    for (size_t index = 0; index < runners.size(); ++index)
    {
        Player* member = runners[index];
        uint8 best = 0;
        float bestDistance = 0.0f;
        for (uint8 node = 1; node <= BPC_NODE_COUNT; ++node)
        {
            if (used[node] || !NodeUsable(st, node))
                continue;

            float const distance = member->GetExactDist2d(NodeSpot(node));
            if (distance <= reach && (!best || distance < bestDistance))
            {
                best = node;
                bestDistance = distance;
            }
        }

        // No free node in reach: the reachable one that costs the raid the fewest hits, shared if it must be,
        // and only if that beats staying put among the bots still to be placed
        bool shared = false;
        if (!best)
        {
            std::vector<Position> unplaced;
            for (size_t later = index + 1; later < runners.size(); ++later)
                if (valanar->GetExactDist2d(runners[later]) <= VORTEX_TARGETS)
                    unplaced.push_back(runners[later]->GetPosition());

            auto const [stayHits, stayAdded] = hits(member->GetPosition(), unplaced);
            uint32 bestCost = stayHits + stayAdded;
            for (uint8 node = 1; node <= BPC_NODE_COUNT; ++node)
            {
                float const distance = member->GetExactDist2d(NodeSpot(node));
                if (!NodeUsable(st, node) || distance > reach)
                    continue;

                auto const [taken, added] = hits(NodeSpot(node), none);
                uint32 const cost = taken + added;
                if (cost < bestCost || (best && cost == bestCost && distance < bestDistance))
                {
                    best = node;
                    bestCost = cost;
                    bestDistance = distance;
                }
            }

            shared = best && used[best];
        }

        if (!best)
        {
            carry(member->GetPosition());
            continue;
        }

        st.scatterHits[member->GetGUID()] = hits(NodeSpot(best), none).first;
        used[best] = true;
        st.slot[member->GetGUID()] = best;
        st.scatter.insert(member->GetGUID());
        if (shared)
            st.scatterShared.insert(member->GetGUID());

        carry(NodeSpot(best));
    }
}

float BombHeight(Creature* bomb)
{
    float const ground = bomb->GetMapHeight(bomb->GetPositionX(), bomb->GetPositionY(), bomb->GetPositionZ());
    return ground > INVALID_HEIGHT ? bomb->GetPositionZ() - ground : 0.0f;
}

// Only a damaging hit procs the bomb's Unstable, never periodic damage: hunters fire instants and Auto Shot,
// mages open on direct spells, a warlock or a shadow priest often on a DoT. A melee dealer hits it only once
// it falls within 5 yd, so it goes last.
uint8 BombTier(Player* member)
{
    if (!PlayerbotAI::IsRanged(member))
        return 3;

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

bool IsBombShooter(BpcState const& st, Player* member)
{
    return IsDutyBot(member) && !IsTankPlayer(member) && !OwnedBy(st, member) && IsRaidDpsBot(member);
}

// Kept while both live, so a pet stays under its bomb
void AssignPetBombs(BpcState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<Creature*> const bombs = Resolve(bot, st.bombs);
    std::unordered_set<ObjectGuid> live;
    for (Creature* bomb : bombs)
        live.insert(bomb->GetGUID());

    auto const petOf = [](Player* member) -> Guardian*
    {
        Guardian* pet = member->GetGuardianPet();
        return pet && pet->IsAlive() && pet->GetReactState() != REACT_PASSIVE ? pet : nullptr;
    };

    std::unordered_map<ObjectGuid, Player*> keepers;
    for (Player* member : members)
        if (IsDutyBot(member) && !IsTankPlayer(member) && petOf(member))
            keepers[member->GetGUID()] = member;

    std::unordered_set<ObjectGuid> covered;
    std::vector<ObjectGuid> ended;
    for (auto const& [owner, bomb] : st.petBomb)
    {
        if (!keepers.count(owner) || !live.count(bomb))
            ended.push_back(owner);
        else
            covered.insert(bomb);
    }

    for (ObjectGuid const& owner : ended)
        st.petBomb.erase(owner);

    auto const tier = [](Player* member) -> uint8
    {
        switch (member->getClass())
        {
            case CLASS_HUNTER:
                return 0;
            case CLASS_WARLOCK:
                return 1;
            case CLASS_DEATH_KNIGHT:
                return 2;
            default:
                return 3;
        }
    };

    for (Creature* bomb : bombs)
    {
        if (covered.count(bomb->GetGUID()))
            continue;

        Player* best = nullptr;
        uint8 bestTier = 0;
        float bestDistance = 0.0f;
        for (auto const& [guid, member] : keepers)
        {
            if (st.petBomb.count(guid))
                continue;

            float const distance = petOf(member)->GetExactDist2d(bomb);
            if (distance > BOMB_DUTY_RANGE)
                continue;

            uint8 const rank = tier(member);
            if (!best || rank < bestTier || (rank == bestTier && distance < bestDistance) ||
                (rank == bestTier && distance == bestDistance && guid < best->GetGUID()))
            {
                best = member;
                bestTier = rank;
                bestDistance = distance;
            }
        }

        if (best)
            st.petBomb[best->GetGUID()] = bomb->GetGUID();
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
        if (IsBombShooter(st, member))
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
    float const px = pos.GetPositionX() - from.GetPositionX();
    float const py = pos.GetPositionY() - from.GetPositionY();
    float const length2 = dx * dx + dy * dy;
    float const t = length2 > 0.001f ? std::clamp((px * dx + py * dy) / length2, 0.0f, 1.0f) : 0.0f;
    return std::hypot(t * dx - px, t * dy - py);
}

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
    // The melee nearest its path first, then the rest by the same distance
    std::vector<std::pair<float, Player*>> ranked;
    for (Player* member : members)
    {
        if (member == victim || !IsDutyBot(member) || IsTankPlayer(member) || OwnedBy(st, member) ||
            st.bomb.count(member->GetGUID()))
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

bool CanPayFor(Player* member, SpellInfo const* info)
{
    // POWER_HEALTH is -2 and Powers is signed, hence the cast.
    if (info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(member->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(member, info->GetSchoolMask());
}

// The magic dispel the member can land on the carrier from where it stands. Shamans cleanse no magic.
char const* ReadyDispel(Player* member, Player* carrier)
{
    char const* name = member->getClass() == CLASS_PRIEST    ? "dispel magic"
                       : member->getClass() == CLASS_PALADIN ? "cleanse"
                                                             : nullptr;
    PlayerbotAI* memberAI = name ? GET_PLAYERBOT_AI(member) : nullptr;
    if (!memberAI || !IsDutyBot(member) || !member->IsAlive() || member->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
        member->HasUnitFlag(UNIT_FLAG_SILENCED) || member->GetExactDist(carrier) > DISPEL_RANGE)
        return nullptr;

    uint32 const spellId = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return info && member->HasSpell(spellId) && !member->HasSpellCooldown(spellId) && CanPayFor(member, info) &&
                   info->CheckShapeshift(member->GetShapeshiftForm()) == SPELL_CAST_OK &&
                   member->IsWithinLOSInMap(carrier)
               ? name
               : nullptr;
}

// One bot per Glittering Sparks carrier, the nearest that can land it now
void UpdateDispel(BpcState& st, Player* bot, std::vector<Player*> const& members)
{
    uint32 const sparks = sSpellMgr->GetSpellIdForDifficulty(SPELL_BPC_GLITTERING_SPARKS, bot);
    std::unordered_set<ObjectGuid> carriers;
    for (Player* member : members)
        if (member->HasAura(sparks))
            carriers.insert(member->GetGUID());

    std::vector<ObjectGuid> cleared;
    for (auto const& entry : st.dispel)
        if (!carriers.count(entry.first))
            cleared.push_back(entry.first);

    for (ObjectGuid const& guid : cleared)
        st.dispel.erase(guid);

    for (Player* carrier : members)
    {
        if (!carriers.count(carrier->GetGUID()))
            continue;

        auto const held = st.dispel.find(carrier->GetGUID());
        if (held != st.dispel.end())
            if (Player* current = ObjectAccessor::FindPlayer(held->second))
                if (IccSameInstance(carrier, current) && ReadyDispel(current, carrier))
                    continue;

        Player* best = nullptr;
        for (Player* member : members)
            if (ReadyDispel(member, carrier) &&
                (!best || member->GetExactDist(carrier) < best->GetExactDist(carrier) ||
                 (member->GetExactDist(carrier) == best->GetExactDist(carrier) && member->GetGUID() < best->GetGUID())))
                best = member;

        if (best)
            st.dispel[carrier->GetGUID()] = best->GetGUID();
        else
            st.dispel.erase(carrier->GetGUID());
    }
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
    AssignPetBombs(st, bot, members);
    AssignSoak(st, bot, members);
    UpdateDispel(st, bot, members);

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
        // Melee owners fight a parked Keleseth where he stands, a ranged one keeps the spot: he casts from 10 yd
        bool const letGo = owned == BPC_PRINCE_KELESETH && st.kelesethParked && !PlayerbotAI::IsRanged(bot);
        if (holdingAll && !letGo && !InVortex(st, spot, VORTEX_CLEAR))
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

    // A melee dealer walks under it first
    float lead = BOMB_SHOOT_LEAD_S;
    if (!PlayerbotAI::IsRanged(bot))
        lead += bot->GetExactDist2d(bomb) / BOT_SPEED;

    return BombHeight(bomb) <= bomb->GetSpeed(MOVE_WALK) * lead ? bomb : nullptr;
}

Creature* DutyBomb(BpcState const& st, Player* bot, uint8& node)
{
    auto const duty = st.bomb.find(bot->GetGUID());
    auto const held = st.slot.find(bot->GetGUID());
    node = held != st.slot.end() && held->second >= 1 && held->second <= BPC_NODE_COUNT ? held->second : 0;
    Creature* bomb = duty != st.bomb.end() ? bot->GetMap()->GetCreature(duty->second) : nullptr;
    return bomb && bomb->IsAlive() ? bomb : nullptr;
}

// A duty bomb out of range of the bot's node keeps it off that node until the bomb goes
bool BombAway(BpcState const& st, Player* bot)
{
    uint8 node = 0;
    Creature* bomb = DutyBomb(st, bot, node);
    return bomb && node && NodeSpot(node).GetExactDist2d(bomb) > RangedReach(bot, true);
}

// The shot reaches the bomb from the bot's node, its height included, so the bot holds its node through a
// cast window
bool NodeReachesBomb(BpcState const& st, Player* bot)
{
    uint8 node = 0;
    Creature* bomb = DutyBomb(st, bot, node);
    float const reach = bot->getClass() == CLASS_HUNTER ? HUNTER_SHOT_REACH : NODE_SHOT_REACH;
    return bomb && node && PlayerbotAI::IsRanged(bot) && NodeSpot(node).GetExactDist(bomb) <= reach;
}

// A bomb shot it can't take from its node, a ball on the bot or a soak takes it off its node for the while
bool OnSideDuty(BpcState const& st, Player* bot)
{
    return (ShotFor(st, bot) && !NodeReachesBomb(st, bot)) || BombAway(st, bot) || BallChasing(st, bot) ||
           st.soak.count(bot->GetGUID());
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

// A stand-in with no taunt takes a nucleus by hitting it: any direct damage moves it on, periodic doesn't
Creature* NucleusGrab(BpcState const& st, Player* bot)
{
    if (!IsNucleusOwner(st, bot) || GrabsByTaunt(bot) ||
        bot->GetAuraCount(SPELL_BPC_SHADOW_RESONANCE) >= BPC_NUCLEUS_STACKS)
        return nullptr;

    Creature* nucleus = NearestLooseNucleus(st, bot);
    return nucleus && bot->GetExactDist(nucleus) <= TAUNT_RANGE && bot->IsWithinLOSInMap(nucleus) ? nucleus : nullptr;
}

Unit* ResolveTarget(PlayerbotAI* botAI, BpcState& st)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    if (Creature* nucleus = NucleusGrab(st, bot))
    {
        NoteDerived(bot, "bpc.target", "nucleus");
        NoteDerived(bot, "bpc.nucleus", "hit " + std::to_string(bot->GetAuraCount(SPELL_BPC_SHADOW_RESONANCE)));
        return nucleus;
    }

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

    Creature* empowered = EmpoweredPrince(bot);
    bool const braked = empowered && st.brake.count(bot->GetGUID());
    if (Unit* skull = GetRaidHumanSkull(botAI, &IsPrinceUnit))
        if (!braked || skull != empowered)
        {
            NoteDerived(bot, "bpc.target", "skull");
            return skull;
        }

    if (braked)
    {
        NoteDerived(bot, "bpc.target", "brake");
        return nullptr;
    }

    NoteDerived(bot, "bpc.target", empowered ? "empowered" : "none");
    return empowered;
}

// Keleseth's owner walks toward a nucleus out of reach while short of stacks, outside his own phase: that
// is the phase the stacks are for, and he stays put on his owner through it. A taunter goes only with its
// taunt ready, or it arrives, can't taunt and its stand walks it straight back.
Creature* NucleusToWalkTo(BpcState const& st, PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsNucleusOwner(st, bot) || bot->GetAuraCount(SPELL_BPC_SHADOW_RESONANCE) >= BPC_NUCLEUS_STACKS ||
        (GrabsByTaunt(bot) && !ClassTauntReady(botAI)))
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

enum class SoakPlace : uint8
{
    None,
    OnPath,
    Walk
};

// A soaker of the live Ball of Inferno Flame: already on its path and clear of the explosion round the
// victim, a point of the path to walk to, or neither once the ball has outrun it
SoakPlace FindSoakPlace(BpcState const& st, Player* bot, Position& spot)
{
    if (!st.soak.count(bot->GetGUID()))
        return SoakPlace::None;

    Player* victim = nullptr;
    Creature* ball = InfernoBall(st, bot, victim);
    if (!ball || ball->GetGUID() != st.soakBall)
        return SoakPlace::None;

    Position const from = ball->GetPosition();
    Position const to = victim->GetPosition();
    if (DistanceToSegment(from, to, bot->GetPosition()) <= FLARE_REACH - 2.0f &&
        bot->GetExactDist2d(victim) >= BALL_CLEAR)
        return SoakPlace::OnPath;

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
            return SoakPlace::Walk;
        }
    }

    return SoakPlace::None;
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

    // Taunts on a nucleus never diminish. A stand-in with no taunt hits it through its resolver.
    if (bot->GetExactDist(nucleus) > TAUNT_RANGE || !bot->IsWithinLOSInMap(nucleus) || !GrabsByTaunt(bot))
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

bool GetBpcBallWalk(PlayerbotAI* botAI, Position& spot)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    Creature* ball = st ? BallChasing(*st, bot) : nullptr;
    if (!ball)
        return false;

    char const* kind = ball->GetEntry() == NPC_BPC_BALL_OF_INFERNO_FLAME ? "inferno" : "plain";
    std::vector<Position> others;
    std::vector<HazardCircle> hazards;
    for (Player* member : MembersOnMap(bot))
        if (member != bot)
        {
            others.push_back(member->GetPosition());
            hazards.emplace_back(member->GetPosition(), BALL_CLEAR);
        }
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
    if (found.GetPositionX() != 0.0f || found.GetPositionY() != 0.0f)
    {
        NoteDerived(bot, "bpc.ball", std::string(kind) + " step");
        spot = found;
        return true;
    }

    // No spot clear of everyone: the nearby node with the fewest players in the explosion, if it beats here
    auto const crowd = [&](Position const& pos)
    {
        return std::count_if(others.begin(), others.end(),
                             [&](Position const& other) { return other.GetExactDist2d(pos) < BALL_CLEAR; });
    };
    auto bestCrowd = crowd(bot->GetPosition());
    uint8 best = 0;
    float bestDistance = 0.0f;
    for (uint8 node = 1; node <= BPC_NODE_COUNT; ++node)
    {
        float const distance = bot->GetExactDist2d(NodeSpot(node));
        if (distance > BALL_CROWD_WALK || InVortex(*st, NodeSpot(node), VORTEX_CLEAR))
            continue;

        auto const here = crowd(NodeSpot(node));
        if (here < bestCrowd || (best && here == bestCrowd && distance < bestDistance))
        {
            best = node;
            bestCrowd = here;
            bestDistance = distance;
        }
    }

    if (!best)
    {
        NoteDerived(bot, "bpc.ball", std::string(kind) + " none");
        return false;
    }

    NoteDerived(bot, "bpc.ball", std::string(kind) + " fewest");
    spot = NodeSpot(best);
    return true;
}

bool GetBpcSoakWalk(PlayerbotAI* botAI, Position& spot)
{
    BpcState* st = LiveState(botAI);
    return st && FindSoakPlace(*st, botAI->GetBot(), spot) == SoakPlace::Walk;
}

bool IsBpcSoakHeld(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    Position spot;
    return st && FindSoakPlace(*st, botAI->GetBot(), spot) == SoakPlace::OnPath;
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

Unit* GetBpcPetTarget(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    Player* bot = botAI->GetBot();
    auto const duty = st->petBomb.find(bot->GetGUID());
    Creature* bomb = duty != st->petBomb.end() ? bot->GetMap()->GetCreature(duty->second) : nullptr;
    return bomb && bomb->IsAlive() ? bomb : GetBpcTarget(botAI);
}

Unit* GetBpcDirectHitTarget(PlayerbotAI* botAI)
{
    // The resolver hands out a bomb only as a shot and a nucleus only as a grab
    Unit* pick = GetBpcTarget(botAI);
    uint32 const entry = pick ? pick->GetEntry() : 0;
    return entry == NPC_BPC_KINETIC_BOMB || entry == NPC_BPC_DARK_NUCLEUS ? pick : nullptr;
}

bool IsBpcDirectHitPick(PlayerbotAI* botAI) { return GetBpcDirectHitTarget(botAI) != nullptr; }

Creature* GetBpcBrakeTarget(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    return st && st->brake.count(bot->GetGUID()) ? EmpoweredPrince(bot) : nullptr;
}

bool IsBpcBraked(PlayerbotAI* botAI) { return GetBpcBrakeTarget(botAI) != nullptr; }

bool GetBpcLanceHit(PlayerbotAI* botAI, RaidPredictedHit& hit)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || IsTankPlayer(bot) || !IsNucleusOwner(*st, bot))
        return false;

    Creature* keleseth = LivePrince(bot, BPC_PRINCE_KELESETH);
    Spell* cast = keleseth ? keleseth->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
    if (!cast || cast->m_targets.GetUnitTargetGUID() != bot->GetGUID())
        return false;

    uint32 const id = cast->GetSpellInfo()->Id;
    if (id != sSpellMgr->GetSpellIdForDifficulty(SPELL_BPC_SHADOW_LANCE, keleseth) &&
        id != sSpellMgr->GetSpellIdForDifficulty(SPELL_BPC_EMPOWERED_SHADOW_LANCE, keleseth))
        return false;

    SpellEffectInfo const& effect = cast->GetSpellInfo()->Effects[EFFECT_0];
    hit = RaidPredictedHit();
    hit.startMs = getMSTime() + static_cast<uint32>(std::max(0, cast->GetCastTimeRemaining()));
    hit.durationMs = LANCE_FLIGHT_MS;
    hit.damage = static_cast<uint32>(std::max(effect.BasePoints + (effect.DieSides + 1) / 2, 0));
    hit.school = SPELL_SCHOOL_MASK_SHADOW;
    // An immunity drops it off Keleseth, so only a lethal lance gets one
    hit.keepVictim = true;
    return true;
}

uint32 GetBpcFlameDefensive(PlayerbotAI* botAI)
{
    BpcState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    Creature* ball = st ? BallChasing(*st, bot) : nullptr;
    if (!st || !ball || ball->GetEntry() != NPC_BPC_BALL_OF_INFERNO_FLAME ||
        ball->GetExactDist2d(bot) > BALL_SPEED * FLAME_LEAD_S)
    {
        if (st)
            NoteDerived(bot, "bpc.flame", "idle");

        return 0;
    }

    // Flames plus what the ball's stacks still add to it
    SpellInfo const* flames = sSpellMgr->GetSpellInfo(sSpellMgr->GetSpellIdForDifficulty(SPELL_BPC_FLAMES, ball));
    float damage = flames ? static_cast<float>(std::max(flames->Effects[EFFECT_0].BasePoints +
                                                            (flames->Effects[EFFECT_0].DieSides + 1) / 2,
                                                        0))
                          : 0.0f;
    uint32 const procId = sSpellMgr->GetSpellIdForDifficulty(SPELL_BPC_BALL_OF_FLAMES_PROC, ball);
    if (AuraEffect const* bonus = ball->GetAuraEffect(procId, EFFECT_1))
        damage += static_cast<float>(std::max(bonus->GetAmount(), 0));

    if (static_cast<float>(bot->GetHealth()) - damage >= FLAME_FLOOR * static_cast<float>(bot->GetMaxHealth()))
    {
        NoteDerived(bot, "bpc.flame", "safe");
        return 0;
    }

    uint32 const needMs = static_cast<uint32>(ball->GetExactDist2d(bot) / BALL_SPEED * 1000.0f) + FLAME_TAIL_MS;
    uint32 spellId = NextMagicDefensive(botAI, bot, nullptr, needMs);
    if (!spellId && bot->getClass() == CLASS_PRIEST && bot->HasSpell(SPELL_DISPERSION) &&
        botAI->CanCastSpell(SPELL_DISPERSION, bot))
        spellId = SPELL_DISPERSION;

    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    NoteDerived(bot, "bpc.flame", info ? info->SpellName[0] : "none");
    return spellId;
}

Player* GetBpcDispelTarget(PlayerbotAI* botAI, char const*& spell)
{
    BpcState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    Player* bot = botAI->GetBot();
    for (auto const& [carrierGuid, dispeller] : st->dispel)
    {
        if (dispeller != bot->GetGUID())
            continue;

        Player* carrier = ObjectAccessor::FindPlayer(carrierGuid);
        if (!carrier || !IccSameInstance(bot, carrier))
            continue;

        spell = ReadyDispel(bot, carrier);
        if (spell)
            return carrier;
    }

    return nullptr;
}

namespace
{
// Past BRAKE_CAP of the threat of the empowered prince's owner, or of his victim when no owner is on his
// threat list, until under BRAKE_RELEASE. A bot holding him away from his owner is braked too. Read from
// the threat list: the "threat" value is a uint8 percentage and wraps past 255%.
void UpdateBrake(BpcState& st, Player* bot)
{
    bool hold = false;
    Creature* empowered = EmpoweredPrince(bot);
    uint8 const bit = PrinceBit(empowered);
    if (empowered && IsDutyBot(bot) && !PlayerbotAI::IsHeal(bot) && !(OwnedBy(st, bot) & bit))
    {
        Unit* reference = OwnerOf(st, bot, bit);
        if (!reference || empowered->GetThreatMgr().GetThreat(reference) <= 0.0f)
            reference = empowered->GetVictim();

        if (reference && reference != bot)
        {
            float const top = empowered->GetThreatMgr().GetThreat(reference);
            float const own = empowered->GetThreatMgr().GetThreat(bot);
            float const share = st.brake.count(bot->GetGUID()) ? BRAKE_RELEASE : BRAKE_CAP;
            hold = top > 0.0f && own > 0.0f && own >= share * top;
        }
    }

    if (hold)
        st.brake.insert(bot->GetGUID());
    else
        st.brake.erase(bot->GetGUID());
}

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
    if (!RaidObs::Active())
        return;

    if (st.bomb.count(bot->GetGUID()))
        NoteDerived(bot, "bpc.juggle", ShotFor(st, bot) ? "shoot" : "wait");

    auto const hits = st.scatterHits.find(bot->GetGUID());
    NoteDerived(bot, "bpc.hits", hits != st.scatterHits.end() ? std::to_string(hits->second) : "none");
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
    UpdateBrake(st, bot);
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
