#include "ToCHelpers_ResistAura.h"

#include "ToCEncounterGate.h"
#include "ToCHelpers_Anubarak.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "ToCHelpers_Staging.h"

namespace TrialOfTheCrusaderHelpers
{
namespace
{
uint8 SchoolsOfArrival(ToCStagingBoss arrival)
{
    switch (arrival)
    {
        case ToCStagingBoss::Gormok:
            return RAID_RESIST_FIRE;
        case ToCStagingBoss::Worms:
            return RAID_RESIST_FIRE | RAID_RESIST_NATURE;
        case ToCStagingBoss::Icehowl:
            return RAID_RESIST_FROST;
        case ToCStagingBoss::Jaraxxus:
            return RAID_RESIST_FIRE | RAID_RESIST_NATURE;
        case ToCStagingBoss::Twins:
            return RAID_RESIST_FIRE | RAID_RESIST_SHADOW;
        case ToCStagingBoss::Anubarak:
            return RAID_RESIST_FROST;
        case ToCStagingBoss::Champions:
            return RAID_RESIST_FIRE;
        default:
            return 0;
    }
}

uint8 SchoolsOfBeastsStage(uint32 mask)
{
    uint8 schools = 0;
    if (mask & BEASTS_STAGE_GORMOK)
        schools |= SchoolsOfArrival(ToCStagingBoss::Gormok);
    if (mask & BEASTS_STAGE_WORMS)
        schools |= SchoolsOfArrival(ToCStagingBoss::Worms);
    if (mask & BEASTS_STAGE_ICEHOWL)
        schools |= SchoolsOfArrival(ToCStagingBoss::Icehowl);

    return schools;
}
}

bool ToCResistFireWantsMastery(PlayerbotAI* botAI)
{
    ToCEncounter const live = ToCLiveEncounter(botAI);
    if (live != ToCEncounter::None)
        return live == ToCEncounter::Jaraxxus || live == ToCEncounter::TwinValkyr;

    ToCStagingBoss const staged = GetToCStagedArrival(botAI);
    return staged == ToCStagingBoss::Jaraxxus || staged == ToCStagingBoss::Twins;
}

bool ToCResistFrostWantsMastery(PlayerbotAI* botAI)
{
    return ToCLiveEncounter(botAI) == ToCEncounter::Anubarak && AnubarakSwarmHealing(botAI);
}

uint8 ToCResistWantedSchools(PlayerbotAI* botAI, uint8 previous)
{
    switch (ToCLiveEncounter(botAI))
    {
        case ToCEncounter::NorthrendBeasts:
        {
            uint8 const schools = SchoolsOfBeastsStage(GetBeastsStageMask(botAI));
            return schools ? schools : previous;
        }
        case ToCEncounter::Jaraxxus:
            return SchoolsOfArrival(ToCStagingBoss::Jaraxxus);
        case ToCEncounter::FactionChampions:
            return SchoolsOfArrival(ToCStagingBoss::Champions);
        case ToCEncounter::TwinValkyr:
            return SchoolsOfArrival(ToCStagingBoss::Twins);
        case ToCEncounter::Anubarak:
        {
            AnubarakPhase const phase = GetAnubarakPhase(botAI);
            bool const nature = phase == AnubarakPhase::Submerged || phase == AnubarakPhase::Swarm;
            return static_cast<uint8>(SchoolsOfArrival(ToCStagingBoss::Anubarak) | (nature ? RAID_RESIST_NATURE : 0));
        }
        default:
            return SchoolsOfArrival(GetToCStagedArrival(botAI));
    }
}

bool IsToCResistPaladinHolding(PlayerbotAI* botAI) { return IsRaidResistPaladinHolding(botAI, TOC_RESIST_AURA); }

bool IsToCResistHunterHolding(PlayerbotAI* botAI) { return IsRaidResistHunterHolding(botAI, TOC_RESIST_AURA); }

}
