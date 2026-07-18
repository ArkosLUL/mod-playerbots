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

bool GormokTankSwapNeededTrigger::IsActive()
{
    // Either tank (main or first assist) taunts when the OTHER tank is the one currently holding Gormok
    // and is carrying a lethal Impale stack count. With two tanks this ping-pongs the boss between them.
    if (!botAI->IsMainTank(bot) && !botAI->IsAssistTankOfIndex(bot, 0, false))
        return false;

    Unit* gormok = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK));
    if (!gormok)
        return false;

    Unit* victim = gormok->GetVictim();
    if (!victim || victim == bot)
        return false;

    return GetGormokImpaleStacks(victim) >= GORMOK_IMPALE_SWAP_STACKS;
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

bool WormsSlimePoolNearbyTrigger::IsActive()
{
    // The slime pool is a persistent ground hazard everyone (tanks included) steps out of, like the
    // Jaraxxus Legion Flame trail.
    constexpr float slimePoolRadius = 6.0f;
    return GetNearestCreatureByEntry(bot, static_cast<uint32>(ToCNpcs::NPC_SLIME_POOL), slimePoolRadius) != nullptr;
}

bool WormsSweepFrontalTrigger::IsActive()
{
    // Sweep is a frontal cone; only bots standing in front of the casting worm need to dodge. Tanks hold
    // the worm head-on and eat it by design, so only non-tanks break off.
    if (botAI->IsTank(bot))
        return false;

    Unit* worm = GetWormCastingSweep(botAI);
    if (!worm)
        return false;

    constexpr float sweepArc = static_cast<float>(M_PI) / 2.0f; // ~90-degree frontal cone
    constexpr float sweepRange = 20.0f;
    return TrialOfTheCrusaderHelpers::IsBotInFrontalCone(bot, worm, sweepArc, sweepRange);
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

// Faction Champions

bool FactionChampionsShouldFocusTrigger::IsActive()
{
    // Healers keep healing the raid; every other role burns the focus target. There is no boss to
    // tank here (threat is artificial), so tanks join the damage dealers on the kill target.
    if (botAI->IsHeal(bot))
        return false;

    return GetPriorityFactionChampion(botAI) != nullptr;
}

// Twin Val'kyr

bool TwinValkyrEngagedByMainTankTrigger::IsActive()
{
    return botAI->IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE));
}

bool TwinValkyrDarkbaneNeedsAssistTankTrigger::IsActive()
{
    return botAI->IsAssistTankOfIndex(bot, 0, false) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE));
}

bool TwinValkyrVortexRequiresEssenceTrigger::IsActive()
{
    // Tanks stay anchored on their twin; only non-tanks run to a portal to swap. A bot that already
    // matches the active vortex colour is fine, so only fire on a genuine mismatch.
    if (botAI->IsTank(bot))
        return false;

    if (TwinValkyrLightVortexActive(botAI) && !HasLightEssence(bot))
        return true;

    return TwinValkyrDarkVortexActive(botAI) && !HasDarkEssence(bot);
}

bool TwinValkyrTouchedRequiresEssenceTrigger::IsActive()
{
    // Touch (heroic) only lands on essence-carrying non-tanks (the boss excludes current tanks). The
    // remedy is to switch to the touch's colour: Light Touch absorbed by Light Essence, and vice versa.
    if (botAI->IsTank(bot))
        return false;

    if (bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_LIGHT_TOUCH)) && !HasLightEssence(bot))
        return true;

    return bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_DARK_TOUCH)) && !HasDarkEssence(bot);
}

bool TwinValkyrNeedsInitialEssenceTrigger::IsActive()
{
    // Everyone (tanks included) grabs an essence at the pull so they have an absorb for the first
    // vortex/ball/touch. Tanks keep this fixed colour for the whole fight (they are excluded from the
    // vortex/touch swap triggers); non-tanks swap from here as those mechanics fire. Low priority.
    return TwinValkyrEncounterActive(botAI) && !HasAnyEssence(bot);
}

bool TwinValkyrPactInterruptibleTrigger::IsActive()
{
    // Pure healers keep the raid up; tanks stay anchored on their twin (the tank on the casting twin
    // already interrupts via its always-on class behaviour). Free DPS retarget the casting twin so their
    // interrupt breaks the heal-to-full channel.
    if (botAI->IsTank(bot) || botAI->IsHeal(bot))
        return false;

    return GetTwinCastingPact(botAI) != nullptr;
}
