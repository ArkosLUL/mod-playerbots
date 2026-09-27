/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "PlayerbotAI.h"
#include "Strategy.h"
#include "UldActions_Ignis.h"
#include "UldData.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Ignis.h"
#include "UldMultipliers_Ignis.h"
#include "UldTriggers_Ignis.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool InUlduar(PlayerbotAI* botAI) { return botAI->GetBot()->GetMapId() == ULDUAR_MAP_ID; }

// The tanks whose spot the encounter owns right now. A construct tank counts only while it is holding
// one: between constructs it has no spot of its own, and these rules are what would otherwise leave it
// standing still with every generic mover taken off it.
bool IgnisPlacedTank(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return botAI->IsMainTank(bot) || GetIgnisHeldConstruct(botAI, bot);
}

// GetIgnis walks the grid, so it goes last.
bool IgnisSlagPotRide(PlayerbotAI* botAI)
{
    return InUlduar(botAI) && IsIgnisSlagPotVictim(botAI->GetBot()) && GetIgnis(botAI);
}

bool IgnisParkedTank(PlayerbotAI* botAI) { return InUlduar(botAI) && IgnisPlacedTank(botAI) && GetIgnis(botAI); }

bool IgnisTankHeld(PlayerbotAI* botAI) { return InUlduar(botAI) && IgnisPlacedTank(botAI) && IsIgnisEngaged(botAI); }

bool IgnisOwnsTargets(PlayerbotAI* botAI) { return InUlduar(botAI) && IsIgnisEngaged(botAI); }

void DefineIgnis(EncounterBuilder& e)
{
    e.Node(
        "ignis fire resistance trigger",
        [](PlayerbotAI* ai) -> Trigger* { return new BossFireResistanceTrigger(ai, "ignis the furnace master"); },
        "ignis fire resistance action",
        [](PlayerbotAI* ai) -> Action* { return new BossFireResistanceAction(ai, "ignis the furnace master"); },
        ACTION_RAID);
    e.Node<IgnisScorchedGroundTrigger, IgnisScorchedGroundAction>(ACTION_RAID + 2);

    // Where the main tank stands is where every fire patch lands, so his spot outranks everything
    // else the raid does. The construct tanks share the band because no bot is ever both.
    //
    // An Iron Construct only dies to the Molten -> Brittle -> Shatter chain, and every one left alive
    // is another stack of Strength of the Creator on the boss, so the kite outranks the raid's damage.
    // Standing next to a Molten construct and sitting in a Slag Pot both kill a bot outright.
    e.Node<IgnisMainTankPositionTrigger, IgnisMainTankPositionAction>(ACTION_RAID + 4);
    e.Node<IgnisConstructTankTrigger, IgnisConstructTankAction>(ACTION_RAID + 4);
    e.Node<IgnisAttackBrittleConstructTrigger, IgnisAttackBrittleConstructAction>(ACTION_RAID + 3);
    e.Node<IgnisAttackBossTrigger, IgnisAttackBossAction>(ACTION_RAID + 0.5f);
    e.Node<IgnisMoltenConstructAvoidTrigger, IgnisMoltenConstructAvoidAction>(ACTION_EMERGENCY);
    e.Node<IgnisSlagPotHealTrigger, IgnisSlagPotHealAction>(ACTION_EMERGENCY + 1);
    e.Node<IgnisFlameJetsTrigger, IgnisFlameJetsHoldCastAction>(ACTION_EMERGENCY + 2);

    // Slag Pot is a vehicle ride: the victim is held in place for the full duration, so movement
    // orders only fight the ride and leave the bot facing the wrong way when it drops.
    e.Exclusive("ignis", Role::Any, IgnisSlagPotRide);

    // The construct tanks are parked on a Scorched Ground patch on purpose - that's what stacks Heat on
    // the construct - so the dodge would undo the kite every tick. The main tank's arc rotation already
    // steps him clear of every patch he drops, and a dodge on top of it would drag Ignis across the room.
    //
    // Both dodges, not just this encounter's: the generic one runs at ACTION_EMERGENCY, above the band
    // the kite sits in, so leaving it on outranks the walk on nearly every tick spent in the fire.
    e.Block("ignis", Role::Any, IgnisParkedTank, Family::AvoidAoe, {IgnisScorchedGroundAction::Name});

    // Only the three tanks the encounter places. Everyone else keeps every generic mover, which is
    // also what keeps the ranged half spread and in range without an anchor of their own. Scoped on
    // purpose: blanket suppression would leave the whole raid standing still if a replacement mover
    // ever failed quietly.
    e.Block("ignis tank movement", Role::Any, IgnisTankHeld,
            Family::Reach | Family::ReachHeal | Family::Charge | Family::Follow | Family::Flee);

    // The two attack rows own every target for the whole fight. Left on, the generic pickers re-pick
    // the lowest-lifetime add on any tick the settled attack returns false, which here means walking
    // off the Brittle construct and into the shatter blast.
    e.OwnTargeting("ignis disable default targeting", Role::Any, IgnisOwnsTargets,
                   Family::DpsAssist | Family::TankAssist | Family::AttackRti);

    e.Multiplier<IgnisFlameJetsHoldCastMultiplier>(Family::Spell);
}
}  // namespace

EncounterDefinition const& UldIgnisDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_IGNIS, BossStateGate, "ignis", &DefineIgnis);
    return definition;
}
