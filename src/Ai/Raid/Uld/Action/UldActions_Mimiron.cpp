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

bool MimironFleeAction::MoveAwayClearOfMines(Unit* from, float distance)
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

            if (MoveTo(from->GetMapId(), dx, dy, dz, false, false, true, exact,
                       MovementPriority::MOVEMENT_COMBAT))
                return true;
        }
    }

    // Every mine-clear bearing was refused. Whatever is being fled here hurts more than a mine, so
    // take the unfiltered fan rather than standing in it.
    return MoveAway(from, distance);
}

bool MimironShockBlastAction::Execute(Event /*event*/)
{
    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (!leviathanMkII)
        return false;

    // Phases 3 and 4 used to teleport out of this instead of running. Running works in every phase;
    // what it needed was the node priority to win the tick.
    if (!MoveAwayClearOfMines(leviathanMkII, 20.0f - bot->GetDistance2d(leviathanMkII)))
        return false;

    if (botAI->IsMelee(bot))
        botAI->SetNextCheckDelay(100);

    return true;
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
    Unit* boss = AI_VALUE2(Unit*, "find target", "vx-001");
    if (!boss || !boss->IsAlive())
        return false;

    MimironBarrageArc const& arc = GetMimironLatchedBarrageArc(bot, boss);

    // Bearing relative to the cone's centreline, folded to (-pi, pi].
    float delta = Position::NormalizeOrientation(boss->GetAngle(bot) - arc.angle);
    if (delta > static_cast<float>(M_PI))
        delta -= 2.0f * static_cast<float>(M_PI);

    float const clearance = ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE + ULDUAR_MIMIRON_BARRAGE_MARGIN;
    float const cwRest = -(clearance + ULDUAR_MIMIRON_BARRAGE_SWEEP_TOTAL);

    // Outside everything the cone reaches before it expires. Returning false rather than true is
    // deliberate: bots that were never in danger keep casting through the barrage.
    if (delta >= clearance || delta <= cwRest)
        return false;

    // Distance does not affect safety - the cone is 50000 yd long - so keep the bot's own radius and
    // change only its bearing. Melee stay in melee range, and a constant-radius turn is the shortest
    // bearing change there is.
    float const radius = std::min(bot->GetDistance2d(boss), ULDUAR_MIMIRON_SPREAD_RADIUS_MAX);
    float const turnRate = radius > 0.0f ? bot->GetSpeed(MOVE_RUN) / radius : 0.0f;

    // Counter-clockwise leaves by the edge the cone is retreating from, so the sweep helps. Clockwise
    // runs with the sweep and only gains ground on the difference of the two rates - impossible past
    // 38 yd, where the cone turns as fast as the bot does. Take whichever clears sooner; that puts
    // the switchover near -21 degrees rather than at the centreline.
    float const ccwSeconds = turnRate > 0.0f
        ? (clearance - delta) / (turnRate + ULDUAR_MIMIRON_BARRAGE_SWEEP_RATE)
        : std::numeric_limits<float>::max();

    float cwSeconds = std::numeric_limits<float>::max();
    if (turnRate > ULDUAR_MIMIRON_BARRAGE_SWEEP_RATE)
        cwSeconds = (delta + clearance) / (turnRate - ULDUAR_MIMIRON_BARRAGE_SWEEP_RATE);

    // Clockwise has to run all the way past where the trailing edge finishes: stopping short lets
    // the sweep catch back up.
    float const heading =
        Position::NormalizeOrientation(arc.angle + (ccwSeconds <= cwSeconds ? clearance : cwRest));

    MoveTo(boss->GetMapId(), boss->GetPositionX() + radius * cos(heading),
           boss->GetPositionY() + radius * sin(heading), boss->GetPositionZ(), false, false, false,
           true, MovementPriority::MOVEMENT_FORCED, true);

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

    // Ten mines land eight seconds after every Shock Blast, and nothing in pathing knows they are
    // there. Holding position beats walking the whole way into a field.
    if (!IsMimironSpotMineSafe(bot, slot))
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

        return false;
    }

    // Bomb Bots detonate for 5 yd and Junk Bots pile in, so nobody outside the tanks should be
    // standing on top of another bot here.
    if (AI_VALUE(float, "disperse distance") != 5.0f)
        SET_AI_VALUE(float, "disperse distance", 5.0f);

    return true;
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
    // teleported instead, off a stale pointer left over from the mech sweep.
    return MoveAwayClearOfMines(rocketStrikeN, 10.0f);
}

bool MimironPhase4MarkDpsAction::Execute(Event /*event*/)
{
    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001);
    Unit* aerialCommandUnit = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);

    if (!leviathanMkII || !vx001 || !aerialCommandUnit)
        return false;

    if (botAI->IsMainTank(bot))
    {
        // All three reassemble in phase 4 and only die together, so the tank holds whichever is
        // furthest from dead and the raid follows it there.
        Unit* focus = leviathanMkII;
        if (vx001->GetHealth() > focus->GetHealth())
            focus = vx001;
        if (aerialCommandUnit->GetHealth() > focus->GetHealth())
            focus = aerialCommandUnit;

        // The skull is for the human raid leader; bots take their own target below and through
        // "mimiron set dps priority", so nothing depends on the icon landing.
        MarkTargetWithSkull(bot, focus);

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

    // A Bomb Bot matches player run speed and detonates on melee contact, so backing away never
    // wins - ranged have to kill it. The Assault Bot comes next because it is the only Magnetic Core
    // source, and the core is what grounds the Aerial Command Unit, so stopping for fire bots or
    // Junk Bots ahead of it only stretches the phase.
    std::vector<std::pair<uint32, Unit*>> priority;
    if (!botAI->IsMelee(bot))
        priority.emplace_back(NPC_BOMB_BOT, SelectByEntry(currentTarget, NPC_BOMB_BOT, bombBots));

    priority.emplace_back(NPC_ASSAULT_BOT, SelectByEntry(currentTarget, NPC_ASSAULT_BOT, assaultBots));

    if (IsMimironHardModeActive(botAI))
        priority.emplace_back(NPC_EMERGENCY_FIRE_BOT,
                              SelectByEntry(currentTarget, NPC_EMERGENCY_FIRE_BOT, fireBots));

    priority.emplace_back(NPC_JUNK_BOT, SelectByEntry(currentTarget, NPC_JUNK_BOT, junkBots));

    // Only one mech is up before phase 4, so this ordering only bites there: all three reassemble and
    // the raid has to even them out, which means staying on whichever has the most health left.
    std::vector<Unit*> mechs;
    for (Unit* mech : {leviathanMkII, vx001, aerialCommandUnit})
        if (mech)
            mechs.push_back(mech);

    std::sort(mechs.begin(), mechs.end(),
              [](Unit* lhs, Unit* rhs) { return lhs->GetHealth() > rhs->GetHealth(); });

    for (Unit* mech : mechs)
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
            // It hovers out of reach until a Magnetic Core grounds it, so melee stay on the adds
            // instead of trailing it around the room.
            return !botAI->IsMelee(bot) || !unit->HasUnitMovementFlag(MOVEMENTFLAG_HOVER);

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

    if (AI_VALUE(Unit*, "current target") != leviathanMkII)
        return Attack(leviathanMkII);

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
            bot->FindNearestCreature(NPC_ASSAULT_BOT, ULDUAR_MIMIRON_CORE_LOOT_RANGE, false);
        if (!corpse)
            return false;

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
