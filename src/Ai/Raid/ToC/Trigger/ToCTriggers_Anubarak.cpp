#include "ToCTriggers_Anubarak.h"

#include "Creature.h"
#include "Map.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "RaidRedirectThreat.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"
#include "ToCData.h"
#include "ToCHelpers_Anubarak.h"
#include "ToCHelpers_Shared.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{

bool IsAnubarakSurfaced(AnubarakPhase phase)
{
    return phase == AnubarakPhase::Surface || phase == AnubarakPhase::Swarm;
}

}

bool AnubarakKiterNeedsImmunityTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && AnubarakKiterNeedsImmunity(bot);
}

bool AnubarakRaidGathersTrigger::IsActive() { return AnubarakEngaged(botAI) && AnubarakGathers(botAI); }

bool AnubarakHealerStandTrigger::IsActive()
{
    if (!AnubarakEngaged(botAI) || !PlayerbotAI::IsHeal(bot))
        return false;

    Position slot;
    if (!GetAnubarakHealerStand(bot, slot) || IsAnubarakDodgeHeld(botAI) || UnitOnPermafrost(bot))
    {
        RaidObs::NoteDerived(bot, "anub.hstand", "none");
        return false;
    }

    bool const off = bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) > ANUBARAK_HEALER_STAND_LEAVE;
    RaidObs::NoteDerived(bot, "anub.hstand", off ? "walk" : "hold");
    return off;
}

bool AnubarakDemonicCircleDueTrigger::IsActive() { return AnubarakDemonicCircleDue(botAI); }

// A held dodge spot inside a patch, when none was clear of one, stays: the lane is worse than the slow
bool AnubarakOnPermafrostTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && !IsAnubarakDodgeHeld(botAI) && AnubarakNonTankOnPermafrost(bot);
}

bool AnubarakMeleeBurrowerStandTrigger::IsActive() { return AnubarakMeleeNeedsBurrowerStand(botAI); }

bool AnubarakPursuedBySpikeTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && bot->HasAura(SPELL_MARK);
}

bool AnubarakKiterNeedsProtectionTrigger::IsActive()
{
    return bot->getClass() == CLASS_PALADIN && AnubarakEngaged(botAI) && GetAnubarakKiterToProtect(bot) != nullptr;
}

bool AnubarakKiterNeedsFreedomTrigger::IsActive()
{
    return bot->getClass() == CLASS_PALADIN && AnubarakEngaged(botAI) && GetAnubarakKiterToFree(bot) != nullptr;
}

bool AnubarakHopEscortTrigger::IsActive()
{
    Position spot;
    if (bot->getClass() != CLASS_PALADIN || !AnubarakEngaged(botAI))
        return false;

    if (!GetAnubarakEscortSpot(bot, spot))
    {
        RaidObs::NoteDerived(bot, "anub.escort", "none");
        return false;
    }

    bool const off = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) > ANUBARAK_ESCORT_HOLD;
    RaidObs::NoteDerived(bot, "anub.escort", off ? "walk" : "hold");
    return off;
}

bool AnubarakRedirectThreatTrigger::IsActive()
{
    AnubarakRedirect redirect;
    if (!AnubarakEngaged(botAI) || !GetAnubarakRedirect(bot, redirect) || !redirect.tank)
        return false;

    if (bot->getClass() == CLASS_ROGUE)
        return botAI->CanCastSpell("tricks of the trade", redirect.tank);

    return botAI->CanCastSpell("misdirection", redirect.tank) ||
           (redirect.dump && bot->HasAura(SPELL_MISDIRECTION_PROC));
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
    return AnubarakEngaged(botAI) && PlayerbotAI::IsRangedDps(bot) && GetAnubarakSphereToShoot(bot);
}

bool AnubarakDpsTargetTrigger::IsActive()
{
    Unit* target = GetAnubarakDpsTarget(botAI);
    return target && (AI_VALUE(Unit*, "current target") != target || ToCPetOffPick(botAI, target));
}

bool AnubarakBurrowerNeedsAssistTankTrigger::IsActive()
{
    if (!AnubarakEngaged(botAI))
        return false;

    if (IsAnubarakPickupTank(bot) && IsAnubarakSurfaced(GetAnubarakPhase(botAI)))
        return false;

    return static_cast<bool>(GetAnubarakBurrowerPick(botAI));
}

bool AnubarakSideTankReadyTrigger::IsActive()
{
    if (!AnubarakEngaged(botAI) || GetAnubarakBurrowerTankSide(bot) < 0 || bot->HasAura(SPELL_MARK))
        return false;

    if (IsAnubarakPickupTank(bot) && IsAnubarakSurfaced(GetAnubarakPhase(botAI)))
        return false;

    return !GetAnubarakBurrowerPick(botAI) && !AnubarakGathers(botAI) && !IsAnubarakEscort(botAI);
}

bool AnubarakSideTankNeedsDefensiveTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && AnubarakSideTankNeedsDefensive(bot) &&
           NextTankDefensive(botAI, bot, nullptr, true);
}

bool AnubarakSideTankDeadTrigger::IsActive()
{
    return AnubarakEngaged(botAI) && GetAnubarakSideTankToRes(botAI);
}

bool AnubarakGripFrostSphereTrigger::IsActive()
{
    AnubarakGripDuty duty;
    return AnubarakEngaged(botAI) && GetAnubarakGripDuty(bot, duty);
}

bool AnubarakScarabOnRaidTrigger::IsActive()
{
    if (!AnubarakEngaged(botAI) || !IsAnubarakTankPlayer(bot))
        return false;

    AnubarakPhase const phase = GetAnubarakPhase(botAI);
    if (IsAnubarakPickupTank(bot) && phase != AnubarakPhase::Submerged)
        return false;

    // Normal phase 2's scarabs are the main tank's and side tanks hold their patches. Heroic has a free
    // side tank peel them off the healers. Either way a side tank keeps its taunts for a wave that's due.
    bool const heroic = bot->GetMap()->IsHeroic();
    if (GetAnubarakBurrowerTankSide(bot) >= 0 &&
        ((!IsAnubarakSurfaced(phase) && !heroic) || AnubarakWaveImminent(botAI) || bot->HasAura(SPELL_MARK) ||
         IsAnubarakEscort(botAI)))
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
    return AnubarakEngaged(botAI) && PlayerbotAI::IsHeal(bot) && GetAnubarakPenetratingColdHealTarget(botAI);
}

void AddToCAnubarakTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("anubarak kiter needs immunity", {
        NextAction("anubarak kiter immunity", ACTION_EMERGENCY + 9) }));

    // Ahead of the kite, so a kiter marked in a patch frees itself before it walks out at 20% speed
    triggers.push_back(new TriggerNode("anubarak kiter needs freedom", {
        NextAction("anubarak free kiter", ACTION_EMERGENCY + 8.5f) }));

    triggers.push_back(new TriggerNode("anubarak pursued by spike", {
        NextAction("anubarak kite spike to permafrost", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode("anubarak kiter needs protection", {
        NextAction("anubarak protect kiter", ACTION_EMERGENCY + 7) }));

    triggers.push_back(new TriggerNode("anubarak spike nearby", {
        NextAction("anubarak avoid spike", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode("anubarak on permafrost", {
        NextAction("anubarak leave permafrost", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode("anubarak burrower casting shadow strike", {
        NextAction("anubarak interrupt shadow strike", ACTION_EMERGENCY + 3) }));

    triggers.push_back(new TriggerNode("anubarak side tank dead", {
        NextAction("anubarak battle res side tank", ACTION_RAID + 7) }));

    triggers.push_back(new TriggerNode("anubarak side tank needs defensive", {
        NextAction("anubarak side tank defensive", ACTION_RAID + 6.5f) }));

    triggers.push_back(new TriggerNode("anubarak leeching swarm on tank", {
        NextAction("anubarak tank defensive", ACTION_RAID + 6) }));

    triggers.push_back(new TriggerNode("anubarak grip frost sphere", {
        NextAction("anubarak grip frost sphere", ACTION_RAID + 5.5f) }));

    triggers.push_back(new TriggerNode("anubarak ranged should seed permafrost", {
        NextAction("anubarak destroy frost sphere", ACTION_RAID + 5) }));

    // Above every other walk of a DPS, side tank or healer bot that can be the escort
    triggers.push_back(new TriggerNode("anubarak hop escort", {
        NextAction("anubarak hop escort", ACTION_RAID + 4.5f) }));

    triggers.push_back(new TriggerNode("anubarak dps target", {
        NextAction("anubarak dps target", ACTION_RAID + 4) }));

    // Under the target pick, so a new focus is taken before the walk to the old one's stand
    triggers.push_back(new TriggerNode("anubarak melee burrower stand", {
        NextAction("anubarak melee burrower stand", ACTION_RAID + 3.8f) }));

    triggers.push_back(new TriggerNode("anubarak burrower needs assist tank", {
        NextAction("anubarak assist tank hold burrower", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("anubarak scarab on raid", {
        NextAction("anubarak tank pick up scarab", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("anubarak side tank ready", {
        NextAction("anubarak side tank ready", ACTION_RAID + 1.5f) }));

    triggers.push_back(new TriggerNode("anubarak raid gathers", {
        NextAction("anubarak gather", ACTION_RAID + 0.5f) }));

    triggers.push_back(new TriggerNode("anubarak healer stand", {
        NextAction("anubarak healer stand", ACTION_RAID + 0.45f) }));

    triggers.push_back(new TriggerNode("anubarak demonic circle due", {
        NextAction("anubarak demonic circle", ACTION_RAID + 0.4f) }));

    triggers.push_back(new TriggerNode("anubarak engaged by main tank", {
        NextAction("anubarak main tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("anubarak redirect threat", {
        NextAction("anubarak redirect threat", ACTION_RAID + 1.2f) }));

    triggers.push_back(new TriggerNode("anubarak penetrating cold on raid", {
        NextAction("anubarak heal penetrating cold", ACTION_MEDIUM_HEAL + 5) }));
}
