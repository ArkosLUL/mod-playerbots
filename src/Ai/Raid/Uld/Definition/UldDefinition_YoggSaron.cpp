/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"
#include "RaidTankDefensive.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Strategy.h"
#include "UldActions_YoggSaron.h"
#include "UldEncounterGate.h"
#include "UldEncounter_YoggSaron.h"
#include "UldMultipliers_YoggSaron.h"
#include "UldTriggers_YoggSaron.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool YoggSaronGuardianToHold(PlayerbotAI* botAI)
{
    constexpr float guardianSearchRadius = 70.0f;
    return YoggSaronInPhase3(botAI) && YoggSaronHoldableGuardianWithin(botAI->GetBot(), guardianSearchRadius);
}

bool YoggSaronTankTargetOwned(PlayerbotAI* botAI)
{
    return YoggSaronInPhase2(botAI) || YoggSaronGuardianToHold(botAI);
}

bool YoggSaronBrainIsTarget(PlayerbotAI* botAI)
{
    if (!YoggSaronOnBrainLevel(botAI->GetBot()))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && target->GetEntry() == NPC_BRAIN;
}

bool YoggSaronCrusherIsTarget(PlayerbotAI* botAI)
{
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && target->IsAlive() && target->GetEntry() == NPC_CRUSHER_TENTACLE &&
           !YoggSaronCrusherStunHolds(botAI, target);
}

bool YoggSaronInsideRing(PlayerbotAI* botAI)
{
    Position const& middle = ULDUAR_YOGG_SARON_MIDDLE;
    return botAI->GetBot()->GetDistance2d(middle.GetPositionX(), middle.GetPositionY()) <
               ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS &&
           YoggSaronEncounterActive(botAI);
}

void DefineYoggSaron(EncounterBuilder& e)
{
    e.Node(
        "sara shadow resistance trigger",
        [](PlayerbotAI* botAI) -> Trigger* { return new BossShadowResistanceTrigger(botAI, "sara"); },
        "sara shadow resistance action",
        [](PlayerbotAI* botAI) -> Action* { return new BossShadowResistanceAction(botAI, "sara"); }, ACTION_RAID);
    e.Node(
        "yogg-saron shadow resistance trigger",
        [](PlayerbotAI* botAI) -> Trigger* { return new BossShadowResistanceTrigger(botAI, "yogg-saron"); },
        "yogg-saron shadow resistance action",
        [](PlayerbotAI* botAI) -> Action* { return new BossShadowResistanceAction(botAI, "yogg-saron"); }, ACTION_RAID);
    e.Node<YoggSaronShadowAuraTrigger, YoggSaronShadowAuraAction>(ACTION_RAID);
    e.Node<YoggSaronGuardianPositioningTrigger, YoggSaronGuardianPositioningAction>(ACTION_RAID);
    e.Node<YoggSaronPhase1SpacingTrigger, YoggSaronPhase1SpacingAction>(ACTION_RAID + 2);
    e.Node<YoggSaronPhase1StationTrigger, YoggSaronPhase1StationAction>(ACTION_RAID);
    e.Node<YoggSaronDarkVolleyTrigger, YoggSaronDarkVolleyInterruptAction>(ACTION_EMERGENCY + 1);
    e.Node<YoggSaronSanityTrigger, YoggSaronSanityAction>(ACTION_RAID + 1);
    e.Node<YoggSaronMaladyOfTheMindTrigger, YoggSaronMaladyOfTheMindAction>(ACTION_RAID);
    e.Node<YoggSaronAntiFearTrigger, YoggSaronAntiFearAction>(ACTION_RAID + 2);
    e.Node<YoggSaronPhase3ControlTrigger, YoggSaronPhase3ControlAction>(ACTION_RAID);
    e.Node<YoggSaronSetDpsPriorityTrigger, YoggSaronSetDpsPriorityAction>(ACTION_RAID + 1);
    e.Node<YoggSaronPhase2SpacingTrigger, YoggSaronPhase2SpacingAction>(ACTION_RAID + 3);
    e.Node<YoggSaronBrainLinkTrigger, YoggSaronBrainLinkAction>(ACTION_RAID + 1.5f);
    e.Node<YoggSaronMoveToEnterPortalTrigger, YoggSaronMoveToEnterPortalAction>(ACTION_RAID);
    e.Node<YoggSaronUsePortalTrigger, YoggSaronUsePortalAction>(ACTION_RAID);
    e.Node<YoggSaronFallFromFloorTrigger, YoggSaronFallFromFloorAction>(ACTION_RAID);
    e.Node<YoggSaronStopFollowingTrigger, YoggSaronStopFollowingAction>(ACTION_RAID);
    e.Node<YoggSaronIllusionRoomTrigger, YoggSaronIllusionRoomAction>(ACTION_RAID);
    e.Node<YoggSaronMoveToExitPortalTrigger, YoggSaronMoveToExitPortalAction>(ACTION_RAID + 4);
    e.Node<YoggSaronLunaticGazeTrigger, YoggSaronLunaticGazeAction>(ACTION_EMERGENCY);
    e.Node<YoggSaronLaughingSkullTrigger, YoggSaronLaughingSkullAction>(ACTION_EMERGENCY);
    e.Node<YoggSaronPhase3PositioningTrigger, YoggSaronPhase3PositioningAction>(ACTION_RAID);
    e.Node<YoggSaronGuardianControlTrigger, YoggSaronGuardianControlAction>(ACTION_RAID + 3);
    e.Node<YoggSaronBeaconDragTrigger, YoggSaronBeaconDragAction>(ACTION_RAID + 4);
    e.Node<YoggSaronBeaconStageTrigger, YoggSaronBeaconStageAction>(ACTION_RAID + 3.5f);
    e.Node<YoggSaronBeaconBearFormTrigger, YoggSaronBeaconBearFormAction>(ACTION_RAID + 4.5f);
    e.Node<YoggSaronPhase3LustTrigger, YoggSaronPhase3LustAction>(ACTION_RAID + 2);
    e.Node<YoggSaronPhase3YoggDotTrigger, YoggSaronPhase3YoggDotAction>(ACTION_HIGH + 4.1f);
    e.Node<YoggSaronSanityConservationTrigger, YoggSaronSanityConservationAction>(ACTION_RAID + 5);
    e.Node<YoggSaronSqueezeEscapeTrigger, YoggSaronSqueezeEscapeAction>(ACTION_RAID + 1);
    e.Node<YoggSaronSqueezeRescueTrigger, YoggSaronSqueezeRescueAction>(ACTION_RAID + 6);

    e.Node<YoggSaronNovaRaidCooldownTrigger, YoggSaronNovaRaidCooldownAction>(ACTION_EMERGENCY + 7);
    e.Node<YoggSaronNovaDefensiveTrigger, YoggSaronNovaDefensiveAction>(ACTION_EMERGENCY + 6);
    e.Node<YoggSaronNovaExternalTrigger, YoggSaronNovaExternalAction>(ACTION_EMERGENCY + 5);

    e.Node<YoggSaronBeaconDefensiveTrigger, YoggSaronBeaconDefensiveAction>(ACTION_EMERGENCY + 6);
    e.Node<YoggSaronBeaconExternalTrigger, YoggSaronBeaconExternalAction>(ACTION_EMERGENCY + 5);

    e.Node<YoggSaronTankLayOnHandsTrigger, YoggSaronTankLayOnHandsAction>(ACTION_EMERGENCY + 4);

    e.Node<YoggSaronRoarAuraMasteryTrigger, YoggSaronRoarAuraMasteryAction>(ACTION_EMERGENCY + 8);

    e.Node<YoggSaronHealerCombatEngineTrigger, YoggSaronHealerCombatEngineAction>(ACTION_EMERGENCY + 9);
    e.Node<YoggSaronRoarAuraTrigger, YoggSaronRoarAuraAction>(ACTION_RAID + 1);
    e.Node<YoggSaronIllusionFacingTrigger, YoggSaronIllusionFacingAction>(ACTION_RAID + 3);
    e.Node<YoggSaronPetGuardTrigger, YoggSaronPetGuardAction>(ACTION_RAID + 2);
    e.Node<YoggSaronDiminishPowerBreakTrigger, YoggSaronDiminishPowerBreakAction>(ACTION_RAID + 2.5f);
    e.Node<YoggSaronBodyDetourTrigger, YoggSaronBodyDetourAction>(ACTION_RAID - 1);
    e.Node<YoggSaronDiminishPowerJudgementTrigger, YoggSaronDiminishPowerJudgementAction>(ACTION_RAID - 0.5f);
    e.Node<YoggSaronMortalStrikeTrigger, YoggSaronMortalStrikeAction>(ACTION_RAID - 0.5f);
    e.Node<YoggSaronWarlockPetTrigger, YoggSaronWarlockPetAction>(ACTION_RAID - 0.5f);
    e.Node<YoggSaronIllusionHealerStationTrigger, YoggSaronIllusionHealerStationAction>(ACTION_RAID);
    e.Node<YoggSaronReprisalRegroupTrigger, YoggSaronReprisalRegroupAction>(ACTION_RAID);
    e.Node<YoggSaronReprisalHealTrigger, YoggSaronReprisalHealAction>(ACTION_RAID - 1);
    e.Node<YoggSaronBrainSpotTrigger, YoggSaronBrainSpotAction>(ACTION_RAID);
    e.Node<YoggSaronNitroBoostsTrigger, YoggSaronNitroBoostsAction>(ACTION_RAID + 0.5f);
    e.Node<YoggSaronBrainDashTrigger, YoggSaronBrainDashAction>(ACTION_RAID + 0.5f);
    e.Node<YoggSaronBrainChargeTrigger, YoggSaronBrainChargeAction>(ACTION_RAID + 0.25f);

    e.OwnTargeting("yogg-saron dps target guard multiplier", Role::NonTank, YoggSaronEncounterActive,
                   Family::DpsAssist);
    e.OwnTargeting("yogg-saron dps target guard multiplier", Role::Tank, YoggSaronTankTargetOwned,
                   Family::TankAssist);

    e.Block("yogg-saron displacement guard multiplier", Role::Any, YoggSaronEncounterActive,
            Family::Blink | Family::Disengage);
    e.Block("yogg-saron displacement guard multiplier", Role::Any, YoggSaronInPhase1, Family::Charge,
            {"killing spree"});
    e.Block("yogg-saron displacement guard multiplier", Role::Any, YoggSaronBrainIsTarget, Family::Charge,
            {"killing spree"});

    e.Block("yogg-saron reprisal hold", Role::Any, YoggSaronReprisalHold,
            Family::Melee | Family::DpsAoe | Family::AggressiveTarget | Family::AttackAnything |
                Family::AttackLeastHp | Family::AttackRti | Family::DebuffOnAttacker | Family::Charge);

    e.Block("yogg-saron illusion stage hold", Role::Any, YoggSaronIllusionStaged,
            Family::Melee | Family::DpsAoe | Family::AggressiveTarget | Family::AttackAnything |
                Family::AttackLeastHp | Family::DebuffOnAttacker | Family::Charge);

    e.Block("yogg-saron brain hold", Role::Any, YoggSaronBrainHeld,
            Family::Melee | Family::DpsAoe | Family::AggressiveTarget | Family::AttackAnything |
                Family::AttackLeastHp | Family::AttackRti | Family::Charge | Family::PetAttack);
    e.Block("yogg-saron brain dot hold", Role::Any, YoggSaronBrainDotHeld, Family::DebuffOnAttacker);

    e.Block("yogg-saron portal spot hold", Role::Any, YoggSaronHoldingPortalSpot,
            Family::Reach | Family::ReachHeal | Family::SetBehind | Family::RearFlank |
                Family::CombatFormationMove | Family::Charge | Family::Follow | Family::MoveRandom);

    e.Block("yogg-saron crusher pet spell hold", Role::Any, YoggSaronCrusherIsTarget, 0,
            {"pet acid spit", "shadowfiend"});

    e.Multiplier<YoggSaronMovementGuardMultiplier>(Family::Flee | Family::SetBehind | Family::Reach |
                                                   Family::ReachHeal);

    e.Multiplier<YoggSaronPhase1AoeHoldMultiplier>(Family::AnyAction);
    e.Multiplier<YoggSaronIllusionMultiTargetHoldMultiplier>(Family::AnyAction);
    e.Multiplier<YoggSaronExitHoldCastMultiplier>(Family::Spell);
    e.Multiplier<YoggSaronHeldTargetCastMultiplier>(Family::Spell);
    e.Multiplier<YoggSaronPhase3AreaMultiplier>(Family::AnyAction);
    e.Multiplier<YoggSaronCrusherAreaHoldMultiplier>(Family::Spell);

    e.Block("yogg-saron stack food guard multiplier", Role::Any, YoggSaronInsideRing, 0, {"eat", "drink"});

    e.Block("yogg-saron nova defensive hold", Role::Melee, YoggSaronNovaHold, 0, HeldMagicDefensiveNames());

    e.Block("yogg-saron nova cooldown hold", Role::Any, YoggSaronNovaHold, 0,
            {"pain suppression", "pain suppression on party", "guardian spirit on party",
             "hand of sacrifice on party", "divine sacrifice"});

    e.Block("yogg-saron shadow aura hold", Role::Any, YoggSaronShadowAuraHeld, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "shadow resistance aura", "fire resistance aura", "frost resistance aura"});

    e.Block("yogg-saron aura mastery hold", Role::Any, YoggSaronNovaHold, 0, {"aura mastery"});

    e.Block("yogg-saron shadow aura other hold", Role::Any, YoggSaronShadowAuraOtherHeld, 0,
            {"shadow resistance aura", "aura mastery", "sara shadow resistance action",
             "yogg-saron shadow resistance action"});

    e.Block("yogg-saron battle rez hold", Role::Any, YoggSaronBattleRezHeld, 0, {"rebirth"});

    e.Block("yogg-saron roar aura hold", Role::Any, YoggSaronRoarAuraHeld, 0,
            {"devotion aura", "retribution aura", "crusader aura", "sanctity aura", "shadow resistance aura",
             "fire resistance aura", "frost resistance aura", "sara shadow resistance action",
             "yogg-saron shadow resistance action"});

    e.Block("yogg-saron roar aura mastery hold", Role::Any, YoggSaronRoarExpected, 0, {"aura mastery"});

    e.Block("yogg-saron beacon immunity hold", Role::Any, YoggSaronBeaconImmunityHeld, 0,
            {"divine shield", "ice block"});

    e.Block("yogg-saron beacon hunter hold", Role::Any, YoggSaronBeaconHunterHold, 0,
            {"feign death", "deterrence"});

    e.Block("yogg-saron beacon bear hold", Role::Any, YoggSaronBeaconBearHeld, 0, {"cat form", "caster form"});

    e.Block("yogg-saron death ray avoid aoe hold", Role::Any, YoggSaronDeathRayNear, Family::AvoidAoe);

    e.Block("yogg-saron tank swap hold", Role::Tank, YoggSaronTankShedding, 0,
            {"consecration", "hammer of the righteous", "avenger's shield", "hand of reckoning",
             "righteous defense"});

    e.Block("yogg-saron phase 3 taunt hold", Role::Tank, YoggSaronInPhase3, Family::Taunt);

    e.Block("yogg-saron tank wrath hold", Role::Tank, YoggSaronTankWrathHeld, 0, {"avenging wrath"});

    e.Block("yogg-saron tank sacrifice hold", Role::Tank, YoggSaronInPhase3, 0, {"hand of sacrifice on party"});

    e.Block("yogg-saron lay on hands hold", Role::Any, YoggSaronEncounterActive, 0, {"lay on hands on party"});
    e.Block("yogg-saron lay on hands hold", Role::NonTank, YoggSaronEncounterActive, 0, {"lay on hands"});
    e.Block("yogg-saron lay on hands hold", Role::Tank, YoggSaronBeforePhase3, 0, {"lay on hands"});

    e.Block("yogg-saron healer target hold", Role::Heal, YoggSaronHealerTargetless, 0, {"drop target"});

    e.Multiplier<YoggSaronPhase1WalkGuardMultiplier>(Family::AnyMovement);
    e.Multiplier<YoggSaronAntiFearTotemGuardMultiplier>(Family::Spell);

    e.Tick(YoggSaronTick);
}
}

EncounterDefinition const& UldYoggSaronDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_YOGGSARON, BossStateGate, "yogg-saron", &DefineYoggSaron);
    return definition;
}
