/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_SindragosaAdds.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDirectTargeting.h"
#include "RaidEncounter.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidResistAura.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr uint32 SCAN_MS = 200;
constexpr uint32 DUTY_MS = 250;
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;
constexpr float SCAN_RANGE = 100.0f;

// The Icy Blast impact and the puddle it leaves both reach 7 yd
constexpr float ICY_RADIUS = 7.0f;
constexpr float ICY_TRIGGER = ICY_RADIUS + 0.5f;
constexpr float ICY_CLEAR = 9.5f;
static_assert(ICY_CLEAR >= ICY_RADIUS + SG_DRAKE_STAND_PARK + 1.0f, "a parked bot must stay out of the impact");
// The missile takes its target's spot as the cast ends and flies 15 yd/s from 25 yd up. Its puddle spawns
// where it lands.
constexpr uint32 BLAST_FLIGHT_MS = 4000;
constexpr float BLAST_LANDED = 3.0f;
constexpr float DODGE_SWEEP = 20.0f;
// A melee bot's dodge spot stays this close to its target's edge
constexpr float MELEE_KEEP = 3.0f;

constexpr float BRAKE_CAP = 0.9f;
constexpr float BRAKE_RELEASE = 0.8f;

// The melee and ranged stands sit round the landing spot, so they hold only while the drake is near it
constexpr float DRAKE_ANCHOR = 8.0f;
// A ranged stand under a puddle moves this far off, keeping out of Bellowing Roar's 35 yd and in sight of the
// drake's tank
constexpr float ALT_STEPS[] = {5.0f, 10.0f};
constexpr uint8 ALT_HEADINGS = 8;
constexpr float ROAR_KEEP = 37.0f;
// The stand hold blocks reach: a 30 yd spell gets to a drake from here, both combat reaches counted
constexpr float ALT_CAST_REACH = 40.0f;
constexpr float ALT_HEAL_REACH = 38.0f;
constexpr float FLOOR_TOLERANCE = 1.5f;
constexpr float SIGHT_EYE = 2.0f;
constexpr uint32 ALT_RETRY_MS = 1000;

// Bellowing Roar comes 20-25 s into the pull, then 25-30 s after each one
constexpr uint32 ROAR_FIRST_MS = 20000;
constexpr uint32 ROAR_NEXT_MS = 25000;
constexpr uint32 FEAR_LEAD_MS = 8000;

// The controller evades once no player is within 100 yd of it
constexpr float GAUNTLET_RANGE = 100.0f;
constexpr float BLADE_RADIUS = 12.0f;
constexpr float BLADE_TRIGGER = BLADE_RADIUS + 0.5f;
constexpr float BLADE_CLEAR = 14.5f;
static_assert(BLADE_CLEAR >= BLADE_RADIUS + 2.0f, "a dodge must clear the whirl with room to park");
constexpr float BLADE_SWEEP = 25.0f;
constexpr float TAUNT_RANGE = 30.0f;
// Interrupts that reach only this far need melee range
constexpr float MELEE_INTERRUPT_RANGE = 5.0f;
constexpr float FREE_RANGE = 30.0f;
constexpr uint32 SPELL_SG_HAND_OF_FREEDOM = 1044;

constexpr uint8 DRAKE_COUNT = 2;
// Kill order. sg.drake.tank holds the index plus one.
constexpr uint32 DRAKE_SLOTS[DRAKE_COUNT] = {SG_DRAKE_DATA_RIMEFANG, SG_DRAKE_DATA_SPINESTALKER};
constexpr char const* DRAKE_NAMES[DRAKE_COUNT] = {"rimefang", "spinestalker"};
constexpr uint8 RIMEFANG = 0;
constexpr uint8 SPINESTALKER = 1;

constexpr uint8 GAUNTLET_TIERS = 5;
// Kill order
constexpr uint32 GAUNTLET_ORDER[GAUNTLET_TIERS] = {NPC_SG_GAUNTLET_SORCERESS, NPC_SG_GAUNTLET_WEBWEAVER,
                                                   NPC_SG_GAUNTLET_CHAMPION, NPC_SG_GAUNTLET_WARRIOR,
                                                   NPC_SG_GAUNTLET_BROODLING};
constexpr char const* GAUNTLET_NAMES[GAUNTLET_TIERS] = {"sorceress", "webweaver", "champion", "warrior",
                                                        "broodling"};
// The Champion and the Warrior, by their place in GAUNTLET_ORDER: the adds a tank takes off a player who is
// no tank
constexpr uint8 GAUNTLET_RESCUE_TIERS[] = {2, 3};
constexpr uint8 SORCERESS_TIER = 0;

enum class StandRole : uint8
{
    None,
    Tank,
    Melee,
    Ranged
};

struct Stand
{
    uint8 drake = 0;  // index plus one
    StandRole role = StandRole::None;
    Position spot;
    bool parked = false;
    // A spot off a puddle on its own stand
    bool alt = false;
    uint32 altTriedMs = 0;
};

enum class IcyKind : uint8
{
    None,
    Puddle,
    Blast
};

// A missile in flight, aimed where its target stood as the cast ended
struct Impact
{
    Position spot;
    uint32 untilMs = 0;
};

// Asked by a trigger, its action and a rule for every action a bot weighs, and a bot never ticks twice in
// one ms
struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct DrakeState
{
    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<Position> puddles;
    // While Rimefang casts Icy Blast: its target, and the spot the missile will take
    bool blastCasting = false;
    ObjectGuid blastTarget;
    Position blastSpot;
    std::vector<Impact> impacts;

    uint32 dutyMs = 0;
    bool dutiesRan = false;
    // Bot tank -> the drake it holds, index plus one
    RaidObs::ObsGuidMap<uint8> tank{"sg.drake.tank"};
    // Both drakes up: healer -> the drake whose ranged stand it takes
    std::unordered_map<ObjectGuid, uint8> healerDrake;
    RaidObs::ObsGuidSet brake{"sg.drake.brake"};
    // 1 while Rimefang flies for his Icy Blasts
    RaidObs::ObsValue<uint8> air{"sg.drake.air"};

    bool roarClock = false;
    bool roarCasting = false;
    uint32 roarDueMs = 0;

    uint32 humanCallMs = 0;
    bool humanCallRan = false;

    std::unordered_map<ObjectGuid, Stand> stands;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<DrakeState> drakeStates;

struct GauntletState
{
    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<ObjectGuid> adds[GAUNTLET_TIERS];
    std::vector<Position> blades;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<GauntletState> gauntletStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// Players hold no duty, and a selfbot counts as a player here.
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

void Note(Player* bot, char const* key, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

// A literal builds its std::string only once a trace is open
void Note(Player* bot, char const* key, char const* value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
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

InstanceScript* IccInstance(Player* bot)
{
    return bot && bot->IsInWorld() && bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
}

Creature* SlotCreature(Player* bot, uint32 data)
{
    InstanceScript* instance = IccInstance(bot);
    return instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(data)) : nullptr;
}

// A pet or totem counts as its owner
bool InGroup(Player* bot, Unit* victim)
{
    Player* player = victim ? victim->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    return player && bot->IsInSameRaidWith(player);
}

bool Casting(Unit* caster, uint32 spellId, Spell** out = nullptr)
{
    Spell* spell = caster ? caster->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
    bool const casting = spell && spell->GetSpellInfo() &&
                         spell->GetSpellInfo()->Id == sSpellMgr->GetSpellIdForDifficulty(spellId, caster);
    if (out)
        *out = casting ? spell : nullptr;

    return casting;
}

Position StepClearOf(Player* bot, Unit* target, std::vector<EncounterHelpers::HazardCircle> const& hazards,
                     float sweep, bool& melee)
{
    melee = target && !PlayerbotAI::IsRanged(bot);
    Position found;
    if (melee)
        found = FindNearestPositionClearOfHazards(bot, hazards, sweep, 2.0f, static_cast<float>(M_PI) / 8.0f,
                                                  nullptr,
                                                  [target](float x, float y)
                                                  {
                                                      return target->GetExactDist2d(x, y) <=
                                                             target->GetCombatReach() + MELEE_KEEP;
                                                  });

    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        melee = false;
        found = FindNearestPositionClearOfHazards(bot, hazards, sweep, nullptr);
    }

    return found;
}

Creature* DrakeUnit(Player* bot, uint8 index) { return SlotCreature(bot, DRAKE_SLOTS[index]); }

int DrakeIndex(Unit* unit)
{
    switch (unit ? unit->GetEntry() : 0)
    {
        case NPC_SG_DRAKE_RIMEFANG:
            return RIMEFANG;
        case NPC_SG_DRAKE_SPINESTALKER:
            return SPINESTALKER;
        default:
            return -1;
    }
}

bool IsDrake(Unit* unit) { return DrakeIndex(unit) >= 0; }

// Its victim is one of the group, or, while it flies with no victim, the bot is on its threat list
bool Fights(Player* bot, Creature* drake)
{
    if (!drake || !drake->IsAlive() || !drake->IsInCombat())
        return false;

    return InGroup(bot, drake->GetVictim()) || bot->IsInCombatWith(drake);
}

Creature* FightingDrake(Player* bot, uint8 index)
{
    Creature* drake = DrakeUnit(bot, index);
    return Fights(bot, drake) ? drake : nullptr;
}

Creature* FirstFighting(Player* bot)
{
    for (uint8 index = 0; index < DRAKE_COUNT; ++index)
        if (Creature* drake = FightingDrake(bot, index))
            return drake;

    return nullptr;
}

// Passive with gravity off from his takeoff until he lands
bool RimefangUp(Creature* rimefang)
{
    return rimefang && rimefang->IsInCombat() && (rimefang->HasReactState(REACT_PASSIVE) || rimefang->IsLevitating());
}

bool Landed(Creature* drake) { return DrakeIndex(drake) != RIMEFANG || !RimefangUp(drake); }

Position const& LandSpot(uint8 index) { return index == RIMEFANG ? ICC_SG_RIMEFANG_LAND : ICC_SG_SPINESTALKER_LAND; }

Position const& StandSpot(uint8 index, StandRole role)
{
    bool const rimefang = index == RIMEFANG;
    switch (role)
    {
        case StandRole::Tank:
            return rimefang ? ICC_SG_RIMEFANG_TANK : ICC_SG_SPINESTALKER_TANK;
        case StandRole::Melee:
            return rimefang ? ICC_SG_RIMEFANG_MELEE : ICC_SG_SPINESTALKER_MELEE;
        default:
            return rimefang ? ICC_SG_RIMEFANG_RANGED : ICC_SG_SPINESTALKER_RANGED;
    }
}

char const* RoleName(StandRole role)
{
    switch (role)
    {
        case StandRole::Tank:
            return "tank";
        case StandRole::Melee:
            return "melee";
        default:
            return "ranged";
    }
}

// Frost Breath goes at his victim
bool BreathOn(Player* player, Creature* rimefang)
{
    Spell* spell = nullptr;
    if (!Casting(rimefang, SPELL_SG_DRAKE_FROST_BREATH, &spell))
        return false;

    return spell->m_targets.GetUnitTarget() == player || rimefang->GetVictim() == player;
}

uint8 OwnedIndex(DrakeState const& st, Player* bot)
{
    auto const itr = st.tank.find(bot->GetGUID());
    return itr != st.tank.end() && itr->second >= 1 && itr->second <= DRAKE_COUNT ? itr->second : 0;
}

Creature* OwnedDrake(DrakeState const& st, Player* bot)
{
    uint8 const owned = OwnedIndex(st, bot);
    return owned ? FightingDrake(bot, owned - 1) : nullptr;
}

// A drake on this tank, held by roster or by its victim
bool HoldsDrake(DrakeState const& st, Player* tank)
{
    if (OwnedIndex(st, tank))
        return true;

    for (uint8 index = 0; index < DRAKE_COUNT; ++index)
        if (Creature* drake = FightingDrake(tank, index))
            if (drake->GetVictim() == tank)
                return true;

    return false;
}

// The drake a bot holding none fights: a player's skull on one, else the first in the kill order. Melee can't
// reach Rimefang while he flies, so they take Spinestalker meanwhile.
Creature* FocusDrake(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (Unit* skull = GetRaidHumanSkull(botAI, &IsDrake))
        if (Creature* drake = skull->ToCreature())
            if (Fights(bot, drake))
                return drake;

    Creature* first = FirstFighting(bot);
    if (DrakeIndex(first) == RIMEFANG && RimefangUp(first) && !PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot))
        if (Creature* spinestalker = FightingDrake(bot, SPINESTALKER))
            return spinestalker;

    return first;
}

DrakeState* LiveDrakes(PlayerbotAI* botAI)
{
    return IsSgDrakesLive(botAI) ? drakeStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

void ScanDrakes(DrakeState& st, Player* bot, uint32 now)
{
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    st.puddles.clear();
    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_SG_DRAKE_ICY_BLAST, SCAN_RANGE);
    for (Creature* puddle : found)
        if (puddle->IsAlive())
            st.puddles.push_back(puddle->GetPosition());

    // A missile is down once its puddle shows or its flight time is up
    auto const down = [&](Impact const& impact)
    {
        if (static_cast<int32>(impact.untilMs - now) <= 0)
            return true;

        return std::any_of(st.puddles.begin(), st.puddles.end(), [&](Position const& puddle)
                           { return puddle.GetExactDist2d(impact.spot) <= BLAST_LANDED; });
    };
    st.impacts.erase(std::remove_if(st.impacts.begin(), st.impacts.end(), down), st.impacts.end());

    Creature* rimefang = FightingDrake(bot, RIMEFANG);
    Spell* spell = nullptr;
    Unit* target = nullptr;
    bool casting = Casting(rimefang, SPELL_SG_DRAKE_ICY_BLAST, &spell);
    if (casting)
    {
        target = spell->m_targets.GetUnitTarget();
        if (spell->m_targets.HasDst())
            st.blastSpot = spell->m_targets.GetDstPos()->GetPosition();
        else if (target)
            st.blastSpot = target->GetPosition();
        else
            casting = false;
    }

    if (casting)
        st.blastTarget = target ? target->GetGUID() : ObjectGuid::Empty;
    else if (st.blastCasting)
        st.impacts.push_back({st.blastSpot, now + BLAST_FLIGHT_MS});

    st.blastCasting = casting;
}

// Main tank, then the assist tanks in their order, then guid
std::vector<Player*> RankedBotTanks(std::vector<Player*> const& members)
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

    std::vector<Player*> tanks;
    for (auto const& entry : ranked)
        tanks.push_back(entry.second);

    return tanks;
}

// A holder keeps its drake for the fight. A drake nobody holds goes to the best ranked free bot tank, so
// the main tank takes the first; one a human tank holds stays his.
void AssignTanks(DrakeState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<ObjectGuid> lapsed;
    for (auto const& [guid, held] : st.tank)
    {
        Player* member = ObjectAccessor::GetPlayer(*bot, guid);
        if (!member || !member->IsAlive() || !IccSameInstance(bot, member) || held < 1 || held > DRAKE_COUNT ||
            !FightingDrake(bot, held - 1))
            lapsed.push_back(guid);
    }

    for (ObjectGuid const& guid : lapsed)
        st.tank.erase(guid);

    std::vector<Player*> const tanks = RankedBotTanks(members);
    for (uint8 index = 0; index < DRAKE_COUNT; ++index)
    {
        Creature* drake = FightingDrake(bot, index);
        if (!drake)
            continue;

        bool owned = false;
        for (auto const& entry : st.tank)
            owned = owned || entry.second == index + 1;

        Unit* victim = drake->GetVictim();
        Player* holder = victim ? victim->ToPlayer() : nullptr;
        if (owned || (holder && !IsDutyBot(holder) && IsTankPlayer(holder)))
            continue;

        for (Player* tank : tanks)
            if (!st.tank.count(tank->GetGUID()))
            {
                st.tank[tank->GetGUID()] = static_cast<uint8>(index + 1);
                break;
            }
    }
}

void UpdateRoarClock(DrakeState& st, Player* bot, uint32 now)
{
    Creature* spinestalker = FightingDrake(bot, SPINESTALKER);
    if (!spinestalker)
    {
        st.roarClock = false;
        st.roarCasting = false;
        return;
    }

    if (!st.roarClock)
    {
        st.roarClock = true;
        st.roarDueMs = now + ROAR_FIRST_MS;
    }

    bool const casting = Casting(spinestalker, SPELL_SG_DRAKE_BELLOWING_ROAR);
    if (casting && !st.roarCasting)
        st.roarDueMs = now + ROAR_NEXT_MS;

    st.roarCasting = casting;
}

// With both drakes pulled, their tanks stand about 78 yd apart: healers split between the two ranged stands by
// guid, Rimefang's first, so each tank keeps one in reach
void AssignHealers(DrakeState& st, Player* bot, std::vector<Player*> const& members)
{
    st.healerDrake.clear();
    if (!FightingDrake(bot, RIMEFANG) || !FightingDrake(bot, SPINESTALKER))
        return;

    std::vector<Player*> healers;
    for (Player* member : members)
        if (IsDutyBot(member) && PlayerbotAI::IsHeal(member) && !IsTankPlayer(member))
            healers.push_back(member);

    std::sort(healers.begin(), healers.end(), IccGuidLess());
    for (size_t index = 0; index < healers.size(); ++index)
        st.healerDrake[healers[index]->GetGUID()] = static_cast<uint8>(index % DRAKE_COUNT);
}

void RefreshDrakeDuties(DrakeState& st, Player* bot, uint32 now)
{
    if (st.dutiesRan && getMSTimeDiff(st.dutyMs, now) < DUTY_MS)
        return;

    st.dutiesRan = true;
    st.dutyMs = now;
    std::vector<Player*> const members = MembersOnMap(bot);
    AssignTanks(st, bot, members);
    AssignHealers(st, bot, members);
    st.air = static_cast<uint8>(RimefangUp(FightingDrake(bot, RIMEFANG)) ? 1 : 0);
    UpdateRoarClock(st, bot, now);
}

// The threat a non-holder stays under: the bot tank holding this drake, else its victim when a tank, else
// the top tank on its threat list
float HolderThreat(DrakeState const& st, Player* bot, Creature* drake, uint8 index)
{
    ThreatManager& threat = drake->GetThreatMgr();
    for (auto const& [guid, held] : st.tank)
        if (held == index + 1 && guid != bot->GetGUID())
            if (Player* owner = ObjectAccessor::GetPlayer(*bot, guid))
                return threat.GetThreat(owner);

    Unit* victim = drake->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (holder && holder != bot && IsTankPlayer(holder))
        return threat.GetThreat(holder);

    float top = 0.0f;
    for (Player* member : MembersOnMap(bot))
        if (member != bot && IsTankPlayer(member))
            top = std::max(top, threat.GetThreat(member));

    return top;
}

// Past BRAKE_CAP of the holder's threat until under BRAKE_RELEASE: a tank holding no drake all fight, since
// its own taunt is held, and everyone else while Rimefang flies, as he lands on the top of his threat list
void UpdateBrake(DrakeState& st, PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    bool hold = false;
    Creature* drake = IsDutyBot(bot) && !PlayerbotAI::IsHeal(bot) && !OwnedIndex(st, bot) ? FocusDrake(botAI) : nullptr;
    int const index = DrakeIndex(drake);
    if (drake && index >= 0 && (IsTankPlayer(bot) || (index == RIMEFANG && RimefangUp(drake))))
    {
        float const top = HolderThreat(st, bot, drake, static_cast<uint8>(index));
        float const own = drake->GetThreatMgr().GetThreat(bot);
        float const share = st.brake.count(bot->GetGUID()) ? BRAKE_RELEASE : BRAKE_CAP;
        hold = top > 0.0f && own > 0.0f && own >= share * top;
    }

    if (hold)
        st.brake.insert(bot->GetGUID());
    else
        st.brake.erase(bot->GetGUID());
}

bool Fouled(DrakeState const& st, Position const& spot)
{
    auto const fouls = [&](Position const& hazard) { return hazard.GetExactDist2d(spot) < ICY_CLEAR; };
    return std::any_of(st.puddles.begin(), st.puddles.end(), fouls) ||
           std::any_of(st.impacts.begin(), st.impacts.end(),
                       [&](Impact const& impact) { return fouls(impact.spot); });
}

// The target of the Icy Blast being cast ignores that one: the missile takes whatever spot it stands on as
// the cast ends, so stepping away only drags the impact along
IcyKind StandingIn(DrakeState const& st, Player* bot)
{
    Position const pos = bot->GetPosition();
    if (st.blastCasting && st.blastTarget != bot->GetGUID() && st.blastSpot.GetExactDist2d(pos) < ICY_TRIGGER)
        return IcyKind::Blast;

    for (Impact const& impact : st.impacts)
        if (impact.spot.GetExactDist2d(pos) < ICY_TRIGGER)
            return IcyKind::Blast;

    for (Position const& puddle : st.puddles)
        if (puddle.GetExactDist2d(pos) < ICY_TRIGGER)
            return IcyKind::Puddle;

    return IcyKind::None;
}

bool StaticSight(Player* bot, Position const& from, Position const& to)
{
    return bot->GetMap()->isInLineOfSight(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ() + SIGHT_EYE,
                                          to.GetPositionX(), to.GetPositionY(), to.GetPositionZ() + SIGHT_EYE,
                                          bot->GetPhaseMask(), LINEOFSIGHT_CHECK_VMAP,
                                          VMAP::ModelIgnoreFlags::Nothing);
}

// The nearest spot round a fouled ranged stand that is clear of every puddle and impact, on the floor, out of
// Bellowing Roar, in spell reach of the drake (a healer's in heal reach of its tank stand) and in sight of that
bool AltStand(DrakeState const& st, Player* bot, uint8 index, Position const& stand, Position& out)
{
    Position const& tank = StandSpot(index, StandRole::Tank);
    bool const healer = PlayerbotAI::IsHeal(bot);
    for (float step : ALT_STEPS)
        for (uint8 heading = 0; heading < ALT_HEADINGS; ++heading)
        {
            float const angle = static_cast<float>(heading) * 2.0f * static_cast<float>(M_PI) / ALT_HEADINGS;
            Position const wanted(stand.GetPositionX() + step * std::cos(angle),
                                  stand.GetPositionY() + step * std::sin(angle), stand.GetPositionZ());
            if (Fouled(st, wanted) || wanted.GetExactDist2d(LandSpot(index)) > ALT_CAST_REACH ||
                (healer && wanted.GetExactDist2d(tank) > ALT_HEAL_REACH) ||
                (index == SPINESTALKER && wanted.GetExactDist2d(ICC_SG_SPINESTALKER_LAND) < ROAR_KEEP))
                continue;

            Position const floor = ValidateStaticFloorPoint(bot, wanted);
            if (floor.GetExactDist2d(wanted) > FLOOR_TOLERANCE || !StaticSight(bot, floor, tank))
                continue;

            out = floor;
            return true;
        }

    return false;
}

void UpdateStand(DrakeState& st, PlayerbotAI* botAI, uint32 now)
{
    Player* bot = botAI->GetBot();
    Stand& stand = st.stands[bot->GetGUID()];
    int index = -1;
    StandRole role = StandRole::None;
    auto const split = st.healerDrake.find(bot->GetGUID());
    if (IsDutyBot(bot))
    {
        if (uint8 const owned = OwnedIndex(st, bot))
        {
            index = owned - 1;
            role = StandRole::Tank;
        }
        else if (split != st.healerDrake.end())
        {
            index = split->second;
            role = StandRole::Ranged;
        }
        else if (Creature* focus = FocusDrake(botAI))
        {
            index = DrakeIndex(focus);
            role = !IsTankPlayer(bot) && (PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsRanged(bot)) ? StandRole::Ranged
                                                                                                 : StandRole::Melee;
        }
    }

    uint8 const slot = static_cast<uint8>(index + 1);
    Creature* drake = index >= 0 ? FightingDrake(bot, static_cast<uint8>(index)) : nullptr;
    Position spot;
    bool alt = false;
    if (drake)
    {
        spot = StandSpot(static_cast<uint8>(index), role);
        bool const anchored =
            role == StandRole::Tank || drake->GetExactDist2d(LandSpot(static_cast<uint8>(index))) <= DRAKE_ANCHOR;
        if (!Landed(drake) || !anchored)
            drake = nullptr;
        else if (Fouled(st, spot))
        {
            // Melee and the tank stay on their drake wherever it is; a ranged stand moves off the puddle
            bool const same = stand.role == role && stand.drake == slot;
            if (same && stand.alt && !Fouled(st, stand.spot))
            {
                spot = stand.spot;
                alt = true;
            }
            // Each try asks the navmesh up to 16 times, so a failed one waits before the next
            else if (role == StandRole::Ranged &&
                     (!stand.altTriedMs || getMSTimeDiff(stand.altTriedMs, now) >= ALT_RETRY_MS))
            {
                stand.altTriedMs = now;
                Position const own = spot;
                alt = AltStand(st, bot, static_cast<uint8>(index), own, spot);
                if (!alt)
                    drake = nullptr;
            }
            else
                drake = nullptr;
        }
    }

    if (!drake)
    {
        stand.role = StandRole::None;
        stand.alt = false;
        Note(bot, "sg.drake.stand", "none");
        return;
    }

    if (stand.role != role || stand.drake != slot || stand.spot.GetExactDist2d(spot) > 0.5f)
    {
        stand.role = role;
        stand.drake = slot;
        stand.spot = spot;
        stand.parked = false;
    }

    stand.alt = alt;
    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= SG_DRAKE_STAND_UNPARK : distance <= SG_DRAKE_STAND_PARK;
    Note(bot, "sg.drake.stand", std::string(RoleName(role)) + " " + DRAKE_NAMES[index] + (alt ? " alt" : ""));
}

Stand const* HeldStand(PlayerbotAI* botAI)
{
    DrakeState* st = LiveDrakes(botAI);
    if (!st)
        return nullptr;

    auto const itr = st->stands.find(botAI->GetBot()->GetGUID());
    return itr != st->stands.end() && itr->second.role != StandRole::None ? &itr->second : nullptr;
}

Unit* ResolveDrakeTarget(PlayerbotAI* botAI, DrakeState const& st)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    bool const braked = st.brake.count(bot->GetGUID()) != 0;
    if (Unit* skull = GetRaidHumanSkull(botAI, &IsDrake))
        if (!braked || skull != FocusDrake(botAI))
        {
            Note(bot, "sg.drake.target", "skull");
            return skull;
        }

    if (tank)
        if (Creature* owned = OwnedDrake(st, bot))
        {
            Note(bot, "sg.drake.target", std::string("owned ") + DRAKE_NAMES[DrakeIndex(owned)]);
            return owned;
        }

    if (braked)
    {
        Note(bot, "sg.drake.target", "brake");
        return nullptr;
    }

    if (Creature* focus = FocusDrake(botAI))
    {
        Note(bot, "sg.drake.target", DRAKE_NAMES[DrakeIndex(focus)]);
        return focus;
    }

    Note(bot, "sg.drake.target", "none");
    return nullptr;
}

// A human tank has no trigger, so it gets told: a button when low, and on each Frost Breath at it
void CallDrakeHumanTank(DrakeState& st, Player* sender, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;
    Creature* rimefang = FightingDrake(sender, RIMEFANG);
    for (Player* member : MembersOnMap(sender))
    {
        if (IsDutyBot(member) || !IsTankPlayer(member))
            continue;

        bool held = false;
        for (uint8 index = 0; index < DRAKE_COUNT; ++index)
            if (Creature* drake = FightingDrake(sender, index))
                held = held || drake->GetVictim() == member;

        bool const breath =
            held && BreathOn(member, rimefang) && member->GetHealthPct() < SG_DRAKE_BREATH_DEFENSIVE_PCT;
        bool const low = held && member->GetHealthPct() < SG_DRAKE_TANK_DEFENSIVE_PCT;
        if (!breath && !low)
            continue;

        Spell* spell = rimefang ? rimefang->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
        uint32 const hitIn = breath && spell ? static_cast<uint32>(std::max(spell->GetCastTimeRemaining(), 0)) : 0;
        CallHumanTankDefensive(sender, member, ReadyTankDefensiveSpell(member, !breath, 0, 0), hitIn,
                               breath ? "Frost Breath" : "you are low", "sg.drake.humancall");
    }
}

int GauntletTier(Unit* unit)
{
    uint32 const entry = unit ? unit->GetEntry() : 0;
    for (int tier = 0; tier < GAUNTLET_TIERS; ++tier)
        if (GAUNTLET_ORDER[tier] == entry)
            return tier;

    return -1;
}

bool IsGauntletAdd(Unit* unit) { return GauntletTier(unit) >= 0; }

bool GauntletInProgress(Player* bot)
{
    InstanceScript* instance = IccInstance(bot);
    return instance && instance->GetBossState(ICC_BOSS_SINDRAGOSA_GAUNTLET) == IN_PROGRESS;
}

Creature* GauntletController(Player* bot) { return SlotCreature(bot, NPC_SG_GAUNTLET_CONTROLLER); }

GauntletState* LiveGauntlet(PlayerbotAI* botAI)
{
    return IsSgGauntletLive(botAI) ? gauntletStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

void ScanGauntlet(GauntletState& st, Creature* controller, uint32 now)
{
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    for (std::vector<ObjectGuid>& tier : st.adds)
        tier.clear();
    st.blades.clear();

    static std::vector<uint32> const entries = {
        NPC_SG_GAUNTLET_SORCERESS, NPC_SG_GAUNTLET_WEBWEAVER, NPC_SG_GAUNTLET_CHAMPION,
        NPC_SG_GAUNTLET_WARRIOR,   NPC_SG_GAUNTLET_BROODLING, NPC_SG_GAUNTLET_FROSTBLADE};
    std::list<Creature*> found;
    controller->GetCreatureListWithEntryInGrid(found, entries, SCAN_RANGE);
    for (Creature* creature : found)
    {
        if (!creature->IsAlive())
            continue;

        if (creature->GetEntry() == NPC_SG_GAUNTLET_FROSTBLADE)
        {
            st.blades.push_back(creature->GetPosition());
            continue;
        }

        int const tier = GauntletTier(creature);
        if (tier >= 0)
            st.adds[tier].push_back(creature->GetGUID());
    }
}

// Spiders still on their web are neither selectable nor attackable
bool Engaged(Player* bot, Creature* add)
{
    return add && add->IsAlive() && add->IsInCombat() && InGroup(bot, add->GetVictim()) &&
           bot->IsValidAttackTarget(add);
}

// A Champion or Warrior hitting a player who is no tank, which the duty tank nearest it takes. For a taunt,
// the nearest with its taunt ready, skipping one a taunt just landed on: it still shows its old victim.
Creature* RescueFor(GauntletState const& st, Player* bot, bool forTaunt)
{
    std::vector<Player*> members;
    for (Player* member : MembersOnMap(bot))
    {
        PlayerbotAI* memberAI = IsDutyBot(member) && IsTankPlayer(member) ? GET_PLAYERBOT_AI(member) : nullptr;
        if (memberAI && (!forTaunt || ClassTauntReady(memberAI)))
            members.push_back(member);
    }

    Creature* pick = nullptr;
    float pickDistance = 0.0f;
    for (uint8 tier : GAUNTLET_RESCUE_TIERS)
        for (ObjectGuid const& guid : st.adds[tier])
        {
            Creature* add = bot->GetMap()->GetCreature(guid);
            if (!Engaged(bot, add) || (forTaunt && add->HasAuraType(SPELL_AURA_MOD_TAUNT)))
                continue;

            Player* hit = add->GetVictim()->ToPlayer();
            if (!hit || IsTankPlayer(hit))
                continue;

            Player* elected = nullptr;
            std::tuple<float, ObjectGuid> electedKey;
            for (Player* member : members)
            {
                auto const key = std::make_tuple(member->GetExactDist2d(add), member->GetGUID());
                if (!elected || key < electedKey)
                {
                    elected = member;
                    electedKey = key;
                }
            }

            float const distance = bot->GetExactDist2d(add);
            if (elected == bot && (!pick || distance < pickDistance))
            {
                pick = add;
                pickDistance = distance;
            }
        }

    return pick;
}

Unit* ResolveGauntletTarget(PlayerbotAI* botAI, GauntletState const& st)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    if (Unit* skull = GetRaidHumanSkull(botAI, &IsGauntletAdd))
    {
        Note(bot, "sg.gauntlet.target", "skull");
        return skull;
    }

    if (tank && IsDutyBot(bot))
        if (Creature* rescue = RescueFor(st, bot, false))
        {
            Note(bot, "sg.gauntlet.target", "rescue");
            return rescue;
        }

    for (uint8 tier = 0; tier < GAUNTLET_TIERS; ++tier)
    {
        Creature* pick = nullptr;
        float pickDistance = 0.0f;
        for (ObjectGuid const& guid : st.adds[tier])
        {
            Creature* add = bot->GetMap()->GetCreature(guid);
            if (!Engaged(bot, add))
                continue;

            float const distance = bot->GetExactDist2d(add);
            if (!pick || distance < pickDistance || (distance == pickDistance && add->GetGUID() < pick->GetGUID()))
            {
                pick = add;
                pickDistance = distance;
            }
        }

        if (pick)
        {
            Note(bot, "sg.gauntlet.target", GAUNTLET_NAMES[tier]);
            return pick;
        }
    }

    Note(bot, "sg.gauntlet.target", "none");
    return nullptr;
}

bool NearBlade(GauntletState const& st, Position const& pos, float reach)
{
    return std::any_of(st.blades.begin(), st.blades.end(),
                       [&](Position const& blade) { return blade.GetExactDist2d(pos) < reach; });
}

bool CanPayFor(Player* bot, SpellInfo const* info)
{
    // POWER_HEALTH is -2 and Powers is signed, hence the cast.
    if (info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(bot->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(bot, info->GetSchoolMask());
}

// An interrupt this member can cast on the caster now, or null. A false ready would hold the real kicker back
// from the election.
char const* ReadyInterrupt(Player* member, Unit* caster)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI || member->HasUnitState(UNIT_STATE_CASTING))
        return nullptr;

    static char const* const interrupts[] = {"kick",        "pummel",       "shield bash",
                                             "mind freeze", "counterspell", "wind shear"};
    for (char const* interrupt : interrupts)
    {
        uint32 const spellId = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", interrupt)->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info || !CanPayFor(member, info) || info->CheckShapeshift(member->GetShapeshiftForm()) != SPELL_CAST_OK)
            continue;

        float const range = member->GetSpellMaxRangeForTarget(caster, info);
        bool const inReach = range <= MELEE_INTERRUPT_RANGE ? member->IsWithinMeleeRange(caster)
                                                            : member->IsWithinCombatRange(caster, range);
        if (inReach && memberAI->CanCastSpell(spellId, caster))
            return interrupt;
    }

    return nullptr;
}

// Hand of Freedom breaks either root, a priest's Dispel Magic only Frost Nova's
char const* FreeSpell(Player* member, Unit* rooted, bool nova)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI)
        return nullptr;

    if (member->getClass() == CLASS_PALADIN && memberAI->CanCastSpell("hand of freedom", rooted))
        return "hand of freedom";

    if (nova && member->getClass() == CLASS_PRIEST && memberAI->CanCastSpell("dispel magic", rooted))
        return "dispel magic";

    return nullptr;
}
}  // namespace

Creature* GetSgDrake(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetMapId() != ICC_MAP_ID || !bot->IsAlive())
        return nullptr;

    Creature* drake = UnitGateLiveUnit<NPC_SG_DRAKE_RIMEFANG, NPC_SG_DRAKE_SPINESTALKER>(botAI);
    return drake && drake->IsAlive() && drake->IsInCombat() ? drake : nullptr;
}

bool IsSgDrakesLive(PlayerbotAI* botAI) { return GetSgDrake(botAI) != nullptr; }

Unit* GetSgDrakeTarget(PlayerbotAI* botAI)
{
    DrakeState* st = LiveDrakes(botAI);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveDrakeTarget(botAI, *st);
    }

    return memo.pick;
}

bool IsSgDrakeBraked(PlayerbotAI* botAI)
{
    DrakeState* st = LiveDrakes(botAI);
    return st && st->brake.count(botAI->GetBot()->GetGUID());
}

Creature* GetSgDrakeBrakeTarget(PlayerbotAI* botAI) { return IsSgDrakeBraked(botAI) ? FocusDrake(botAI) : nullptr; }

bool IsSgDrakeTauntHeld(PlayerbotAI* botAI)
{
    DrakeState* st = LiveDrakes(botAI);
    return st && !OwnedIndex(*st, botAI->GetBot());
}

bool IsSgDrakeInHazard(PlayerbotAI* botAI)
{
    DrakeState* st = LiveDrakes(botAI);
    if (!st)
        return false;

    Player* bot = botAI->GetBot();
    bool const inside = StandingIn(*st, bot) != IcyKind::None;
    if (!inside)
        Note(bot, "sg.drake.dodge", "none");

    return inside;
}

bool FindSgDrakeDodgeSpot(PlayerbotAI* botAI, Position& spot)
{
    DrakeState* st = LiveDrakes(botAI);
    if (!st)
        return false;

    Player* bot = botAI->GetBot();
    IcyKind const kind = StandingIn(*st, bot);
    if (kind == IcyKind::None)
        return false;

    std::vector<HazardCircle> hazards;
    for (Position const& puddle : st->puddles)
        hazards.emplace_back(puddle, ICY_CLEAR);
    for (Impact const& impact : st->impacts)
        hazards.emplace_back(impact.spot, ICY_CLEAR);
    if (st->blastCasting && st->blastTarget != bot->GetGUID())
        hazards.emplace_back(st->blastSpot, ICY_CLEAR);

    bool melee = false;
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    Position const found = StepClearOf(bot, target, hazards, DODGE_SWEEP, melee);
    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        Note(bot, "sg.drake.dodge", "blocked");
        return false;
    }

    Note(bot, "sg.drake.dodge", kind == IcyKind::Blast ? "blast" : "puddle");
    spot = found;
    return true;
}

bool IsSgDrakeHoldingStand(PlayerbotAI* botAI) { return HeldStand(botAI) != nullptr; }

bool GetSgDrakeStandWalk(PlayerbotAI* botAI, Position& spot)
{
    Stand const* stand = HeldStand(botAI);
    if (!stand || stand->parked)
        return false;

    spot = stand->spot;
    return true;
}

bool IsSgDrakeMovementOwned(PlayerbotAI* botAI)
{
    DrakeState* st = LiveDrakes(botAI);
    return st && (HeldStand(botAI) || StandingIn(*st, botAI->GetBot()) != IcyKind::None);
}

bool IsSgSpinestalkerFearWindow(PlayerbotAI* botAI)
{
    DrakeState* st = LiveDrakes(botAI);
    Creature* spinestalker = st ? FightingDrake(botAI->GetBot(), SPINESTALKER) : nullptr;
    if (!spinestalker || !st->roarClock)
        return false;

    return Casting(spinestalker, SPELL_SG_DRAKE_BELLOWING_ROAR) ||
           static_cast<int32>(st->roarDueMs - getMSTime()) <= static_cast<int32>(FEAR_LEAD_MS);
}

uint32 GetSgDrakeTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    DrakeState* st = LiveDrakes(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return 0;

    bool const holds = HoldsDrake(*st, bot);
    bool const breath = holds && BreathOn(bot, FightingDrake(bot, RIMEFANG)) &&
                        bot->GetHealthPct() < SG_DRAKE_BREATH_DEFENSIVE_PCT;
    bool const low = holds && bot->GetHealthPct() < SG_DRAKE_TANK_DEFENSIVE_PCT;
    if (!breath && !low)
    {
        // Change-latched, so the way out of a window is written too
        if (noteKind)
            Note(bot, noteKind, "idle");

        return 0;
    }

    // Frost Breath is frost, the swings physical
    return NextTankDefensive(botAI, bot, noteKind, !breath);
}

bool IsSgDrakeTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsSgDrakesLive(botAI) && botAI->GetBot()->GetHealthPct() >= SG_DRAKE_TANK_LAST_RESORT_PCT;
}

void TickSgDrakesEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // A wipe or a kill ends the pull: the next one rebuilds every duty
    bool fighting = false;
    for (uint8 index = 0; index < DRAKE_COUNT; ++index)
        if (Creature* drake = DrakeUnit(bot, index))
            fighting = fighting || (drake->IsAlive() && drake->IsInCombat());

    if (!fighting)
    {
        if (drakeStates.Find(bot->GetInstanceId()))
            drakeStates.Reset(bot->GetInstanceId());
        return;
    }

    Creature* drake = GetSgDrake(botAI);
    if (!drake)
        return;

    IccMarkPull(bot, drake, "sindragosa");

    DrakeState& st = drakeStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    ScanDrakes(st, bot, now);
    RefreshDrakeDuties(st, bot, now);
    CallDrakeHumanTank(st, bot, now);
    UpdateBrake(st, botAI);
    UpdateStand(st, botAI, now);
}

uint8 IccSindragosaDrakesResistSchools(PlayerbotAI* /*botAI*/, uint8 /*previous*/) { return RAID_RESIST_FROST; }

bool IsSgGauntletLive(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive() || !GauntletInProgress(bot))
        return false;

    Creature* controller = GauntletController(bot);
    return controller && bot->GetExactDist2d(controller) <= GAUNTLET_RANGE;
}

Unit* GetSgGauntletTarget(PlayerbotAI* botAI)
{
    GauntletState* st = LiveGauntlet(botAI);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveGauntletTarget(botAI, *st);
    }

    return memo.pick;
}

Unit* GetSgGauntletTauntTarget(PlayerbotAI* botAI)
{
    GauntletState* st = LiveGauntlet(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return nullptr;

    Creature* add = RescueFor(*st, bot, true);
    bool const ready = add && ClassTauntReady(botAI) && bot->GetExactDist(add) <= TAUNT_RANGE &&
                       bot->IsWithinLOSInMap(add);
    Note(bot, "sg.gauntlet.taunt", ready ? GAUNTLET_NAMES[GauntletTier(add)] : "none");
    return ready ? add : nullptr;
}

bool IsSgGauntletInBlade(PlayerbotAI* botAI)
{
    GauntletState* st = LiveGauntlet(botAI);
    if (!st)
        return false;

    Player* bot = botAI->GetBot();
    bool const inside = NearBlade(*st, bot->GetPosition(), BLADE_TRIGGER);
    if (!inside)
        Note(bot, "sg.gauntlet.dodge", "none");

    return inside;
}

bool FindSgGauntletDodgeSpot(PlayerbotAI* botAI, Position& spot)
{
    GauntletState* st = LiveGauntlet(botAI);
    if (!st)
        return false;

    Player* bot = botAI->GetBot();
    std::vector<HazardCircle> hazards;
    for (Position const& blade : st->blades)
        hazards.emplace_back(blade, BLADE_CLEAR);

    bool melee = false;
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    Position const found = StepClearOf(bot, target, hazards, BLADE_SWEEP, melee);
    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        Note(bot, "sg.gauntlet.dodge", "blocked");
        return false;
    }

    Note(bot, "sg.gauntlet.dodge", melee ? "melee" : "clear");
    spot = found;
    return true;
}

bool IsSgGauntletBladeReachHeld(PlayerbotAI* botAI)
{
    GauntletState* st = LiveGauntlet(botAI);
    if (!st || st->blades.empty())
        return false;

    Player* bot = botAI->GetBot();
    if (PlayerbotAI::IsRanged(bot) && !IsTankPlayer(bot))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && NearBlade(*st, target->GetPosition(), BLADE_CLEAR);
}

Unit* GetSgGauntletKickTarget(PlayerbotAI* botAI, std::string& spell)
{
    GauntletState* st = LiveGauntlet(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsDutyBot(bot))
        return nullptr;

    // One kick per cast: the lowest guid with an interrupt that reaches her
    for (ObjectGuid const& guid : st->adds[SORCERESS_TIER])
    {
        Creature* sorceress = bot->GetMap()->GetCreature(guid);
        if (!sorceress || !sorceress->IsAlive() ||
            !Casting(sorceress, sSpellMgr->GetSpellIdForDifficulty(SPELL_SG_GAUNTLET_ICE_TOMB, sorceress)))
            continue;

        char const* interrupt = ReadyInterrupt(bot, sorceress);
        if (!interrupt)
            continue;

        std::vector<Player*> const members = MembersOnMap(bot);
        bool const first = std::none_of(members.begin(), members.end(),
                                        [&](Player* member)
                                        {
                                            return member->GetGUID() < bot->GetGUID() && IsDutyBot(member) &&
                                                   ReadyInterrupt(member, sorceress);
                                        });
        if (!first)
            continue;

        Note(bot, "sg.gauntlet.kick", "sorceress");
        spell = interrupt;
        return sorceress;
    }

    Note(bot, "sg.gauntlet.kick", "none");
    return nullptr;
}

Unit* GetSgGauntletFreeTarget(PlayerbotAI* botAI, std::string& spell)
{
    GauntletState* st = LiveGauntlet(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsDutyBot(bot) || (bot->getClass() != CLASS_PALADIN && bot->getClass() != CLASS_PRIEST))
        return nullptr;

    if (!st->blades.empty())
    {
        std::vector<Player*> const members = MembersOnMap(bot);
        for (Player* rooted : members)
        {
            bool const nova = rooted->HasAura(SPELL_SG_GAUNTLET_FROST_NOVA);
            if ((!nova && !rooted->HasAura(SPELL_SG_GAUNTLET_WEB)) || rooted->HasAura(SPELL_SG_HAND_OF_FREEDOM) ||
                !NearBlade(*st, rooted->GetPosition(), BLADE_TRIGGER))
                continue;

            Player* elected = nullptr;
            char const* electedSpell = nullptr;
            float electedDistance = 0.0f;
            for (Player* member : members)
            {
                float const distance = member->GetExactDist(rooted);
                if (!IsDutyBot(member) || distance > FREE_RANGE || (elected && distance >= electedDistance))
                    continue;

                char const* name = FreeSpell(member, rooted, nova);
                if (!name || !member->IsWithinLOSInMap(rooted))
                    continue;

                elected = member;
                electedSpell = name;
                electedDistance = distance;
            }

            if (elected == bot)
            {
                Note(bot, "sg.gauntlet.free", electedSpell);
                spell = electedSpell;
                return rooted;
            }
        }
    }

    Note(bot, "sg.gauntlet.free", "none");
    return nullptr;
}

void TickSgGauntletEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // The controller's reset and death move the state off IN_PROGRESS: the next run rebuilds the scan
    if (!GauntletInProgress(bot))
    {
        if (gauntletStates.Find(bot->GetInstanceId()))
            gauntletStates.Reset(bot->GetInstanceId());
        return;
    }

    Creature* controller = GauntletController(bot);
    if (!controller || bot->GetExactDist2d(controller) > GAUNTLET_RANGE)
        return;

    ScanGauntlet(gauntletStates.For(bot->GetInstanceId()), controller, getMSTime());
}
