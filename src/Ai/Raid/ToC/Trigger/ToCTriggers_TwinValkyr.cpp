#include "ToCTriggers_TwinValkyr.h"
#include "ToCData.h"
#include "ToCHelpers_TwinValkyr.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

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

    if (HasLightTouch(bot) && !HasLightEssence(bot))
        return true;

    return HasDarkTouch(bot) && !HasDarkEssence(bot);
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

void AddToCTwinValkyrTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    // Twin Val'kyr. The twins share health, so the main tank skull-marks Fjola and non-tank DPS focus
    // her via the default "dps assist" (killing one kills both). The colour-matching Essence system is
    // the survival core: bots grab an essence at the pull and swap to match the active Vortex / heroic
    // Touch. Powering Up (orb collection) is intentionally out of scope; the enrage is met via the
    // shared-health focus-fire, not the DPS buff.
    triggers.push_back(new TriggerNode("twin valkyr engaged by main tank", {
        NextAction("twin valkyr main tank hold light twin", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("twin valkyr darkbane needs assist tank", {
        NextAction("twin valkyr assist tank hold dark twin", ACTION_RAID + 2) }));

    // Touch outranks Vortex: a touched bot is taking a personal heavy DoT that only the colour swap stops
    triggers.push_back(new TriggerNode("twin valkyr touched requires essence", {
        NextAction("twin valkyr swap essence for touch", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode("twin valkyr vortex requires essence", {
        NextAction("twin valkyr swap essence for vortex", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode("twin valkyr needs initial essence", {
        NextAction("twin valkyr acquire initial essence", ACTION_RAID) }));

    // Break the twins' heal-to-full Twin's Pact channel. Below the essence swaps (+5/+6) so a bot that
    // must swap essence to survive still swaps first.
    triggers.push_back(new TriggerNode("twin valkyr pact interruptible", {
        NextAction("twin valkyr interrupt pact", ACTION_EMERGENCY + 2) }));
}
