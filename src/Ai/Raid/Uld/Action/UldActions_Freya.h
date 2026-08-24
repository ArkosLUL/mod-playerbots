#ifndef PLAYERBOTS_ULDACTIONS_FREYA_H
#define PLAYERBOTS_ULDACTIONS_FREYA_H

#include <vector>

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class FreyaMoveAwayNatureBombAction : public MovementAction
{
public:
    FreyaMoveAwayNatureBombAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya move away nature bomb") {}
    bool Execute(Event event) override;
    bool isUseful() override;
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

    // Pure lasher wave: park at the corral and taunt whatever wanders off it. True means it owned the
    // tick and the add ladder must not run, or the tank walks back to Freya.
    bool HoldLasherCorral(FreyaWaveState const& state, Unit* currentTarget);

    // Latched for the spore's whole life. Fresh spores keep appearing 20 yd from wherever the
    // Conservator currently is, so re-deriving the destination every tick can flip it mid-walk and turn
    // the tank around.
    ObjectGuid parkedSpore;
};

// Step outside Detonate's blast when the bot is too low to survive it.
class FreyaAvoidDetonatingLasherAction : public MovementAction
{
public:
    FreyaAvoidDetonatingLasherAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya avoid detonating lasher") {}
    bool Execute(Event event) override;
    bool isUseful() override;
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

// Walk a lasher that has picked this bot out to the corral behind Freya. No threat handling: the add
// is faster than the bot and follows on its own until its next 10s retarget.
class FreyaDragLasherToCorralAction : public MovementAction
{
public:
    FreyaDragLasherToCorralAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya drag lasher to corral") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Get out of a lethal pile of lashers. Also the return leg of the drag and the mage's exit after a nova.
class FreyaLasherPackStepOutAction : public MovementAction
{
public:
    FreyaLasherPackStepOutAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya lasher pack step out") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Root the corral. Runs above the step-out so the mage novas first and leaves second.
class FreyaFrostNovaLashersAction : public Action
{
public:
    FreyaFrostNovaLashersAction(PlayerbotAI* botAI) : Action(botAI, "freya frost nova lashers") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Keep a Frost Trap on the lane out of the corral.
class FreyaTrapLasherCorralAction : public MovementAction
{
public:
    FreyaTrapLasherCorralAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya trap lasher corral") {}
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
};

#endif
