#ifndef PLAYERBOTS_ULDACTIONS_FREYA_H
#define PLAYERBOTS_ULDACTIONS_FREYA_H

#include <vector>

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Freya.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class FreyaMoveAwayNatureBombAction : public MovementAction
{
public:
    FreyaMoveAwayNatureBombAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya move away nature bomb") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // The trigger fires at AVOID and the escape aims at CLEAR, so re-deriving every tick answers a bot
    // on the rim with a ~2 yd step that combat movement undoes before the next one. Hold the first
    // answer until it stops being safe or the bot arrives.
    Position bombSpot;
    uint32 bombSpotMs = 0;
};

// Main tank only: walks Freya off the bombs so her melee ring is somewhere the rest of the melee can
// stand. Nothing else in the encounter ever moves the boss.
class FreyaTankNatureBombAction : public MovementAction
{
public:
    FreyaTankNatureBombAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya tank nature bomb") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Position kiteSpot;
    uint32 kiteSpotMs = 0;
};

// Owns every DPS bot's target. Eonar's Gift > Ancient Conservator > the trio slot > Detonating
// Lashers > Freya, with the ordering flipped once the trio is low enough that leaving it would let a
// member revive.
class FreyaSetDpsPriorityAction : public AttackAction
{
public:
    FreyaSetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, "freya set dps priority") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Unit* ResolveFreyaDpsTarget(Unit* currentTarget);

    // Whether this bot is one of the few sent at Eonar's Gift while a lasher pack is up.
    bool TakesEonarsGift(FreyaWaveState const& state);

    // Which Gift the share is timing, and when this bot first saw it.
    ObjectGuid giftGuid;
    uint32 giftSeenMs = 0;
};

// Main tank holds Freya; assist tank 0 works down the add ladder and, on the Conservator, walks it onto
// a Healthy Spore so the melee sheltering there can still reach it.
class FreyaTankAddsAction : public AttackAction
{
public:
    FreyaTankAddsAction(PlayerbotAI* botAI) : AttackAction(botAI, "freya tank adds") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool ParkConservator(Unit* conservator);

    // Latched for the spore's whole life. Fresh spores keep appearing 20 yd from wherever the
    // Conservator currently is, so re-deriving the destination every tick can flip it mid-walk and turn
    // the tank around.
    ObjectGuid parkedSpore;
};

class FreyaMoveToHealingSporeAction : public MovementAction
{
public:
    FreyaMoveToHealingSporeAction(PlayerbotAI* ai) : MovementAction(ai, "freya move to healing spore action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Feeds Misdirection / Tricks to the tank that is actually holding what the raid is hitting. Detonating
// Lashers wipe their threat list every 10s, so nothing here can help against those - this is for the
// Snaplasher and the Conservator, which have real threat tables.
class FreyaRedirectThreatAction : public Action
{
public:
    FreyaRedirectThreatAction(PlayerbotAI* botAI) : Action(botAI, "freya redirect threat") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Player* GetRedirectTank();
};

// Step a melee bot out of the blast of the lashers that are about to detonate. Only those, not the
// whole pack: clearing every lasher would park melee outside the fight, which is what the 16 yd
// lattice did before it was reverted.
class FreyaLasherAboutToBlowAction : public MovementAction
{
public:
    FreyaLasherAboutToBlowAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya lasher about to blow") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // Lashers die one by one, so re-deriving the exit every tick answers with a spot a yard along and
    // clears the motion master doing it - the bot resets its walk instead of finishing it. Hold the
    // first answer until it stops being clear or the bot arrives.
    Position bailSpot;
    uint32 bailSpotMs = 0;
};

// Gather the ranged half and the healers a fixed standoff from the pack so the lashers pile onto one
// spot the raid can AoE. The lashers do the walking - a player cannot outrun one, let alone lead one.
class FreyaRangedCampAction : public MovementAction
{
public:
    FreyaRangedCampAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya ranged camp") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Root whatever has closed on the mage. Above the trap so a hunter standing next to it drops the patch
// under an already-rooted pair rather than under one still moving.
class FreyaFrostNovaLashersAction : public Action
{
public:
    FreyaFrostNovaLashersAction(PlayerbotAI* botAI) : Action(botAI, "freya frost nova lashers") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Snare what is running at this hunter. -50% takes a lasher from 8.0 yd/s to 4.0, under a player's
// 7.0, which is the only thing on the encounter that makes one of them slower than the raid.
class FreyaTrapLashersAction : public Action
{
public:
    FreyaTrapLashersAction(PlayerbotAI* botAI) : Action(botAI, "freya trap lashers") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Eight ghouls with an AoE taunt, held for the wave that has no tank. Not named "army of the dead" on
// purpose: that string is on IsBurstCooldownAction's list, and the Ulduar burst gate holds everything
// on it until Attuned to Nature comes off Freya, which is the whole add phase.
class FreyaSummonArmyAction : public Action
{
public:
    FreyaSummonArmyAction(PlayerbotAI* botAI) : Action(botAI, "freya summon army") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode: stop a cast Ground Tremor is about to eat. Worth more than the mana it saves -
// Spell::EffectInterruptCast only applies the 10s school lockout if it finds a cast to cut, so
// stopping first dodges the lockout outright.
class FreyaGroundTremorHoldCastAction : public Action
{
public:
    FreyaGroundTremorHoldCastAction(PlayerbotAI* botAI) : Action(botAI, "freya ground tremor hold cast") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode: kill the Iron Roots creature trapping the bot - its death removes the root DoT.
class FreyaBreakIronRootsAction : public AttackAction
{
public:
    FreyaBreakIronRootsAction(PlayerbotAI* botAI) : AttackAction(botAI, "freya break iron roots") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode: step out of an Unstable Sun Beam before it detonates.
class FreyaDodgeUnstableSunBeamAction : public MovementAction
{
public:
    FreyaDodgeUnstableSunBeamAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya dodge unstable sun beam") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // Beams keep spawning, so re-deriving the escape every tick answers with a spot a yard or two away,
    // and every MoveTo clears the motion master - the bot never travels, and whatever walk a lower node
    // had in flight dies with it. Hold the first answer until it stops being safe or the bot arrives.
    Position dodgeSpot;
    uint32 dodgeSpotMs = 0;
};

// Carry Nature's Fury out of the raid, preferably onto another Healthy Spore: the groups sheltering on
// them stand 15 to 45 yd apart, which clears an 8 yd splash outright and keeps the carrier out of
// Conservator's Grip.
class FreyaNaturesFuryBailAction : public MovementAction
{
public:
    FreyaNaturesFuryBailAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya nature fury bail") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // Allies drift, so re-deriving every tick answers with a spot a yard along and clears the motion
    // master doing it. Hold the first answer until the raid catches up with it or the bot arrives.
    Position bailSpot;
    uint32 bailSpotMs = 0;
};

// Step out of the circle Freya's Sunbeam is about to drop on a neighbour.
class FreyaStepOutOfSunbeamAction : public MovementAction
{
public:
    FreyaStepOutOfSunbeamAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya step out of sunbeam") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Position stepSpot;
    uint32 stepSpotMs = 0;
};

// Main tank only: walk Freya back to her anchor. She follows whoever holds her, so the tank's own feet
// are the only thing that moves her.
class FreyaTankHoldFreyaAction : public MovementAction
{
public:
    FreyaTankHoldFreyaAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya tank hold freya") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    uint32 holdMs = 0;
};

#endif
