#include "ToCTriggers_Jormungars.h"
#include "ToCData.h"
#include "ToCHelpers_Shared.h"
#include "ToCHelpers_Jormungars.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

// Not IsMainTank: on heroic the main tank can be holding Gormok or Icehowl while the worms are up.
bool WormsMobileEngagedByMainTankTrigger::IsActive()
{
    return GetBeastsTankDuty(botAI) == BeastsTankDuty::WormMobile;
}

bool WormsStationaryNeedsAssistTankTrigger::IsActive()
{
    return GetBeastsTankDuty(botAI) == BeastsTankDuty::WormStationary;
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

    return bot->HasAura(SPELL_BURNING_BITE) || bot->HasAura(SPELL_BURNING_SPRAY);
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
    return EncounterHelpers::IsBotInFrontalCone(bot, worm, sweepArc, sweepRange);
}

void AddToCJormungarsTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("northrend worms mobile engaged by main tank", {
        NextAction("northrend worms main tank hold mobile worm", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("northrend worms stationary needs assist tank", {
        NextAction("northrend worms assist tank hold stationary worm", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("northrend worms ranged should spread", {
        NextAction("northrend worms spread", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode("northrend worms afflicted by burning", {
        NextAction("northrend worms keep moving", ACTION_EMERGENCY + 5) }));

    // Persistent ground hazards / frontal cone dodges outrank the generic spread/keep-moving above
    triggers.push_back(new TriggerNode("northrend worms slime pool nearby", {
        NextAction("northrend worms avoid slime pool", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode("northrend worms sweep frontal", {
        NextAction("northrend worms avoid sweep", ACTION_EMERGENCY + 7) }));
}
