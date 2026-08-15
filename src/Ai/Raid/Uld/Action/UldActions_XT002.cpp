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

bool XT002MoveClearAction::MoveClearOf(std::vector<Unit*> const& avoid, float range)
{
    if (avoid.empty())
        return false;

    // Scored by how far the nearest unit ends up, capped at range: everything that clears the whole
    // group scores the same, so the first (nearest) such spot wins and the bot does not run further
    // than the mechanic needs.
    auto score = [&avoid, range](float x, float y)
    {
        float closest = std::numeric_limits<float>::max();
        for (Unit* unit : avoid)
            closest = std::min(closest, unit->GetExactDist2d(x, y));

        return std::min(closest, range);
    };

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
        for (float distance = increment; distance <= (range + 5.0f); distance += increment)
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

bool XT002MoveAwayFromDebuffedAllyAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    uint32 const spellId = GetDebuffSpellId();

    std::vector<Unit*> debuffed;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        if (member->HasAura(spellId))
            debuffed.push_back(member);
    }

    return MoveClearOf(debuffed, range);
}

bool XT002GravityBombCarrierAction::ParkVoidZone(Unit* boss)
{
    Position const& origin = botAI->IsMelee(bot) ? ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE
                                                 : ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED;
    float const originX = origin.GetPositionX();
    float const originY = origin.GetPositionY();
    float const originZ = origin.GetPositionZ();

    std::list<Creature*> voidZones;
    boss->GetCreatureListWithEntryInGrid(voidZones, PB_NPC_XT002_VOID_ZONE, ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS);

    for (int cell = 0; cell < ULDUAR_XT002_BOMB_GRID_ROWS * ULDUAR_XT002_BOMB_GRID_COLS; ++cell)
    {
        float const candidateX = originX + (cell % ULDUAR_XT002_BOMB_GRID_ROWS) * ULDUAR_XT002_BOMB_GRID_STEP;
        float const candidateY = originY + (cell / ULDUAR_XT002_BOMB_GRID_ROWS) * ULDUAR_XT002_BOMB_GRID_STEP;

        // Room geometry is script-summoned rather than spawned, so the grid cannot be checked against
        // the map offline - the LOS test is what keeps a cell behind a wall from being picked.
        if (!bot->IsWithinLOS(candidateX, candidateY, originZ))
            continue;

        bool occupied = false;
        for (Creature* voidZone : voidZones)
        {
            if (voidZone->GetExactDist2d(candidateX, candidateY) < ULDUAR_XT002_VOID_ZONE_RADIUS)
            {
                occupied = true;
                break;
            }
        }

        if (occupied)
            continue;

        if (bot->GetDistance(candidateX, candidateY, originZ) < 1.0f)
            return true;

        return MoveTo(bot->GetMapId(), candidateX, candidateY, originZ, false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT, true);
    }

    return false;
}

bool XT002GravityBombCarrierAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Void Zones only drop once XT actually carries Heartbreak, so before then there is nothing to
    // park and the carrier just needs to be somewhere the splash misses - which keeps melee uptime.
    if (IsXT002HeartbreakActive(botAI))
    {
        if (Unit* boss = GetXT002(botAI))
        {
            if (ParkVoidZone(boss))
                return true;
        }
    }

    std::vector<Unit*> allies;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        allies.push_back(member);
    }

    return MoveClearOf(allies, ULDUAR_XT002_DEBUFF_SPREAD_RADIUS);
}

bool XT002PummellerTauntAction::Execute(Event /*event*/)
{
    Unit* pummeller = GetFirstAliveUnitByEntry(botAI, PB_NPC_XT002_PUMMELLER);
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

    // A Pummeller is the off-tank's job, and it is the add most likely to peel onto a redirecter.
    if (GetFirstAliveUnitByEntry(botAI, PB_NPC_XT002_PUMMELLER))
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

    if (botAI->IsRangedDps(bot))
    {
        if (bot->GetExactDist(ULDUAR_XT002_RANGED_SPOT) <= ULDUAR_XT002_RANGED_SPOT_TOLERANCE)
            return false;

        return MoveTo(bot->GetMapId(), ULDUAR_XT002_RANGED_SPOT.GetPositionX(),
                      ULDUAR_XT002_RANGED_SPOT.GetPositionY(), ULDUAR_XT002_RANGED_SPOT.GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true);
    }

    return false;
}

bool XT002SearingLightCarrierAction::Execute(Event /*event*/)
{
    // The main tank stays on XT whatever it is carrying: dragging the boss across the room to dodge a
    // splash costs the raid far more than the splash does.
    if (botAI->IsTank(bot))
        return false;

    if (bot->GetExactDist(ULDUAR_XT002_SEARING_LIGHT_SPOT) < 1.0f)
        return false;

    return MoveTo(bot->GetMapId(), ULDUAR_XT002_SEARING_LIGHT_SPOT.GetPositionX(),
                  ULDUAR_XT002_SEARING_LIGHT_SPOT.GetPositionY(),
                  ULDUAR_XT002_SEARING_LIGHT_SPOT.GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool XT002SetDpsPriorityAction::IsAllowedTarget(Unit* unit) const
{
    if (!unit || !unit->IsAlive())
        return false;

    switch (unit->GetEntry())
    {
        case PB_NPC_XT002_BOOMBOT:
            // Melee must never pick one up, and ranged only from outside the blast: closer than that
            // the avoid action should be moving the bot, not this one holding it in place.
            return !botAI->IsMelee(bot) && unit->GetExactDist2d(bot) >= ULDUAR_XT002_BOOMBOT_AVOID_RADIUS;

        case NPC_HEART_OF_DECONSTRUCTOR:
            // Only a Life Spark is worth breaking off for, since Static Charged chains through the raid.
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

    // The Heart is only selectable while it is exposed, so this keeps it out of the list the rest of
    // the fight without a second lookup.
    if (heart && heart->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        heart = nullptr;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // Life Sparks chain Static Charged through the raid and Scrapbots heal XT back up if they reach
    // him; a Boombot only costs damage, and a Pummeller can simply be tanked. The Heart outranks add
    // DPS because hitting it is what spawns the adds, so a raid that stops for every Scrapbot never
    // gets it down.
    std::vector<std::pair<uint32, Unit*>> priority;
    priority.emplace_back(PB_NPC_XT002_LIFE_SPARK,
                          SelectByEntry(currentTarget, PB_NPC_XT002_LIFE_SPARK, lifeSparks));
    priority.emplace_back(NPC_XS013_SCRAPBOT, SelectByEntry(currentTarget, NPC_XS013_SCRAPBOT, scrapbots));
    if (!botAI->IsMelee(bot))
        priority.emplace_back(PB_NPC_XT002_BOOMBOT,
                              SelectByEntry(currentTarget, PB_NPC_XT002_BOOMBOT, boombots));
    priority.emplace_back(PB_NPC_XT002_PUMMELLER,
                          SelectByEntry(currentTarget, PB_NPC_XT002_PUMMELLER, pummellers));
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
    // Scrapbots cannot keep resetting swing and cast timers.
    if (currentTarget && priorityIndex(currentTarget) <= priorityIndex(target))
        target = currentTarget;

    return target ? target : AI_VALUE(Unit*, "dps target");
}

bool XT002SetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");
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
