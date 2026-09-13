#include "UldActions_Hodir.h"
#include "UldActions_Shared.h"

#include <algorithm>
#include <cmath>
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

// PointMovementGenerator never launches a spline while IsMovementPreventedByCasting is true, so the
// MoveTo below is accepted, books the movement slot for a second, and the bot stands still until the
// cast ends. Blizzard held one caster in an Ice Shards pool for 5.7s and Evocation another for 3.5s;
// both died there. Only the two nodes that run when the bot is already in danger call this - a cast
// is worth more than a shed leg, and less than 14000 from Ice Shards or a Flash Freeze.
void BreakCastPinningTheFeet(PlayerbotAI* botAI, Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        botAI->RequestSpellInterrupt();
}

}  // namespace

bool HodirMoveSnowpackedIcicleAction::Execute(Event /*event*/)
{
    // The trigger fires only outside the release ring with somewhere to shelter on the floor, so
    // reaching here always means the bot has to cross ground it is not standing on yet.
    BreakCastPinningTheFeet(botAI, bot);

    Creature* shelter = GetHodirShelter(botAI, bot);
    if (!shelter)
        return false;

    // MoveInside answers false for a bot already inside the radius, so one caught inside a falling
    // drift's blast falls through to the dodge below, which collects drifts at the same clear and
    // walks it out. Nothing here has to push.
    return MoveInside(bot->GetMapId(), shelter->GetPositionX(), shelter->GetPositionY(),
                      shelter->GetPositionZ(), GetHodirShelterPark(shelter),
                      MovementPriority::MOVEMENT_COMBAT);
}

bool HodirFrostResistanceAction::Execute(Event /*event*/)
{
    return botAI->DoSpecificAction("frost resistance aura", Event(), true);
}

bool HodirIcicleDodgeAction::Execute(Event /*event*/)
{
    // The trigger fires only inside a lethal radius, and it stands tanks down, so every bot that
    // reaches here is standing somewhere that kills and has about 3s of icicle fuse to leave it.
    BreakCastPinningTheFeet(botAI, bot);

    std::vector<HazardCircle> const hazards = CollectHodirIcicleHazards(
        bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS, ULDUAR_HODIR_ICE_SHARDS_CLEAR, ULDUAR_HODIR_BIG_SHARDS_CLEAR);
    if (hazards.empty())
    {
        _dest = Position();
        return false;
    }

    // Hold the spot already chosen, arrival included, and re-validate it against the live hazard list
    // rather than sweeping again. The sweep below rings outward from wherever the bot is standing in
    // 2 yd steps, so deriving every tick just hands it another 2 yd hop - 4197 forced moves in a six
    // minute pull, a fresh destination every 410ms, 71% under half a second apart, each accepted MoveTo
    // clearing the MotionMaster so the walk never finished. Clearing the spot on arrival dropped
    // through to that sweep in the same tick, which is where the loop came from. Only a spot that has
    // stopped being clear is worth a new one; re-offering the same point answers Duplicate and returns
    // false, which is correct, because the forced walk still holds the slot.
    if (!IsEmptyPosition(_dest) && IsClearOfHazards(_dest, hazards))
    {
        float const remaining = bot->GetExactDist2d(&_dest);
        if (remaining <= ULDUAR_HODIR_DODGE_ARRIVE)
            return false;

        if (remaining <= _destDist + ULDUAR_HODIR_DODGE_SLIP)
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
        _leg = Position();
        return false;
    }

    // Shared with everything that steps aside for the shed, so the two cannot disagree about whether
    // one is coming. It reads the aura rather than a zone lookup: that is what the bot is actually
    // paid for, it costs no grid sweep, and it is the thing about to be lost.
    if (!_shedding && !IsHodirBitingColdShedArmed(bot))
        return false;

    // Once started, keep going until the aura is gone. A stack comes off only on the second moving
    // tick and any stationary tick in between resets that progress, so stopping early wastes the
    // movement already spent.
    _shedding = true;

    // The walk holds one leg instead of re-deriving under its own walk. Stateless it issued 6686 moves
    // against 4122 refusals and turned the bot around a median 879ms apart, which is the movement slot
    // the rest of the raid never got. On arrival it derives the next leg rather than stopping, because
    // it is the chain that covers consecutive aura ticks - one leg cannot.
    if (!IsEmptyPosition(_leg))
    {
        float const remaining = bot->GetExactDist2d(&_leg);
        bool const clear = IsClearOfHazards(
            _leg, CollectHodirIcicleHazards(bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS, ULDUAR_HODIR_ICE_SHARDS_CLEAR,
                                            ULDUAR_HODIR_BIG_SHARDS_CLEAR));

        if (clear && remaining > ULDUAR_HODIR_DODGE_ARRIVE && remaining <= _legDist + ULDUAR_HODIR_DODGE_SLIP)
        {
            _legDist = std::min(_legDist, remaining);

            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "hodir.shuttle", "held");

            return MoveTo(bot->GetMapId(), _leg.GetPositionX(), _leg.GetPositionY(), _leg.GetPositionZ(),
                          false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }

    Position leg;
    if (!GetHodirShuttleLeg(botAI, bot, leg))
    {
        _leg = Position();
        return false;
    }

    _leg = leg;
    _legDist = bot->GetExactDist2d(&_leg);

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
    Position rally;
    if (!GetHodirStormCloudRally(botAI, bot, bot, rally))
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.stormcloud", "rally " + RaidObs::DescribeDerived(rally));

    // Holding still is the whole job. Storm Power is an area pulse at the carrier's own feet reaching
    // 3 yd, and the carry has 6 charges spent about one a second, so a carrier that keeps walking
    // spends them on whoever it happens to pass - measured p50 0-1 raiders inside the pulse, and 4.4%
    // of raider-samples during a live carry. Lapping the ring was the best available answer while the
    // carrier was the only thing that could move; now the receivers come instead.
    //
    // The rally is seeded from where this bot was standing when the cloud landed, so MoveInside
    // ordinarily answers false straight away and the engine descends to whatever it can cast. It only
    // walks when something else - a dodge, a shelter run - has pushed the carrier off the point.
    return MoveInside(bot->GetMapId(), rally.GetPositionX(), rally.GetPositionY(), rally.GetPositionZ(),
                      ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK, MovementPriority::MOVEMENT_COMBAT);
}

bool HodirCollectStormPowerAction::Execute(Event /*event*/)
{
    Player* carrier = GetHodirStormCloudCarrier(botAI, bot);
    if (!carrier || carrier == bot)
        return false;

    Position rally;
    if (!GetHodirStormCloudRally(botAI, bot, carrier, rally))
        return false;

    // No cast is broken to get here, unlike the shelter run and the dodge. Those are avoiding a 14000
    // hit; this is collecting a buff, and a caster that drops a cast to walk 3 yd has already spent
    // more than the pulse is worth.
    return MoveInside(bot->GetMapId(), rally.GetPositionX(), rally.GetPositionY(), rally.GetPositionZ(),
                      ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK, MovementPriority::MOVEMENT_COMBAT);
}
