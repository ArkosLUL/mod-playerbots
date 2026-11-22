/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_XT002.h"

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "Timer.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

#include <algorithm>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace EncounterHelpers;

// XT-002 anchors. The tank spot sits just behind XT's spawn; the ranged spot keeps a 30 yd caster in range.
const Position ULDUAR_XT002_SPAWN = Position(886.28f, -12.05f, 409.6f);
const Position ULDUAR_XT002_MAINTANK_SPOT = Position(895.82f, -12.53954f, 409.68756f);
const Position ULDUAR_XT002_RANGED_SPOT = Position(866.0f, -12.5f, 409.8f);
// South-west so the 8 yd splash reaches neither the formation nor a bomb cell, and 38 yd from the melee stack,
// inside a 9 s run. North is closer but findSmoothPath answers NOPATH there from every melee position.
const Position ULDUAR_XT002_SEARING_LIGHT_SPOT = Position(846.0f, -22.0f, 409.597f);
// Separate origins so melee and ranged carriers never drop Void Zones on each other, each the grid corner
// nearest the raid. The northern one is the southern reflected about the formation centre.
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE = Position(871.5199f, -42.04216f, 409.80377f);
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_SOUTH = Position(837.0746f, -41.01061f, 409.80362f);
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_NORTH = Position(837.0746f, 28.01061f, 409.811f);

// XT and the Heart go UNIT_FLAG_NOT_SELECTABLE for part of the fight and AttackersValue::IsPossibleTarget
// rejects the flag, so scan the raw nearby-npc list to keep them visible through the Heart phases.
static Unit* GetFirstAliveNpcByEntry(PlayerbotAI* botAI, GuidVector const& npcs, uint32 entry)
{
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == entry)
            return unit;
    }

    return nullptr;
}

static Unit* GetFirstAliveNpcByEntry(PlayerbotAI* botAI, uint32 entry)
{
    return GetFirstAliveNpcByEntry(botAI, botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get(),
                                   entry);
}

Unit* GetXT002(PlayerbotAI* botAI) { return GetFirstAliveNpcByEntry(botAI, NPC_XT002); }

Unit* GetXT002ExposedHeart(PlayerbotAI* botAI)
{
    Unit* heart = GetFirstAliveNpcByEntry(botAI, NPC_HEART_OF_DECONSTRUCTOR);
    if (!heart || heart->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return nullptr;

    // Only worth hitting while it channels Exposed Heart: that aura is what moves its damage to XT.
    return heart->HasAura(SPELL_XT002_EXPOSED_HEART) ? heart : nullptr;
}

bool IsXT002Submerged(Unit* xt002)
{
    if (!xt002)
        return false;

    return xt002->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || xt002->HasAura(SPELL_XT002_SUBMERGE);
}

uint32 GetXT002SearingLightSpellId(Player* bot)
{
    return sSpellMgr->GetSpellIdForDifficulty(SPELL_XT002_SEARING_LIGHT, bot);
}

uint32 GetXT002HeartbreakSpellId(Player* bot)
{
    return sSpellMgr->GetSpellIdForDifficulty(SPELL_XT002_HEARTBREAK, bot);
}

uint32 GetXT002GravityBombSpellId(Player* bot)
{
    return sSpellMgr->GetSpellIdForDifficulty(SPELL_XT002_GRAVITY_BOMB, bot);
}

static std::string DescribeXT002TankDuty(XT002TankRoles const& roles, Player* bot)
{
    if (roles.holder == bot && roles.pummeller == bot)
        return "holds XT, pummellers";
    if (roles.holder == bot)
        return "holds XT";
    return roles.pummeller == bot ? "pummellers" : "none";
}

XT002TankRoles GetXT002TankRoles(Player* bot, Unit* xt002)
{
    // Living tanks ranked like GetGroupAssistTank: flagged main tank, then raid assistants, then the rest.
    // Every fallback below takes the first, and the Pummellers go to the first that isn't the holder.
    std::vector<Player*> tanks;
    if (Group* group = bot->GetGroup())
    {
        ObjectGuid const flagged = PlayerbotAI::GetMainTankGuid(group);
        std::vector<Player*> rest;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId() ||
                !PlayerbotAI::IsTank(member))
                continue;

            if (member->GetGUID() == flagged)
                tanks.insert(tanks.begin(), member);
            else if (group->IsAssistant(member->GetGUID()))
                tanks.push_back(member);
            else
                rest.push_back(member);
        }

        tanks.insert(tanks.end(), rest.begin(), rest.end());
    }
    else if (bot->IsAlive() && PlayerbotAI::IsTank(bot))
        tanks.push_back(bot);

    XT002TankRoles roles;
    if (tanks.empty())
        return roles;

    if (xt002)
    {
        // Submerged he attacks nobody, but the threat list keeps picking whom he turns on once he stands.
        Unit* victim = xt002->GetVictim();
        if (!victim && xt002->CanHaveThreatList())
            victim = xt002->GetThreatMgr().GetLastVictim();

        auto const held = std::find(tanks.begin(), tanks.end(), victim);
        if (held != tanks.end())
            roles.holder = *held;
        else
        {
            // XT on a non-tank, e.g. right after his holder died: the tank with most threat gets him back first.
            float most = 0.0f;
            for (Player* tank : tanks)
            {
                float const threat = xt002->GetThreatMgr().GetThreat(tank);
                if (threat > most)
                {
                    most = threat;
                    roles.holder = tank;
                }
            }
        }
    }

    if (!roles.holder)
        roles.holder = tanks.front();

    for (Player* tank : tanks)
    {
        if (tank != roles.holder)
        {
            roles.pummeller = tank;
            break;
        }
    }

    if (!roles.pummeller)
        roles.pummeller = roles.holder;

    // Tanks only: a hunter or rogue reading the roles for its redirect has no duty to note.
    if (RaidObs::Active() && PlayerbotAI::IsTank(bot))
        RaidObs::NoteDerived(bot, "xt002.tank", DescribeXT002TankDuty(roles, bot));

    return roles;
}

bool IsXT002PummellerTank(Player* bot, Unit* xt002)
{
    return PlayerbotAI::IsTank(bot) && GetXT002TankRoles(bot, xt002).pummeller == bot;
}

namespace
{
struct XT002BuffTankState
{
    RaidObs::ObsValue<ObjectGuid> tank{"xt002.buffs"};
    ObjectGuid candidate;  // a new holder waiting out the settle time
    uint32 candidateSinceMs = 0;
    uint32 lastAskedMs = 0;
};

RaidInstanceState<XT002BuffTankState> xt002BuffTankStates;

Player* FindLivingMember(Group* group, ObjectGuid const& guid)
{
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->GetGUID() == guid)
            return member->IsAlive() ? member : nullptr;
    }

    return nullptr;
}
}  // namespace

Player* GetXT002BuffTank(Player* bot, Unit* xt002)
{
    Group* group = bot->GetGroup();
    if (!group || !xt002 || !xt002->IsInCombat() || !bot->GetInstanceId())
        return nullptr;

    Player* holder = GetXT002TankRoles(bot, xt002).holder;
    if (!holder)
        return nullptr;

    uint32 const now = getMSTime();
    XT002BuffTankState& state = xt002BuffTankStates.For(bot->GetInstanceId());

    // first ask of a pull
    if (!state.lastAskedMs || getMSTimeDiff(state.lastAskedMs, now) > ULDUAR_XT002_BUFF_TANK_STALE_MS)
    {
        state.tank = holder->GetGUID();
        state.candidate.Clear();
    }
    state.lastAskedMs = now;

    if (holder->GetGUID() == state.tank.Get())
        state.candidate.Clear();
    else
    {
        if (holder->GetGUID() != state.candidate)
        {
            state.candidate = holder->GetGUID();
            state.candidateSinceMs = now;
        }

        if (!FindLivingMember(group, state.tank.Get()) ||
            getMSTimeDiff(state.candidateSinceMs, now) >= ULDUAR_XT002_BUFF_TANK_SETTLE_MS)
        {
            state.tank = holder->GetGUID();
            state.candidate.Clear();
        }
    }

    return FindLivingMember(group, state.tank.Get());
}

Player* GetXT002BuffTarget(PlayerbotAI* botAI, std::string const& spell)
{
    Player* tank = GetXT002BuffTank(botAI->GetBot(), GetXT002(botAI));
    if (!tank)
        return nullptr;

    // Same owner rule as the class nodes: Beacon and Renew count only this healer's copy, the shields anyone's.
    bool const ownOnly = spell == "beacon of light" || spell == "renew";
    if (botAI->HasAura(spell, tank, false, ownOnly))
        return nullptr;

    if (spell == "power word: shield" && botAI->HasAura("weakened soul", tank))
        return nullptr;

    return botAI->CanCastSpell(spell, tank) ? tank : nullptr;
}

namespace
{
struct XT002ThreatBrakeState
{
    RaidObs::ObsGuidSet braked{"xt002.brake"};
    // Per bot, the ms its brake was last worked out: the multiplier asks once per candidate action.
    std::unordered_map<ObjectGuid, uint32> readMs;
};

RaidInstanceState<XT002ThreatBrakeState> xt002ThreatBrakeStates;
}  // namespace

bool IsXT002ThreatBraked(Player* bot, Unit* xt002)
{
    if (!bot || !bot->GetInstanceId() || !IsRaidDpsBot(bot))
        return false;

    XT002ThreatBrakeState& state = xt002ThreatBrakeStates.For(bot->GetInstanceId());
    ObjectGuid const guid = bot->GetGUID();
    bool const held = state.braked.count(guid) != 0;

    uint32 const now = getMSTime();
    auto const stamp = state.readMs.find(guid);
    if (stamp != state.readMs.end() && stamp->second == now)
        return held;

    state.readMs[guid] = now;

    bool brake = false;
    if (bot->IsAlive() && xt002 && xt002->IsAlive() && xt002->IsInCombat() && xt002->CanHaveThreatList())
    {
        if (Player* holder = GetXT002TankRoles(bot, xt002).holder)
        {
            // Off the threat list, not the "threat" value: that one is a uint8 share and wraps past 255%.
            ThreatManager& threat = xt002->GetThreatMgr();
            float const share = held ? ULDUAR_XT002_THREAT_RELEASE : ULDUAR_XT002_THREAT_CAP;
            brake = threat.GetThreat(bot) >= share * threat.GetThreat(holder);
        }
    }

    if (brake && !held)
        state.braked.insert(guid);
    else if (!brake && held)
        state.braked.erase(guid);

    return brake;
}

namespace
{
struct XT002BombHealState
{
    // bomb carrier -> the healer sent to it
    RaidObs::ObsGuidMap<ObjectGuid> healers{"xt002.bombheal"};
};

RaidInstanceState<XT002BombHealState> xt002BombHealStates;

Player* FindByGuid(std::vector<Player*> const& players, ObjectGuid const& guid)
{
    auto const it = std::find_if(players.begin(), players.end(),
                                 [&guid](Player* player) { return player->GetGUID() == guid; });
    return it != players.end() ? *it : nullptr;
}

// Bots only, and never one carrying either debuff: its own carrier walk wins.
bool CanGoToXT002BombCarrier(Player* healer)
{
    return GET_PLAYERBOT_AI(healer) && !healer->HasAura(GetXT002GravityBombSpellId(healer)) &&
           !healer->HasAura(GetXT002SearingLightSpellId(healer));
}
}  // namespace

std::vector<Player*> GetXT002BombCarriers(Player* bot)
{
    std::vector<Player*> carriers;
    Group* group = bot->GetGroup();
    if (!group)
        return carriers;

    uint32 const bomb = GetXT002GravityBombSpellId(bot);
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->GetMapId() == bot->GetMapId() && member->HasAura(bomb))
            carriers.push_back(member);
    }

    return carriers;
}

Player* GetXT002BombHealCarrier(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group || !bot->GetInstanceId() || !PlayerbotAI::IsHeal(bot))
        return nullptr;

    // Carriers first: there's one every 16-20 s, so most ticks end here.
    std::vector<Player*> const carriers = GetXT002BombCarriers(bot);
    XT002BombHealState* state = xt002BombHealStates.Find(bot->GetInstanceId());
    if (carriers.empty() && (!state || state->healers.empty()))
        return nullptr;

    if (!state)
        state = &xt002BombHealStates.For(bot->GetInstanceId());

    // Real players count as cover for a carrier in their range, they just never get sent.
    std::vector<Player*> healers;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->GetMapId() == bot->GetMapId() && PlayerbotAI::IsHeal(member))
            healers.push_back(member);
    }

    std::vector<ObjectGuid> released;
    for (auto const& [carrier, healer] : state->healers)
    {
        Player* sent = FindByGuid(healers, healer);
        if (!FindByGuid(carriers, carrier) || !sent || !CanGoToXT002BombCarrier(sent))
            released.push_back(carrier);
    }

    for (ObjectGuid const& carrier : released)
        state->healers.erase(carrier);

    float const healRange = sPlayerbotAIConfig.HealDistance;
    for (Player* carrier : carriers)
    {
        if (state->healers.count(carrier->GetGUID()))
            continue;

        // A healer carrying the bomb covers itself.
        bool const covered = std::any_of(healers.begin(), healers.end(), [carrier, healRange](Player* healer)
                                         { return healer->GetExactDist2d(carrier) <= healRange; });
        if (covered)
            continue;

        Player* nearest = nullptr;
        float nearestDistance = 0.0f;
        for (Player* healer : healers)
        {
            if (!CanGoToXT002BombCarrier(healer))
                continue;

            bool const sentElsewhere =
                std::any_of(state->healers.begin(), state->healers.end(),
                            [healer](auto const& latch) { return latch.second == healer->GetGUID(); });
            if (sentElsewhere)
                continue;

            float const distance = healer->GetExactDist2d(carrier);
            if (!nearest || distance < nearestDistance)
            {
                nearest = healer;
                nearestDistance = distance;
            }
        }

        if (nearest)
            state->healers[carrier->GetGUID()] = nearest->GetGUID();
    }

    for (auto const& [carrier, healer] : state->healers)
        if (healer == bot->GetGUID())
            return FindByGuid(carriers, carrier);

    return nullptr;
}

Player* GetXT002BombHealTarget(Player* bot)
{
    Player* carrier = GetXT002BombHealCarrier(bot);
    if (!carrier || carrier->GetHealthPct() > ULDUAR_XT002_BOMB_HEAL_TOP_PCT)
        return nullptr;

    return bot->GetExactDist2d(carrier) <= sPlayerbotAIConfig.HealDistance ? carrier : nullptr;
}

bool IsXT002AddEngageable(Unit* xt002, Unit* unit)
{
    if (!unit)
        return false;

    if (!xt002)
        return true;

    return unit->GetExactDist2d(xt002) <= ULDUAR_XT002_ADD_LEASH_RADIUS;
}

Unit* GetXT002EngageableAdd(PlayerbotAI* botAI, Player* bot, uint32 entry, float botReach)
{
    Unit* nearest = nullptr;
    float nearestDistance = 0.0f;

    GuidVector const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();

    // Same unit GetXT002 would return, read off the list already in hand instead of a scan per add.
    Unit* xt002 = GetFirstAliveNpcByEntry(botAI, npcs, NPC_XT002);

    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
            continue;

        if (!IsXT002AddEngageable(xt002, unit))
            continue;

        float const distance = unit->GetExactDist2d(bot);
        if (distance > botReach)
            continue;

        if (!nearest || distance < nearestDistance)
        {
            nearest = unit;
            nearestDistance = distance;
        }
    }

    return nearest;
}

bool IsXT002AddEvading(Unit* unit)
{
    Creature const* creature = unit ? unit->ToCreature() : nullptr;
    return creature && creature->IsEvadingAttacks();
}

// Formation roster, in the order every bot derives, with hunters split off for the arc. The dead keep their
// slots: indexing by the living re-seats everyone behind a corpse mid-fight.
static bool BuildXT002FormationMembers(Player* bot, std::vector<Player*>& ring, std::vector<Player*>& hunters)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ring.clear();
    hunters.clear();

    // Heal flag read once per member, not on both sides of every compare in the sort.
    std::vector<std::pair<bool, Player*>> ranked;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || PlayerbotAI::IsTank(member))
            continue;

        bool const heal = PlayerbotAI::IsHeal(member);
        if (!heal && !PlayerbotAI::IsRangedDps(member))
            continue;

        if (!heal && member->getClass() == CLASS_HUNTER)
            hunters.push_back(member);
        else
            ranked.emplace_back(heal, member);
    }

    if (ranked.empty() && hunters.empty())
        return false;

    std::sort(hunters.begin(), hunters.end(),
              [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); });

    // Healers ahead of ranged dps, then guid: every bot derives the same order, and healers land on the centre
    // and inner ring, nearest the tank spot.
    std::sort(ranked.begin(), ranked.end(),
              [](std::pair<bool, Player*> const& left, std::pair<bool, Player*> const& right)
              {
                  if (left.first != right.first)
                      return left.first;
                  return left.second->GetGUID() < right.second->GetGUID();
              });

    for (std::pair<bool, Player*> const& entry : ranked)
        ring.push_back(entry.second);

    return true;
}

static Position XT002HunterSlotPoint(size_t index)
{
    float const radius = ULDUAR_XT002_HUNTER_ARC_RADIUS +
                         ULDUAR_XT002_HUNTER_ARC_ROW_STEP * static_cast<float>(index / ULDUAR_XT002_HUNTER_ARC_SLOTS);
    float const bearingDeg = ULDUAR_XT002_HUNTER_ARC_FIRST_BEARING_DEG -
                             ULDUAR_XT002_HUNTER_ARC_BEARING_STEP_DEG *
                                 static_cast<float>(index % ULDUAR_XT002_HUNTER_ARC_SLOTS);
    float const bearing = bearingDeg * static_cast<float>(M_PI) / 180.0f;

    return Position(ULDUAR_XT002_SPAWN.GetPositionX() + std::cos(bearing) * radius,
                    ULDUAR_XT002_SPAWN.GetPositionY() + std::sin(bearing) * radius,
                    ULDUAR_XT002_RANGED_SPOT.GetPositionZ());
}

// Raw formation geometry for one slot, no ground or collision pass.
static Position XT002RingSlotPoint(size_t slot, size_t total)
{
    // Bearing between two fixed points, so the layout never rotates, and it runs towards the tank spot
    // so slot 1 is the one nearest both XT and the tank.
    float const baseAngle =
        std::atan2(ULDUAR_XT002_MAINTANK_SPOT.GetPositionY() - ULDUAR_XT002_RANGED_SPOT.GetPositionY(),
                   ULDUAR_XT002_MAINTANK_SPOT.GetPositionX() - ULDUAR_XT002_RANGED_SPOT.GetPositionX());

    size_t const inner = std::min<size_t>(ULDUAR_XT002_RANGED_RING_INNER_SLOTS, total > 0 ? total - 1 : 0);

    float radiusX = 0.0f;
    float radiusY = 0.0f;
    float angle = baseAngle;

    // Slot 0 is the centre, nearest both the tank spot and the melee stack, so it goes to a healer.
    if (slot && slot <= inner)
    {
        radiusX = ULDUAR_XT002_RANGED_RING_INNER_X;
        radiusY = ULDUAR_XT002_RANGED_RING_INNER_Y;
        angle = baseAngle + 2.0f * static_cast<float>(M_PI) * static_cast<float>(slot - 1) / static_cast<float>(inner);
    }
    else if (slot)
    {
        size_t const outerCount = total - inner - 1;
        size_t const outerSlot = slot - inner - 1;
        radiusX = ULDUAR_XT002_RANGED_RING_OUTER_X;
        radiusY = ULDUAR_XT002_RANGED_RING_OUTER_Y;
        angle = baseAngle +
                2.0f * static_cast<float>(M_PI) * static_cast<float>(outerSlot) / static_cast<float>(outerCount);
    }

    angle = Position::NormalizeOrientation(angle);

    return Position(ULDUAR_XT002_RANGED_SPOT.GetPositionX() + std::cos(angle) * radiusX,
                    ULDUAR_XT002_RANGED_SPOT.GetPositionY() + ULDUAR_XT002_RANGED_RING_OFFSET_Y +
                        std::sin(angle) * radiusY,
                    ULDUAR_XT002_RANGED_SPOT.GetPositionZ());
}

bool GetXT002RangedSlot(Player* bot, Unit* xt002, Position& out)
{
    std::vector<Player*> members;
    std::vector<Player*> hunters;
    if (!BuildXT002FormationMembers(bot, members, hunters))
        return false;

    // Melee and tanks ask too, so they leave before the grid scan.
    auto const hunter = std::find(hunters.begin(), hunters.end(), bot);
    auto const member = std::find(members.begin(), members.end(), bot);
    if (hunter == hunters.end() && member == members.end())
        return false;

    // Every ranged bot and healer asks for this every tick, so skip the grid scan until Heartbreak: nothing
    // drops a puddle before it.
    std::list<Creature*> voidZones;
    if (IsXT002HeartbreakActive(bot, xt002))
        bot->GetCreatureListWithEntryInGrid(voidZones, PB_NPC_XT002_VOID_ZONE, ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS);

    auto const inVoidZone = [&voidZones](Position const& point)
    {
        for (Creature* voidZone : voidZones)
            if (voidZone->GetExactDist2d(point.GetPositionX(), point.GetPositionY()) < ULDUAR_XT002_VOID_ZONE_RADIUS)
                return true;

        return false;
    };

    if (hunter != hunters.end())
    {
        size_t const index = static_cast<size_t>(hunter - hunters.begin());

        // Puddle on the spot: one row further out on the same bearing, still in range of XT.
        Position spot = XT002HunterSlotPoint(index);
        if (inVoidZone(spot))
            spot = XT002HunterSlotPoint(index + ULDUAR_XT002_HUNTER_ARC_SLOTS);

        out = ValidateFloorPoint(bot, spot);

        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "xt002.slot",
                                 "hunter " + std::to_string(index) + "/" + std::to_string(hunters.size()) + " " +
                                     RaidObs::DescribeDerived(out));

        return true;
    }

    size_t const slot = static_cast<size_t>(member - members.begin());
    size_t const total = members.size();

    // Index into the slots clear of Consumption: every bot sees the same puddles, so they agree on the new
    // layout. A bot left without one keeps its own, and the hazard dodge, which outranks the anchor, moves it.
    std::vector<Position> clear;
    for (size_t i = 0; i < total; ++i)
    {
        Position const candidate = XT002RingSlotPoint(i, total);
        if (!inVoidZone(candidate))
            clear.push_back(candidate);
    }

    out = ValidateFloorPoint(bot, slot < clear.size() ? clear[slot] : XT002RingSlotPoint(slot, total));

    // The slot index and the size of the formation it was cut from, so a layout that re-seated is
    // readable without re-deriving the sort from the roster.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "xt002.slot",
                             std::to_string(slot) + "/" + std::to_string(total) + " " +
                                 RaidObs::DescribeDerived(out));

    return true;
}

// Where the formation says bots belong, not where they are standing: a carrier picks its destination
// while the raid is still walking, so the slots are what it has to miss.
std::vector<Position> GetXT002OtherFormationSlots(Player* bot)
{
    std::vector<Position> slots;
    std::vector<Player*> members;
    std::vector<Player*> hunters;
    if (!BuildXT002FormationMembers(bot, members, hunters))
        return slots;

    for (size_t i = 0; i < members.size(); ++i)
        if (members[i] != bot)
            slots.push_back(XT002RingSlotPoint(i, members.size()));

    for (size_t i = 0; i < hunters.size(); ++i)
        if (hunters[i] != bot)
            slots.push_back(XT002HunterSlotPoint(i));

    return slots;
}

// Melee get no northern lot: from their stack findSmoothPath refuses everything north of the boss line, and
// the route that does path is longer than a carrier can run in 9 s. Their lot grows east instead.
std::vector<XT002BombLot> GetXT002BombLots(Player* bot)
{
    if (PlayerbotAI::IsMelee(bot))
        return {{ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE, -1.0f, ULDUAR_XT002_BOMB_GRID_X_CELLS_MELEE}};

    return {{ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_SOUTH, -1.0f},
            {ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_NORTH, 1.0f}};
}
