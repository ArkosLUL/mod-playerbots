/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "EncounterHelpers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "UldActions_Mimiron.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Mimiron.h"
#include "UldHardMode.h"
#include "UldMultipliers_Mimiron.h"
#include "UldScripts.h"
#include "UldTriggers_Mimiron.h"

using namespace EncounterHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
// The windows where taking the hit ends the bot: Shock Blast is 100000 in a 15 yd circle, the Laser
// Barrage cone one-shots, a Rocket Strike lands on the ranged ring, and Firefighter's fire and Frost
// Bomb both tick people down. Proximity Mines and Bomb Bots are left out on purpose - the mine row
// sits below the whole ladder because eating one is healable, and letting it veto a charge would
// contradict its own ranking.
bool MimironLethalWindowActive(PlayerbotAI* botAI)
{
    if (!GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) && !GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
        !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    MimironShockBlastTrigger shockBlast(botAI);
    if (shockBlast.IsActive())
        return true;

    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return true;

    MimironRocketStrikeTrigger rocketStrike(botAI);
    if (rocketStrike.IsActive())
        return true;

    // Both of these check the hard-mode config first, so they cost nothing on a normal clear.
    MimironDodgeFlamesTrigger dodgeFlames(botAI);
    if (dodgeFlames.IsActive())
        return true;

    MimironFrostBombTrigger frostBomb(botAI);
    return frostBomb.IsActive();
}

// The barrage row owns every bot's position for the cast. It hands the tick back while a bot holds at
// the band edge, and melee have no slot in phase 4, so the unstacker would walk them back into the
// band. Otherwise only while the bot stands on its slot, the window the formation declines to act in.
bool MimironFormationHeld(PlayerbotAI* botAI)
{
    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return true;

    Player* bot = botAI->GetBot();
    Position slot;
    return GetMimironSpreadSlot(botAI, bot, slot) &&
           bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) <= ULDUAR_MIMIRON_SPREAD_TOLERANCE;
}

// Only while the fire is there to be mishandled: the hard mode switch is a config read that holds all
// over Ulduar.
bool MimironFireFight(PlayerbotAI* botAI) { return IsMimironHardModeActive(botAI) && IsMimironEngaged(botAI); }

// Empty off hard mode, so a normal clear eats and drinks as before.
bool MimironNearFire(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);
    for (Position const& node : hazards.flames)
        if (bot->GetExactDist2d(node.GetPositionX(), node.GetPositionY()) < ULDUAR_MIMIRON_DRINK_FIRE_CLEARANCE)
            return true;

    return false;
}

// Room test before the grid scan: the hard mode switch holds all over Ulduar.
bool MimironNearFrostBomb(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsMimironHardModeActive(botAI) || !IsNearMimironRoom(bot))
        return false;

    float const hold = ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE + ULDUAR_MIMIRON_FROST_BOMB_HOLD_MARGIN;
    Creature* bomb = bot->FindNearestCreature(NPC_FROST_BOMB, hold + 5.0f);
    return bomb && bot->GetExactDist2d(bomb) < hold;
}

// Water Spray is instant, a 15 yd line, and 23000 to 26000 against a 22000 to 24000 pool, so nothing
// may walk a bot back into one: two died to a spray 3.5 s after a "reach melee" leg put them in it.
bool MimironFireHold(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsMimironHardModeActive(botAI) || !IsNearMimironRoom(bot))
        return false;

    return IsMimironFireHoldActive(bot) ||
           IsMimironSpotInFireBotSpray(GetMimironFirefighterHazards(botAI), bot->GetPosition(),
                                       ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH);
}

bool MimironResistancePaladin(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return bot->getClass() == CLASS_PALADIN && GetMimironFrostResistancePaladin(botAI, bot) == bot;
}

bool MimironRaidDropping(PlayerbotAI* botAI)
{
    return IsMimironPhase2(botAI) && botAI->GetAiObjectContext()->GetValue<uint8>("aoe heal", "low")->Get() <
                                         ULDUAR_MIMIRON_STORM_COOLDOWN_LOW_COUNT;
}

bool MimironMeleeSector(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (PlayerbotAI::IsRanged(bot))
        return false;

    if (IsMimironHardModeActive(botAI) && (IsMimironPhase2(botAI) || IsMimironPhase4(bot)))
        return true;

    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    return barrage.IsActive();
}

bool MimironPhase4MainTank(PlayerbotAI* botAI) { return IsMimironPhase4(botAI->GetBot()); }

void DefineMimiron(EncounterBuilder& e)
{
    // Laser Barrage outranks everything else here: its cone one-shots. Ranked below it by
    // what a hit actually costs - Shock Blast is 100000 damage in a 15 yd circle, a Proximity Mine
    // is 3 yd and healable, which is why the mine dodge sits under the whole rest of the ladder.
    //
    // No Rapid Burst node. A bot steps 1.1 to 1.8 s into the 3 s cone and still takes a hit after
    // it, so the step saved ~0.3 ticks and cost a walk back to the slot. Destinations are screened
    // against the live cone instead, so nothing walks into one.
    e.Node<MimironP3Wx2LaserBarrageTrigger, MimironP3Wx2LaserBarrageAction>(ACTION_RAID + 7);
    e.Node<MimironShockBlastTrigger, MimironShockBlastAction>(ACTION_RAID + 5.5f);
    e.Node(
        "mimiron fire resistance trigger",
        [](PlayerbotAI* ai) -> Trigger* { return new BossFireResistanceTrigger(ai, "mimiron"); },
        "mimiron fire resistance action",
        [](PlayerbotAI* ai) -> Action* { return new BossFireResistanceAction(ai, "mimiron"); }, ACTION_RAID);
    e.Node<MimironFrostResistanceTrigger, MimironFrostResistanceAction>(ACTION_RAID);
    e.Node<MimironPhase1PositioningTrigger, MimironPhase1PositioningAction>(ACTION_RAID);
    e.Node<MimironArcSpreadTrigger, MimironArcSpreadAction>(ACTION_RAID);
    e.Node<MimironAerialCommandUnitTrigger, MimironAerialCommandUnitAction>(ACTION_RAID);
    e.Node<MimironRocketStrikeTrigger, MimironRocketStrikeAction>(ACTION_RAID + 5);
    e.Node<MimironPhase4FocusTrigger, MimironPhase4FocusAction>(ACTION_RAID);
    e.Node<MimironMagneticCoreTrigger, MimironMagneticCoreAction>(ACTION_RAID + 1);
    e.Node<MimironPlasmaBlastDefensiveTrigger, MimironPlasmaBlastDefensiveAction>(ACTION_RAID + 6);
    e.Node<MimironRedirectThreatTrigger, MimironRedirectThreatAction>(ACTION_RAID + 1);
    e.Node<MimironSetDpsPriorityTrigger, MimironSetDpsPriorityAction>(ACTION_RAID);
    // Last of the Mimiron nodes and tied with none of them, so it only ever takes a tick no other
    // mechanic wants. A mine can never cost the raid a dodge, a taunt or a core delivery.
    e.Node<MimironProximityMineTrigger, MimironProximityMineAction>(ACTION_RAID - 1);
    e.Node<MimironBombBotTrigger, MimironBombBotAction>(ACTION_RAID + 2);
    e.Node<MimironSlowBombBotTrigger, MimironSlowBombBotAction>(ACTION_RAID + 2);
    // The action always returns false, so this only ever redirects the pet - the bot keeps its tick.
    e.Node<MimironPetControlTrigger, MimironPetControlAction>(ACTION_RAID);
    // Hard mode (config-gated): step out of the persistent ground fire and clear the Frost Bomb.
    //
    // The bomb outranks the fire, and the gap between them is the point. Both used to sit on
    // ACTION_RAID + 4 alongside the rocket strike, the queue breaks an exact tie by push order, and
    // the engine stops the tick at the first action that returns true - so the fire step, which wins
    // that tie, ended the tick 143 to 221 times a pull and the bomb node was reached 6 to 13. Fire is
    // 3000 a second and healable; the explosion is 47000 in 30 yd against a 24000 health pool.
    e.Node<MimironDodgeFlamesTrigger, MimironDodgeFlamesAction>(ACTION_RAID + 4);
    // Below every Mimiron node and above the generic reach nodes at ACTION_HIGH, which is the whole
    // point: the formation and the dodges answer first, and only a bot they left with nothing to do
    // but walk at its target gets steered round the fire instead of into it.
    e.Node<MimironApproachTargetTrigger, MimironApproachTargetAction>(ACTION_RAID - 2);
    e.Node<MimironFrostBombTrigger, MimironFrostBombAction>(ACTION_RAID + 6);
    // Below the fire dodge, whose fan already refuses the spray line and the silence. A fire bot only
    // sprays when it reaches a flame; the fire ticks every second.
    e.Node<MimironFireBotTrigger, MimironFireBotAction>(ACTION_RAID + 3);
    e.Node<MimironResetEncounterStateTrigger, MimironResetEncounterStateAction>(ACTION_RAID);

    // The set dps priority row owns every non-tank's target, so the generic picker would drag bots back
    // onto whatever is nearest each tick. Engaged, not merely present: that row doesn't run before the
    // pull. "attack rti target" is left alone on purpose: a mark a player sets should still win.
    e.OwnTargeting("mimiron target guard", Role::NonTank, IsMimironEngaged, Family::DpsAssist);

    // Phase 4 focus row picks the main tank's target. VX-001 never takes a victim, so tank assist
    // reads it as loose and pulls him off the MK II, twice a second, and his rotation never runs.
    e.OwnTargeting("mimiron phase 4 tank target guard", Role::MainTank, MimironPhase4MainTank);

    // A gap-closer moves the bot in a straight line and reads nothing about the ground, so it mustn't
    // fire while the bot is dodging something that kills. "reach melee" is the same move without the
    // spell: a melee bot thrown 12 yd clear of the fire was walked back into it on the next tick. The
    // other reach actions stay, or ranged and healers are stranded when they're needed most.
    e.Block("mimiron charge guard", Role::Any, MimironLethalWindowActive, Family::Charge, {"reach melee"});

    // The generic avoid aoe flees to the fire aura's own 3 yd radius, which in a field growing in 7 yd
    // steps lands on the next node, takes the movement lock and returns false, so the Mimiron dodge is
    // issued into a held lock. Measured: 642 evaluations, zero wins, seven in ten dodges refused.
    e.Block("mimiron avoid aoe guard", Role::Any, MimironFireFight, 0, {"avoid aoe"});

    // The unstacker and the formation both move the same ranged bot, and on Firefighter they disagreed
    // by construction: the wedge deals rows 6 yd apart and the unstack threshold was the same 6, so
    // every bot on its slot got shoved off it. Tank face derives from the formation move and keeps the
    // tank pointed away from the raid, so it isn't in this family.
    e.Block("mimiron formation guard", Role::Any, MimironFormationHeld, Family::CombatFormationMove);

    // The rogue's own Tricks picker hands the buff to the hardest-hitting melee in the opener, which
    // then pulls the MK II off the tank 2 to 3 s later. The redirect threat row owns it instead.
    e.Block("mimiron generic redirect guard", Role::Any, MimironPhase1Active, 0, {"tricks of the trade"});

    e.Multiplier<MimironTankAnchorGuardMultiplier>(Family::TankFace | Family::Reach);

    // Eating or drinking stops the bot thinking for up to 18 s, so one that sits down near the fire
    // doesn't dodge it. Clean ground only: healers gained 30 to 40% mana across one handover.
    e.Block("mimiron drink guard", Role::Any, MimironNearFire, 0, {"drink", "food"});

    // Every mover that picks its destination from a unit rather than from the floor. The Frost Bomb flee
    // stops at its clearance and lets go just inside it, so any of these walks the bot back into the
    // blast for the whole 10 s fuse. The fire hold covers a window after the dodge rather than the fire
    // being near, which is all of phase 3 and would strand melee and healers.
    e.Block("mimiron frost bomb guard", Role::Any, MimironNearFrostBomb, 0,
            {"reach melee", "reach spell", "reach party member to heal", "set behind", "follow",
             MimironApproachTargetAction::Name});
    e.Block("mimiron fire hold guard", Role::Any, MimironFireHold, 0,
            {"reach melee", "reach spell", "reach party member to heal", "set behind", "follow",
             MimironApproachTargetAction::Name});

    // Whether an action splashes is its spell, and the totem and trap summoners are named.
    e.Multiplier<MimironFireBotAoeGuardMultiplier>(Family::AnyAction);
    e.Multiplier<MimironPlasmaDefensiveHoldMultiplier>(Family::AnyAction);

    // Holds the paladin aura slot open for the phase 3 Frost Resistance Aura, including the shared fire
    // row, which would hand this bot "rfire" after its own pick dies. Lifts with phase 3.
    e.Block("mimiron paladin aura", Role::Any, MimironResistancePaladin, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "shadow resistance aura", "fire resistance aura", "mimiron fire resistance action"});

    // Their trigger fires on the walk-in Rapid Burst, which healers top off anyway, and then all three
    // are down for the stretch after the first barrage where the pulls die. Holding Divine Sacrifice
    // holds its cancel too.
    e.Block("mimiron storm cooldown hold", Role::Any, MimironRaidDropping, 0,
            {"divine sacrifice", "divine hymn", "power infusion"});

    // Under Firefighter melee hold the sector opposite the ranged wedge in phases 2 and 4. VX-001 turns
    // to face each Rapid Burst and Hand Pulse target, so "behind" moves every couple of seconds and
    // walks the melee off their sector. Any mode during a barrage too: the dodge hands the tick back
    // at the band edge, and set behind walked a melee straight back into the beams.
    e.Block("mimiron vx001 facing guard", Role::Any, MimironMeleeSector, Family::TankFace, {"set behind"});

    e.Tick(MimironTick);
}
}  // namespace

EncounterDefinition const& UldMimironDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_MIMIRON, BossStateGate, "mimiron", &DefineMimiron);
    return definition;
}
