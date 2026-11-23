#include "UldActions_Freya.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

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
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "UldData.h"
#include "UldEncounter_Freya.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

namespace
{
void FreyaClearCastBlockingMove(Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);
}

bool FreyaBotCannotWalk(Player* bot)
{
    return bot->HasUnitState(UNIT_STATE_NOT_MOVE) || IsFreyaArmyChanneling(bot);
}

Position const* GetFreyaBeamEscapeBias(PlayerbotAI* botAI)
{
    Unit* freya = GetFreyaScan(botAI).Boss();
    return freya && freya->IsAlive() ? freya : nullptr;
}

bool TryFreyaLasherPickup(PlayerbotAI* botAI, FreyaWaveState const& state)
{
    Player* bot = botAI->GetBot();

    std::vector<Unit*> loose;
    std::vector<std::pair<Player*, uint32>> hitCounts;
    for (Unit* lasher : state.detonatingLashers)
    {
        Player* victim = GetFreyaLooseLasherVictim(lasher);
        if (!victim)
            continue;

        loose.push_back(lasher);
        auto const counted = std::find_if(hitCounts.begin(), hitCounts.end(),
                                          [victim](auto const& entry) { return entry.first == victim; });
        if (counted == hitCounts.end())
            hitCounts.emplace_back(victim, 1);
        else
            ++counted->second;
    }

    if (loose.empty())
        return false;

    Unit* freya = GetFreyaScanUnitByEntry(botAI, NPC_FREYA);

    Player* rescue = nullptr;
    uint32 mostHits = 0;
    for (auto const& [member, count] : hitCounts)
    {
        if (count > mostHits && (!freya || freya->GetVictim() != member) &&
            bot->GetExactDist2d(member) <= ULDUAR_FREYA_RIGHTEOUS_DEFENSE_RANGE && bot->IsWithinLOSInMap(member))
        {
            rescue = member;
            mostHits = count;
        }
    }

    if (rescue && botAI->CanCastSpell("righteous defense", rescue))
        return botAI->CastSpell("righteous defense", rescue);

    bool const freyaClear =
        !freya || bot->GetExactDist2d(freya) > ULDUAR_FREYA_LASHER_SHOUT_RADIUS + freya->GetCombatReach();
    auto const inShout = [bot](Unit* lasher) { return bot->GetExactDist2d(lasher) <= ULDUAR_FREYA_LASHER_SHOUT_RADIUS; };
    if (freyaClear && std::count_if(loose.begin(), loose.end(), inShout) >= 2)
    {
        for (char const* shout : {"challenging shout", "challenging roar"})
        {
            if (botAI->CanCastSpell(shout, bot))
                return botAI->CastSpell(shout, bot);
        }
    }

    if (!ClassTauntReady(botAI))
        return false;

    Unit* nearest = nullptr;
    for (Unit* lasher : loose)
    {
        float const dist = bot->GetExactDist2d(lasher);
        if (dist <= ULDUAR_FREYA_LASHER_TAUNT_RANGE && (!nearest || dist < bot->GetExactDist2d(nearest)) &&
            bot->IsWithinLOSInMap(lasher))
            nearest = lasher;
    }

    return nearest && CastClassTaunt(botAI, nearest);
}

std::vector<Position> GetFreyaAlliesNear(PlayerbotAI* botAI, float radius)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return {};

    std::vector<Position> allies;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (bot->GetExactDist2d(member) < radius)
            allies.push_back(member->GetPosition());
    }

    return allies;
}

Position StepOffNatureBombs(Player* bot, std::vector<Position> const& bombs)
{
    float sumX = 0.0f;
    float sumY = 0.0f;
    uint32 covering = 0;
    for (Position const& bomb : bombs)
    {
        if (bomb.GetExactDist2d(bot->GetPositionX(), bot->GetPositionY()) >= ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS)
            continue;

        sumX += bomb.GetPositionX();
        sumY += bomb.GetPositionY();
        ++covering;
    }

    if (!covering)
        return Position();

    float const centreX = sumX / covering;
    float const centreY = sumY / covering;

    float const bearing = bot->GetExactDist2d(centreX, centreY) > CONTACT_DISTANCE
                              ? std::atan2(bot->GetPositionY() - centreY, bot->GetPositionX() - centreX)
                              : bot->GetOrientation();

    float x = bot->GetPositionX() + std::cos(bearing) * ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS;
    float y = bot->GetPositionY() + std::sin(bearing) * ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS;
    float z = bot->GetPositionZ();
    if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                        bot->GetPositionZ(), x, y, z))
        return Position();

    return Position(x, y, z, 0.0f);
}
}

bool FreyaMoveAwayNatureBombAction::isUseful()
{
    FreyaNearNatureBombTrigger trigger(botAI);
    if (!trigger.IsActive())
        return false;

    if (bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS))
        return true;

    return bombSpotMs && getMSTimeDiff(bombSpotMs, getMSTime()) < ULDUAR_FREYA_NATURE_BOMB_LATCH_MS;
}

bool FreyaMoveAwayNatureBombAction::Execute(Event /*event*/)
{
    if (FreyaBotCannotWalk(bot))
    {
        bombSpotMs = 0;
        return false;
    }

    std::vector<Position> bombs = GetFreyaNatureBombPositions(bot, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    if (bombs.empty())
    {
        bombSpotMs = 0;
        return false;
    }

    std::vector<HazardCircle> const hazards =
        BuildFreyaEscapeHazards(bombs, GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS),
                                GetFreyaEscapeLasherPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS));

    auto const stillClear = [&hazards](Position const& spot)
    {
        for (HazardCircle const& hazard : hazards)
            if (hazard.first.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < hazard.second)
                return false;

        return true;
    };

    auto const covered = [&bombs](Position const& spot)
    {
        for (Position const& bomb : bombs)
            if (bomb.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS)
                return true;

        return false;
    };

    uint32 const now = getMSTime();
    bool const latched = bombSpotMs && getMSTimeDiff(bombSpotMs, now) < ULDUAR_FREYA_NATURE_BOMB_LATCH_MS;

    if (latched && stillClear(bombSpot) && !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(bombSpot.GetPositionX(), bombSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    Unit* freya = PlayerbotAI::IsMelee(bot) ? GetFreyaScan(botAI).Boss() : nullptr;
    Position const freyaAt = freya ? freya->GetPosition() : Position();
    float const reach = freya ? bot->GetMeleeRange(freya) - 1.0f : 0.0f;
    auto const inReach = [&freyaAt, reach](float x, float y) { return freyaAt.GetExactDist2d(x, y) <= reach; };

    Position const from = bot->GetPosition();
    std::vector<Position> const walkBombs =
        GetFreyaNatureBombPositions(bot, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS + ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS);
    auto const walkClear = [&from, &walkBombs](float x, float y)
    {
        return !FreyaWalkCrossesNatureBomb(from, Position(x, y, from.GetPositionZ(), 0.0f), walkBombs,
                                           ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS);
    };
    auto const inReachWalkClear = [&inReach, &walkClear](float x, float y) { return inReach(x, y) && walkClear(x, y); };

    if (!covered(bot->GetPosition()))
    {
        if (latched && PlayerbotAI::IsMelee(bot) && stillClear(bombSpot) && covered(bombOrigin))
        {
            if (freya && bot->IsWithinMeleeRange(freya))
                return false;

            if (freya && !freya->isMoving() &&
                (!reachRetryMs || getMSTimeDiff(reachRetryMs, now) >= ULDUAR_FREYA_MELEE_REACH_RETRY_MS))
            {
                reachRetryMs = now;
                HazardSweepCache sweep;
                Position const spot = FindNearestPositionClearOfHazards(
                    bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, ULDUAR_FREYA_SWEEP_STEP,
                    ULDUAR_FREYA_SWEEP_ANGLE_STEP, &freyaAt, inReachWalkClear, &sweep);
                if (spot != Position() &&
                    MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false,
                           false, false, true, MovementPriority::MOVEMENT_FORCED, true, false))
                {
                    bombSpot = spot;
                    bombSpotMs = now;
                }
            }

            return true;
        }

        bombSpotMs = 0;
        return false;
    }

    bombSpotMs = 0;

    HazardSweepCache sweep;

    Position safe;
    if (freya)
        safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                                 ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP, &freyaAt,
                                                 inReachWalkClear, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                                 ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP, nullptr,
                                                 walkClear, &sweep);

    if (safe == Position() && freya)
        safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                                 ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP, &freyaAt,
                                                 inReach, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS + 1.0f,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_BLAST_RADIUS,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position() && freya)
        safe = FindFreyaStaticFloorSpot(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &freyaAt, inReachWalkClear);

    if (safe == Position())
        safe = FindFreyaStaticFloorSpot(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, nullptr, walkClear);

    if (safe == Position())
    {
        std::vector<HazardCircle> blasts;
        blasts.reserve(bombs.size());
        for (Position const& bomb : bombs)
            blasts.emplace_back(bomb, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS + 1.0f);

        safe = FindFreyaStaticFloorSpot(bot, blasts, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, nullptr, {});
    }

    if (safe == Position())
        safe = StepOffNatureBombs(bot, bombs);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    bombOrigin = bot->GetPosition();
    bombSpot = safe;
    bombSpotMs = now;

    return true;
}

bool FreyaTankNatureBombAction::isUseful()
{
    FreyaTankNatureBombTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaTankNatureBombAction::Execute(Event /*event*/)
{
    if (FreyaBotCannotWalk(bot))
    {
        kiteSpotMs = 0;
        return false;
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
    {
        kiteSpotMs = 0;
        return false;
    }

    std::vector<Position> bombs = GetFreyaNatureBombPositions(bot, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    if (bombs.empty())
    {
        kiteSpotMs = 0;
        return false;
    }

    auto const stillClear = [&bombs](Position const& spot)
    {
        for (Position const& bomb : bombs)
            if (bomb.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS)
                return false;

        return true;
    };

    uint32 const now = getMSTime();
    LastMovement const& last = AI_VALUE(LastMovement&, "last movement");

    if (kiteSpotMs && getMSTimeDiff(kiteSpotMs, now) < ULDUAR_FREYA_TANK_BOMB_LATCH_MS && stillClear(kiteSpot) &&
        kiteSpot.GetExactDist2d(last.lastMoveToX, last.lastMoveToY) <= CONTACT_DISTANCE &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(kiteSpot.GetPositionX(), kiteSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    kiteSpotMs = 0;

    Position preferred = ULDUAR_FREYA_TANK_ANCHOR;
    if (boss->GetExactDist2d(&ULDUAR_FREYA_TANK_ANCHOR) <= ULDUAR_FREYA_TANK_LEASH)
    {
        if (Player* anchor = GetFreyaRangedCampAnchor(botAI))
        {
            float const dx = boss->GetPositionX() - anchor->GetPositionX();
            float const dy = boss->GetPositionY() - anchor->GetPositionY();
            float const length = std::sqrt(dx * dx + dy * dy);
            if (length > 0.0f)
                preferred = Position(boss->GetPositionX() + dx / length * ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                     boss->GetPositionY() + dy / length * ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                     boss->GetPositionZ(), 0.0f);
        }
    }

    Position const* preferNear = &preferred;

    std::vector<EncounterHelpers::HazardCircle> hazards =
        BuildFreyaEscapeHazards(bombs, GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS));

    HazardSweepCache sweep;

    Position safe;
    if (boss->GetVictim() == bot)
    {
        Position const freyaAt = boss->GetPosition();
        float const gap = bot->GetExactDist2d(boss);
        std::vector<Position> const nearHer = GetFreyaNatureBombPositions(
            bot, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS + gap + ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS);
        auto const freyaEndsClear = [&nearHer, &freyaAt, gap](float x, float y)
        {
            float const dx = freyaAt.GetPositionX() - x;
            float const dy = freyaAt.GetPositionY() - y;
            float const walk = std::sqrt(dx * dx + dy * dy);
            float const keep = walk > gap ? gap / walk : 1.0f;
            float const endX = x + dx * keep;
            float const endY = y + dy * keep;
            for (Position const& bomb : nearHer)
                if (bomb.GetExactDist2d(endX, endY) < ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS)
                    return false;

            return true;
        };

        safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                                 ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP, preferNear,
                                                 freyaEndsClear, &sweep);
    }

    if (safe == Position() &&
        !bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS))
        return false;

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                                 ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP, preferNear,
                                                 {}, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS + 1.0f,
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    kiteSpot = safe;
    kiteSpotMs = now;

    return true;
}

bool FreyaSetDpsPriorityAction::isUseful()
{
    FreyaSetDpsPriorityTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    Unit* target = ResolveFreyaDpsTarget(currentTarget);
    if (!target)
        return false;

    if (target->GetEntry() == NPC_EONARS_GIFT)
        CutWalkAwayFromGift(target);

    CommandPetAttack(botAI, target);

    bool needsAttack = currentTarget != target;
    if (PlayerbotAI::IsMelee(bot))
        needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    return needsAttack ? Attack(target) : false;
}

bool FreyaSetDpsPriorityAction::TakesEonarsGift(FreyaWaveState const& state)
{
    if (!state.eonarsGift || !state.eonarsGift->IsAlive())
    {
        giftGuid.Clear();
        return false;
    }

    if (!PlayerbotAI::IsRangedDps(bot))
    {
        ObjectGuid const gift = state.eonarsGift->GetGUID();
        if (filledGiftGuid != gift && IsFreyaGiftFiller(botAI, state.eonarsGift))
            filledGiftGuid = gift;

        return filledGiftGuid == gift;
    }

    bool packUp = false;
    for (Unit* lasher : state.detonatingLashers)
        packUp = packUp || (lasher && lasher->IsAlive());

    if (!packUp)
        return true;

    uint32 const now = getMSTime();
    if (giftGuid != state.eonarsGift->GetGUID())
    {
        giftGuid = state.eonarsGift->GetGUID();
        giftSeenMs = now;
    }

    if (getMSTimeDiff(giftSeenMs, now) >= ULDUAR_FREYA_GIFT_SHARE_MS)
        return true;

    return GetFreyaRangedDpsRank(botAI) < ULDUAR_FREYA_GIFT_SHARE;
}

void FreyaSetDpsPriorityAction::CutWalkAwayFromGift(Unit* gift)
{
    if (walkCutGiftGuid == gift->GetGUID())
        return;

    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    if (!last.msTime || last.priority > MovementPriority::MOVEMENT_COMBAT || !bot->isMoving())
        return;

    float const from = bot->GetExactDist2d(gift);
    float const to = gift->GetExactDist2d(last.lastMoveToX, last.lastMoveToY);
    if (to <= from)
        return;

    walkCutGiftGuid = gift->GetGUID();

    bot->GetMotionMaster()->Clear();
    bot->StopMoving();
    last.clear();

    if (RaidObs::Active())
    {
        char walk[32];
        std::snprintf(walk, sizeof(walk), "%.0f of %.0f yd", from, to);
        RaidObs::NoteDerived(bot, "freya.giftcut", walk);
    }
}

Unit* FreyaSetDpsPriorityAction::ShedSnaplasherThreat(FreyaWaveState const& state, Unit* assigned)
{
    if (!state.snaplasher)
    {
        snaplasherGuid.Clear();
        snaplasherShed = false;
        return assigned;
    }

    uint32 const now = getMSTime();
    if (snaplasherGuid != state.snaplasher->GetGUID())
    {
        snaplasherGuid = state.snaplasher->GetGUID();
        snaplasherSeenMs = now;
        snaplasherShed = false;
    }

    if (assigned != state.snaplasher)
        return assigned;

    Unit* sibling = nullptr;
    for (Unit* member : {state.stormLasher, state.waterSpirit})
    {
        if (!member || !member->IsAlive() || FreyaTrioSyncSuppress(state, member))
            continue;

        if (!sibling || member->GetHealth() > sibling->GetHealth())
            sibling = member;
    }

    if (PlayerbotAI::IsMelee(bot) && GetFreyaSnaplasherToKite(botAI) == assigned)
        return sibling;

    if (!sibling)
        return assigned;

    snaplasherShed = FreyaShedsSnaplasherThreat(botAI, state.snaplasher, getMSTimeDiff(snaplasherSeenMs, now),
                                                snaplasherShed);

    return snaplasherShed ? sibling : assigned;
}

Unit* FreyaSetDpsPriorityAction::ResolveFreyaDpsTarget(Unit* currentTarget)
{
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    bool const takesGift = TakesEonarsGift(state);

    bool const trioLocked = state.TrioLocked();

    bool const isRanged = PlayerbotAI::IsRangedDps(bot);

    auto const pickLasher = [&]() -> Unit*
    {
        float const reach = isRanged ? sPlayerbotAIConfig.SpellDistance : ULDUAR_FREYA_MELEE_LASHER_RANGE;

        Unit* lasher = isRanged ? GetFreyaLowLasherOnPlayer(bot, state, reach) : nullptr;
        if (!lasher && isRanged)
            lasher = GetFreyaRangedLasherAim(state);

        if (lasher && bot->GetExactDist2d(lasher) > reach)
            lasher = nullptr;

        if (!lasher)
            lasher = GetFreyaLocalLasherTarget(botAI, state, currentTarget, reach);

        return lasher;
    };

    std::vector<Unit*> priority;

    if (takesGift && !state.TrioReleased())
    {
        if (isRanged && !trioLocked)
            priority.push_back(GetFreyaLowLasherOnPlayer(bot, state, sPlayerbotAIConfig.SpellDistance, true));

        priority.push_back(state.eonarsGift);
    }

    if (!trioLocked)
        priority.push_back(state.conservator);

    if (isRanged && !trioLocked && !state.LivingTrio().empty() && IsFreyaLasherTail(state))
        priority.push_back(pickLasher());

    Unit* const trioMember = ShedSnaplasherThreat(state, GetFreyaTrioAssignment(botAI, state));
    priority.push_back(trioMember);

    if (trioLocked)
        priority.push_back(state.conservator);
    else
        priority.push_back(pickLasher());

    priority.push_back(AI_VALUE2(Unit*, "find target", "freya"));

    Unit* target = nullptr;
    for (Unit* candidate : priority)
    {
        if (candidate && candidate->IsAlive())
        {
            target = candidate;
            break;
        }
    }

    auto const priorityIndex = [&priority](Unit* unit) -> size_t
    {
        if (!unit || !unit->IsAlive())
            return priority.size();

        for (size_t i = 0; i < priority.size(); ++i)
        {
            if (priority[i] == unit)
                return i;
        }

        return priority.size();
    };

    if (currentTarget && currentTarget != trioMember && target != trioMember &&
        priorityIndex(currentTarget) <= priorityIndex(target))
        target = currentTarget;

    return target ? target : AI_VALUE(Unit*, "dps target");
}

bool FreyaTankAddsAction::isUseful()
{
    FreyaTankAddsTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaTankAddsAction::Execute(Event /*event*/)
{
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    Unit* target = GetFreyaTankTarget(botAI, state, currentTarget);
    if (!target)
        return false;

    Unit* storm = state.stormLasher;
    if (PlayerbotAI::IsMainTank(bot) && storm && storm->IsAlive() && storm->GetVictim() != bot &&
        CastClassTaunt(botAI, storm))
        return true;

    bool const owned = target == state.snaplasher || target == state.conservator;
    if (owned && target->GetVictim() != bot && CastClassTaunt(botAI, target))
        return true;

    bool const pastAddRungs = target->GetEntry() == NPC_DETONATING_LASHER || target->GetEntry() == NPC_FREYA;
    if (pastAddRungs && PlayerbotAI::IsAssistTankOfIndex(bot, 0, true) && TryFreyaLasherPickup(botAI, state))
        return true;

    if (currentTarget != target)
        return Attack(target);

    return target == state.conservator ? ParkConservator(target) : false;
}

bool FreyaTankAddsAction::ParkConservator(Unit* conservator)
{
    Unit* spore = botAI->GetUnit(parkedSpore);
    if (!spore || !spore->IsAlive())
    {
        spore = GetFreyaConservatorSpore(conservator);
        parkedSpore = spore ? spore->GetGUID() : ObjectGuid::Empty;
    }

    if (!spore)
        return false;

    if (conservator->GetExactDist2d(spore) <= ULDUAR_FREYA_SPORE_RADIUS - 1.0f)
        return false;

    return MoveTo(bot->GetMapId(), spore->GetPositionX(), spore->GetPositionY(), spore->GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool FreyaMoveToHealingSporeAction::isUseful()
{
    FreyaMoveToHealingSporeTrigger freyaMoveToHealingSporeTrigger(botAI);
    return freyaMoveToHealingSporeTrigger.IsActive();
}

bool FreyaMoveToHealingSporeAction::Execute(Event /*event*/)
{
    Unit* target = GetFreyaTargetSpore(botAI);
    if (!target)
        return false;

    float const distance = bot->GetExactDist2d(target);
    if (distance <= ULDUAR_FREYA_SPORE_STAND_RANGE)
        return false;

    float const ratio = ULDUAR_FREYA_SPORE_STAND_RANGE / distance;
    float const x = target->GetPositionX() + (bot->GetPositionX() - target->GetPositionX()) * ratio;
    float const y = target->GetPositionY() + (bot->GetPositionY() - target->GetPositionY()) * ratio;

    return MoveTo(target->GetMapId(), x, y, target->GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool FreyaKiteSnaplasherAction::isUseful()
{
    FreyaKiteSnaplasherTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaKiteSnaplasherAction::Execute(Event /*event*/)
{
    if (FreyaBotCannotWalk(bot))
    {
        kiteSpotMs = 0;
        return false;
    }

    Unit* snaplasher = GetFreyaSnaplasherToKite(botAI);
    if (!snaplasher)
    {
        kiteSpotMs = 0;
        return false;
    }

    Position const snaplasherAt = snaplasher->GetPosition();
    float const margin = GetFreyaSnaplasherKiteReach(snaplasher, bot, ULDUAR_FREYA_SNAPLASHER_KITE_MARGIN);
    float const clear = GetFreyaSnaplasherKiteReach(snaplasher, bot, ULDUAR_FREYA_SNAPLASHER_KITE_CLEAR);

    std::vector<HazardCircle> hazards = GetFreyaEscapeHazards(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);

    auto const stillClear = [&hazards, &snaplasherAt, margin](Position const& spot)
    {
        if (snaplasherAt.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < margin)
            return false;

        for (HazardCircle const& hazard : hazards)
            if (hazard.first.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < hazard.second)
                return false;

        return true;
    };

    uint32 const now = getMSTime();
    if (kiteSpotMs && getMSTimeDiff(kiteSpotMs, now) < ULDUAR_FREYA_SNAPLASHER_KITE_LATCH_MS && stillClear(kiteSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(kiteSpot.GetPositionX(), kiteSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    kiteSpotMs = 0;

    Position const from = bot->GetPosition();
    float const gap = from.GetExactDist2d(snaplasherAt.GetPositionX(), snaplasherAt.GetPositionY());
    float const away = gap > CONTACT_DISTANCE ? snaplasherAt.GetAngle(&from) : bot->GetOrientation() + float(M_PI);
    Position const ahead(from.GetPositionX() + clear * std::cos(away), from.GetPositionY() + clear * std::sin(away),
                         from.GetPositionZ(), 0.0f);

    HazardCircle const swing(snaplasherAt,
                             std::max(0.0f, std::min(gap, snaplasher->GetMeleeRange(bot)) - CONTACT_DISTANCE));
    auto const keepsClear = [&from, &swing](float x, float y)
    { return !SegmentCrossesCircle(from, Position(x, y, from.GetPositionZ(), 0.0f), swing); };

    hazards.emplace_back(snaplasherAt, clear);

    HazardSweepCache sweep;
    Position const spot = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                                            ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP,
                                                            &ahead, keepsClear, &sweep);
    if (spot == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    kiteSpot = spot;
    kiteSpotMs = now;

    return true;
}

bool FreyaAddTankDefensiveAction::Execute(Event /*event*/)
{
    if (HasFreyaTankExternal(bot))
        return false;

    uint32 const spellId = NextTankDefensive(botAI, bot, "freya.adddef", true);
    return spellId && botAI->CastSpell(spellId, bot);
}

bool FreyaAddTankExternalAction::Execute(Event /*event*/)
{
    Player* tank = nullptr;
    char const* external = GetFreyaTankExternal(botAI, tank);
    if (!external)
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "freya.addext", external);

    return botAI->CastSpell(external, tank);
}

bool FreyaRedirectThreatAction::isUseful()
{
    return bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_ROGUE;
}

Player* FreyaRedirectThreatAction::GetRedirectTank(Unit*& dumpTarget)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Unit* snaplasher = nullptr;
    Unit* conservator = nullptr;
    Unit* freya = nullptr;
    for (ObjectGuid const& guid : GetFreyaScan(botAI).PossibleTargets())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_SNAPLASHER:
                if (!snaplasher)
                    snaplasher = unit;
                break;
            case NPC_ANCIENT_CONSERVATOR:
                if (!conservator)
                    conservator = unit;
                break;
            case NPC_FREYA:
                if (!freya)
                    freya = unit;
                break;
            default:
                break;
        }
    }

    if (snaplasher || conservator)
    {
        if (Player* assistTank = GetGroupAssistTank(bot, 0))
        {
            dumpTarget = snaplasher ? snaplasher : conservator;
            return assistTank;
        }
    }

    dumpTarget = freya;

    if (freya)
    {
        if (Unit* victim = freya->GetVictim())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !member->IsAlive() || member == bot)
                    continue;

                if (PlayerbotAI::IsTank(member) && member == victim)
                    return member;
            }
        }
    }

    return GetGroupMainTank(bot);
}

bool FreyaRedirectThreatAction::Execute(Event /*event*/)
{
    Unit* dumpTarget = nullptr;
    Player* tank = GetRedirectTank(dumpTarget);
    if (!tank || tank == bot)
        return false;

    if (bot->getClass() == CLASS_ROGUE)
    {
        return botAI->CanCastSpell("tricks of the trade", tank) && botAI->CastSpell("tricks of the trade", tank);
    }

    if (botAI->CanCastSpell("misdirection", tank))
        return botAI->CastSpell("misdirection", tank);

    if (!dumpTarget || !bot->HasAura(SPELL_MISDIRECTION))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);
    if (FreyaTrioSyncSuppress(state, dumpTarget))
        return false;

    return botAI->CanCastSpell("steady shot", dumpTarget) && botAI->CastSpell("steady shot", dumpTarget);
}

bool FreyaBreakIronRootsAction::isUseful()
{
    FreyaBreakIronRootsTrigger freyaBreakIronRootsTrigger(botAI);
    return freyaBreakIronRootsTrigger.IsActive();
}

bool FreyaBreakIronRootsAction::Execute(Event /*event*/)
{
    Creature* root = bot->FindNearestCreature(NPC_FREYA_STRENGTHENED_IRON_ROOTS, ULDUAR_FREYA_IRON_ROOTS_SEARCH_RADIUS);
    if (!root)
        root = bot->FindNearestCreature(NPC_FREYA_IRON_ROOTS, ULDUAR_FREYA_IRON_ROOTS_SEARCH_RADIUS);

    if (!root || !root->IsAlive())
        return false;

    if (PlayerbotAI::IsDps(bot) && bot->IsNonMeleeSpellCast(false) && !IsFreyaArmyChanneling(bot) &&
        GetFreyaHazardsUnder(botAI, bot) > 0)
    {
        Spell const* cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!cast)
            cast = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

        if (!cast || cast->m_targets.GetUnitTarget() != root)
            bot->InterruptNonMeleeSpells(false);
    }

    return Attack(root);
}

bool FreyaFreeRootedAllyAction::isUseful()
{
    FreyaFreeRootedAllyTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaFreeRootedAllyAction::Execute(Event /*event*/)
{
    if (bot->getClass() == CLASS_PALADIN)
    {
        Player* rooted = GetFreyaRootedAllyToFree(botAI);
        return rooted && botAI->CanCastSpell("hand of freedom", rooted) && botAI->CastSpell("hand of freedom", rooted);
    }

    Position centre;
    if (!IsFreyaRooted(bot) || !GetFreyaHazardsUnder(botAI, bot, &centre))
        return false;

    uint32 const blink = AI_VALUE2(uint32, "spell id", "blink");
    if (!blink || bot->HasSpellCooldown(blink))
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    if (bot->GetExactDist2d(centre.GetPositionX(), centre.GetPositionY()) > CONTACT_DISTANCE)
        bot->SetOrientation(std::atan2(bot->GetPositionY() - centre.GetPositionY(),
                                       bot->GetPositionX() - centre.GetPositionX()));

    return botAI->CastSpell("blink", bot);
}

bool FreyaDodgeUnstableSunBeamAction::isUseful()
{
    FreyaDodgeUnstableSunBeamTrigger freyaDodgeUnstableSunBeamTrigger(botAI);
    return freyaDodgeUnstableSunBeamTrigger.IsActive();
}

bool FreyaDodgeUnstableSunBeamAction::Execute(Event /*event*/)
{
    if (FreyaBotCannotWalk(bot))
    {
        dodgeSpotMs = 0;
        return false;
    }

    std::vector<Position> beams = GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);

    if (beams.empty())
    {
        dodgeSpotMs = 0;
        return false;
    }

    auto const stillClear = [&beams](Position const& spot)
    {
        for (Position const& beam : beams)
            if (beam.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_FREYA_SUN_BEAM_CLEARANCE)
                return false;

        return true;
    };

    uint32 const now = getMSTime();

    if (dodgeSpotMs && getMSTimeDiff(dodgeSpotMs, now) < ULDUAR_FREYA_SUN_BEAM_LATCH_MS && stillClear(dodgeSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(dodgeSpot.GetPositionX(), dodgeSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    dodgeSpotMs = 0;

    HazardSweepCache sweep;
    Position const* preferNear = GetFreyaBeamEscapeBias(botAI);

    auto const beamsAt = [&beams](float clearance)
    {
        std::vector<EncounterHelpers::HazardCircle> circles;
        circles.reserve(beams.size());
        for (Position const& beam : beams)
            circles.emplace_back(beam, clearance);

        return circles;
    };

    Position safe = FindNearestPositionClearOfHazards(
        bot,
        BuildFreyaEscapeHazards(GetFreyaNatureBombPositions(bot, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS), beams,
                                GetFreyaEscapeLasherPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS)),
        ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP, preferNear, {},
        &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, beamsAt(ULDUAR_FREYA_SUN_BEAM_CLEARANCE),
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, ULDUAR_FREYA_SWEEP_STEP,
                                                 ULDUAR_FREYA_SWEEP_ANGLE_STEP, preferNear, {}, &sweep);

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, beamsAt(ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS + 1.0f),
                                                 ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, ULDUAR_FREYA_SWEEP_STEP,
                                                 ULDUAR_FREYA_SWEEP_ANGLE_STEP, preferNear, {}, &sweep);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    dodgeSpot = safe;
    dodgeSpotMs = now;

    return true;
}

bool FreyaNaturesFuryBailAction::isUseful()
{
    FreyaNaturesFuryBailTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaNaturesFuryBailAction::Execute(Event /*event*/)
{
    if (FreyaBotCannotWalk(bot))
    {
        bailSpotMs = 0;
        return false;
    }

    auto const stillClear = [this](Position const& spot)
    { return CountFreyaRaidNear(botAI, spot, ULDUAR_FREYA_NATURES_FURY_RADIUS, bot) == 0; };

    uint32 const now = getMSTime();

    if (bailSpotMs && getMSTimeDiff(bailSpotMs, now) < ULDUAR_FREYA_NATURES_FURY_LATCH_MS && stillClear(bailSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(bailSpot.GetPositionX(), bailSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    bailSpotMs = 0;

    Position safe;
    if (Unit* shelter = GetFreyaNaturesFuryShelter(botAI))
    {
        float const distance = bot->GetExactDist2d(shelter);
        if (distance > ULDUAR_FREYA_SPORE_STAND_RANGE)
        {
            float const ratio = ULDUAR_FREYA_SPORE_STAND_RANGE / distance;
            safe = Position(shelter->GetPositionX() + (bot->GetPositionX() - shelter->GetPositionX()) * ratio,
                            shelter->GetPositionY() + (bot->GetPositionY() - shelter->GetPositionY()) * ratio,
                            shelter->GetPositionZ(), 0.0f);
        }
    }

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(bot, GetFreyaAlliesNear(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS),
                                                 ULDUAR_FREYA_NATURES_FURY_CLEAR, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    bailSpot = safe;
    bailSpotMs = now;

    return true;
}

bool FreyaStepOutOfSunbeamAction::isUseful()
{
    FreyaStepOutOfSunbeamTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaStepOutOfSunbeamAction::Execute(Event /*event*/)
{
    if (FreyaBotCannotWalk(bot))
    {
        stepSpotMs = 0;
        return false;
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    Unit* target = boss ? GetFreyaSunbeamTarget(boss) : nullptr;
    if (!target)
    {
        stepSpotMs = 0;
        return false;
    }

    Position const beam = target->GetPosition();

    auto const stillClear = [&beam](Position const& spot)
    { return beam.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) >= ULDUAR_FREYA_SUNBEAM_CLEAR_RADIUS; };

    uint32 const now = getMSTime();

    if (stepSpotMs && getMSTimeDiff(stepSpotMs, now) < ULDUAR_FREYA_SUNBEAM_LATCH_MS && stillClear(stepSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(stepSpot.GetPositionX(), stepSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    stepSpotMs = 0;

    std::vector<EncounterHelpers::HazardCircle> hazards =
        GetFreyaEscapeHazards(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    hazards.emplace_back(beam, ULDUAR_FREYA_SUNBEAM_CLEAR_RADIUS);

    HazardSweepCache sweep;
    Position const* preferNear = GetFreyaBeamEscapeBias(botAI);

    Position safe = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                                      ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP,
                                                      preferNear, {}, &sweep);

    if (safe == Position())
    {
        std::vector<EncounterHelpers::HazardCircle> blastAndBeam = BuildFreyaEscapeHazards(
            {}, {}, GetFreyaEscapeLasherPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS));
        blastAndBeam.emplace_back(beam, ULDUAR_FREYA_SUNBEAM_CLEAR_RADIUS);

        safe = FindNearestPositionClearOfHazards(bot, blastAndBeam, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS,
                                                 ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP, preferNear,
                                                 {}, &sweep);
    }

    if (safe == Position())
        safe = FindNearestPositionClearOfHazards(
            bot, std::vector<EncounterHelpers::HazardCircle>{{beam, ULDUAR_FREYA_SUNBEAM_CLEAR_RADIUS}},
            ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP, preferNear, {},
            &sweep);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    stepSpot = safe;
    stepSpotMs = now;

    return true;
}

bool FreyaTankHoldFreyaAction::isUseful()
{
    FreyaTankHoldFreyaTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaTankHoldFreyaAction::Execute(Event /*event*/)
{
    uint32 const now = getMSTime();

    if (holdMs && getMSTimeDiff(holdMs, now) < ULDUAR_FREYA_TANK_HOLD_LATCH_MS &&
        !bot->IsMovementPreventedByCasting() && bot->GetExactDist2d(&ULDUAR_FREYA_TANK_ANCHOR) > CONTACT_DISTANCE)
        return true;

    holdMs = 0;

    if (!MoveTo(bot->GetMapId(), ULDUAR_FREYA_TANK_ANCHOR.GetPositionX(), ULDUAR_FREYA_TANK_ANCHOR.GetPositionY(),
                ULDUAR_FREYA_TANK_ANCHOR.GetPositionZ(), false, false, false, true,
                MovementPriority::MOVEMENT_COMBAT))
        return false;

    holdMs = now;

    return true;
}

bool FreyaLasherAboutToBlowAction::isUseful()
{
    FreyaLasherAboutToBlowTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaLasherAboutToBlowAction::Execute(Event /*event*/)
{
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return StepOutOfBlasts(
        GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS),
        ULDUAR_FREYA_LASHER_PACK_CLEAR);
}

bool FreyaLasherBailAction::StepOutOfBlasts(std::vector<Position> const& blasts, float clearance)
{
    if (FreyaBotCannotWalk(bot) || blasts.empty())
    {
        bailSpotMs = 0;
        return false;
    }

    float const edge = ULDUAR_FREYA_DETONATE_RADIUS + 1.0f;

    auto const stillClear = [&blasts, edge](Position const& spot)
    {
        for (Position const& blast : blasts)
            if (blast.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < edge)
                return false;

        return true;
    };

    uint32 const now = getMSTime();

    if (bailSpotMs && getMSTimeDiff(bailSpotMs, now) < ULDUAR_FREYA_LASHER_BAIL_LATCH_MS && stillClear(bailSpot) &&
        !bot->IsMovementPreventedByCasting() &&
        bot->GetExactDist2d(bailSpot.GetPositionX(), bailSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    bailSpotMs = 0;

    HazardSweepCache sweep;

    Position safe =
        FindNearestPositionClearOfHazards(bot, blasts, clearance, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position() && clearance > edge)
        safe = FindNearestPositionClearOfHazards(bot, blasts, edge, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, &sweep);

    if (safe == Position())
        return false;

    FreyaClearCastBlockingMove(bot);

    if (!MoveTo(bot->GetMapId(), safe.GetPositionX(), safe.GetPositionY(), safe.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_FORCED, true, false))
        return false;

    bailSpot = safe;
    bailSpotMs = now;

    return true;
}

bool FreyaRangedCampAction::isUseful()
{
    FreyaRangedCampTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaRangedCampAction::Execute(Event /*event*/)
{
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    std::vector<Position> keepOff =
        GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    std::vector<Position> const submerged =
        GetFreyaSubmergedLasherPositions(botAI, state, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    keepOff.insert(keepOff.end(), submerged.begin(), submerged.end());
    if (!keepOff.empty())
        return StepOutOfBlasts(keepOff, ULDUAR_FREYA_LASHER_CAMP_STANDOFF);

    Position const camp = GetFreyaLasherCampSpot(botAI);
    if (camp == Position())
        return false;

    float const distance = bot->GetExactDist2d(camp.GetPositionX(), camp.GetPositionY());
    float const bearing =
        distance > CONTACT_DISTANCE ? camp.GetAngle(bot->GetPositionX(), bot->GetPositionY()) : bot->GetOrientation();
    float const x = camp.GetPositionX() + std::cos(bearing) * ULDUAR_FREYA_RANGED_CAMP_SPACING;
    float const y = camp.GetPositionY() + std::sin(bearing) * ULDUAR_FREYA_RANGED_CAMP_SPACING;

    return MoveTo(bot->GetMapId(), x, y, camp.GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool FreyaReachHeldPackAction::isUseful()
{
    FreyaReachHeldPackTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaReachHeldPackAction::Execute(Event /*event*/)
{
    if (FreyaBotCannotWalk(bot) || bot->IsMovementPreventedByCasting())
    {
        walkSpotMs = 0;
        return false;
    }

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    Unit* focus = GetFreyaRangedLasherAim(state);
    if (!focus)
    {
        walkSpotMs = 0;
        return false;
    }

    float const reach = sPlayerbotAIConfig.SpellDistance - ULDUAR_FREYA_HELD_PACK_REACH_MARGIN;

    std::vector<HazardCircle> hazards =
        BuildFreyaEscapeHazards(GetFreyaNatureBombPositions(bot, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS),
                                GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS));
    for (Unit* lasher : state.detonatingLashers)
        if (lasher && lasher->IsAlive())
            hazards.emplace_back(lasher->GetPosition(), ULDUAR_FREYA_LASHER_CAMP_STANDOFF);

    auto const usable = [&hazards, focus, reach](Position const& spot)
    {
        if (focus->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) > reach)
            return false;

        for (HazardCircle const& hazard : hazards)
            if (hazard.first.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < hazard.second)
                return false;

        return true;
    };

    uint32 const now = getMSTime();
    if (walkSpotMs && getMSTimeDiff(walkSpotMs, now) < ULDUAR_FREYA_HELD_PACK_LATCH_MS && usable(walkSpot) &&
        bot->GetExactDist2d(walkSpot.GetPositionX(), walkSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    walkSpotMs = 0;

    if (retryMs && getMSTimeDiff(retryMs, now) < ULDUAR_FREYA_HELD_PACK_RETRY_MS)
        return false;

    HazardSweepCache sweep;
    Position const focusAt = focus->GetPosition();
    Position const spot = FindNearestPositionClearOfHazards(
        bot, hazards, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS, ULDUAR_FREYA_SWEEP_STEP, ULDUAR_FREYA_SWEEP_ANGLE_STEP,
        &focusAt, [&focusAt, reach](float x, float y) { return focusAt.GetExactDist2d(x, y) <= reach; }, &sweep);

    if (spot == Position())
    {
        retryMs = now;
        return false;
    }

    if (!MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_COMBAT))
        return false;

    walkSpot = spot;
    walkSpotMs = now;

    return true;
}

bool FreyaReachGiftAction::isUseful()
{
    FreyaReachGiftTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaReachGiftAction::Execute(Event /*event*/)
{
    if (FreyaBotCannotWalk(bot) || bot->IsMovementPreventedByCasting())
    {
        walkSpotMs = 0;
        return false;
    }

    Unit* gift = AI_VALUE(Unit*, "current target");
    if (!gift || !gift->IsAlive() || gift->GetEntry() != NPC_EONARS_GIFT)
    {
        walkSpotMs = 0;
        return false;
    }

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    float const giftDistance = bot->GetExactDist2d(gift);
    float const corridor = giftDistance + ULDUAR_FREYA_LASHER_CAMP_STANDOFF;

    std::vector<HazardCircle> blasts;
    for (Position const& low : GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, corridor))
        blasts.emplace_back(low, ULDUAR_FREYA_DETONATE_RADIUS + 1.0f);

    for (Position const& waking : GetFreyaSubmergedLasherPositions(botAI, state, corridor))
        blasts.emplace_back(waking, ULDUAR_FREYA_LASHER_CAMP_STANDOFF);

    std::vector<HazardCircle> hazards =
        BuildFreyaEscapeHazards(GetFreyaNatureBombPositions(bot, corridor), GetFreyaSunBeamPositions(botAI, corridor));
    hazards.insert(hazards.end(), blasts.begin(), blasts.end());

    float const reach = sPlayerbotAIConfig.SpellDistance - ULDUAR_FREYA_GIFT_WALK_MARGIN;
    Position const giftAt = gift->GetPosition();
    Position const from = bot->GetPosition();

    auto const walkClear = [&blasts, &from](float x, float y)
    {
        Position const to(x, y, from.GetPositionZ(), 0.0f);
        for (HazardCircle const& blast : blasts)
            if (blast.first.GetExactDist2d(from.GetPositionX(), from.GetPositionY()) >= blast.second &&
                SegmentCrossesCircle(from, to, blast))
                return false;

        return true;
    };

    auto const inReachWalkClear = [&giftAt, reach, &walkClear](float x, float y)
    { return giftAt.GetExactDist2d(x, y) <= reach && walkClear(x, y); };

    auto const usable = [&hazards, &inReachWalkClear](Position const& spot)
    {
        for (HazardCircle const& hazard : hazards)
            if (hazard.first.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < hazard.second)
                return false;

        return inReachWalkClear(spot.GetPositionX(), spot.GetPositionY());
    };

    uint32 const now = getMSTime();
    if (walkSpotMs && getMSTimeDiff(walkSpotMs, now) < ULDUAR_FREYA_GIFT_WALK_LATCH_MS && usable(walkSpot) &&
        bot->GetExactDist2d(walkSpot.GetPositionX(), walkSpot.GetPositionY()) > CONTACT_DISTANCE)
        return true;

    walkSpotMs = 0;

    if (retryMs && getMSTimeDiff(retryMs, now) < ULDUAR_FREYA_GIFT_WALK_RETRY_MS)
        return false;

    HazardSweepCache sweep;
    Position const spot =
        FindNearestPositionClearOfHazards(bot, hazards, giftDistance, ULDUAR_FREYA_SWEEP_STEP,
                                          ULDUAR_FREYA_SWEEP_ANGLE_STEP, &giftAt, inReachWalkClear, &sweep);

    if (spot == Position())
    {
        retryMs = now;
        return false;
    }

    if (!MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                true, MovementPriority::MOVEMENT_COMBAT))
        return false;

    walkSpot = spot;
    walkSpotMs = now;

    return true;
}

bool FreyaFrostNovaLashersAction::isUseful()
{
    FreyaFrostNovaLashersTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaFrostNovaLashersAction::Execute(Event /*event*/)
{
    return botAI->CastSpell("frost nova", bot);
}

bool FreyaTrapLashersAction::isUseful()
{
    FreyaTrapLashersTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaTrapLashersAction::Execute(Event /*event*/)
{
    return botAI->CastSpell("frost trap", bot);
}

bool FreyaSummonArmyAction::isUseful()
{
    FreyaSummonArmyTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaSummonArmyAction::Execute(Event /*event*/)
{
    return botAI->CastSpell("army of the dead", bot);
}

bool FreyaGroundTremorHoldCastAction::isUseful()
{
    FreyaGroundTremorHoldCastTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaGroundTremorHoldCastAction::Execute(Event /*event*/)
{
    Unit* boss = GetFreyaBossByEntry(botAI);
    if (!boss)
        return false;

    Spell* tremor = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);

    Spell* own = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!own)
        own = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

    if (!tremor || !own)
        return false;

    if (own->GetCastTimeRemaining() < tremor->GetCastTimeRemaining())
        return false;

    bot->CastStop();

    return true;
}
