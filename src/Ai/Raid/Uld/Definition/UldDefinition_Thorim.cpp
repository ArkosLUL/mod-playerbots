/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "BossResistanceMultipliers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "UldActions_Thorim.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Thorim.h"
#include "UldMultipliers_Thorim.h"
#include "UldTriggers_Thorim.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
// The generic picker families. Picking a target isn't walking anywhere, and the encounter's own picker
// row is the one that drops a bad target, so no guard here may take these away.
constexpr RaidEncounterRules::FamilyMask PICKERS = Family::DpsAssist | Family::DpsAoe | Family::TankAssist |
                                                   Family::AggressiveTarget | Family::AttackAnything |
                                                   Family::AttackLeastHp;

constexpr RaidEncounterRules::FamilyMask DPS_PICKERS =
    Family::DpsAssist | Family::DpsAoe | Family::AggressiveTarget | Family::AttackAnything | Family::AttackLeastHp;

bool ThorimLeashBreached(PlayerbotAI* botAI) { return ThorimArenaLeashBreached(botAI, botAI->GetBot()); }

bool ThorimArenaSettled(PlayerbotAI* botAI) { return ThorimArenaAnchorSettled(botAI, botAI->GetBot()); }

bool ThorimRingSettled(PlayerbotAI* botAI) { return ThorimMeleeRingSettled(botAI, botAI->GetBot()); }

// A healer's target drives its wand and its offensive dispels, so it keeps the generic pickers.
bool ThorimPickingFor(PlayerbotAI* botAI)
{
    return botAI->GetState() == BOT_STATE_COMBAT && !botAI->IsHeal(botAI->GetBot());
}

// In phase 2 the pickup row owns the tank's target and there is one thing left alive worth hitting.
// Letting "tank assist" keep voting held a tank on a Warbringer for the first 30s of the phase while
// Thorim ate the ranged camp. Outside it "tank assist" holds whatever is swinging at the raid, which
// beats steering a tank onto the ranged pick.
bool ThorimTankPicked(PlayerbotAI* botAI) { return ThorimPickingFor(botAI) && ThorimPhase2Active(botAI); }

// Inside the encounter the picker row is the only target source, so nothing from it means nothing
// legal to hit and standing still is the right answer. Releasing the generic pickers instead has bots
// latch onto Thorim between add waves: he's untouchable on the balcony, the picker row drops him, and
// the generic picker hands him straight back next tick.
bool ThorimDpsPicked(PlayerbotAI* botAI)
{
    return ThorimPickingFor(botAI) && (ThorimHasDpsTarget(botAI, botAI->GetBot()) || ThorimSplitActive(botAI));
}

bool ThorimPhase2DpsPicked(PlayerbotAI* botAI) { return ThorimPhase2Active(botAI) && ThorimDpsPicked(botAI); }

// Another tank has him, human or bot, and he's this bot's target. Adds are still fair game, so
// taunting one off a healer still works. The pickup row's own taunts go out through
// DoSpecificAction, which never sees a multiplier.
bool ThorimTauntWouldSteal(PlayerbotAI* botAI)
{
    if (!ThorimPhase2Active(botAI))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() != boss)
        return false;

    Unit* victim = boss->GetVictim();
    if (!victim || victim == botAI->GetBot())
        return false;

    Player* holder = victim->ToPlayer();
    return holder && PlayerbotAI::IsTank(holder);
}

// Above the floor line, in the corridor half, and only while the walk has somewhere to go. The instant
// the hallway isn't the job any more this lets go, so nothing can leave a bot up there with no mover.
bool ThorimBalconyWalk(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD ||
        GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    ThorimBalconyAdvanceTrigger balconyAdvance(botAI);
    return balconyAdvance.IsActive();
}

void TickThorim(PlayerbotAI* botAI) { ThorimTickBarrierBail(botAI, botAI->GetBot()); }

void DefineThorim(EncounterBuilder& e)
{
    e.Node(
        "thorim nature resistance trigger",
        [](PlayerbotAI* ai) -> Trigger* { return new BossNatureResistanceTrigger(ai, "thorim"); },
        "thorim nature resistance action",
        [](PlayerbotAI* ai) -> Action* { return new BossNatureResistanceAction(ai, "thorim"); }, ACTION_RAID);
    e.Node(
        "thorim frost resistance trigger",
        [](PlayerbotAI* ai) -> Trigger* { return new BossFrostResistanceTrigger(ai, "thorim"); },
        "thorim frost resistance action",
        [](PlayerbotAI* ai) -> Action* { return new BossFrostResistanceAction(ai, "thorim"); }, ACTION_RAID);
    e.Node<ThorimUnbalancingStrikeSwapTrigger, ThorimUnbalancingStrikeSwapAction>(ACTION_RAID + 2);
    e.Node<ThorimTankPickupTrigger, ThorimTankPickupAction>(ACTION_RAID + 2);
    e.Node<ThorimDpsPriorityTrigger, ThorimDpsPriorityAction>(ACTION_RAID);

    // Recalls a pet, not the bot, so it competes with nothing and sits at the base rank.
    e.Node<ThorimPetLeashTrigger, ThorimPetLeashAction>(ACTION_RAID);

    e.Node<ThorimGauntletPositioningTrigger, ThorimGauntletPositioningAction>(ACTION_RAID);

    // Above the corridor walk, because on the balcony that one has no waypoint to offer and the
    // hallway is where the two Paralytic Field bunnies are.
    e.Node<ThorimBalconyAdvanceTrigger, ThorimBalconyAdvanceAction>(ACTION_RAID + 2);

    e.Node<ThorimArenaPositioningTrigger, ThorimArenaPositioningAction>(ACTION_RAID);
    e.Node<ThorimFallFromFloorTrigger, ThorimFallFromFloorAction>(ACTION_RAID + 1);
    e.Node<ThorimPhase2PositioningTrigger, ThorimPhase2PositioningAction>(ACTION_RAID);
    e.Node<ThorimSifBlizzardTrigger, ThorimSifBlizzardAction>(ACTION_RAID + 3);

    // Charge Orb: it only ever fires while Thorim is still on the balcony. 3k a second for 15s across a
    // 32 yd circle is phase 1's largest avoidable damage source, so it has to beat the ring and the add
    // chase both. Lightning Charge has no node of its own - the cone answer is baked into the phase 2
    // spot, and a second mover for the same point only fought the first one at the movement gate.
    e.Node<ThorimChargedOrbTrigger, ThorimChargedOrbAction>(ACTION_RAID + 4);

    e.Node<ThorimRunicSmashTrigger, ThorimRunicSmashAction>(ACTION_RAID + 3);
    e.Node<ThorimRunicBarrierBailTrigger, ThorimRunicBarrierBailAction>(ACTION_RAID + 2);

    // Top of the Thorim ladder. An arena squad member outside the box is not a positioning problem:
    // one 5 second scan finding nobody in there summons the Lightning Orb and kills the raid.
    e.Node<ThorimArenaLeashTrigger, ThorimArenaLeashAction>(ACTION_RAID + 5);

    e.Node<ThorimResetEncounterStateTrigger, ThorimResetEncounterStateAction>(ACTION_RAID);

    e.Multiplier<ThorimRunicBarrierMultiplier>(Family::Melee | Family::Reach | Family::ReachHeal);

    // The picker row picks every non-tank target in phase 1. Without this the generic pickers don't
    // lose, they alternate: the row goes quiet the moment the bot holds its pick, the engine falls
    // through to "dps assist" at 50, and that retargets. A traced arena squad switched target twice a
    // second all fight.
    e.OwnTargeting("thorim disable automatic targeting", Role::Tank, ThorimTankPicked, DPS_PICKERS | Family::TankAssist);
    e.OwnTargeting("thorim disable automatic targeting", Role::NonTank, ThorimDpsPicked, DPS_PICKERS);
    e.OwnTargeting("thorim disable automatic targeting", Role::NonTank, ThorimPhase2DpsPicked, Family::TankAssist);

    // Every melee DPS gets the "behind" strategy from AiFactory, so once the phase 2 ring row yields,
    // SetBehindTargetAction walks all three stacks back into one arc behind the boss - and Chain
    // Lightning jumps 5 yd here, so one arc is one chain. Scoped to a settled ring holder rather than the
    // whole phase, because a permanent movement freeze is the Void Reaver failure. The balcony walk is
    // listed because it's the one mover a bot that hasn't made it down yet has left.
    e.OwnMovement("thorim movement guard", Role::Any, ThorimRingSettled,
                  Family::Attack | Family::Reach | Family::ReachHeal | Family::AvoidAoe,
                  {ThorimPhase2PositioningAction::Name, ThorimBalconyAdvanceAction::Name, ThorimSifBlizzardAction::Name});

    // The arena squad has to keep one living body inside the box boss_thorim.cpp scans. Unlike the
    // other guards this one keeps no attacks and no reach: corridor mobs sit ~92 yd from the arena
    // centre, inside the 100 yd sight cap, so the chase is exactly what walks a bot out. The window is
    // "currently outside" and closes on re-entry, so it can't become a permanent freeze.
    e.Exclusive("thorim arena leash", Role::Any, ThorimLeashBreached, Family::AvoidAoe | PICKERS,
                {ThorimArenaLeashAction::Name, ThorimArenaPositioningAction::Name, ThorimChargedOrbAction::Name,
                 ThorimSifBlizzardAction::Name, ThorimDpsPriorityAction::Name});

    e.Multiplier<ThorimArenaTargetGuardMultiplier>(Family::Attack | Family::Reach | Family::ReachHeal);

    // The phase 1 arena formation, once a bot has reached its spot. The generic movers smear the squad
    // east across the arena until it stands in the corridor mouth, where the adds stop seeing it and
    // re-roll onto a healer. Keeps the chase: an add can land 38 yd from an outer ring slot, and a
    // ranged bot that can't step into range is silent.
    e.OwnMovement("thorim arena anchor guard", Role::Any, ThorimArenaSettled,
                  Family::Attack | Family::Reach | Family::ReachHeal | Family::AvoidAoe,
                  {ThorimArenaPositioningAction::Name, ThorimArenaLeashAction::Name, ThorimChargedOrbAction::Name,
                   ThorimSifBlizzardAction::Name});

    // Up on the balcony, "follow" walks the corridor squad the only walkable way down to the master,
    // 300 yd back through the corridor. Rune Detonation lands up there, so the dodge stays.
    e.OwnMovement("thorim balcony guard", Role::Any, ThorimBalconyWalk,
                  Family::Attack | Family::Reach | Family::ReachHeal | Family::AvoidAoe,
                  {ThorimBalconyAdvanceAction::Name});
    e.Multiplier<ThorimBalconyGuardMultiplier>(Family::Attack | Family::Reach | Family::ReachHeal);

    // The class "lose aggro" node taunts on its own cooldown and knows nothing about who is meant to
    // be holding him, so a paladin ripped him off the human tank every 8s and the pair walked him
    // across the arena. The pickup row owns the trade instead.
    e.Block("thorim taunt guard", Role::Any, ThorimTauntWouldSteal, Family::Taunt);

    e.Multiplier<BossNatureAspectHoldMultiplier>(Family::Spell, "thorim");

    e.Tick(TickThorim);
}
}  // namespace

EncounterDefinition const& UldThorimDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_THORIM, BossStateGate, "thorim", &DefineThorim);
    return definition;
}
