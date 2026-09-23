#include "ToCMultipliers_Shared.h"
#include "ToCEncounterGate.h"
#include "ToCMultipliers_Anubarak.h"
#include "ToCMultipliers_FactionChampions.h"
#include "ToCMultipliers_Jaraxxus.h"
#include "ToCMultipliers_NorthrendBeasts.h"
#include "ToCMultipliers_TwinValkyr.h"
#include "Action.h"
#include "BurstCooldowns.h"
#include "Playerbots.h"
#include "Timer.h"

#include <string>

namespace
{
ToCBurstWindow LiveEncounterBurstWindow(PlayerbotAI* botAI)
{
    switch (ToCLiveEncounter(botAI))
    {
        case ToCEncounter::NorthrendBeasts:
            return ToCNorthrendBeastsBurstWindow(botAI);
        case ToCEncounter::Jaraxxus:
            return ToCJaraxxusBurstWindow(botAI);
        case ToCEncounter::FactionChampions:
            return ToCFactionChampionsBurstWindow(botAI);
        case ToCEncounter::TwinValkyr:
            return ToCTwinValkyrBurstWindow(botAI);
        case ToCEncounter::Anubarak:
            return ToCAnubarakBurstWindow(botAI);
        case ToCEncounter::None:
            break;
    }

    return {};
}
}

float ToCBurstWindowMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    std::string const name = action->getName();
    if (!IsBurstCooldownAction(name) || IsManaReturnCooldown(bot, name))
        return 1.0f;

    if (!bot->IsInCombat())
        return 1.0f;

    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedValue = LiveEncounterBurstWindow(botAI);
    }

    bool const allowed = (name == "bloodlust" || name == "heroism") ? cachedValue.allowLust : cachedValue.allowAll;

    return allowed ? 1.0f : 0.0f;
}
