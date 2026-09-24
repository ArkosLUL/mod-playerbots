/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

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
// One star alive is the state the pacing is trying to reach, so splash is harmless there. Phase 2 has
// no stars at all, which leaves Dark Matter cleave untouched.
bool AlgalonStarsPaced(PlayerbotAI* botAI)
{
    return AlgalonEncounterActive(botAI) && AlgalonAliveStarCount(botAI) >= 2;
}

// Only the roles the formation places. Melee and the off-tank hold the boss, so both keep every
// generic mover.
bool AlgalonFormationHolds(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return AlgalonEncounterActive(botAI) && (AlgalonTakesRingSlot(bot) || botAI->IsMainTank(bot));
}

void TickAlgalon(PlayerbotAI* botAI)
{
    if (GetAlgalon(botAI))
        AlgalonTickEncounterState(botAI);
}

void DefineAlgalon(EncounterBuilder& e)
{
    // Big Bang outranks everything because missing it is 76312 or a boss reset, Cosmic Smash comes
    // next on its hard 4s fuse, and stepping out of a hole beats both once neither is happening.
    // Position is last and yields as soon as it is parked.
    e.Node<AlgalonResetEncounterStateTrigger, AlgalonResetEncounterStateAction>(ACTION_EMERGENCY + 10);
    e.Node<AlgalonBigBangHideTrigger, AlgalonBigBangHideAction>(ACTION_EMERGENCY + 8, EncounterRow::Mover);
    e.Node<AlgalonBigBangSoakTrigger, AlgalonBigBangSoakAction>(ACTION_EMERGENCY + 8);
    e.Node<AlgalonCosmicSmashTrigger, AlgalonCosmicSmashAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<AlgalonLeaveBlackHoleTrigger, AlgalonLeaveBlackHoleAction>(ACTION_EMERGENCY + 4, EncounterRow::Mover);
    e.Node<AlgalonPhasePunchSwapTrigger, AlgalonPhasePunchSwapAction>(ACTION_RAID + 7);
    e.Node<AlgalonConstellationTauntTrigger, AlgalonConstellationTauntAction>(ACTION_RAID + 6);
    e.Node<AlgalonDarkMatterTankTrigger, AlgalonDarkMatterTankAction>(ACTION_RAID + 5);
    e.Node<AlgalonConstellationKiteTrigger, AlgalonConstellationKiteAction>(ACTION_RAID + 4, EncounterRow::Mover);
    e.Node<AlgalonCollapsingStarFocusTrigger, AlgalonCollapsingStarFocusAction>(ACTION_RAID + 3);
    e.Node<AlgalonDarkMatterMarkTrigger, AlgalonDarkMatterMarkAction>(ACTION_RAID + 2);
    e.Node<AlgalonRaidPositionTrigger, AlgalonRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    e.Multiplier<AlgalonSoakCooldownReserveMultiplier>(Family::Spell);

    // Collapsing Stars die one at a time on purpose, each death 16-21k to the whole raid, so an area
    // attack that clips a second one undoes the pacing.
    e.Block("algalon collapsing star aoe", Role::Any, AlgalonStarsPaced, Family::DpsAoe);

    // Zeroes whatever would land on the wrong target, so it has to see every action.
    e.Multiplier<AlgalonTargetGuardMultiplier>(Family::AnyAction);

    // The generic movers would walk the ranged half off their formation slots, and the slot node would
    // then fire again next tick and pace them all fight. ReachHeal walks a healer into range of someone
    // the rings can't reach.
    e.OwnMovement("algalon control movement", Role::Any, AlgalonFormationHolds,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    e.Tick(TickAlgalon);
}
}  // namespace

EncounterDefinition const& UldAlgalonDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_ALGALON, BossStateGate, "algalon", &DefineAlgalon);
    return definition;
}
