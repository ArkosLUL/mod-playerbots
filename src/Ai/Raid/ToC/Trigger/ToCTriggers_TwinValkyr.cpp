#include "ToCTriggers_TwinValkyr.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_TwinValkyr.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
bool WantsEssenceFor(PlayerbotAI* botAI, Player* bot, TwinEssenceReason reason)
{
    TwinEssenceWant const want = GetWantedEssence(botAI);
    return want.reason == reason && want.colour != EssenceOf(bot);
}

// Both twins stay non-attackable until the script releases them at the pull.
bool IsAttackableTwin(Unit* twin)
{
    return twin && !twin->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
}
}  // namespace

bool TwinValkyrPactInterruptDutyTrigger::IsActive()
{
    Unit* twin = GetTwinCastingPact(botAI);
    return twin && IsTwinPactInterrupter(botAI, twin);
}

bool TwinValkyrTouchedRequiresEssenceTrigger::IsActive()
{
    return WantsEssenceFor(botAI, bot, TwinEssenceReason::Touch);
}

bool TwinValkyrVortexRequiresEssenceTrigger::IsActive()
{
    return WantsEssenceFor(botAI, bot, TwinEssenceReason::Vortex);
}

bool TwinValkyrOrbIncomingTrigger::IsActive()
{
    return !TwinValkyrMustSwapEssence(botAI, true) && TwinOrbThreatens(botAI, TWIN_ORB_DODGE_CLEARANCE);
}

bool TwinValkyrShieldRequiresEssenceTrigger::IsActive()
{
    return WantsEssenceFor(botAI, bot, TwinEssenceReason::Shield);
}

bool TwinValkyrNeedsBaseEssenceTrigger::IsActive()
{
    return WantsEssenceFor(botAI, bot, TwinEssenceReason::Base);
}

bool TwinValkyrEngagedByMainTankTrigger::IsActive()
{
    Unit* fjola = GetFjola(botAI);
    return IsAttackableTwin(fjola) && IsTwinTank(bot, fjola);
}

bool TwinValkyrDarkbaneNeedsAssistTankTrigger::IsActive()
{
    Unit* eydis = GetEydis(botAI);
    if (!IsAttackableTwin(eydis) || !IsTwinTank(bot, eydis))
        return false;

    // a lone tank holds both twins from the main tank node
    Unit* fjola = GetFjola(botAI);
    return !fjola || !IsTwinTank(bot, fjola);
}

bool TwinValkyrRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::TwinValkyr))
        return false;

    Unit* twin = GetTwinDpsTarget(botAI);
    if (!twin)
        return false;

    Player* tank = GetTwinTank(bot, twin);
    return tank && tank != bot && tank->IsAlive();
}

bool TwinValkyrDpsTargetTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsTank(bot, true) || PlayerbotAI::IsHeal(bot))
        return false;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::TwinValkyr))
        return false;

    Unit* twin = GetTwinDpsTarget(botAI);
    return twin && (AI_VALUE(Unit*, "rti target") != twin || AI_VALUE(Unit*, "current target") != twin);
}

void AddToCTwinValkyrTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("twin valkyr pact interrupt duty", {
        NextAction("twin valkyr interrupt pact", ACTION_EMERGENCY + 7) }));

    // Touch over Vortex: in this core a Touch ticks on the whole raid until the touched bot swaps
    triggers.push_back(new TriggerNode("twin valkyr touched requires essence", {
        NextAction("twin valkyr swap essence for touch", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode("twin valkyr vortex requires essence", {
        NextAction("twin valkyr swap essence for vortex", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode("twin valkyr orb incoming", {
        NextAction("twin valkyr dodge orb", ACTION_EMERGENCY + 4) }));

    triggers.push_back(new TriggerNode("twin valkyr shield requires essence", {
        NextAction("twin valkyr swap essence for shield", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode("twin valkyr needs base essence", {
        NextAction("twin valkyr take base essence", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode("twin valkyr engaged by main tank", {
        NextAction("twin valkyr main tank hold light twin", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("twin valkyr darkbane needs assist tank", {
        NextAction("twin valkyr assist tank hold dark twin", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("twin valkyr redirect threat", {
        NextAction("twin valkyr redirect threat", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("twin valkyr dps target", {
        NextAction("twin valkyr focus twin", ACTION_RAID) }));
}
