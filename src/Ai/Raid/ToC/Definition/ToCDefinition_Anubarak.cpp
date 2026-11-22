#include "ToCDefinitions.h"

#include "AiObjectContext.h"
#include "Creature.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Strategy.h"
#include "ToCActions_Anubarak.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Anubarak.h"
#include "ToCMultipliers_Anubarak.h"
#include "ToCTriggers_Anubarak.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool AnubarakDpsTargetOwned(PlayerbotAI* botAI) { return GetAnubarakDpsTarget(botAI) != nullptr; }

bool AnubarakPickupTankHolds(PlayerbotAI* botAI)
{
    if (!AnubarakEngaged(botAI) || !IsAnubarakPickupTank(botAI->GetBot()))
        return false;

    AnubarakPhase const phase = GetAnubarakPhase(botAI);
    return phase == AnubarakPhase::Surface || phase == AnubarakPhase::Swarm;
}

bool AnubarakSideTankHolds(PlayerbotAI* botAI)
{
    return AnubarakEngaged(botAI) && GetAnubarakBurrowerTankSide(botAI->GetBot()) >= 0;
}

bool AnubarakSphereShooterHolds(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!AnubarakEngaged(botAI))
        return false;

    AnubarakGripDuty grip;
    return (PlayerbotAI::IsRangedDps(bot) && GetAnubarakSphereToShoot(bot)) ||
           (GetAnubarakGripDuty(bot, grip) && grip.step != AnubarakGripStep::Hold);
}

bool AnubarakShooterOnDutySphere(PlayerbotAI* botAI)
{
    if (!AnubarakEngaged(botAI))
        return false;

    Creature* sphere = GetAnubarakDutySphere(botAI->GetBot());
    return sphere && IsFrostSphereFlying(sphere) &&
           botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() == sphere;
}

bool AnubarakGripHolds(PlayerbotAI* botAI)
{
    AnubarakGripDuty grip;
    return AnubarakEngaged(botAI) && GetAnubarakGripDuty(botAI->GetBot(), grip) &&
           grip.step != AnubarakGripStep::Kill;
}

bool AnubarakPickupTankStands(PlayerbotAI* botAI)
{
    return AnubarakPickupTankHolds(botAI) && AnubarakTankPatchLatched(botAI->GetBot());
}

bool AnubarakGripOwed(PlayerbotAI* botAI)
{
    AnubarakGripDuty grip;
    return AnubarakEngaged(botAI) && GetAnubarakGripDuty(botAI->GetBot(), grip);
}

bool AnubarakMainTankLeavesScarabs(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return AnubarakEngaged(botAI) && bot->GetMap()->IsHeroic() && IsAnubarakPickupTank(bot) &&
           GetAnubarakPhase(botAI) == AnubarakPhase::Submerged;
}

bool AnubarakMainTankEngaged(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return AnubarakEngaged(botAI) && IsAnubarakTankPlayer(bot) && GetAnubarakBurrowerTankSide(bot) < 0;
}

bool AnubarakKiteImmunitiesKept(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return AnubarakEngaged(botAI) && bot->GetMap()->IsHeroic() && !bot->HasAura(SPELL_MARK) &&
           GetAnubarakPhase(botAI) == AnubarakPhase::Submerged;
}

bool AnubarakKiteSprintKept(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return AnubarakEngaged(botAI) && bot->GetMap()->IsHeroic() && !bot->HasAura(SPELL_MARK) &&
           GetAnubarakPhase(botAI) != AnubarakPhase::Swarm;
}

void DefineAnubarak(EncounterBuilder& e)
{
    e.Node<AnubarakKiterNeedsImmunityTrigger, AnubarakKiterImmunityAction>(ACTION_EMERGENCY + 9);

    e.Node<AnubarakKiterNeedsFreedomTrigger, AnubarakFreeKiterAction>(ACTION_EMERGENCY + 8.5f);
    e.Node<AnubarakPursuedBySpikeTrigger, AnubarakKiteSpikeToPermafrostAction>(ACTION_EMERGENCY + 8);
    e.Node<AnubarakKiterNeedsProtectionTrigger, AnubarakProtectKiterAction>(ACTION_EMERGENCY + 7);
    e.Node<AnubarakSpikeNearbyTrigger, AnubarakAvoidSpikeAction>(ACTION_EMERGENCY + 6);
    e.Node<AnubarakOnPermafrostTrigger, AnubarakLeavePermafrostAction>(ACTION_EMERGENCY + 5);

    e.Node<AnubarakTankNeedsRescueTrigger, AnubarakTankRescueAction>(ACTION_EMERGENCY + 4);
    e.Node<AnubarakBurrowerCastingShadowStrikeTrigger, AnubarakInterruptShadowStrikeAction>(ACTION_EMERGENCY + 3);

    e.Node<AnubarakColdSelfSaveTrigger, AnubarakColdSelfSaveAction>(ACTION_EMERGENCY + 2);

    e.Node<AnubarakLustDueTrigger, AnubarakLustAction>(ACTION_EMERGENCY + 1);

    e.Node<AnubarakKiteCatDropTrigger, AnubarakKiteCatDropAction>(ACTION_RAID + 7.5f);
    e.Node<AnubarakSideTankDeadTrigger, AnubarakBattleResSideTankAction>(ACTION_RAID + 7);
    e.Node(
        AnubarakSideTankNeedsDefensiveTrigger::Name,
        [](PlayerbotAI* botAI) -> Trigger* { return new AnubarakSideTankNeedsDefensiveTrigger(botAI); },
        "anubarak side tank defensive",
        [](PlayerbotAI* botAI) -> Action*
        { return new AnubarakTankDefensiveAction(botAI, "anubarak side tank defensive", true); },
        ACTION_RAID + 6.5f);
    e.Node<AnubarakLeechingSwarmOnTankTrigger, AnubarakTankDefensiveAction>(ACTION_RAID + 6);
    e.Node<AnubarakGripFrostSphereTrigger, AnubarakGripFrostSphereAction>(ACTION_RAID + 5.5f);
    e.Node<AnubarakRangedShouldSeedPermafrostTrigger, AnubarakDestroyFrostSphereAction>(ACTION_RAID + 5);

    e.Node<AnubarakHopEscortTrigger, AnubarakHopEscortAction>(ACTION_RAID + 4.5f);

    e.Node<AnubarakBurrowerLeadTrigger, AnubarakBurrowerLeadAction>(ACTION_RAID + 4.4f);

    e.Node<AnubarakScarabKiteTrigger, AnubarakKiteScarabAction>(ACTION_RAID + 4.3f);

    e.Node<AnubarakCleaveBuffNearBurrowerTrigger, AnubarakDropCleaveBuffAction>(ACTION_RAID + 4.2f);
    e.Node<AnubarakDpsTargetTrigger, AnubarakDpsTargetAction>(ACTION_RAID + 4);

    e.Node<AnubarakMeleeBurrowerStandTrigger, AnubarakMeleeBurrowerStandAction>(ACTION_RAID + 3.8f);
    e.Node<AnubarakBurrowerNeedsAssistTankTrigger, AnubarakAssistTankHoldBurrowerAction>(ACTION_RAID + 3);
    e.Node<AnubarakScarabOnRaidTrigger, AnubarakTankPickUpScarabAction>(ACTION_RAID + 2);

    e.Node<AnubarakSideTankAreaThreatTrigger, AnubarakSideTankAreaThreatAction>(ACTION_RAID + 2.5f);
    e.Node<AnubarakSideTankReadyTrigger, AnubarakSideTankReadyAction>(ACTION_RAID + 1.5f);
    e.Node<AnubarakRaidGathersTrigger, AnubarakGatherAction>(ACTION_RAID + 0.5f);
    e.Node<AnubarakHealerStandTrigger, AnubarakHealerStandAction>(ACTION_RAID + 0.45f);
    e.Node<AnubarakRangedStandTrigger, AnubarakRangedStandAction>(ACTION_RAID + 0.45f);
    e.Node<AnubarakDemonicCircleDueTrigger, AnubarakDemonicCircleAction>(ACTION_RAID + 0.4f);

    e.Node<AnubarakHealingStreamDueTrigger>("healing stream totem", ACTION_RAID - 0.5f);
    e.Node<AnubarakManaTopUpTrigger, AnubarakTopUpManaAction>(ACTION_RAID - 1);
    e.Node<AnubarakLightJudgeDueTrigger, AnubarakJudgeLightAction>(ACTION_HIGH + 5);
    e.Node<AnubarakEngagedByMainTankTrigger, AnubarakMainTankHoldBossAction>(ACTION_RAID + 1);
    e.Node<AnubarakRedirectThreatTrigger, AnubarakRedirectThreatAction>(ACTION_RAID + 1.2f);
    e.Node<AnubarakPenetratingColdOnRaidTrigger, AnubarakHealPenetratingColdAction>(ACTION_MEDIUM_HEAL + 5);

    e.Node<AnubarakAuraMasteryDueTrigger, AnubarakAuraMasteryAction>(ACTION_CRITICAL_HEAL + 13);

    e.Node<AnubarakSwarmHealUrgentTrigger, AnubarakSwarmHealAction>(ACTION_CRITICAL_HEAL + 12);
    e.Node<AnubarakSwarmHealTrigger, AnubarakSwarmHealAction>(ACTION_MEDIUM_HEAL + 0.75f);

    e.Node<AnubarakDivinePleaTrigger>("divine plea", ACTION_MEDIUM_HEAL + 6);
    e.Node<AnubarakInnervateHealerTrigger, AnubarakInnervateHealerAction>(ACTION_MEDIUM_HEAL + 6);

    e.LiveMultiplier<AnubarakControlTankMovementMultiplier>(Family::AnyMovement);
    e.LiveMultiplier<AnubarakProtectSpikeKiteMultiplier>(Family::AnyMovement | Family::Spell);

    e.OwnTargeting("anubarak dps target guard", Role::Any, AnubarakDpsTargetOwned, Family::DpsAssist);
    e.OwnTargeting("anubarak pickup tank target guard", Role::Any, AnubarakPickupTankHolds,
                   Family::DpsAssist | Family::TankAssist);
    e.OwnTargeting("anubarak side tank target guard", Role::Any, AnubarakSideTankHolds,
                   Family::DpsAssist | Family::TankAssist);
    e.OwnTargeting("anubarak sphere shooter target guard", Role::Any, AnubarakSphereShooterHolds,
                   Family::DpsAssist);
    e.OwnTargeting("anubarak main tank scarab guard", Role::Any, AnubarakMainTankLeavesScarabs, Family::TankAssist);

    e.Block("anubarak spike dodge hold", Role::Any, IsAnubarakDodgeHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach melee", "reach spell"});
    e.Block("anubarak boss drag hold", Role::Any, IsAnubarakDragInFlight,
            Family::SetBehind | Family::TankFace | Family::CombatFormationMove | Family::Follow | Family::Charge,
            {"reach melee"});
    e.Block("anubarak grip spot hold", Role::Any, AnubarakGripHolds,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Charge,
            {"reach melee"});
    e.Block("anubarak pickup tank stand guard", Role::Any, AnubarakPickupTankStands, Family::TankFace);
    e.Block("anubarak melee burrower stand hold", Role::Any, AnubarakMeleeBurrowerStandApplies,
            Family::SetBehind | Family::RearFlank, {"reach melee"});
    e.Block("anubarak gather hold", Role::Any, AnubarakGathers,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Charge |
                Family::Flee,
            {"reach melee", "reach spell", "reach party member to heal", "anubarak melee burrower stand"});
    e.Block("anubarak healer stand hold", Role::Heal, AnubarakHealerStandApplies,
            Family::CombatFormationMove | Family::Follow | Family::Flee);
    e.Block("anubarak ranged stand hold", Role::Ranged, AnubarakRangedStandApplies,
            Family::CombatFormationMove | Family::Follow | Family::Flee);
    e.Block("anubarak side tank follow hold", Role::Any, AnubarakSideTankHolds, Family::Follow);
    e.Block("anubarak side tank swarm leash", Role::Any, AnubarakSideTankSwarmLeash,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Charge,
            {"reach melee", "reach spell"});
    e.Block("anubarak hop escort hold", Role::Any, IsAnubarakEscort,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Charge,
            {"reach melee", "reach spell", "reach party member to heal", "anubarak melee burrower stand"});
    e.Block("anubarak hand reserve", Role::Any, AnubarakHandReserved, Family::Spell,
            {"anubarak heal penetrating cold", "anubarak interrupt shadow strike"});
    e.Block("anubarak main tank scarab taunt guard", Role::Any, AnubarakMainTankLeavesScarabs, Family::Taunt);
    e.Block("anubarak redirect guard", Role::Any, AnubarakEngaged, 0,
            {"misdirection on main tank", "tricks of the trade", "tricks of the trade on main tank"});
    e.Block("anubarak taunt guard", Role::Any, AnubarakTauntWouldSteal, Family::Taunt);
    e.Block("anubarak sphere shooter drop guard", Role::Any, AnubarakShooterOnDutySphere, Family::DropTarget);
    e.Block("anubarak grip reserve", Role::Any, AnubarakGripOwed, 0, {"death grip"});
    e.Block("anubarak side tank grip reserve", Role::Any, AnubarakSideTankHolds, 0, {"death grip"});
    e.Block("anubarak side tank righteous defense reserve", Role::Any, AnubarakSideTankHolds, 0,
            {"righteous defense"});
    e.Block("anubarak no ghouls", Role::Any, AnubarakEngaged, 0, {"army of the dead"});
    e.Block("anubarak tank sacrifice hold", Role::Tank, AnubarakEngaged, 0, {"hand of sacrifice on party"});
    e.Block("anubarak swarm sacrifice hold", Role::Any, AnubarakLeechingSwarmActive, 0,
            {"hand of sacrifice on party"});
    e.Block("anubarak swarm save hold", Role::Any, AnubarakSwarmHealing, 0,
            {"lay on hands", "lay on hands on party", "pain suppression", "pain suppression on party",
             "guardian spirit on party"});
    e.Block("anubarak swarm self save hold", Role::Any, AnubarakSwarmSavesHeld, 0,
            {"cloak of shadows", "evasion", "feint", "ice block", "divine shield", "divine protection",
             "dispersion", "barkskin", "shamanistic rage", "icebound fortitude", "anti magic shell",
             "deterrence", "enraged regeneration", "mana shield", "power word: shield", "healthstone",
             "healing potion", "lifeblood", "gift of the naaru"});
    e.Block("anubarak swarm light hold", Role::Any, AnubarakSwarmLightHeld, 0, {"judgement of light"});
    e.Block("anubarak swarm judgement hold", Role::Any, AnubarakSwarmJudgementHeld, 0,
            {"judgement", "judgement of wisdom", "judgement of justice"});
    e.Block("anubarak swarm mana spring hold", Role::Any, AnubarakLeechingSwarmActive, 0, {"mana spring totem"});
    e.Block("anubarak kite immunity reserve", Role::Any, AnubarakKiteImmunitiesKept, 0,
            {"divine shield", "ice block", "vanish", "feign death"});
    e.Block("anubarak kite sprint reserve", Role::Any, AnubarakKiteSprintKept, 0, {"sprint", "dash"});
    e.Block("anubarak kite cat hold", Role::Any, AnubarakKiteCatHeld, 0,
            {"caster form", "bear form", "dire bear form", "tree of life", "moonkin form"});
    e.Block("anubarak emerge bear hold", Role::Any, AnubarakMainTankKeepsBear, 0, {"caster form"});
    e.Block("anubarak main tank shout guard", Role::Any, AnubarakMainTankEngaged, 0,
            {"challenging roar", "challenging shout"});
    e.Block("anubarak swarm group heal guard", Role::Any, AnubarakSwarmHealing, 0,
            {"chain heal on party", "prayer of healing on party", "circle of healing on party",
             "wild growth on party", "wild growth blanket", "divine hymn", "tranquility"});
    e.Block("anubarak aura mastery reserve", Role::Any, AnubarakAuraMasteryReserved, 0, {"aura mastery"});
    e.Block("anubarak self innervate guard", Role::Any, AnubarakSelfInnervateHeld, 0, {"innervate"});

    e.Multiplier<AnubarakThreatCapMultiplier>(Family::Spell | Family::Melee);
    e.Multiplier<AnubarakBurrowerThreatCapMultiplier>(Family::Spell | Family::Melee);
    e.Multiplier<AnubarakSphereGuardMultiplier>(Family::Spell | Family::Attack);
    e.Multiplier<AnubarakBurrowerGuardMultiplier>(Family::Spell | Family::Attack);
    e.Multiplier<AnubarakSideTankAreaGuardMultiplier>(Family::Spell);
    e.Multiplier<AnubarakSwarmHealMultiplier>(Family::Spell | Family::ReachHeal);
}
}

EncounterDefinition const& ToCAnubarakDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::Anubarak), ToCGate,
                                                "anub-arak", &DefineAnubarak);
    return definition;
}
