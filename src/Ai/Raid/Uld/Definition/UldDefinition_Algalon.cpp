/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "Player.h"
#include "PlayerbotAI.h"
#include "RaidTankDefensive.h"
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
           (AlgalonBigBangCasting(botAI) || AlgalonBigBangWithin(botAI, ULDUAR_ALGALON_COOLDOWN_HOLD_SECONDS));
}

// Either swap tank can end up holding him when the cast starts, so both keep their buttons.
bool AlgalonTankDefensivesHeld(PlayerbotAI* botAI)
{
    return IsAlgalonSwapTank(botAI->GetBot()) && AlgalonCooldownsHeld(botAI);
}

// Phased bots are left out: inside the void they are safe, and phase-16 Dark Matter is on them.
bool AlgalonHidingNow(PlayerbotAI* botAI) { return AlgalonShouldRunForShelter(botAI->GetBot()); }

// A star isn't an attacker until something hits it, so "dps assist" would pull the team straight back
// to Algalon between the star node's checks.
bool AlgalonStarTeamFocused(PlayerbotAI* botAI)
{
    return IsAlgalonStarTeam(botAI->GetBot()) && GetAlgalonFocusStar(botAI) && AlgalonEngaged(botAI);
}

bool AlgalonSwapTankEngaged(PlayerbotAI* botAI)
{
    return IsAlgalonSwapTank(botAI->GetBot()) && AlgalonEngaged(botAI);
}

bool AlgalonHandlerKiting(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!AlgalonEngaged(botAI) || GetAlgalonHandler(botAI) != bot)
        return false;

    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    return constellation && constellation->GetVictim() == bot;
}

// Only the roles the formation places. Melee and the tank not holding him keep every generic mover.
bool AlgalonFormationHolds(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!AlgalonPresent(botAI))
        return false;

    if (AlgalonTakesRingSlot(bot))
        return true;

    Player* holder = AlgalonEngaged(botAI) ? GetAlgalonBossTank(botAI) : nullptr;
    return holder && IsAlgalonSwapTank(holder) ? holder == bot : botAI->IsMainTank(bot);
}

void DefineAlgalon(EncounterBuilder& e)
{
    // A survival ranking. Missing Big Bang is 76-112k or a reset, Cosmic Smash lands ~4.8s after its
    // marker, and a bot left inside a hole when its phase ends is phased again. Position is last and
    // yields as soon as it is parked.
    e.Node<AlgalonBigBangSoakTrigger, AlgalonBigBangSoakAction>(ACTION_EMERGENCY + 9);
    e.Node<AlgalonBigBangExternalTrigger, AlgalonBigBangExternalAction>(ACTION_EMERGENCY + 8);
    e.Node<AlgalonBigBangHideTrigger, AlgalonBigBangHideAction>(ACTION_EMERGENCY + 7, EncounterRow::Mover);
    e.Node<AlgalonCosmicSmashTrigger, AlgalonCosmicSmashAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<AlgalonLeaveBlackHoleTrigger, AlgalonLeaveBlackHoleAction>(ACTION_EMERGENCY + 4, EncounterRow::Mover);
    e.Node<AlgalonTankPickupTrigger, AlgalonTankPickupAction>(ACTION_RAID + 8);
    e.Node<AlgalonPhasePunchSwapTrigger, AlgalonPhasePunchSwapAction>(ACTION_RAID + 7);
    e.Node<AlgalonConstellationTauntTrigger, AlgalonConstellationTauntAction>(ACTION_RAID + 6);
    e.Node<AlgalonDarkMatterTankTrigger, AlgalonDarkMatterTankAction>(ACTION_RAID + 5);
    e.Node<AlgalonConstellationKiteTrigger, AlgalonConstellationKiteAction>(ACTION_RAID + 4, EncounterRow::Mover);
    e.Node<AlgalonStarTeamTrigger, AlgalonStarTeamAction>(ACTION_RAID + 3);
    e.Node<AlgalonStarMarkTrigger, AlgalonStarMarkAction>(ACTION_RAID + 2);
    e.Node<AlgalonRaidPositionTrigger, AlgalonRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    // The soak and externals cast these directly, so they stay free for the one cast they're for.
    e.Block("algalon hold tank defensives", Role::Tank, AlgalonTankDefensivesHeld, 0, HeldTankDefensiveNames());
    e.Block("algalon hold big bang cooldowns", Role::Any, AlgalonCooldownsHeld, 0,
            {"dispersion", "pain suppression", "guardian spirit on party"});

    // Every other mover would walk a hider back out of its hole, and MoveTo answers Duplicate from the
    // run's second tick, so the run can't defend itself.
    e.Exclusive("algalon big bang hide", Role::Any, AlgalonHidingNow, 0,
                {AlgalonBigBangHideAction::Name, AlgalonLeaveBlackHoleAction::Name, AlgalonCosmicSmashAction::Name});
    e.Block("algalon big bang spell movers", Role::Any, AlgalonHidingNow,
            Family::Charge | Family::Blink | Family::Disengage);

    // The class "lose aggro" nodes taunt on cooldown whenever someone else holds him, 4 stacks or not.
    // The swap, pickup, constellation and Dark Matter nodes taunt through CastClassTaunt instead.
    e.Block("algalon taunt guard", Role::Tank, AlgalonSwapTankEngaged, Family::Taunt);

    e.OwnTargeting("algalon star team", Role::Dps, AlgalonStarTeamFocused);

    e.Multiplier<AlgalonStarAoeMultiplier>(Family::Spell);
    e.Multiplier<AlgalonTargetGuardMultiplier>(Family::Melee | Family::Spell | Family::PetAttack);

    // Attack would chase the constellation and Reach would walk back to Algalon while the handler leads
    // one to a hole. Its own taunts are AttackActions too, and a swap it can't make phases the holder out.
    e.OwnMovement("algalon kite movement", Role::Tank, AlgalonHandlerKiting, 0,
                  {AlgalonConstellationKiteAction::Name, AlgalonBigBangHideAction::Name, AlgalonCosmicSmashAction::Name,
                   AlgalonLeaveBlackHoleAction::Name, AlgalonTankPickupAction::Name, AlgalonPhasePunchSwapAction::Name,
                   AlgalonConstellationTauntAction::Name, AlgalonDarkMatterTankAction::Name});

    // The generic movers would walk the ranged half off their slots, and the slot node would then fire
    // again next tick and pace them all fight. ReachHeal still walks a healer to someone the rings
    // can't reach.
    e.OwnMovement("algalon control movement", Role::Ranged | Role::Tank, AlgalonFormationHolds,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    e.Tick(AlgalonTickEncounterState);
}
}  // namespace

EncounterDefinition const& UldAlgalonDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_ALGALON, BossStateGate, "algalon", &DefineAlgalon);
    return definition;
}
