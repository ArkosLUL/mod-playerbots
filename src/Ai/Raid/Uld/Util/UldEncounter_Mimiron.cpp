/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Mimiron.h"

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Item.h"
#include "LootMgr.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "ServerFacade.h"
#include "SpellAuras.h"
#include "ThreatManager.h"
#include "Timer.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace EncounterHelpers;

const Position ULDUAR_MIMIRON_ROOM_CENTER = Position(2744.65f, 2569.46f, 364.32f);
const Position ULDUAR_MIMIRON_PHASE3_STAGE = Position(2762.65f, 2569.46f, 364.31f);
const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT = Position(2744.5754f, 2570.8657f, 364.3138f);
const Position ULDUAR_MIMIRON_PHASE1_TANK_SPOT = Position(2691.5762f, 2568.5315f, 364.3138f);
const Position ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[ULDUAR_MIMIRON_PHASE1_STACK_COUNT] = {
    Position(2716.0465f, 2579.9422f, 364.3138f),  // 25 deg off the tank spot
    Position(2717.6562f, 2561.5434f, 364.3138f),  // 345
};

namespace
{
// Where NPC 33576 will be `seconds` from now: it laps the room clockwise on a fixed waypoint path, so
// rotating its current position about the room centre by (speed / radius) * time predicts it. Radius
// is measured live rather than hardcoded, which absorbs the polygon's 110-116 yd wobble.
Position MimironOrbitAhead(Position const& now, float seconds)
{
    float const dx = now.GetPositionX() - ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX();
    float const dy = now.GetPositionY() - ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY();
    float const radius = std::sqrt(dx * dx + dy * dy);
    if (radius < 1.0f)
        return now;

    float const turned = -ULDUAR_MIMIRON_DB_TARGET_SPEED * seconds / radius;
    float const c = std::cos(turned);
    float const sn = std::sin(turned);

    return Position(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + dx * c - dy * sn,
                    ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + dx * sn + dy * c,
                    now.GetPositionZ());
}
}  // namespace

float GetMimironSpinningUpSeconds(Unit* vx001)
{
    if (!vx001)
        return -1.0f;

    Spell* spinningUp = vx001->FindCurrentSpellBySpellId(SPELL_SPINNING_UP);
    if (!spinningUp)
        return -1.0f;

    // Clamped. The channel timer is decremented before the tick that ends the channel, so the last pass
    // can read a negative, and a negative here would predict the ignition backwards.
    return std::max(0.0f, static_cast<float>(spinningUp->GetCastTimeRemaining()) / 1000.0f);
}

MimironBarrageWindow GetMimironBarrageWindow(Player* bot, Unit* vx001)
{
    MimironBarrageWindow window;
    if (!bot || !vx001)
        return window;

    // Spinning Up is a 4 s channel that ends by starting the barrage, and the barrage aura then runs
    // 10 s. Reading both live rather than assuming them is what makes the model survive a bot joining
    // the fight mid-cast.
    float fire = ULDUAR_MIMIRON_BARRAGE_FIRE_SECONDS;
    float const spinning = GetMimironSpinningUpSeconds(vx001);
    if (spinning >= 0.0f)
        window.untilLive = spinning;
    else if (Aura* barrage = vx001->GetAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1))
        fire = static_cast<float>(barrage->GetDuration()) / 1000.0f;
    else
        return window;

    window.valid = true;

    Creature* dbTarget = bot->FindNearestCreature(NPC_MIMIRON_DB_TARGET, 250.0f);
    if (!dbTarget)
    {
        // FaceBarrageArc returns early without 33576, so the core never re-aims and the cone stays
        // frozen wherever it is pointing. A static wedge is the honest read of that; running a sweep
        // that is not happening would walk the raid straight through the beams.
        window.lead = vx001->GetOrientation();
        return window;
    }

    Position const dbNow = dbTarget->GetPosition();
    Position const ignition = MimironOrbitAhead(dbNow, window.untilLive);
    Position const finish = MimironOrbitAhead(dbNow, window.untilLive + fire);

    window.lead = vx001->GetAngle(ignition.GetPositionX(), ignition.GetPositionY());

    float const tail = vx001->GetAngle(finish.GetPositionX(), finish.GetPositionY());
    window.sweep = Position::NormalizeOrientation(window.lead - tail);
    window.rate = fire > 0.0f ? window.sweep / fire : 0.0f;

    return window;
}

bool IsMimironSpotMineSafe(Player* bot, Position const& dest, float clearance)
{
    if (!bot)
        return true;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return true;

    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_PROXIMITY_MINE)
            continue;

        if (dest.GetExactDist2d(unit->GetPositionX(), unit->GetPositionY()) < clearance)
            return false;
    }

    return true;
}

MimironFirefighterHazards GetMimironFirefighterHazards(PlayerbotAI* botAI)
{
    MimironFirefighterHazards hazards;
    if (!botAI || !IsMimironHardModeActive(botAI))
        return hazards;

    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_FLAMES_SPREAD:
            case NPC_FLAMES_INITIAL:
                hazards.flames.push_back(unit->GetPosition());
                break;
            case NPC_FROST_BOMB:
                hazards.bombs.push_back(unit->GetPosition());
                break;
            case NPC_EMERGENCY_FIRE_BOT:
                hazards.fireBots.push_back(unit->GetPosition());
                break;
            default:
                break;
        }
    }

    return hazards;
}

bool IsMimironSpotFireSafe(MimironFirefighterHazards const& hazards, Position const& dest)
{
    for (Position const& node : hazards.flames)
        if (dest.GetExactDist2d(node.GetPositionX(), node.GetPositionY()) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            return false;

    return true;
}

bool IsMimironSpotBombSafe(MimironFirefighterHazards const& hazards, Position const& dest)
{
    for (Position const& bomb : hazards.bombs)
        if (dest.GetExactDist2d(bomb.GetPositionX(), bomb.GetPositionY()) < ULDUAR_MIMIRON_FROST_BOMB_RADIUS)
            return false;

    return true;
}

bool IsMimironSpotFireBotSafe(Player* bot, MimironFirefighterHazards const& hazards, Position const& dest)
{
    if (hazards.fireBots.empty())
        return true;

    bool const silenced = bot && (PlayerbotAI::IsCaster(bot) || PlayerbotAI::IsHeal(bot)) &&
                          bot->GetMap()->Is25ManRaid();

    for (Position const& fireBot : hazards.fireBots)
    {
        float const dx = dest.GetPositionX() - fireBot.GetPositionX();
        float const dy = dest.GetPositionY() - fireBot.GetPositionY();
        if (silenced && std::sqrt(dx * dx + dy * dy) < ULDUAR_MIMIRON_FIREBOT_SIREN_CLEARANCE)
            return false;

        // The line only reaches forward: HasInLine checks the front half-circle first.
        float const facing = fireBot.GetOrientation();
        float const ahead = dx * std::cos(facing) + dy * std::sin(facing);
        float const side = dy * std::cos(facing) - dx * std::sin(facing);
        if (ahead >= 0.0f && ahead < ULDUAR_MIMIRON_FIREBOT_SPRAY_LENGTH &&
            std::fabs(side) < ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH)
            return false;
    }

    return true;
}

bool IsMimironSpotShockSafe(PlayerbotAI* botAI, Position const& dest)
{
    Unit* leviathanMkII = botAI ? GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) : nullptr;
    if (!leviathanMkII || !leviathanMkII->FindCurrentSpellBySpellId(SPELL_SHOCK_BLAST))
        return true;

    return leviathanMkII->GetExactDist2d(dest.GetPositionX(), dest.GetPositionY()) >=
           ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST;
}

namespace
{
// The standing hazards MimironFirefighterHazards leaves out: mines, live Rocket Strike markers and a
// Shock Blast being cast. Gathered once, so screening a batch of candidate spots costs one scan.
struct MimironMarkers
{
    std::vector<Position> mines;
    std::vector<Position> rockets;
    bool shock = false;
    Position shockCentre;
};

MimironMarkers GetMimironMarkers(PlayerbotAI* botAI)
{
    MimironMarkers markers;
    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() == NPC_PROXIMITY_MINE)
            markers.mines.push_back(unit->GetPosition());
        else if (unit->GetEntry() == NPC_ROCKET_STRIKE_N)
            markers.rockets.push_back(unit->GetPosition());
    }

    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (leviathanMkII && leviathanMkII->FindCurrentSpellBySpellId(SPELL_SHOCK_BLAST))
    {
        markers.shock = true;
        markers.shockCentre = leviathanMkII->GetPosition();
    }

    return markers;
}

bool IsMimironSpotStandable(Player* bot, Position const& dest, MimironMarkers const& markers,
                            MimironFirefighterHazards const& hazards)
{
    for (Position const& mine : markers.mines)
        if (dest.GetExactDist2d(mine.GetPositionX(), mine.GetPositionY()) < ULDUAR_MIMIRON_MINE_CLEARANCE)
            return false;

    for (Position const& rocket : markers.rockets)
        if (dest.GetExactDist2d(rocket.GetPositionX(), rocket.GetPositionY()) < ULDUAR_MIMIRON_ROCKET_CLEARANCE)
            return false;

    if (markers.shock && dest.GetExactDist2d(markers.shockCentre.GetPositionX(),
                                             markers.shockCentre.GetPositionY()) <
                             ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST)
        return false;

    // Firefighter spreads ground fire across the floor, so a standing spot can end up inside it, and
    // the Frost Bomb makes a 30 yd disc of the room lethal for ten seconds at a time. Without the fire
    // half the flames node at ACTION_RAID + 4 pushes the bot out and the formation at ACTION_RAID pulls
    // it straight back, and it paces on the edge until it burns down; without the bomb half the
    // formation walks the raid back into the blast while the fuse runs.
    return IsMimironSpotFireSafe(hazards, dest) && IsMimironSpotBombSafe(hazards, dest) &&
           IsMimironSpotFireBotSafe(bot, hazards, dest);
}

// The straight walk from `from` to `dest` against the fire. Nodes `from` already stands in are left
// out, since walking out of those is the point.
bool IsMimironLegFireSafe(Position const& from, MimironFirefighterHazards const& hazards,
                          Position const& dest)
{
    if (hazards.flames.empty())
        return true;

    float const fromX = from.GetPositionX();
    float const fromY = from.GetPositionY();
    float const dx = dest.GetPositionX() - fromX;
    float const dy = dest.GetPositionY() - fromY;
    float const lengthSq = dx * dx + dy * dy;
    if (lengthSq <= 0.0f)
        return true;

    for (Position const& node : hazards.flames)
    {
        float const nx = node.GetPositionX() - fromX;
        float const ny = node.GetPositionY() - fromY;
        if (std::sqrt(nx * nx + ny * ny) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            continue;

        // Closest point of the walk to this node.
        float const t = std::clamp((nx * dx + ny * dy) / lengthSq, 0.0f, 1.0f);
        float const ox = nx - t * dx;
        float const oy = ny - t * dy;
        if (std::sqrt(ox * ox + oy * oy) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            return false;
    }

    return true;
}
}  // namespace

bool IsMimironWalkFireSafe(Player* bot, MimironFirefighterHazards const& hazards, Position const& dest)
{
    return !bot || IsMimironLegFireSafe(bot->GetPosition(), hazards, dest);
}

bool IsMimironSpotSafe(Player* bot, Position const& dest)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI)
        return true;

    return IsMimironSpotStandable(bot, dest, GetMimironMarkers(botAI), GetMimironFirefighterHazards(botAI));
}

std::vector<MimironApproach> GetMimironSlotApproaches(PlayerbotAI* botAI, Player* bot, Position const& slot,
                                                      MimironFirefighterHazards const& hazards)
{
    std::vector<MimironApproach> approaches;
    if (!botAI || !bot)
        return approaches;

    MimironMarkers const markers = GetMimironMarkers(botAI);
    Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001);
    MimironRapidBurstWindow const burst =
        vx001 ? GetMimironRapidBurstWindow(botAI, bot, vx001) : MimironRapidBurstWindow();
    auto const standable = [&](Position const& spot)
    {
        return IsMimironSpotStandable(bot, spot, markers, hazards) &&
               IsMimironSpotRapidBurstSafe(vx001, burst, spot);
    };

    Position goal = slot;
    char const* how = "direct";
    if (!IsMimironSpotStandable(bot, slot, markers, hazards))
    {
        // Already on clear ground beside it: stay. A substitute moves every time the fire grows, and
        // chasing it is a walk every tick.
        if (bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) <=
                ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS &&
            IsMimironSpotStandable(bot, bot->GetPosition(), markers, hazards))
            return approaches;

        // Phase 1 keeps its slots. Its camp moves off its own fire by switching anchors, and a stand-in
        // squeezed between 6 yd rows is how two bots end up under one Napalm Shell.
        if (!vx001 && GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
            !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
            return approaches;

        std::vector<Position> others;
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (member && member != bot && member->IsAlive() && member->IsInMap(bot))
                    others.push_back(member->GetPosition());
            }
        }

        // Nearest ring first, and on each ring the bearing toward the bot first, so the stand-in is on
        // the near side of the slot.
        float const towardBot = slot.GetAngle(bot->GetPositionX(), bot->GetPositionY());
        bool found = false;
        for (float radius = 2.0f; radius <= ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS + 0.01f && !found;
             radius += 2.0f)
        {
            for (uint32 step = 0; step < 12 && !found; ++step)
            {
                float const turn = static_cast<float>((step + 1) / 2) * static_cast<float>(M_PI) / 6.0f;
                float const bearing = towardBot + (step % 2 ? turn : -turn);
                Position const candidate(slot.GetPositionX() + radius * std::cos(bearing),
                                         slot.GetPositionY() + radius * std::sin(bearing),
                                         slot.GetPositionZ());
                if (!standable(candidate))
                    continue;

                bool crowded = false;
                for (Position const& other : others)
                {
                    if (candidate.GetExactDist2d(other.GetPositionX(), other.GetPositionY()) <
                        ULDUAR_MIMIRON_DISPERSE_DISTANCE)
                    {
                        crowded = true;
                        break;
                    }
                }

                if (!crowded)
                {
                    goal = candidate;
                    found = true;
                }
            }
        }

        if (!found)
            return approaches;

        how = "substitute";
    }

    Position const from = bot->GetPosition();
    if (IsMimironLegFireSafe(from, hazards, goal))
    {
        approaches.push_back({goal, how, 0.0f});
        return approaches;
    }

    // Round the fire instead: one waypoint over the walk's midpoint, whose legs both miss it.
    float const distance = from.GetExactDist2d(goal.GetPositionX(), goal.GetPositionY());
    float const bearing = from.GetAngle(goal.GetPositionX(), goal.GetPositionY());
    for (float degrees : ULDUAR_MIMIRON_DETOUR_TURNS_DEG)
    {
        float const turn = degrees * static_cast<float>(M_PI) / 180.0f;
        float const leg = 0.5f * distance / std::cos(turn);
        for (float sign : {1.0f, -1.0f})
        {
            Position const waypoint(from.GetPositionX() + leg * std::cos(bearing + sign * turn),
                                    from.GetPositionY() + leg * std::sin(bearing + sign * turn),
                                    from.GetPositionZ());
            if (standable(waypoint) && IsMimironLegFireSafe(from, hazards, waypoint) &&
                IsMimironLegFireSafe(waypoint, hazards, goal))
                approaches.push_back({waypoint, "detour", sign * turn});
        }
    }

    return approaches;
}

bool IsMimironSpotBarrageSafe(Unit* vx001, MimironBarrageWindow const& window, Position const& dest,
                              float travelSeconds)
{
    if (!vx001 || !window.valid)
        return true;

    float const clearance = ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE + ULDUAR_MIMIRON_BARRAGE_MARGIN;
    float const twoPi = 2.0f * static_cast<float>(M_PI);

    // Extend the band by the sweep the leg will not be able to react to. The band only grows on the
    // trailing side - that is the edge coming toward a bot standing still. Nothing to add while the
    // boss is still spinning up: the band is fixed in world space until it ignites and `sweep` already
    // spans the whole fire, so growing it again would refuse bearings that are clear. The dodge zeroes
    // its own sweep rate for the same window, and the two have to agree or a flee walks into a spot the
    // dodge just called safe.
    float const rate = window.untilLive > 0.0f ? 0.0f : window.rate;
    float const grown =
        std::min(window.sweep + rate * std::max(travelSeconds, 0.0f), twoPi - 2.0f * clearance);

    float const cw = Position::NormalizeOrientation(
        window.lead - vx001->GetAngle(dest.GetPositionX(), dest.GetPositionY()));

    return cw > grown + clearance && cw < twoPi - clearance;
}

std::string GetMimironBombBotSnare(Player* bot)
{
    if (!bot)
        return "";

    switch (bot->getClass())
    {
        case CLASS_HUNTER:  return "concussive shot";
        case CLASS_SHAMAN:  return "frost shock";
        case CLASS_WARLOCK: return "curse of exhaustion";
        default:            return "";
    }
}

Unit* GetMimironBombBotChasing(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return nullptr;

    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_BOMB_BOT)
            continue;

        if (ServerFacade::instance().GetChaseTarget(unit) == bot)
            return unit;
    }

    return nullptr;
}

float GetMimironBombBotApproach(Player* bot, Unit* bombBot)
{
    if (!bot || !bombBot)
        return 0.0f;

    if (Unit* victim = ServerFacade::instance().GetChaseTarget(bombBot))
        return bombBot->GetExactDist2d(victim);

    return bombBot->GetExactDist2d(bot);
}

bool IsMimironAcuGrounded(PlayerbotAI* botAI)
{
    if (!botAI)
        return false;

    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    return aerialCommandUnit && aerialCommandUnit->HasAura(SPELL_MIMIRON_MAGNETIC_CORE_AURA);
}

bool IsMimironAcuAirborne(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return false;

    // Phase 4 has it attackable too, sitting on the chassis.
    return GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT) && !IsMimironPhase4(bot) &&
           !IsMimironAcuGrounded(botAI);
}

Unit* GetMimironRingFocus(PlayerbotAI* botAI)
{
    if (!botAI)
        return nullptr;

    if (Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001))
        return vx001;

    if (Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII))
        return leviathanMkII;

    return GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
}

Unit* GetMimironStagingFocus(Player* bot)
{
    if (!bot)
        return nullptr;

    Creature* vx001 = bot->FindNearestCreature(NPC_VX001, ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);
    Creature* aerialCommandUnit =
        bot->FindNearestCreature(NPC_AERIAL_COMMAND_UNIT, ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);

    // VX-001 only ever rides anything in phase 4, so this is the moment the assembly is far enough
    // along to be worth forming a ring for. It boards 18.8 s into a 31.8 s handover, which still leaves
    // more than twice the walk from a phase 3 wedge slot.
    if (vx001 && vx001->GetVehicleBase())
        return vx001;

    if (aerialCommandUnit)
        return aerialCommandUnit;

    // Nothing before the pull or after a wipe: the MK II is NOT_SELECTABLE until it is pulled, and
    // evade despawns VX-001 and the Aerial Command Unit outright.
    return vx001;
}

namespace
{
// Which constructs are up. Read against this enum, not guessed from the number: a trace records the
// raw value, and the handover is the state that matters most and is not a phase number at all.
enum MimironTracedPhase : uint32
{
    MIMIRON_TRACE_NONE = 0,      // nothing in the room - before the pull, or after a wipe
    MIMIRON_TRACE_MKII = 1,      // phase 1
    MIMIRON_TRACE_VX001 = 2,     // phase 2
    MIMIRON_TRACE_ACU = 3,       // phase 3
    MIMIRON_TRACE_ALL = 4,       // phase 4, all three assembled
    MIMIRON_TRACE_HANDOVER = 5,  // a construct exists but nothing is attackable yet
};

// Raid-wide answers, folded once per instance per tick rather than once per bot. All three are
// derived fresh everywhere else and stored nowhere, so without this a trace has no phase timeline,
// no Magnetic Core window and no way to say who was supposed to be carrying the core.
struct MimironObsState
{
    RaidObs::ObsValue<uint32> phase{"mimiron.phase"};

    // Aura 64436 sits on the Aerial Command Unit, and RaidObs records auras for roster players only,
    // so the one window phase 3 can be shortened in is otherwise invisible.
    RaidObs::ObsValue<bool> acuGrounded{"mimiron.core"};

    RaidObs::ObsValue<ObjectGuid> coreCarrier{"mimiron.carrier"};

    uint32 scanMs = 0;
};

// Not thread_local. A map is updated by one thread at a time but is never pinned to one, so
// per-thread copies hand the same instance a fresh state whenever the pool reassigns it, which
// silently resets every latch mid-pull. References into an unordered_map survive rehashing, so the
// lock only has to cover the lookup.
std::mutex mimironObsStatesMutex;
std::unordered_map<uint32 /*instanceId*/, MimironObsState> mimironObsStates;

MimironObsState& MimironObsStateFor(Player* bot)
{
    std::lock_guard<std::mutex> guard(mimironObsStatesMutex);
    return mimironObsStates[bot->GetInstanceId()];
}

// Raid-wide phase 1 answers, and both have to be the same for every bot in the instance. A tank
// hold each bot latched for itself would let the formation start walking while the tank was still
// building threat; a stack anchor picked per bot is not a stack. Same locking as the state above -
// the map is only touched by one thread at a time but is not pinned to one, and references into it
// survive rehashing, so the guard only has to cover the lookup.
struct MimironFightState
{
    // One-way per pull. Set when the main tank's threat lead on the MK II is real, or when the hold
    // times out; until then he has no anchor and stands on the boss.
    bool tankDragReady = false;
    uint32 tankHoldStartedMs = 0;

    uint8 stackAnchor = 0;
    uint32 stackPickedMs = 0;
    uint32 stackScanMs = 0;

    // Extra turn on the phase 1 camp toward the room, radians, so every slot keeps sight of the MK II.
    float campTurn = 0.0f;
    uint32 campTurnScanMs = 0;
    uint32 campTurnRaisedMs = 0;

    ObjectGuid plasmaClaimedBy;
    uint32 plasmaWindowMs = 0;

    // Assault Bot corpses that already gave their Magnetic Core.
    std::unordered_set<ObjectGuid> coreCorpses;

    std::vector<ObjectGuid> keptFireBots;
    uint32 fireBotScanMs = 0;
};

std::mutex mimironFightStatesMutex;
std::unordered_map<uint32 /*instanceId*/, MimironFightState> mimironFightStates;

MimironFightState& MimironFightStateFor(Player* bot)
{
    std::lock_guard<std::mutex> guard(mimironFightStatesMutex);
    return mimironFightStates[bot->GetInstanceId()];
}

// Everything the trace needs once per instance per tick rather than once per bot: the phase, the
// Magnetic Core window, who holds the core, and the Laser Barrage cone.
void TickMimironObs(PlayerbotAI* botAI, Player* bot, Unit* leviathanMkII, Unit* vx001,
                    Unit* aerialCommandUnit)
{
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return;

    MimironObsState& state = MimironObsStateFor(bot);
    if (state.scanMs && GetMSTimeDiffToNow(state.scanMs) < ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS)
        return;

    state.scanMs = getMSTime();

    // Off the constructs, never off the calling bot's combat state: one bot dropping combat is not a
    // wipe, and a latch left set would leave the re-pull with nothing to emit.
    if (!leviathanMkII && !vx001 && !aerialCommandUnit)
    {
        // Nothing attackable is either a handover or an empty room, and telling those apart is the
        // whole point of the row - the handovers are where the raid paces. GetMimironStagingFocus
        // scans for the creature rather than the target list, so it still finds a NOT_SELECTABLE
        // mech mid-script, and both constructs it looks for are despawned by an evade.
        state.phase = GetMimironStagingFocus(bot) ? MIMIRON_TRACE_HANDOVER : MIMIRON_TRACE_NONE;
        state.acuGrounded = false;
        state.coreCarrier = ObjectGuid::Empty;
        ResetMimironFightState(bot);
        return;
    }

    if (leviathanMkII && vx001 && aerialCommandUnit)
        state.phase = MIMIRON_TRACE_ALL;
    else if (aerialCommandUnit)
        state.phase = MIMIRON_TRACE_ACU;
    else if (vx001)
        state.phase = MIMIRON_TRACE_VX001;
    else
        state.phase = MIMIRON_TRACE_MKII;

    state.acuGrounded = IsMimironAcuGrounded(botAI);

    // Past here the work is only worth doing for a trace: the carrier election walks the group, and
    // reading the barrage window costs a grid scan for the DB Target.
    if (!RaidObs::Active())
        return;

    Player* carrier = GetMimironCoreCarrier(botAI);
    state.coreCarrier = carrier ? carrier->GetGUID() : ObjectGuid::Empty;

    if (!vx001)
        return;

    // The cone has no world object behind it, so the snapshot sweep has nothing to find and a death
    // inside the beams reads as damage from nowhere. The DB Target it aims at is not hostile either,
    // so the sweep skips that too - this helper is the only thing that knows where the cone points.
    MimironBarrageWindow const window = GetMimironBarrageWindow(bot, vx001);
    if (!window.valid)
        return;

    char params[96];
    snprintf(params, sizeof(params), "\"lead\":%.2f,\"sweep\":%.2f,\"rate\":%.2f,\"live\":%.1f",
             window.lead, window.sweep, window.rate, window.untilLive);

    // Two spell ids rather than one, so the 4 s Spinning Up warning and the 10 s barrage read as
    // separate stages on the timeline. The origin is resampled every pass because in phase 4 VX-001
    // rides the chassis, and the apex drifting under the raid is the thing worth seeing.
    RaidObs::NoteHazard(bot->GetMap(),
                        window.untilLive > 0.0f ? SPELL_SPINNING_UP : SPELL_P3WX2_LASER_BARRAGE_AURA_1,
                        vx001->GetPosition(), "sweep", params, ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS);
}
}  // namespace

bool IsMimironEngaged(PlayerbotAI* botAI)
{
    // Any construct, because each phase hands over to the next: the outgoing one goes passive and
    // unselectable while the incoming one calls SetInCombatWithZone, so between the two there is
    // nothing worth targeting anyway.
    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001);
    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);

    // Ahead of the combat test, because the housekeeping it drives includes the wipe reset. Every
    // non-tank reaches here every tick through MimironTargetGuardMultiplier, and the pass throttles
    // itself, so this is the one place a raid-wide fold is guaranteed to run.
    TickMimironObs(botAI, botAI->GetBot(), leviathanMkII, vx001, aerialCommandUnit);

    for (Unit* construct : {leviathanMkII, vx001, aerialCommandUnit})
        if (construct && construct->IsInCombat())
            return true;

    return false;
}

bool IsMimironPhase4(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI)
        return false;

    // Cached lookups only. This is asked several times per bot per tick, from the target list, the
    // tank node and the pet node, so a grid sweep here would cost the whole raid every phase.

    // VX-001 and the Aerial Command Unit both ride something from phase 4 on, and neither does before
    // it, so either one answers on its own while it is still attackable.
    if (Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001))
        return vx001->GetVehicleBase() != nullptr;

    if (Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return aerialCommandUnit->GetVehicleBase() != nullptr;

    // Both pushed under 15000 and gone NON_ATTACKABLE, so only the chassis is left. It never rides
    // anything itself, but seat 3 holds the cannon in phase 1 and VX-001 from phase 4 on, and who is
    // sitting there is readable whatever flags the passengers carry.
    if (Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII))
        if (Vehicle* kit = leviathanMkII->GetVehicleKit())
            if (Unit* seated = kit->GetPassenger(3))
                return seated->GetEntry() == NPC_VX001;

    return false;
}

Unit* GetMimironPhase4Focus(PlayerbotAI* botAI, Player* bot, bool melee)
{
    if (!botAI || !bot || !IsMimironPhase4(bot))
        return nullptr;

    std::vector<Unit*> parts;
    for (uint32 entry : {NPC_LEVIATHAN_MKII, NPC_VX001, NPC_AERIAL_COMMAND_UNIT})
        if (Unit* part = GetFirstAliveUnitByEntry(botAI, entry))
            parts.push_back(part);

    if (parts.empty())
        return nullptr;

    // Banded, and ties broken by the fixed entry order the parts are collected in. Both halves matter:
    // the band stops the two ground mechs trading the lead several times a second while the raid burns
    // them level, and deriving the answer from state alone is what keeps the tank node, this node and
    // the pets on the same part - a per-bot "what was I on last tick" would let the three disagree.
    auto const highest = [](std::vector<Unit*> const& candidates) -> Unit*
    {
        auto const band = [](Unit* unit)
        { return static_cast<int32>(unit->GetHealthPct() / ULDUAR_MIMIRON_PHASE4_FOCUS_BAND_PCT); };

        Unit* best = nullptr;
        for (Unit* candidate : candidates)
            if (!best || band(candidate) > band(best))
                best = candidate;

        return best;
    };

    // Fewer than three attackable means one is already channelling Self Repair and the 15 s clock is
    // running. The rendezvous is over, so every restriction comes off - including melee on the Aerial
    // Command Unit, who would otherwise have nothing to hit through the stretch that decides whether
    // the kill lands or the whole phase resets.
    if (parts.size() < 3)
        return highest(parts);

    std::vector<Unit*> allowed;
    for (Unit* part : parts)
    {
        // Ranged DPS own the Aerial Command Unit. IsRangedDps rather than IsRanged so a healer is never
        // steered onto it, nor into the hold below, where it would stop healing.
        if (part->GetEntry() == NPC_AERIAL_COMMAND_UNIT && (melee || !PlayerbotAI::IsRangedDps(bot)))
            continue;

        allowed.push_back(part);
    }

    if (allowed.empty())
        return nullptr;

    // All three levelled out, so stop holding and push them under together.
    bool levelled = true;
    for (Unit* part : parts)
        if (part->GetHealthPct() > ULDUAR_MIMIRON_PHASE4_HOLD_PCT)
            levelled = false;

    if (levelled)
        return highest(allowed);

    std::vector<Unit*> aboveFloor;
    for (Unit* part : allowed)
        if (part->GetHealthPct() > ULDUAR_MIMIRON_PHASE4_HOLD_PCT)
            aboveFloor.push_back(part);

    // Nothing left this bot may touch that is not already at the floor. Hold: all three sit on one
    // point server-side, so cleave splashes every part, and 10 % is the margin that keeps incidental
    // damage from pushing one under while the others are still high.
    return aboveFloor.empty() ? nullptr : highest(aboveFloor);
}

Spell* GetMimironPlasmaBlastCast(Unit* cannon)
{
    if (!cannon)
        return nullptr;

    if (Spell* spell = cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST))
        return spell;

    return cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST_25);
}

bool IsMimironTankAnchorSlot(PlayerbotAI* botAI, Player* bot)
{
    // Phases 1 and 4 both park the MK II, and both are the phases it lays mines in.
    return botAI && bot && PlayerbotAI::IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) != nullptr;
}

float GetMimironPhase1DisperseDistance(PlayerbotAI* /*botAI*/)
{
    // Both modes. The Firefighter camp deals its own 6 yd slots, and a threshold equal to that would
    // have the unstacker judge every bot on its slot too close and shove it off.
    return ULDUAR_MIMIRON_DISPERSE_DISTANCE;
}

void ResetMimironFightState(Player* bot)
{
    if (!bot)
        return;

    MimironFightState& state = MimironFightStateFor(bot);
    state = MimironFightState();
}

bool ClaimMimironPlasmaWindow(Player* bot)
{
    if (!bot)
        return false;

    MimironFightState& state = MimironFightStateFor(bot);
    if (state.plasmaWindowMs &&
        GetMSTimeDiffToNow(state.plasmaWindowMs) < ULDUAR_MIMIRON_PLASMA_WINDOW_MS)
    {
        return state.plasmaClaimedBy == bot->GetGUID();
    }

    state.plasmaWindowMs = getMSTime();
    state.plasmaClaimedBy = bot->GetGUID();
    return true;
}

bool IsMimironTankDragReady(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return false;

    MimironFightState& state = MimironFightStateFor(bot);
    if (state.tankDragReady)
        return true;

    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (!leviathanMkII)
        return false;

    if (!state.tankHoldStartedMs)
        state.tankHoldStartedMs = getMSTime();
    else if (GetMSTimeDiffToNow(state.tankHoldStartedMs) >= ULDUAR_MIMIRON_TANK_HOLD_MAX_MS)
    {
        // Nobody is going to win this threat table. Walking a boss somebody else is holding is still
        // better than standing at the pull spot burning the middle of the room down.
        state.tankDragReady = true;
        return true;
    }

    if (leviathanMkII->GetVictim() != bot)
        return false;

    ThreatManager& mgr = leviathanMkII->GetThreatMgr();
    float const tankThreat = mgr.GetThreat(bot);
    if (tankThreat <= 0.0f)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Against the raid rather than against the runner-up tank: an off-tank above the lead is fine,
    // it is a dps crossing the line that stops the boss dead halfway through the drag.
    float highest = 0.0f;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || PlayerbotAI::IsTank(member))
            continue;

        highest = std::max(highest, mgr.GetThreat(member));
    }

    if (tankThreat < highest * ULDUAR_MIMIRON_TANK_THREAT_LEAD)
        return false;

    state.tankDragReady = true;
    return true;
}

Position const& GetMimironPhase1StackAnchor(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[0];

    MimironFightState& state = MimironFightStateFor(bot);

    // Gathering the field costs a scan of every nearby npc, and twenty-five bots ask for this every
    // tick, so the answer is folded on the same interval the observability pass uses.
    if (state.stackScanMs &&
        GetMSTimeDiffToNow(state.stackScanMs) < ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS)
    {
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[state.stackAnchor];
    }

    state.stackScanMs = getMSTime();

    if (state.stackPickedMs && GetMSTimeDiffToNow(state.stackPickedMs) < ULDUAR_MIMIRON_STACK_HOLD_MS)
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[state.stackAnchor];

    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);

    uint32 counts[ULDUAR_MIMIRON_PHASE1_STACK_COUNT] = {};
    for (uint8 i = 0; i < ULDUAR_MIMIRON_PHASE1_STACK_COUNT; ++i)
    {
        Position const& spot = ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[i];
        for (Position const& flame : hazards.flames)
        {
            if (spot.GetExactDist2d(flame.GetPositionX(), flame.GetPositionY()) <=
                ULDUAR_MIMIRON_STACK_FIRE_RADIUS)
            {
                ++counts[i];
            }
        }
    }

    uint8 const live = state.stackAnchor;
    if (counts[live] <= ULDUAR_MIMIRON_STACK_FIRE_LIMIT)
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[live];

    uint8 best = live;
    for (uint8 i = 0; i < ULDUAR_MIMIRON_PHASE1_STACK_COUNT; ++i)
        if (counts[i] + ULDUAR_MIMIRON_STACK_FIRE_MARGIN <= counts[best])
            best = i;

    if (best == live)
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[live];

    state.stackAnchor = best;
    state.stackPickedMs = getMSTime();

    // Both counts, not just the winner: a switch that traded two nodes for one is the raid pacing,
    // and that reads identically to a good one unless the number it left behind is on the line.
    if (RaidObs::Active())
    {
        char line[48];
        snprintf(line, sizeof(line), "%u:%u -> %u:%u", uint32(live), counts[live], uint32(best),
                 counts[best]);
        RaidObs::NoteDerived(bot, "mimiron.stack", line);
    }

    return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[best];
}

MimironRapidBurstWindow GetMimironRapidBurstWindow(PlayerbotAI* botAI, Player* bot, Unit* vx001)
{
    MimironRapidBurstWindow window;
    if (!botAI || !bot || !vx001 || !vx001->IsAlive())
        return window;

    Group* group = bot->GetGroup();
    if (!group)
        return window;

    Player* carrier = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref && !carrier; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->HasAura(SPELL_MIMIRON_RAPID_BURST))
            carrier = member;
    }

    if (!carrier)
        return window;

    window.valid = true;
    window.centreline = vx001->GetAngle(carrier->GetPositionX(), carrier->GetPositionY());

    float const mine = vx001->GetAngle(bot->GetPositionX(), bot->GetPositionY());
    float off = Position::NormalizeOrientation(mine - window.centreline);
    if (off > static_cast<float>(M_PI))
        off -= 2.0f * static_cast<float>(M_PI);
    window.offset = off;

    // Arc, not chord: the bot leaves the cone by turning around VX-001, and how far that is scales
    // with how far out it is standing. Zero once it is already clear, which is most of the raid.
    float const inside = ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE - std::fabs(window.offset);
    window.escape = inside <= 0.0f
                        ? 0.0f
                        : (inside + ULDUAR_MIMIRON_RAPID_BURST_MARGIN) * bot->GetExactDist2d(vx001);

    return window;
}

bool IsMimironSpotRapidBurstSafe(Unit* vx001, MimironRapidBurstWindow const& window,
                                 Position const& dest)
{
    if (!vx001 || !window.valid)
        return true;

    float const bearing = vx001->GetAngle(dest.GetPositionX(), dest.GetPositionY());
    float off = Position::NormalizeOrientation(bearing - window.centreline);
    if (off > static_cast<float>(M_PI))
        off -= 2.0f * static_cast<float>(M_PI);

    return std::fabs(off) > ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE;
}

Player* GetMimironCoreCarrier(PlayerbotAI* botAI)
{
    if (!botAI)
        return nullptr;

    Group* group = botAI->GetBot()->GetGroup();
    if (!group)
        return nullptr;

    Player* fallback = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !GET_PLAYERBOT_AI(member))
            continue;

        if (!fallback)
            fallback = member;

        if (!PlayerbotAI::IsRanged(member) && !PlayerbotAI::IsTank(member))
            return member;
    }

    return fallback;
}

namespace
{
enum : int32
{
    MIMIRON_CORE_NOT_IN_LOOT = -1,  // loot never filled for this corpse
    MIMIRON_CORE_ALREADY_TAKEN = -2
};

// The loot slot holding this corpse's Magnetic Core, or one of the two codes above.
int32 MimironCoreLootSlot(Creature* corpse)
{
    int32 result = MIMIRON_CORE_NOT_IN_LOOT;
    for (size_t i = 0; i < corpse->loot.items.size(); ++i)
    {
        LootItem const& item = corpse->loot.items[i];
        if (item.itemid != ITEM_MIMIRON_MAGNETIC_CORE)
            continue;

        if (!item.is_looted)
            return static_cast<int32>(i);

        result = MIMIRON_CORE_ALREADY_TAKEN;
    }

    return result;
}
}  // namespace

Creature* GetMimironCoreCorpse(Player* bot)
{
    if (!bot)
        return nullptr;

    MimironFightState& state = MimironFightStateFor(bot);

    std::list<Creature*> assaultBots;
    bot->GetCreatureListWithEntryInGrid(assaultBots, NPC_ASSAULT_BOT, ULDUAR_MIMIRON_CORE_SEARCH_RANGE);

    Creature* nearest = nullptr;
    float nearestDist = 0.0f;
    for (Creature* corpse : assaultBots)
    {
        if (!corpse || corpse->IsAlive() || state.coreCorpses.count(corpse->GetGUID()) ||
            MimironCoreLootSlot(corpse) == MIMIRON_CORE_ALREADY_TAKEN)
            continue;

        float const dist = bot->GetExactDist2d(corpse);
        if (!nearest || dist < nearestDist)
        {
            nearest = corpse;
            nearestDist = dist;
        }
    }

    return nearest;
}

bool TakeMimironCore(Player* bot, Creature* corpse)
{
    if (!bot || !corpse)
        return false;

    ItemPosCountVec dest;
    if (bot->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_MIMIRON_MAGNETIC_CORE, 1) != EQUIP_ERR_OK)
        return false;

    MimironFightStateFor(bot).coreCorpses.insert(corpse->GetGUID());

    // Same bookkeeping as Player::StoreLootItem and the creature branch of DoLootRelease, so the
    // corpse shows nothing left to a player who opens it afterwards.
    int32 const slot = MimironCoreLootSlot(corpse);
    if (slot >= 0)
    {
        Loot& loot = corpse->loot;
        loot.items[slot].is_looted = true;
        --loot.unlootedCount;
        loot.NotifyItemRemoved(static_cast<uint8>(slot));

        if (loot.isLooted())
        {
            corpse->AllLootRemovedFromCorpse();
            corpse->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
            loot.clear();
        }
    }

    bot->StoreNewItem(dest, ITEM_MIMIRON_MAGNETIC_CORE, true,
                      Item::GenerateItemRandomPropertyId(ITEM_MIMIRON_MAGNETIC_CORE));
    return true;
}

bool IsMimironCoreUseReady(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return false;

    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit || IsMimironAcuGrounded(botAI) ||
        !aerialCommandUnit->HasUnitMovementFlag(MOVEMENTFLAG_HOVER))
        return false;

    // Hover alone is not enough: it stays set for the 3 s before a core arms, and the script sets it
    // again while an aura is still on, so a carrier going by it chains core after core.
    return !bot->FindNearestCreature(NPC_MAGNETIC_CORE, ULDUAR_MIMIRON_CORE_PENDING_RANGE);
}

std::vector<ObjectGuid> GetMimironKeptFireBots(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !IsMimironHardModeActive(botAI))
        return {};

    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit || IsMimironPhase4(bot) ||
        aerialCommandUnit->GetHealthPct() <= ULDUAR_MIMIRON_FIREBOT_CLEANUP_PCT)
        return {};

    // A grid scan, since "possible targets" is per bot and the kept pair has to be the same for
    // everyone. Folded on the observability interval, like the stack anchor.
    MimironFightState& state = MimironFightStateFor(bot);
    if (!state.fireBotScanMs ||
        GetMSTimeDiffToNow(state.fireBotScanMs) >= ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS)
    {
        state.fireBotScanMs = getMSTime();

        std::list<Creature*> fireBots;
        bot->GetCreatureListWithEntryInGrid(fireBots, NPC_EMERGENCY_FIRE_BOT,
                                            ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);

        // Lowest guid is the oldest, so a new wave never displaces the pair already working.
        std::vector<ObjectGuid> alive;
        for (Creature* fireBot : fireBots)
            if (fireBot && fireBot->IsAlive())
                alive.push_back(fireBot->GetGUID());

        std::sort(alive.begin(), alive.end());
        if (alive.size() > ULDUAR_MIMIRON_FIREBOT_KEEP)
            alive.resize(ULDUAR_MIMIRON_FIREBOT_KEEP);

        state.keptFireBots = alive;
    }

    return state.keptFireBots;
}

bool IsMimironFireBotProtected(PlayerbotAI* botAI, Player* bot, Unit* fireBot)
{
    if (!fireBot || fireBot->GetEntry() != NPC_EMERGENCY_FIRE_BOT)
        return false;

    std::vector<ObjectGuid> const kept = GetMimironKeptFireBots(botAI, bot);
    return std::find(kept.begin(), kept.end(), fireBot->GetGUID()) != kept.end();
}

namespace
{
// Keep a formation anchor on the floor. Moves the anchor and never a single slot: clamping slots one
// at a time deforms the formation into a lopsided blob leaning at the boss, which hands Rapid Burst
// and the Bomb Bots exactly the clumps the spread exists to prevent.
//
// This used to also slide the anchor toward the focus until the outermost slot was inside casting
// range. That measured `extent` in every direction while the phase 3 wedge only occupies 120 degrees
// of it, so with spellDistance 28.5 and a two-row wedge the slide always landed within half a yard of
// the boss - and could overshoot past it, because the excess was never clamped to the distance. The
// wedge then tracked the Aerial Command Unit exactly while the unit held 30 yd from a bot inside that
// wedge, and raid and boss circled the room together.
Position ClampMimironAnchorToRoom(Position anchor)
{
    float const z = ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ();

    float const fromCentre =
        ULDUAR_MIMIRON_ROOM_CENTER.GetExactDist2d(anchor.GetPositionX(), anchor.GetPositionY());
    if (fromCentre > ULDUAR_MIMIRON_ROOM_RADIUS)
    {
        float const bearing =
            ULDUAR_MIMIRON_ROOM_CENTER.GetAngle(anchor.GetPositionX(), anchor.GetPositionY());
        anchor = Position(
            ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + ULDUAR_MIMIRON_ROOM_RADIUS * cos(bearing),
            ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + ULDUAR_MIMIRON_ROOM_RADIUS * sin(bearing), z);
    }

    return anchor;
}

// How many rows `count` bots need, capped at `maxRows`. Once the band is full the remaining rows just
// pack tighter: spacing is the thing to give up, not range, because a Bomb Bot catching two bots is
// cheaper than half the raid unable to reach the boss at all.
uint32 MimironWedgeRows(float firstRow, uint32 maxRows, uint32 count, float halfAngle, float spacing)
{
    uint32 held = 0;
    for (uint32 rows = 1; rows <= maxRows; ++rows)
    {
        float const radius = firstRow + (rows - 1) * spacing;
        held += static_cast<uint32>(2.0f * halfAngle * radius / spacing);
        if (held >= count)
            return rows;
    }

    return maxRows;
}

// Slot `index` of `count`, dealt row by row from the inside out and then spread edge to edge along
// whichever row it landed in.
void MimironWedgeSlot(float firstRow, uint32 rows, uint32 index, uint32 count, float halfAngle,
                      float spacing, float& outRadius, float& outOffset)
{
    uint32 const base = count / rows;
    uint32 const extra = count % rows;  // the first `extra` rows carry one more

    uint32 row = 0;
    uint32 filled = 0;
    for (; row + 1 < rows; ++row)
    {
        uint32 const size = base + (row < extra ? 1 : 0);
        if (index < filled + size)
            break;

        filled += size;
    }

    uint32 const size = base + (row < extra ? 1 : 0);
    uint32 const slot = index - filled;

    outRadius = firstRow + row * spacing;
    outOffset = size <= 1 ? 0.0f : -halfAngle + 2.0f * halfAngle * slot / (size - 1);
}

// The one move that brings every slot of a wedge inside casting range of `focus`, measured in 3D the
// way IsWithinCombatRange does. Rigid, and computed over all the slots, so every bot derives the same
// move and the wedge keeps its shape. A player's reach rather than the asking bot's own, so a gnome
// and a tauren get the same wedge. Only the floor distance can be walked off, so a unit hovering
// overhead leaves less of it.
void MimironShiftIntoRange(std::vector<Position> const& slots, Unit* focus, float& shiftX, float& shiftY)
{
    shiftX = 0.0f;
    shiftY = 0.0f;
    if (!focus)
        return;

    float const reach = std::max(sPlayerbotAIConfig.spellDistance + focus->GetCombatReach() +
                                     DEFAULT_COMBAT_REACH - ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN,
                                 1.0f);

    // One pass puts the farthest slot on the limit, but a rigid move can leave a second one just past
    // it at a different bearing, so settle it a couple more times.
    for (uint32 pass = 0; pass < 3; ++pass)
    {
        float worst = 0.0f;
        float worstX = 0.0f;
        float worstY = 0.0f;
        for (Position const& slot : slots)
        {
            float const x = slot.GetPositionX() + shiftX;
            float const y = slot.GetPositionY() + shiftY;
            float const dz = focus->GetPositionZ() - slot.GetPositionZ();
            float const floor = std::sqrt(std::max(reach * reach - dz * dz, 0.0f));
            float const excess = focus->GetExactDist2d(x, y) - floor;
            if (excess > worst)
            {
                worst = excess;
                worstX = x;
                worstY = y;
            }
        }

        if (worst <= 0.0f)
            break;

        float const bearing = std::atan2(focus->GetPositionY() - worstY, focus->GetPositionX() - worstX);
        shiftX += worst * std::cos(bearing);
        shiftY += worst * std::sin(bearing);
    }
}

// Every slot of the Firefighter phase 1 camp: a wedge on the tank spot, centreline through the live
// stack anchor, turned `turn` radians toward the room. Fixed on purpose - chains grow toward whoever is
// nearest their head, so a camp that tracked the boss would smear the field along behind it. It gives
// ground only to stay in casting range, since "reach spell" is ACTION_HIGH against this formation at
// ACTION_RAID and a slot past range deadlocks instead of correcting itself. That is also what a dead
// main tank looks like.
//
// Range the way the bots test it: IsWithinCombatRange adds both combat reaches, so against the MK II
// the limit is about 34 raw. The raw spellDistance less margin would be 24.5, which is Napalm's floor
// to within half a yard.
std::vector<Position> MimironPhase1CampSlots(Position const& anchor, Unit* focus, uint32 count, float turn)
{
    Position const& hub = ULDUAR_MIMIRON_PHASE1_TANK_SPOT;
    float centreline = hub.GetAngle(anchor.GetPositionX(), anchor.GetPositionY());

    // Never past the room's own bearing, or the two anchors cross over.
    float toRoom = Position::NormalizeOrientation(
        hub.GetAngle(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX(), ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY()) -
        centreline);
    if (toRoom > static_cast<float>(M_PI))
        toRoom -= 2.0f * static_cast<float>(M_PI);
    centreline += (toRoom >= 0.0f ? 1.0f : -1.0f) * std::min(turn, std::fabs(toRoom));

    uint32 const rows =
        MimironWedgeRows(ULDUAR_MIMIRON_PHASE1_CAMP_FIRST_ROW, ULDUAR_MIMIRON_PHASE1_CAMP_ROWS, count,
                         ULDUAR_MIMIRON_PHASE1_CAMP_HALF_ANGLE, ULDUAR_MIMIRON_PHASE1_CAMP_SPACING);

    // Every slot, not just this one: the shift is set by whichever is farthest out, and every bot has
    // to derive the same shift or the wedge deforms into the lopsided blob the slots exist to prevent.
    std::vector<Position> slots;
    slots.reserve(count);
    for (uint32 i = 0; i < count; ++i)
    {
        float radius = 0.0f;
        float offset = 0.0f;
        MimironWedgeSlot(ULDUAR_MIMIRON_PHASE1_CAMP_FIRST_ROW, rows, i, count,
                         ULDUAR_MIMIRON_PHASE1_CAMP_HALF_ANGLE, ULDUAR_MIMIRON_PHASE1_CAMP_SPACING,
                         radius, offset);

        float const bearing = Position::NormalizeOrientation(centreline + offset);
        slots.emplace_back(hub.GetPositionX() + radius * std::cos(bearing),
                           hub.GetPositionY() + radius * std::sin(bearing), hub.GetPositionZ());
    }

    float shiftX = 0.0f;
    float shiftY = 0.0f;
    MimironShiftIntoRange(slots, focus, shiftX, shiftY);

    for (Position& slot : slots)
        slot.Relocate(slot.GetPositionX() + shiftX, slot.GetPositionY() + shiftY, slot.GetPositionZ());

    return slots;
}

// Whether a player standing on `slot` sees `focus`. Same ray IsWithinLOSInMap casts from a player: its
// eye to the creature's hit sphere point. Out of sight is an invalid target and the bot drops it.
bool MimironSlotSeesFocus(Position const& slot, Unit* focus)
{
    Position const eye(slot.GetPositionX(), slot.GetPositionY(),
                       slot.GetPositionZ() + ULDUAR_MIMIRON_CAMP_SIGHT_EYE);
    Position const hit = focus->GetHitSpherePointFor(eye);
    return focus->GetMap()->isInLineOfSight(eye.GetPositionX(), eye.GetPositionY(), eye.GetPositionZ(),
                                            hit.GetPositionX(), hit.GetPositionY(), hit.GetPositionZ(),
                                            focus->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS,
                                            VMAP::ModelIgnoreFlags::Nothing);
}

// The smallest turn at which every camp slot sees the MK II. Raid-wide and folded on the observability
// interval: every bot has to build the same camp, and each step costs a ray per slot. Goes up at once,
// comes down only after ULDUAR_MIMIRON_CAMP_SIGHT_HOLD_MS, and holds when no step clears.
float MimironPhase1CampTurn(Player* bot, Position const& anchor, Unit* focus, uint32 count)
{
    MimironFightState& state = MimironFightStateFor(bot);
    if (!focus || (state.campTurnScanMs &&
                   GetMSTimeDiffToNow(state.campTurnScanMs) < ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS))
        return state.campTurn;

    state.campTurnScanMs = getMSTime();

    float wanted = -1.0f;
    for (float turn = 0.0f; turn <= ULDUAR_MIMIRON_CAMP_SIGHT_MAX + 0.001f;
         turn += ULDUAR_MIMIRON_CAMP_SIGHT_STEP)
    {
        std::vector<Position> const slots = MimironPhase1CampSlots(anchor, focus, count, turn);
        if (std::all_of(slots.begin(), slots.end(),
                        [focus](Position const& slot) { return MimironSlotSeesFocus(slot, focus); }))
        {
            wanted = turn;
            break;
        }
    }

    if (wanted < 0.0f)
        return state.campTurn;

    float const before = state.campTurn;
    if (wanted > state.campTurn)
    {
        state.campTurn = wanted;
        state.campTurnRaisedMs = getMSTime();
    }
    else if (wanted < state.campTurn &&
             GetMSTimeDiffToNow(state.campTurnRaisedMs) >= ULDUAR_MIMIRON_CAMP_SIGHT_HOLD_MS)
    {
        state.campTurn = wanted;
    }

    if (state.campTurn != before && RaidObs::Active())
    {
        char line[16];
        snprintf(line, sizeof(line), "%.0f", state.campTurn * 180.0f / static_cast<float>(M_PI));
        RaidObs::NoteDerived(bot, "mimiron.campturn", line);
    }

    return state.campTurn;
}

Position MimironPhase1CampSlot(Player* bot, Position const& anchor, Unit* focus, uint32 index,
                               uint32 count)
{
    std::vector<Position> const slots =
        MimironPhase1CampSlots(anchor, focus, count, MimironPhase1CampTurn(bot, anchor, focus, count));
    return slots[std::min(index, count - 1)];
}

// Phase 3. The raid groups in the east wedge instead of ringing the room: the summon pads sit on three
// arms - west, north-east and south-east, each carrying pads at roughly 17, 29 and 40 yd - so a ring
// drops lone ranged bots straight into an add's path.
bool GetMimironPhase3Slot(Player* bot, Group* group, Unit* focus, Position& out, uint32& index,
                          uint32& count)
{
    // Melee stand on whatever they are hitting. Every add walks in from a pad well outside the wedge,
    // so any fixed melee slot is a spot the target is not in - and this formation runs at ACTION_RAID,
    // above the chase at ACTION_HIGH, so it wins the tick and the bot never lands a swing.
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    index = 0;
    count = 0;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsRanged(member))
            continue;

        if (member == bot)
            index = count;

        ++count;
    }

    if (count == 0)
        return false;

    // The band stops at casting range, so the wedge is built to fit rather than grown until it does.
    // Past that the rows pack tighter instead: a Bomb Bot catching two bots is cheaper than half the
    // raid unable to reach the boss.
    float const rangedDepth = std::max(sPlayerbotAIConfig.spellDistance -
                                           ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN -
                                           ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
                                       0.0f);
    uint32 const rows = MimironWedgeRows(
        ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
        1u + static_cast<uint32>(rangedDepth / ULDUAR_MIMIRON_PHASE3_SPACING), count,
        ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE, ULDUAR_MIMIRON_PHASE3_SPACING);

    // Anchored on the room centre: holding still leaves a Bomb Bot spawning on the unit roughly 30 yd
    // of open floor to cross at 8.0 yd/s. The centreline is the bearing to the staging point, the
    // middle of the gap between the two east arms and the one direction nothing walks in from.
    Position const& anchor = ULDUAR_MIMIRON_ROOM_CENTER;
    float const centreline = ULDUAR_MIMIRON_ROOM_CENTER.GetAngle(
        ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionX(), ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionY());

    std::vector<Position> slots;
    slots.reserve(count);
    for (uint32 i = 0; i < count; ++i)
    {
        float radius = 0.0f;
        float offset = 0.0f;
        MimironWedgeSlot(ULDUAR_MIMIRON_PHASE3_MIN_RADIUS, rows, i, count,
                         ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE, ULDUAR_MIMIRON_PHASE3_SPACING, radius,
                         offset);

        float const bearing = Position::NormalizeOrientation(centreline + offset);
        slots.emplace_back(anchor.GetPositionX() + radius * std::cos(bearing),
                           anchor.GetPositionY() + radius * std::sin(bearing),
                           ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    }

    // Then only as far toward the unit as casting range needs. Ranged have to hit it: it hovers about
    // 16 yd up and holds 30 yd from whoever it is on, so a slot that looks close on the floor can be
    // well past range. Never a slide onto it: tracking the unit exactly walked raid and boss round the
    // room together whenever its victim was one of these bots.
    float shiftX = 0.0f;
    float shiftY = 0.0f;
    MimironShiftIntoRange(slots, focus, shiftX, shiftY);

    Position const& mine = slots[std::min(index, count - 1)];
    out = Position(mine.GetPositionX() + shiftX, mine.GetPositionY() + shiftY, mine.GetPositionZ());
    return true;
}

// `branch` names which shape answered, and is what a trace records: the coordinate on its own cannot
// tell a wedge slot from a ring slot that happens to land near it, and which shape a bot was given is
// the thing that goes wrong.
bool DeriveMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out, char const*& branch,
                             uint32& index, uint32& count)
{
    branch = "none";
    index = 0;
    count = 0;

    if (!botAI || !bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Nothing attackable means a phase handover, which runs anywhere from 24 to 48 seconds. The mechs
    // are all NOT_SELECTABLE for the whole of it, so without this the raid falls through to follow and
    // walks into the next phase from wherever its master happened to be standing.
    Unit* focus = GetMimironRingFocus(botAI);
    bool const staging = focus == nullptr;
    if (staging)
        focus = GetMimironStagingFocus(bot);

    if (!focus)
        return false;

    // The main tank holds the chassis spot once all three mechs are up. Walking it anywhere else in
    // phase 4 drags VX-001 with it, and VX-001 is what the Laser Barrage cone radiates from.
    bool const phase4 = staging ? focus->GetEntry() == NPC_VX001 && focus->GetVehicleBase() != nullptr
                                : GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
                                      GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
                                      GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);

    if (PlayerbotAI::IsMainTank(bot) && phase4)
    {
        branch = "p4tank";
        out = ULDUAR_MIMIRON_PHASE4_TANK_SPOT;
        return true;
    }

    // Nobody else is placed during a handover. Refusing hands the tick to follow at relevance 1.0 and
    // the raid walks in behind its master, which is where the raid leader wants it. Measured over two
    // handovers the masters stood 43 to 54 yd off the room centre, which is also where the fire wants
    // taking: chains grow 1.22 yd/s and cannot catch a pack that is walking. Behind p4tank on purpose
    // - that one holds the chassis still and is a boss-holding spot rather than somewhere to wait.
    if (staging)
        return false;

    // Phase 3 tank spot, and the reason is the Magnetic Core rather than the tanking. The Aerial
    // Command Unit chases whoever holds it to within 30 yd, and the core summons underneath the unit
    // rather than under the player who places it, so where the tank stands decides where the raid
    // spends the next 20 s. Left to chase, tank and unit converge wherever the last Bomb Bot sidestep
    // happened to leave them: one kill grounded it 16.8 yd off centre and put 7 to 12 of 25 past
    // casting range for both windows.
    if (PlayerbotAI::IsMainTank(bot) && focus->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
    {
        branch = "p3tank";
        out = ULDUAR_MIMIRON_ROOM_CENTER;
        return true;
    }

    if (focus->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
    {
        branch = "p3wedge";
        return GetMimironPhase3Slot(bot, group, focus, out, index, count);
    }

    // Phase 1 tank spot. Nothing else brings the MK II back: the tank is melee, so it flees Shock
    // Blast every 30 s and the boss follows, and over a five minute phase that walks the fight round
    // the room until half the raid is out of casting range.
    //
    // Under Firefighter it is held 53 yd west rather than on the room centre. The fire is seeded on
    // players and not on the boss, and the raid stands on its tank, so this is what keeps every batch
    // off the ground VX-001 is summoned onto. Normal mode has no fire and keeps the centre: a 53 yd
    // drag buys nothing there.
    bool const phase1 = !phase4 && focus->GetEntry() == NPC_LEVIATHAN_MKII;
    bool const firefighter = IsMimironHardModeActive(botAI);

    if (PlayerbotAI::IsMainTank(bot) && phase1)
    {
        // No anchor until the boss is actually his, so "reach melee" owns him and he stands on it
        // building threat. Only under Firefighter: normal mode anchors on the room centre, which is
        // where the MK II already is, so there is no drag to lose the boss halfway through.
        if (firefighter && !IsMimironTankDragReady(botAI, bot))
            return false;

        branch = "p1tank";
        out = firefighter ? ULDUAR_MIMIRON_PHASE1_TANK_SPOT : ULDUAR_MIMIRON_ROOM_CENTER;
        return true;
    }

    // Melee stand on whatever they are hitting, so only ranged and healers get a slot from here on.
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsRanged(member) ||
            PlayerbotAI::IsMainTank(member))
            continue;

        if (member == bot)
            index = count;

        ++count;
    }

    if (count == 0)
        return false;

    // Ranged and healers hold one camp rather than the ring, so every chain grows toward the same
    // place instead of being dragged out along every radius the raid occupies. Which anchor it points
    // at is raid-wide and moves off the fire.
    if (phase1 && firefighter)
    {
        branch = "p1stack";
        out = MimironPhase1CampSlot(bot, GetMimironPhase1StackAnchor(botAI, bot), focus, index, count);
        return true;
    }

    // Centred on the mech: both ground mechs get dragged about by their tanks, and a ring pinned to
    // the room centre puts the far half of the raid past casting range after only six yards of drift
    // - which then deadlocks rather than self-correcting, because "reach spell" is ACTION_HIGH and
    // this ring is ACTION_RAID.
    Position const anchor = ClampMimironAnchorToRoom(Position(
        focus->GetPositionX(), focus->GetPositionY(), ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ()));

    // Firefighter, mid-phase: a wedge rather than a ring, because the fire chases the raid. Every
    // chain grows toward whoever is nearest its head, so 25 bots on 25 bearings drag 25 chains out
    // along 25 radii and the fire ends up everywhere. Grouped into one sector the chains converge
    // instead, and the Frost Bomb - which only ever summons on a burning node - lands in that sector
    // and clears it. The cost is Rapid Burst: a 104 degree cone covers most of a 120 degree wedge,
    // where the full ring was chosen so it could not. Fire was outdamaging Rapid Burst five to eight
    // times over.
    if (firefighter)
    {
        branch = "hmwedge";

        float const rangedDepth = std::max(sPlayerbotAIConfig.spellDistance -
                                               ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN -
                                               ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
                                           0.0f);
        uint32 const rows = MimironWedgeRows(
            ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
            1u + static_cast<uint32>(rangedDepth / ULDUAR_MIMIRON_PHASE3_SPACING), count,
            ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE, ULDUAR_MIMIRON_PHASE3_SPACING);

        float radius = 0.0f;
        float offset = 0.0f;
        MimironWedgeSlot(ULDUAR_MIMIRON_PHASE3_MIN_RADIUS, rows, index, count,
                         ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE, ULDUAR_MIMIRON_PHASE3_SPACING, radius,
                         offset);

        // Bearing off the room centre, not off the anchor: the sector has to stay put in the room for
        // the fire to pile up in it, and it is the same east gap phase 3 already forms up in.
        float const centreline = ULDUAR_MIMIRON_ROOM_CENTER.GetAngle(
            ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionX(), ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionY());
        float const bearing = Position::NormalizeOrientation(centreline + offset);

        out = Position(anchor.GetPositionX() + radius * cos(bearing),
                       anchor.GetPositionY() + radius * sin(bearing),
                       ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
        return true;
    }

    branch = "ring";

    float const angle = 2.0f * static_cast<float>(M_PI) * index / count;
    out = Position(anchor.GetPositionX() + ULDUAR_MIMIRON_SPREAD_RADIUS * cos(angle),
                   anchor.GetPositionY() + ULDUAR_MIMIRON_SPREAD_RADIUS * sin(angle),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    return true;
}
}  // namespace

bool GetMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out)
{
    char const* branch = "none";
    uint32 index = 0;
    uint32 count = 0;
    bool const found = DeriveMimironSpreadSlot(botAI, bot, out, branch, index, count);

    if (RaidObs::Active())
    {
        // "none" covers both a bot the formation has nothing for - melee outside a handover, by
        // design - and one whose shape refused it. Without the row those two are the same silence,
        // and the second is a bug.
        std::string value = "none";
        if (found)
        {
            value = branch;
            if (count)
                value += " " + std::to_string(index) + "/" + std::to_string(count);

            value += " " + RaidObs::DescribeDerived(out);
        }

        RaidObs::NoteDerived(bot, "mimiron.slot", value);
    }

    return found;
}
