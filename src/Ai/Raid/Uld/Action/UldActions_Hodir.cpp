#include "UldActions_Hodir.h"
#include "UldActions_Shared.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Position.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Timer.h"
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

void BreakCastPinningTheFeet(PlayerbotAI* botAI, Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        botAI->RequestSpellInterrupt();
}

void ReleaseStalledWalk(PlayerbotAI* botAI, Player* bot)
{
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (!bot->isMoving() && last.msTime && getMSTimeDiff(last.msTime, getMSTime()) > ULDUAR_HODIR_STALL_MS)
        last.clear();
}

void CutShedHop(PlayerbotAI* botAI, Player* bot)
{
    if (!IsHodirShedHopInFlight(bot))
        return;

    // Clear(false) on purpose, a reset stops the bot mid-air if nothing moves it next.
    bot->GetMotionMaster()->Clear(false);
    botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get().clear();

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.shuttle", "cut");
}

}

bool HodirMoveSnowpackedIcicleAction::Execute(Event /*event*/)
{
    BreakCastPinningTheFeet(botAI, bot);
    CutShedHop(botAI, bot);

    Creature* shelter = GetHodirShelter(botAI, bot);
    if (!shelter)
        return false;

    ReleaseStalledWalk(botAI, bot);
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
    BreakCastPinningTheFeet(botAI, bot);
    CutShedHop(botAI, bot);

    std::vector<HazardCircle> const hazards = CollectHodirIcicleHazards(
        bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS, ULDUAR_HODIR_ICE_SHARDS_CLEAR, ULDUAR_HODIR_BIG_SHARDS_CLEAR);
    if (hazards.empty())
    {
        _dest = Position();
        return false;
    }

    if (!IsEmptyPosition(_dest) && IsClearOfHazards(_dest, hazards))
    {
        float const remaining = bot->GetExactDist2d(&_dest);
        if (remaining <= ULDUAR_HODIR_DODGE_ARRIVE)
            return false;

        if (remaining <= _destDist + ULDUAR_HODIR_DODGE_SLIP)
        {
            _destDist = std::min(_destDist, remaining);
            ReleaseStalledWalk(botAI, bot);
            return MoveTo(bot->GetMapId(), _dest.GetPositionX(), _dest.GetPositionY(), _dest.GetPositionZ(),
                          false, false, false, false, MovementPriority::MOVEMENT_FORCED);
        }
    }

    Position preference;
    Position const* preferNear = GetHodirDodgePreference(botAI, bot, preference) ? &preference : nullptr;

    Position dest = FindNearestPositionClearOfHazards(bot, hazards, ULDUAR_HODIR_DODGE_LEASH, ULDUAR_HODIR_SWEEP_STEP,
                                                      ULDUAR_HODIR_SWEEP_ANGLE_STEP, preferNear);

    if (IsEmptyPosition(dest))
        dest = FindNearestPositionClearOfHazards(
            bot,
            CollectHodirIcicleHazards(bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS,
                                      ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN,
                                      ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN),
            ULDUAR_HODIR_DODGE_LEASH * 2.0f, ULDUAR_HODIR_SWEEP_STEP, ULDUAR_HODIR_SWEEP_ANGLE_STEP, preferNear);

    if (IsEmptyPosition(dest))
    {
        _dest = Position();
        return false;
    }

    _dest = dest;
    _destDist = bot->GetExactDist2d(&_dest);

    return MoveTo(bot->GetMapId(), dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), false,
                  false, false, false, MovementPriority::MOVEMENT_FORCED);
}

bool HodirBitingColdShedAction::Execute(Event /*event*/)
{
    Aura* cold = bot->GetAura(SPELL_BITING_COLD_PLAYER_AURA);
    if (!cold)
    {
        StopShedding();
        return false;
    }

    if (!_shedding && !IsHodirBitingColdShedArmed(botAI, bot))
        return false;

    if (_shedding && cold->GetStackAmount() <= ULDUAR_HODIR_BITING_COLD_SHED_FLOOR)
    {
        StopShedding();
        return false;
    }

    _shedding = true;

    if (IsHodirShedHopInFlight(bot))
        return false;

    if (bot->IsMovementPreventedByCasting())
    {
        if (!bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            return false;

        botAI->RequestSpellInterrupt();
    }

    Position hop;
    uint32 flightMs = 0;
    if (GetHodirShedHop(botAI, bot, hop, flightMs))
    {
        _leg = Position();
        ReleaseStalledWalk(botAI, bot);

        float const speedXY = bot->GetExactDist(&hop) * IN_MILLISECONDS / flightMs;
        if (!JumpTo(bot->GetMapId(), hop.GetPositionX(), hop.GetPositionY(), hop.GetPositionZ(),
                    MovementPriority::MOVEMENT_COMBAT, speedXY, ULDUAR_HODIR_SHED_HOP_SPEED_Z))
            return false;

        LatchHodirShedHop(bot, hop);
        return true;
    }

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

            ReleaseStalledWalk(botAI, bot);
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

void HodirBitingColdShedAction::StopShedding()
{
    _shedding = false;
    _leg = Position();
    DropHodirShedHop(bot);
}

bool HodirRaidPositionAction::Execute(Event /*event*/)
{
    Position anchor;
    float tolerance = 0.0f;
    if (GetHodirAnchor(botAI, bot, anchor, tolerance))
    {
        if (bot->GetExactDist2d(&anchor) <= tolerance)
            return false;

        return MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    std::vector<HazardCircle> hazards;

    for (ObjectGuid const& guid : AI_VALUE(GuidVector, "nearest friendly players"))
    {
        Unit* ally = botAI->GetUnit(guid);
        if (!ally || ally == bot || !ally->IsAlive() || !ally->IsPlayer())
            continue;

        if (!PlayerbotAI::IsRanged(ally->ToPlayer()) && !PlayerbotAI::IsHeal(ally->ToPlayer()))
            continue;

        if (bot->GetExactDist2d(ally) <= ULDUAR_HODIR_DODGE_LEASH)
            hazards.emplace_back(ally->GetPosition(), ULDUAR_HODIR_DECLUMP_RADIUS);
    }

    for (HazardCircle const& icicle : CollectHodirIcicleHazards(bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS,
                                                               ULDUAR_HODIR_ICE_SHARDS_CLEAR,
                                                               ULDUAR_HODIR_BIG_SHARDS_CLEAR))
        hazards.push_back(icicle);

    Unit* hodir = GetHodir(botAI);
    if (hodir)
        hazards.emplace_back(hodir->GetPosition(), ULDUAR_HODIR_RANGED_MIN_BOSS_GAP);

    auto inBand = [&](float x, float y)
    { return !hodir || hodir->GetExactDist2d(x, y) <= ULDUAR_HODIR_CASTER_MAX_BOSS_GAP; };

    Position const dest = FindNearestPositionClearOfHazards(
        bot, hazards, ULDUAR_HODIR_CASTER_MAX_BOSS_GAP, ULDUAR_HODIR_SWEEP_STEP, ULDUAR_HODIR_SWEEP_ANGLE_STEP, nullptr,
        inBand);
    if (IsEmptyPosition(dest))
        return false;

    ReleaseStalledWalk(botAI, bot);
    return MoveTo(bot->GetMapId(), dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT);
}

Unit* HodirSetDpsPriorityAction::ResolveTarget()
{
    Unit* hodir = GetHodir(botAI);
    if (!hodir)
        return nullptr;

    if (PlayerbotAI::IsTank(bot))
        return hodir;

    Unit* trappedAlly = nullptr;

    for (ObjectGuid const& guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_HODIR_FLASH_FREEZE_PLAYER)
            continue;

        if (bot->GetExactDist2d(unit) > ULDUAR_HODIR_TRAPPED_ALLY_RANGE)
            continue;

        if (!IsHodirTrappedAllyBreaker(bot, unit))
            continue;

        if (!trappedAlly ||
            bot->GetExactDist2d(unit) + ULDUAR_HODIR_TRAPPED_ALLY_SWITCH_MARGIN < bot->GetExactDist2d(trappedAlly))
            trappedAlly = unit;
    }

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
    {
        std::string value = RaidObs::DescribeAssignment(target->GetGUID());
        if (char const* kind = GetHodirHelperBlockKind(target))
            value = std::string(kind) + " " + value;

        RaidObs::NoteDerived(bot, "hodir.dpstarget", value);
    }

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
    return CastClassTaunt(botAI, GetHodir(botAI));
}

bool HodirTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetHodirTankDefensive(botAI, bot, "hodir.defensive");
    if (!spellId || !botAI->CastSpell(spellId, bot))
        return false;

    LatchHodirTankDefensive(botAI, bot);
    return true;
}

Player* HodirRedirectThreatAction::GetRedirectTank()
{
    if (Player* holding = GetTankHolding(GetHodir(botAI)))
        return holding;

    return GetGroupMainTank(bot);
}

Unit* HodirRedirectThreatAction::GetThreatDumpTarget() { return GetHodir(botAI); }

bool HodirSpreadStormCloudAction::Execute(Event /*event*/)
{
    Position rally;
    if (!GetHodirStormCloudRally(bot, bot, rally))
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.stormcloud", "rally " + RaidObs::DescribeDerived(rally));

    if (bot->GetExactDist2d(&rally) <= ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK)
        return false;

    return MoveTo(bot->GetMapId(), rally.GetPositionX(), rally.GetPositionY(), rally.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT);
}

bool HodirCollectStormPowerAction::Execute(Event /*event*/)
{
    Player* carrier = GetHodirStormCloudCarrier(botAI, bot);
    if (!carrier || carrier == bot)
        return false;

    Position rally;
    if (!GetHodirStormCloudRally(bot, carrier, rally))
        return false;

    if (bot->GetExactDist2d(&rally) <= ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK)
        return false;

    return MoveTo(bot->GetMapId(), rally.GetPositionX(), rally.GetPositionY(), rally.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT);
}
