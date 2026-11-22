#include "UldActions_XT002.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "Group.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "RaidObs.h"
#include "UldData.h"
#include "UldEncounter_XT002.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTargets.h"
#include "Unit.h"

using namespace EncounterHelpers;

bool XT002MoveClearAction::isPossible() { return bot->CanFreeMove(); }

XT002MoveClearAction::MoveIssue XT002MoveClearAction::IssueMove(float x, float y, float z)
{
    switch (TryMoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true))
    {
        case MoveOutcome::NoPath:
            return MoveIssue::NoPath;
        case MoveOutcome::NotAllowed:
            return MoveIssue::Refused;
        default:
            return MoveIssue::Taken;
    }
}

bool XT002MoveClearAction::MoveClearOf(std::vector<std::pair<Unit*, float>> const& avoid)
{
    if (avoid.empty())
        return false;

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

    constexpr float searchMargin = 5.0f;
    searchRadius += searchMargin;

    int const directions = 8;
    float const increment = 3.0f;

    float const standingScore = score(bot->GetPositionX(), bot->GetPositionY());

    struct Candidate
    {
        float x;
        float y;
        float score;
        float distance;
    };

    std::vector<Candidate> candidates;
    for (int i = 0; i < directions; ++i)
    {
        float const angle = (i * 2 * M_PI) / directions;
        for (float distance = increment; distance <= searchRadius; distance += increment)
        {
            float const moveX = bot->GetPositionX() + distance * cos(angle);
            float const moveY = bot->GetPositionY() + distance * sin(angle);

            float const candidate = score(moveX, moveY);
            if (candidate > standingScore)
                candidates.push_back({moveX, moveY, candidate, distance});
        }
    }

    if (candidates.empty())
        return false;

    std::sort(candidates.begin(), candidates.end(), [](Candidate const& left, Candidate const& right)
    {
        if (left.score != right.score)
            return left.score > right.score;
        return left.distance < right.distance;
    });

    size_t const attempts = std::min<size_t>(ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS, candidates.size());
    for (size_t i = 0; i < attempts; ++i)
    {
        switch (IssueMove(candidates[i].x, candidates[i].y, bot->GetPositionZ()))
        {
            case MoveIssue::Taken:
                return true;
            case MoveIssue::Refused:
                return false;
            case MoveIssue::NoPath:
                break;
        }
    }

    return false;
}

float XT002DebuffCarrierAction::ApproachClearance(float x, float y, std::list<Creature*> const& voidZones) const
{
    float const fromX = bot->GetPositionX();
    float const fromY = bot->GetPositionY();
    float const runX = x - fromX;
    float const runY = y - fromY;
    float const runLengthSq = runX * runX + runY * runY;

    float nearest = std::numeric_limits<float>::max();
    for (Creature* voidZone : voidZones)
    {
        float projection = 0.0f;
        if (runLengthSq > 0.0f)
        {
            projection = ((voidZone->GetPositionX() - fromX) * runX +
                          (voidZone->GetPositionY() - fromY) * runY) / runLengthSq;
            projection = std::max(0.0f, std::min(1.0f, projection));
        }

        nearest = std::min(nearest,
                           voidZone->GetExactDist2d(fromX + runX * projection, fromY + runY * projection));
    }

    return nearest;
}

bool XT002DebuffCarrierAction::StandingInVoidZone(std::list<Creature*> const& voidZones) const
{
    for (Creature* voidZone : voidZones)
        if (voidZone->GetExactDist2d(bot) < ULDUAR_XT002_VOID_ZONE_RADIUS)
            return true;

    return false;
}

bool XT002DebuffCarrierAction::InsideParkingLot() const
{
    float const margin = ULDUAR_XT002_BOMB_CELL_REENGAGE;

    for (XT002BombLot const& lot : GetXT002BombLots(bot))
    {
        float const originX = lot.origin.GetPositionX();
        float const originY = lot.origin.GetPositionY();
        float const farY = originY + lot.yDirection * (lot.yCells - 1) * ULDUAR_XT002_BOMB_GRID_STEP;

        float const minX = originX - margin;
        float const maxX = originX + (lot.xCells - 1) * ULDUAR_XT002_BOMB_GRID_STEP + margin;
        float const minY = std::min(originY, farY) - margin;
        float const maxY = std::max(originY, farY) + margin;

        if (bot->GetPositionX() >= minX && bot->GetPositionX() <= maxX && bot->GetPositionY() >= minY &&
            bot->GetPositionY() <= maxY)
            return true;
    }

    return false;
}

float XT002DebuffCarrierAction::TravelReach(uint32 remainingMs) const
{
    if (remainingMs <= ULDUAR_XT002_BOMB_TRAVEL_MARGIN_MS)
        return 0.0f;

    return bot->GetSpeed(MOVE_RUN) * (remainingMs - ULDUAR_XT002_BOMB_TRAVEL_MARGIN_MS) / IN_MILLISECONDS;
}

bool XT002DebuffCarrierAction::StopShortOf(float cellX, float cellY, float reach,
                                           std::list<Creature*> const& voidZones)
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

    for (Creature* voidZone : voidZones)
        if (voidZone->GetExactDist2d(stopX, stopY) < ULDUAR_XT002_VOID_ZONE_RADIUS)
            return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        if (member->GetExactDist2d(stopX, stopY) < ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS)
            return false;
    }

    return IssueMove(stopX, stopY, stopZ) == MoveIssue::Taken;
}

float XT002DebuffCarrierAction::RaidClearance(float x, float y) const
{
    Group* group = bot->GetGroup();
    if (!group)
        return std::numeric_limits<float>::max();

    float nearest = std::numeric_limits<float>::max();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        nearest = std::min(nearest, member->GetExactDist2d(x, y));
    }

    return nearest;
}

bool XT002DebuffCarrierAction::HoldsXT002OrPummeller(Unit* xt002) const
{
    if (xt002 && xt002->GetVictim() == bot)
        return true;

    GuidVector const& npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == PB_NPC_XT002_PUMMELLER && unit->GetVictim() == bot)
            return true;
    }

    return false;
}

bool XT002DebuffCarrierAction::MoveToSearingLightSpot(bool heartbreak)
{
    std::list<Creature*> voidZones;
    if (heartbreak)
        bot->GetCreatureListWithEntryInGrid(voidZones, PB_NPC_XT002_VOID_ZONE, ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS);

    auto clearOfPuddles = [&voidZones](float x, float y)
    {
        for (Creature* voidZone : voidZones)
            if (voidZone->GetExactDist2d(x, y) < ULDUAR_XT002_VOID_ZONE_RADIUS)
                return false;

        return true;
    };

    float const spotX = ULDUAR_XT002_SEARING_LIGHT_SPOT.GetPositionX();
    float const spotY = ULDUAR_XT002_SEARING_LIGHT_SPOT.GetPositionY();
    float const spotZ = ULDUAR_XT002_SEARING_LIGHT_SPOT.GetPositionZ();

    std::vector<Position> const formation = GetXT002OtherFormationSlots(bot);

    auto usable = [&clearOfPuddles, &formation](float x, float y)
    {
        if (!clearOfPuddles(x, y))
            return false;

        for (Position const& slot : formation)
            if (slot.GetExactDist2d(x, y) < ULDUAR_XT002_SEARING_LIGHT_SLOT_CLEARANCE)
                return false;

        return true;
    };

    if (bot->GetExactDist(ULDUAR_XT002_SEARING_LIGHT_SPOT) < 1.0f && usable(spotX, spotY))
        return false;

    struct Candidate
    {
        float x;
        float y;
        float clearance;
    };

    std::vector<Candidate> candidates;
    if (usable(spotX, spotY))
        candidates.push_back({spotX, spotY, RaidClearance(spotX, spotY)});

    int const directions = 8;
    for (int i = 0; i < directions; ++i)
    {
        float const angle = (i * 2 * M_PI) / directions;
        float const ringX = spotX + ULDUAR_XT002_SEARING_LIGHT_DETOUR * cos(angle);
        float const ringY = spotY + ULDUAR_XT002_SEARING_LIGHT_DETOUR * sin(angle);

        if (usable(ringX, ringY))
            candidates.push_back({ringX, ringY, RaidClearance(ringX, ringY)});
    }

    std::sort(candidates.begin(), candidates.end(), [](Candidate const& left, Candidate const& right)
    {
        return left.clearance > right.clearance;
    });

    size_t const attempts = std::min<size_t>(ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS, candidates.size());
    for (size_t i = 0; i < attempts; ++i)
    {
        switch (IssueMove(candidates[i].x, candidates[i].y, spotZ))
        {
            case MoveIssue::Taken:
                return true;
            case MoveIssue::Refused:
                return false;
            case MoveIssue::NoPath:
                break;
        }
    }

    return false;
}

XT002DebuffCarrierAction::ParkResult XT002DebuffCarrierAction::ParkVoidZone(std::list<Creature*> const& voidZones,
                                                                           float reach, float& cellX,
                                                                           float& cellY)
{
    struct Cell
    {
        float x;
        float y;
        float z;
        bool reachable;
        float lane;
        bool roomy;
        float distance;
    };

    std::vector<Cell> cells;
    float nearestDistance = std::numeric_limits<float>::max();

    for (XT002BombLot const& lot : GetXT002BombLots(bot))
    {
        for (int cell = 0; cell < lot.xCells * lot.yCells; ++cell)
        {
            float const candidateX =
                lot.origin.GetPositionX() + (cell % lot.xCells) * ULDUAR_XT002_BOMB_GRID_STEP;
            float const candidateY =
                lot.origin.GetPositionY() + lot.yDirection * (cell / lot.xCells) * ULDUAR_XT002_BOMB_GRID_STEP;

            float nearestVoidZone = std::numeric_limits<float>::max();
            for (Creature* voidZone : voidZones)
                nearestVoidZone = std::min(nearestVoidZone, voidZone->GetExactDist2d(candidateX, candidateY));

            if (nearestVoidZone < ULDUAR_XT002_VOID_ZONE_RADIUS)
                continue;

            float const distance = bot->GetExactDist2d(candidateX, candidateY);

            bool const reachable = distance + ULDUAR_XT002_BOMB_CELL_ARRIVED <= reach;
            float const lane = std::min(ApproachClearance(candidateX, candidateY, voidZones),
                                        ULDUAR_XT002_BOMB_APPROACH_CLEARANCE);
            bool const roomy = nearestVoidZone >= ULDUAR_XT002_BOMB_CELL_PREFERRED_CLEARANCE;

            cells.push_back({candidateX, candidateY, lot.origin.GetPositionZ(), reachable, lane, roomy,
                             distance});
            nearestDistance = std::min(nearestDistance, distance);
        }
    }

    if (cells.empty())
    {
        parked = false;
        return ParkResult::None;
    }

    std::sort(cells.begin(), cells.end(), [](Cell const& left, Cell const& right)
    {
        if (left.reachable != right.reachable)
            return left.reachable;
        if (left.lane != right.lane)
            return left.lane > right.lane;
        if (left.roomy != right.roomy)
            return left.roomy;
        return left.distance < right.distance;
    });

    cellX = cells.front().x;
    cellY = cells.front().y;

    bool const inVoidZone = StandingInVoidZone(voidZones);

    if (parked && !inVoidZone && nearestDistance <= ULDUAR_XT002_BOMB_CELL_REENGAGE)
    {
        if (bot->isMoving())
            bot->StopMoving();

        return ParkResult::Parked;
    }

    parked = false;

    if (!inVoidZone && nearestDistance <= ULDUAR_XT002_BOMB_CELL_ARRIVED)
    {
        parked = true;
        if (bot->isMoving())
            bot->StopMoving();

        return ParkResult::Parked;
    }

    if (!cells.front().reachable)
        return ParkResult::OutOfReach;

    size_t attempts = 0;
    for (Cell const& candidate : cells)
    {
        if (!candidate.reachable || attempts >= ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS)
            break;

        ++attempts;
        switch (IssueMove(candidate.x, candidate.y, candidate.z))
        {
            case MoveIssue::Taken:
                return ParkResult::Moving;
            case MoveIssue::Refused:
                return ParkResult::None;
            case MoveIssue::NoPath:
                break;
        }
    }

    return ParkResult::OutOfReach;
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

    Unit* xt002 = GetXT002(botAI);

    bool const heartbreak = IsXT002HeartbreakActive(bot, xt002);

    bool const steppingOffOwnBomb = !hasBomb && heartbreak && InsideParkingLot();

    std::list<Creature*> voidZones;
    if (heartbreak && xt002)
        xt002->GetCreatureListWithEntryInGrid(voidZones, PB_NPC_XT002_VOID_ZONE,
                                              ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS);

    if (heartbreak && (hasBomb || steppingOffOwnBomb))
    {
        float reach = std::numeric_limits<float>::max();
        if (hasBomb)
            if (Aura* bomb = bot->GetAura(GetXT002GravityBombSpellId(bot)))
                reach = TravelReach(std::max(0, bomb->GetDuration()));

        if (xt002)
        {
            float cellX = 0.0f;
            float cellY = 0.0f;
            switch (ParkVoidZone(voidZones, reach, cellX, cellY))
            {
                case ParkResult::Moving:
                    return true;
                case ParkResult::Parked:
                    return false;
                case ParkResult::OutOfReach:
                    if (StopShortOf(cellX, cellY, reach, voidZones))
                        return true;
                    break;
                case ParkResult::None:
                    break;
            }
        }

        if (steppingOffOwnBomb)
            return false;
    }
    else
    {
        parked = false;
    }

    if (!hasBomb)
    {
        if (PlayerbotAI::IsTank(bot) && (!heartbreak || HoldsXT002OrPummeller(xt002)))
            return false;

        return MoveToSearingLightSpot(heartbreak);
    }

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<std::pair<Unit*, float>> avoid;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        avoid.emplace_back(member, ULDUAR_XT002_DEBUFF_CLEAR_RADIUS);
    }

    for (Creature* voidZone : voidZones)
        avoid.emplace_back(voidZone, ULDUAR_XT002_VOID_ZONE_RADIUS);

    return MoveClearOf(avoid);
}

bool XT002AvoidHazardAction::Execute(Event /*event*/)
{
    bool const carryingDebuff = bot->HasAura(GetXT002SearingLightSpellId(bot)) ||
                                bot->HasAura(GetXT002GravityBombSpellId(bot));

    std::vector<std::pair<Unit*, float>> hazards;
    GuidVector const& npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() == PB_NPC_XT002_BOOMBOT && PlayerbotAI::IsMelee(bot))
            hazards.emplace_back(unit, ULDUAR_XT002_BOOMBOT_AVOID_RADIUS);
        else if (unit->GetEntry() == PB_NPC_XT002_VOID_ZONE && !carryingDebuff)
            hazards.emplace_back(unit, ULDUAR_XT002_VOID_ZONE_RADIUS + ULDUAR_XT002_BOMB_CELL_ARRIVED);
    }

    return MoveClearOf(hazards);
}

bool XT002BombHealAction::Execute(Event /*event*/)
{
    Player* carrier = GetXT002BombHealCarrier(bot);
    float const healRange = sPlayerbotAIConfig.HealDistance;
    if (!carrier || bot->GetExactDist2d(carrier) <= healRange)
        return false;

    std::vector<Player*> const carriers = GetXT002BombCarriers(bot);
    std::list<Creature*> voidZones;
    bot->GetCreatureListWithEntryInGrid(voidZones, PB_NPC_XT002_VOID_ZONE, ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS);

    float const puddleClearance = ULDUAR_XT002_VOID_ZONE_RADIUS + ULDUAR_XT002_BOMB_CELL_ARRIVED;
    float const stand = healRange - ULDUAR_XT002_BOMB_HEAL_MARGIN;
    float const bearing = carrier->GetAngle(bot);

    struct Candidate
    {
        float x;
        float y;
        float distance;
    };

    std::vector<Candidate> candidates;
    for (int step = -ULDUAR_XT002_BOMB_HEAL_BEARING_STEPS; step <= ULDUAR_XT002_BOMB_HEAL_BEARING_STEPS; ++step)
    {
        float const angle = bearing + static_cast<float>(step) * ULDUAR_XT002_BOMB_HEAL_BEARING_STEP_DEG *
                                          static_cast<float>(M_PI) / 180.0f;
        float const x = carrier->GetPositionX() + std::cos(angle) * stand;
        float const y = carrier->GetPositionY() + std::sin(angle) * stand;

        bool const clearOfBombs =
            std::none_of(carriers.begin(), carriers.end(), [x, y](Player* other)
                         { return other->GetExactDist2d(x, y) < ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS; });
        bool const clearOfPuddles =
            std::none_of(voidZones.begin(), voidZones.end(), [x, y, puddleClearance](Creature* voidZone)
                         { return voidZone->GetExactDist2d(x, y) < puddleClearance; });

        if (clearOfBombs && clearOfPuddles)
            candidates.push_back({x, y, bot->GetExactDist2d(x, y)});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](Candidate const& left, Candidate const& right) { return left.distance < right.distance; });

    size_t const attempts = std::min<size_t>(ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS, candidates.size());
    for (size_t i = 0; i < attempts; ++i)
    {
        switch (IssueMove(candidates[i].x, candidates[i].y, bot->GetPositionZ()))
        {
            case MoveIssue::Taken:
                return true;
            case MoveIssue::Refused:
                return false;
            case MoveIssue::NoPath:
                break;
        }
    }

    return false;
}

bool XT002BombCarrierHealAction::Execute(Event /*event*/)
{
    Player* carrier = GetXT002BombHealTarget(bot);
    if (!carrier)
        return false;

    std::vector<char const*> heals;
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            if (!botAI->HasAura("weakened soul", carrier))
                heals.push_back("power word: shield");
            heals.insert(heals.end(), {"penance", "flash heal"});
            if (!botAI->HasAura("renew", carrier, false, true))
                heals.push_back("renew");
            break;
        case CLASS_SHAMAN:
            heals = {"riptide", "healing wave", "lesser healing wave"};
            break;
        case CLASS_PALADIN:
            heals = {"holy shock", "holy light", "flash of light"};
            break;
        case CLASS_DRUID:
            if (!botAI->HasAura("rejuvenation", carrier, false, true))
                heals.push_back("rejuvenation");
            heals.insert(heals.end(), {"swiftmend", "nourish"});
            break;
        default:
            break;
    }

    for (char const* heal : heals)
        if (botAI->CanCastSpell(heal, carrier) && botAI->CastSpell(heal, carrier))
            return true;

    return false;
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

Player* XT002RedirectThreatAction::GetRedirectTank(Unit* xt002)
{
    XT002TankRoles const roles = GetXT002TankRoles(bot, xt002);

    if (GetXT002EngageableAdd(botAI, bot, PB_NPC_XT002_PUMMELLER, ULDUAR_XT002_ADD_LEASH_RADIUS))
        return roles.pummeller;

    return roles.holder;
}

bool XT002RedirectThreatAction::Execute(Event /*event*/)
{
    Unit* xt002 = GetXT002(botAI);

    Player* tank = GetRedirectTank(xt002);
    if (!tank || tank == bot)
        return false;

    if (bot->getClass() == CLASS_ROGUE)
    {
        return botAI->CanCastSpell("tricks of the trade", tank) && botAI->CastSpell("tricks of the trade", tank);
    }

    if (botAI->CanCastSpell("misdirection", tank))
        return botAI->CastSpell("misdirection", tank);

    if (xt002 && !IsXT002Submerged(xt002) && bot->HasAura(SPELL_MISDIRECTION) &&
        botAI->CanCastSpell("steady shot", xt002))
        return botAI->CastSpell("steady shot", xt002);

    return false;
}

bool XT002HolderBuffAction::Execute(Event /*event*/)
{
    Player* tank = GetXT002BuffTarget(botAI, spell);
    return tank && botAI->CastSpell(spell, tank);
}

bool XT002RaidPositionAction::Execute(Event /*event*/)
{
    if (PlayerbotAI::IsTank(bot))
    {
        if (bot->GetExactDist(ULDUAR_XT002_MAINTANK_SPOT) <= ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE)
            return false;

        return MoveTo(bot->GetMapId(), ULDUAR_XT002_MAINTANK_SPOT.GetPositionX(),
                      ULDUAR_XT002_MAINTANK_SPOT.GetPositionY(), ULDUAR_XT002_MAINTANK_SPOT.GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    }

    Position slot;
    if (GetXT002RangedSlot(bot, GetXT002(botAI), slot))
    {
        if (bot->GetExactDist(slot) <= ULDUAR_XT002_RANGED_SPOT_TOLERANCE)
            return false;

        return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(), false,
                      false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    }

    return false;
}

bool XT002SetDpsPriorityAction::IsAllowedTarget(Unit* unit, Unit* xt002) const
{
    if (!unit || !unit->IsAlive() || unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    float const reach = PlayerbotAI::IsMelee(bot) ? ULDUAR_XT002_MELEE_ENGAGE_RANGE
                                            : ULDUAR_XT002_RANGED_ENGAGE_RANGE;
    bool const inReach = PlayerbotAI::IsTank(bot) || unit->GetExactDist2d(bot) <= reach;

    switch (unit->GetEntry())
    {
        case PB_NPC_XT002_LIFE_SPARK:
            return inReach && !IsXT002AddEvading(unit);

        case NPC_XS013_SCRAPBOT:
        case PB_NPC_XT002_PUMMELLER:
            return inReach && IsXT002AddEngageable(xt002, unit) && !IsXT002AddEvading(unit);

        case PB_NPC_XT002_BOOMBOT:
            return !PlayerbotAI::IsMelee(bot) && unit->GetExactDist2d(bot) >= ULDUAR_XT002_BOOMBOT_AVOID_RADIUS &&
                   inReach && IsXT002AddEngageable(xt002, unit) && !IsXT002AddEvading(unit);

        case NPC_HEART_OF_DECONSTRUCTOR:
            if (!unit->HasAura(SPELL_XT002_EXPOSED_HEART))
                return false;

            if (GetUldUnitByEntry(botAI, PB_NPC_XT002_LIFE_SPARK))
                return false;

            if (IsXT002HardModeActive(botAI))
                return true;

            return unit->GetHealthPct() > ULDUAR_XT002_HEART_SAFE_HP_PCT;

        case NPC_XT002:
            return !IsXT002Submerged(xt002);

        default:
            return true;
    }
}

Unit* XT002SetDpsPriorityAction::SelectByEntry(Unit* currentTarget, uint32 entry,
                                               std::vector<Unit*> const& candidates) const
{
    Unit* selected = nullptr;
    if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == entry &&
        !IsXT002AddEvading(currentTarget))
        selected = currentTarget;

    float const switchMargin = 10.0f;
    for (Unit* candidate : candidates)
    {
        if (!candidate || candidate == selected || IsXT002AddEvading(candidate))
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

std::vector<std::pair<uint32, Unit*>> XT002SetDpsPriorityAction::BuildPriorityList(Unit*& xt002)
{
    Unit* boss = nullptr;
    Unit* heart = nullptr;
    std::vector<Unit*> lifeSparks;
    std::vector<Unit*> scrapbots;
    std::vector<Unit*> boombots;
    std::vector<Unit*> pummellers;
    xt002 = nullptr;
    bool const probing = RaidObs::Active();
    std::string evadingSparks;

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
                if (!xt002)
                    xt002 = unit;
                break;
            case NPC_HEART_OF_DECONSTRUCTOR:
                heart = unit;
                break;
            case PB_NPC_XT002_LIFE_SPARK:
                lifeSparks.push_back(unit);
                if (probing && IsXT002AddEvading(unit))
                    evadingSparks += (evadingSparks.empty() ? "" : " ") + RaidObs::DescribeAssignment(unit->GetGUID());
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

    if (probing)
        RaidObs::NoteDerived(bot, "xt002.evading", evadingSparks.empty() ? "none" : evadingSparks);

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    if (PlayerbotAI::IsTank(bot))
    {
        std::vector<std::pair<uint32, Unit*>> tankPriority;

        XT002TankRoles const roles = GetXT002TankRoles(bot, xt002);
        if (roles.pummeller == bot && roles.holder != bot)
            tankPriority.emplace_back(PB_NPC_XT002_PUMMELLER,
                                      SelectByEntry(currentTarget, PB_NPC_XT002_PUMMELLER, pummellers));

        if (IsXT002HardModeActive(botAI))
            tankPriority.emplace_back(NPC_HEART_OF_DECONSTRUCTOR, heart);

        tankPriority.emplace_back(NPC_XT002, boss);
        return tankPriority;
    }

    bool const hardMode = IsXT002HardModeActive(botAI);

    std::vector<std::pair<uint32, Unit*>> priority;
    if (hardMode)
        priority.emplace_back(NPC_HEART_OF_DECONSTRUCTOR, heart);

    priority.emplace_back(PB_NPC_XT002_LIFE_SPARK,
                          SelectByEntry(currentTarget, PB_NPC_XT002_LIFE_SPARK, lifeSparks));
    priority.emplace_back(NPC_XS013_SCRAPBOT, SelectByEntry(currentTarget, NPC_XS013_SCRAPBOT, scrapbots));
    if (!PlayerbotAI::IsMelee(bot))
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
    Unit* xt002 = nullptr;
    std::vector<std::pair<uint32, Unit*>> const priority = BuildPriorityList(xt002);

    Unit* target = nullptr;
    for (auto const& candidate : priority)
    {
        if (IsAllowedTarget(candidate.second, xt002))
        {
            target = candidate.second;
            break;
        }
    }

    auto const priorityIndex = [&](Unit* unit) -> size_t
    {
        if (!IsAllowedTarget(unit, xt002))
            return priority.size();

        for (size_t index = 0; index < priority.size(); ++index)
        {
            if (priority[index].first == unit->GetEntry())
                return index;
        }

        return priority.size();
    };

    size_t const currentIndex = priorityIndex(currentTarget);
    if (currentIndex < priority.size() && currentIndex <= priorityIndex(target))
        target = currentTarget;

    return target;
}

bool XT002SetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    Unit* target = ResolveTarget(currentTarget);
    if (!target)
        return false;

    if (target->GetEntry() == NPC_XT002 && IsXT002ThreatBraked(bot, target))
    {
        bool const selected = SelectTarget(botAI, target);
        StopAttackingTarget(bot, target);
        return selected;
    }

    bool needsAttack = currentTarget != target;
    if (PlayerbotAI::IsMelee(bot))
        needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    return needsAttack ? Attack(target) : false;
}
