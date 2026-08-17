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
