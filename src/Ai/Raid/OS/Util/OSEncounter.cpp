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
#include "RaidBossHelpers.h"
#include "Unit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <mutex>
#include <unordered_map>

namespace OsHelpers
{

namespace
{

// Weakest first, which here is shortest cooldown first: the strongest button stays in hand longest.
// Bubbles are deliberately absent - a paladin under Divine Shield does no damage and holds nothing. So
// is short rotational mitigation (shield block, bone shield, spell reflection), which keeps running on
// its own class logic; only the real cooldowns are sequenced. Cast names are the strings the class
// contexts already register, so one row serves both the cast and the multiplier, and the aura is
// matched by id because PlayerbotAI::HasAura compares the DBC string exactly - "anti magic shell" is
// not "Anti-Magic Shell".
struct TankDefensive
{
    uint8 playerClass;
    char const* castName;
    uint32 auraId;
};

std::array<TankDefensive, 9> const TANK_DEFENSIVES = { {
    { CLASS_WARRIOR, "last stand", 12975 },                // 180s
    { CLASS_WARRIOR, "shield wall", 871 },                  // 300s
    { CLASS_PALADIN, "divine protection", 498 },            // 180s
    { CLASS_DRUID, "barkskin", 22812 },                     //  60s
    { CLASS_DRUID, "frenzied regeneration", 22842 },        // 180s
    { CLASS_DRUID, "survival instincts", 61336 },           // 180s
    { CLASS_DEATH_KNIGHT, "anti magic shell", 48707 },      //  45s
    { CLASS_DEATH_KNIGHT, "vampiric blood", 55233 },        //  60s
    { CLASS_DEATH_KNIGHT, "icebound fortitude", 48792 },    // 120s
} };

// The bots poll this several times a second during a pull, so a gap this long means combat stopped
// and the corridor and portal squad have to be re-derived.
constexpr uint32 STALE_STATE_MS = 15000;

struct EncounterState
{
    ObjectGuid bossGuid;
    uint32 fightStartMs = 0;
    uint32 lastSeenMs = 0;
    // One-way. Set when the pull drag has actually landed Sartharion, so it never runs twice.
    bool mainTankDragged = false;
    // When the tank reached the drag corner, 0 while he is off it.
    uint32 mainTankDragArrivedMs = 0;
    // When the drag itself began, so its timeout does not ride on the encounter clock.
    uint32 mainTankDragStartedMs = 0;
    // One-way. Set the first tick Shadron is seen on the ground.
    bool burstWindowOpen = false;
    // One-way. Set the first tick Shadron is seen at or below MAIN_TANK_COOLDOWN_SHADRON_PCT, or once
    // he is gone for good. The panic escape hatch is not latched and rides on top of this.
    bool tankCooldownWindowOpen = false;
    bool assignmentsResolved = false;
    std::vector<ObjectGuid> portalSquad;
    bool offTankWarned = false;
};

// One state per instance, shared by every bot and by every trigger/action/multiplier: they each hold
// separate helper instances and cannot agree through a member. The corridor in particular has to be
// raid-wide, or half the raid dodges to one gap and half to the other.
EncounterState& StateFor(Unit* boss)
{
    // Instances update on parallel map threads, so the container lookup needs guarding. The state
    // itself is only ever touched by the map thread that owns the instance, and unordered_map nodes
    // keep their address across rehashes.
    static std::mutex mutex;
    static std::unordered_map<uint32, EncounterState> states;

    std::lock_guard<std::mutex> guard(mutex);
    EncounterState& state = states[boss->GetInstanceId()];
    uint32 const now = getMSTime();
    // Rebuilt every tick he is out of combat, so fightStartMs lands within a tick of the pull and every
    // latch below re-arms on a wipe. The staleness check cannot do that on its own: PortalSquadMember
    // resolves through here with no encounter gate, and the boss is already inside the 200yd search
    // from the instance entrance, so lastSeenMs is refreshed from the moment the raid zones in.
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

bool NeedsPlatformReturn(PlayerbotAI* botAI, Player* bot)
{
    // Not gated on OnThePlatform: that excludes shifted bots, and the Twilight Realm sits on the same
    // coordinates. A shifted bot cannot resolve Sartharion anyway, so the encounter gate covers it.
    // Bounded by the 200yd Sartharion search, so this catches a bot on its first step off the arena
    // rather than one that is already halfway across the zone.
    if (bot->GetMapId() != OS_MAP_ID || !SartharionEncounterActive(bot))
        return false;

    if (!InsideRoom(bot))
        return true;

    // The pull drag corner is a hand-measured position 1.74yd south of PLATFORM_MIN_Y, so the one bot
    // meant to stand off the box is exempt until the drag latches.
    Unit* boss = GetSartharion(bot);
    if (botAI->IsMainTank(bot) && boss && !MainTankDragDone(boss))
        return false;

    // The room box is 18yd wider than the platform on X, so without this a bot standing in the lava
    // off the east rim reads as in the fight and nothing ever walks it back.
    return OffThePlatform(bot);
}

bool NeedsTsunamiDodge(Player* bot)
{
    // Not a dodge: nothing in phase 16 can be touched by a wave. This is where the bot will be standing
    // when the shift is stripped, which the raid's shared portal refcount only allows once the last
    // acolyte is dead, so it lands on a tick nobody chooses.
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

    // A bot already standing where this pattern cannot reach stays put. The corridor holds are the
    // fallback, not the only safe ground: the off-tank's drake spots and melee behind a drake clear
    // some of the lines outright, and walking them 28yd to a lane that is no safer costs the trip
    // twice - and the walk back is what their own hold spends the next tick undoing.
    if (WaveClearsY(bot->GetPositionY(), wave))
        return false;

    return std::abs(bot->GetPositionY() - SafeCorridorY(bot)) > CorridorToleranceFor(bot);
}

bool NeedsFissureDodge(Player* bot)
{
    if (!OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    // requireSelectable off. The fissure is UNIT_FLAG_NOT_SELECTABLE for its whole life, so the
    // default search skipped it and this trigger had never fired.
    return FindUnitByEntries(bot, { NpcId::TwilightFissure, NpcId::TwilightFissureH },
                             FISSURE_CLEAR_RADIUS, false) != nullptr;
}

Player* GetOffTank(PlayerbotAI* botAI, Player* bot)
{
    return GetGroupAssistTank(botAI, bot, 0);
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
    Player* mainTank = GetGroupMainTank(botAI, bot);

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
    return boss && victim == boss ? GetGroupMainTank(botAI, bot) : nullptr;
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

// Resolved once per pull and never reshuffled, so a healer who dies is not silently replaced
// mid-fight by whoever sorts next.
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

        // Tested ahead of the tank skip below, because he is one.
        if (PlayerbotAI::IsAssistTankOfIndex(member, 1, true))
        {
            secondOffTank = member;
            continue;
        }

        // The main tank keeps Sartharion and the first off-tank keeps the drakes, so neither goes.
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
        state.portalSquad.push_back(healers[i]->GetGUID());

    // Every one of them, melee included. The acolyte is what the trip is for and it dies to whatever
    // the raid brings; leaving half the damage on the platform only lengthens the immunity.
    for (Player const* member : dps)
        state.portalSquad.push_back(member->GetGUID());

    if (secondOffTank)
        state.portalSquad.push_back(secondOffTank->GetGUID());

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

    return std::find(state.portalSquad.begin(), state.portalSquad.end(), bot->GetGUID()) !=
           state.portalSquad.end();
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
    // Selectable-only, which is the whole test: the flag comes off in MovementInform(POINT_LANDING).
    return FindUnitByEntries(bot, { NpcId::Shadron, NpcId::ShadronH }, ROOM_SEARCH_RADIUS);
}

bool ShadronGone(Player* bot)
{
    if (!bot || LandedShadron(bot))
        return false;

    // The elapsed guard covers the tick or two between Sartharion engaging and the drake's own cast
    // landing, where the aura is not up yet and nothing is on the ground either.
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

char const* NextTankDefensive(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return nullptr;

    char const* weakest = nullptr;
    for (TankDefensive const& entry : TANK_DEFENSIVES)
    {
        if (entry.playerClass != bot->getClass())
            continue;

        // One at a time: anything still running means the tank is already covered, and stacking the
        // next one on top spends two buttons on one window.
        if (bot->HasAura(entry.auraId))
            return nullptr;

        if (!weakest && botAI->CanCastSpell(entry.castName, bot))
            weakest = entry.castName;
    }
    return weakest;
}

bool IsHeldTankDefensive(std::string const& actionName)
{
    for (TankDefensive const& entry : TANK_DEFENSIVES)
    {
        if (actionName == entry.castName)
            return true;
    }
    return false;
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
    // One flat order serves both phases: the realm adds are phase 16 and the drakes phase 1, so a
    // bot only ever sees the half of this list it can actually reach.
    //
    // The Acolyte of Shadron tops it unconditionally - it grants Gift of Twilight, which makes
    // Sartharion immune to every school, so nothing else the raid hits counts while it lives. Twilight
    // Eggs are not on it at all: they carry their own 25s fuse and hatch into phase-1 whelps the
    // off-tank already picks up, so hitting one trades a raid debuff for nothing.
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

    for (std::vector<uint32> const& tier : priority)
    {
        if (Unit* target = FindUnitByEntries(bot, tier, ROOM_SEARCH_RADIUS))
            return target;
    }
    return nullptr;
}

Unit* TranquilizeTargetFor(Player* bot)
{
    int32 const index = HunterIndex(bot);
    if (index < 0)
        return nullptr;

    // Lava Blazes and nothing else can carry it. Molten Fury (60430) picks its targets through
    // TARGET_UNIT_SRC_AREA_ENTRY, and its conditions row pins the entry to 30643, so a wave sweeping
    // over Sartharion or a drake cannot enrage them however much it looks like it should.
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

    // Stable order so every hunter builds the same list and the index below hands out distinct
    // targets instead of stacking three dispels on one blaze.
    std::sort(enraged.begin(), enraged.end(),
              [](Unit const* lhs, Unit const* rhs) { return lhs->GetGUID() < rhs->GetGUID(); });

    return enraged[index % enraged.size()];
}

}
