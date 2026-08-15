/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSActions.h"

#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Random.h"
#include "Timer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace OsHelpers;

bool OsPositioningAction::MoveToClamped(float x, float y, float tolerance)
{
    ClampDestination(x, y);

    if (bot->GetExactDist2d(x, y) <= tolerance)
        return false;

    return MoveTo(OS_MAP_ID, x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool OsTsunamiCorridorAction::Execute(Event /*event*/)
{
    // Everyone but the main tank keeps the X they dodged from, so the raid line and whoever is holding
    // a drake stay where they are instead of drifting downrange every time a wave goes out.
    // The tank is the exception: his two holds are measured positions 1.3yd apart on X, and keeping his
    // own X would leave him beside the coordinate rather than on it.
    float const x = botAI->IsMainTank(bot) ? TankHoldX(bot) : bot->GetPositionX();
    return MoveToClamped(x, SafeCorridorY(bot));
}

bool OsAvoidTwilightFissureAction::Execute(Event /*event*/)
{
    // requireSelectable off: the fissure is permanently UNIT_FLAG_NOT_SELECTABLE, which is why this
    // dodge never once fired.
    Unit* fissure = FindUnitByEntries(bot, { NpcId::TwilightFissure, NpcId::TwilightFissureH },
                                      FISSURE_CLEAR_RADIUS, false);
    if (!fissure)
        return false;

    // The Y an X sidestep keeps: the bot's own, not the raid corridor's. For ranged the two are the
    // same; melee on a drake stand up to 28yd off it, and walking there to dodge a 5.5yd blast cost a
    // round trip on every fissure. A live wave still overrides, because that Y is not a choice.
    TsunamiWave const wave = ClassifyTsunamiWave(bot);
    float const stepY = WaveClearsY(bot->GetPositionY(), wave) ? bot->GetPositionY() : SafeCorridorY(bot);
    float const fx = fissure->GetPositionX();
    float const fy = fissure->GetPositionY();
    // Stepping to exactly the detection radius leaves the bot on the trigger boundary, where the
    // dodge fires again next tick.
    float const step = FISSURE_CLEAR_RADIUS + FISSURE_STEP_MARGIN;

    // The default list, for ranged and for melee: X steps first, because the corridor is 15yd wide and
    // the blast is 5.5yd, so sidestepping along X always clears it while keeping the Y a tsunami dodge
    // just bought. A bare MoveAway is free to pick Y, which walks the bot out of the corridor and
    // starts a fight with the corridor action. The Y steps are the rim fallback, for when both X
    // candidates clamp back onto the fissure. Both tanks replace it below - for them X is the axis that
    // swings a cone into the raid.
    std::array<std::pair<float, float>, 4> candidates = { {
        { fx + step, stepY },
        { fx - step, stepY },
        { bot->GetPositionX(), fy + step },
        { bot->GetPositionX(), fy - step },
    } };

    if (botAI->IsMainTank(bot))
    {
        // He is the one bot whose dodge turns the boss, and X is the axis that turns him into the raid:
        // a 10yd step east off his left hold swings the 60yd frontal cone from 138 degrees round to 66,
        // straight down the raid line at 57. South keeps both cones off it and puts him back on his
        // hold X on the way; north is no better than east, because PlatformMinX steps from 3220 to 3227
        // at Y 520 and turns the step into a 5.6yd move east.
        //
        // With a wave in the air he does not dodge at all. His safe band is 15yd on the left hold and
        // 5yd on the right, so no 10yd step fits inside either, and one Void Blast on a tank is a better
        // trade than a frontal cone on the whole ranged line. Returning false hands the tick down to the
        // corridor dodge, which owns his Y anyway.
        //
        // The X pair below is kept as the tail. Between waves he is always on the left hold, and the two
        // Y candidates are 20yd apart with 10yd of clearance each, so both only fail together for a
        // fissure that pins him against the southern rim.
        if (wave != TsunamiWave::None)
            return false;

        float const holdX = TankHoldX(bot);
        candidates = { {
            { holdX, fy - step },
            { holdX, fy + step },
            { fx + step, stepY },
            { fx - step, stepY },
        } };
    }
    else if (IsOffTank(bot))
    {
        // His position is the drake's facing, and the 15yd Shadow Breath points at empty platform only
        // while he stands where the spot puts him. Shadron decides it: held from the west at
        // (3228.94, 534.66) the drake faces away, and a 10yd step east turns it onto the raid line,
        // whose west end is 10.2yd off on the same Y. A Y step turns it across the line instead.
        //
        // The X pair is kept as the tail, for a fissure that pins him against a rim. Both Y candidates
        // have to clamp or fall in a lane before it is reached.
        candidates = { {
            { bot->GetPositionX(), fy - step },
            { bot->GetPositionX(), fy + step },
            { fx + step, stepY },
            { fx - step, stepY },
        } };
    }

    float bestX = 0.0f;
    float bestY = 0.0f;
    float bestClearance = -1.0f;
    for (auto& candidate : candidates)
    {
        float x = candidate.first;
        float y = candidate.second;
        ClampDestination(x, y);

        // Only the off-tank's Y pair can fail this. The X candidates carry stepY, which is wave-clear by
        // construction, and the main tank does not dodge at all while a wave is up.
        if (!WaveClearsY(y, wave))
            continue;

        float const clearance = std::sqrt((x - fx) * (x - fx) + (y - fy) * (y - fy));
        if (clearance >= FISSURE_CLEAR_RADIUS)
            return MoveToClamped(x, y, 0.0f);

        if (clearance > bestClearance)
        {
            bestClearance = clearance;
            bestX = x;
            bestY = y;
        }
    }

    if (bestClearance < 0.0f)
        return false;

    return MoveToClamped(bestX, bestY, 0.0f);
}

bool OsMainTankHoldAction::Execute(Event /*event*/)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    // Gated on the victim rather than on the "current target" AI value. That value can never hold a
    // friendly - AttackAction::Attack bails on IsFriendlyTo long before it writes it - but the
    // client-side selection is overwritten by every buff the tank casts on a raid member, and keying
    // off the AI value means the boss is never re-selected afterwards. Acquiring must not cost the
    // tick either, so this falls through to the move rather than returning.
    if (bot->GetVictim() != boss)
        Attack(boss);
    else if (bot->GetTarget() != boss->GetGUID())
        bot->SetSelection(boss->GetGUID());

    // The tank walks Sartharion off his home spot down to the entrance end and holds him there,
    // dodging along Y with everyone else. He follows on threat, and there is no leash to hit:
    // Creature::_IsTargetAcceptable skips the home-distance check outright inside a dungeon.
    float x = TankHoldX(bot);
    float y = SafeCorridorY(bot);

    if (!MainTankDragDone(boss))
    {
        // The one-shot pull drag into the southern tip. Sartharion stops chasing 20.83yd short of the
        // tank, so holding a corridor from the start parks him wherever that ray happens to end,
        // aimed diagonally through the raid. Overshooting to the corner once puts him at
        // (3228.5, 504.7), inside his reach from both corridor holds.
        //
        // Deliberately not clamped: this is a hand-measured position past the south edge of the
        // platform box, and it is already inside the Range Marker radius, so both clamps would only
        // damage it.
        uint32 const now = getMSTime();
        uint32 dragStartedMs = MainTankDragStartedMs(boss);
        if (!dragStartedMs)
        {
            dragStartedMs = now;
            SetMainTankDragStartedMs(boss, dragStartedMs);
        }

        uint32 arrivedMs = MainTankDragArrivedMs(boss);
        bool const onPoint =
            bot->GetExactDist2d(MAIN_TANK_DRAG_X, MAIN_TANK_DRAG_Y) <= MAIN_TANK_DRAG_TOLERANCE;

        if (!onPoint)
            arrivedMs = 0;
        else if (!arrivedMs)
            arrivedMs = now;

        SetMainTankDragArrivedMs(boss, arrivedMs);

        // Two ways out. The normal one is the full dwell with the boss in melee range - the tank gets
        // to the corner in about 3s and the boss needs 7-9s to cross the room, so arrival on its own
        // proves nothing, and neither does the dwell if a player is holding threat and the boss never
        // came. The timeout is the bail for exactly that case, and waiting past it is worse than an
        // imperfect facing: waves start at 20s and the corner is lethal under a left one.
        bool const dwellDone = arrivedMs && getMSTimeDiff(arrivedMs, now) >= MAIN_TANK_DRAG_DWELL_MS &&
                               boss->GetExactDist2d(bot) <= bot->GetMeleeRange(boss);
        bool const timedOut = getMSTimeDiff(dragStartedMs, now) > MAIN_TANK_DRAG_TIMEOUT_MS;

        if (dwellDone || timedOut)
        {
            // The corner is a hand-measured position past where the navmesh floor starts, so the drag
            // failing and MoveTo silently refusing the destination look identical from in game. One
            // line, once per pull, tells them apart.
            if (!dwellDone)
            {
                LOG_WARN("playerbots",
                         "Obsidian Sanctum: main tank gave up the pull drag at ({:.1f}, {:.1f}), "
                         "{:.1f}yd short of the corner",
                         bot->GetPositionX(), bot->GetPositionY(),
                         bot->GetExactDist2d(MAIN_TANK_DRAG_X, MAIN_TANK_DRAG_Y));
            }

            SetMainTankDragged(boss);
            return false;
        }

        if (onPoint)
            return false;

        return MoveTo(OS_MAP_ID, MAIN_TANK_DRAG_X, MAIN_TANK_DRAG_Y, bot->GetPositionZ(), false,
                      false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    ClampDestination(x, y);

    if (FissureBlocks(bot, x, y))
        return false;

    if (bot->GetExactDist2d(x, y) <= CorridorToleranceFor(bot))
        return false;

    return MoveTo(OS_MAP_ID, x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool OsDrakeLandingPositionAction::Execute(Event /*event*/)
{
    Unit* drake = FindInboundDrake(bot);
    if (!drake)
        return false;

    Position const* landing = LandingPositionFor(drake->GetEntry());
    if (!landing)
        return false;

    // The off-tank waits on the spot he will hold the drake from, so it faces the right way from the
    // moment it touches down rather than being crossed over afterwards. Melee meet it east of the
    // touchdown point and never west: a drake stops at melee range of whoever it is coming for and
    // faces them, so arriving from the east aims the 15yd Shadow Breath cone at empty platform instead
    // of through the raid. They then get arced off the front by "rear flank" once it is their target.
    Position const* spot = IsOffTank(bot) ? DrakeTankSpotFor(drake->GetEntry()) : nullptr;

    float x = spot ? spot->GetPositionX() : landing->GetPositionX() + OFFTANK_EAST_OFFSET;

    // Every landing coord, and every tank spot, sits inside a lethal band under one wave pattern or
    // both, so a wave that can reach this Y overrides it outright.
    float y = spot ? spot->GetPositionY() : landing->GetPositionY();
    if (!WaveClearsY(y, ClassifyTsunamiWave(bot)))
        y = SafeCorridorY(bot);

    return MoveToClamped(x, y);
}

bool OsOffTankHoldAction::Execute(Event /*event*/)
{
    // Ahead of everything else, because the one tick this is in range is the tick he sets off: he
    // attacks the newest drake only, and a handover walks him 35.5yd away from the one he was holding.
    if (Unit* stray = OffTankTauntTarget(bot))
    {
        char const* taunt = TauntSpellFor(bot);
        if (taunt && botAI->CanCastSpell(taunt, stray) && botAI->CastSpell(taunt, stray))
            return true;
    }

    Unit* primary = OffTankChargeFor(bot);

    if (primary)
    {
        // Acquiring the target must not cost the tick. Returning here is what let the off-tank chase
        // a drake to wherever it stood - including the perch it sits on before it is called - instead
        // of taunting it from the anchor and letting it come.
        if (bot->GetVictim() != primary)
            Attack(primary);
        else if (bot->GetTarget() != primary->GetGUID())
            bot->SetSelection(primary->GetGUID());
    }
    else
    {
        // Nothing to hold, so let go of Sartharion. The multiplier blocks re-acquiring him but cannot
        // drop a target already picked up, and AttackStop on its own is not enough: it clears the swing
        // and leaves "current target" set, which is what the class rotation casts at and what
        // ReachTargetAction walks to. He picks the boss up in the first place because every trigger
        // here needs him in combat, so the pull itself still runs on generic tank behaviour.
        Unit* boss = GetSartharion(bot);
        if (boss && (bot->GetVictim() == boss || AI_VALUE(Unit*, "current target") == boss))
        {
            bot->AttackStop();
            context->GetValue<Unit*>("current target")->Set(nullptr);
        }
    }

    // A drake is held beside where it lands, a Lava Blaze or a whelp is dragged to the east end past
    // the raid, and with nothing at all he waits on Sartharion's empty spawn spot - the east end is a
    // few yards off the rim and there is no reason to stand there through the first 20s, or through
    // any gap between adds.
    Position const anchor = OffTankAnchor(bot);
    float x = anchor.GetPositionX();
    float y = anchor.GetPositionY();
    ClampDestination(x, y);

    if (FissureBlocks(bot, x, y))
        return false;

    if (bot->GetExactDist2d(x, y) <= CORRIDOR_ARRIVAL_TOLERANCE)
        return false;

    return MoveTo(OS_MAP_ID, x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool OsSartharionFlankAction::Execute(Event /*event*/)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    float const reach = std::max(bot->GetMeleeRange(boss) - FLANK_STAND_BACK, 1.0f);

    // Melee hold the northern flank, and the southern one is only the fallback. Both are equally safe
    // from the cones, so the side has to be picked on something else, and picking it on which survived
    // the platform clamp better - the old rule - is not a side at all: it follows the boss's facing, so
    // it hands melee the north flank while the tank is on his left hold and the south one the moment a
    // right wave swaps him over.
    float const bearing = boss->GetOrientation() + static_cast<float>(M_PI) / 2.0f;
    std::pair<float, float> north = { boss->GetPositionX() + reach * std::cos(bearing),
                                      boss->GetPositionY() + reach * std::sin(bearing) };
    // The other flank is this one reflected through him, which is exact and saves a second sin/cos.
    std::pair<float, float> south = { 2.0f * boss->GetPositionX() - north.first,
                                      2.0f * boss->GetPositionY() - north.second };

    if (south.second > north.second)
        std::swap(north, south);

    TsunamiWave const wave = ClassifyTsunamiWave(bot);

    for (auto const& flank : { north, south })
    {
        float x = flank.first;
        float y = flank.second;

        // The flank keeps X and a wave takes Y, so nothing pins melee sideways for a mechanic that
        // only kills along Y. His northern flank lands around Y 518.7, 5.3yd off the left line at 524,
        // so without this it walked them back into the wave the corridor dodge had just cleared.
        if (!WaveClearsY(y, wave))
            y = SafeCorridorY(bot);

        ClampDestination(x, y);

        // Re-checked after clamping: pulling the point back onto the platform can swing it into a
        // cone, and moving into one is worse than standing still.
        Position const candidate(x, y, bot->GetPositionZ(), 0.0f);
        if (InSartharionCone(boss, candidate))
            continue;

        return MoveToClamped(x, y, CORRIDOR_ARRIVAL_TOLERANCE);
    }

    return false;
}

bool OsDrakeRearAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVictim();
    if (!drake || !IsDrakeEntry(drake->GetEntry()))
        return false;

    float const reach = std::max(bot->GetMeleeRange(drake) - FLANK_STAND_BACK, 1.0f);

    // Drawn off straight back rather than on it, so a pack of melee spreads across the tail instead of
    // stacking on one coordinate. The mirror is the fallback: Vesperon is held 2yd from the east rim,
    // so one side of the tail can clamp back out of the band.
    float const spread = frand(0.0f, DRAKE_REAR_DRAW_DEGREES) * static_cast<float>(M_PI) / 180.0f;
    float const rear = drake->GetOrientation() + static_cast<float>(M_PI);
    TsunamiWave const wave = ClassifyTsunamiWave(bot);

    for (float const bearing : { rear + spread, rear - spread })
    {
        float x = drake->GetPositionX() + reach * std::cos(bearing);
        float y = drake->GetPositionY() + reach * std::sin(bearing);

        // X keeps tracking the drake and only Y gives way. Without this the corridor dodge and this
        // action alternated for the whole 11s a wave was up: the dodge moves Y and then releases the
        // tick on its own duplicate-move guard, and this walked them straight back into the lane.
        if (!WaveClearsY(y, wave))
            y = SafeCorridorY(bot);

        ClampDestination(x, y);

        // The rear check only bites inside the breath. A wave puts melee well outside it, and the
        // off-tank drags the drake to the same corridor Y, so they are on its tail again as it lands.
        Position const candidate(x, y, bot->GetPositionZ(), 0.0f);
        if (drake->GetExactDist2d(x, y) <= DRAKE_BREATH_RANGE && !BehindDrake(drake, candidate))
            continue;

        return MoveToClamped(x, y, CORRIDOR_ARRIVAL_TOLERANCE);
    }

    return false;
}

bool OsTankShapeshiftAction::Execute(Event /*event*/)
{
    // Dire bear first: it is the same form with more armour, and the class strategy falls back the
    // same way for a druid who has not learned it.
    for (char const* form : { "dire bear form", "bear form" })
    {
        if (botAI->CanCastSpell(form, bot) && botAI->CastSpell(form, bot))
            return true;
    }
    return false;
}

bool OsReturnToPlatformAction::Execute(Event /*event*/)
{
    // No tolerance anywhere below: this only ever runs on a bot that is already off the arena, so
    // there is no "close enough" to settle for.
    //
    // The off-tank goes to both of his anchor's axes rather than to its X on the corridor Y. His hold
    // is a drake spot, so taking the corridor Y here only bought a second walk north once he was back
    // on the platform.
    if (IsOffTank(bot))
    {
        Position const anchor = OffTankAnchor(bot);
        return MoveToClamped(anchor.GetPositionX(), anchor.GetPositionY(), 0.0f);
    }

    float const x = botAI->IsMainTank(bot) ? TankHoldX(bot) : RaidLineX(bot);
    return MoveToClamped(x, SafeCorridorY(bot), 0.0f);
}

bool OsRaidHoldAction::Execute(Event /*event*/)
{
    float const corridorY = SafeCorridorY(bot);

    // Y first and on its own, keeping X: that is the correction a wave demands, and doing it without
    // touching X means the ranged group does not stampede along the line every time one goes out.
    if (std::abs(bot->GetPositionY() - corridorY) > CORRIDOR_ARRIVAL_TOLERANCE)
        return MoveToClamped(bot->GetPositionX(), corridorY);

    // X is frozen while a wave can still reach this bot, and thaws the moment it is past rather than
    // when it despawns. Re-forming the line is a sideways walk of up to 8yd, and doing it while the
    // group is dodging is what makes a bot look like it is drifting east mid-wave - the dodge above is
    // Y-only for exactly that reason.
    if (ClassifyTsunamiWave(bot) != TsunamiWave::None)
        return false;

    // The slot is 10yd from where a fissure dodge just put this bot, which is more than the line
    // tolerance forgives, so re-forming would land him back on the fissure he stepped off.
    float const lineX = RaidLineX(bot);
    if (FissureBlocks(bot, lineX, corridorY))
        return false;

    return MoveToClamped(lineX, corridorY, RAID_LINE_TOLERANCE_X);
}

Player* OsRedirectThreatAction::GetRedirectTank()
{
    return RedirectTankFor(botAI, bot);
}

bool OsMainTankCooldownAction::Execute(Event /*event*/)
{
    char const* spell = NextTankDefensive(botAI, bot);
    return spell && botAI->CastSpell(spell, bot);
}

Unit* OsRedirectThreatAction::GetThreatDumpTarget()
{
    return PriorityTarget(bot);
}

bool OsTranquilizeEnrageAction::Execute(Event /*event*/)
{
    Unit* target = TranquilizeTargetFor(bot);
    if (!target)
        return false;

    return botAI->CanCastSpell("tranquilizing shot", target) &&
           botAI->CastSpell("tranquilizing shot", target);
}

bool SartharionAttackPriorityAction::Execute(Event /*event*/)
{
    Unit* target = PriorityTarget(bot);

    // Ahead of the early return below, or a pet whose owner is already on the right target never
    // gets the order. Skipped while the owner is shifted: he resolves phase-16 adds his phase-1 pet
    // cannot touch, and leaving the last order standing keeps the pet on the drake it is hitting.
    if (bot->GetGuardianPet() && !HasTwilightShift(bot))
    {
        if (target)
            CommandPetAttack(botAI, target);
        else
            StopPet(botAI);
    }

    if (target && bot->GetVictim() != target)
        return Attack(target);

    return false;
}

bool EnterTwilightPortalAction::Execute(Event /*event*/)
{
    // The only unclamped destinations in this strategy, and they do not need clamping: the in-fight
    // portal is fixed at (3247.29, 529.80), 17yd from the southern Safe Area trigger and well inside
    // its Range Marker.
    GameObject* portal = bot->FindNearestGameObject(GoId::TwilightPortal, 100.0f);
    if (!portal)
        return false;

    // The portal is a fixed coordinate 2.2yd off the left wave line at 532, so a bot standing on clear
    // ground will still walk into a lane to click it. Reading the wave against the bot's own X is enough:
    // one that has passed the bot runs at 12 yd/s against its 7, so it sweeps the ground in between and
    // is gone before the bot gets there.
    if (!WaveClearsY(portal->GetPositionY(), ClassifyTsunamiWave(bot)))
        return false;

    if (!portal->IsAtInteractDistance(bot))
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f));

    WorldPacket data(CMSG_GAMEOBJ_USE);
    data << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);

    return true;
}

bool ExitTwilightPortalAction::Execute(Event /*event*/)
{
    GameObject* portal = bot->FindNearestGameObject(GoId::NormalPortal, 100.0f);
    if (!portal)
        return false;

    // Same coordinate as the way in, and the same lane. The classification reaches through the phase wall
    // for this one - a shifted bot borrows the eyes of a groupmate still on the platform.
    if (!WaveClearsY(portal->GetPositionY(), ClassifyTsunamiWave(bot)))
        return false;

    if (!portal->IsAtInteractDistance(bot))
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f));

    WorldPacket data(CMSG_GAMEOBJ_USE);
    data << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);

    return true;
}
