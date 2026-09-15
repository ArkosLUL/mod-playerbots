/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_YoggSaron.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "AiObjectContext.h"
#include "Creature.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "UldEncounterGate.h"
#include "Unit.h"

const std::vector<uint32> ULDUAR_YOGG_SARON_ILLUSION_MOBS = {
    NPC_INFLUENCE_TENTACLE, NPC_SUIT_OF_ARMOR,   NPC_DEATHSWORN_ZEALOT,  NPC_RUBY_CONSORT,
    NPC_AZURE_CONSORT,      NPC_BRONZE_CONSORT,  NPC_EMERALD_CONSORT,    NPC_OBSIDIAN_CONSORT};

// Which disguise a tentacle wears is decided by where it spawned, not by which illusion is running:
// x in (2000, 2150) is a Consort, otherwise y in (-150, -90) is a Zealot, otherwise a Suit of Armor.
const Position ULDUAR_YOGG_SARON_PORTAL_SPOTS[ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN] = {
    Position(1964.60f, -42.71f, 325.08f), Position(1986.94f, -46.21f, 324.98f),
    Position(1989.50f, -6.70f, 325.08f),  Position(1965.52f, -8.09f, 324.95f),
    Position(2000.84f, -25.40f, 325.19f), Position(1960.22f, -26.14f, 325.01f),
    Position(1976.30f, -47.83f, 325.11f), Position(1997.69f, -37.46f, 325.04f),
    Position(1998.07f, -13.36f, 325.17f), Position(1976.99f, -3.96f, 325.17f)};

const Position ULDUAR_YOGG_SARON_MIDDLE = Position(1980.28f, -25.5868f, 329.397f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE = Position(1927.1511f, 68.507256f, 242.37657f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE = Position(1925.6553f, -121.59296f, 239.98965f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE = Position(2104.5667f, -25.509348f, 242.64679f);
const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE = Position(1980.1971f, -27.854689f, 236.06789f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE = Position(1954.06f, 21.66f, 239.71f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE = Position(1950.11f, -79.284f, 239.98982f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE = Position(2048.63f, -25.5f, 239.72f);
const Position ULDUAR_YOGG_SARON_P1_RANGED_SPOT = Position(1958.78f, -25.587f, 324.889f);
const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT = Position(1998.5377f, -22.90317f, 324.8895f);
const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT = Position(2018.7628f, -18.896868f, 327.07245f);

namespace
{
// Raid-wide answers folded once per instance, and the per-bot exposure probes paced off the same
// state. Every number the last two attempts were read from came out of a throwaway script over raw
// snapshot rows, which is the definition of a missing probe.
//
// Not thread_local: a map is updated by one thread at a time but is never pinned to one, so
// per-thread copies would hand the same instance a fresh latch whenever the pool reassigns it.
// References into an unordered_map survive rehashing, so the lock only has to cover the lookup.
struct YoggSaronEncounterState
{
    RaidObs::ObsValue<uint32> phase{"yogg.phase"};
    RaidObs::ObsValue<uint32> wave{"yogg.wave"};
    RaidObs::ObsGuidSet brainTeam{"yogg.brainteam"};
    RaidObs::ObsGuidMap<uint8> portalSlot{"yogg.portalslot"};

    // Phase 2's clock. nextWaveMs is a prediction: it starts at +60 s and re-anchors on every wave a
    // bot actually sees, so a delayed wave corrects itself rather than compounding.
    uint32 phase2StartMs = 0;
    uint32 lastWaveMs = 0;
    uint32 nextWaveMs = 0;
    uint32 waveOrdinal = 0;
    uint32 slotWave = 0;

    // Nothing in the world counts the transformation dialogue down, so the window is timed off the
    // tick Yogg was first seen without his barrier.
    uint32 handoverStartMs = 0;

    uint32 hazardNoteMs = 0;
    std::unordered_map<ObjectGuid, uint32> obsScanMs;
};

std::mutex yoggSaronStatesMutex;
std::unordered_map<uint32 /*instanceId*/, YoggSaronEncounterState> yoggSaronStates;

YoggSaronEncounterState& YoggSaronStateFor(Player* bot)
{
    std::lock_guard<std::mutex> guard(yoggSaronStatesMutex);
    return yoggSaronStates[bot->GetInstanceId()];
}

void TickYoggSaronObs(PlayerbotAI* botAI, uint32 phase);
}  // namespace

uint32 YoggSaronPhase(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    // Ahead of the sweeps, so a bot anywhere else in Ulduar pays a map lookup rather than two 200 yd
    // grid searches a tick. Sara's own combat flag used to sit here and could not do the job: she is
    // FACTION_FRIENDLY through phase 1, CombatManager::CanBeginCombat refuses a combat reference while
    // either side is friendly, and InitFight's SetInCombatWithZone therefore never touches her. The
    // boss state is IN_PROGRESS from inside InitFight itself.
    if (!UldEncounterIsLive(botAI, ULD_BOSS_YOGGSARON))
    {
        TickYoggSaronObs(botAI, 0);
        return 0;
    }

    Creature* sara = bot->FindNearestCreature(NPC_SARA_PHASE_1, 200.0f, true);
    Creature* yogg = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);

    uint32 phase = 0;
    if (yogg && yogg->IsAlive() && yogg->HasAura(SPELL_SHADOW_BARRIER))
        phase = 2;
    // The Brain, not the absence of a phase-1 Guardian. A test made only of absences is true between
    // the phases it separates: the last Guardian dies ~9.5 s before the Shadow Barrier lands, and the
    // whole raid used to run its phase 3 positioning through that gap. The Brain spawns with the first
    // tentacle wave and lives to the end, so it is the positive fact.
    else if (yogg && yogg->IsAlive() && bot->FindNearestCreature(NPC_BRAIN, 200.0f, true))
        phase = 3;
    else if (sara)
        phase = 1;

    TickYoggSaronObs(botAI, phase);

    return phase;
}

bool YoggSaronEncounterActive(PlayerbotAI* botAI) { return YoggSaronPhase(botAI) != 0; }
bool YoggSaronInPhase1(PlayerbotAI* botAI) { return YoggSaronPhase(botAI) == 1; }
bool YoggSaronInPhase2(PlayerbotAI* botAI) { return YoggSaronPhase(botAI) == 2; }
bool YoggSaronInPhase3(PlayerbotAI* botAI) { return YoggSaronPhase(botAI) == 3; }

bool YoggSaronOnBrainLevel(Player* player)
{
    return player && player->GetPositionZ() > ULDUAR_YOGG_SARON_BRAIN_LEVEL_MIN_Z &&
           player->GetPositionZ() < ULDUAR_YOGG_SARON_BRAIN_LEVEL_MAX_Z;
}

YoggSaronRoom YoggSaronRoomOf(Player* player)
{
    if (!player)
        return YOGG_SARON_ROOM_NONE;

    if (player->GetPositionZ() >= ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return YOGG_SARON_ROOM_ARENA;

    if (!YoggSaronOnBrainLevel(player))
        return YOGG_SARON_ROOM_NONE;

    // Brain room first. It is 108-124 yd from all three illusion middles, so any radius wide enough
    // to overlap them makes whichever room is tested first swallow it.
    if (player->GetDistance2d(ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionY()) <
        ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS)
        return YOGG_SARON_ROOM_BRAIN;

    if (player->GetDistance2d(ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionY()) <
        ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS)
        return YOGG_SARON_ROOM_STORMWIND;

    if (player->GetDistance2d(ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionY()) <
        ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS)
        return YOGG_SARON_ROOM_ICECROWN;

    if (player->GetDistance2d(ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionY()) <
        ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS)
        return YOGG_SARON_ROOM_CHAMBER;

    return YOGG_SARON_ROOM_NONE;
}

bool YoggSaronRoomMiddle(Player* player, Position& middle)
{
    switch (YoggSaronRoomOf(player))
    {
        case YOGG_SARON_ROOM_STORMWIND:
            middle = ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE;
            return true;
        case YOGG_SARON_ROOM_ICECROWN:
            middle = ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE;
            return true;
        case YOGG_SARON_ROOM_CHAMBER:
            middle = ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE;
            return true;
        default:
            return false;
    }
}

bool YoggSaronBrainRoomApproachable(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    if (!YoggSaronInfluenceTentaclesCleared(botAI))
        return false;

    uint32 doorEntry = 0;
    switch (YoggSaronRoomOf(bot))
    {
        // Already through it.
        case YOGG_SARON_ROOM_BRAIN:
            return true;
        case YOGG_SARON_ROOM_STORMWIND:
            doorEntry = GO_STORMWIND_ILLUSION_DOORS;
            break;
        case YOGG_SARON_ROOM_ICECROWN:
            doorEntry = GO_ICECROWN_ILLUSION_DOORS;
            break;
        case YOGG_SARON_ROOM_CHAMBER:
            doorEntry = GO_CHAMBER_ILLUSION_DOORS;
            break;
        default:
            return false;
    }

    GameObject* door = bot->FindNearestGameObject(doorEntry, 200.0f);

    return door && door->GetGoState() == GO_STATE_ACTIVE;
}

YoggSaronRoomState YoggSaronRoomStateOf(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    YoggSaronRoomState state = YOGG_SARON_ROOM_STATE_NONE;
    switch (YoggSaronRoomOf(bot))
    {
        case YOGG_SARON_ROOM_BRAIN:
            state = YOGG_SARON_ROOM_STATE_AT_BRAIN;
            break;
        case YOGG_SARON_ROOM_STORMWIND:
        case YOGG_SARON_ROOM_ICECROWN:
        case YOGG_SARON_ROOM_CHAMBER:
        {
            if (YoggSaronInfluenceTentaclesCleared(botAI))
            {
                state = YoggSaronBrainRoomApproachable(botAI) ? YOGG_SARON_ROOM_STATE_TO_BRAIN
                                                              : YOGG_SARON_ROOM_STATE_DOOR_SHUT;
                break;
            }

            // Line of sight is the whole difference. Two of the three rooms put their tentacles
            // behind a doorway, the dps resolver will not pick what it cannot see, and nothing else
            // in the fight moves a bot once the portal has dropped it.
            Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
            state = target && target->IsAlive() && bot->IsWithinLOSInMap(target)
                        ? YOGG_SARON_ROOM_STATE_FIGHTING
                        : YOGG_SARON_ROOM_STATE_WALKING_IN;
            break;
        }
        default:
            break;
    }

    if (RaidObs::Active())
    {
        char const* names[] = {"none", "walkingin", "fighting", "doorshut", "tobrain", "atbrain"};
        RaidObs::NoteDerived(bot, "yogg.roomstate", names[state]);
    }

    return state;
}

bool YoggSaronShouldLeaveBrainLevel(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Creature const* brain = bot->FindNearestCreature(NPC_BRAIN, 200.0f, true);
    if (!brain || !brain->IsAlive())
        return false;

    char const* branch = "safe";
    bool leave = false;

    if (brain->HasUnitState(UNIT_STATE_CASTING))
    {
        Spell* induceMadness = brain->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (induceMadness && induceMadness->m_spellInfo->Id == SPELL_INDUCE_MADNESS)
        {
            uint32 const castingTimeLeft = induceMadness->GetCastTimeRemaining();

            // Every millisecond of lead is damage the Brain does not take, so it is measured against
            // the walk the bot actually faces rather than set flat for the worst case.
            uint32 lead = ULDUAR_YOGG_SARON_EXIT_LEAD_FLOOR_MS;
            GameObject* portal = bot->FindNearestGameObject(GO_FLEE_TO_THE_SURFACE_PORTAL, 200.0f);
            float const speed = bot->GetSpeed(MOVE_RUN);
            if (portal && speed > 0.0f)
            {
                lead = std::max(lead, static_cast<uint32>(bot->GetDistance2d(portal) / speed *
                                                          ULDUAR_YOGG_SARON_EXIT_LEAD_SAFETY * 1000.0f));
            }

            if (castingTimeLeft < lead)
            {
                leave = true;
                branch = castingTimeLeft < ULDUAR_YOGG_SARON_EXIT_LEAD_FLOOR_MS ? "late" : "leaving";
            }
        }
    }
    else if (brain->GetHealth() < brain->GetMaxHealth() * 0.3f)
    {
        leave = true;
        branch = "leaving";
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.exit", branch);

    return leave;
}

std::vector<Unit*> GetYoggSaronSkullsInArc(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> skulls;
    bot->GetCreatureListWithEntryInGrid(skulls, NPC_LAUGHING_SKULL, ULDUAR_YOGG_SARON_LAUGHING_SKULL_RADIUS);

    std::vector<Unit*> inArc;
    for (Creature* skull : skulls)
    {
        if (!skull->IsAlive())
            continue;

        // The exact filter the spell uses, so what the node answers for and what the raid is hit by
        // are the same set.
        if (bot->HasInArc(static_cast<float>(M_PI), skull))
            inArc.push_back(skull);
    }

    return inArc;
}

YoggSaronPortalWave YoggSaronPortalWaveState(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    if (YoggSaronPhase(botAI) != 2)
    {
        std::lock_guard<std::mutex> guard(yoggSaronStatesMutex);
        state.phase2StartMs = 0;
        state.lastWaveMs = 0;
        state.nextWaveMs = 0;
        return YoggSaronPortalWave();
    }

    bool const portalsUp =
        bot->FindNearestCreature(NPC_DESCEND_INTO_MADNESS, ULDUAR_YOGG_SARON_PORTAL_SEARCH_RADIUS, true) != nullptr;

    std::lock_guard<std::mutex> guard(yoggSaronStatesMutex);

    if (!state.phase2StartMs)
    {
        state.phase2StartMs = now;
        state.nextWaveMs = now + ULDUAR_YOGG_SARON_PORTAL_FIRST_WAVE_MS;
    }

    // Only a sighting moves the clock. A bot underground sees no portal, and reading that silence as
    // "the wave ended" would restart the count every time the brain team went down.
    if (portalsUp &&
        (!state.lastWaveMs || getMSTimeDiff(state.lastWaveMs, now) > ULDUAR_YOGG_SARON_PORTAL_WAVE_DEBOUNCE_MS))
    {
        state.lastWaveMs = now;
        state.nextWaveMs = now + ULDUAR_YOGG_SARON_PORTAL_WAVE_PERIOD_MS;
        ++state.waveOrdinal;
        state.wave = state.waveOrdinal;
    }
    // A prediction nobody confirmed rolls on by a whole period rather than reading "any moment now"
    // forever, which would keep the brain team parked on its spots for the rest of the fight.
    else if (!portalsUp && state.nextWaveMs &&
             getMSTimeDiff(state.nextWaveMs, now) > ULDUAR_YOGG_SARON_PORTAL_WAVE_DEBOUNCE_MS)
    {
        state.nextWaveMs += ULDUAR_YOGG_SARON_PORTAL_WAVE_PERIOD_MS;
    }

    YoggSaronPortalWave answer;
    answer.active = true;
    answer.ordinal = state.waveOrdinal;
    answer.portalsUp = portalsUp;

    int32 const remaining = static_cast<int32>(state.nextWaveMs - now);
    answer.msToNextWave = remaining > 0 ? static_cast<uint32>(remaining) : 0;

    return answer;
}

std::vector<Player*> GetYoggSaronBrainTeam(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Group* group = bot->GetGroup();
    if (!group)
        return {};

    uint32 size = bot->GetRaidDifficulty() == Difficulty::RAID_DIFFICULTY_10MAN_NORMAL
                      ? ULDUAR_YOGG_SARON_PORTAL_SPOTS_10MAN
                      : ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN;

    // A non-tank master takes a portal of its own, and it is not ours to move.
    Player* master = botAI->GetMaster();
    bool const masterTakesOne = master && !PlayerbotAI::IsTank(master);
    if (masterTakesOne && size)
        --size;

    std::vector<Player*> melee, healers, ranged;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || PlayerbotAI::IsTank(member))
            continue;

        if (masterTakesOne && member == master)
            continue;

        if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsMelee(member))
            melee.push_back(member);
        else
            ranged.push_back(member);
    }

    // By GUID inside each band, because every bot derives this list for itself and group order is not
    // the same walk from every seat.
    auto const byGuid = [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); };
    std::sort(melee.begin(), melee.end(), byGuid);
    std::sort(healers.begin(), healers.end(), byGuid);
    std::sort(ranged.begin(), ranged.end(), byGuid);

    std::vector<Player*> team = melee;
    if (!healers.empty())
        team.push_back(healers.front());
    team.insert(team.end(), ranged.begin(), ranged.end());
    team.insert(team.end(), healers.begin() + (healers.empty() ? 0 : 1), healers.end());

    if (team.size() > size)
        team.resize(size);

    return team;
}

YoggSaronPortalIntent YoggSaronPortalPlan(PlayerbotAI* botAI, Position& spot)
{
    Player* bot = botAI->GetBot();

    YoggSaronPortalWave const wave = YoggSaronPortalWaveState(botAI);
    if (!wave.active)
        return YOGG_SARON_PORTAL_NOT_TEAM;

    std::vector<Player*> const team = GetYoggSaronBrainTeam(botAI);

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    uint32 const spotCount = bot->GetRaidDifficulty() == Difficulty::RAID_DIFFICULTY_10MAN_NORMAL
                                 ? ULDUAR_YOGG_SARON_PORTAL_SPOTS_10MAN
                                 : ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN;

    // The wave the spread is for: the one that is up, or the next one if none is.
    uint32 const assignmentWave = wave.ordinal + (wave.portalsUp ? 0 : 1);

    uint8 slot = 0;
    bool assigned = false;
    {
        std::lock_guard<std::mutex> guard(yoggSaronStatesMutex);

        // Latched per wave. Nearest-first off live positions churns every tick as bots walk, and two
        // bots swapping spots mid-approach costs both of them the window.
        if (state.slotWave != assignmentWave)
        {
            state.slotWave = assignmentWave;
            state.portalSlot.clear();
            state.brainTeam.clear();

            std::vector<bool> taken(spotCount, false);
            for (Player* member : team)
            {
                state.brainTeam.insert(member->GetGUID());

                uint32 best = spotCount;
                float bestDistance = 0.0f;
                for (uint32 candidate = 0; candidate < spotCount; ++candidate)
                {
                    if (taken[candidate])
                        continue;

                    float const distance = member->GetExactDist2d(ULDUAR_YOGG_SARON_PORTAL_SPOTS[candidate]);
                    if (best == spotCount || distance < bestDistance)
                    {
                        best = candidate;
                        bestDistance = distance;
                    }
                }

                if (best == spotCount)
                    break;

                taken[best] = true;
                state.portalSlot[member->GetGUID()] = static_cast<uint8>(best);
            }
        }

        auto const mine = state.portalSlot.find(bot->GetGUID());
        if (mine != state.portalSlot.end())
        {
            slot = mine->second;
            assigned = true;
        }
    }

    YoggSaronPortalIntent intent = YOGG_SARON_PORTAL_NOT_TEAM;
    if (assigned)
    {
        spot = ULDUAR_YOGG_SARON_PORTAL_SPOTS[slot];

        float const distance = bot->GetExactDist2d(spot);
        if (distance <= ULDUAR_YOGG_SARON_PORTAL_ARRIVED_RADIUS)
            intent = YOGG_SARON_PORTAL_HOLDING;
        else if (wave.portalsUp)
            intent = YOGG_SARON_PORTAL_LATE;
        else
        {
            float const speed = bot->GetSpeed(MOVE_RUN);
            uint32 lead = ULDUAR_YOGG_SARON_PORTAL_SPREAD_LEAD_FLOOR_MS;
            if (speed > 0.0f)
            {
                lead = std::max(lead, static_cast<uint32>(distance / speed *
                                                          ULDUAR_YOGG_SARON_PORTAL_SPREAD_LEAD_SAFETY * 1000.0f));
            }

            intent = wave.msToNextWave > lead ? YOGG_SARON_PORTAL_WAITING : YOGG_SARON_PORTAL_SPREADING;
        }
    }

    if (RaidObs::Active())
    {
        char const* names[] = {"notteam", "waiting", "spreading", "holding", "late"};
        RaidObs::NoteDerived(bot, "yogg.portal", names[intent]);
    }

    return intent;
}

YoggSaronHandover YoggSaronHandoverState(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Creature* yogg = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);

    // Yogg without the barrier is phase 3 as well, where ACTION_YOGG_SARON_START_P3 strips it again.
    // The Brain separates them: it is summoned in the same tick the barrier first lands, so it is up
    // for everything after this window and absent for the whole of it.
    bool const open = yogg && yogg->IsAlive() && !yogg->HasAura(SPELL_SHADOW_BARRIER) &&
                      !bot->FindNearestCreature(NPC_BRAIN, 200.0f, true);

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    YoggSaronHandover answer;
    {
        std::lock_guard<std::mutex> guard(yoggSaronStatesMutex);

        if (!open)
        {
            state.handoverStartMs = 0;
            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "yogg.handover", "clear");

            return answer;
        }

        uint32 const now = getMSTime();
        if (!state.handoverStartMs)
            state.handoverStartMs = now;

        uint32 const elapsed = getMSTimeDiff(state.handoverStartMs, now);
        answer.active = true;
        answer.msToRing =
            elapsed >= ULDUAR_YOGG_SARON_HANDOVER_MS ? 0 : ULDUAR_YOGG_SARON_HANDOVER_MS - elapsed;
    }

    // Only melee and the tank are ever inside the ring when it lights up; the back line is already
    // parked at 21.5 yd, which is 8.2 yd clear of it.
    float const fromMiddle =
        bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());
    float const walk = ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS - fromMiddle;

    if (walk > 0.0f)
    {
        uint32 lead = ULDUAR_YOGG_SARON_HANDOVER_LEAD_FLOOR_MS;
        float const speed = bot->GetSpeed(MOVE_RUN);
        if (speed > 0.0f)
        {
            lead = std::max(
                lead, static_cast<uint32>(walk / speed * ULDUAR_YOGG_SARON_HANDOVER_LEAD_SAFETY * 1000.0f));
        }

        answer.clearing = answer.msToRing <= lead;
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.handover", answer.clearing ? "clearing" : "holding");

    return answer;
}

bool YoggSaronInBodyKnockback(Player* player)
{
    return player && player->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(),
                                           ULDUAR_YOGG_SARON_MIDDLE.GetPositionY()) <
                         ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS;
}

namespace
{
// Closest approach of the walk from (fromX, fromY) to (toX, toY) to a point, clamped to the segment.
float ClosestApproach(float fromX, float fromY, float toX, float toY, Position const& to)
{
    float const legX = toX - fromX;
    float const legY = toY - fromY;
    float const legSquared = legX * legX + legY * legY;

    float along = 0.0f;
    if (legSquared > 0.0f)
    {
        along = ((to.GetPositionX() - fromX) * legX + (to.GetPositionY() - fromY) * legY) / legSquared;
        along = std::max(0.0f, std::min(1.0f, along));
    }

    return to.GetExactDist2d(fromX + legX * along, fromY + legY * along);
}
}  // namespace

bool YoggSaronRouteClearOfBody(Player* bot, float x, float y)
{
    // No opinion from inside it. Every short step out of the ring passes close to the middle by
    // definition, so a filter that judged those would reject the only walks that end the problem.
    if (YoggSaronInBodyKnockback(bot))
        return true;

    return ClosestApproach(bot->GetPositionX(), bot->GetPositionY(), x, y, ULDUAR_YOGG_SARON_MIDDLE) >=
           ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS;
}

bool YoggSaronBodyDetour(Player* bot, Position const& destination, Position& waypoint)
{
    if (YoggSaronRouteClearOfBody(bot, destination.GetPositionX(), destination.GetPositionY()))
        return false;

    float const middleX = ULDUAR_YOGG_SARON_MIDDLE.GetPositionX();
    float const middleY = ULDUAR_YOGG_SARON_MIDDLE.GetPositionY();

    float const from = std::atan2(bot->GetPositionY() - middleY, bot->GetPositionX() - middleX);
    float const to = std::atan2(destination.GetPositionY() - middleY, destination.GetPositionX() - middleX);

    // Halfway round the shorter arc. Walking it leaves the bot facing at most a quarter turn to the
    // destination, whose own chord clears the ring, so one waypoint is the whole detour.
    float turn = Position::NormalizeOrientation(to - from);
    if (turn > static_cast<float>(M_PI))
        turn -= 2.0f * static_cast<float>(M_PI);

    float const heading = from + turn / 2.0f;

    waypoint = Position(middleX + std::cos(heading) * ULDUAR_YOGG_SARON_BODY_DETOUR_RADIUS,
                        middleY + std::sin(heading) * ULDUAR_YOGG_SARON_BODY_DETOUR_RADIUS,
                        destination.GetPositionZ());

    return true;
}

namespace
{
struct YoggSaronWalkLatch
{
    std::string node;
    Position destination;
    float bestDistance = 0.0f;
    uint32 lastProgressMs = 0;
    uint32 lastAskedMs = 0;
};

// Per instance, then per bot, never evicted - the destinations are a handful of fixed spots and three
// portals, so the vector stays short enough for a linear scan. Not thread_local: a map is updated by
// one thread at a time but is never pinned to one, and per-thread copies would hand the same bot a
// fresh latch whenever the pool reassigns its map.
std::mutex yoggSaronWalkLatchesMutex;
std::unordered_map<uint32 /*instanceId*/, std::unordered_map<ObjectGuid, std::vector<YoggSaronWalkLatch>>>
    yoggSaronWalkLatches;
}  // namespace

bool YoggSaronWalkMakingProgress(PlayerbotAI* botAI, char const* node, Position const& destination)
{
    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();
    float const distance = bot->GetExactDist(destination);

    bool giveUp = false;
    char const* branch = "walking";
    {
        std::lock_guard<std::mutex> guard(yoggSaronWalkLatchesMutex);
        std::vector<YoggSaronWalkLatch>& latches = yoggSaronWalkLatches[bot->GetInstanceId()][bot->GetGUID()];

        YoggSaronWalkLatch* latch = nullptr;
        for (YoggSaronWalkLatch& candidate : latches)
        {
            if (candidate.node == node && candidate.destination.GetExactDist(destination) < 1.0f)
            {
                latch = &candidate;
                break;
            }
        }

        if (!latch)
        {
            latches.push_back(YoggSaronWalkLatch{node, destination, distance, now, now});
            latch = &latches.back();
        }

        // A gap in the asking starts a new attempt rather than continuing the old one. Without it a
        // give-up outlives the walk that earned it: the node stands down, the bot ends up somewhere
        // else entirely, and every later walk reads as "no closer than last time" forever.
        bool const fresh = getMSTimeDiff(latch->lastAskedMs, now) >= ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS;
        bool const arrived = distance <= ULDUAR_YOGG_SARON_WALK_ARRIVED_RADIUS;
        latch->lastAskedMs = now;

        if (fresh || arrived || distance < latch->bestDistance)
        {
            latch->bestDistance = distance;
            latch->lastProgressMs = now;
            branch = arrived ? "arrived" : "walking";
        }
        else if (getMSTimeDiff(latch->lastProgressMs, now) >= ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS)
        {
            giveUp = true;
            branch = "gaveup";
        }
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.walk", std::string(node) + " " + branch);

    return !giveUp;
}

std::vector<Unit*> GetYoggSaronDarkVolleyCasters(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, ULDUAR_YOGG_SARON_INTERRUPT_SEARCH_RADIUS);

    std::vector<Unit*> casters;
    for (Creature* guardian : guardians)
    {
        if (!guardian->IsAlive())
            continue;

        if (guardian->FindCurrentSpellBySpellId(SPELL_DARK_VOLLEY) ||
            guardian->FindCurrentSpellBySpellId(SPELL_DARK_VOLLEY_H))
        {
            casters.push_back(guardian);
        }
    }

    return casters;
}

std::vector<char const*> YoggSaronInterruptSpells(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_DEATH_KNIGHT:
            return {"mind freeze", "strangulate"};
        case CLASS_HUNTER:
            return {"silencing shot"};
        case CLASS_MAGE:
            return {"counterspell"};
        case CLASS_ROGUE:
            return {"kick"};
        case CLASS_SHAMAN:
            return {"wind shear"};
        case CLASS_WARRIOR:
            return {"pummel", "shield bash"};
        default:
            if (bot->getRace() == RACE_BLOODELF)
                return {"arcane torrent"};

            return {};
    }
}

bool YoggSaronCanInterrupt(Player* bot) { return !YoggSaronInterruptSpells(bot).empty(); }

std::vector<Unit*> GetYoggSaronNovaThreats(PlayerbotAI* botAI, float radius)
{
    Player* bot = botAI->GetBot();

    // Standing in a nova is the price of killing a Guardian at all, and Sara's Fervor is what turns
    // that price into a death: +100% damage taken for 15 s. Measured over one pull, novas ran to a
    // 13,877 median across 199 hits and 23,789-34,039 across the 4 that landed on a Fervor holder,
    // against caster and healer pools of 25,000-30,000. All four were lethal, one from full health.
    bool const fervor = bot->HasAura(SPELL_SARAS_FERVOR);
    bool const atRange = PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot);
    if (!fervor && !atRange)
        return {};

    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, radius);

    std::vector<Unit*> threats;
    for (Creature* guardian : guardians)
    {
        float const gate = fervor ? ULDUAR_YOGG_SARON_FERVOR_NOVA_HEALTH_PCT
                                  : ULDUAR_YOGG_SARON_GUARDIAN_NOVA_HEALTH_PCT;
        if (!guardian->IsAlive() || guardian->GetHealthPct() > gate)
            continue;

        // At spell range the only way into a 15 yd nova is for the Guardian to have walked over, so
        // one chasing somebody else is a blast the bot is already clear of. A bot carrying Fervor is
        // in it wherever the Guardian is heading.
        if (!fervor && guardian->GetVictim() != bot)
            continue;

        threats.push_back(guardian);
    }

    return threats;
}

Position YoggSaronCloudLead(Creature* cloud)
{
    // An escort-AI creature faces the leg it is walking, so its own orientation is the heading - no
    // need to track the orbit or know which way round it was sent.
    float const travel = cloud->GetSpeed(MOVE_RUN) * ULDUAR_YOGG_SARON_CLOUD_LEAD_MS / 1000.0f;
    float const heading = cloud->GetOrientation();

    return Position(cloud->GetPositionX() + std::cos(heading) * travel,
                    cloud->GetPositionY() + std::sin(heading) * travel, cloud->GetPositionZ());
}

bool YoggSaronRouteClearOfClouds(Player* bot, std::vector<Position> const& clouds, float x, float y)
{
    float const originX = bot->GetPositionX();
    float const originY = bot->GetPositionY();
    float const legX = x - originX;
    float const legY = y - originY;
    float const legSquared = legX * legX + legY * legY;

    for (Position const& cloud : clouds)
    {
        float along = 0.0f;
        if (legSquared > 0.0f)
        {
            along = ((cloud.GetPositionX() - originX) * legX + (cloud.GetPositionY() - originY) * legY) / legSquared;
            along = std::max(0.0f, std::min(1.0f, along));
        }

        if (cloud.GetExactDist2d(originX + legX * along, originY + legY * along) <
            ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS)
            return false;
    }

    return true;
}

bool YoggSaronInfluenceTentaclesCleared(PlayerbotAI* botAI)
{
    // Room radius, not the 200 yd the rest of this fight sweeps at: the Stormwind and Chamber middles
    // are 200.8 yd apart, so a hair more reach and a bot would read the next room's tentacles as its
    // own. Getting that wrong is not a wasted tick - damaging the Brain while one lives is
    // Unit::Kill(who, who) on whoever dealt it. From the brain room itself the reach has to cover
    // whichever room the bot came out of.
    Player* bot = botAI->GetBot();
    float const radius = YoggSaronRoomOf(bot) == YOGG_SARON_ROOM_BRAIN
                             ? ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS + ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS
                             : ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS;

    return !bot->FindNearestCreature(NPC_INFLUENCE_TENTACLE, radius, true);
}

std::vector<Position> GetYoggSaronCrushWedges(PlayerbotAI* botAI, float searchRadius)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE, searchRadius);

    std::vector<Position> wedges;
    for (Creature* crusher : crushers)
    {
        if (!crusher->IsAlive() || crusher->GetVictim() == bot)
            continue;

        wedges.push_back(crusher->GetPosition());
    }

    return wedges;
}

bool InYoggSaronCrushWedge(std::vector<Position> const& wedges, float x, float y, float arcDegrees)
{
    float const arc = arcDegrees * static_cast<float>(M_PI) / 180.0f;

    for (Position const& wedge : wedges)
    {
        if (wedge.GetExactDist2d(x, y) > ULDUAR_YOGG_SARON_CRUSH_RANGE)
            continue;

        float const bearing = std::atan2(y - wedge.GetPositionY(), x - wedge.GetPositionX());
        float offset = Position::NormalizeOrientation(bearing - wedge.GetOrientation());
        if (offset > static_cast<float>(M_PI))
            offset = 2.0f * static_cast<float>(M_PI) - offset;

        if (offset <= arc)
            return true;
    }

    return false;
}

bool YoggSaronFearWindowActive(PlayerbotAI* botAI)
{
    uint32 const phase = YoggSaronPhase(botAI);

    return phase == 2 || phase == 3;
}

namespace
{
// Which cloud orbits can reach this bot. The clouds themselves are in the snapshot, but "how many
// rings could summon on me where I am standing" is a property of the room rather than of any unit in
// it, and it is the number the whole phase 1 design turns on.
std::string DescribeCloudReach(Player* bot)
{
    float const fromMiddle =
        bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());

    constexpr size_t orbits = sizeof(ULDUAR_YOGG_SARON_CLOUD_ORBITS) / sizeof(ULDUAR_YOGG_SARON_CLOUD_ORBITS[0]);

    std::string reach;
    for (size_t orbit = 0; orbit < orbits; ++orbit)
    {
        if (std::abs(fromMiddle - ULDUAR_YOGG_SARON_CLOUD_ORBITS[orbit]) > ULDUAR_YOGG_SARON_CLOUD_SUMMON_REACH)
            continue;

        if (!reach.empty())
            reach += "+";

        reach += "orbit" + std::to_string(orbit + 1);
    }

    return reach.empty() ? "clear" : reach;
}

// Paced per instance rather than per bot: NoteHazard has no change-latch of its own, so 25 bots
// reaching it once a tick would write 25 rows a tick.
void NoteYoggSaronHazards(Player* bot, uint32 phase)
{
    if (phase == 2 || phase == 3)
    {
        RaidObs::NoteHazardCircle(bot->GetMap(), SPELL_KNOCK_BACK_TRIGGERED, ULDUAR_YOGG_SARON_MIDDLE,
                                  ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS, ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS);
    }

    // Every live Crusher, not the set the dodge is asked about - that one drops whichever tentacle is
    // already swinging at the reader, which is exactly the wedge a death has to be explained against.
    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE, ULDUAR_YOGG_SARON_P2_SPACING_MAX_FROM_MIDDLE);
    for (Creature* crusher : crushers)
    {
        if (!crusher->IsAlive())
            continue;

        // A wedge origin with no heading is untestable afterwards, so the facing goes on the row.
        char params[80];
        snprintf(params, sizeof(params), "\"facing\":%.2f,\"arc\":%.1f,\"range\":%.1f", crusher->GetOrientation(),
                 ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC, ULDUAR_YOGG_SARON_CRUSH_RANGE);

        RaidObs::NoteHazard(bot->GetMap(), SPELL_CRUSH_CONE, crusher->GetPosition(), "wedge", params,
                            ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS);
    }
}

void TickYoggSaronObs(PlayerbotAI* botAI, uint32 phase)
{
    if (!RaidObs::Active())
        return;

    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    bool noteHazards = false;
    {
        std::lock_guard<std::mutex> guard(yoggSaronStatesMutex);

        uint32 const now = getMSTime();
        uint32& last = state.obsScanMs[bot->GetGUID()];
        if (last && getMSTimeDiff(last, now) < ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS)
            return;

        last = now;
        state.phase = phase;

        if (phase && (!state.hazardNoteMs ||
                      getMSTimeDiff(state.hazardNoteMs, now) >= ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS))
        {
            state.hazardNoteMs = now;
            noteHazards = true;
        }
    }

    // Per bot, not per instance: the gate closing for one bot while the fight runs is exactly what
    // went wrong with the encounter read, and only a per-bot row shows it.
    RaidObs::NoteDerived(bot, "yogg.engaged", phase ? "engaged" : "idle");

    if (!phase)
        return;

    char const* roomNames[] = {"none", "arena", "stormwind", "icecrown", "chamber", "brain"};
    YoggSaronRoom const room = YoggSaronRoomOf(bot);
    RaidObs::NoteDerived(bot, "yogg.room", roomNames[room]);

    if (room == YOGG_SARON_ROOM_ARENA)
    {
        RaidObs::NoteDerived(bot, "yogg.cloudreach", DescribeCloudReach(bot));

        // The ring lights up at ACTION_YOGG_SARON_APPEAR, which is also when the barrier lands and the
        // phase read turns 2. Reading the ring through the handover as well is the only thing that
        // says whether melee got out before it did.
        bool const handover = phase == 1 && YoggSaronHandoverState(botAI).active;

        if (phase != 1 || handover)
            RaidObs::NoteDerived(bot, "yogg.knockback", YoggSaronInBodyKnockback(bot) ? "inside" : "clear");

        if (phase == 2 || phase == 3)
        {
            std::vector<Position> const wedges = GetYoggSaronCrushWedges(botAI, ULDUAR_YOGG_SARON_CRUSH_RANGE);
            char const* crush = "clear";
            if (InYoggSaronCrushWedge(wedges, bot->GetPositionX(), bot->GetPositionY(),
                                      ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC))
                crush = "wedge";
            else if (!wedges.empty())
                crush = "range";

            RaidObs::NoteDerived(bot, "yogg.crush", crush);
            RaidObs::NoteDerived(
                bot, "yogg.deathray",
                bot->FindNearestCreature(NPC_DEATH_RAY, ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS, true) ? "inside"
                                                                                                          : "clear");
        }

        if (noteHazards)
            NoteYoggSaronHazards(bot, phase);

        return;
    }

    if (room == YOGG_SARON_ROOM_NONE)
        return;

    YoggSaronRoomStateOf(botAI);

    size_t const skulls = GetYoggSaronSkullsInArc(botAI).size();
    RaidObs::NoteDerived(bot, "yogg.skull", skulls ? std::to_string(skulls) + " in arc" : "clear");
}
}  // namespace
