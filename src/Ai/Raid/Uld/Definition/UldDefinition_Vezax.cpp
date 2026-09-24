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
    // The engine stops at the first action that returns true, so this order is a survival ranking,
    // and no two rows share a number: a tie falls to insertion order, which is not a decision anyone
    // made.
    //
    // The dodge leads: Shadow Crash is 11310 plus a knockback, and it's the only hazard here with a
    // deadline, about 2.7-3.6s of missile flight. The interrupt comes next, because losing a kick
    // costs the whole raid 13875-16125 fire at once. Then the two halves of Mark of the Faceless,
    // which is the only mechanic whose failure heals the boss (5000 a tick off everyone nearby, at 20x
    // back into him) but drains over 10s rather than landing at once, so both sit under the dodge.
    // Breaking out of someone else's mark outranks carrying your own: the bot that holds it is the one
    // person its leech skips, so it's never the one taking damage.
    //
    // The RAID band is the reward half, and soaking a field is worth nothing to a bot that is already
    // dying. The Animus redirect needs the bot on it already, so it never competes with the switch.
    // Bringing the Animus back to the tank sits above the soak, or its victim would lead it into the camp.
    // Position is last on purpose, and yields as soon as it's parked, so the class interrupts at
    // ACTION_INTERRUPT (40) still get a tick.
    e.Node<VezaxResetEncounterStateTrigger, VezaxResetEncounterStateAction>(ACTION_EMERGENCY + 10);
    e.Node<VezaxShadowCrashDodgeTrigger, VezaxShadowCrashDodgeAction>(ACTION_EMERGENCY + 9, EncounterRow::Mover);
    e.Node<VezaxSearingFlamesInterruptTrigger, VezaxSearingFlamesInterruptAction>(ACTION_EMERGENCY + 8);
    e.Node<VezaxMarkOfTheFacelessBreakTrigger, VezaxMarkOfTheFacelessBreakAction>(ACTION_EMERGENCY + 7,
                                                                                 EncounterRow::Mover);
    e.Node<VezaxMarkOfTheFacelessTrigger, VezaxMarkOfTheFacelessAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<VezaxSurgeOfDarknessTrigger, VezaxSurgeOfDarknessAction>(ACTION_EMERGENCY + 5);
    e.Node<VezaxAnimusRedirectTrigger, VezaxAnimusRedirectAction>(ACTION_RAID + 6);
    e.Node<VezaxSaroniteAnimusTrigger, VezaxSaroniteAnimusAction>(ACTION_RAID + 5);
    e.Node<VezaxHoldTargetTrigger, VezaxHoldTargetAction>(ACTION_RAID + 4);
    e.Node<VezaxAnimusBringBackTrigger, VezaxAnimusBringBackAction>(ACTION_RAID + 3, EncounterRow::Mover);
    e.Node<VezaxShadowCrashSoakTrigger, VezaxShadowCrashSoakAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node(
        "vezax shadow resistance",
        [](PlayerbotAI* ai) -> Trigger* { return new BossShadowResistanceTrigger(ai, "general vezax"); },
        "vezax shadow resistance action",
        [](PlayerbotAI* ai) -> Action* { return new BossShadowResistanceAction(ai, "general vezax"); },
        ACTION_RAID + 1);
    e.Node<VezaxRaidPositionTrigger, VezaxRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    // The formation pins the ranged half and the healers to arc slots and the main tank to the anchor
    // every radius is measured from, so the generic movers stand down for them or the formation gets
    // re-derived and abandoned on alternate ticks. Melee ride the boss and keep every generic mover,
    // including the de-clump the position node falls through to. Reach keeps the tank in melee, and
    // ReachHeal walks a healer into range of someone the formation can't reach.
    e.OwnMovement("vezax control movement multiplier", Role::Ranged | Role::MainTank, VezaxFormationActive,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    e.Multiplier<VezaxSuppressLifeTapMultiplier>(Family::Spell);

    // Saronite Vapors are hostile, pulse damage on anyone near them, and the generic pickers happily
    // take one. Killing one calls DoAction(1) on the boss and ends hard mode for good. The debuff
    // family matters as much as the two pickers: it lands DoTs on whatever a caster drifted onto.
    //
    // This takes away the only generic source of a target, so the hold target row above has to be the
    // one that gives it back. Without it no bot ever calls Attack, the combat engine never starts and
    // the raid stands there healing itself - two pulls on 2026-09-23 left him at 99% health.
    e.OwnTargeting("vezax target guard multiplier", Role::Any, VezaxEncounterActive,
                   Family::DpsAssist | Family::TankAssist | Family::DebuffOnAttacker);

    // Hard mode only, until the Animus is summoned or a vapor dies. Lust waits for the Animus. Starfall's
    // stars pick random enemies in range, which the target guard never sees, and a vapor can spawn under
    // one already falling, so a range check wouldn't do.
    e.Block("vezax hard mode hold", Role::Any, VezaxHardModePending, 0, {"heroism", "bloodlust", "starfall"});

    // Kept off cooldown for the Animus, which only a tank's threat holds. Left alone, the generic
    // triggers spend them on Vezax, where the tank has had minutes to build threat, or on a dps.
    e.Block("vezax save redirects for the animus", Role::Any, VezaxAnimusDue, 0,
            {"misdirection on main tank", "tricks of the trade", "tricks of the trade on main tank"});

    e.Multiplier<VezaxHoldCastOutsideFieldMultiplier>(Family::Melee | Family::Spell);

    // Nothing generic holds a dps bot under the tank's threat, since no bot ever gets the "threat"
    // strategy, and the Animus can't be taunted back once one passes him.
    e.Multiplier<VezaxAnimusThreatMultiplier>(Family::Melee | Family::Spell);

    e.Tick(TickVezax);
}
}  // namespace

EncounterDefinition const& UldVezaxDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_VEZAX, BossStateGate, "vezax", &DefineVezax);
    return definition;
}
