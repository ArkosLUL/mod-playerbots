#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "RaidCombatHelpers.h"
#include "RaidDefensive.h"
#include "Strategy.h"
#include "ToCActions_TwinValkyr.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_TwinValkyr.h"
#include "ToCMultipliers_TwinValkyr.h"
#include "ToCTriggers_TwinValkyr.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool TwinValkyrDpsTargetOwned(PlayerbotAI* botAI)
{
    return IsRaidDpsBot(botAI->GetBot()) && ToCEncounterIsLive(botAI, ToCEncounter::TwinValkyr) &&
           GetTwinDpsTarget(botAI);
}

bool TwinValkyrTankTargetOwned(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Unit* fjola = GetFjola(botAI);
    if (IsTwinAttackable(fjola) && IsTwinTank(bot, fjola))
        return true;

    Unit* eydis = GetEydis(botAI);
    return IsTwinAttackable(eydis) && IsTwinTank(bot, eydis);
}

void DefineTwinValkyr(EncounterBuilder& e)
{
    e.Node<TwinValkyrPactInterruptDutyTrigger, TwinValkyrInterruptPactAction>(ACTION_EMERGENCY + 8);
    e.Node<TwinValkyrVortexRequiresEssenceTrigger, TwinValkyrSwapEssenceForVortexAction>(ACTION_EMERGENCY + 7);

    e.Node<TwinValkyrTouchedRequiresEssenceTrigger, TwinValkyrSwapEssenceForTouchAction>(ACTION_EMERGENCY + 6);
    e.Node<TwinValkyrVortexRaidCoverTrigger, TwinValkyrCastVortexRaidCoverAction>(ACTION_EMERGENCY + 5);
    e.Node<TwinValkyrVortexDefensiveTrigger, TwinValkyrCastVortexDefensiveAction>(ACTION_EMERGENCY + 4);
    e.Node<TwinValkyrOrbIncomingTrigger, TwinValkyrDodgeOrbAction>(ACTION_EMERGENCY + 3);
    e.Node<TwinValkyrNextPortalDueTrigger, TwinValkyrWalkToNextPortalAction>(ACTION_EMERGENCY + 2);
    e.Node<TwinValkyrNeedsBaseEssenceTrigger, TwinValkyrTakeBaseEssenceAction>(ACTION_RAID + 5);
    e.Node<TwinValkyrEngagedByMainTankTrigger, TwinValkyrMainTankHoldLightTwinAction>(ACTION_RAID + 4);
    e.Node<TwinValkyrDarkbaneNeedsAssistTankTrigger, TwinValkyrAssistTankHoldDarkTwinAction>(ACTION_RAID + 3);
    e.Node<TwinValkyrRedirectThreatTrigger, TwinValkyrRedirectThreatAction>(ACTION_RAID + 2);
    e.Node<TwinValkyrDoorStandTrigger, TwinValkyrMoveToDoorStandAction>(ACTION_RAID + 1);
    e.Node<TwinValkyrDpsTargetTrigger, TwinValkyrFocusTwinAction>(ACTION_RAID);

    e.LiveMultiplier<TwinValkyrControlTankMovementMultiplier>(Family::AnyMovement);
    e.LiveMultiplier<TwinValkyrTauntGuardMultiplier>(Family::Spell);
    e.LiveMultiplier<TwinValkyrInterruptHoldMultiplier>(Family::AnyAction);
    e.LiveMultiplier<TwinValkyrRedirectGuardMultiplier>(Family::AnyMovement | Family::Spell);

    e.OwnTargeting("twin valkyr dps target guard", Role::Any, TwinValkyrDpsTargetOwned, Family::DpsAssist);
    e.OwnTargeting("twin valkyr tank target guard", Role::Any, TwinValkyrTankTargetOwned, Family::TankAssist);

    e.Block("twin valkyr door hold", Role::Any, TwinValkyrDoorHold,
            Family::Reach | Family::ReachHeal | Family::SetBehind | Family::RearFlank | Family::CombatFormationMove |
                Family::Follow | Family::Flee | Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink |
                Family::Disengage);

    e.Block("twin valkyr raid cover hold", Role::Any, TwinValkyrRaidCoverHeld, 0, RaidCoverClassNodes());

    e.Block("twin valkyr power infusion hold", Role::Heal, TwinValkyrBurstHeld, 0,
            {"power infusion", "power infusion on party"});
}
}

EncounterDefinition const& ToCTwinValkyrDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::TwinValkyr), ToCGate,
                                                "val-kyr-twins", &DefineTwinValkyr);
    return definition;
}
