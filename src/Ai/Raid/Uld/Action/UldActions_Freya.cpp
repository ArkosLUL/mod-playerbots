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
    {
        priority.push_back(state.conservator);
    }
    else
    {
        bool const isRanged = PlayerbotAI::IsRangedDps(bot);
        Unit* lasher = isRanged ? GetFreyaRangedLasherFocus(state) : nullptr;

        // The raid-wide focus can be most of the room away. Walking to it would put the bot inside the
        // 15 yd blast, which is the one thing that keeps ranged safe here, so it shoots whatever it can
        // already reach instead - and the two converge on their own as lashers close on players.
        float const reach = isRanged ? sPlayerbotAIConfig.spellDistance : ULDUAR_FREYA_MELEE_LASHER_RANGE;
        if (lasher && bot->GetExactDist2d(lasher) > reach)
            lasher = nullptr;

        if (!lasher)
            lasher = GetFreyaLocalLasherTarget(botAI, state, currentTarget, reach);

        priority.push_back(lasher);
    }

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

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    Unit* target = GetFreyaTankTarget(botAI, state, currentTarget);
    if (!target)
        return false;

    // Taunt only what this encounter actually claims. The rest of the ladder is borrowed for damage:
    // taunting Freya would fight a human main tank whose raid roles are set differently, taunting a Storm
    // Lasher or Water Spirit would mean owning Tidal Wave positioning, and a lasher drops the taunt again
    // on its next 10s threat wipe anyway.
    bool const owned = target == state.snaplasher || target == state.conservator;
    if (owned && target->GetVictim() != bot && UldCastClassTaunt(botAI, target))
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
        spore = GetFreyaConservatorSpore(botAI, conservator);
        parkedSpore = spore ? spore->GetGUID() : ObjectGuid::Empty;
    }

    if (!spore)
        return false;

    // Pheromones is a 6 yd aura on the spore and the Conservator stops at melee range of the tank, so
    // standing on the spore is what puts the boss in reach of everyone sheltering on it. Hold still once
    // it is there rather than nudging it back and forth.
    if (conservator->GetExactDist2d(spore) <= ULDUAR_FREYA_SPORE_RADIUS - 1.0f)
        return false;

    return MoveTo(bot->GetMapId(), spore->GetPositionX(), spore->GetPositionY(), spore->GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true, false);
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
    Unit* target = nullptr;

    // Melee go to the spore the Conservator is parked on, not the nearest one - anywhere else and the
    // DPS node drags them back out of the aura to reach the boss, and the two nodes fight all wave.
    if (PlayerbotAI::IsMelee(bot))
        target = GetFreyaConservatorSpore(botAI, GetFirstAliveUnitByEntry(botAI, NPC_ANCIENT_CONSERVATOR));

    // Ranged and healers only need the aura, not melee range, and every spore sits 20 yd from the
    // Conservator - inside casting range of it and of the melee stack. No reason to join the pile.
    if (!target)
    {
        float nearestDistance = std::numeric_limits<float>::max();
        for (auto const& guid : AI_VALUE(GuidVector, "nearest npcs"))
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_HEALTHY_SPORE)
                continue;

            float const distance = bot->GetDistance2d(unit);
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                target = unit;
            }
        }
    }

    if (!target)
        return false;

    return MoveTo(target->GetMapId(), target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), false,
                  false, false, true, MovementPriority::MOVEMENT_COMBAT);
}

bool FreyaRedirectThreatAction::isUseful()
{
    return bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_ROGUE;
}

Player* FreyaRedirectThreatAction::GetRedirectTank()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    // The two adds with a real threat table that someone else is holding. Detonating Lashers reset
    // threat every 10s, so no redirect can ever help there.
    if (GetFirstAliveUnitByEntry(botAI, NPC_SNAPLASHER) || GetFirstAliveUnitByEntry(botAI, NPC_ANCIENT_CONSERVATOR))
    {
        if (Player* assistTank = GetGroupAssistTank(botAI, bot, 0))
            return assistTank;
    }

    // Otherwise feed whoever is actually holding Freya, which survives a swap or a tank death.
    if (Unit* freya = GetFirstAliveUnitByEntry(botAI, NPC_FREYA))
    {
        if (Unit* victim = freya->GetVictim())
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

bool FreyaRedirectThreatAction::Execute(Event /*event*/)
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

    // Misdirection only moves the threat of the next three shots. Spend them on Freya rather than
    // leaving them to whatever the rotation picks - never on an add the tank does not want.
    Unit* freya = GetFirstAliveUnitByEntry(botAI, NPC_FREYA);
    if (freya && bot->HasAura(SPELL_MISDIRECTION) && botAI->CanCastSpell("steady shot", freya))
        return botAI->CastSpell("steady shot", freya);

    return false;
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
