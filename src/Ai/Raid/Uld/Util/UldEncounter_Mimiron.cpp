/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Mimiron.h"

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "ServerFacade.h"
#include "Timer.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

using namespace EncounterHelpers;

const Position ULDUAR_MIMIRON_ROOM_CENTER = Position(2744.65f, 2569.46f, 364.32f);
const Position ULDUAR_MIMIRON_PHASE3_STAGE = Position(2762.65f, 2569.46f, 364.31f);
const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT = Position(2744.5754f, 2570.8657f, 364.3138f);

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

bool IsMimironSpotSafe(Player* bot, Position const& dest)
{
    if (!bot)
        return true;

    if (!IsMimironSpotMineSafe(bot, dest))
        return false;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return true;

    // Firefighter spreads ground fire across the floor, so a standing spot can end up inside it. Without
    // this the flames node at ACTION_RAID + 4 pushes the bot out and the formation at ACTION_RAID pulls
    // it straight back, and it paces on the edge until it burns down.
    bool const hardMode = IsMimironHardModeActive(botAI);

    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        uint32 const entry = unit->GetEntry();
        float clearance = 0.0f;

        if (entry == NPC_ROCKET_STRIKE_N)
            clearance = ULDUAR_MIMIRON_ROCKET_CLEARANCE;
        else if (hardMode && (entry == NPC_FLAMES_SPREAD || entry == NPC_FLAMES_INITIAL))
            clearance = ULDUAR_MIMIRON_FLAMES_RADIUS;
        else
            continue;

        if (dest.GetExactDist2d(unit->GetPositionX(), unit->GetPositionY()) < clearance)
            return false;
    }

    return true;
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

bool IsMimironTankAnchorSlot(PlayerbotAI* botAI, Player* bot)
{
    // Phases 1 and 4 both park the MK II, and both are the phases it lays mines in.
    return botAI && bot && PlayerbotAI::IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) != nullptr;
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
uint32 MimironWedgeRows(float firstRow, uint32 maxRows, uint32 count)
{
    uint32 held = 0;
    for (uint32 rows = 1; rows <= maxRows; ++rows)
    {
        float const radius = firstRow + (rows - 1) * ULDUAR_MIMIRON_PHASE3_SPACING;
        held += static_cast<uint32>(2.0f * ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE * radius /
                                    ULDUAR_MIMIRON_PHASE3_SPACING);
        if (held >= count)
            return rows;
    }

    return maxRows;
}

// Slot `index` of `count`, dealt row by row from the inside out and then spread edge to edge along
// whichever row it landed in.
void MimironWedgeSlot(float firstRow, uint32 rows, uint32 index, uint32 count, float& outRadius,
                      float& outOffset)
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

    outRadius = firstRow + row * ULDUAR_MIMIRON_PHASE3_SPACING;
    outOffset = size <= 1 ? 0.0f
                          : -ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE +
                                2.0f * ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE * slot / (size - 1);
}

// Where melee and tanks wait out a handover: a small ring on the room centre, which is where all three
// handovers converge - VX-001 is summoned there, the Aerial Command Unit spawns and is walked back
// there, and the chassis ends there. Never on the focus itself: it is mid-script for most of the
// window, so a ring pinned to it drags the raid along the chassis charge waypoints.
bool GetMimironStagingMeleeSlot(Player* bot, Group* group, Unit* focus, Position& out, uint32& index,
                                uint32& count)
{
    index = 0;
    count = 0;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        // Main tanks are counted here too. Only a phase 4 main tank has a spot of its own, and that
        // one is handed out before this is ever reached.
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || PlayerbotAI::IsRanged(member))
            continue;

        if (member == bot)
            index = count;

        ++count;
    }

    if (count == 0)
        return false;

    // Outside the mech's own model. The chassis has the largest reach of the three at 8, so a flat
    // 8 yd ring would stage half the melee inside it.
    float const radius = focus ? std::max(ULDUAR_MIMIRON_STAGING_MELEE_RADIUS,
                                          focus->GetCombatReach() + 1.0f)
                              : ULDUAR_MIMIRON_STAGING_MELEE_RADIUS;

    float const bearing = 2.0f * static_cast<float>(M_PI) * index / count;
    out = Position(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + radius * std::cos(bearing),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + radius * std::sin(bearing),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    return true;
}

// Phase 3. The raid groups in the east wedge instead of ringing the room: the summon pads sit on three
// arms - west, north-east and south-east, each carrying pads at roughly 17, 29 and 40 yd - so a ring
// drops lone ranged bots straight into an add's path.
bool GetMimironPhase3Slot(Player* bot, Group* group, Position& out, uint32& index, uint32& count)
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
        1u + static_cast<uint32>(rangedDepth / ULDUAR_MIMIRON_PHASE3_SPACING), count);

    float radius = 0.0f;
    float offset = 0.0f;
    MimironWedgeSlot(ULDUAR_MIMIRON_PHASE3_MIN_RADIUS, rows, index, count, radius, offset);

    // The room centre, and nothing else. The Aerial Command Unit has no attack in this phase - its
    // whole event list is add summons - so there is nothing range on it buys, and holding still is
    // what leaves a Bomb Bot spawning on it roughly 30 yd of open floor to cross at 8.0 yd/s.
    Position const& anchor = ULDUAR_MIMIRON_ROOM_CENTER;

    // The centreline is the bearing to the staging point: the middle of the gap between the two east
    // arms, and the one direction nothing walks in from.
    float const centreline = ULDUAR_MIMIRON_ROOM_CENTER.GetAngle(
        ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionX(), ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionY());
    float const bearing = Position::NormalizeOrientation(centreline + offset);

    out = Position(anchor.GetPositionX() + radius * cos(bearing),
                   anchor.GetPositionY() + radius * sin(bearing),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    return true;
}

// `branch` names which of the five shapes answered, and is what a trace records: the coordinate on
// its own cannot tell a wedge slot from a staging ring slot that happens to land near it, and which
// shape a bot was given is the thing that goes wrong.
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

    // Melee only get a spot while staging, where there is no chase for it to fight and arriving in
    // melee range before the boss goes live is the whole point.
    if (staging && !PlayerbotAI::IsRanged(bot))
    {
        branch = "stagemelee";
        return GetMimironStagingMeleeSlot(bot, group, focus, out, index, count);
    }

    // Phase 3 tank spot, and the reason is the Magnetic Core rather than the tanking. The Aerial
    // Command Unit hovers directly over whoever holds it and the core summons underneath the unit
    // rather than under the player who places it, so wherever the tank stands when a core lands is
    // where the raid spends the next 20 s. Left to chase, tank and unit converge wherever the last
    // Bomb Bot sidestep happened to leave them: one kill grounded it 16.8 yd off centre and put 7 to
    // 12 of 25 past casting range for both windows.
    if (PlayerbotAI::IsMainTank(bot) && focus->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
    {
        branch = "p3tank";
        out = ULDUAR_MIMIRON_ROOM_CENTER;
        return true;
    }

    if (focus->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
    {
        branch = "p3wedge";
        return GetMimironPhase3Slot(bot, group, out, index, count);
    }

    // Phase 1 tank spot. Nothing else brings the MK II back: the tank is melee, so it flees Shock
    // Blast every 30 s and the boss follows, and over a five minute phase that walks the fight round
    // the room until half the raid is out of casting range. This is the point the encounter script
    // itself charges the MK II to.
    if (PlayerbotAI::IsMainTank(bot) && focus->GetEntry() == NPC_LEVIATHAN_MKII)
    {
        branch = "p1tank";
        out = ULDUAR_MIMIRON_ROOM_CENTER;
        return true;
    }

    // Melee stand on whatever they are hitting, so only ranged and healers get a ring slot.
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

    // Centred on the mech while a phase is live: both ground mechs get dragged about by their tanks,
    // and a ring pinned to the room centre puts the far half of the raid past casting range after only
    // six yards of drift - which then deadlocks rather than self-correcting, because "reach spell" is
    // ACTION_HIGH and this ring is ACTION_RAID. During a handover it is the room centre instead, for
    // the same reason the melee staging ring is: the focus is mid-script and walking.
    Position const anchor = ClampMimironAnchorToRoom(
        staging ? ULDUAR_MIMIRON_ROOM_CENTER
                : Position(focus->GetPositionX(), focus->GetPositionY(),
                           ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ()));

    branch = staging ? "stagering" : "ring";

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
