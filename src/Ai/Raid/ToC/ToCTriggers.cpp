#include "ToCTriggers.h"
#include "ToCHelpers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"

using namespace TrialOfTheCrusaderHelpers;

// Gormok the Impaler

bool GormokEngagedByMainTankTrigger::IsActive()
{
    return botAI->IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK));
}

bool GormokSnoboldOnRaidTrigger::IsActive()
{
    // Only melee DPS peel onto Snobolds; ranged keep damaging Gormok so the boss still dies
    if (botAI->IsTank(bot) || botAI->IsHeal(bot) || !botAI->IsMelee(bot))
        return false;

    return GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK)) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_SNOBOLD_VASSAL));
}

// Acidmaw & Dreadscale

bool WormsMobileEngagedByMainTankTrigger::IsActive()
{
    if (!botAI->IsMainTank(bot))
        return false;

    Unit* acidmaw = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ACIDMAW));
    Unit* dreadscale = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_DREADSCALE));
    return IsWormMobile(acidmaw) || IsWormMobile(dreadscale);
}

bool WormsStationaryNeedsAssistTankTrigger::IsActive()
{
    if (!botAI->IsAssistTankOfIndex(bot, 0, false))
        return false;

    Unit* acidmaw = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ACIDMAW));
    Unit* dreadscale = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_DREADSCALE));
    return (acidmaw && !IsWormMobile(acidmaw)) || (dreadscale && !IsWormMobile(dreadscale));
}

bool WormsRangedShouldSpreadTrigger::IsActive()
{
    if (!botAI->IsRanged(bot))
        return false;

    if (!GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ACIDMAW)) &&
        !GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_DREADSCALE)))
    {
        return false;
    }

    constexpr float minSpreadDistance = 8.0f;
    return GetNearestPlayerInRadius(bot, minSpreadDistance) != nullptr;
}

bool WormsAfflictedByBurningTrigger::IsActive()
{
    // Tanks hold threat through the burn; only let non-tanks break off to keep moving
    if (botAI->IsTank(bot))
        return false;

    return bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_BURNING_BITE)) ||
           bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_BURNING_SPRAY));
}

// Icehowl

bool IcehowlEngagedByMainTankTrigger::IsActive()
{
    return botAI->IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
}

bool IcehowlChargeIncomingTrigger::IsActive()
{
    Unit* icehowl = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
    if (!icehowl)
        return false;

    // During the jump/charge sequence Icehowl drops his victim; the Massive Crash aura is the
    // early warning. Require him to be in combat so this never fires before the pull, and only
    // react if the bot is actually inside the charge corridor.
    bool const chargePhase = HasMassiveCrashAura(bot) || (icehowl->IsInCombat() && !icehowl->GetVictim());
    if (!chargePhase)
        return false;

    constexpr float corridorHalfWidth = 14.0f;
    return IsBotInChargeCorridor(bot, icehowl, corridorHalfWidth);
}
