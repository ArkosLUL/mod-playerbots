#ifndef PLAYERBOTS_ULDACTIONS_FREYA_H
#define PLAYERBOTS_ULDACTIONS_FREYA_H

#include <vector>

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
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

// Gather the ranged half and the healers on the anchor bot so the lashers pile onto one spot the raid
// can AoE. The lashers do the walking - a player cannot outrun one, let alone lead one.
class FreyaRangedCampAction : public MovementAction
{
public:
    FreyaRangedCampAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya ranged camp") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Leave the pack once it is down to the finish, so the blasts land behind the bot one at a time.
class FreyaLasherPackStepOutAction : public MovementAction
{
public:
    FreyaLasherPackStepOutAction(PlayerbotAI* botAI) : MovementAction(botAI, "freya lasher pack step out") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Root the pack for the finish. Runs above the step-out so the mage novas first and leaves second.
class FreyaFrostNovaLashersAction : public Action
{
public:
    FreyaFrostNovaLashersAction(PlayerbotAI* botAI) : Action(botAI, "freya frost nova lashers") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Snare the pack for the finish. Below the step-out on purpose: the hunter has already moved by the
// time this fires, so the patch lands on the lane between the pack and the raid.
class FreyaTrapLashersAction : public Action
{
public:
    FreyaTrapLashersAction(PlayerbotAI* botAI) : Action(botAI, "freya trap lashers") {}
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

#endif
