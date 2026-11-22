/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Valithria.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <list>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AiObjectContext.h"
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
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidResistAura.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr float LIVE_RANGE = 100.0f;
constexpr float SCAN_RANGE = 100.0f;
constexpr uint32 SCAN_MS = 200;
constexpr uint32 DUTY_MS = 500;

constexpr float ADD_RANGE = 50.0f;
constexpr float TAUNT_RANGE = 30.0f;
constexpr float KITE_RANGE = 45.0f;
constexpr float SPLASH_GUARD = 10.0f;

constexpr float COLUMN_RADIUS = 3.0f;
constexpr uint32 COLUMN_LIVE_MS = 2500;
constexpr float BURST_RADIUS = 15.0f;
constexpr int32 BURST_HEALTH_PCT = 15;
constexpr float VOID_RADIUS = 6.0f;
constexpr float DANGER_PAD = 0.75f;
constexpr float CLEAR_PAD = 2.5f;
constexpr float DODGE_SWEEP = 20.0f;

constexpr float KITE_ZOMBIE_CLEAR = 6.0f;

constexpr float PORTAL_AT = 2.0f;
constexpr float HEAL_CHECK_RANGE = 40.0f;
constexpr float RAID_LOW_PCT = 40.0f;
constexpr float RAID_TOPPED_PCT = 85.0f;

constexpr int32 DESCENT_MS = 3500;
constexpr int32 REFRESHED_MS = 19000;
constexpr float GATHER_RANGE = 6.0f;
constexpr float CLOUD_ARRIVE = 3.0f;
constexpr uint32 CLOUD_POP_MS = 1300;
constexpr uint32 CLOUD_FORGET_MS = 5000;
constexpr float LAND_HEIGHT = 1.0f;

constexpr uint8 HEALER_RAID = 1;
constexpr uint8 HEALER_PORTAL = 2;

constexpr uint32 HUMAN_CALL_CHECK_MS = 500;
constexpr float MELEE_INTERRUPT_RANGE = 5.0f;
constexpr float FREEDOM_RANGE = 30.0f;
constexpr float GUARDIAN_RANGE = 40.0f;
constexpr float BEACON_RANGE = 60.0f;
constexpr int32 BEACON_REFRESH_MS = 5000;

constexpr char const* KICKS[] = {"kick", "pummel", "shield bash", "mind freeze", "counterspell", "wind shear"};

struct Freedom
{
    char const* spell;
    char const* label;
};
constexpr Freedom FREEDOMS[] = {{"hand of freedom", "hand"}, {"cleanse", "dispel"}, {"dispel magic", "dispel"}};

struct Snare
{
    char const* spell;
    float range;
};
constexpr Snare SNARES[] = {{"chains of ice", 20.0f}, {"concussive shot", 30.0f}, {"frost shock", 20.0f}};
constexpr float SNARE_MAX_RANGE = 30.0f;

constexpr uint32 ADD_ENTRIES[] = {NPC_BLAZING_SKELETON, NPC_RISEN_ARCHMAGE, NPC_ROT_WORM,
                                  NPC_SUPPRESSER,       NPC_BLISTERING_ZOMBIE, NPC_GLUTTONOUS_ABOMINATION};

struct Stand
{
    bool set = false;
    Position spot;
    bool parked = false;
};

struct Column
{
    Position pos;
    uint32 seenMs = 0;
};

struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct OwnedMemo
{
    uint32 ms = 0;
    bool set = false;
    bool owned = false;
    bool kiting = false;
};

struct VdState
{
    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<ObjectGuid> adds;
    std::vector<ObjectGuid> zombies;
    std::vector<ObjectGuid> abominations;
    std::vector<ObjectGuid> preEffects;
    std::vector<ObjectGuid> portals;
    std::vector<Position> voids;
    std::unordered_map<ObjectGuid, Column> columns;
    std::vector<ObjectGuid> clouds;

    uint32 dutyMs = 0;
    bool dutiesRan = false;
    RaidObs::ObsGuidMap<uint8> healer{"vd.healer"};
    RaidObs::ObsGuidMap<ObjectGuid> portalClaim{"vd.portalclaim"};
    std::unordered_map<ObjectGuid, Position> claimSpot;
    RaidObs::ObsGuidMap<ObjectGuid> tank{"vd.tank"};
    std::unordered_map<ObjectGuid, ObjectGuid> snarer;
    std::unordered_map<ObjectGuid, int8> kiteDir;
    std::unordered_map<ObjectGuid, Stand> stands;
    std::unordered_map<ObjectGuid, ObjectGuid> kicks;
    std::unordered_map<ObjectGuid, ObjectGuid> freers;
    uint32 humanCallMs = 0;
    bool humanCallRan = false;

    ObjectGuid dreamLeader;
    ObjectGuid dreamCloud;
    uint32 cloudArrivedMs = 0;
    std::unordered_map<ObjectGuid, uint32> popped;

    std::unordered_map<ObjectGuid, TargetMemo> targets;
    std::unordered_map<ObjectGuid, OwnedMemo> owned;
};

RaidInstanceState<VdState> vdStates;
RaidInstanceState<std::unordered_set<ObjectGuid>> vdRtiChecked;

bool IsTankPlayer(Player* player)
{
    return player && (PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true));
}

bool IsDutyBot(Player* player) { return player && GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool IsHealer(Player* player) { return player && PlayerbotAI::IsHeal(player) && !IsTankPlayer(player); }

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

void NoteDerived(Player* bot, char const* key, char const* value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
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

bool InProgress(Player* bot)
{
    InstanceScript* instance = IccInstance(bot);
    return instance && instance->GetBossState(ICC_BOSS_VALITHRIA_DREAMWALKER) == IN_PROGRESS;
}

int8 TierOf(uint32 entry)
{
    switch (entry)
    {
        case NPC_BLAZING_SKELETON:
        case NPC_RISEN_ARCHMAGE:
            return 0;
        case NPC_ROT_WORM:
        case NPC_SUPPRESSER:
            return 1;
        case NPC_BLISTERING_ZOMBIE:
        case NPC_GLUTTONOUS_ABOMINATION:
            return 2;
        default:
            return -1;
    }
}

char const* TierName(uint32 entry)
{
    switch (entry)
    {
        case NPC_BLAZING_SKELETON:
            return "skeleton";
        case NPC_RISEN_ARCHMAGE:
            return "archmage";
        case NPC_ROT_WORM:
            return "worm";
        case NPC_SUPPRESSER:
            return "suppresser";
        case NPC_BLISTERING_ZOMBIE:
            return "zombie";
        default:
            return "abomination";
    }
}

bool IsVdAdd(Unit* unit) { return unit && TierOf(unit->GetEntry()) >= 0; }

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

bool Dreaming(Player* player) { return player->HasAura(SPELL_DREAM_STATE); }

VdState* LiveState(PlayerbotAI* botAI, Creature** valithria = nullptr)
{
    Creature* her = IsVdLive(botAI) ? GetVdValithria(botAI) : nullptr;
    if (valithria)
        *valithria = her;

    return her ? vdStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

void Scan(VdState& st, Creature* valithria, uint32 now)
{
    st.adds.clear();
    st.zombies.clear();
    st.abominations.clear();
    for (uint32 entry : ADD_ENTRIES)
    {
        std::list<Creature*> found;
        valithria->GetCreatureListWithEntryInGrid(found, entry, SCAN_RANGE);
        for (Creature* add : found)
        {
            if (!add->IsAlive() || !add->IsInCombat())
                continue;

            if (entry == NPC_BLISTERING_ZOMBIE)
                st.zombies.push_back(add->GetGUID());
            if (add->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
                continue;

            st.adds.push_back(add->GetGUID());
            if (entry == NPC_GLUTTONOUS_ABOMINATION)
                st.abominations.push_back(add->GetGUID());
        }
    }

    std::unordered_map<ObjectGuid, Column> columns;
    std::list<Creature*> found;
    valithria->GetCreatureListWithEntryInGrid(found, NPC_COLUMN_OF_FROST, SCAN_RANGE);
    for (Creature* column : found)
    {
        if (!column->IsAlive())
            continue;

        auto const seen = st.columns.find(column->GetGUID());
        Column& kept = columns[column->GetGUID()];
        kept.pos = column->GetPosition();
        kept.seenMs = seen != st.columns.end() ? seen->second.seenMs : now;
        if (seen == st.columns.end() && RaidObs::Active())
            RaidObs::NoteHazardCircle(valithria->GetMap(),
                                      sSpellMgr->GetSpellIdForDifficulty(SPELL_VD_COLUMN_OF_FROST, valithria),
                                      kept.pos, COLUMN_RADIUS, COLUMN_LIVE_MS);
    }
    st.columns.swap(columns);

    st.voids.clear();
    found.clear();
    valithria->GetCreatureListWithEntryInGrid(found, NPC_MANA_VOID, SCAN_RANGE);
    for (Creature* manaVoid : found)
        if (manaVoid->IsAlive())
            st.voids.push_back(manaVoid->GetPosition());

    st.preEffects.clear();
    st.portals.clear();
    for (uint32 entry : {NPC_DREAM_PORTAL_PRE_EFFECT, NPC_NIGHTMARE_PORTAL_PRE_EFFECT, NPC_DREAM_PORTAL,
                         NPC_NIGHTMARE_PORTAL})
    {
        found.clear();
        valithria->GetCreatureListWithEntryInGrid(found, entry, SCAN_RANGE);
        for (Creature* portal : found)
        {
            if (!portal->IsAlive())
                continue;

            if (entry == NPC_DREAM_PORTAL || entry == NPC_NIGHTMARE_PORTAL)
                st.portals.push_back(portal->GetGUID());
            else if (portal->HasAura(SPELL_VD_DREAM_PORTAL_VISUAL_PRE) ||
                     portal->HasAura(SPELL_VD_NIGHTMARE_PORTAL_VISUAL_PRE))
                st.preEffects.push_back(portal->GetGUID());
        }
    }

    st.clouds.clear();
    InstanceScript* instance = valithria->GetInstanceScript();
    Creature* trigger = instance ? ObjectAccessor::GetCreature(*valithria,
                                                                instance->GetGuidData(VD_DATA_COMBAT_TRIGGER_GUID))
                                 : nullptr;
    if (!trigger)
        return;

    for (uint32 entry : {NPC_DREAM_CLOUD, NPC_NIGHTMARE_CLOUD})
    {
        found.clear();
        trigger->GetCreatureListWithEntryInGrid(found, entry, SCAN_RANGE);
        for (Creature* cloud : found)
            if (cloud->IsAlive())
                st.clouds.push_back(cloud->GetGUID());
    }
}

bool ColumnLive(Column const& column, uint32 now) { return getMSTimeDiff(column.seenMs, now) < COLUMN_LIVE_MS; }

bool ZombieDying(Creature* zombie)
{
    if (zombie->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    uint32 const acidBurst = sSpellMgr->GetSpellIdForDifficulty(SPELL_ACID_BURST, zombie);
    return zombie->HealthBelowPct(BURST_HEALTH_PCT) || zombie->FindCurrentSpellBySpellId(acidBurst);
}

bool HoldsAbomination(VdState const& st, Player* bot)
{
    auto const itr = st.tank.find(bot->GetGUID());
    Creature* abomination = itr != st.tank.end() ? bot->GetMap()->GetCreature(itr->second) : nullptr;
    return abomination && abomination->IsAlive() && abomination->GetVictim() == bot;
}

Creature* ChasingZombie(VdState const& st, Player* bot)
{
    for (Creature* zombie : Resolve(bot, st.zombies))
        if (zombie->GetVictim() == bot && !zombie->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
            bot->GetExactDist2d(zombie) <= KITE_RANGE)
            return zombie;

    return nullptr;
}

bool Kiting(VdState const& st, Player* bot)
{
    return !Dreaming(bot) && ChasingZombie(st, bot) && !(IsTankPlayer(bot) && HoldsAbomination(st, bot));
}

char const* Danger(VdState const& st, Player* bot)
{
    if (Dreaming(bot) || Kiting(st, bot))
        return nullptr;

    uint32 const now = getMSTime();
    for (auto const& [guid, column] : st.columns)
        if (ColumnLive(column, now) && bot->GetExactDist2d(column.pos) < COLUMN_RADIUS + DANGER_PAD)
            return "column";

    for (Creature* zombie : Resolve(bot, st.zombies))
        if (zombie->GetVictim() != bot && ZombieDying(zombie) &&
            bot->GetExactDist2d(zombie) < BURST_RADIUS + DANGER_PAD)
            return "burst";

    return nullptr;
}

bool InVoid(VdState const& st, Player* bot)
{
    if (bot->getPowerType() != POWER_MANA || Dreaming(bot) || Kiting(st, bot))
        return false;

    for (Position const& manaVoid : st.voids)
        if (bot->GetExactDist2d(manaVoid) < VOID_RADIUS + DANGER_PAD)
            return true;

    return false;
}

std::vector<HazardCircle> Hazards(VdState const& st, Player* bot, bool voids)
{
    std::vector<HazardCircle> hazards;
    uint32 const now = getMSTime();
    for (auto const& [guid, column] : st.columns)
        if (ColumnLive(column, now))
            hazards.emplace_back(column.pos, COLUMN_RADIUS + CLEAR_PAD);

    for (Creature* zombie : Resolve(bot, st.zombies))
        if (ZombieDying(zombie))
            hazards.emplace_back(zombie->GetPosition(), BURST_RADIUS + CLEAR_PAD);

    if (voids)
        for (Position const& manaVoid : st.voids)
            hazards.emplace_back(manaVoid, VOID_RADIUS + CLEAR_PAD);

    return hazards;
}

bool ClearSpot(Player* bot, Creature* valithria, std::vector<HazardCircle> const& hazards, Position& spot)
{
    Position const from = bot->GetPosition();
    std::vector<HazardCircle> columns;
    for (HazardCircle const& hazard : hazards)
        if (hazard.second <= COLUMN_RADIUS + CLEAR_PAD)
            columns.push_back(hazard);

    auto const onFloor = [valithria](float x, float y)
    { return valithria->GetExactDist2d(x, y) <= VD_FLOOR_RADIUS; };
    auto const onFloorNoCrossing = [&](float x, float y)
    {
        if (!onFloor(x, y))
            return false;

        Position const to(x, y, from.GetPositionZ());
        for (HazardCircle const& column : columns)
            if (SegmentCrossesCircle(from, to, column))
                return false;

        return true;
    };

    Position found = FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP, 2.0f,
                                                       static_cast<float>(M_PI) / 8.0f, &from, onFloorNoCrossing);
    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
        found = FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP, 2.0f, static_cast<float>(M_PI) / 8.0f,
                                                  &from, onFloor);

    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
        return false;

    spot = found;
    return true;
}

Position RingPoint(Creature* valithria, int32 index)
{
    int32 const wrapped = ((index % VD_KITE_POINTS) + VD_KITE_POINTS) % VD_KITE_POINTS;
    float const angle = 2.0f * static_cast<float>(M_PI) * wrapped / VD_KITE_POINTS;
    return Position(valithria->GetPositionX() + VD_KITE_RADIUS * std::cos(angle),
                    valithria->GetPositionY() + VD_KITE_RADIUS * std::sin(angle), valithria->GetPositionZ());
}

int8 KiteDirection(Creature* valithria, Player* bot, Creature* zombie)
{
    float const bot_ = valithria->GetAngle(bot);
    float const zombie_ = valithria->GetAngle(zombie);
    float delta = bot_ - zombie_;
    while (delta > static_cast<float>(M_PI))
        delta -= 2.0f * static_cast<float>(M_PI);
    while (delta <= -static_cast<float>(M_PI))
        delta += 2.0f * static_cast<float>(M_PI);

    return delta >= 0.0f ? 1 : -1;
}

Player* LowestGuid(std::vector<Player*> const& players)
{
    Player* lowest = nullptr;
    for (Player* player : players)
        if (!lowest || player->GetGUID() < lowest->GetGUID())
            lowest = player;

    return lowest;
}

uint32 VigorId(Player* bot) { return sSpellMgr->GetSpellIdForDifficulty(SPELL_EMERALD_VIGOR, bot); }

bool Refreshed(Player* player)
{
    Aura* dream = player->GetAura(SPELL_DREAM_STATE);
    Aura* vigor = player->GetAura(VigorId(player));
    return dream && vigor && vigor->GetDuration() - dream->GetDuration() >= REFRESHED_MS;
}

uint8 NightmareStacks(Player* bot)
{
    if (!IsHeroic(bot))
        return 0;

    Aura* nightmares = bot->GetAura(VigorId(bot));
    return nightmares ? nightmares->GetStackAmount() : 0;
}

Creature* RealPortalAt(VdState const& st, Player* bot, Position const& spot)
{
    for (Creature* portal : Resolve(bot, st.portals))
        if (portal->GetExactDist2d(spot) <= PORTAL_AT)
            return portal;

    return nullptr;
}

void AssignHealers(VdState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<Player*> healers;
    for (Player* member : members)
        if (IsDutyBot(member) && IsHealer(member))
            healers.push_back(member);

    std::sort(healers.begin(), healers.end(),
              [](Player* a, Player* b)
              {
                  return std::make_tuple(a->getClass() == CLASS_PALADIN, a->GetGUID()) <
                         std::make_tuple(b->getClass() == CLASS_PALADIN, b->GetGUID());
              });

    size_t const wanted = IsRaid25Man(bot) ? 2 : 1;
    size_t const raid = healers.empty() ? 0 : std::min(wanted, healers.size() - 1);

    std::vector<ObjectGuid> gone;
    for (auto const& [guid, duty] : st.healer)
        if (std::none_of(healers.begin(), healers.end(), [&](Player* healer) { return healer->GetGUID() == guid; }))
            gone.push_back(guid);
    for (ObjectGuid const& guid : gone)
        st.healer.erase(guid);

    for (size_t i = 0; i < healers.size(); ++i)
    {
        uint8 const duty = i < raid ? HEALER_RAID : HEALER_PORTAL;
        auto const itr = st.healer.find(healers[i]->GetGUID());
        if (itr == st.healer.end() || itr->second != duty)
            st.healer[healers[i]->GetGUID()] = duty;
    }
}

void AssignPortals(VdState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<ObjectGuid> lapsed;
    for (auto const& [guid, portal] : st.portalClaim)
    {
        Player* healer = ObjectAccessor::GetPlayer(*bot, guid);
        auto const spot = st.claimSpot.find(guid);
        auto const duty = st.healer.find(guid);
        bool const waiting = std::find(st.preEffects.begin(), st.preEffects.end(), portal) != st.preEffects.end();
        bool const open = spot != st.claimSpot.end() && RealPortalAt(st, bot, spot->second);
        if (!healer || !healer->IsAlive() || Dreaming(healer) || duty == st.healer.end() ||
            duty->second != HEALER_PORTAL || (!waiting && !open))
            lapsed.push_back(guid);
    }
    for (ObjectGuid const& guid : lapsed)
    {
        st.portalClaim.erase(guid);
        st.claimSpot.erase(guid);
    }

    std::vector<Player*> healers;
    for (Player* member : members)
    {
        auto const duty = st.healer.find(member->GetGUID());
        if (duty == st.healer.end() || duty->second != HEALER_PORTAL || st.portalClaim.count(member->GetGUID()) ||
            Dreaming(member) || Kiting(st, member) || NightmareStacks(member) >= VD_NIGHTMARE_STACK_CAP)
            continue;

        healers.push_back(member);
    }
    std::sort(healers.begin(), healers.end(), IccGuidLess());

    std::vector<Creature*> spots = Resolve(bot, st.preEffects);
    for (Creature* portal : Resolve(bot, st.portals))
        spots.push_back(portal);

    for (Player* healer : healers)
    {
        Creature* best = nullptr;
        for (Creature* portal : spots)
        {
            bool taken = false;
            for (auto const& [guid, spot] : st.claimSpot)
                taken = taken || portal->GetExactDist2d(spot) <= PORTAL_AT;
            if (taken)
                continue;

            if (!best || healer->GetExactDist2d(portal) < healer->GetExactDist2d(best))
                best = portal;
        }

        if (!best)
            break;

        st.portalClaim[healer->GetGUID()] = best->GetGUID();
        st.claimSpot[healer->GetGUID()] = best->GetPosition();
    }
}

void AssignAbominations(VdState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<ObjectGuid> lapsed;
    for (auto const& [holder, guid] : st.tank)
    {
        Creature* abomination = bot->GetMap()->GetCreature(guid);
        Player* member = ObjectAccessor::GetPlayer(*bot, holder);
        if (!abomination || !abomination->IsAlive() || !member || !member->IsAlive() ||
            !IccSameInstance(bot, member))
            lapsed.push_back(holder);
    }
    for (ObjectGuid const& holder : lapsed)
        st.tank.erase(holder);

    for (Creature* abomination : Resolve(bot, st.abominations))
    {
        bool owned = false;
        for (auto const& [holder, guid] : st.tank)
            owned = owned || guid == abomination->GetGUID();
        if (owned)
            continue;

        Player* best = nullptr;
        for (Player* member : members)
        {
            if (!IsDutyBot(member) || !IsTankPlayer(member) || Dreaming(member) || st.tank.count(member->GetGUID()))
                continue;

            if (!best || std::make_tuple(member->GetExactDist2d(abomination), member->GetGUID()) <
                             std::make_tuple(best->GetExactDist2d(abomination), best->GetGUID()))
                best = member;
        }

        if (best)
            st.tank[best->GetGUID()] = abomination->GetGUID();
    }
}

char const* ReadySnare(Player* member, Unit* zombie)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI || member->GetExactDist(zombie) > SNARE_MAX_RANGE || !member->IsWithinLOSInMap(zombie))
        return nullptr;

    for (Snare const& snare : SNARES)
        if (member->GetExactDist(zombie) <= snare.range && memberAI->CanCastSpell(snare.spell, zombie))
            return snare.spell;

    return nullptr;
}

void AssignSnares(VdState& st, Player* bot, std::vector<Player*> const& members)
{
    std::unordered_map<ObjectGuid, ObjectGuid> kept;
    for (Creature* zombie : Resolve(bot, st.zombies))
    {
        if (!zombie->GetVictim() || zombie->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ||
            zombie->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
            continue;

        Player* best = nullptr;
        for (Player* member : members)
        {
            if (!IsDutyBot(member) || IsHealer(member) || Dreaming(member) || !ReadySnare(member, zombie))
                continue;

            if (!best || std::make_tuple(member->GetExactDist(zombie), member->GetGUID()) <
                             std::make_tuple(best->GetExactDist(zombie), best->GetGUID()))
                best = member;
        }

        if (best)
            kept[zombie->GetGUID()] = best->GetGUID();
    }

    st.snarer.swap(kept);
}

void UpdateKites(VdState& st, Creature* valithria, std::vector<Player*> const& members)
{
    std::unordered_map<ObjectGuid, int8> kept;
    for (Player* member : members)
    {
        Creature* zombie = Kiting(st, member) ? ChasingZombie(st, member) : nullptr;
        if (!zombie)
            continue;

        auto const itr = st.kiteDir.find(member->GetGUID());
        kept[member->GetGUID()] = itr != st.kiteDir.end() ? itr->second : KiteDirection(valithria, member, zombie);
    }

    st.kiteDir.swap(kept);
}

void UpdateDream(VdState& st, Player* bot, std::vector<Player*> const& members, uint32 now)
{
    std::vector<Player*> group;
    for (Player* member : members)
        if (IsDutyBot(member) && Dreaming(member) && Refreshed(member))
            group.push_back(member);

    for (auto itr = st.popped.begin(); itr != st.popped.end();)
        itr = getMSTimeDiff(itr->second, now) > CLOUD_FORGET_MS ? st.popped.erase(itr) : std::next(itr);

    Player* leader = LowestGuid(group);
    st.dreamLeader = leader ? leader->GetGUID() : ObjectGuid::Empty;
    ObjectGuid const before = st.dreamCloud;
    if (!leader)
    {
        st.dreamCloud = ObjectGuid::Empty;
        st.cloudArrivedMs = 0;
        if (!before.IsEmpty())
            NoteDerived(bot, "vd.cloud", "none");
        return;
    }

    std::vector<Creature*> clouds = Resolve(bot, st.clouds);
    Creature* target = nullptr;
    for (Creature* cloud : clouds)
        if (cloud->GetGUID() == st.dreamCloud)
            target = cloud;

    if (target && leader->GetExactDist(target) <= CLOUD_ARRIVE)
    {
        if (!st.cloudArrivedMs)
            st.cloudArrivedMs = now;
        else if (getMSTimeDiff(st.cloudArrivedMs, now) >= CLOUD_POP_MS)
        {
            st.popped[target->GetGUID()] = now;
            target = nullptr;
        }
    }
    else
        st.cloudArrivedMs = 0;

    if (!target)
    {
        st.cloudArrivedMs = 0;
        for (Creature* cloud : clouds)
            if (!st.popped.count(cloud->GetGUID()) &&
                (!target || leader->GetExactDist(cloud) < leader->GetExactDist(target)))
                target = cloud;
    }

    st.dreamCloud = target ? target->GetGUID() : ObjectGuid::Empty;
    if (st.dreamCloud != before)
        NoteDerived(bot, "vd.cloud",
                    target ? "cloud " + RaidObs::DescribeAssignment(target->GetGUID()) : std::string("none"));
}

void UpdateStand(VdState& st, Player* bot)
{
    Stand& stand = st.stands[bot->GetGUID()];
    auto const spot = st.claimSpot.find(bot->GetGUID());
    if (spot == st.claimSpot.end() || Dreaming(bot))
    {
        stand.set = false;
        return;
    }

    if (!stand.set || stand.spot.GetExactDist2d(spot->second) > 0.5f)
    {
        stand.set = true;
        stand.spot = spot->second;
        stand.parked = false;
    }

    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= VD_STAND_UNPARK : distance <= VD_STAND_PARK;
}

void NoteWave(VdState const& st, Player* bot)
{
    if (!RaidObs::Active() || !IsDutyBot(bot) || !IsHealer(bot) || Dreaming(bot))
        return;

    NoteDerived(bot, "vd.wave", !st.portals.empty() ? "open" : !st.preEffects.empty() ? "pre" : "none");
}

Unit* ResolveTarget(PlayerbotAI* botAI, VdState const& st, Creature* valithria)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if ((!tank && !IsRaidDpsBot(bot)) || Dreaming(bot) || Kiting(st, bot))
        return nullptr;

    if (Unit* skull = GetRaidHumanSkull(botAI, &IsVdAdd))
    {
        NoteDerived(bot, "vd.target", "skull");
        return skull;
    }

    if (tank)
    {
        auto const itr = st.tank.find(bot->GetGUID());
        Creature* abomination = itr != st.tank.end() ? bot->GetMap()->GetCreature(itr->second) : nullptr;
        if (abomination && abomination->IsAlive() && abomination->IsInCombat())
        {
            NoteDerived(bot, "vd.target", "owned");
            return abomination;
        }
    }

    bool const melee = !PlayerbotAI::IsRanged(bot);
    auto const usable = [&](Creature* add)
    {
        uint32 const entry = add->GetEntry();
        if (valithria->GetExactDist2d(add) > ADD_RANGE || !bot->IsValidAttackTarget(add))
            return false;
        if (entry == NPC_BLISTERING_ZOMBIE)
            return !melee;
        if (entry == NPC_GLUTTONOUS_ABOMINATION)
            return !tank && add->GetVictim() && IsTankPlayer(add->GetVictim()->ToPlayer());
        return true;
    };

    Creature* best = nullptr;
    int8 bestTier = 3;
    for (Creature* add : Resolve(bot, st.adds))
    {
        if (!usable(add))
            continue;

        int8 const tier = TierOf(add->GetEntry());
        if (!best || tier < bestTier ||
            (tier == bestTier && valithria->GetExactDist2d(add) < valithria->GetExactDist2d(best)))
        {
            best = add;
            bestTier = tier;
        }
    }

    Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    Creature* held = current ? current->ToCreature() : nullptr;
    if (best && held && held != best && held->IsAlive() && IsVdAdd(held) && TierOf(held->GetEntry()) == bestTier &&
        std::find(st.adds.begin(), st.adds.end(), held->GetGUID()) != st.adds.end() && usable(held))
        best = held;

    NoteDerived(bot, "vd.target", best ? TierName(best->GetEntry()) : "none");
    return best;
}

bool CanPayFor(Player* bot, SpellInfo const* info)
{
    if (info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(bot->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(bot, info->GetSchoolMask());
}

char const* ReadyKick(Player* member, Unit* caster)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI || member->HasUnitState(UNIT_STATE_CASTING) || member->GetExactDist(caster) > SCAN_RANGE)
        return nullptr;

    for (char const* kick : KICKS)
    {
        uint32 const spellId = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", kick)->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info || !CanPayFor(member, info) || info->CheckShapeshift(member->GetShapeshiftForm()) != SPELL_CAST_OK)
            continue;

        float const range = member->GetSpellMaxRangeForTarget(caster, info);
        bool const inReach = range <= MELEE_INTERRUPT_RANGE ? member->IsWithinMeleeRange(caster)
                                                            : member->IsWithinCombatRange(caster, range);
        if (inReach && memberAI->CanCastSpell(spellId, caster) && member->IsWithinLOSInMap(caster))
            return kick;
    }

    return nullptr;
}

bool CastingVolley(Creature* archmage)
{
    Spell* spell = archmage->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    return spell && spell->GetSpellInfo() &&
           spell->GetSpellInfo()->Id == sSpellMgr->GetSpellIdForDifficulty(SPELL_VD_FROSTBOLT_VOLLEY, archmage);
}

void AssignKicks(VdState& st, Player* bot, std::vector<Player*> const& members)
{
    st.kicks.clear();
    std::vector<Creature*> casters;
    for (Creature* add : Resolve(bot, st.adds))
        if (add->GetEntry() == NPC_RISEN_ARCHMAGE && CastingVolley(add))
            casters.push_back(add);
    std::sort(casters.begin(), casters.end(), IccGuidLess());

    for (Creature* caster : casters)
    {
        Player* best = nullptr;
        std::tuple<float, ObjectGuid> bestKey;
        for (Player* member : members)
        {
            if (!IsDutyBot(member) || Dreaming(member) || Kiting(st, member) || st.kicks.count(member->GetGUID()) ||
                !ReadyKick(member, caster))
                continue;

            auto const key = std::make_tuple(member->GetExactDist(caster), member->GetGUID());
            if (!best || key < bestKey)
            {
                best = member;
                bestKey = key;
            }
        }

        if (best)
            st.kicks[best->GetGUID()] = caster->GetGUID();
    }
}

bool Slowed(Player* kiter)
{
    return kiter->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_VD_FROSTBOLT_VOLLEY, kiter));
}

Freedom const* ReadyFreedom(Player* member, Player* kiter)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI || member->HasUnitState(UNIT_STATE_CASTING) ||
        (member != kiter && (member->GetExactDist(kiter) > FREEDOM_RANGE || !member->IsWithinLOSInMap(kiter))))
        return nullptr;

    for (Freedom const& freedom : FREEDOMS)
        if (memberAI->CanCastSpell(freedom.spell, kiter))
            return &freedom;

    return nullptr;
}

void AssignFreedom(VdState& st, std::vector<Player*> const& members)
{
    st.freers.clear();
    for (Player* kiter : members)
    {
        if (!Kiting(st, kiter) || !Slowed(kiter))
            continue;

        if (IsDutyBot(kiter) && ReadyFreedom(kiter, kiter))
        {
            st.freers[kiter->GetGUID()] = kiter->GetGUID();
            continue;
        }

        Player* best = nullptr;
        std::tuple<bool, float, ObjectGuid> bestKey;
        for (Player* member : members)
        {
            if (member == kiter || !IsDutyBot(member) || Dreaming(member) || Kiting(st, member) ||
                st.freers.count(member->GetGUID()))
                continue;

            Freedom const* freedom = ReadyFreedom(member, kiter);
            if (!freedom)
                continue;

            auto const key = std::make_tuple(freedom != &FREEDOMS[0], member->GetExactDist(kiter), member->GetGUID());
            if (!best || key < bestKey)
            {
                best = member;
                bestKey = key;
            }
        }

        if (best)
            st.freers[best->GetGUID()] = kiter->GetGUID();
    }
}

float TankDefensivePct(Player* tank)
{
    return tank->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_VD_GUT_SPRAY, tank)) ? VD_TANK_SPRAYED_PCT
                                                                                      : VD_TANK_DEFENSIVE_PCT;
}

bool AbominationHits(VdState const& st, Player* bot, Player* member)
{
    for (Creature* abomination : Resolve(bot, st.abominations))
        if (abomination->GetVictim() == member)
            return true;

    return false;
}

void CallHumanTank(VdState& st, Player* sender, std::vector<Player*> const& members, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;
    for (Player* member : members)
    {
        if (IsDutyBot(member) || !IsTankPlayer(member) || member->GetHealthPct() >= TankDefensivePct(member) ||
            !AbominationHits(st, sender, member))
            continue;

        if (uint32 const spellId = ReadyTankDefensiveSpell(member, true, 0, 0))
            CallHumanTankDefensive(sender, member, spellId, 0, "you are low", "vd.humancall");
    }
}

bool PushWindow(Creature* valithria, Player* bot)
{
    if (valithria->GetHealthPct() < VD_PUSH_PCT)
        return false;

    bool vigor = false;
    for (Player* member : MembersOnMap(bot))
    {
        if (!IsHealer(member))
            continue;

        if (IsDutyBot(member) && Dreaming(member))
            return false;

        vigor = vigor || member->HasAura(VigorId(member));
    }

    return vigor;
}

bool LustRunning(Player* bot)
{
    for (Player* member : MembersOnMap(bot))
        if (member->HasAura(SPELL_VD_BLOODLUST) || member->HasAura(SPELL_VD_HEROISM))
            return true;

    return false;
}

char const* ReadyLust(Player* shaman)
{
    PlayerbotAI* shamanAI = GET_PLAYERBOT_AI(shaman);
    if (!shamanAI || shaman->getClass() != CLASS_SHAMAN || shaman->HasAura(SPELL_VD_EXHAUSTION) ||
        shaman->HasAura(SPELL_VD_SATED))
        return nullptr;

    for (char const* lust : {"heroism", "bloodlust"})
        if (shamanAI->CanCastSpell(lust, shaman))
            return lust;

    return nullptr;
}

bool CanGuardSpirit(Player* priest, Creature* valithria)
{
    PlayerbotAI* priestAI = GET_PLAYERBOT_AI(priest);
    return priestAI && priest->getClass() == CLASS_PRIEST && priest->GetExactDist(valithria) <= GUARDIAN_RANGE &&
           priestAI->CanCastSpell("guardian spirit", valithria) && priest->IsWithinLOSInMap(valithria);
}

bool LandingPending(Player* bot)
{
    if (!bot->HasUnitMovementFlag(MOVEMENTFLAG_FLYING | MOVEMENTFLAG_DISABLE_GRAVITY) || bot->HasFlyAura() ||
        bot->HasAura(SPELL_DREAM_STATE))
        return false;

    return bot->GetMapId() == ICC_MAP_ID && bot->GetExactDist2d(VD_HOME_X, VD_HOME_Y) <= LIVE_RANGE;
}
}

Creature* GetVdValithria(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Creature* valithria = SlotCreature(bot, VD_DATA_VALITHRIA_GUID);
    return valithria && valithria->IsAlive() && bot->GetExactDist2d(valithria) <= LIVE_RANGE ? valithria : nullptr;
}

bool IsVdLive(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    return bot && InProgress(bot) && GetVdValithria(botAI);
}

bool IsVdDreaming(PlayerbotAI* botAI) { return IsVdLive(botAI) && Dreaming(botAI->GetBot()); }

Unit* GetVdTarget(PlayerbotAI* botAI)
{
    Creature* valithria = nullptr;
    VdState* st = LiveState(botAI, &valithria);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, *st, valithria);
    }

    return memo.pick;
}

Unit* GetVdTankTaunt(PlayerbotAI* botAI)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsDutyBot(bot) || !IsTankPlayer(bot) || Dreaming(bot) || !ClassTauntReady(botAI))
        return nullptr;

    auto const reachable = [bot](Unit* unit)
    { return bot->GetExactDist(unit) <= TAUNT_RANGE && bot->IsWithinLOSInMap(unit); };

    auto const owned = st->tank.find(bot->GetGUID());
    Creature* abomination = owned != st->tank.end() ? bot->GetMap()->GetCreature(owned->second) : nullptr;
    bool const holding = abomination && abomination->IsAlive();
    if (holding && abomination->GetVictim() != bot && reachable(abomination))
        return abomination;

    auto const onHealer = [](Unit* add)
    {
        Player* victim = add->GetVictim() ? add->GetVictim()->ToPlayer() : nullptr;
        return victim && IsHealer(victim);
    };

    Creature* best = nullptr;
    for (Creature* add : Resolve(bot, st->adds))
    {
        bool const wanted = add->GetEntry() == NPC_ROT_WORM ||
                            (!holding && add->GetEntry() == NPC_BLISTERING_ZOMBIE);
        if (!wanted || !onHealer(add) || !reachable(add))
            continue;

        if (!best || bot->GetExactDist(add) < bot->GetExactDist(best))
            best = add;
    }

    return best;
}

Unit* GetVdZombieToSnare(PlayerbotAI* botAI, char const*& spell)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    spell = nullptr;
    if (!st || !IsDutyBot(bot) || IsHealer(bot))
        return nullptr;

    for (auto const& [zombieGuid, snarer] : st->snarer)
    {
        if (snarer != bot->GetGUID())
            continue;

        Creature* zombie = bot->GetMap()->GetCreature(zombieGuid);
        if (!zombie || !zombie->IsAlive() || zombie->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
            continue;

        spell = ReadySnare(bot, zombie);
        if (spell)
        {
            NoteDerived(bot, "vd.snare", spell);
            return zombie;
        }
    }

    NoteDerived(bot, "vd.snare", "none");
    return nullptr;
}

bool IsVdInDanger(PlayerbotAI* botAI)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    char const* danger = Danger(*st, bot);
    if (!danger)
        NoteDerived(bot, "vd.dodge", "none");

    return danger != nullptr;
}

bool FindVdDodgeSpot(PlayerbotAI* botAI, Position& spot)
{
    Creature* valithria = nullptr;
    VdState* st = LiveState(botAI, &valithria);
    Player* bot = botAI->GetBot();
    char const* danger = st ? Danger(*st, bot) : nullptr;
    if (!danger)
        return false;

    if (!ClearSpot(bot, valithria, Hazards(*st, bot, bot->getPowerType() == POWER_MANA), spot))
    {
        NoteDerived(bot, "vd.dodge", "blocked");
        return false;
    }

    NoteDerived(bot, "vd.dodge", danger);
    return true;
}

bool GetVdVoidStep(PlayerbotAI* botAI, Position& spot)
{
    Creature* valithria = nullptr;
    VdState* st = LiveState(botAI, &valithria);
    Player* bot = botAI->GetBot();
    if (!st || bot->getPowerType() != POWER_MANA)
        return false;

    if (!InVoid(*st, bot) || Danger(*st, bot))
    {
        NoteDerived(bot, "vd.void", "none");
        return false;
    }

    if (!ClearSpot(bot, valithria, Hazards(*st, bot, true), spot))
    {
        NoteDerived(bot, "vd.void", "blocked");
        return false;
    }

    NoteDerived(bot, "vd.void", "step");
    return true;
}

bool IsVdKiting(PlayerbotAI* botAI)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    bool const kiting = Kiting(*st, bot);
    if (!kiting)
        NoteDerived(bot, "vd.kite", "none");

    return kiting;
}

bool GetVdKiteLeg(PlayerbotAI* botAI, Position& leg)
{
    Creature* valithria = nullptr;
    VdState* st = LiveState(botAI, &valithria);
    Player* bot = botAI->GetBot();
    Creature* zombie = st && Kiting(*st, bot) ? ChasingZombie(*st, bot) : nullptr;
    if (!zombie)
        return false;

    auto const latched = st->kiteDir.find(bot->GetGUID());
    int8 const dir = latched != st->kiteDir.end() ? latched->second : KiteDirection(valithria, bot, zombie);
    float const step = 2.0f * static_cast<float>(M_PI) / VD_KITE_POINTS;
    int32 const ringSlot = static_cast<int32>(std::lround(valithria->GetAngle(bot) / step));

    uint32 const now = getMSTime();
    Position const from = bot->GetPosition();
    HazardCircle const chaser(zombie->GetPosition(), KITE_ZOMBIE_CLEAR);
    std::vector<HazardCircle> bursts;
    for (Creature* other : Resolve(bot, st->zombies))
        if (other != zombie && ZombieDying(other))
            bursts.emplace_back(other->GetPosition(), BURST_RADIUS + CLEAR_PAD);

    auto const clear = [&](Position const& to)
    {
        if (SegmentCrossesCircle(from, to, chaser) || to.GetExactDist2d(zombie) < 2.0f * KITE_ZOMBIE_CLEAR)
            return false;

        for (auto const& [guid, column] : st->columns)
            if (ColumnLive(column, now) && SegmentCrossesCircle(from, to, {column.pos, COLUMN_RADIUS + CLEAR_PAD}))
                return false;

        for (HazardCircle const& burst : bursts)
            if (to.GetExactDist2d(burst.first) < burst.second ||
                (from.GetExactDist2d(burst.first) >= burst.second && SegmentCrossesCircle(from, to, burst)))
                return false;

        return true;
    };

    for (int32 hops : {2, 3, 1, -2, -3})
    {
        Position const to = RingPoint(valithria, ringSlot + hops * dir);
        if (!clear(to))
            continue;

        leg = to;
        NoteDerived(bot, "vd.kite", hops > 0 ? (dir > 0 ? "ring ccw" : "ring cw") : "reverse");
        return true;
    }

    Position farthest;
    for (int32 index = 0; index < VD_KITE_POINTS; ++index)
    {
        Position const to = RingPoint(valithria, index);
        if (index == 0 || to.GetExactDist2d(zombie) > farthest.GetExactDist2d(zombie))
            farthest = to;
    }

    leg = farthest;
    NoteDerived(bot, "vd.kite", "blocked");
    return true;
}

bool GetVdPortalWalk(PlayerbotAI* botAI, Position& spot, Creature*& portal)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    portal = nullptr;
    if (!st || Dreaming(bot))
        return false;

    auto const claim = st->claimSpot.find(bot->GetGUID());
    if (claim == st->claimSpot.end())
        return false;

    spot = claim->second;
    portal = RealPortalAt(*st, bot, spot);
    if (portal)
        return true;

    for (Position const& manaVoid : st->voids)
        if (manaVoid.GetExactDist2d(spot) < VOID_RADIUS + DANGER_PAD)
            return false;

    auto const stand = st->stands.find(bot->GetGUID());
    return stand == st->stands.end() || !stand->second.set || !stand->second.parked;
}

bool IsVdHoldingPortal(PlayerbotAI* botAI)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    return st && st->claimSpot.count(bot->GetGUID()) && !Dreaming(bot);
}

Creature* GetVdHealTarget(PlayerbotAI* botAI)
{
    Creature* valithria = nullptr;
    VdState* st = LiveState(botAI, &valithria);
    Player* bot = botAI->GetBot();
    if (!st || !IsDutyBot(bot) || !IsHealer(bot) || Dreaming(bot) || Kiting(*st, bot))
        return nullptr;

    auto const duty = st->healer.find(bot->GetGUID());
    bool const raid = duty != st->healer.end() && duty->second == HEALER_RAID;
    for (Player* member : MembersOnMap(bot))
    {
        if (Dreaming(member) || bot->GetExactDist(member) > HEAL_CHECK_RANGE)
            continue;

        float const health = member->GetHealthPct();
        if ((raid && health < RAID_TOPPED_PCT) || (!raid && health < RAID_LOW_PCT))
            return nullptr;
    }

    return valithria;
}

VdFlight GetVdFlight(PlayerbotAI* botAI, Position& dest)
{
    Player* bot = botAI->GetBot();
    if (LandingPending(bot))
    {
        NoteDerived(bot, "vd.dream", "land");
        return VdFlight::Land;
    }

    Creature* valithria = GetVdValithria(botAI);
    if (!valithria)
        return VdFlight::None;

    Aura* dream = bot->GetAura(SPELL_DREAM_STATE);
    if (!dream)
    {
        if (IsDutyBot(bot) && IsHealer(bot))
            NoteDerived(bot, "vd.dream", "none");
        return VdFlight::None;
    }

    if (dream->GetDuration() <= DESCENT_MS)
    {
        float const angle = valithria->GetAngle(bot);
        float const radius = std::min(valithria->GetExactDist2d(bot), VD_KITE_RADIUS);
        dest.Relocate(valithria->GetPositionX() + radius * std::cos(angle),
                      valithria->GetPositionY() + radius * std::sin(angle), valithria->GetPositionZ() + LAND_HEIGHT);
        NoteDerived(bot, "vd.dream", "descend");
        return VdFlight::Descend;
    }

    std::vector<Player*> humans;
    for (Player* member : MembersOnMap(bot))
        if (!IsDutyBot(member) && Dreaming(member))
            humans.push_back(member);
    if (Player* human = LowestGuid(humans))
    {
        dest = human->GetPosition();
        NoteDerived(bot, "vd.dream", "human");
        return VdFlight::Human;
    }

    VdState* st = vdStates.Find(bot->GetInstanceId());
    std::vector<Creature*> clouds = st ? Resolve(bot, st->clouds) : std::vector<Creature*>();
    if (st && !Refreshed(bot))
    {
        Creature* nearest = nullptr;
        for (Creature* cloud : clouds)
            if (!st->popped.count(cloud->GetGUID()) &&
                (!nearest || bot->GetExactDist(cloud) < bot->GetExactDist(nearest)))
                nearest = cloud;

        if (nearest)
        {
            dest = nearest->GetPosition();
            NoteDerived(bot, "vd.dream", "refresh");
            return VdFlight::Refresh;
        }
    }
    else if (st)
    {
        Player* leader = st->dreamLeader.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, st->dreamLeader);
        if (leader && leader != bot && bot->GetExactDist(leader) > GATHER_RANGE)
        {
            dest = leader->GetPosition();
            NoteDerived(bot, "vd.dream", "gather");
            return VdFlight::Gather;
        }

        for (Creature* cloud : clouds)
            if (cloud->GetGUID() == st->dreamCloud)
            {
                dest = cloud->GetPosition();
                NoteDerived(bot, "vd.dream", "cloud");
                return VdFlight::Cloud;
            }
    }

    dest = bot->GetPosition();
    NoteDerived(bot, "vd.dream", "hover");
    return VdFlight::Hover;
}

bool VdGateOpen(PlayerbotAI* botAI, uint32 encounterId)
{
    if (BossStateGateOpen(botAI, encounterId))
        return true;

    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !LandingPending(bot))
        return false;

    InstanceScript* instance = IccInstance(bot);
    return instance && instance->GetBossState(ICC_BOSS_VALITHRIA_DREAMWALKER) == DONE;
}

bool IsVdPushWindow(PlayerbotAI* botAI)
{
    Creature* valithria = nullptr;
    VdState* st = LiveState(botAI, &valithria);
    if (!st)
        return false;

    bool const open = PushWindow(valithria, botAI->GetBot());
    NoteDerived(botAI->GetBot(), "vd.push", open ? "open" : "closed");
    return open;
}

char const* GetVdLust(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_SHAMAN || !IsDutyBot(bot) || Dreaming(bot))
        return nullptr;

    VdState* st = LiveState(botAI);
    char const* lust = st && !Kiting(*st, bot) ? ReadyLust(bot) : nullptr;
    if (!lust || !IsVdPushWindow(botAI) || LustRunning(bot))
        return nullptr;

    for (Player* member : MembersOnMap(bot))
        if (member->GetGUID() < bot->GetGUID() && IsDutyBot(member) && !Dreaming(member) && !Kiting(*st, member) &&
            ReadyLust(member))
            return nullptr;

    return lust;
}

Creature* GetVdGuardianSpiritTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_PRIEST || !IsDutyBot(bot) || !IsHealer(bot) || Dreaming(bot))
        return nullptr;

    Creature* valithria = nullptr;
    VdState* st = LiveState(botAI, &valithria);
    if (!st || Kiting(*st, bot) || valithria->HasAura(SPELL_VD_GUARDIAN_SPIRIT) || !CanGuardSpirit(bot, valithria) ||
        !IsVdPushWindow(botAI))
        return nullptr;

    for (Player* member : MembersOnMap(bot))
        if (member->GetGUID() < bot->GetGUID() && IsDutyBot(member) && IsHealer(member) && !Dreaming(member) &&
            !Kiting(*st, member) && CanGuardSpirit(member, valithria))
            return nullptr;

    return valithria;
}

bool IsVdBeaconOnHer(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_PALADIN || !IsDutyBot(bot) || !IsHealer(bot))
        return false;

    VdState* st = LiveState(botAI);
    if (!st)
        return false;

    auto const duty = st->healer.find(bot->GetGUID());
    return duty != st->healer.end() && (duty->second == HEALER_RAID || IsHeroic(bot));
}

Creature* GetVdBeaconTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_PALADIN || !IsDutyBot(bot) || !IsHealer(bot))
        return nullptr;

    bool const onHer = IsVdBeaconOnHer(botAI);
    NoteDerived(bot, "vd.beacon", onHer ? "her" : "none");
    Creature* valithria = onHer ? GetVdValithria(botAI) : nullptr;
    if (!valithria || Dreaming(bot) || bot->GetExactDist(valithria) > BEACON_RANGE)
        return nullptr;

    Aura* beacon = valithria->GetAura(SPELL_VD_BEACON_OF_LIGHT, bot->GetGUID());
    if (beacon && beacon->GetDuration() > BEACON_REFRESH_MS)
        return nullptr;

    return botAI->CanCastSpell("beacon of light", valithria) && bot->IsWithinLOSInMap(valithria) ? valithria
                                                                                                 : nullptr;
}

Unit* GetVdInterrupt(PlayerbotAI* botAI, char const*& spell)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    spell = nullptr;
    if (!st || !IsDutyBot(bot))
        return nullptr;

    auto const itr = st->kicks.find(bot->GetGUID());
    Creature* archmage = itr != st->kicks.end() ? bot->GetMap()->GetCreature(itr->second) : nullptr;
    spell = archmage && archmage->IsAlive() && CastingVolley(archmage) ? ReadyKick(bot, archmage) : nullptr;
    NoteDerived(bot, "vd.interrupt", spell ? "volley" : "none");
    return spell ? archmage : nullptr;
}

Unit* GetVdFreedom(PlayerbotAI* botAI, char const*& spell)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    spell = nullptr;
    if (!st || !IsDutyBot(bot))
        return nullptr;

    auto const itr = st->freers.find(bot->GetGUID());
    Player* kiter = itr != st->freers.end() ? ObjectAccessor::GetPlayer(*bot, itr->second) : nullptr;
    Freedom const* freedom = kiter && kiter->IsAlive() && Slowed(kiter) ? ReadyFreedom(bot, kiter) : nullptr;
    NoteDerived(bot, "vd.freedom", freedom ? freedom->label : "none");
    if (!freedom)
        return nullptr;

    spell = freedom->spell;
    return kiter;
}

uint32 GetVdTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !bot->IsAlive() || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return 0;

    if (bot->GetHealthPct() >= TankDefensivePct(bot) || !HoldsAbomination(*st, bot))
    {
        if (noteKind)
            NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    return NextTankDefensive(botAI, bot, noteKind, true);
}

bool IsVdTankDefensiveHeld(PlayerbotAI* botAI)
{
    return LiveState(botAI) && botAI->GetBot()->GetHealthPct() >= VD_TANK_LAST_RESORT_PCT;
}

namespace
{
OwnedMemo const* Owned(PlayerbotAI* botAI)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return nullptr;

    OwnedMemo& memo = st->owned[bot->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.kiting = Kiting(*st, bot);
        memo.owned = memo.kiting || Dreaming(bot) || st->claimSpot.count(bot->GetGUID()) || Danger(*st, bot) ||
                     InVoid(*st, bot);
    }

    return &memo;
}
}

bool IsVdMovementOwned(PlayerbotAI* botAI)
{
    OwnedMemo const* memo = Owned(botAI);
    return memo && memo->owned;
}

bool IsVdKiteHeld(PlayerbotAI* botAI)
{
    OwnedMemo const* memo = Owned(botAI);
    return memo && memo->kiting;
}

bool IsVdSplashHeld(PlayerbotAI* botAI)
{
    VdState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || IsTankPlayer(bot))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    for (Creature* zombie : Resolve(bot, st->zombies))
    {
        if (zombie->GetVictim() == bot || zombie->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            continue;

        if (zombie->GetExactDist2d(bot) < SPLASH_GUARD || (target && zombie->GetExactDist2d(target) < SPLASH_GUARD))
            return true;
    }

    return false;
}

void TickVdEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    if (vdRtiChecked.For(bot->GetInstanceId()).insert(bot->GetGUID()).second)
    {
        Value<std::string>* rti = botAI->GetAiObjectContext()->GetValue<std::string>("rti");
        if (rti->Get() == "cross")
            rti->Set("skull");
    }

    if (!InProgress(bot))
    {
        if (vdStates.Find(bot->GetInstanceId()))
            vdStates.Reset(bot->GetInstanceId());
        return;
    }

    Creature* valithria = GetVdValithria(botAI);
    if (!valithria)
        return;

    VdState& st = vdStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (!st.scanned || getMSTimeDiff(st.scanMs, now) >= SCAN_MS)
    {
        st.scanned = true;
        st.scanMs = now;
        Scan(st, valithria, now);
        std::vector<Player*> const members = MembersOnMap(bot);
        AssignSnares(st, bot, members);
        UpdateKites(st, valithria, members);
        UpdateDream(st, bot, members, now);
        AssignKicks(st, bot, members);
        AssignFreedom(st, members);
        CallHumanTank(st, bot, members, now);

        if (!st.dutiesRan || getMSTimeDiff(st.dutyMs, now) >= DUTY_MS)
        {
            st.dutiesRan = true;
            st.dutyMs = now;
            AssignHealers(st, bot, members);
            AssignAbominations(st, bot, members);
            AssignPortals(st, bot, members);
        }
    }

    UpdateStand(st, bot);
    NoteWave(st, bot);
}

uint8 IccValithriaResistSchools(PlayerbotAI* /*botAI*/, uint8 /*previous*/)
{
    return RAID_RESIST_FIRE | RAID_RESIST_FROST | RAID_RESIST_NATURE;
}
