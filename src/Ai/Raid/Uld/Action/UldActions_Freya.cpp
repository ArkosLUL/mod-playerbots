#include "UldActions_Freya.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <cmath>

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
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <TankAssistStrategy.h>

bool FreyaMoveAwayNatureBombAction::isUseful()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Find the nearest Nature Bomb
    GameObject* target = bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, 12.0f);
    if (!target)
        return false;

    return true;
}

bool FreyaMoveAwayNatureBombAction::Execute(Event /*event*/)
{
    GameObject* target = bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, 12.0f);
    if (!target)
        return false;

    return FleePosition(target->GetPosition(), 13.0f);
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

    // Every tick, not only on a switch: the early return below is the common case, and a pet left on
    // a dead or floored add would never catch up. CommandPetAttack no-ops when it is already there.
    CommandPetAttack(botAI, target);

    bool needsAttack = currentTarget != target;
    if (PlayerbotAI::IsMelee(bot))
        needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    // Returning false once the bot is on the right target is what lets the lower-priority nodes run:
    // the engine ends the tick at the first action that succeeds.
    return needsAttack ? Attack(target) : false;
}

Unit* FreyaSetDpsPriorityAction::SelectNearestLasher(Unit* currentTarget, std::vector<Unit*> const& candidates) const
{
    Unit* selected = nullptr;
    if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == NPC_DETONATING_LASHER)
        selected = currentTarget;

    // The margin stops two lashers at similar range from trading the bot back and forth every tick.
    constexpr float switchMargin = 10.0f;
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

Unit* FreyaSetDpsPriorityAction::ResolveFreyaDpsTarget(Unit* currentTarget)
{
    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    // Eonar's Gift heals Freya for 30-60% if it lives 12s. Ranged burn it from where they stand, so
    // melee never eat the travel time both ways - unless there is no ranged DPS left to do it.
    bool const takesGift = PlayerbotAI::IsRangedDps(bot) || !FreyaHasLivingRangedDps(botAI);

    // Once the trio is low, leaving it costs the whole wave: a member abandoned above the floor
    // revives 11s later, and the 60s until the next wave spawns is gone.
    bool const trioLocked = state.TrioLocked();

    std::vector<Unit*> priority;

    if (takesGift && !state.TrioReleased())
        priority.push_back(state.eonarsGift);

    if (!trioLocked)
        priority.push_back(state.conservator);

    // One slot for all three members; which one this bot takes is the greedy split, not entry order.
    Unit* const trioMember = GetFreyaTrioAssignment(botAI, state);
    priority.push_back(trioMember);

    if (trioLocked)
        priority.push_back(state.conservator);
    else
        priority.push_back(SelectNearestLasher(currentTarget, state.detonatingLashers));

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

    // Hold the current target unless something strictly more urgent is up, so a churn of adds cannot
    // keep resetting swing and cast timers. The trio slot is exempt: GetFreyaTrioAssignment is already
    // stable by construction, and stickiness on top would freeze each bot onto its first pick.
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
    {
        target = currentTarget;
    }

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

    Unit* target = GetFreyaTankTarget(botAI, state);
    if (!target)
        return false;

    if (target->GetVictim() != bot && UldCastClassTaunt(botAI, target))
        return true;

    return AI_VALUE(Unit*, "current target") != target ? Attack(target) : false;
}

bool FreyaAvoidDetonatingLasherAction::isUseful()
{
    FreyaAvoidDetonatingLasherTrigger trigger(botAI);
    return trigger.IsActive();
}

bool FreyaAvoidDetonatingLasherAction::Execute(Event /*event*/)
{
    Creature* lasher = bot->FindNearestCreature(NPC_DETONATING_LASHER, ULDUAR_FREYA_DETONATE_RADIUS);
    if (!lasher || !lasher->IsAlive())
        return false;

    return FleePosition(lasher->GetPosition(), ULDUAR_FREYA_DETONATE_RADIUS + 1.0f);
}

bool FreyaMoveToHealingSporeAction::isUseful()
{
    FreyaMoveToHealingSporeTrigger freyaMoveToHealingSporeTrigger(botAI);
    return freyaMoveToHealingSporeTrigger.IsActive();
}

bool FreyaMoveToHealingSporeAction::Execute(Event /*event*/)
{
    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    Creature* nearestSpore = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();

    for (auto guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        // Check if this unit is a healthy spore and alive
        if (unit->GetEntry() != NPC_HEALTHY_SPORE || !unit->IsAlive())
            continue;

        float distance = bot->GetDistance2d(unit);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestSpore = static_cast<Creature*>(unit);
        }
    }

    if (!nearestSpore)
        return false;

    return MoveTo(nearestSpore->GetMapId(), nearestSpore->GetPositionX(), nearestSpore->GetPositionY(),
                  nearestSpore->GetPositionZ(), false, false, false, true, MovementPriority::MOVEMENT_COMBAT);
}

bool FreyaBreakIronRootsAction::isUseful()
{
    FreyaBreakIronRootsTrigger freyaBreakIronRootsTrigger(botAI);
    return freyaBreakIronRootsTrigger.IsActive();
}

bool FreyaBreakIronRootsAction::Execute(Event /*event*/)
{
    // The root creature is summoned on the trapped bot; killing it removes the root DoT.
    Creature* root = bot->FindNearestCreature(NPC_FREYA_STRENGTHENED_IRON_ROOTS, 10.0f);
    if (!root)
        root = bot->FindNearestCreature(NPC_FREYA_IRON_ROOTS, 10.0f);

    if (!root || !root->IsAlive())
        return false;

    return Attack(root);
}

bool FreyaDodgeUnstableSunBeamAction::isUseful()
{
    FreyaDodgeUnstableSunBeamTrigger freyaDodgeUnstableSunBeamTrigger(botAI);
    return freyaDodgeUnstableSunBeamTrigger.IsActive();
}

bool FreyaDodgeUnstableSunBeamAction::Execute(Event /*event*/)
{
    // Beam stalkers are non-selectable, so find them via the raw nearby-npc list. Flee from the centre of
    // every in-range beam (not just the nearest) out past the whole cluster, so a bot in overlapping beams
    // steps clear instead of sidestepping one beam straight into another.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    std::vector<Position> beams;

    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FREYA_SUN_BEAM && unit->GetEntry() != NPC_FREYA_UNSTABLE_SUN_BEAM)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS)
            beams.push_back(unit->GetPosition());
    }

    if (beams.empty())
        return false;

    float cx = 0.0f, cy = 0.0f;
    for (Position const& beam : beams)
    {
        cx += beam.GetPositionX();
        cy += beam.GetPositionY();
    }
    cx /= beams.size();
    cy /= beams.size();

    // Flee far enough to clear the outermost in-range beam, not just the centre.
    Position const centre(cx, cy, 0.0f);
    float spread = 0.0f;
    for (Position const& beam : beams)
    {
        float const d = centre.GetExactDist2d(beam.GetPositionX(), beam.GetPositionY());
        if (d > spread)
            spread = d;
    }

    return FleePosition(Position(cx, cy, bot->GetPositionZ()),
                        ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS + spread + 1.0f);
}
