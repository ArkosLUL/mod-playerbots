#include "UldActions_Mimiron.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <algorithm>
#include <cmath>
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
#include "Position.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

bool MimironFleeAction::MoveAwayClearOfMines(Unit* from, float distance, MovementPriority priority,
                                             bool fallbackUnfiltered)
{
    if (!from || distance <= 0.0f)
        return false;

    float const away = from->GetAngle(bot);
    for (float delta = 0.0f; delta <= static_cast<float>(M_PI) / 2.0f;
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

            if (!IsMimironSpotMineSafe(bot, Position(dx, dy, dz)))
                continue;

            if (MoveTo(from->GetMapId(), dx, dy, dz, false, false, true, exact, priority))
                return true;
        }
    }

    // Every mine-clear bearing was refused.
    if (!fallbackUnfiltered)
        return false;

    return MoveAway(from, distance);
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
        MoveAwayClearOfMines(leviathanMkII, gap, MovementPriority::MOVEMENT_FORCED);

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

bool MimironPhase1PositioningAction::Execute(Event /*event*/)
{
    SET_AI_VALUE(float, "disperse distance", 6.0f);
    return true;
}

bool MimironPhase1PositioningAction::isUseful()
{
    MimironPhase1PositioningTrigger mimironPhase1PositioningTrigger(botAI);
    return mimironPhase1PositioningTrigger.IsActive();
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
        return false;

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

    bool goClockwise = false;
    if (cw <= window.sweep + clearance)
    {
        if (cw > half)
        {
            // The beams have not reached this bearing yet. Clockwise keeps it that way and costs
            // nothing; turning back would walk the bot into a cone it is currently in front of.
            goClockwise = true;
        }
        else
        {
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
        return false;

    // Walk the ring one bounded step at a time. Aiming a single move at the far side draws a chord
    // that cuts through VX-001 - creatures are not in the navmesh - and a chord across the apex
    // crosses every bearing the cone covers. MOVEMENT_FORCED serialises the legs by itself:
    // IsWaitingForLastMove refuses the next one until the current leg's lock expires.
    float const stepped =
        std::copysign(std::min(std::fabs(remaining), ULDUAR_MIMIRON_BARRAGE_STEP), remaining);
    float const heading = Position::NormalizeOrientation(boss->GetAngle(bot) + stepped);

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
    // for the tank, whose spot is under the mech that laid them.
    if (!IsMimironTankAnchorSlot(botAI, bot) && !IsMimironSpotSafe(bot, slot))
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
        if (focus && IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
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
    return MoveAwayClearOfMines(rocketStrikeN, 10.0f, MovementPriority::MOVEMENT_FORCED);
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

    return true;
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
                                false);
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

    // Phase 3. A ground pet cannot reach a boss hovering 15 yd up, so the adds are the better target
    // anyway. Assault Bot first: it is the only Magnetic Core source, and the core is what brings the
    // Aerial Command Unit down.
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
    // Fire nodes are non-selectable, so find them via the raw nearby-npc list. Flee from the centre of the
    // whole in-range fire field (not just the nearest node) out past its edge, so the bot leaves the field
    // instead of stepping out of one node straight into the next.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    std::vector<Position> nodes;

    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FLAMES_SPREAD && unit->GetEntry() != NPC_FLAMES_INITIAL)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            nodes.push_back(unit->GetPosition());
    }

    if (nodes.empty())
        return false;

    float cx = 0.0f, cy = 0.0f;
    for (Position const& node : nodes)
    {
        cx += node.GetPositionX();
        cy += node.GetPositionY();
    }
    cx /= nodes.size();
    cy /= nodes.size();

    // Flee far enough to clear the outermost in-range node, not just the centre.
    Position const centre(cx, cy, 0.0f);
    float spread = 0.0f;
    for (Position const& node : nodes)
    {
        float const d = centre.GetExactDist2d(node.GetPositionX(), node.GetPositionY());
        if (d > spread)
            spread = d;
    }

    return FleePosition(Position(cx, cy, bot->GetPositionZ()), ULDUAR_MIMIRON_FLAMES_RADIUS + spread + 1.0f);
}

bool MimironFrostBombAction::isUseful()
{
    MimironFrostBombTrigger mimironFrostBombTrigger(botAI);
    return mimironFrostBombTrigger.IsActive();
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
            // Melee cannot reach one without setting it off, and ranged only from outside the blast:
            // closer than that the avoid action should own the bot, not this one holding it still.
            return !botAI->IsMelee(bot) &&
                   unit->GetExactDist2d(bot) >= ULDUAR_MIMIRON_BOMB_BOT_RADIUS;

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
                return false;

            return GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) == nullptr ||
                   GetFirstAliveUnitByEntry(botAI, NPC_VX001) == nullptr;
        }

        default:
            return true;
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
    if (currentTarget && priorityIndex(currentTarget) <= priorityIndex(target))
        target = currentTarget;

    return target ? target : AI_VALUE(Unit*, "dps target");
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

bool MimironMagneticCoreAction::isUseful()
{
    MimironMagneticCoreTrigger mimironMagneticCoreTrigger(botAI);
    return mimironMagneticCoreTrigger.IsActive();
}

bool MimironMagneticCoreAction::Execute(Event /*event*/)
{
    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit)
        return false;

    Item* core = bot->GetItemByEntry(ITEM_MIMIRON_MAGNETIC_CORE);
    if (!core)
    {
        Creature* corpse =
            bot->FindNearestCreature(NPC_ASSAULT_BOT, ULDUAR_MIMIRON_CORE_SEARCH_RANGE, false);
        if (!corpse)
            return false;

        // Go and get it. The corpse lasts 25 s and the Assault Bot dies wherever the raid stopped it,
        // so the node has to walk: the old version only ever fired if the carrier happened to already
        // be standing on one, which is why the core never reached the Aerial Command Unit.
        if (bot->GetExactDist2d(corpse) > ULDUAR_MIMIRON_CORE_LOOT_RANGE)
            return MoveTo(corpse->GetMapId(), corpse->GetPositionX(), corpse->GetPositionY(),
                          corpse->GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_COMBAT, true);

        // Bots have no in-combat loot path - looting is only wired into LootNonCombatStrategy - and
        // 46029 is a white consumable the loot strategies discard as junk even when one is open. The
        // Assault Bot drops it at 100%, so a real raid always leaves this fight holding one. Handing
        // it over stands in for the missing packet exchange, gated on what a player would still have
        // to do: kill the bot, stand on the corpse, and not already be carrying one.
        ItemPosCountVec dest;
        if (bot->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_MIMIRON_MAGNETIC_CORE, 1) != EQUIP_ERR_OK)
            return false;

        bot->StoreNewItem(dest, ITEM_MIMIRON_MAGNETIC_CORE, true,
                          Item::GenerateItemRandomPropertyId(ITEM_MIMIRON_MAGNETIC_CORE));
        return true;
    }

    // 64444 places its summon by nearest entry, so the core only reaches the ACU from underneath it.
    if (bot->GetExactDist2d(aerialCommandUnit) > ULDUAR_MIMIRON_CORE_USE_RANGE)
    {
        return MoveTo(aerialCommandUnit->GetMapId(), aerialCommandUnit->GetPositionX(),
                      aerialCommandUnit->GetPositionY(), bot->GetPositionZ(), false, false, false,
                      true, MovementPriority::MOVEMENT_COMBAT, true);
    }

    if (bot->CanUseItem(core) != EQUIP_ERR_OK || bot->IsNonMeleeSpellCast(false))
        return false;

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
        return false;

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
