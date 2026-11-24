/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "Player.h"
#include "PlayerbotAI.h"
#include "Strategy.h"
#include "UldActions_Algalon.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Algalon.h"
#include "UldMultipliers_Algalon.h"
#include "UldTriggers_Algalon.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool AlgalonCooldownsHeld(PlayerbotAI* botAI)
{
    return AlgalonEngaged(botAI) &&
           (AlgalonBigBangCasting(botAI) || AlgalonBigBangWithin(botAI, ULDUAR_ALGALON_COOLDOWN_HOLD_SECONDS) ||
            IsAlgalonPhased(botAI->GetBot()));
}

bool AlgalonExplosionButtonsHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return AlgalonEngaged(botAI) && bot->GetHealthPct() > ULDUAR_ALGALON_BUTTON_HOLD_PCT &&
           (IsAlgalonPhased(bot) || (AlgalonInPhaseOne(botAI) && !IsAlgalonSwapTank(bot)));
}

bool AlgalonHidingNow(PlayerbotAI* botAI) { return AlgalonShouldRunForShelter(botAI->GetBot()); }

bool AlgalonHiderCasting(PlayerbotAI* botAI) { return AlgalonHiderInCast(botAI->GetBot()); }

bool AlgalonStarTeamFocused(PlayerbotAI* botAI)
{
    return IsAlgalonStarTeam(botAI->GetBot()) && GetAlgalonFocusStar(botAI) && AlgalonEngaged(botAI);
}

bool AlgalonYielding(PlayerbotAI* botAI) { return AlgalonYieldsBoss(botAI->GetBot()); }

bool AlgalonTankHealing(PlayerbotAI* botAI) { return IsAlgalonTankHealer(botAI->GetBot()); }

bool AlgalonFireTotemHeld(PlayerbotAI* botAI) { return IsAlgalonFireTotemShaman(botAI->GetBot()); }

bool AlgalonShadowAuraHeld(PlayerbotAI* botAI)
{
    return AlgalonInPhaseOne(botAI) && IsAlgalonShadowAuraPaladin(botAI->GetBot());
}

bool AlgalonHandlerKiting(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!AlgalonEngaged(botAI) || !IsAlgalonHandler(bot) || AlgalonTakesBossNow(bot))
        return false;

    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    return constellation && (constellation->GetVictim() == bot || AlgalonKitingBehind(bot, constellation));
}

bool AlgalonFormationHolds(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!AlgalonPresent(botAI))
        return false;

    if (AlgalonTakesRingSlot(bot))
        return true;

    if (!IsAlgalonSwapTank(bot))
        return false;

    return !AlgalonEngaged(botAI) || !AlgalonOffTankWorking(bot);
}

bool AlgalonMeleeHeldBack(PlayerbotAI* botAI) { return AlgalonMeleeStaged(botAI->GetBot()); }

bool AlgalonGoingToFarHole(PlayerbotAI* botAI) { return AlgalonHeadsForFarHole(botAI->GetBot()); }

bool AlgalonSmashHold(PlayerbotAI* botAI) { return AlgalonEngaged(botAI) && AlgalonSmashHoldsGround(botAI->GetBot()); }

bool AlgalonSmashChargeHold(PlayerbotAI* botAI)
{
    if (AlgalonSmashHold(botAI))
        return true;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return AlgalonEngaged(botAI) && target &&
           AlgalonCosmicSmashMarkerNear(botAI, target->GetPosition(),
                                        ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE + target->GetCombatReach());
}

void DefineAlgalon(EncounterBuilder& e)
{
    e.Node<AlgalonBigBangSoakTrigger, AlgalonBigBangSoakAction>(ACTION_EMERGENCY + 9);
    e.Node<AlgalonBigBangExternalTrigger, AlgalonBigBangExternalAction>(ACTION_EMERGENCY + 8);
    e.Node<AlgalonBigBangHideTrigger, AlgalonBigBangHideAction>(ACTION_EMERGENCY + 7, EncounterRow::Mover);
    e.Node<AlgalonCosmicSmashTrigger, AlgalonCosmicSmashAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<AlgalonSoakerPrepTrigger, AlgalonSoakerPrepAction>(ACTION_EMERGENCY + 5);
    e.Node<AlgalonLeaveBlackHoleTrigger, AlgalonLeaveBlackHoleAction>(ACTION_EMERGENCY + 4, EncounterRow::Mover);
    e.Node<AlgalonSoakerRescueTrigger, AlgalonSoakerRescueAction>(ACTION_EMERGENCY + 3);
    e.Node<AlgalonTankDefensiveTrigger, AlgalonTankDefensiveAction>(ACTION_EMERGENCY + 2);
    e.Node<AlgalonExplosionCoverTrigger, AlgalonExplosionCoverAction>(ACTION_EMERGENCY + 1);
    e.Node<AlgalonExplosionPersonalTrigger, AlgalonExplosionPersonalAction>(ACTION_EMERGENCY);
    e.Node<AlgalonTankHealCriticalTrigger, AlgalonTankHealAction>(ACTION_RAID + 9);
    e.Node<AlgalonTankPickupTrigger, AlgalonTankPickupAction>(ACTION_RAID + 8);
    e.Node<AlgalonPhasePunchSwapTrigger, AlgalonPhasePunchSwapAction>(ACTION_RAID + 7);
    e.Node<AlgalonYieldBossTrigger, AlgalonYieldBossAction>(ACTION_RAID + 7);
    e.Node<AlgalonConstellationTauntTrigger, AlgalonConstellationTauntAction>(ACTION_RAID + 6);
    e.Node<AlgalonOffTankTauntTrigger, AlgalonOffTankTauntAction>(ACTION_RAID + 6);
    e.Node<AlgalonDarkMatterTankTrigger, AlgalonDarkMatterTankAction>(ACTION_RAID + 5);
    e.Node<AlgalonConstellationKiteTrigger, AlgalonConstellationKiteAction>(ACTION_RAID + 4, EncounterRow::Mover);
    e.Node<AlgalonStarTeamTrigger, AlgalonStarTeamAction>(ACTION_RAID + 3);
    e.Node<AlgalonStarMarkTrigger, AlgalonStarMarkAction>(ACTION_RAID + 2);
    e.Node<AlgalonThreatRedirectTrigger, AlgalonThreatRedirectAction>(ACTION_RAID + 1);
    e.Node<AlgalonRaidPositionTrigger, AlgalonRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);
    e.Node<AlgalonFireTotemTrigger, AlgalonFireTotemAction>(ACTION_RAID + 0.5f);
    e.Node<AlgalonShadowAuraTrigger, AlgalonShadowAuraAction>(ACTION_MEDIUM_HEAL + 2.0f);
    e.Node<AlgalonTankBeaconTrigger, AlgalonTankBeaconAction>(ACTION_MEDIUM_HEAL + 1.0f);
    e.Node<AlgalonTankHealTrigger, AlgalonTankHealAction>(ACTION_MEDIUM_HEAL + 0.75f);

    e.Multiplier<AlgalonLeaveFieldMultiplier>(Family::AnyAction);
    e.Multiplier<AlgalonCooldownReserveMultiplier>(Family::Spell);
    e.Multiplier<AlgalonTankHealerMultiplier>(Family::Spell | Family::ReachHeal);
    e.Multiplier<AlgalonHideCastMultiplier>(Family::Spell);
    e.Block("algalon phase immunity hold", Role::Any, AlgalonCooldownsHeld, 0,
            {"ice block", "divine shield", "cloak of shadows", "anti magic shell"});
    e.Block("algalon explosion button hold", Role::Any, AlgalonExplosionButtonsHeld, 0,
            {"barkskin", "shadow ward", "icebound fortitude", "shamanistic rage", "divine protection"});
    e.Block("algalon explosion cover hold", Role::Any, AlgalonInPhaseOne, 0, {"divine sacrifice"});
    e.Block("algalon food guard", Role::Any, AlgalonEngaged, 0, {"eat", "drink"});
    e.Block("algalon aura mastery hold", Role::Any, AlgalonEngaged, 0, {"aura mastery"});
    e.Block("algalon shadow aura hold", Role::Any, AlgalonShadowAuraHeld, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "shadow resistance aura", "fire resistance aura", "frost resistance aura"});
    e.Block("algalon fire totem hold", Role::Any, AlgalonFireTotemHeld, 0,
            {"mana spring totem", "healing stream totem", "cleansing totem", "set mana spring totem",
             "set healing stream totem", "set cleansing totem"});
    e.Block("algalon stray frost aura", Role::Any, AlgalonEngaged, 0, {"frost resistance aura"});
    e.Block("algalon tank healer buffs", Role::Heal, AlgalonTankHealing, 0,
            {"beacon of light on tank", "beacon of light on main tank", "sacred shield on tank",
             "sacred shield on main tank", "earth shield on main tank", "earth shield on party", "divine hymn"});

    e.Exclusive("algalon big bang hide", Role::Any, AlgalonHidingNow, 0,
                {AlgalonBigBangHideAction::Name, AlgalonLeaveBlackHoleAction::Name, AlgalonCosmicSmashAction::Name});
    e.Block("algalon big bang spell movers", Role::Any, AlgalonHiderCasting,
            Family::Charge | Family::Blink | Family::Disengage, {"killing spree"});

    e.Block("algalon taunt guard", Role::Any, AlgalonEngaged, Family::Taunt);
    e.Block("algalon tank form hold", Role::Tank, AlgalonEngaged, 0, {"caster form"});
    e.Block("algalon class tricks", Role::Any, AlgalonEngaged, 0,
            {"tricks of the trade", "tricks of the trade on main tank"});
    e.Block("algalon yield skull", Role::Tank, AlgalonYielding, Family::AttackRti);

    e.OwnTargeting("algalon star team", Role::Dps, AlgalonStarTeamFocused);

    e.Multiplier<AlgalonSplashHoldMultiplier>(Family::AnyAction);
    e.Multiplier<AlgalonTargetGuardMultiplier>(Family::Melee | Family::Spell | Family::PetAttack);

    e.Multiplier<AlgalonWalkGuardMultiplier>(Family::Reach | Family::ReachHeal | Family::SetBehind |
                                             Family::RearFlank | Family::Charge);

    e.OwnMovement("algalon kite movement", Role::Tank, AlgalonHandlerKiting, 0,
                  {AlgalonConstellationKiteAction::Name, AlgalonBigBangHideAction::Name, AlgalonCosmicSmashAction::Name,
                   AlgalonLeaveBlackHoleAction::Name, AlgalonTankPickupAction::Name, AlgalonPhasePunchSwapAction::Name,
                   AlgalonConstellationTauntAction::Name, AlgalonDarkMatterTankAction::Name});

    e.OwnMovement("algalon control movement", Role::Ranged | Role::Tank, AlgalonFormationHolds,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    e.OwnMovement("algalon melee staging", Role::Melee, AlgalonMeleeHeldBack, Family::Attack);
    e.Block("algalon melee staging charges", Role::Melee, AlgalonMeleeHeldBack, Family::Charge);

    e.OwnMovement("algalon smash hold", Role::Any, AlgalonSmashHold, Family::Attack);
    e.Block("algalon smash hold charges", Role::Melee, AlgalonSmashChargeHold, Family::Charge);

    e.OwnMovement("algalon far hole", Role::Any, AlgalonGoingToFarHole, Family::Attack);

    e.Tick(AlgalonTickEncounterState);
}
}

EncounterDefinition const& UldAlgalonDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_ALGALON, BossStateGate, "algalon", &DefineAlgalon);
    return definition;
}
