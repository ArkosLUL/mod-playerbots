#include "ToCTriggers.h"
#include "ToCHelpers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Creature.h"

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

// Lord Jaraxxus

bool JaraxxusEngagedByMainTankTrigger::IsActive()
{
    return botAI->IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS));
}

bool JaraxxusAddNeedsAssistTankTrigger::IsActive()
{
    return botAI->IsAssistTankOfIndex(bot, 0, false) && GetPriorityJaraxxusAdd(botAI);
}

bool JaraxxusSecondAddNeedsAssistTankTrigger::IsActive()
{
    // A second assist tank picks up the overlapping add so it never free-roams onto the raid
    return botAI->IsAssistTankOfIndex(bot, 1, false) && GetSecondaryJaraxxusAdd(botAI);
}

bool JaraxxusAddShouldBeFocusedTrigger::IsActive()
{
    // Damage dealers burn the adds down; tanks and healers keep their assignments
    if (botAI->IsTank(bot) || botAI->IsHeal(bot))
        return false;

    return GetPriorityJaraxxusAdd(botAI) != nullptr;
}

bool JaraxxusLegionFlameNearbyTrigger::IsActive()
{
    // The pursuing fire is lethal to anyone standing in it, tanks included; everyone steps out
    constexpr float legionFlameRadius = 6.0f;
    return GetNearestCreatureByEntry(bot, static_cast<uint32>(ToCNpcs::NPC_LEGION_FLAME), legionFlameRadius) != nullptr;
}

bool JaraxxusIncinerateFleshOnRaidTrigger::IsActive()
{
    if (!botAI->IsHeal(bot))
        return false;

    GuidVector const& members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& guid : members)
    {
        Unit* member = botAI->GetUnit(guid);
        if (member && member->IsAlive() &&
            member->HasAura(static_cast<uint32>(ToCSpells::SPELL_INCINERATE_FLESH)))
        {
            return true;
        }
    }

    return false;
}

bool JaraxxusNetherPowerActiveTrigger::IsActive()
{
    // Only classes with an offensive magic dispel can strip the buff
    switch (bot->getClass())
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
            break;
        default:
            return false;
    }

    return JaraxxusHasNetherPower(GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS)));
}

bool JaraxxusFelFireballInterruptibleTrigger::IsActive()
{
    // Reinforce the interrupt only when there are no adds to fight, so add damage is not lost.
    // While adds are up the boss tank's own class interrupt handles Fel Fireball.
    if (botAI->IsTank(bot) || botAI->IsHeal(bot) || GetPriorityJaraxxusAdd(botAI))
        return false;

    Unit* jaraxxus = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS));
    return jaraxxus && jaraxxus->FindCurrentSpellBySpellId(static_cast<uint32>(ToCSpells::SPELL_FEL_FIREBALL));
}

// Anub'arak

bool AnubarakEngagedByMainTankTrigger::IsActive()
{
    // The boss is unselectable while submerged, so it drops out of "possible targets" and this
    // naturally goes idle during phase 2; it picks back up on the surface (phase 1 and phase 3).
    return botAI->IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ANUBARAK));
}

bool AnubarakBurrowerNeedsAssistTankTrigger::IsActive()
{
    return botAI->IsAssistTankOfIndex(bot, 0, false) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER));
}

bool AnubarakBurrowerShouldBeFocusedTrigger::IsActive()
{
    // Damage dealers burn the burrowers; tanks and healers keep their assignments
    if (botAI->IsTank(bot) || botAI->IsHeal(bot))
        return false;

    return GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER)) != nullptr;
}

bool AnubarakScarabOnRaidTrigger::IsActive()
{
    // Melee peel onto the scarabs during submerge; ranged seed Permafrost instead (see below)
    if (botAI->IsTank(bot) || botAI->IsHeal(bot) || !botAI->IsMelee(bot))
        return false;

    return GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_SWARM_SCARAB)) != nullptr;
}

bool AnubarakPursuedBySpikeTrigger::IsActive()
{
    return bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MARK));
}

bool AnubarakRangedShouldSeedPermafrostTrigger::IsActive()
{
    // Ranged destroy flying Frost Spheres while the boss is burrowed, dropping Permafrost patches
    // the spike-chase target can be kited through. The marked player never breaks off to do this.
    if (!botAI->IsRanged(bot) || botAI->IsHeal(bot))
        return false;

    if (bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MARK)) || !AnubarakSubmerged(botAI))
        return false;

    // A flying sphere is an alive Frost Sphere that has not yet grounded into a Permafrost patch
    std::list<Creature*> spheres;
    bot->GetCreatureListWithEntryInGrid(spheres, static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE), 100.0f);
    for (Creature* sphere : spheres)
    {
        if (sphere->IsAlive() && !sphere->HasAura(static_cast<uint32>(ToCSpells::SPELL_PERMAFROST)))
            return true;
    }

    return false;
}
