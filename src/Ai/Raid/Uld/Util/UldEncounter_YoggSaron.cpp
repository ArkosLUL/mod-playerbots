/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_YoggSaron.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Creature.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidObs.h"

const std::vector<uint32> ULDUAR_YOGG_SARON_ILLUSION_MOBS = {
    NPC_INFLUENCE_TENTACLE, NPC_RUBY_CONSORT,    NPC_AZURE_CONSORT,       NPC_BRONZE_CONSORT,
    NPC_EMERALD_CONSORT,    NPC_OBSIDIAN_CONSORT, NPC_ALEXTRASZA,         NPC_MALYGOS_ILLUSION,
    NPC_NELTHARION,         NPC_YSERA,           NPC_DEATHSWORN_ZEALOT,   NPC_LICH_KING_ILLUSION,
    NPC_IMMOLATED_CHAMPION, NPC_SUIT_OF_ARMOR,   NPC_GARONA,              NPC_KING_LLANE};

const Position ULDUAR_YOGG_SARON_MIDDLE = Position(1980.28f, -25.5868f, 329.397f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE = Position(1927.1511f, 68.507256f, 242.37657f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE = Position(1925.6553f, -121.59296f, 239.98965f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE = Position(2104.5667f, -25.509348f, 242.64679f);
const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE = Position(1980.1971f, -27.854689f, 236.06789f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE = Position(1954.06f, 21.66f, 239.71f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE = Position(1950.11f, -79.284f, 239.98982f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE = Position(2048.63f, -25.5f, 239.72f);
const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT = Position(1998.5377f, -22.90317f, 324.8895f);
const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT = Position(2018.7628f, -18.896868f, 327.07245f);

bool YoggSaronInPhase1(PlayerbotAI* botAI)
{
    Creature* sara = botAI->GetBot()->FindNearestCreature(NPC_SARA_PHASE_1, 200.0f, true);

    return sara && !YoggSaronInPhase2(botAI) && !YoggSaronInPhase3(botAI);
}

bool YoggSaronInPhase2(PlayerbotAI* botAI)
{
    Creature* yogg = botAI->GetBot()->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);

    return yogg && yogg->IsAlive() && yogg->HasAura(SPELL_SHADOW_BARRIER);
}

// The Brain, not the absence of a phase-1 Guardian. A test made only of absences is true between the
// phases it separates: the last Guardian dies ~9.5 s before the Shadow Barrier lands, and the whole
// raid used to run its phase 3 positioning through that gap. The Brain spawns with the first tentacle
// wave and lives to the end, so it is the positive fact. 200 yd covers the worst case, ~80 yd from the
// room's edge including the 62 yd drop.
bool YoggSaronInPhase3(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* yogg = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);
    if (!yogg || !yogg->IsAlive() || yogg->HasAura(SPELL_SHADOW_BARRIER))
        return false;

    return bot->FindNearestCreature(NPC_BRAIN, 200.0f, true) != nullptr;
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
            ULDUAR_YOGG_SARON_CLOUD_SUMMON_RADIUS)
            return false;
    }

    return true;
}

bool YoggSaronInfluenceTentaclesCleared(PlayerbotAI* botAI)
{
    return !botAI->GetBot()->FindNearestCreature(NPC_INFLUENCE_TENTACLE, 200.0f, true);
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

bool YoggSaronFearWindowActive(PlayerbotAI* botAI) { return YoggSaronInPhase2(botAI) || YoggSaronInPhase3(botAI); }
