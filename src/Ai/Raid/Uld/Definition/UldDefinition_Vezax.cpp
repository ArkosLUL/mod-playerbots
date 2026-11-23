/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "Strategy.h"
#include "UldActions_Vezax.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Vezax.h"
#include "UldMultipliers_Vezax.h"
#include "UldTriggers_Vezax.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineVezax(EncounterBuilder& e)
{
    e.Node<VezaxResetEncounterStateTrigger, VezaxResetEncounterStateAction>(ACTION_EMERGENCY + 10);
    e.Node<VezaxShadowCrashDodgeTrigger, VezaxShadowCrashDodgeAction>(ACTION_EMERGENCY + 9, EncounterRow::Mover);
    e.Node<VezaxSearingFlamesInterruptTrigger, VezaxSearingFlamesInterruptAction>(ACTION_EMERGENCY + 8);
    e.Node<VezaxMarkOfTheFacelessBreakTrigger, VezaxMarkOfTheFacelessBreakAction>(ACTION_EMERGENCY + 7,
                                                                                 EncounterRow::Mover);
    e.Node<VezaxMarkOfTheFacelessTrigger, VezaxMarkOfTheFacelessAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<VezaxSurgeOfDarknessTrigger, VezaxSurgeOfDarknessAction>(ACTION_EMERGENCY + 5);
    e.Node<VezaxDamageHoldTrigger, VezaxDamageHoldAction>(ACTION_RAID + 8);
    e.Node<VezaxAnimusRedirectTrigger, VezaxAnimusRedirectAction>(ACTION_RAID + 7);
    e.Node<VezaxSaroniteAnimusTrigger, VezaxSaroniteAnimusAction>(ACTION_RAID + 6);
    e.Node<VezaxHoldTargetTrigger, VezaxHoldTargetAction>(ACTION_RAID + 5);
    e.Node<VezaxAnimusBringBackTrigger, VezaxAnimusBringBackAction>(ACTION_RAID + 4, EncounterRow::Mover);
    e.Node<VezaxAnimusMeleeSpotTrigger, VezaxAnimusMeleeSpotAction>(ACTION_RAID + 3, EncounterRow::Mover);
    e.Node<VezaxShadowCrashSoakTrigger, VezaxShadowCrashSoakAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node(
        "vezax shadow resistance",
        [](PlayerbotAI* botAI) -> Trigger* { return new BossShadowResistanceTrigger(botAI, "general vezax"); },
        "vezax shadow resistance action",
        [](PlayerbotAI* botAI) -> Action* { return new BossShadowResistanceAction(botAI, "general vezax"); },
        ACTION_RAID + 1);
    e.Node<VezaxRaidPositionTrigger, VezaxRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    e.OwnMovement("vezax control movement multiplier", Role::Ranged | Role::MainTank, VezaxFormationActive,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    e.OwnMovement("vezax animus melee movement multiplier", Role::Melee, VezaxAnimusPhaseActive);

    e.Block("vezax main tank anchor hold", Role::MainTank, VezaxAnimusOffAnchor, Family::Charge,
            {"intervene", "reach melee"});

    e.Multiplier<VezaxSuppressLifeTapMultiplier>(Family::Spell);

    e.OwnTargeting("vezax target guard multiplier", Role::Any, VezaxEncounterActive,
                   Family::DpsAssist | Family::TankAssist | Family::DebuffOnAttacker);

    e.Block("vezax hard mode hold", Role::Any, VezaxHardModePending, 0, {"heroism", "bloodlust", "starfall"});

    e.Block("vezax vapor area hold", Role::Any, VezaxVaporInAreaReach, 0,
            {"avenger's shield", "blade flurry", "bladestorm", "blast wave", "blizzard", "blood boil",
             "chain lightning", "cleave", "cone of cold", "consecration", "corpse explosion", "death and decay",
             "divine storm", "dragon's breath", "explosive trap", "fan of knives", "fire elemental totem",
             "fire elemental totem melee", "fire nova", "flamestrike", "frost nova", "hammer of the righteous",
             "holy nova", "holy wrath", "howling blast", "hurricane", "immolation aura", "killing spree",
             "living bomb", "magma totem", "mind sear", "multi-shot", "pestilence", "rain of fire",
             "seed of corruption", "shadowflame", "shadowfury", "shockwave", "sweeping strikes", "swipe (bear)",
             "swipe (cat)", "thunder clap", "thunderstorm", "trap launcher: explosive trap", "typhoon", "volley",
             "whirlwind"});

    e.Block("vezax damage hold", Role::Any, VezaxDamageHeld,
            Family::Melee | Family::PetAttack | Family::Charge | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti,
            {"army of the dead", "dancing rune weapon", "feral spirit", "fire elemental totem", "force of nature",
             "magma totem", "mirror image", "searing totem", "summon gargoyle"});
    e.Multiplier<VezaxDamageHoldMultiplier>(Family::Spell);

    e.Block("vezax save redirects for the animus", Role::Any, VezaxAnimusDue, 0,
            {"misdirection on main tank", "tricks of the trade", "tricks of the trade on main tank"});
    e.Multiplier<VezaxTricksOnTankMultiplier>(Family::Spell);

    e.Multiplier<VezaxHoldCastOutsideFieldMultiplier>(Family::Melee | Family::Spell);

    e.Multiplier<VezaxThreatMultiplier>(Family::Melee | Family::Spell);

    e.Tick(TickVezax);
}
}

EncounterDefinition const& UldVezaxDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_VEZAX, BossStateGate, "vezax", &DefineVezax);
    return definition;
}
