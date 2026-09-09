#include "UldActions_Mimiron.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <limits>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Position.h"
#include "UldData.h"
#include "UldEncounter_Mimiron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

bool MimironFleeAction::MoveAwayClearOfMines(Unit* from, float distance, MovementPriority priority,
                                             bool fallbackUnfiltered, bool interrupt, char const* what)
{
    if (!from)
        return false;

    return FleeFan(from->GetPosition(), from, distance, priority, fallbackUnfiltered, interrupt, what);
}

bool MimironFleeAction::MoveAwayClearOfMines(Position const& from, float distance,
                                             MovementPriority priority, bool fallbackUnfiltered,
                                             bool interrupt, char const* what)
{
    return FleeFan(from, nullptr, distance, priority, fallbackUnfiltered, interrupt, what);
}

bool MimironFleeAction::MoveTowardClearOfMines(Position const& dest, MovementPriority priority,
                                               bool fallbackUnfiltered, bool interrupt,
                                               char const* what)
{
    float const distance = bot->GetExactDist2d(dest.GetPositionX(), dest.GetPositionY());
    if (distance <= 0.0f)
        return false;

    // Mirror the destination through the bot. Fleeing that point walks straight at `dest` on the
    // fan's first bearing, and every swept alternative still ends further from it than the bot
    // started, so the back-tracking filter passes them all.
    Position const mirror(2.0f * bot->GetPositionX() - dest.GetPositionX(),
                          2.0f * bot->GetPositionY() - dest.GetPositionY(), bot->GetPositionZ());

    return FleeFan(mirror, nullptr, distance, priority, fallbackUnfiltered, interrupt, what);
}

bool MimironFleeAction::FleeFan(Position const& from, Unit* fallbackFrom, float distance,
                                MovementPriority priority, bool fallbackUnfiltered, bool interrupt,
                                char const* what)
{
    if (distance <= 0.0f)
        return false;

    // Why the fan emptied, for the trace. A bearing that is never issued reaches no MotionMaster and
    // so writes no move record, which makes a flee that refuses everything indistinguishable from one
    // that was never asked - and that is exactly the case that kills a bot.
    uint32 refusedBack = 0;
    uint32 refusedMine = 0;
    uint32 refusedCone = 0;
    uint32 refusedFire = 0;
    uint32 refusedBomb = 0;
    uint32 refusedBurst = 0;

    // A bot with a cast in flight cannot be moved at all: PointMovementGenerator discards the spline
    // outright for anything IsMovementPreventedByCasting, and MoveTo still reports success and stamps
    // a LastMovement delay, so it also blocks its own retries for a leg it never walked. Only the
    // hazards that kill do this - the mine dodge would rather keep its cast than avoid 9000 damage.
    if (interrupt)
        bot->CastStop();

    float const speed = bot->GetSpeed(MOVE_RUN);
    float const travel = speed > 0.0f ? distance / speed : 0.0f;
    float const away = from.GetAngle(bot->GetPositionX(), bot->GetPositionY());
    float const started = bot->GetExactDist2d(from.GetPositionX(), from.GetPositionY());

    // Resolved once for the whole fan: reading the window costs a grid scan for the DB Target, and
    // gathering the Firefighter hazards walks a fire field that runs to 50 or 60 nodes.
    Unit* const vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001);
    MimironBarrageWindow const barrage =
        vx001 ? GetMimironBarrageWindow(bot, vx001) : MimironBarrageWindow();
    MimironRapidBurstWindow const burst =
        vx001 ? GetMimironRapidBurstWindow(botAI, bot, vx001) : MimironRapidBurstWindow();
    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);

    // Past about 120 degrees off the escape bearing the geometry turns back inward, so the fan stops
    // short of that. It was 90; two stacked filters can empty the first quadrant, and the alternative
    // to a wider fan is standing still in a 5000000 damage blast.
    for (float delta = 0.0f; delta <= 5.0f * static_cast<float>(M_PI) / 8.0f + 0.001f;
         delta += static_cast<float>(M_PI) / 8.0f)
    {
        for (float sign : {1.0f, -1.0f})
        {
            if (delta == 0.0f && sign < 0.0f)
                continue;

            float const angle = away + sign * delta;
            float dx = bot->GetPositionX() + cos(angle) * distance;
            float dy = bot->GetPositionY() + sin(angle) * distance;
            float dz = bot->GetPositionZ();
            bool exact = true;
            if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(),
                                                               bot->GetPositionY(),
                                                               bot->GetPositionZ(), dx, dy, dz))
            {
                dx = bot->GetPositionX() + cos(angle) * distance;
                dy = bot->GetPositionY() + sin(angle) * distance;
                dz = bot->GetPositionZ();
                exact = false;
            }

            Position const dest(dx, dy, dz);

            // Collision can shorten the step enough to leave the bot no better off than it started.
            if (dest.GetExactDist2d(from.GetPositionX(), from.GetPositionY()) <= started)
            {
                ++refusedBack;
                continue;
            }

            if (!IsMimironSpotMineSafe(bot, dest))
            {
                ++refusedMine;
                continue;
            }

            // Judged at arrival, not at issue. This leg holds the movement lock for its whole
            // duration and IsWaitingForLastMove refuses anything not strictly above it, so a spot
            // that is only clear right now strands the bot in the beams until the leg expires.
            if (!IsMimironSpotBarrageSafe(vx001, barrage, dest, travel))
            {
                ++refusedCone;
                continue;
            }

            // Firefighter only, and empty otherwise. Without these a Shock Blast or barrage dodge
            // lands the bot in the fire it is about to have to leave again, and a fire dodge steps
            // out of one node straight into the next - the nodes are 7 yd apart and the hops were 4.
            if (!IsMimironSpotFireSafe(hazards, dest))
            {
                ++refusedFire;
                continue;
            }

            if (!IsMimironSpotBombSafe(hazards, dest))
            {
                ++refusedBomb;
                continue;
            }

            // The Rapid Burst cone is only 60 degrees wide but it is 100 yd deep, so any dodge can
            // hand a bot straight into it - and the step that exists to leave it would otherwise be
            // free to sweep a bearing right back in.
            if (!IsMimironSpotRapidBurstSafe(vx001, burst, dest))
            {
                ++refusedBurst;
                continue;
            }

            if (MoveTo(bot->GetMapId(), dx, dy, dz, false, false, true, exact, priority))
            {
                float const taken = sign * delta;
                NoteFleeOutcome(what, "ok", &taken, refusedBack, refusedMine, refusedCone, refusedFire,
                                refusedBomb, refusedBurst);
                return true;
            }
        }
    }

    // Every bearing in the fan was refused - by a mine, the barrage, the fire, the bomb, the Rapid
    // Burst cone, or collision leaving the bot no further from the hazard than it started.
    if (!fallbackUnfiltered)
    {
        NoteFleeOutcome(what, "none", nullptr, refusedBack, refusedMine, refusedCone, refusedFire,
                        refusedBomb, refusedBurst);
        return false;
    }

    NoteFleeOutcome(what, "fallback", nullptr, refusedBack, refusedMine, refusedCone, refusedFire,
                    refusedBomb, refusedBurst);

    if (fallbackFrom)
        return MoveAway(fallbackFrom, distance);

    return MoveTo(bot->GetMapId(), bot->GetPositionX() + cos(away) * distance,
                  bot->GetPositionY() + sin(away) * distance, bot->GetPositionZ(), false, false, true,
                  false, priority);
}

// `taken` is the bearing that won, in radians off straight away from the hazard, and is null for the
// two outcomes where no bearing won at all.
void MimironFleeAction::NoteFleeOutcome(char const* what, char const* outcome, float const* taken,
                                        uint32 refusedBack, uint32 refusedMine, uint32 refusedCone,
                                        uint32 refusedFire, uint32 refusedBomb, uint32 refusedBurst)
{
    if (!RaidObs::Active())
        return;

    char bearing[16] = "";
    if (taken)
        snprintf(bearing, sizeof(bearing), " %+.0f", *taken * 180.0f / static_cast<float>(M_PI));

    char line[144];
    snprintf(line, sizeof(line), "%s %s%s (back%u mine%u cone%u fire%u bomb%u burst%u)", what, outcome,
             bearing, refusedBack, refusedMine, refusedCone, refusedFire, refusedBomb, refusedBurst);

    RaidObs::NoteDerived(bot, "mimiron.flee", line);
}

bool MimironShockBlastAction::Execute(Event /*event*/)
{
    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (!leviathanMkII)
        return false;

    // Phases 3 and 4 used to teleport out of this instead of running. Running works in every phase;
    // what it needed was the node priority to win the tick. MOVEMENT_FORCED because the arc spread
    // issues at MOVEMENT_COMBAT and IsWaitingForLastMove refuses anything not strictly above the
    // move already in flight - a flee starting mid-walk would otherwise be dropped silently.
    float const gap = ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST - bot->GetExactDist2d(leviathanMkII);
    if (gap > 0.0f)
    {
        MoveAwayClearOfMines(leviathanMkII, gap, MovementPriority::MOVEMENT_FORCED, true, true,
                             "shock");

        if (botAI->IsMelee(bot))
            botAI->SetNextCheckDelay(100);

        // Still inside the circle, so nothing else gets this tick - not even while a leg is in flight,
        // where the lock refuses a second move and every bearing comes back false. Yielding there is
        // what let Charge run at relevance 40 and put the warrior back under the cast.
        return true;
    }

    // Clear. Melee keep holding the tick because they are the only ones carrying a gap-closer, and at
    // 18 yd they have nothing else to do anyway. Ranged and healers go back to work rather than lose
    // 4 s in every 30 - a resto druid frozen through the cast costs more than the cast does.
    return botAI->IsMelee(bot);
}

bool MimironShockBlastAction::isUseful()
{
    MimironShockBlastTrigger mimironShockBlastTrigger(botAI);
    return mimironShockBlastTrigger.IsActive();
}

bool MimironResetEncounterStateAction::Execute(Event /*event*/)
{
    RESET_AI_VALUE(float, "disperse distance");

    // Never claims the tick: clearing state is bookkeeping, and whatever the bot is standing in still
    // needs the nodes below this one to run.
    return false;
}

bool MimironPhase1PositioningAction::Execute(Event /*event*/)
{
    SET_AI_VALUE(float, "disperse distance", ULDUAR_MIMIRON_DISPERSE_DISTANCE);

    // Never claims the tick. Writing the value is all this does, and the engine stops a pass at the
    // first action returning true - at ACTION_RAID that would be every cast and heal the bot owns.
    return false;
}

bool MimironPhase1PositioningAction::isUseful()
{
    MimironPhase1PositioningTrigger mimironPhase1PositioningTrigger(botAI);
    return mimironPhase1PositioningTrigger.IsActive();
}

// Which rule the dodge applied, not where it went: the destination is already a move record naming
// this action. What that record cannot say is whether the bot thought it was clear, ahead of the
// beams, inside them, or being carried out by the sweep - and picking the wrong one of those is what
// leaves a body on the floor. `cw` is where the bot stands clockwise of the cone centreline, bucketed
// to 15 degrees: at whole degrees it changes every tick, so NoteDerived's change test suppressed
// nothing and one kill wrote 2300 of these. The orbit radius is left out for the same reason and
// because snap.u samples the bot's position four times a second beside the hazard row's apex.
void MimironP3Wx2LaserBarrageAction::NoteBarrageDecision(char const* branch, char const* direction,
                                                        float cw)
{
    if (!RaidObs::Active())
        return;

    int const bucket = static_cast<int>(cw * 180.0f / static_cast<float>(M_PI) / 15.0f) * 15;

    char line[64];
    snprintf(line, sizeof(line), "%s%s%s %d", branch, direction ? " " : "",
             direction ? direction : "", bucket);

    RaidObs::NoteDerived(bot, "mimiron.barrage", line);
}

bool MimironP3Wx2LaserBarrageAction::isUseful()
{
    MimironP3Wx2LaserBarrageTrigger mimironP3Wx2LaserBarrageTrigger(botAI);
    return mimironP3Wx2LaserBarrageTrigger.IsActive();
}

bool MimironP3Wx2LaserBarrageAction::Execute(Event /*event*/)
{
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_VX001);
    if (!boss || !boss->IsAlive())
        return false;

    MimironBarrageWindow const window = GetMimironBarrageWindow(bot, boss);
    if (!window.valid)
        return false;

    float const clearance = ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE + ULDUAR_MIMIRON_BARRAGE_MARGIN;
    float const twoPi = 2.0f * static_cast<float>(M_PI);

    // How far clockwise of the cone centreline the bot stands, in [0, 2pi). Measured this way rather
    // than folded to (-pi, pi] on purpose: the danger band runs sweep plus two clearances, 240 degrees
    // at the room centre and wider off it, so a signed fold puts part of it on the wrong side of pi.
    // That is how the far side of the room used to report safe while the beams swept across it.
    float const cw = Position::NormalizeOrientation(window.lead - boss->GetAngle(bot));

    // Two fringes: ahead of the leading edge, and behind where the trailing edge finishes. Returning
    // false rather than true is deliberate - bots that were never in danger keep casting.
    if (cw > window.sweep + clearance && cw < twoPi - clearance)
    {
        NoteBarrageDecision("clear", nullptr, cw);
        return false;
    }

    // Distance does not affect safety - the cone is 50000 yd long - so change bearing and leave the
    // radius alone, which is the shortest bearing change there is. The floor is the exception: melee
    // sit inside VX-001's combat reach, and orbiting at their own radius runs through the model.
    // GetExactDist2d, not GetDistance2d: the latter subtracts both object sizes, and this radius is
    // paired with a centre-based bearing to build a point measured from VX-001's centre.
    //
    // The phase 4 main tank gets a tighter floor. It cannot simply hold its spot - 20000 damage every
    // 250 ms - but every yard it runs drags the chassis and the cone apex with it, so it orbits inside
    // the chassis's chase range and the MK II stays where it is.
    bool const phase4MainTank = PlayerbotAI::IsMainTank(bot) && boss->GetVehicleBase();
    float const minRing = boss->GetCombatReach() + (phase4MainTank
                                                        ? ULDUAR_MIMIRON_BARRAGE_TANK_RING_MARGIN
                                                        : ULDUAR_MIMIRON_BARRAGE_RING_MARGIN);
    float const radius =
        std::clamp(bot->GetExactDist2d(boss), minRing, ULDUAR_MIMIRON_SPREAD_RADIUS_MAX);

    // Direction is chosen on **time spent inside the cone**, not on distance travelled. Distance is
    // what the old model compared, and it is the wrong currency: the short way round is frequently
    // straight through 104 degrees of beam at 20000 damage every 250 ms.
    //
    // Note the sign convention. `cw` falls on its own as the cone sweeps, so running clockwise raises
    // it against the sweep at (turnRate - rate), and counter-clockwise lowers it with the sweep at
    // (turnRate + rate).
    float const turnRate = radius > 0.0f ? bot->GetSpeed(MOVE_RUN) / radius : 0.0f;
    float const half = ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE;

    // Zero while Spinning Up: the boss aims once and then holds for four seconds, so the band is fixed
    // in world space and neither direction gets any help from the sweep. Getting this wrong is what
    // made a bot inside the ignition cone pick the far edge and still be there when the beams lit.
    float const sweepRate = window.untilLive > 0.0f ? 0.0f : window.rate;

    char const* branch = "trailing";
    bool goClockwise = false;
    if (cw <= window.sweep + clearance)
    {
        if (cw > half)
        {
            // The beams have not reached this bearing yet. Clockwise keeps it that way and costs
            // nothing; turning back would walk the bot into a cone it is currently in front of.
            branch = "ahead";
            goClockwise = true;
        }
        else
        {
            branch = "inside";
            // Inside the cone, on the leading side. Leave by whichever edge is sooner: counter-
            // clockwise rides the sweep out through the trailing edge, clockwise pushes out through
            // the leading edge against it. The crossover sits around 30 degrees off the centreline.
            float const outCcw = (cw + half) / (turnRate + sweepRate);
            float const outCw = turnRate > sweepRate
                                    ? (half - cw) / (turnRate - sweepRate)
                                    : std::numeric_limits<float>::max();
            goClockwise = outCw < outCcw;
        }
    }
    // Otherwise the bot sits counter-clockwise of the centreline, where the sweep is already carrying
    // it clear. Counter-clockwise, and it is out in a fraction of a second.

    float const ccwTravel = Position::NormalizeOrientation(cw + clearance);
    float const cwTravel = Position::NormalizeOrientation(window.sweep + clearance - cw);
    float const remaining = goClockwise ? -cwTravel : ccwTravel;

    // Under a yard of travel left; call it arrived rather than issuing a move nothing can act on.
    if (std::fabs(remaining) * radius < 1.0f)
    {
        NoteBarrageDecision("hold", nullptr, cw);
        return false;
    }

    // Walk the ring one bounded step at a time. Aiming a single move at the far side draws a chord
    // that cuts through VX-001 - creatures are not in the navmesh - and a chord across the apex
    // crosses every bearing the cone covers. MOVEMENT_FORCED serialises the legs by itself:
    // IsWaitingForLastMove refuses the next one until the current leg's lock expires.
    float const stepped =
        std::copysign(std::min(std::fabs(remaining), ULDUAR_MIMIRON_BARRAGE_STEP), remaining);
    float const heading = Position::NormalizeOrientation(boss->GetAngle(bot) + stepped);

    NoteBarrageDecision(branch, goClockwise ? "cw" : "ccw", cw);

    // Nothing survives standing in this to finish a cast, and a casting bot cannot be moved at all -
    // see the note on MoveAwayClearOfMines.
    bot->CastStop();

    MoveTo(boss->GetMapId(), boss->GetPositionX() + radius * cos(heading),
           boss->GetPositionY() + radius * sin(heading), boss->GetPositionZ(), false, false, false,
           true, MovementPriority::MOVEMENT_FORCED, true);

    // Re-check fast while relocating: the cone lands damage every 250 ms and turns nearly three degrees
    // in that time, so a normal interval is most of a cone width. Only while moving - a bot already
    // clear only gets clearer as the sweep retreats from it.
    botAI->SetNextCheckDelay(100);

    // Hold the tick. Spinning Up is only 4s of warning and the cone kills outright, so a relocating
    // bot must not stop to finish a cast.
    return true;
}

bool MimironArcSpreadAction::isUseful()
{
    MimironArcSpreadTrigger mimironArcSpreadTrigger(botAI);
    return mimironArcSpreadTrigger.IsActive();
}

bool MimironArcSpreadAction::Execute(Event /*event*/)
{
    Position slot;
    if (!GetMimironSpreadSlot(botAI, bot, slot))
        return false;

    // Ten mines land eight seconds after every Shock Blast, and Rocket Strike markers sit on the
    // ring for five. Nothing in pathing knows about either, so holding beats walking into them - except
    // for the tank, whose spot is under the mech that laid them, and for the handover lap, which has
    // already swept its own waypoint and must never hand the tick back.
    if (!IsMimironTankAnchorSlot(botAI, bot) && !IsMimironLapSlot(botAI, bot) &&
        !IsMimironSpotSafe(bot, slot))
        return false;

    return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool MimironAerialCommandUnitAction::Execute(Event /*event*/)
{
    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0))
    {
        Unit* assaultBot = GetFirstAliveUnitByEntry(botAI, NPC_ASSAULT_BOT);
        Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);

        // The mark is for the human raid leader only - nothing reads it back. Bots pick their own
        // target through "mimiron set dps priority", so a stale icon can no longer strand the raid.
        Unit* focus = assaultBot ? assaultBot : boss;
        if (focus && IsMechanicTrackerBot(bot, ULDUAR_MAP_ID))
            MarkTargetWithSkull(bot, focus);
    }

    return false;
}

bool MimironRocketStrikeAction::isUseful()
{
    MimironRocketStrikeTrigger mimironRocketStrikeTrigger(botAI);
    return mimironRocketStrikeTrigger.IsActive();
}

bool MimironRocketStrikeAction::Execute(Event /*event*/)
{
    Creature* rocketStrikeN = bot->FindNearestCreature(NPC_ROCKET_STRIKE_N, 100.0f);
    if (!rocketStrikeN)
        return false;

    // 63041 blasts 3 yd; 10 covers the bot's footprint and pathing slop. The old phase 3/4 branch
    // teleported instead, off a stale pointer left over from the mech sweep. MOVEMENT_FORCED so the
    // arc-spread leg the bot is usually mid-way through cannot swallow the dodge.
    return MoveAwayClearOfMines(rocketStrikeN, 10.0f, MovementPriority::MOVEMENT_FORCED, true, true,
                                "rocket");
}

bool MimironPhase4FocusAction::Execute(Event /*event*/)
{
    if (!IsMimironPhase4(bot))
        return false;

    if (botAI->IsMainTank(bot))
    {
        // Direct targeting, no raid icon. All three reassemble and only stay down if they reach Self
        // Repair together, so the tank follows whichever is furthest from the floor by percent - the
        // Aerial Command Unit's pool is two thirds the others', and ordering on raw health starved it.
        Unit* const focus = GetMimironPhase4Focus(botAI, bot, true);

        // Nothing left above the floor that the tank may touch, so it stops swinging with its melee.
        // Safe only because nothing else is generating threat by then: with the DPS held, threat is
        // static and the mech does not change hands. A tank left swinging is a slow steady push toward
        // 15000 on the one part nobody wants pushed.
        if (!focus)
        {
            bot->AttackStop();
            return true;
        }

        if (AI_VALUE(Unit*, "current target") != focus)
            return Attack(focus);

        return false;
    }

    // Hand Pulse is a cone that re-aims every 1.75s, so there is nothing to sidestep - the only
    // thing that helps is not being bunched up behind whoever it picks.
    if (AI_VALUE(float, "disperse distance") != 4.0f)
        SET_AI_VALUE(float, "disperse distance", 4.0f);

    // Never claims the tick: "combat formation move" is what reads the value, and it sits below
    // ACTION_RAID.
    return false;
}

bool MimironProximityMineAction::isUseful()
{
    MimironProximityMineTrigger mimironProximityMineTrigger(botAI);
    return mimironProximityMineTrigger.IsActive();
}

bool MimironProximityMineAction::Execute(Event /*event*/)
{
    Unit* nearest = nullptr;
    float nearestDist = ULDUAR_MIMIRON_MINE_TRIGGER_RADIUS;

    // Mines are non-selectable, so they only show up on the raw nearby-npc list.
    for (auto const& guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_PROXIMITY_MINE)
            continue;

        float const dist = bot->GetExactDist2d(unit);
        if (dist < nearestDist)
        {
            nearestDist = dist;
            nearest = unit;
        }
    }

    if (!nearest)
        return false;

    // Just far enough to clear the 1.9 yd poll and the 3 yd blast, capped so a bot standing in a
    // fresh field sidesteps instead of running the room. No unfiltered fallback: swapping one mine
    // for another is not worth the travel, and a single blast is healable.
    float const step = std::min(ULDUAR_MIMIRON_MINE_CLEARANCE + 1.0f - nearestDist,
                                ULDUAR_MIMIRON_MINE_MAX_STEP);
    return MoveAwayClearOfMines(nearest, std::max(step, 2.0f), MovementPriority::MOVEMENT_COMBAT,
                                false, false, "mine");
}

bool MimironPetControlAction::isUseful()
{
    MimironPetControlTrigger mimironPetControlTrigger(botAI);
    return mimironPetControlTrigger.IsActive();
}

bool MimironPetControlAction::Execute(Event /*event*/)
{
    // PetAttackAction's node is commented out globally, so a pet keeps whatever it last latched onto
    // unless a fight says otherwise. Left alone it stands under the hovering Aerial Command Unit in
    // phase 3, then carries a dead Junk Bot through the whole of phase 4.
    if (IsMimironPhase4(bot))
    {
        // Pets go where the melee go, and hold when the melee hold. A hunter's pet is about a fifth of
        // that hunter's damage, and leaking that into the rendezvous is what the floor exists to stop.
        // Asking for a melee answer also keeps them off the Aerial Command Unit, which is reachable
        // here - seat offsets are zero and the chassis writes its passengers to its own position - so
        // this is holding the ranged and melee split, not working around a hitbox.
        if (Unit* focus = GetMimironPhase4Focus(botAI, bot, true))
            CommandPetAttack(botAI, focus);
        else
            StopPet(botAI);

        return false;
    }

    // Phase 3, with a Magnetic Core down. Twenty seconds of a stationary boss on the floor taking +50%
    // damage, and nothing new spawning for twenty-five - the one window the phase can be shortened in.
    if (IsMimironAcuGrounded(botAI))
    {
        if (Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        {
            CommandPetAttack(botAI, aerialCommandUnit);
            return false;
        }
    }

    // Phase 3 otherwise. A ground pet cannot reach a boss hovering 15 yd up, so the adds are the better
    // target anyway. Assault Bot first: it is the only Magnetic Core source, and the core is what
    // brings the Aerial Command Unit down.
    for (uint32 entry : {NPC_ASSAULT_BOT, NPC_JUNK_BOT, NPC_BOMB_BOT})
    {
        Unit* nearest = nullptr;
        float nearestDist = 0.0f;

        for (ObjectGuid const& guid : AI_VALUE(GuidVector, "possible targets no los"))
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
                continue;

            // Nearest, not first: the pet has to run there, and the raw list is in no useful order.
            float const dist = bot->GetExactDist2d(unit);
            if (!nearest || dist < nearestDist)
            {
                nearest = unit;
                nearestDist = dist;
            }
        }

        if (nearest)
        {
            CommandPetAttack(botAI, nearest);
            return false;
        }
    }

    StopPet(botAI);

    // Always false: this only redirects the pet, and the bot still needs the tick for its own turn.
    return false;
}

bool MimironDodgeFlamesAction::isUseful()
{
    MimironDodgeFlamesTrigger mimironDodgeFlamesTrigger(botAI);
    return mimironDodgeFlamesTrigger.IsActive();
}

bool MimironDodgeFlamesAction::Execute(Event /*event*/)
{
    // Fire nodes are non-selectable, so find them via the raw nearby-npc list. Flee the centre of the
    // burning cluster rather than the nearest node, so the bot leaves the cluster instead of stepping
    // out of one node into the next one along.
    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);

    std::vector<Position> cluster;
    for (Position const& node : hazards.flames)
        if (bot->GetExactDist2d(node.GetPositionX(), node.GetPositionY()) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            cluster.push_back(node);

    if (cluster.empty())
        return false;

    float cx = 0.0f, cy = 0.0f;
    for (Position const& node : cluster)
    {
        cx += node.GetPositionX();
        cy += node.GetPositionY();
    }
    cx /= cluster.size();
    cy /= cluster.size();

    Position const centre(cx, cy, bot->GetPositionZ());
    float spread = 0.0f;
    for (Position const& node : cluster)
        spread = std::max(spread, centre.GetExactDist2d(node.GetPositionX(), node.GetPositionY()));

    // Shortest hop that actually clears the field, not the longest one that might. Every rung is
    // screened against the whole field by the fan, so a short one is only taken when it lands clean,
    // and what the bot saves is the walk back - a fixed 12 yd hop cost melee two thirds of their time
    // in range. Only the last rung may fall through to an unscreened MoveAway; the earlier ones fail
    // quietly so the ladder can carry on. Distinct names per rung, because which one won is the whole
    // question the next trace has to answer.
    for (size_t rung = 0; rung < std::size(ULDUAR_MIMIRON_FLAMES_STEP_LADDER); ++rung)
    {
        float const step = ULDUAR_MIMIRON_FLAMES_STEP_LADDER[rung];
        bool const last = rung + 1 == std::size(ULDUAR_MIMIRON_FLAMES_STEP_LADDER);

        char what[16];
        snprintf(what, sizeof(what), "flames+%.0f", step);

        if (MoveAwayClearOfMines(centre, ULDUAR_MIMIRON_FLAMES_RADIUS + spread + step,
                                 MovementPriority::MOVEMENT_COMBAT, last, true, what))
            return true;
    }

    return false;
}

bool MimironRapidBurstAction::isUseful()
{
    MimironRapidBurstTrigger mimironRapidBurstTrigger(botAI);
    return mimironRapidBurstTrigger.IsActive();
}

bool MimironRapidBurstAction::Execute(Event /*event*/)
{
    Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001);
    if (!vx001)
        return false;

    MimironRapidBurstWindow const window = GetMimironRapidBurstWindow(botAI, bot, vx001);
    if (!window.valid || window.escape <= 0.0f)
        return false;

    // Out the near edge, keeping the radius the bot is already standing at: it is in casting or melee
    // range there, so nothing pulls it back afterwards, and moving in or out changes nothing against a
    // 100 yd cone. Exactly on the centreline both edges cost the same and the sign is arbitrary.
    float const side = window.offset >= 0.0f ? 1.0f : -1.0f;
    float const bearing = Position::NormalizeOrientation(
        window.centreline +
        side * (ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE + ULDUAR_MIMIRON_RAPID_BURST_MARGIN));
    float const radius = bot->GetExactDist2d(vx001);

    Position const dest(vx001->GetPositionX() + radius * std::cos(bearing),
                        vx001->GetPositionY() + radius * std::sin(bearing), bot->GetPositionZ());

    // MOVEMENT_FORCED so a formation leg already in flight cannot swallow it: the whole window is 3 s
    // and IsWaitingForLastMove refuses anything not strictly above the move it is holding.
    return MoveTowardClearOfMines(dest, MovementPriority::MOVEMENT_FORCED, true, true, "rapidburst");
}

bool MimironFrostBombAction::isUseful()
{
    MimironFrostBombTrigger mimironFrostBombTrigger(botAI);
    return mimironFrostBombTrigger.IsActive();
}

bool MimironFrostBombAction::Execute(Event /*event*/)
{
    Creature* frostBomb = bot->FindNearestCreature(NPC_FROST_BOMB, ULDUAR_MIMIRON_FROST_BOMB_RADIUS);
    if (!frostBomb)
        return false;

    // MOVEMENT_FORCED because the formation leg a bot is usually mid-way through outlives the whole
    // ten second fuse otherwise: one healer issued the right 38 yd escape, had it held, and died 23 yd
    // from the bomb still walking a flames hop. Unfiltered fallback because a 30 yd blast can leave no
    // clean bearing at all, and moving somewhere beats standing in it.
    float const gap = ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE - bot->GetExactDist2d(frostBomb);
    return MoveAwayClearOfMines(frostBomb, gap, MovementPriority::MOVEMENT_FORCED, true, true,
                                "frostbomb");
}

std::vector<std::pair<uint32, Unit*>> MimironSetDpsPriorityAction::BuildPriorityList()
{
    Unit* leviathanMkII = nullptr;
    Unit* vx001 = nullptr;
    Unit* aerialCommandUnit = nullptr;
    std::vector<Unit*> bombBots;
    std::vector<Unit*> assaultBots;
    std::vector<Unit*> fireBots;
    std::vector<Unit*> junkBots;

    for (ObjectGuid const& guid :
         botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_LEVIATHAN_MKII:      leviathanMkII = unit; break;
            case NPC_VX001:               vx001 = unit; break;
            case NPC_AERIAL_COMMAND_UNIT: aerialCommandUnit = unit; break;
            case NPC_BOMB_BOT:            bombBots.push_back(unit); break;
            case NPC_ASSAULT_BOT:         assaultBots.push_back(unit); break;
            case NPC_EMERGENCY_FIRE_BOT:  fireBots.push_back(unit); break;
            case NPC_JUNK_BOT:            junkBots.push_back(unit); break;
            default: break;
        }
    }

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // A Bomb Bot runs 8.0 yd/s against a player's 7.0 and detonates on melee contact, so backing away
    // never wins - ranged have to kill it, and it dies to almost nothing. Only ones already in casting
    // range go on the list: chasing one abandons the mech for an add somebody else can reach, and drops
    // the bot into the same "reach spell" versus ACTION_RAID deadlock the ring positioning had. The
    // Assault Bot comes next because it is the only Magnetic Core source, and the core is what grounds
    // the Aerial Command Unit, so stopping for fire bots or Junk Bots ahead of it stretches the phase.
    std::vector<std::pair<uint32, Unit*>> priority;
    if (PlayerbotAI::IsRangedDps(bot))
    {
        std::vector<Unit*> reachable;
        for (Unit* bombBot : bombBots)
            if (bot->GetExactDist2d(bombBot) <= sPlayerbotAIConfig.spellDistance)
                reachable.push_back(bombBot);

        priority.emplace_back(NPC_BOMB_BOT, SelectByEntry(currentTarget, NPC_BOMB_BOT, reachable));
    }

    // A grounded Aerial Command Unit outranks the adds for melee. Nothing new spawns for the whole
    // window, so the only competition is whatever survived it, and +50% damage on the boss beats any
    // of it. Ranged keep the add order and arrive here on their own once the leftovers are dead.
    // Tanks never reach this - "mimiron set dps priority" stands down for them - so the Assault Bot
    // keeps its tank throughout, which is deliberate: it is the one add nobody can ignore.
    if (aerialCommandUnit && botAI->IsMelee(bot) && IsMimironAcuGrounded(botAI))
        priority.emplace_back(NPC_AERIAL_COMMAND_UNIT, aerialCommandUnit);

    priority.emplace_back(NPC_ASSAULT_BOT, SelectByEntry(currentTarget, NPC_ASSAULT_BOT, assaultBots));

    if (IsMimironHardModeActive(botAI))
        priority.emplace_back(NPC_EMERGENCY_FIRE_BOT,
                              SelectByEntry(currentTarget, NPC_EMERGENCY_FIRE_BOT, fireBots));

    priority.emplace_back(NPC_JUNK_BOT, SelectByEntry(currentTarget, NPC_JUNK_BOT, junkBots));

    // Phase 4 has its own rule: the three only stay down if all of them reach Self Repair inside its
    // 15 s cast, so they have to come down level. One shared helper answers for the tank node, this one
    // and the pets, which is what stops the three disagreeing about who is being evened out.
    if (IsMimironPhase4(bot))
    {
        if (Unit* focus = GetMimironPhase4Focus(botAI, bot, botAI->IsMelee(bot)))
            priority.emplace_back(focus->GetEntry(), focus);

        return priority;
    }

    // Outside phase 4 exactly one mech is up at a time, so there is nothing to order.
    for (Unit* mech : {leviathanMkII, vx001, aerialCommandUnit})
        if (mech)
            priority.emplace_back(mech->GetEntry(), mech);

    return priority;
}

Unit* MimironSetDpsPriorityAction::SelectByEntry(Unit* currentTarget, uint32 entry,
                                                std::vector<Unit*> const& candidates) const
{
    Unit* selected = nullptr;
    if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == entry)
        selected = currentTarget;

    // Adds walk in from pads all round the room, so nearest-to-the-bot beats measuring from an
    // anchor. The margin stops two similar adds trading the bot back and forth every tick.
    float const switchMargin = 10.0f;
    for (Unit* candidate : candidates)
    {
        if (!candidate || candidate == selected)
            continue;

        if (!selected)
        {
            selected = candidate;
            continue;
        }

        if (candidate->GetExactDist2d(bot) + switchMargin < selected->GetExactDist2d(bot))
            selected = candidate;
    }

    return selected;
}

bool MimironSetDpsPriorityAction::IsAllowedTarget(Unit* unit) const
{
    if (!unit || !unit->IsAlive())
        return false;

    switch (unit->GetEntry())
    {
        case NPC_BOMB_BOT:
            // Melee cannot reach one without setting it off. Ranged shoot it from outside the blast,
            // and keep shooting it inside the blast when it is the one chasing them: at 8.0 yd/s
            // against 7.0 they cannot leave, so dropping it there left them neither shooting nor
            // stepping out, which is how two of them died standing on one at full health.
            if (botAI->IsMelee(bot))
                return false;

            return unit->GetExactDist2d(bot) >= ULDUAR_MIMIRON_BOMB_BOT_RADIUS ||
                   GetMimironBombBotChasing(botAI, bot) == unit;

        case NPC_AERIAL_COMMAND_UNIT:
        {
            if (!botAI->IsMelee(bot))
                return true;

            // In phase 3 it hovers out of reach until a Magnetic Core grounds it. In phase 4 it is
            // reachable, and melee are kept off it by choice so the ranged half of the raid can bring
            // it down level with the other two. Not tested through MOVEMENTFLAG_HOVER: that survives
            // the phase 3 defeat and the vehicle boarding, so it says nothing about which phase it is.
            //
            // The bar lifts once a part is already self-repairing. The rendezvous is over by then and
            // melee would otherwise have nothing to hit through the 15 s that decides the kill.
            if (!IsMimironPhase4(bot))
                return IsMimironAcuGrounded(botAI);

            return GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) == nullptr ||
                   GetFirstAliveUnitByEntry(botAI, NPC_VX001) == nullptr;
        }

        default:
            return true;
    }
}

// Short name for the trace. The entry number alone would do, but a note stream is read by eye and
// "assaultbot" beats looking 33343 up in the header.
char const* MimironSetDpsPriorityAction::DescribeTargetRule(Unit* unit)
{
    if (!unit)
        return "none";

    switch (unit->GetEntry())
    {
        case NPC_BOMB_BOT:            return "bombbot";
        case NPC_ASSAULT_BOT:         return "assaultbot";
        case NPC_EMERGENCY_FIRE_BOT:  return "firebot";
        case NPC_JUNK_BOT:            return "junkbot";
        case NPC_AERIAL_COMMAND_UNIT: return "acu";
        case NPC_VX001:               return "vx001";
        case NPC_LEVIATHAN_MKII:      return "mkii";
        default:                      return "other";
    }
}

Unit* MimironSetDpsPriorityAction::ResolveTarget(Unit* currentTarget)
{
    std::vector<std::pair<uint32, Unit*>> const priority = BuildPriorityList();

    Unit* target = nullptr;
    for (auto const& candidate : priority)
    {
        if (IsAllowedTarget(candidate.second))
        {
            target = candidate.second;
            break;
        }
    }

    auto const priorityIndex = [&](Unit* unit) -> size_t
    {
        if (!IsAllowedTarget(unit))
            return priority.size();

        for (size_t index = 0; index < priority.size(); ++index)
        {
            if (priority[index].first == unit->GetEntry())
                return index;
        }

        return priority.size();
    };

    // Hold what the bot is already on unless something strictly more urgent is up, so a churn of
    // Junk Bots cannot keep resetting swing and cast timers.
    bool held = false;
    if (currentTarget && priorityIndex(currentTarget) <= priorityIndex(target))
    {
        held = target != currentTarget;
        target = currentTarget;
    }

    if (target)
    {
        // The rule, not the guid: snap.u already samples every bot's target four times a second.
        // What it cannot say is whether the list picked this one or the hold kept it, which is the
        // difference between a priority bug and a bot that simply never re-evaluated.
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "mimiron.dpsrule",
                                 held ? std::string("held:") + DescribeTargetRule(target)
                                      : std::string(DescribeTargetRule(target)));

        return target;
    }

    // Nothing on the list is allowed, so the generic picker answers instead - which is the state
    // worth seeing, because it means this node stopped steering.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "mimiron.dpsrule", "fallback");

    return AI_VALUE(Unit*, "dps target");
}

bool MimironSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // Everything this bot may touch in phase 4 is already at the floor, so it holds rather than pushing
    // a part under while the others are still high. Holding the tick keeps the class strategies from
    // re-engaging; everything above ACTION_RAID still runs, so the dodges are untouched.
    //
    // Never for a healer. Its allowed set is the two ground mechs like any melee, so it does reach the
    // floor - but holding the tick here would stop it healing, which costs far more than the trickle of
    // splash damage it was contributing.
    if (IsMimironPhase4(bot) && !botAI->IsHeal(bot) &&
        !GetMimironPhase4Focus(botAI, bot, botAI->IsMelee(bot)))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "mimiron.dpsrule", "p4hold");

        bot->AttackStop();
        return true;
    }

    Unit* target = ResolveTarget(currentTarget);
    if (!target)
        return false;

    // Returning false once the bot is already on the right target is what lets the lower-priority
    // nodes run at all: the engine ends the tick at the first action that succeeds.
    bool needsAttack = currentTarget != target;
    if (botAI->IsMelee(bot))
        needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    return needsAttack ? Attack(target) : false;
}

bool MimironPlasmaBlastAction::isUseful()
{
    MimironPlasmaBlastTrigger mimironPlasmaBlastTrigger(botAI);
    return mimironPlasmaBlastTrigger.IsActive();
}

bool MimironPlasmaBlastAction::Execute(Event event)
{
    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (!leviathanMkII || leviathanMkII->GetVictim() == bot)
        return false;

    // Target and taunt on the same tick. Spending one on Attack, which succeeds and ends the tick, put
    // the taunt a tick late; the trigger now fires between casts rather than during one, so there is
    // room for that, but no reason to give the window away either.
    if (AI_VALUE(Unit*, "current target") != leviathanMkII)
        Attack(leviathanMkII);

    // Taunt sets the taunter's threat equal to the current highest, so the swap holds across the 22 s
    // cycle as long as this tank keeps swinging - which is why it has to land before the cast rather
    // than during it. "taunt spell" is registered for all four tank specs.
    return botAI->DoSpecificAction("taunt spell", event, true);
}

bool MimironSlowBombBotAction::isUseful()
{
    MimironSlowBombBotTrigger mimironSlowBombBotTrigger(botAI);
    return mimironSlowBombBotTrigger.IsActive();
}

bool MimironSlowBombBotAction::Execute(Event event)
{
    std::string const spell = GetMimironBombBotSnare(bot);
    if (spell.empty())
        return false;

    // Returns its own result rather than holding the tick: a bot that does not have the spell, or has
    // it on cooldown, should drop straight through to its rotation and keep shooting.
    return botAI->DoSpecificAction(spell, event, true);
}

bool MimironMagneticCoreAction::isUseful()
{
    MimironMagneticCoreTrigger mimironMagneticCoreTrigger(botAI);
    return mimironMagneticCoreTrigger.IsActive();
}

// Which of the five steps the carrier is on. The act stream only says the node ran; when a core never
// reaches the Aerial Command Unit, the answer is always which step it stopped at.
void MimironMagneticCoreAction::NoteCoreStep(char const* step)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "mimiron.corestep", step);
}

bool MimironMagneticCoreAction::Execute(Event /*event*/)
{
    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit)
    {
        NoteCoreStep("no-acu");
        return false;
    }

    Item* core = bot->GetItemByEntry(ITEM_MIMIRON_MAGNETIC_CORE);
    if (!core)
    {
        Creature* corpse =
            bot->FindNearestCreature(NPC_ASSAULT_BOT, ULDUAR_MIMIRON_CORE_SEARCH_RANGE, false);
        if (!corpse)
        {
            NoteCoreStep("no-corpse");
            return false;
        }

        // Go and get it. The corpse lasts 25 s and the Assault Bot dies wherever the raid stopped it,
        // so the node has to walk: the old version only ever fired if the carrier happened to already
        // be standing on one, which is why the core never reached the Aerial Command Unit.
        if (bot->GetExactDist2d(corpse) > ULDUAR_MIMIRON_CORE_LOOT_RANGE)
        {
            NoteCoreStep("walk-corpse");
            return MoveTo(corpse->GetMapId(), corpse->GetPositionX(), corpse->GetPositionY(),
                          corpse->GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_COMBAT, true);
        }

        // Bots have no in-combat loot path - looting is only wired into LootNonCombatStrategy - and
        // 46029 is a white consumable the loot strategies discard as junk even when one is open. The
        // Assault Bot drops it at 100%, so a real raid always leaves this fight holding one. Handing
        // it over stands in for the missing packet exchange, gated on what a player would still have
        // to do: kill the bot, stand on the corpse, and not already be carrying one.
        ItemPosCountVec dest;
        if (bot->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_MIMIRON_MAGNETIC_CORE, 1) != EQUIP_ERR_OK)
        {
            NoteCoreStep("bags-full");
            return false;
        }

        bot->StoreNewItem(dest, ITEM_MIMIRON_MAGNETIC_CORE, true,
                          Item::GenerateItemRandomPropertyId(ITEM_MIMIRON_MAGNETIC_CORE));
        NoteCoreStep("loot");
        return true;
    }

    // 64444 places its summon by nearest entry, so the core only reaches the ACU from underneath it.
    if (bot->GetExactDist2d(aerialCommandUnit) > ULDUAR_MIMIRON_CORE_USE_RANGE)
    {
        NoteCoreStep("walk-acu");
        return MoveTo(aerialCommandUnit->GetMapId(), aerialCommandUnit->GetPositionX(),
                      aerialCommandUnit->GetPositionY(), bot->GetPositionZ(), false, false, false,
                      true, MovementPriority::MOVEMENT_COMBAT, true);
    }

    if (bot->CanUseItem(core) != EQUIP_ERR_OK || bot->IsNonMeleeSpellCast(false))
    {
        NoteCoreStep("blocked");
        return false;
    }

    uint32 spellId = 0;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (core->GetTemplate()->Spells[i].SpellId > 0)
        {
            spellId = core->GetTemplate()->Spells[i].SpellId;
            break;
        }
    }

    if (!spellId)
    {
        NoteCoreStep("blocked");
        return false;
    }

    NoteCoreStep("use");

    uint8 const bagIndex = core->GetBagSlot();
    uint8 const slot = core->GetSlot();
    constexpr uint8 castCount = 0;
    constexpr uint32 glyphIndex = 0;
    constexpr uint8 castFlags = 0;

    WorldPacket packet(CMSG_USE_ITEM);
    packet << bagIndex;
    packet << slot;
    packet << castCount;
    packet << spellId;
    packet << core->GetGUID();
    packet << glyphIndex;
    packet << castFlags;
    packet << (uint32)TARGET_FLAG_NONE;

    bot->GetSession()->HandleUseItemOpcode(packet);
    return true;
}
