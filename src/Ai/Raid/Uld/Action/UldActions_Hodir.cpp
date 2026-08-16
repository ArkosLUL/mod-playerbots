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
#include "SpellMgr.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "Unit.h"

namespace
{
// Every icicle close enough to matter, of either entry. The drift entry is included because it is
// lethal while it is still falling; the trigger decides when it has stopped being a hazard.
std::vector<Creature*> CollectHodirIcicles(Player* bot, float radius)
{
    std::vector<Creature*> icicles;
    std::list<Creature*> found;

    bot->GetCreatureListWithEntryInGrid(found, NPC_HODIR_ICICLE_SMALL, radius);
    for (Creature* icicle : found)
        if (icicle && icicle->IsAlive())
            icicles.push_back(icicle);

    found.clear();
    bot->GetCreatureListWithEntryInGrid(found, NPC_HODIR_ICICLE_DRIFT, radius);
    for (Creature* icicle : found)
        if (icicle && icicle->IsAlive())
            icicles.push_back(icicle);

    return icicles;
}

}  // namespace

bool HodirMoveSnowpackedIcicleAction::isUseful()
{
    HodirNearSnowpackedIcicleTrigger trigger(botAI);
    return trigger.IsActive();
}

bool HodirMoveSnowpackedIcicleAction::Execute(Event /*event*/)
{
    Creature* shelter = GetHodirSharedShelter(botAI, bot);
    if (!shelter)
        return false;

    return MoveInside(bot->GetMapId(), shelter->GetPositionX(), shelter->GetPositionY(),
                      shelter->GetPositionZ(), ULDUAR_HODIR_SAFE_AREA_TOLERANCE,
                      MovementPriority::MOVEMENT_COMBAT);
}

bool HodirIcicleDodgeAction::isUseful()
{
    HodirIcicleDodgeTrigger trigger(botAI);
    return trigger.IsActive();
}

bool HodirIcicleDodgeAction::Execute(Event /*event*/)
{
    std::vector<Creature*> const icicles = CollectHodirIcicles(bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);
    if (icicles.empty())
        return false;

    Position anchor;
    float tolerance = 0.0f;
    bool const anchored = GetHodirAnchor(botAI, bot, anchor, tolerance);
    Position const ringCentre = GetHodirRingCentre(botAI, bot);

    // Rank candidates on staying in Starlight first, then on the smallest step. Keeping the buff is
    // the whole reason the raid stands here, and at a 5 yd ring inside an 8 yd zone a six-yard
    // sidestep traces a chord that lands back on the ring - so there is always an in-zone escape.
    // Smallest step second, because maximising distance from the hazard is what walks bots out of
    // the room.
    Position best;
    bool found = false;
    bool bestInZone = false;
    float bestStep = 0.0f;

    for (int ring = 1; ring * 2.0f <= ULDUAR_HODIR_DODGE_LEASH; ++ring)
    {
        float const step = ring * 2.0f;
        for (int i = 0; i < 8; ++i)
        {
            float const angle = static_cast<float>(i) * static_cast<float>(M_PI) / 4.0f;
            float const x = bot->GetPositionX() + std::cos(angle) * step;
            float const y = bot->GetPositionY() + std::sin(angle) * step;
            float const z = bot->GetPositionZ();
            Position const candidate(x, y, z);

            bool clear = true;
            for (Creature* icicle : icicles)
            {
                if (icicle->GetExactDist2d(x, y) < ULDUAR_HODIR_ICE_SHARDS_CLEAR)
                {
                    clear = false;
                    break;
                }
            }

            if (!clear)
                continue;

            if (anchored && candidate.GetExactDist2d(&anchor) > ULDUAR_HODIR_DODGE_LEASH)
                continue;

            if (!bot->IsWithinLOS(x, y, z))
                continue;

            bool const inZone = candidate.GetExactDist2d(&ringCentre) <= ULDUAR_HODIR_STARLIGHT_RADIUS;
            bool better = !found;
            if (!better && inZone != bestInZone)
                better = inZone;
            else if (!better && step < bestStep)
                better = true;

            if (!better)
                continue;

            best = candidate;
            bestInZone = inZone;
            bestStep = step;
            found = true;
        }

        // Rings are searched inside out, so once an in-zone candidate exists nothing further out can
        // beat it on either key.
        if (found && bestInZone)
            break;
    }

    if (!found)
    {
        Creature* nearest = nullptr;
        for (Creature* icicle : icicles)
            if (!nearest || bot->GetExactDist2d(icicle) < bot->GetExactDist2d(nearest))
                nearest = icicle;

        if (!nearest)
            return false;

        return FleePosition(nearest->GetPosition(), ULDUAR_HODIR_ICE_SHARDS_CLEAR, 500);
    }

    return MoveTo(bot->GetMapId(), best.GetPositionX(), best.GetPositionY(), best.GetPositionZ(), false,
                  false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

bool HodirBitingColdJumpAction::isUseful()
{
    HodirBitingColdTrigger trigger(botAI);
    return trigger.IsActive();
}

bool HodirBitingColdJumpAction::Execute(Event /*event*/)
{
    Position anchor;
    float tolerance = 0.0f;
    if (!GetHodirAnchor(botAI, bot, anchor, tolerance))
        anchor = bot->GetPosition();

    // Alternate between the anchor and a fixed offset from it. JumpTo refuses a destination it has
    // just used, so jumping on one exact spot would be dropped after the first hop; two yards stays
    // inside every arrival tolerance, so the hop never reads as leaving the anchor.
    float const bearing = static_cast<float>(bot->GetGUID().GetCounter() % 8) * static_cast<float>(M_PI) / 4.0f;
    Position const offset(anchor.GetPositionX() + std::cos(bearing) * ULDUAR_HODIR_JUMP_HOP,
                          anchor.GetPositionY() + std::sin(bearing) * ULDUAR_HODIR_JUMP_HOP,
                          anchor.GetPositionZ());

    Position const& target = bot->GetExactDist2d(&anchor) < bot->GetExactDist2d(&offset) ? offset : anchor;

    return JumpTo(bot->GetMapId(), target.GetPositionX(), target.GetPositionY(), target.GetPositionZ(),
                  MovementPriority::MOVEMENT_COMBAT);
}

bool HodirRaidPositionAction::Execute(Event /*event*/)
{
    Position anchor;
    float tolerance = 0.0f;
    if (!GetHodirAnchor(botAI, bot, anchor, tolerance))
    {
        // Melee have no anchor, but they still gain from not standing on each other while icicles
        // splash 4 yd.
        if (Player* crowd = GetNearestPlayerInRadius(bot, ULDUAR_HODIR_DECLUMP_RADIUS))
            return FleePosition(crowd->GetPosition(), ULDUAR_HODIR_DECLUMP_RADIUS, 1000);

        return false;
    }

    float const distance = bot->GetExactDist2d(&anchor);

    // Reach then hold. Without the latch the bot re-issues a move on every drift inside the
    // tolerance, and a moving bot cannot start a cast - it slides on the spot and never casts.
    if (_anchorReached && distance > tolerance * 2.0f)
        _anchorReached = false;

    if (_anchorReached || distance <= tolerance)
    {
        _anchorReached = true;
        return false;
    }

    return MoveInside(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(),
                      tolerance, MovementPriority::MOVEMENT_COMBAT);
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

bool HodirSpreadStormCloudAction::isUseful()
{
    HodirSpreadStormCloudTrigger trigger(botAI);
    if (trigger.IsActive())
        return true;

    // The lap direction is latched for one carry. Clearing it when the aura is gone is what lets the
    // next Storm Cloud pick a fresh direction instead of inheriting the last one.
    _direction = 0;
    return false;
}

bool HodirSpreadStormCloudAction::Execute(Event /*event*/)
{
    Position const centre = GetHodirRingCentre(botAI, bot);
    uint32 const stormPower = sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_POWER, bot);

    float const botAngle = std::atan2(bot->GetPositionY() - centre.GetPositionY(),
                                      bot->GetPositionX() - centre.GetPositionX());

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

    float const x = centre.GetPositionX() + std::cos(nextAngle) * ULDUAR_HODIR_RAID_RING_RADIUS;
    float const y = centre.GetPositionY() + std::sin(nextAngle) * ULDUAR_HODIR_RAID_RING_RADIUS;
    float z = bot->GetMapWaterOrGroundLevel(x, y, centre.GetPositionZ());
    if (z <= INVALID_HEIGHT)
        z = centre.GetPositionZ();

    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}
