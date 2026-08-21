#include "UldActions_Hodir.h"
#include "UldActions_Shared.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <utility>
#include <vector>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "Group.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "Unit.h"

namespace
{
// Where every icicle that has not detonated yet is standing, both entries. The drift entry is
// included because it is lethal on the way down; once it lands it becomes the shelter and
// IsHodirIcicleLethal stops reporting it.
std::vector<Position> CollectHodirIcicleHazards(Player* bot, float radius)
{
    std::vector<Position> hazards;

    for (uint32 entry : {NPC_HODIR_ICICLE_SMALL, NPC_HODIR_ICICLE_DRIFT})
    {
        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, entry, radius);
        for (Creature* icicle : found)
            if (IsHodirIcicleLethal(icicle))
                hazards.push_back(icicle->GetPosition());
    }

    return hazards;
}

bool IsEmptyPosition(Position const& position)
{
    return !position.GetPositionX() && !position.GetPositionY();
}

}  // namespace

bool HodirMoveSnowpackedIcicleAction::Execute(Event /*event*/)
{
    Creature* shelter = GetHodirSharedShelter(botAI, bot);
    if (!shelter)
        return false;

    return MoveInside(bot->GetMapId(), shelter->GetPositionX(), shelter->GetPositionY(),
                      shelter->GetPositionZ(), ULDUAR_HODIR_SAFE_AREA_TOLERANCE,
                      MovementPriority::MOVEMENT_COMBAT);
}

bool HodirIcicleDodgeAction::Execute(Event /*event*/)
{
    std::vector<Position> const hazards = CollectHodirIcicleHazards(bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);
    if (hazards.empty())
        return false;

    // The shared helper rings outward and validates every candidate against the collision mesh, so
    // the first hit is both the shortest walk and somewhere MoveTo will actually accept. FleePosition
    // is the wrong tool here: it clamps travel to AiPlayerbot.FleeDistance and reads one hazard, so
    // the second icicle of a volley leaves the bot standing in the blast.
    Position dest = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_HODIR_ICE_SHARDS_CLEAR,
                                                      ULDUAR_HODIR_DODGE_LEASH);

    // Nothing fully clear: 6 yd was margin, 4 is the radius that actually kills, and moving to the
    // least bad spot beats holding still because the sweep found no perfect one.
    if (IsEmptyPosition(dest))
        dest = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_HODIR_ICE_SHARDS_RADIUS + 0.5f,
                                                 ULDUAR_HODIR_DODGE_LEASH * 2.0f);

    if (IsEmptyPosition(dest))
        return false;

    // FORCED so the dodge outranks the anchor walking the bot back in. A second icicle arriving
    // mid-dodge still cannot preempt this one - IsWaitingForLastMove only yields to a strictly higher
    // priority - which is what the 4.5 yd formation spacing is for. Do not escalate further.
    return MoveTo(bot->GetMapId(), dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), false,
                  false, false, false, MovementPriority::MOVEMENT_FORCED);
}

bool HodirBitingColdShedAction::Execute(Event /*event*/)
{
    Aura* cold = bot->GetAura(SPELL_BITING_COLD_PLAYER_AURA);
    if (!cold)
    {
        _shedding = false;
        return false;
    }

    if (!_shedding && cold->GetStackAmount() < ULDUAR_HODIR_BITING_COLD_SHED_STACKS)
        return false;

    // Once started, keep going until the aura is gone. A stack comes off only on the second moving
    // tick and any stationary tick in between resets that progress, so stopping early wastes the
    // movement already spent.
    _shedding = true;

    Position leg;
    if (!GetHodirShuttleLeg(botAI, bot, leg))
        return false;

    return MoveTo(bot->GetMapId(), leg.GetPositionX(), leg.GetPositionY(), leg.GetPositionZ(), false,
                  false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

bool HodirRaidPositionAction::Execute(Event /*event*/)
{
    Position anchor;
    float tolerance = 0.0f;
    if (!GetHodirAnchor(botAI, bot, anchor, tolerance))
        return false;

    float const distance = bot->GetExactDist2d(&anchor);

    // Reach then hold. Without the latch the bot re-issues a move on every drift inside the
    // tolerance, and a moving bot cannot start a cast - it slides on the spot and never casts. The
    // release threshold also has to clear the centre quantum, or a one-step druid shuffle restarts
    // the whole formation.
    if (_anchorReached && distance > tolerance * 2.0f)
        _anchorReached = false;

    if (_anchorReached || distance <= tolerance)
    {
        _anchorReached = true;
        return false;
    }

    // The exact slot, not MoveInside: that one offsets the destination by the tolerance at the bot's
    // follow angle, which parks everyone a couple of yards off-formation in an unrelated direction
    // and eats the spacing the layout is built on.
    return MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(), false,
                  false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

Unit* HodirSetDpsPriorityAction::ResolveTarget(Unit* currentTarget)
{
    Unit* hodir = GetHodir(botAI);
    if (!hodir)
        return nullptr;

    Unit* trappedAlly = nullptr;
    Unit* helperBlock = nullptr;

    for (auto const& guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (bot->GetExactDist2d(unit) > ULDUAR_HODIR_TRAPPED_ALLY_RANGE)
            continue;

        // Sticky per entry: switch blocks only when another is clearly closer, or two bots standing
        // either side of a pair ping-pong between them.
        if (unit->GetEntry() == NPC_HODIR_FLASH_FREEZE_PLAYER)
        {
            if (!IsHodirTrappedAllyBreaker(botAI, bot, unit))
                continue;

            if (!trappedAlly || bot->GetExactDist2d(unit) + 10.0f < bot->GetExactDist2d(trappedAlly))
                trappedAlly = unit;
        }
        else if (unit->GetEntry() == NPC_HODIR_FLASH_FREEZE_BLOCK)
        {
            if (!helperBlock || bot->GetExactDist2d(unit) + 10.0f < bot->GetExactDist2d(helperBlock))
                helperBlock = unit;
        }
    }

    std::vector<std::pair<uint32, Unit*>> const priority = {
        {static_cast<uint32>(NPC_HODIR_FLASH_FREEZE_PLAYER), trappedAlly},
        {static_cast<uint32>(NPC_HODIR_FLASH_FREEZE_BLOCK), helperBlock},
        {static_cast<uint32>(NPC_HODIR), hodir}};

    Unit* target = nullptr;
    for (auto const& candidate : priority)
    {
        if (candidate.second)
        {
            target = candidate.second;
            break;
        }
    }

    auto const priorityIndex = [&priority](Unit* unit) -> size_t
    {
        if (!unit || !unit->IsAlive())
            return priority.size();

        for (size_t i = 0; i < priority.size(); ++i)
            if (priority[i].first == unit->GetEntry())
                return i;

        return priority.size();
    };

    // Sticky across entries: hold what we have while it is at least as important as the new pick.
    if (currentTarget && priorityIndex(currentTarget) <= priorityIndex(target))
        target = currentTarget;

    if (!target)
        target = AI_VALUE(Unit*, "dps target");

    return target;
}

bool HodirSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = context->GetValue<Unit*>("current target")->Get();
    Unit* target = ResolveTarget(currentTarget);
    if (!target)
        return false;

    // No threat wipe in this fight, and it is a race - nobody should be throttling.
    if (target == GetHodir(botAI))
        context->GetValue<bool>("neglect threat")->Set(true);

    bool needsAttack = currentTarget != target;
    if (!needsAttack && PlayerbotAI::IsMelee(bot))
        needsAttack = !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    if (needsAttack)
        return Attack(target);

    return false;
}

bool HodirFrozenBlowsSwapAction::Execute(Event /*event*/)
{
    return UldCastClassTaunt(botAI, GetHodir(botAI));
}

bool HodirSpreadStormCloudAction::Execute(Event /*event*/)
{
    Position const centre = GetHodirRingCentre(botAI, bot);
    uint32 const stormPower = sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_POWER, bot);

    // Storm Cloud only sheds stacks, so a rise is a new carry and the lap direction has to be picked
    // again rather than inherited from the last one.
    Aura* cloud = bot->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, bot));
    uint8 const stacks = cloud ? cloud->GetStackAmount() : 0;
    if (stacks > _lastStacks)
        _direction = 0;
    _lastStacks = stacks;

    float const botAngle = std::atan2(bot->GetPositionY() - centre.GetPositionY(),
                                      bot->GetPositionX() - centre.GetPositionX());
    float const lapRadius = std::max(bot->GetExactDist2d(&centre), ULDUAR_HODIR_RAID_RING_INNER);

    // Storm Power reaches 3 yd and the carrier has only 4-6 one-second ticks, so it laps the ring
    // rather than stepping to one neighbour. The direction is chosen once, toward whichever way has
    // more allies still missing the buff, then held: re-deciding every tick makes it pace on the spot.
    if (!_direction)
    {
        uint32 ahead = 0;
        uint32 behind = 0;

        for (auto const& guid : AI_VALUE(GuidVector, "nearest friendly players"))
        {
            Unit* ally = botAI->GetUnit(guid);
            if (!ally || !ally->IsAlive() || ally == bot || ally->HasAura(stormPower))
                continue;

            // Tanks are skipped: they hold the corner, and walking one out of it for a damage buff
            // is a trade the raid loses.
            Player* allyPlayer = ally->ToPlayer();
            if (!allyPlayer || PlayerbotAI::IsTank(allyPlayer))
                continue;

            float const allyAngle = std::atan2(ally->GetPositionY() - centre.GetPositionY(),
                                               ally->GetPositionX() - centre.GetPositionX());
            if (Position::NormalizeOrientation(allyAngle - botAngle) < static_cast<float>(M_PI))
                ++ahead;
            else
                ++behind;
        }

        _direction = ahead >= behind ? 1 : -1;
    }

    float const nextAngle = Position::NormalizeOrientation(
        botAngle + static_cast<float>(_direction) * static_cast<float>(M_PI) / 4.0f);

    // Lap at whatever radius the carrier is already standing at, so an outer-ring carrier does not
    // dive through the middle of the formation on its way round.
    float const x = centre.GetPositionX() + std::cos(nextAngle) * lapRadius;
    float const y = centre.GetPositionY() + std::sin(nextAngle) * lapRadius;
    float z = bot->GetMapWaterOrGroundLevel(x, y, centre.GetPositionZ());
    if (z <= INVALID_HEIGHT)
        z = centre.GetPositionZ();

    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}
