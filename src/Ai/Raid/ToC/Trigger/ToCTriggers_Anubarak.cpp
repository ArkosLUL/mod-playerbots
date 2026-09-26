#include "ToCTriggers_Anubarak.h"

#include <string>

#include "Creature.h"
#include "Playerbots.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"
#include "ToCData.h"
#include "ToCHelpers_Anubarak.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{

bool IsAnubarakSurfaced(AnubarakPhase phase)
{
    return phase == AnubarakPhase::Surface || phase == AnubarakPhase::Swarm;
}

}

bool AnubarakPursuedBySpikeTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && bot->HasAura(SPELL_MARK);
}

bool AnubarakSpikeNearbyTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && AnubarakInSpikeDanger(bot);
}

bool AnubarakBurrowerCastingShadowStrikeTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && GetAnubarakShadowStrikeDuty(bot);
}

bool AnubarakLeechingSwarmOnTankTrigger::IsActive()
{
    if (!AnubarakEngaged(botAI) || GetAnubarakPhase(botAI) != AnubarakPhase::Swarm)
        return false;

    Creature* boss = GetAnubarak(bot);
    return boss && boss->GetVictim() == bot && NextTankDefensive(botAI, bot, nullptr);
}

bool AnubarakRangedShouldSeedPermafrostTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && botAI->IsRangedDps(bot) && GetAnubarakSphereToShoot(bot);
}

bool AnubarakBurrowerShouldBeFocusedTrigger::IsActive()
{
    if (!AnubarakEngaged(botAI) || !botAI->IsDps(bot))
        return false;

    // Stays up on a leftover cross too, so the action can hand the rti back to the skull
    if (!GetAnubarakFocusBurrower(bot) && AI_VALUE(std::string, "rti") != ANUBARAK_BURROWER_RTI)
        return false;

    // A sphere shooter would swap between the sphere and the burrower every tick
    return !(botAI->IsRangedDps(bot) && GetAnubarakSphereToShoot(bot));
}

bool AnubarakBurrowerNeedsAssistTankTrigger::IsActive()
{
    if (!AnubarakEngaged(botAI))
        return false;

    if (IsAnubarakPickupTank(bot) && IsAnubarakSurfaced(GetAnubarakPhase(botAI)))
        return false;

    // Stays up on a leftover side mark too, so the action can hand the rti back to the skull
    return GetAnubarakBurrowerPick(botAI) || IsAnubarakSideRti(AI_VALUE(std::string, "rti"));
}

bool AnubarakScarabOnRaidTrigger::IsActive()
{
    if (!AnubarakEngaged(botAI) || !IsAnubarakTankPlayer(bot))
        return false;

    if (IsAnubarakPickupTank(bot) && GetAnubarakPhase(botAI) != AnubarakPhase::Submerged)
        return false;

    // Leaves burrower duty to the hold node, which outranks this one and would swap targets back
    if (GetAnubarakBurrowerPick(botAI))
        return false;

    return GetAnubarakScarabToPickUp(bot) != nullptr;
}

bool AnubarakEngagedByMainTankTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && IsAnubarakPickupTank(bot) && IsAnubarakSurfaced(GetAnubarakPhase(botAI));
}

bool AnubarakPenetratingColdOnRaidTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && botAI->IsHeal(bot) && GetAnubarakPenetratingColdHealTarget(botAI);
}

void AddToCAnubarakTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("anubarak pursued by spike", {
        NextAction("anubarak kite spike to permafrost", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode("anubarak spike nearby", {
        NextAction("anubarak avoid spike", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode("anubarak burrower casting shadow strike", {
        NextAction("anubarak interrupt shadow strike", ACTION_EMERGENCY + 3) }));

    triggers.push_back(new TriggerNode("anubarak leeching swarm on tank", {
        NextAction("anubarak tank defensive", ACTION_RAID + 6) }));

    triggers.push_back(new TriggerNode("anubarak ranged should seed permafrost", {
        NextAction("anubarak destroy frost sphere", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode("anubarak burrower should be focused", {
        NextAction("anubarak focus burrower", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode("anubarak burrower needs assist tank", {
        NextAction("anubarak assist tank hold burrower", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("anubarak scarab on raid", {
        NextAction("anubarak tank pick up scarab", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("anubarak engaged by main tank", {
        NextAction("anubarak main tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("anubarak penetrating cold on raid", {
        NextAction("anubarak heal penetrating cold", ACTION_MEDIUM_HEAL + 5) }));
}
