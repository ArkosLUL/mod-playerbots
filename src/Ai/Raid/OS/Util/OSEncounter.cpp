/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSHelpers.h"
#include "Creature.h"
#include "Group.h"
#include "GroupReference.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "EncounterHelpers.h"
#include "RaidObs.h"
#include "Unit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <unordered_set>

using namespace EncounterHelpers;

namespace OsHelpers
{

namespace
{

constexpr uint32 STALE_STATE_MS = 15000;

struct EncounterState
{
    ObjectGuid bossGuid;
    uint32 fightStartMs = 0;
    uint32 lastSeenMs = 0;
    RaidObs::ObsValue<bool> mainTankDragged{"sartharion.dragdone"};
    uint32 mainTankDragArrivedMs = 0;
    uint32 mainTankDragStartedMs = 0;
    RaidObs::ObsValue<bool> burstWindowOpen{"sartharion.burstwindow"};
    RaidObs::ObsValue<bool> tankCooldownWindowOpen{"sartharion.tankcdwindow"};
    bool assignmentsResolved = false;
    RaidObs::ObsGuidSet portalSquad{"sartharion.portalsquad"};
    bool offTankWarned = false;
    std::unordered_set<ObjectGuid> tsunamiTraced;
};

RaidInstanceState<EncounterState> encounterStates;

EncounterState& StateFor(Unit* boss)
{
    EncounterState& state = encounterStates.For(boss->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.bossGuid != boss->GetGUID() || !boss->IsInCombat() ||
        (state.lastSeenMs && getMSTimeDiff(state.lastSeenMs, now) > STALE_STATE_MS))
    {
        state = EncounterState();
        state.bossGuid = boss->GetGUID();
        state.fightStartMs = now;
    }
    state.lastSeenMs = now;
    return state;
}

int32 HunterIndex(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return -1;

    int32 index = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->getClass() != CLASS_HUNTER)
            continue;

        if (member == bot)
            return index;

        ++index;
    }
    return -1;
}

}

Unit* GetSartharion(Player* bot)
{
    if (!bot || bot->GetMapId() != OS_MAP_ID)
        return nullptr;

    return FindUnitByEntries(bot, { NpcId::Sartharion, NpcId::SartharionH }, ROOM_SEARCH_RADIUS);
}

bool SartharionEncounterActive(Player* bot)
{
    Unit* boss = GetSartharion(bot);
    if (!boss || !boss->IsInCombat())
        return false;

    StateFor(boss);
    return true;
}

SartharionSnapshot const& SartharionSnapshotFor(PlayerbotAI* botAI)
{
    thread_local PlayerbotAI* cachedFor = nullptr;
    thread_local uint32 cachedAtMs = 0;
    thread_local SartharionSnapshot cached;

    uint32 const now = getMSTime();
    if (cachedFor == botAI && cachedAtMs == now && cachedAtMs)
        return cached;

    cachedFor = botAI;
    cachedAtMs = now;
    cached = SartharionSnapshot();

    Player* bot = botAI->GetBot();
    cached.encounterActive = SartharionEncounterActive(bot);
    if (!cached.encounterActive)
        return cached;

    cached.boss = GetSartharion(bot);
    cached.dodgeLive = ClassifyTsunamiWave(bot) != TsunamiWave::None ||
                       FindUnitByEntries(bot, { NpcId::TwilightFissure, NpcId::TwilightFissureH },
                                         FISSURE_CLEAR_RADIUS, false) != nullptr;
    return cached;
}

void TickSartharionObs(Player* bot)
{
    if (!RaidObs::Active() || !IsMechanicTrackerBot(bot, OS_MAP_ID))
        return;

    Unit* boss = GetSartharion(bot);
    if (boss && boss->IsInCombat())
        NoteTsunamiHazards(bot, StateFor(boss).tsunamiTraced);
}

bool SartharionDamageImmune(Player* bot)
{
    Unit* boss = GetSartharion(bot);
    return boss && boss->HasAura(SpellId::GiftOfTwilightFire);
}

uint32 EncounterElapsedMs(Player* bot)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return 0;

    return getMSTimeDiff(StateFor(boss).fightStartMs, getMSTime());
}

bool HasTwilightShift(Unit const* unit)
{
    return unit && (unit->HasAura(SpellId::TwilightShift) || unit->HasAura(SpellId::TwilightShiftAlt));
}

bool TwilightRealmWorthEntering(Player* bot)
{
    return bot && (SartharionDamageImmune(bot) ||
                   bot->HasAura(SpellId::TwilightTormentSartharion) ||
                   bot->HasAura(SpellId::TwilightTormentVesperon));
}

bool HasMoltenFury(Unit const* unit)
{
    return unit && unit->HasAura(SpellId::MoltenFury);
}

bool OnThePlatform(Player* bot)
{
    return bot->GetMapId() == OS_MAP_ID && !HasTwilightShift(bot);
}

bool NeedsPlatformReturn(PlayerbotAI* /*botAI*/, Player* bot)
{
    if (bot->GetMapId() != OS_MAP_ID || !SartharionEncounterActive(bot))
        return false;

    if (!InsideRoom(bot))
        return true;

    Unit* boss = GetSartharion(bot);
    if (PlayerbotAI::IsMainTank(bot) && boss && !MainTankDragDone(boss))
        return false;

    return OffThePlatform(bot);
}

bool NeedsTsunamiDodge(Player* bot)
{
    if (HasTwilightShift(bot))
    {
        if (!TwilightRealmWaveWait(bot) || WaveClearsY(bot->GetPositionY(), ClassifyTsunamiWave(bot)))
            return false;

        return std::abs(bot->GetPositionY() - SafeCorridorY(bot)) > CorridorToleranceFor(bot);
    }

    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    TsunamiWave const wave = ClassifyTsunamiWave(bot);
    if (wave == TsunamiWave::None)
        return false;

    if (WaveClearsY(bot->GetPositionY(), wave))
        return false;

    return std::abs(bot->GetPositionY() - SafeCorridorY(bot)) > CorridorToleranceFor(bot);
}

bool NeedsFissureDodge(Player* bot)
{
    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    return FindUnitByEntries(bot, { NpcId::TwilightFissure, NpcId::TwilightFissureH },
                             FISSURE_CLEAR_RADIUS, false) != nullptr;
}

Player* GetOffTank(PlayerbotAI* /*botAI*/, Player* bot)
{
    return GetGroupAssistTank(bot, 0);
}

bool IsOffTank(Player* bot)
{
    return PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
}

bool RequireOffTank(PlayerbotAI* botAI, Player* bot)
{
    if (GetOffTank(botAI, bot))
        return true;

    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    EncounterState& state = StateFor(boss);
    if (!state.offTankWarned)
    {
        state.offTankWarned = true;
        LOG_WARN("playerbots",
                 "Obsidian Sanctum: raid has no assist tank, every off-tank behaviour stays inactive");
    }
    return false;
}

Player* RedirectTarget(PlayerbotAI* botAI, Player* bot)
{
    Player* mainTank = GetGroupMainTank(bot);

    if (EncounterElapsedMs(bot) < PULL_WINDOW_MS)
        return mainTank;

    Player* offTank = GetOffTank(botAI, bot);
    if (!offTank)
        return mainTank;

    if (!LandedDrakes(bot).empty() ||
        FindUnitByEntries(bot, OFFTANK_PICKUP_ENTRIES, ROOM_SEARCH_RADIUS))
    {
        return offTank;
    }

    return mainTank;
}

Player* RedirectTankFor(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || bot->getClass() != CLASS_ROGUE)
        return RedirectTarget(botAI, bot);

    Unit* victim = bot->GetVictim();
    if (!victim)
        return nullptr;

    if (IsDrakeEntry(victim->GetEntry()))
        return GetOffTank(botAI, bot);

    Unit* boss = GetSartharion(bot);
    return boss && victim == boss ? GetGroupMainTank(bot) : nullptr;
}

Unit* OffTankTauntTarget(Player* bot)
{
    if (!bot)
        return nullptr;

    Unit* nearest = nullptr;
    float best = 0.0f;
    for (Unit* drake : LandedDrakes(bot))
    {
        if (drake->GetVictim() == bot)
            continue;

        float const distance = bot->GetExactDist2d(drake);
        if (distance > LAVA_BLAZE_TAUNT_RANGE)
            continue;

        if (!nearest || distance < best)
        {
            nearest = drake;
            best = distance;
        }
    }
    return nearest;
}

char const* TauntSpellFor(Player* bot)
{
    if (!bot)
        return nullptr;

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            return "taunt";
        case CLASS_DRUID:
            return "growl";
        case CLASS_PALADIN:
            return "hand of reckoning";
        case CLASS_DEATH_KNIGHT:
            return "dark command";
        default:
            return nullptr;
    }
}

namespace
{

void ResolveAssignments(EncounterState& state, Player* bot)
{
    if (state.assignmentsResolved)
        return;

    Group* group = bot->GetGroup();
    if (!group)
        return;

    std::vector<Player*> healers;
    std::vector<Player*> dps;
    Player* secondOffTank = nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != OS_MAP_ID)
            continue;

        if (PlayerbotAI::IsAssistTankOfIndex(member, 1, true))
        {
            secondOffTank = member;
            continue;
        }

        if (PlayerbotAI::IsTank(member))
            continue;

        if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else
            dps.push_back(member);
    }

    auto byGuid = [](Player const* lhs, Player const* rhs) { return lhs->GetGUID() < rhs->GetGUID(); };
    std::sort(healers.begin(), healers.end(), byGuid);
    std::sort(dps.begin(), dps.end(), byGuid);

    for (size_t i = 0; i < healers.size() && i < PORTAL_SQUAD_HEALERS; ++i)
        state.portalSquad.insert(healers[i]->GetGUID());

    for (Player const* member : dps)
        state.portalSquad.insert(member->GetGUID());

    if (secondOffTank)
        state.portalSquad.insert(secondOffTank->GetGUID());

    state.assignmentsResolved = true;
}

}

bool PortalSquadMember(Player* bot)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    EncounterState& state = StateFor(boss);
    ResolveAssignments(state, bot);

    return state.portalSquad.count(bot->GetGUID()) != 0;
}

bool TwilightAddsAlive(Player* bot)
{
    return FindUnitByEntries(bot, TWILIGHT_ADD_ENTRIES, ROOM_SEARCH_RADIUS) != nullptr;
}

bool MainTankDragDone(Unit* boss)
{
    return boss && StateFor(boss).mainTankDragged;
}

uint32 MainTankDragArrivedMs(Unit* boss)
{
    return boss ? StateFor(boss).mainTankDragArrivedMs : 0;
}

void SetMainTankDragArrivedMs(Unit* boss, uint32 arrivedMs)
{
    if (boss)
        StateFor(boss).mainTankDragArrivedMs = arrivedMs;
}

Unit* LandedShadron(Player* bot)
{
    return FindUnitByEntries(bot, { NpcId::Shadron, NpcId::ShadronH }, ROOM_SEARCH_RADIUS);
}

bool ShadronGone(Player* bot)
{
    if (!bot || LandedShadron(bot))
        return false;

    return EncounterElapsedMs(bot) > PULL_WINDOW_MS && !bot->HasAura(SpellId::PowerOfShadron);
}

bool SartharionBurstWindowOpen(Player* bot)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    EncounterState& state = StateFor(boss);
    if (state.burstWindowOpen)
        return true;

    if (LandedShadron(bot) || ShadronGone(bot))
        state.burstWindowOpen = true;

    return state.burstWindowOpen;
}

bool MainTankCooldownWindowOpen(Player* bot)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    EncounterState& state = StateFor(boss);
    if (!state.tankCooldownWindowOpen)
    {
        if (Unit* shadron = LandedShadron(bot))
            state.tankCooldownWindowOpen = shadron->GetHealthPct() <= MAIN_TANK_COOLDOWN_SHADRON_PCT;
        else if (ShadronGone(bot))
            state.tankCooldownWindowOpen = true;
    }

    return state.tankCooldownWindowOpen || bot->GetHealthPct() < MAIN_TANK_COOLDOWN_PANIC_PCT;
}

uint32 MainTankDragStartedMs(Unit* boss)
{
    return boss ? StateFor(boss).mainTankDragStartedMs : 0;
}

void SetMainTankDragStartedMs(Unit* boss, uint32 startedMs)
{
    if (boss)
        StateFor(boss).mainTankDragStartedMs = startedMs;
}

void SetMainTankDragged(Unit* boss)
{
    if (boss)
        StateFor(boss).mainTankDragged = true;
}

Unit* PriorityTarget(Player* bot)
{
    static std::vector<std::vector<uint32>> const priority = {
        { NpcId::AcolyteOfShadron, NpcId::AcolyteOfShadronH },
        { NpcId::AcolyteOfVesperon, NpcId::AcolyteOfVesperonH },
        { NpcId::DiscipleOfShadron, NpcId::DiscipleOfShadronH },
        { NpcId::DiscipleOfVesperon, NpcId::DiscipleOfVesperonH },
        { NpcId::Tenebron, NpcId::TenebronH },
        { NpcId::Shadron, NpcId::ShadronH },
        { NpcId::Vesperon, NpcId::VesperonH },
        { NpcId::LavaBlaze, NpcId::LavaBlazeH },
        { NpcId::Sartharion, NpcId::SartharionH },
    };

    Unit* target = nullptr;
    for (std::vector<uint32> const& tier : priority)
    {
        target = FindUnitByEntries(bot, tier, ROOM_SEARCH_RADIUS);
        if (target)
            break;
    }

    if (RaidObs::Active())
    {
        RaidObs::NoteDerived(bot, "sartharion.target",
                             target ? RaidObs::DescribeAssignment(target->GetGUID()) : "none");
    }

    return target;
}

Unit* TranquilizeTargetFor(Player* bot)
{
    int32 const index = HunterIndex(bot);
    if (index < 0)
        return nullptr;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, { NpcId::LavaBlaze, NpcId::LavaBlazeH },
                                        TRANQUILIZING_SHOT_RANGE);

    std::vector<Unit*> enraged;
    for (Creature* creature : found)
    {
        if (!creature || !creature->IsAlive() || creature->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ||
            !InsideRoom(creature))
        {
            continue;
        }

        if (HasMoltenFury(creature))
            enraged.push_back(creature);
    }

    if (enraged.empty())
        return nullptr;

    std::sort(enraged.begin(), enraged.end(),
              [](Unit const* lhs, Unit const* rhs) { return lhs->GetGUID() < rhs->GetGUID(); });

    return enraged[index % enraged.size()];
}

}
