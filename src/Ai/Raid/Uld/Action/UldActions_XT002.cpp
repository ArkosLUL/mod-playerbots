#include "UldActions_XT002.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Group.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

bool XT002MoveClearAction::isPossible() { return bot->CanFreeMove(); }

bool XT002MoveClearAction::MoveClearOf(std::vector<std::pair<Unit*, float>> const& avoid)
{
    if (avoid.empty())
        return false;

    // Scored by the worst shortfall against any one unit's own clearance, capped at zero: every spot
    // that clears the whole list scores the same, so the first (nearest) one wins and the bot does not
    // run further than the mechanic needs.
    auto score = [&avoid](float x, float y)
    {
        float worst = std::numeric_limits<float>::max();
        for (auto const& entry : avoid)
            worst = std::min(worst, entry.first->GetExactDist2d(x, y) - entry.second);

        return std::min(worst, 0.0f);
    };

    float searchRadius = 0.0f;
    for (auto const& entry : avoid)
        searchRadius = std::max(searchRadius, entry.second);

    searchRadius += 5.0f;

    int const directions = 8;
    float const increment = 3.0f;
    float bestX = 0.0f;
    float bestY = 0.0f;
    float bestZ = 0.0f;
    bool found = false;

    // A packed 25-man often leaves no spot inside the search ring that clears everyone. Starting from
    // the bot's own score means any improvement is taken instead - most of the raid still gets out of
    // the splash - and only a move that gains nothing is rejected.
    float bestScore = score(bot->GetPositionX(), bot->GetPositionY());

    for (int i = 0; i < directions; ++i)
    {
        float const angle = (i * 2 * M_PI) / directions;
        for (float distance = increment; distance <= searchRadius; distance += increment)
        {
            float const moveX = bot->GetPositionX() + distance * cos(angle);
            float const moveY = bot->GetPositionY() + distance * sin(angle);
            float const moveZ = bot->GetPositionZ();

            float const candidate = score(moveX, moveY);
            if (candidate > bestScore && bot->IsWithinLOS(moveX, moveY, moveZ))
            {
                bestScore = candidate;
                bestX = moveX;
                bestY = moveY;
                bestZ = moveZ;
                found = true;
            }
        }
    }

    if (!found)
        return false;

    return MoveTo(bot->GetMapId(), bestX, bestY, bestZ, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool XT002DebuffCarrierAction::ApproachIsClear(float x, float y, std::list<Creature*> const& voidZones) const
{
    float const fromX = bot->GetPositionX();
    float const fromY = bot->GetPositionY();
    float const runX = x - fromX;
    float const runY = y - fromY;
    float const runLengthSq = runX * runX + runY * runY;

    for (Creature* voidZone : voidZones)
    {
        float projection = 0.0f;
        if (runLengthSq > 0.0f)
        {
            projection = ((voidZone->GetPositionX() - fromX) * runX +
                          (voidZone->GetPositionY() - fromY) * runY) / runLengthSq;
            projection = std::max(0.0f, std::min(1.0f, projection));
        }

        if (voidZone->GetExactDist2d(fromX + runX * projection, fromY + runY * projection) <
            ULDUAR_XT002_BOMB_APPROACH_CLEARANCE)
        {
            return false;
        }
    }

    return true;
}

bool XT002DebuffCarrierAction::InsideParkingLot() const
{
    Position const& origin = botAI->IsMelee(bot) ? ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE
                                                 : ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED;

    // The grid is axis-aligned, so its extent is a rectangle. The margin is the re-engage band, which
    // is what the carrier already tolerates as "on a cell".
    float const margin = ULDUAR_XT002_BOMB_CELL_REENGAGE;
    float const minX = origin.GetPositionX() - margin;
    float const maxX = origin.GetPositionX() + (ULDUAR_XT002_BOMB_GRID_X_CELLS - 1) * ULDUAR_XT002_BOMB_GRID_STEP + margin;
    float const maxY = origin.GetPositionY() + margin;
    float const minY = origin.GetPositionY() - (ULDUAR_XT002_BOMB_GRID_Y_CELLS - 1) * ULDUAR_XT002_BOMB_GRID_STEP - margin;

    return bot->GetPositionX() >= minX && bot->GetPositionX() <= maxX && bot->GetPositionY() >= minY &&
           bot->GetPositionY() <= maxY;
}

float XT002DebuffCarrierAction::TravelReach(uint32 remainingMs) const
{
    if (remainingMs <= ULDUAR_XT002_BOMB_TRAVEL_MARGIN_MS)
        return 0.0f;

    return bot->GetSpeed(MOVE_RUN) * (remainingMs - ULDUAR_XT002_BOMB_TRAVEL_MARGIN_MS) / 1000.0f;
}

bool XT002DebuffCarrierAction::StopShortOf(float cellX, float cellY, float reach)
{
    float const runX = cellX - bot->GetPositionX();
    float const runY = cellY - bot->GetPositionY();
    float const runLength = std::sqrt(runX * runX + runY * runY);
    if (runLength <= 0.0f || reach <= ULDUAR_XT002_BOMB_CELL_ARRIVED)
        return false;

    float const scale = std::min(reach, runLength) / runLength;
    float const stopX = bot->GetPositionX() + runX * scale;
    float const stopY = bot->GetPositionY() + runY * scale;
    float const stopZ = bot->GetPositionZ();

    if (!bot->IsWithinLOS(stopX, stopY, stopZ))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Worth stopping here only if the expiry burst misses the raid from it. The pull matters more
    // than the damage: 20 yd of yank drops whoever it catches into the puddle that just landed.
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        if (member->GetExactDist2d(stopX, stopY) < ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS)
            return false;
    }

    // Same reason ParkVoidZone ignores it: MoveTo goes false while the bot is already walking to this
    // exact point, and reading that would hand the tick to the dynamic search mid-run.
    MoveTo(bot->GetMapId(), stopX, stopY, stopZ, false, false, false, false,
           MovementPriority::MOVEMENT_COMBAT, true);
    return true;
}

XT002DebuffCarrierAction::ParkResult XT002DebuffCarrierAction::ParkVoidZone(Unit* boss, float reach,
                                                                           float& cellX, float& cellY)
{
    Position const& origin = botAI->IsMelee(bot) ? ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE
                                                 : ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED;
    float const originX = origin.GetPositionX();
    float const originY = origin.GetPositionY();
    float const originZ = origin.GetPositionZ();

    std::list<Creature*> voidZones;
    boss->GetCreatureListWithEntryInGrid(voidZones, PB_NPC_XT002_VOID_ZONE, ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS);

    // Nearest qualifying cell rather than the first in index order. The grid runs away from the raid,
    // so index order always picked the furthest cell - 44yd for melee, against a debuff that lasts 9s.
    float bestX = 0.0f;
    float bestY = 0.0f;
    float bestDistance = 0.0f;
    int bestRank = 0;
    bool found = false;

    for (int cell = 0; cell < ULDUAR_XT002_BOMB_GRID_X_CELLS * ULDUAR_XT002_BOMB_GRID_Y_CELLS; ++cell)
    {
        float const candidateX = originX + (cell % ULDUAR_XT002_BOMB_GRID_X_CELLS) * ULDUAR_XT002_BOMB_GRID_STEP;
        float const candidateY = originY - (cell / ULDUAR_XT002_BOMB_GRID_X_CELLS) * ULDUAR_XT002_BOMB_GRID_STEP;

        // Room geometry is script-summoned rather than spawned, so the grid cannot be checked against
        // the map offline - the LOS test is what keeps a cell behind a wall from being picked.
        if (!bot->IsWithinLOS(candidateX, candidateY, originZ))
            continue;

        // Measured against where the puddles actually are rather than the cells they were aimed at, so
        // a drop that landed off-centre blocks whatever it is really near.
        float nearestVoidZone = std::numeric_limits<float>::max();
        for (Creature* voidZone : voidZones)
            nearestVoidZone = std::min(nearestVoidZone, voidZone->GetExactDist2d(candidateX, candidateY));

        if (nearestVoidZone < ULDUAR_XT002_VOID_ZONE_RADIUS)
            continue;

        float const distance = bot->GetExactDist2d(candidateX, candidateY);

        // Reachable first, then room, then a clear approach, then the shortest walk. Reachable leads
        // because a cell the bomb goes off before the bot arrives at is worth nothing at all, however
        // roomy - the lot runs 30-50 yd out against a 9s debuff, so this is a real gate and not a
        // formality. Room comes next: standing in Consumption costs the whole debuff, where crossing
        // a puddle on the way costs a second or two.
        bool const reachable = distance + ULDUAR_XT002_BOMB_CELL_ARRIVED <= reach;
        int const rank = (reachable ? 4 : 0) +
                         (nearestVoidZone >= ULDUAR_XT002_BOMB_CELL_PREFERRED_CLEARANCE ? 2 : 0) +
                         (ApproachIsClear(candidateX, candidateY, voidZones) ? 1 : 0);

        if (found && (rank < bestRank || (rank == bestRank && distance >= bestDistance)))
            continue;

        bestX = candidateX;
        bestY = candidateY;
        bestDistance = distance;
        bestRank = rank;
        found = true;
    }

    if (!found)
    {
        parked = false;
        return ParkResult::None;
    }

    cellX = bestX;
    cellY = bestY;

    // Nothing in the lot is close enough. A parked bot never lands here - it is standing on its cell,
    // and one whose cell was just taken by its own puddle is a single grid step from the next.
    if (bestRank < 4)
    {
        parked = false;
        return ParkResult::OutOfReach;
    }

    // Hold through drift once parked instead of re-issuing. The re-engage band is under one grid step,
    // so a cell taken by a fresh puddle still drops the latch and sends the carrier to the next one.
    if (parked && bestDistance <= ULDUAR_XT002_BOMB_CELL_REENGAGE)
    {
        if (bot->isMoving())
            bot->StopMoving();

        return ParkResult::Parked;
    }

    parked = false;

    if (bestDistance <= ULDUAR_XT002_BOMB_CELL_ARRIVED)
    {
        parked = true;
        if (bot->isMoving())
            bot->StopMoving();

        return ParkResult::Parked;
    }

    // Deliberately not MoveTo's return value: it goes false while the bot is already walking to this
    // exact cell, and Execute would read that as "nowhere to park" and hand the tick to the dynamic
    // search, which turns the carrier around mid-run.
    MoveTo(bot->GetMapId(), bestX, bestY, originZ, false, false, false, false,
           MovementPriority::MOVEMENT_COMBAT, true);
    return ParkResult::Moving;
}

bool XT002DebuffCarrierAction::Execute(Event /*event*/)
{
    bool const hasBomb = bot->HasAura(GetXT002GravityBombSpellId(bot));
    bool const hasSearingLight = bot->HasAura(GetXT002SearingLightSpellId(bot));
    if (!hasBomb && !hasSearingLight)
    {
        parked = false;
        return false;
    }

    // Void Zones only drop once XT carries Heartbreak, so before then there is nothing to park and a
    // bomb carrier just needs to be somewhere the splash misses - which keeps melee uptime.
    bool const heartbreak = IsXT002HeartbreakActive(botAI);

    // The lot also owns a bot that is only carrying Searing Light while it is standing in one: that is
    // a carrier whose bomb has just gone off under its feet, and it has to step off its own puddle
    // without walking the splash back through the raid to reach the Searing Light spot.
    bool const steppingOffOwnBomb = !hasBomb && heartbreak && InsideParkingLot();

    if (heartbreak && (hasBomb || steppingOffOwnBomb))
    {
        // Only a bot actually holding the bomb runs against a clock. Stepping off one's own puddle is
        // a single grid step with nothing about to go off, so it is allowed the whole lot.
        float reach = std::numeric_limits<float>::max();
        if (hasBomb)
            if (Aura* bomb = bot->GetAura(GetXT002GravityBombSpellId(bot)))
                reach = TravelReach(std::max(0, bomb->GetDuration()));

        if (Unit* boss = GetXT002(botAI))
        {
            float cellX = 0.0f;
            float cellY = 0.0f;
            switch (ParkVoidZone(boss, reach, cellX, cellY))
            {
                case ParkResult::Moving:
                    return true;
                // Standing on the cell with movement stopped. Yielding the tick is what lets a healer
                // carrier keep healing and a hunter keep shooting for the rest of the debuff.
                case ParkResult::Parked:
                    return false;
                case ParkResult::OutOfReach:
                    if (StopShortOf(cellX, cellY, reach))
                        return true;
                    break;
                case ParkResult::None:
                    break;
            }
        }

        // Every cell is taken, since reach is unlimited without a bomb. A bot that is only here
        // because its own bomb went off holds position - the splash expires over an empty lot
        // wherever in it the bot stands, and walking it back through the raid to reach the Searing
        // Light spot is what this node exists to prevent.
        if (steppingOffOwnBomb)
            return false;
    }
    else
    {
        parked = false;
    }

    if (!hasBomb)
    {
        // The main tank stays on XT whatever it is carrying, and an off-tank stays on its Pummeller:
        // dragging the boss or dropping an add to dodge a splash costs the raid far more than the
        // splash does.
        if (botAI->IsTank(bot))
            return false;

        if (bot->GetExactDist(ULDUAR_XT002_SEARING_LIGHT_SPOT) < 1.0f)
            return false;

        return MoveTo(bot->GetMapId(), ULDUAR_XT002_SEARING_LIGHT_SPOT.GetPositionX(),
                      ULDUAR_XT002_SEARING_LIGHT_SPOT.GetPositionY(),
                      ULDUAR_XT002_SEARING_LIGHT_SPOT.GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT, true);
    }

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<std::pair<Unit*, float>> allies;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        allies.emplace_back(member, ULDUAR_XT002_DEBUFF_CLEAR_RADIUS);
    }

    return MoveClearOf(allies);
}

bool XT002AvoidHazardAction::Execute(Event /*event*/)
{
    // A carrier's puddles are the carrier action's business - it is what picks the cell and what walks
    // one off its own bomb, and this search would undo both. Boombots still apply to everyone.
    bool const carryingDebuff = bot->HasAura(GetXT002SearingLightSpellId(bot)) ||
                                bot->HasAura(GetXT002GravityBombSpellId(bot));

    std::vector<std::pair<Unit*, float>> hazards;
    GuidVector const& npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        // Ranged already stand outside the blast, so pulling them out too would only break their casts.
        if (unit->GetEntry() == PB_NPC_XT002_BOOMBOT && botAI->IsMelee(bot))
            hazards.emplace_back(unit, ULDUAR_XT002_BOOMBOT_AVOID_RADIUS);
        else if (unit->GetEntry() == PB_NPC_XT002_VOID_ZONE && !carryingDebuff)
            hazards.emplace_back(unit, ULDUAR_XT002_VOID_ZONE_RADIUS);
    }

    return MoveClearOf(hazards);
}

bool XT002PummellerTauntAction::Execute(Event /*event*/)
{
    Unit* pummeller = GetXT002EngageableAdd(botAI, bot, PB_NPC_XT002_PUMMELLER, ULDUAR_XT002_TAUNT_RANGE);
    if (!pummeller)
        return false;

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            return botAI->CastSpell("taunt", pummeller);
        case CLASS_PALADIN:
            return botAI->CastSpell("hand of reckoning", pummeller);
        case CLASS_DEATH_KNIGHT:
            return botAI->CastSpell("dark command", pummeller);
        case CLASS_DRUID:
            return botAI->CastSpell("growl", pummeller);
        default:
            return false;
    }
}

bool XT002RedirectThreatAction::isUseful()
{
    return bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_ROGUE;
}

Player* XT002RedirectThreatAction::GetRedirectTank()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    // A Pummeller is the off-tank's job, and it is the add most likely to peel onto a redirecter. One
    // still out at a toy pile is nobody's, so the leash decides whether there is really one in play.
    if (GetXT002EngageableAdd(botAI, bot, PB_NPC_XT002_PUMMELLER, ULDUAR_XT002_ADD_LEASH_RADIUS))
    {
        if (Player* assistTank = GetGroupAssistTank(botAI, bot, 0))
            return assistTank;
    }

    // Otherwise feed whoever is actually holding XT, which survives a tank swap or a tank death.
    Unit* xt002 = GetXT002(botAI);
    if (xt002)
    {
        if (Unit* victim = xt002->GetVictim())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !member->IsAlive() || member == bot)
                    continue;

                if (botAI->IsTank(member) && member == victim)
                    return member;
            }
        }
    }

    return GetGroupMainTank(botAI, bot);
}

bool XT002RedirectThreatAction::Execute(Event /*event*/)
{
    Player* tank = GetRedirectTank();
    if (!tank || tank == bot)
        return false;

    if (bot->getClass() == CLASS_ROGUE)
    {
        // Tricks redirects everything the rogue does for the next 6s, so there is no dump shot.
        return botAI->CanCastSpell("tricks of the trade", tank) && botAI->CastSpell("tricks of the trade", tank);
    }

    if (botAI->CanCastSpell("misdirection", tank))
        return botAI->CastSpell("misdirection", tank);

    // Misdirection only moves the threat of the next three shots, so spend them on XT rather than
    // leaving them to whatever the rotation picks - and never on an add the tank does not want.
    Unit* xt002 = GetXT002(botAI);
    if (xt002 && !IsXT002Submerged(botAI) && bot->HasAura(SPELL_MISDIRECTION) &&
        botAI->CanCastSpell("steady shot", xt002))
    {
        return botAI->CastSpell("steady shot", xt002);
    }

    return false;
}

bool XT002RaidPositionAction::Execute(Event /*event*/)
{
    if (botAI->IsMainTank(bot))
    {
        if (bot->GetExactDist(ULDUAR_XT002_MAINTANK_SPOT) <= ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE)
            return false;

        return MoveTo(bot->GetMapId(), ULDUAR_XT002_MAINTANK_SPOT.GetPositionX(),
                      ULDUAR_XT002_MAINTANK_SPOT.GetPositionY(), ULDUAR_XT002_MAINTANK_SPOT.GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    }

    if (botAI->IsRangedDps(bot) || botAI->IsHeal(bot))
    {
        // Ranged first, the same order the trigger uses: a spec that answered to both would otherwise
        // be sent here on one band and turned away on the other, burning the tick every time.
        float const tolerance = botAI->IsRangedDps(bot) ? ULDUAR_XT002_RANGED_SPOT_TOLERANCE
                                                        : ULDUAR_XT002_HEALER_SPOT_TOLERANCE;
        if (bot->GetExactDist(ULDUAR_XT002_RANGED_SPOT) <= tolerance)
            return false;

        return MoveTo(bot->GetMapId(), ULDUAR_XT002_RANGED_SPOT.GetPositionX(),
                      ULDUAR_XT002_RANGED_SPOT.GetPositionY(), ULDUAR_XT002_RANGED_SPOT.GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    }

    return false;
}

bool XT002SetDpsPriorityAction::IsAllowedTarget(Unit* unit) const
{
    // The Heart is hidden, not despawned, when its window shuts, and XT is hidden while submerged.
    // Both stay alive, and the core refuses an untargetable unit, so a bot left holding one queues
    // spells that all fail and looks like it is doing nothing.
    if (!unit || !unit->IsAlive() || unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    // How far this bot will travel for an add. Tanks are exempt because this runs over their list too,
    // and the off-tank is supposed to walk to the Pummeller.
    float const reach = botAI->IsMelee(bot) ? ULDUAR_XT002_MELEE_ENGAGE_RANGE
                                            : ULDUAR_XT002_RANGED_ENGAGE_RANGE;
    bool const inReach = botAI->IsTank(bot) || unit->GetExactDist2d(bot) <= reach;

    switch (unit->GetEntry())
    {
        case PB_NPC_XT002_LIFE_SPARK:
            // No leash on this one - it spawns on the Searing Light carrier and chases players rather
            // than XT. Static Charged is a 500yd area aura, so standing further off gains nothing and
            // there is nothing to dodge; it simply has to die, and it walks into the raid by itself.
            return inReach;

        case NPC_XS013_SCRAPBOT:
        case PB_NPC_XT002_PUMMELLER:
            return inReach && IsXT002AddEngageable(botAI, unit);

        case PB_NPC_XT002_BOOMBOT:
            // Melee must never pick one up, and ranged only from outside the blast: closer than that
            // the avoid action should be moving the bot, not this one holding it in place.
            return !botAI->IsMelee(bot) && unit->GetExactDist2d(bot) >= ULDUAR_XT002_BOOMBOT_AVOID_RADIUS &&
                   inReach && IsXT002AddEngageable(botAI, unit);

        case NPC_HEART_OF_DECONSTRUCTOR:
            // Damage only reaches XT while the Heart channels Exposed Heart.
            if (!unit->HasAura(SPELL_XT002_EXPOSED_HEART))
                return false;

            // Only a Life Spark is worth breaking off for: Static Charged is a raid-wide aura that
            // runs until the spark dies, and no amount of distance reduces it.
            if (GetFirstAliveUnitByEntry(botAI, PB_NPC_XT002_LIFE_SPARK))
                return false;

            // Reads the config flag, not the Heartbreak aura the parking code uses. This one states
            // intent, and it has to hold before Heartbreak exists, because breaking the Heart is what
            // creates it.
            if (IsXT002HardModeActive(botAI))
                return true;

            return unit->GetHealthPct() > ULDUAR_XT002_HEART_SAFE_HP_PCT;

        case NPC_XT002:
            return !IsXT002Submerged(botAI);

        default:
            return true;
    }
}

Unit* XT002SetDpsPriorityAction::SelectByEntry(Unit* currentTarget, uint32 entry,
                                               std::vector<Unit*> const& candidates) const
{
    Unit* selected = nullptr;
    if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == entry)
        selected = currentTarget;

    // Adds come from toy piles on both flanks, so nearest-to-the-bot beats measuring from a raid
    // anchor. The margin stops two similar adds from trading the bot back and forth every tick.
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

std::vector<std::pair<uint32, Unit*>> XT002SetDpsPriorityAction::BuildPriorityList()
{
    Unit* boss = nullptr;
    Unit* heart = nullptr;
    std::vector<Unit*> lifeSparks;
    std::vector<Unit*> scrapbots;
    std::vector<Unit*> boombots;
    std::vector<Unit*> pummellers;

    // One pass over the list every other XT-002 trigger already forces, so this adds no grid work.
    GuidVector const& npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_XT002:
                boss = unit;
                break;
            case NPC_HEART_OF_DECONSTRUCTOR:
                heart = unit;
                break;
            case PB_NPC_XT002_LIFE_SPARK:
                lifeSparks.push_back(unit);
                break;
            case NPC_XS013_SCRAPBOT:
                scrapbots.push_back(unit);
                break;
            case PB_NPC_XT002_BOOMBOT:
                boombots.push_back(unit);
                break;
            case PB_NPC_XT002_PUMMELLER:
                pummellers.push_back(unit);
                break;
            default:
                break;
        }
    }

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // A tank is only ever handed the boss. The generic smart tank targeting ranks every add the tank
    // has no aggro on above XT, so a tank left on it walks into the add pile and the boss follows.
    if (botAI->IsTank(bot))
    {
        std::vector<std::pair<uint32, Unit*>> tankPriority;

        // The taunt reaches 30yd and the toy piles are 80yd out, so the tank holding XT never has to
        // walk at the Pummeller - the add comes to him. Only a tank with someone else on the boss
        // chases it.
        if (IsXT002PummellerTank(botAI, bot) && botAI->GetGroupTankNum(bot) > 1)
            tankPriority.emplace_back(PB_NPC_XT002_PUMMELLER,
                                      SelectByEntry(currentTarget, PB_NPC_XT002_PUMMELLER, pummellers));

        // XT is submerged for the whole Heart window, so in hard mode the tanks have nothing else to
        // hit and the 30s burn needs every source of damage. In normal mode a tank hit is exactly what
        // would flip the raid into hard mode by accident, so they stay off it there.
        if (IsXT002HardModeActive(botAI))
            tankPriority.emplace_back(NPC_HEART_OF_DECONSTRUCTOR, heart);

        tankPriority.emplace_back(NPC_XT002, boss);
        return tankPriority;
    }

    // Life Sparks chain Static Charged through the raid and Scrapbots heal XT back up if they reach
    // him; a Boombot only costs damage, and a Pummeller can simply be tanked. The Heart outranks add
    // DPS because hitting it is what spawns the adds, so a raid that stops for every Scrapbot never
    // gets it down.
    //
    // Hard mode moves it all the way to the front. Every hit on the Heart fires another Energy Orb at
    // a toy pile, so the wave only ends when the Heart does - and the whole mode is decided inside the
    // 30s it stays exposed.
    bool const hardMode = IsXT002HardModeActive(botAI);

    std::vector<std::pair<uint32, Unit*>> priority;
    if (hardMode)
        priority.emplace_back(NPC_HEART_OF_DECONSTRUCTOR, heart);

    priority.emplace_back(PB_NPC_XT002_LIFE_SPARK,
                          SelectByEntry(currentTarget, PB_NPC_XT002_LIFE_SPARK, lifeSparks));
    priority.emplace_back(NPC_XS013_SCRAPBOT, SelectByEntry(currentTarget, NPC_XS013_SCRAPBOT, scrapbots));
    if (!botAI->IsMelee(bot))
        priority.emplace_back(PB_NPC_XT002_BOOMBOT,
                              SelectByEntry(currentTarget, PB_NPC_XT002_BOOMBOT, boombots));
    priority.emplace_back(PB_NPC_XT002_PUMMELLER,
                          SelectByEntry(currentTarget, PB_NPC_XT002_PUMMELLER, pummellers));
    if (!hardMode)
        priority.emplace_back(NPC_HEART_OF_DECONSTRUCTOR, heart);

    priority.emplace_back(NPC_XT002, boss);

    return priority;
}

Unit* XT002SetDpsPriorityAction::ResolveTarget(Unit* currentTarget)
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
    // Scrapbots cannot keep resetting swing and cast timers. Never hold one the gates just rejected:
    // that is what stranded bots on the Heart once its window shut.
    size_t const currentIndex = priorityIndex(currentTarget);
    if (currentIndex < priority.size() && currentIndex <= priorityIndex(target))
        target = currentTarget;

    // No fallback to "dps target": it resolves to whatever add is nearest with no leash of any kind,
    // so it hands straight back the one out at a toy pile that the gates above just rejected. Every
    // unit this encounter cares about is already on the list, so nothing on it means nothing to hit -
    // XT is submerged - and falling through to the anchor is the right answer.
    return target;
}

bool XT002SetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // Ulduar is in RestrictedHealerDPSMaps, so a healer here has no damage node that could use a
    // target - all one does is keep "enemy out of spell" true and walk them at whatever add is
    // furthest out. Declining to hand one over is not enough, because a leftover from the last wave
    // does the same thing. Not the generic "drop target": that also switches the bot to the
    // non-combat engine, which would stop it healing.
    if (botAI->IsHeal(bot))
    {
        if (!currentTarget)
            return false;

        context->GetValue<Unit*>("current target")->Set(nullptr);
        bot->SetTarget(ObjectGuid::Empty);
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
