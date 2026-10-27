#include "ToCTriggers_NorthrendBeasts.h"

#include "Playerbots.h"
#include "RaidDirectTargeting.h"
#include "ToCData.h"
#include "ToCHelpers_NorthrendBeasts.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
bool BelongsToStem(Unit* unit, NorthrendBeastsStem stem)
{
    ToCNpcs const entry = static_cast<ToCNpcs>(unit->GetEntry());
    switch (stem)
    {
        case NorthrendBeastsStem::Gormok:
            return entry == ToCNpcs::NPC_GORMOK || entry == ToCNpcs::NPC_SNOBOLD_VASSAL;
        case NorthrendBeastsStem::Worms:
            return entry == ToCNpcs::NPC_ACIDMAW || entry == ToCNpcs::NPC_DREADSCALE;
        case NorthrendBeastsStem::Icehowl:
            return entry == ToCNpcs::NPC_ICEHOWL;
    }

    return false;
}
}  // namespace

bool NorthrendBeastsDpsTargetTrigger::IsActive()
{
    Unit* pick = GetBeastsDpsTarget(botAI).unit;
    return pick && BelongsToStem(pick, stem) &&
           (AI_VALUE(Unit*, "current target") != pick || RaidPetOffPick(botAI, pick));
}
