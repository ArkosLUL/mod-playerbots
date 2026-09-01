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
#include "RaidObs.h"
#include "Position.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "UldEncounter_Hodir.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "Unit.h"

using namespace EncounterHelpers;

namespace
{
// Where every icicle that has not detonated yet is standing, each carrying the distance its own pool
// needs. The drift entry is included because it is lethal on the way down; once it lands it marks the
// shelter and IsHodirIcicleLethal stops reporting it. The two clears are passed in because the dodge
// sweeps twice: once for real margin, once tightened to the radius that actually kills.
std::vector<HazardCircle> CollectHodirIcicleHazards(Player* bot, float radius, float smallClear, float bigClear)
{
    std::vector<HazardCircle> hazards;

    for (auto const& entry : {std::make_pair(static_cast<uint32>(NPC_HODIR_ICICLE_SMALL), smallClear),
                              std::make_pair(static_cast<uint32>(NPC_HODIR_ICICLE_DRIFT), bigClear)})
    {
        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, entry.first, radius);
        for (Creature* icicle : found)
            if (IsHodirIcicleLethal(icicle))
                hazards.emplace_back(icicle->GetPosition(), entry.second);
    }

    return hazards;
}

bool IsEmptyPosition(Position const& position)
{
    return !position.GetPositionX() && !position.GetPositionY();
}

bool IsClearOfHazards(Position const& position, std::vector<HazardCircle> const& hazards)
{
    for (HazardCircle const& hazard : hazards)
        if (hazard.first.GetExactDist2d(&position) < hazard.second)
            return false;

    return true;
}

// Which of several equally short dodge spots the bot would rather have. Melee want the boss; ranged and
// healers want the ring slot they were pulled off. Tanks never reach here - the dodge trigger excludes
// them - and a bot with no anchor gets no preference, which is the old behaviour.
bool GetHodirDodgePreference(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!PlayerbotAI::IsMelee(bot))
    {
        float tolerance = 0.0f;
        return GetHodirAnchor(botAI, bot, out, tolerance);
    }

    Unit* boss = GetHodir(botAI);
    if (!boss)
        return false;

    out = boss->GetPosition();
    return true;
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

bool HodirFrostResistanceAction::Execute(Event /*event*/)
{
    return botAI->DoSpecificAction("frost resistance aura", Event(), true);
}

bool HodirIcicleDodgeAction::Execute(Event /*event*/)
{
    std::vector<HazardCircle> const hazards = CollectHodirIcicleHazards(
        bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS, ULDUAR_HODIR_ICE_SHARDS_CLEAR, ULDUAR_HODIR_BIG_SHARDS_CLEAR);
    if (hazards.empty())
    {
        _dest = Position();
        return false;
    }

    // Keep walking to the spot already chosen. The sweep below searches out from wherever the bot is
    // standing, so deriving it again every tick chases its own answer outward - measured at a fresh
    // destination every 420ms, each 2 yd past the last, 89% of them further from the boss, and every
    // accepted MoveTo clearing the MotionMaster so the walk never finished. Re-offering the same point
    // answers Duplicate and returns false, which is correct: the forced walk still holds the slot.
    if (!IsEmptyPosition(_dest) && IsClearOfHazards(_dest, hazards))
    {
        float const remaining = bot->GetExactDist2d(&_dest);
        if (remaining <= ULDUAR_HODIR_DODGE_ARRIVE)
            _dest = Position();
        else if (remaining <= _destDist + ULDUAR_HODIR_DODGE_SLIP)
        {
            _destDist = std::min(_destDist, remaining);
            return MoveTo(bot->GetMapId(), _dest.GetPositionX(), _dest.GetPositionY(), _dest.GetPositionZ(),
                          false, false, false, false, MovementPriority::MOVEMENT_FORCED);
        }
    }

    Position preference;
    Position const* preferNear = GetHodirDodgePreference(botAI, bot, preference) ? &preference : nullptr;

    // The shared helper rings outward and validates every candidate against the collision mesh, so
    // the first hit is both the shortest walk and somewhere MoveTo will actually accept. FleePosition
    // is the wrong tool here: it clamps travel to AiPlayerbot.FleeDistance and reads one hazard, so
    // the second icicle of a volley leaves the bot standing in the blast.
    Position dest = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_HODIR_DODGE_LEASH, 2.0f,
                                                      static_cast<float>(M_PI) / 8.0f, preferNear);

    // Nothing fully clear: the clears above were margin, these are the radii that actually kill, and
    // moving to the least bad spot beats holding still because the sweep found no perfect one.
    if (IsEmptyPosition(dest))
        dest = FindNearestPositionClearOfHazards(
            bot,
            CollectHodirIcicleHazards(bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS, ULDUAR_HODIR_ICE_SHARDS_RADIUS + 0.5f,
                                      ULDUAR_HODIR_BIG_SHARDS_RADIUS + 0.5f),
            ULDUAR_HODIR_DODGE_LEASH * 2.0f, 2.0f, static_cast<float>(M_PI) / 8.0f, preferNear);

    if (IsEmptyPosition(dest))
    {
        _dest = Position();
        return false;
    }

    _dest = dest;
    _destDist = bot->GetExactDist2d(&_dest);

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

    // The aura, not a zone lookup: it is what the bot is actually paid for, it costs no grid sweep,
    // and it is the thing about to be lost. The leg the shuttle picks stays inside the zone, so the
    // extra stack buys uninterrupted haste rather than an uninterrupted stand.
    uint32 const arm = bot->HasAura(SPELL_HODIR_STARLIGHT) ? ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT
                                                           : ULDUAR_HODIR_BITING_COLD_SHED_STACKS;

    if (!_shedding && cold->GetStackAmount() < arm)
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

    // No arrival latch here. The trigger stands down inside the tolerance, which is what the latch
    // used to be for, and holding one across ticks would swallow the reactive re-anchors: a bot that
    // has to move because it is clumped or standing in Hodir's melee is usually still well inside
    // twice the tolerance, so a latched action would refuse the move the trigger just asked for.
    if (bot->GetExactDist2d(&anchor) <= tolerance)
        return false;

    // The exact slot, not MoveInside: that one offsets the destination by the tolerance at the bot's
    // follow angle, which parks everyone a couple of yards off-formation in an unrelated direction
    // and eats the spacing the layout is built on.
    return MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(), false,
                  false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

Unit* HodirSetDpsPriorityAction::ResolveTarget()
{
    Unit* hodir = GetHodir(botAI);
    if (!hodir)
        return nullptr;

    // Tanks never break ice. He follows whoever holds him, so a tank that walks 20 yd to a block puts
    // him on the ranged formation, and a freed helper is worth a lot less than that costs.
    if (botAI->IsTank(bot))
        return hodir;

    Unit* trappedAlly = nullptr;

    for (auto const& guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_HODIR_FLASH_FREEZE_PLAYER)
            continue;

        if (bot->GetExactDist2d(unit) > ULDUAR_HODIR_TRAPPED_ALLY_RANGE)
            continue;

        if (!IsHodirTrappedAllyBreaker(botAI, bot, unit))
            continue;

        // Sticky: switch only when another is clearly closer, or two bots standing either side of a
        // pair ping-pong between them.
        if (!trappedAlly || bot->GetExactDist2d(unit) + 10.0f < bot->GetExactDist2d(trappedAlly))
            trappedAlly = unit;
    }

    // Helper blocks are assigned, not searched for: one breaker each across every block that is up.
    // A per-block cap multiplies by the eight helpers one Flash Freeze puts up at once, which is how
    // the whole raid ended up on ice with as many as eleven bots on a single block. No stickiness
    // needed either - the assignment is guid-ordered, so it does not move on its own.
    //
    // Skipped when a trapped raider is already in hand, since that outranks a helper below and the
    // assignment costs a grid pass.
    Unit* helperBlock = trappedAlly ? nullptr : GetHodirAssignedHelperBlock(botAI, bot);

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

    // No stickiness across entries. Holding a block because it outranks the boss on this list is what
    // parked the raid on helper ice for the rest of the fight: the block outranks Hodir for as long as
    // it lives, whether or not this bot is still assigned to it. The 10 yd margin in the scan above
    // already stops two bots ping-ponging between adjacent blocks, which is all the hysteresis needed.
    if (!target)
        target = AI_VALUE(Unit*, "dps target");

    return target;
}

bool HodirSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = context->GetValue<Unit*>("current target")->Get();
    Unit* target = ResolveTarget();
    if (!target)
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.dpstarget", RaidObs::DescribeAssignment(target->GetGUID()));

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
    // Deliberately unprobed. Every taunt is an OK verdict on this action and every failed one a FAILED,
    // both already in the act stream with a repeat count - and all four class taunts share an 8s
    // cooldown, so a swap that has to wait for it retries at tick rate until it lands.
    return CastClassTaunt(botAI, GetHodir(botAI));
}

Player* HodirRedirectThreatAction::GetRedirectTank()
{
    // Follow the swap rather than mirroring its state: whoever is holding him is the one who needs
    // the lead, and that is the off-tank for the length of a Frozen Blows window.
    if (Player* holding = GetTankHolding(GetHodir(botAI)))
        return holding;

    // Hodir is on somebody who is not a tank, which is the case the redirect exists for. Feed the
    // main tank so the taunt that follows has something to hold.
    return GetGroupMainTank(bot);
}

Unit* HodirRedirectThreatAction::GetThreatDumpTarget() { return GetHodir(botAI); }

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
