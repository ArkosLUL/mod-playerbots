#ifndef PLAYERBOTS_ULDACTIONS_MIMIRON_H
#define PLAYERBOTS_ULDACTIONS_MIMIRON_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Mimiron.h"
#include "UldTriggers.h"
#include "Vehicle.h"

// Shared by the Mimiron flee actions. Proximity Mines are non-selectable, so pathing knows nothing
// about them and a bot stepping out of one hazard can land in the ten that follow a Shock Blast.
class MimironFleeAction : public MovementAction
{
public:
    MimironFleeAction(PlayerbotAI* ai, std::string const name) : MovementAction(ai, name) {}

protected:
    // fallbackUnfiltered takes the plain MoveAway fan when every mine-clear bearing was refused. Set
    // it for hazards that hurt more than a mine; the mine dodge itself passes false, because
    // escaping one mine into another is not an escape.
    // interrupt cancels an in-flight cast first. Set it for anything that kills outright: a bot
    // that IsMovementPreventedByCasting cannot be moved at all, so the dodge is a no-op without it.
    // what names the hazard for the trace, because a refused bearing writes no movement record and a
    // flee that finds none is otherwise a silent gap.
    bool MoveAwayClearOfMines(Unit* from, float distance,
                              MovementPriority priority = MovementPriority::MOVEMENT_COMBAT,
                              bool fallbackUnfiltered = true, bool interrupt = false,
                              char const* what = "flee");

private:
    void NoteFleeOutcome(char const* what, char const* outcome, float const* taken, uint32 refusedBack,
                         uint32 refusedMine, uint32 refusedCone);
};

class MimironShockBlastAction : public MimironFleeAction
{
public:
    MimironShockBlastAction(PlayerbotAI* ai) : MimironFleeAction(ai, "mimiron shock blast action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironPhase1PositioningAction : public MovementAction
{
public:
    MimironPhase1PositioningAction(PlayerbotAI* ai) : MovementAction(ai, "mimiron phase 1 positioning action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironP3Wx2LaserBarrageAction : public MovementAction
{
public:
    MimironP3Wx2LaserBarrageAction(PlayerbotAI* ai)
        : MovementAction(ai, "mimiron p3wx2 laser barrage action") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    void NoteBarrageDecision(char const* branch, char const* direction, float cw);
};

// Rapid Burst and Hand Pulse are both 104 degree cones, so no arrangement dodges them; what helps is
// occupying more bearings than one cone covers. The six spots this replaced stacked the raid into
// three clumps, which is the worst possible shape for that.
class MimironArcSpreadAction : public MovementAction
{
public:
    MimironArcSpreadAction(PlayerbotAI* ai) : MovementAction(ai, "mimiron arc spread action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironAerialCommandUnitAction : public Action
{
public:
    MimironAerialCommandUnitAction(PlayerbotAI* ai) : Action(ai, "mimiron aerial command unit action") {}

    bool Execute(Event event) override;
};

class MimironRocketStrikeAction : public MimironFleeAction
{
public:
    MimironRocketStrikeAction(PlayerbotAI* ai) : MimironFleeAction(ai, "mimiron rocket strike action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironPhase4FocusAction : public AttackAction
{
public:
    MimironPhase4FocusAction(PlayerbotAI* ai) : AttackAction(ai, "mimiron phase 4 focus action") {}

    bool Execute(Event event) override;
};

// One designated bot carries the Magnetic Core to the Aerial Command Unit and grounds it. Without
// this the phase only ends when ranged whittle the ACU down from the floor.
class MimironMagneticCoreAction : public MovementAction
{
public:
    MimironMagneticCoreAction(PlayerbotAI* ai) : MovementAction(ai, "mimiron magnetic core action") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    void NoteCoreStep(char const* step);
};

// Plasma Blast is a 3s cast on whoever is holding the MK II, every 22s, and it does not stack. The
// two tanks alternate on it and never taunt back: one taunt each per cycle is 22s apart, clear of
// the 15s taunt-DR reset, whereas swapping back would put two taunts 11s apart and cut the next
// one's duration to 65%.
class MimironPlasmaBlastAction : public AttackAction
{
public:
    MimironPlasmaBlastAction(PlayerbotAI* ai) : AttackAction(ai, "mimiron plasma blast action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Owns "current target" for every non-tank while any mech is up. Raid icons stay cosmetic here: they
// are group-global, and reading one back is what made the old rti round-trip fail whenever a bot had
// a different "rti" string from the one the marking bot wrote.
class MimironSetDpsPriorityAction : public AttackAction
{
public:
    MimironSetDpsPriorityAction(PlayerbotAI* botAI)
        : AttackAction(botAI, "mimiron set dps priority action") {}

    bool Execute(Event event) override;

private:
    // Ordered candidates for this bot's role, most urgent first. Entries the role must not touch are
    // left out entirely rather than filtered later.
    std::vector<std::pair<uint32, Unit*>> BuildPriorityList();

    // Nearest live candidate of `entry`, preferring the current target so two identical adds cannot
    // make the bot alternate between them every tick.
    Unit* SelectByEntry(Unit* currentTarget, uint32 entry, std::vector<Unit*> const& candidates) const;

    bool IsAllowedTarget(Unit* unit) const;

    Unit* ResolveTarget(Unit* currentTarget);

    static char const* DescribeTargetRule(Unit* unit);
};

// Steps the shortest distance that clears every mine, rather than the safest point in the room. The
// generic MoveAwayFromCreatureAction maximises min-distance-to-any-mine over a fan reaching 30 yd,
// which in a fresh ten-mine field means a long run to a different answer every tick.
class MimironProximityMineAction : public MimironFleeAction
{
public:
    MimironProximityMineAction(PlayerbotAI* ai)
        : MimironFleeAction(ai, "mimiron proximity mine action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironBombBotAction : public MoveAwayFromCreatureAction
{
public:
    MimironBombBotAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "mimiron bomb bot action", NPC_BOMB_BOT,
                                     ULDUAR_MIMIRON_BOMB_BOT_RADIUS) {}
};

// Where a pet goes in phases 3 and 4: the adds while the Aerial Command Unit hovers out of reach, the
// unit itself while a Magnetic Core has it down, and whatever the melee are on in phase 4.
class MimironPetControlAction : public Action
{
public:
    MimironPetControlAction(PlayerbotAI* ai) : Action(ai, "mimiron pet control action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode (Firefighter): step out of the persistent ground fire before it burns the bot down.
class MimironDodgeFlamesAction : public MovementAction
{
public:
    MimironDodgeFlamesAction(PlayerbotAI* ai) : MovementAction(ai, "mimiron dodge flames action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode (Firefighter): clear VX-001's Frost Bomb radius before it detonates.
class MimironFrostBombAction : public MoveAwayFromCreatureAction
{
public:
    MimironFrostBombAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "mimiron frost bomb action", NPC_FROST_BOMB,
                                     ULDUAR_MIMIRON_FROST_BOMB_RADIUS) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Bomb Bots are the one Mimiron add that takes a snare - immunity set -263 leaves SNARE, ROOT, STUN,
// FREEZE, GRIP and KNOCKOUT off, all of which the Assault Bot's -285 carries. 20000 HP at 8.0 yd/s,
// so every second of extra approach is most of a cast.
class MimironSlowBombBotAction : public Action
{
public:
    MimironSlowBombBotAction(PlayerbotAI* ai) : Action(ai, "mimiron slow bomb bot action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
