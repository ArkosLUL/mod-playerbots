/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEActions_Drakes.h"
#include "EoEData.h"
#include "EoEEncounter_Drakes.h"
#include "EoEEncounter_Malygos.h"
#include "MotionMaster.h"
#include "Playerbots.h"
#include "RaidDifficulty.h"
#include "Timer.h"
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <vector>

bool EoEFlyDrakeAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool EoEFlyDrakeAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake)
        return false;

    MotionMaster* mm = drake->GetMotionMaster();

    if (!_stackCalcAtMs || GetMSTimeDiffToNow(_stackCalcAtMs) >= DRAKE_STACK_RECALC_MS)
    {
        std::vector<Unit*> fields;
        GetStaticFields(bot, fields);
        _stackValid = GetDrakeStackPoint(bot, fields, _stackX, _stackY, _stackZ);
        _stackCalcAtMs = getMSTime();
    }

    if (_stackValid)
    {
        Unit* boss = GetMalygos(bot);

        if (drake->GetExactDist(_stackX, _stackY, _stackZ) > DRAKE_STACK_TOLERANCE)
        {
            bool const sameSpot =
                _issued && std::fabs(_stackX - _issuedX) < DRAKE_DESTINATION_EPSILON &&
                std::fabs(_stackY - _issuedY) < DRAKE_DESTINATION_EPSILON;

            if (!sameSpot || mm->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
            {
                float legX;
                float legY;
                GetDrakeApproachPoint(drake, boss, _stackX, _stackY, legX, legY);

                drake->SetCanFly(true);
                mm->Clear(false);
                mm->MovePoint(0, legX, legY, _stackZ, FORCED_MOVEMENT_NONE, 0.0f, 0.0f,
                              /*generatePath*/ false, /*forceDestination*/ true);
                drake->SendMovementFlagUpdate();
                _issuedX = _stackX;
                _issuedY = _stackY;
                _issued = true;
            }
            return true;
        }

        _issued = false;
        if (mm->GetCurrentMovementGeneratorType() != IDLE_MOTION_TYPE)
        {
            mm->Clear(false);
            mm->MoveIdle();
            drake->SendMovementFlagUpdate();
        }

        if (boss)
        {
            if (!drake->HasInArc(CAST_ANGLE_IN_FRONT, boss))
                drake->SetFacingToObject(boss);
        }

        return false;
    }

    Player* master = botAI->GetMaster();
    if (!master || !master->IsInMap(bot))
        return false;
    Unit* masterVehicle = master->GetVehicleBase();
    if (!masterVehicle)
        return false;

    if (drake->GetExactDist(masterVehicle) > DRAKE_FORMUP_RADIUS)
    {
        uint8 const numPlayers = IsRaid25Man(bot) ? EOE_RAID_SIZE_25MAN : EOE_RAID_SIZE_10MAN;
        float const quarter = static_cast<float>(M_PI_2);
        float const angle = static_cast<float>(botAI->GetGroupSlotIndex(bot)) * (EOE_TWO_PI - quarter) /
                                static_cast<float>(numPlayers) + quarter;
        drake->SetCanFly(true);
        mm->MoveFollow(masterVehicle, DRAKE_FORMUP_SPREAD, angle);
        drake->SendMovementFlagUpdate();
        return true;
    }
    return false;
}

bool EoEDrakeAttackAction::isUseful() { return GetMalygos(bot) != nullptr; }

bool EoEDrakeAttackAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool EoEDrakeAttackAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake)
        return false;

    std::vector<ObjectGuid> healers;
    GetDrakeHealerGuids(botAI, healers);

    if (IsDrakeHealer(botAI, healers))
        return HealRotation(drake, healers);

    Unit* boss = GetMalygos(bot);
    if (!boss)
        return false;

    return DpsRotation(drake, boss);
}

bool EoEDrakeAttackAction::CastDrakeSpell(Unit* target, uint32 spellId)
{
    return botAI->CanCastVehicleSpell(spellId, target) && botAI->CastVehicleSpell(spellId, target);
}

bool EoEDrakeAttackAction::DpsRotation(Unit* drake, Unit* target)
{
    if (drake->GetExactDist(target) > DRAKE_ATTACK_RANGE)
        return false;

    uint8 comboPoints = drake->GetComboPoints(target);

    uint32 surgeElapsed = 0;
    if (GetDrakeSurgeElapsedMs(botAI, surgeElapsed) && surgeElapsed < SURGE_BEAM_END_MS)
    {
        if (drake->HasAura(SPELL_FLAME_SHIELD))
        {
            return DrakeCanAffordWithShield(drake, SPELL_FLAME_SPIKE) &&
                   CastDrakeSpell(target, SPELL_FLAME_SPIKE);
        }

        if (comboPoints >= DRAKE_ENGULF_SURGE_COMBO && DrakeCanAffordWithShield(drake, SPELL_ENGULF_IN_FLAMES))
            return CastDrakeSpell(target, SPELL_ENGULF_IN_FLAMES);

        if (comboPoints < DRAKE_SHIELD_RESERVE_COMBO &&
            (!comboPoints || DrakeCanAffordWithShield(drake, SPELL_FLAME_SPIKE)))
            return CastDrakeSpell(target, SPELL_FLAME_SPIKE);

        return false;
    }

    if (comboPoints >= DRAKE_ENGULF_COMBO)
        return CastDrakeSpell(target, SPELL_ENGULF_IN_FLAMES);
    else
        return CastDrakeSpell(target, SPELL_FLAME_SPIKE);
}

bool EoEDrakeAttackAction::HealRotation(Unit* drake, std::vector<ObjectGuid> const& healers)
{
    uint32 surgeElapsed = 0;
    bool const fixated = GetDrakeSurgeElapsedMs(botAI, surgeElapsed) && surgeElapsed < SURGE_BEAM_END_MS;

    if (fixated && !drake->HasAura(SPELL_FLAME_SHIELD))
    {
        uint8 const comboPoints = drake->GetComboPoints();
        if (comboPoints >= DRAKE_LIFE_BURST_COMBO && DrakeCanAffordWithShield(drake, SPELL_LIFE_BURST))
            return botAI->CastVehicleSpell(SPELL_LIFE_BURST, drake);

        if (comboPoints < DRAKE_SHIELD_RESERVE_COMBO &&
            (!comboPoints || DrakeCanAffordWithShield(drake, SPELL_REVIVIFY)))
            return botAI->CastVehicleSpell(SPELL_REVIVIFY, drake);

        return false;
    }

    if (drake->GetComboPoints() < DRAKE_LIFE_BURST_COMBO)
    {
        if (fixated ? !DrakeCanAffordWithShield(drake, SPELL_REVIVIFY) : !DrakeCanAfford(drake, SPELL_REVIVIFY))
            return false;

        return botAI->CastVehicleSpell(SPELL_REVIVIFY, drake);
    }

    bool const canBurst = DrakeCanAfford(drake, SPELL_LIFE_BURST);

    std::vector<Unit*> flight;
    uint8 healerRank = 0;
    GetDrakeFlightAndHealerRank(botAI, healers, flight, healerRank);

    uint8 worstPct = 100;
    uint32 worstMissing = 0;
    for (Unit* other : flight)
    {
        uint32 const max = other->GetMaxHealth();
        if (!max)
            continue;

        uint8 const pct = static_cast<uint8>(other->GetHealth() * 100 / max);
        if (pct < worstPct)
        {
            worstPct = pct;
            worstMissing = max - other->GetHealth();
        }
    }

    if (canBurst && worstPct <= DRAKE_BURST_EMERGENCY_PCT)
        return botAI->CastVehicleSpell(SPELL_LIFE_BURST, drake);

    bool recentBurst = false;
    for (Unit* other : flight)
    {
        if (other == drake)
            continue;

        if (DrakeAuraRemainingMs(other, SPELL_LIFE_BURST) > DRAKE_LIFE_BURST_BUFF_MS - DRAKE_BURST_STAGGER_MS)
        {
            recentBurst = true;
            break;
        }
    }

    if (canBurst && !recentBurst)
    {
        if (DrakeAuraRemainingMs(drake, SPELL_LIFE_BURST) < DRAKE_LIFE_BURST_REFRESH_MS)
            return botAI->CastVehicleSpell(SPELL_LIFE_BURST, drake);

        if (worstPct <= DRAKE_BURST_HEALTH_PCT)
        {
            uint32 const wanted = (worstMissing + DRAKE_LIFE_BURST_HEAL - 1) / DRAKE_LIFE_BURST_HEAL;
            if (healerRank < wanted)
                return botAI->CastVehicleSpell(SPELL_LIFE_BURST, drake);
        }
    }

    if (drake->GetPower(POWER_ENERGY) >= DRAKE_HOLD_ENERGY_FLOOR)
        return botAI->CastVehicleSpell(SPELL_REVIVIFY, drake);

    return false;
}

bool DrakeSurgeShieldAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool DrakeSurgeShieldAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake)
        return false;

    // keep above the shield check, it restamps the fixate clock or the next pick reads as this one
    uint32 elapsed = 0;
    if (!GetDrakeSurgeElapsedMs(botAI, elapsed))
        return false;

    if (drake->HasAura(SPELL_FLAME_SHIELD))
        return false;

    uint8 const comboPoints = drake->GetComboPoints();

    if (!comboPoints)
        return false;

    uint32 const cover = DRAKE_SHIELD_BASE_MS + comboPoints * DRAKE_SHIELD_MS_PER_COMBO;
    uint32 const castAt = std::min(cover >= SURGE_BEAM_END_MS ? 0u : SURGE_BEAM_END_MS - cover, SURGE_BEAM_DELAY_MS);

    if (elapsed < castAt)
        return false;

    if (comboPoints > DRAKE_SHIELD_MAX_COMBO && elapsed < SURGE_BEAM_DELAY_MS)
        return false;

    if (!DrakeCanAfford(drake, SPELL_FLAME_SHIELD))
        return false;

    botAI->CastVehicleSpell(SPELL_FLAME_SHIELD, drake);

    return false;
}
