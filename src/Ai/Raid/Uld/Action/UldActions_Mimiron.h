#ifndef PLAYERBOTS_ULDACTIONS_MIMIRON_H
#define PLAYERBOTS_ULDACTIONS_MIMIRON_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "RaidRedirectThreat.h"
#include "Playerbots.h"
#include "UldEncounter_Mimiron.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class MimironResetEncounterStateAction : public Action
{
public:
    static constexpr char const* Name = "mimiron reset encounter state action";

    MimironResetEncounterStateAction(PlayerbotAI* ai) : Action(ai, Name) {}

    bool Execute(Event event) override;
};

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
    // clearRadius, when set, replaces the flat distance with the per-bearing hop that lands the
    // destination that far from `from`. One radius swept over a fan only reaches it on the
    // straight-away bearing; every other lands short, which is how an escape sweeps eleven
    // headings and stays inside the circle it was running from.
    // sectorScreen keeps a hard-mode phase 2 bot off the other group's Rapid Burst line
    // (GetMimironOtherGroupSector) while any bearing on its own side is left.
    bool MoveAwayClearOfMines(Unit* from, float distance,
                              MovementPriority priority = MovementPriority::MOVEMENT_COMBAT,
                              bool fallbackUnfiltered = true, bool interrupt = false,
                              char const* what = "flee", float clearRadius = 0.0f,
                              bool sectorScreen = false);

    // Same fan, run away from a point rather than a unit. The ground fire is a field of 50 to 60
    // nodes with no single unit to flee, so the flames dodge hands in its centroid.
    bool MoveAwayClearOfMines(Position const& from, float distance,
                              MovementPriority priority = MovementPriority::MOVEMENT_COMBAT,
                              bool fallbackUnfiltered = true, bool interrupt = false,
                              char const* what = "flee", float clearRadius = 0.0f);

    // Same fan again, aimed at a destination instead of away from a hazard, by fleeing the point
    // mirrored through the bot - so the straight-ahead bearing is the destination itself and the
    // sweep supplies the alternatives.
    // allowFire drops the ground-fire screen. Only for an escape from something that outdamages a
    // node: a node is ~3100 a second, and a fan that refuses all eleven bearings for fire leaves
    // the bot standing in a Water Spray line that one-shots it.
    bool MoveTowardClearOfMines(Position const& dest,
                                MovementPriority priority = MovementPriority::MOVEMENT_COMBAT,
                                bool fallbackUnfiltered = true, bool interrupt = false,
                                char const* what = "step", bool allowFire = false);

private:
    // The fan itself. `fallbackFrom` is the unit the two public overloads were asked about, and is
    // only ever used for the unfiltered MoveAway once every bearing has been refused; the point
    // overload has no unit to hand it, so it walks straight away from `from` instead.
    bool FleeFan(Position const& from, Unit* fallbackFrom, float distance, MovementPriority priority,
                 bool fallbackUnfiltered, bool interrupt, char const* what, float clearRadius = 0.0f,
                 bool allowFire = false, bool sectorScreen = false);

    void NoteFleeOutcome(char const* what, char const* outcome, float const* taken, uint32 refusedBack,
                         uint32 refusedMine, uint32 refusedCone, uint32 refusedFire, uint32 refusedBomb,
                         uint32 refusedBurst, uint32 refusedShock, uint32 refusedSpray,
                         uint32 refusedMove);
};

class MimironShockBlastAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron shock blast action";

    MimironShockBlastAction(PlayerbotAI* ai) : MimironFleeAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool FleeShockToAnchor();
};

class MimironPhase1PositioningAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron phase 1 positioning action";

    MimironPhase1PositioningAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironP3Wx2LaserBarrageAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron p3wx2 laser barrage action";

    MimironP3Wx2LaserBarrageAction(PlayerbotAI* ai)
        : MovementAction(ai, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    void NoteBarrageDecision(char const* branch, char const* direction, float cw);
};

// Where a bot stands when nothing is trying to kill it this second. The six fixed spots this replaced
// stacked the raid into three clumps, which is the worst possible shape against anything conical.
class MimironArcSpreadAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron arc spread action";

    MimironArcSpreadAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironAerialCommandUnitAction : public Action
{
public:
    static constexpr char const* Name = "mimiron aerial command unit action";

    MimironAerialCommandUnitAction(PlayerbotAI* ai) : Action(ai, Name) {}

    bool Execute(Event event) override;
};

class MimironRocketStrikeAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron rocket strike action";

    MimironRocketStrikeAction(PlayerbotAI* ai) : MimironFleeAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironPhase4FocusAction : public AttackAction
{
public:
    static constexpr char const* Name = "mimiron phase 4 focus action";

    MimironPhase4FocusAction(PlayerbotAI* ai) : AttackAction(ai, Name) {}

    bool Execute(Event event) override;
};

// One designated bot carries the Magnetic Core to the Aerial Command Unit and grounds it. Without
// this the phase only ends when ranged whittle the ACU down from the floor.
class MimironMagneticCoreAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron magnetic core action";

    MimironMagneticCoreAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    void NoteCoreStep(char const* step);

    // Set by every spend and cleared once the bags are empty with no core on the floor, so the next
    // core in hand goes down the moment the live landing ends instead of waiting to bank again. The
    // count alone can't tell: one core left after spending the first of a pair reads as a bank of one.
    bool chaining = false;
};

// Raise Frost Resistance Aura for phase 3. Cast directly rather than through ChangeStrategy: nothing
// removes an added "rfrost", so the pick would outlive the phase. The mimiron paladin aura rule holds
// the slot open while it runs.
class MimironFrostResistanceAction : public Action
{
public:
    static constexpr char const* Name = "mimiron frost resistance action";

    MimironFrostResistanceAction(PlayerbotAI* ai) : Action(ai, Name) {}
    bool Execute(Event event) override;
};

// One big defensive on the tank for the Plasma Blast window, spent when the cast starts rather than
// when a health bar has already dropped. Six ticks totalling 54k to 119k land on a 43k pool in about
// five seconds, so a threshold notices it around the third one.
//
// Exactly one button per window, tank or healer and never both: either alone carries it, and the
// spare is worth more on the next window 22 s later. The tank's own cooldown goes first because it
// costs the raid nothing; a healer only steps in when he has none left.
class MimironPlasmaBlastDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "mimiron plasma blast defensive action";

    MimironPlasmaBlastDefensiveAction(PlayerbotAI* ai)
        : Action(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // Whichever of these the healer has, in the order they are worth spending. Hand of Protection is
    // deliberately absent: it sheds threat and would hand the boss straight back to the raid.
    static constexpr char const* HEALER_EXTERNALS[] = {"pain suppression", "guardian spirit",
                                                       "hand of sacrifice"};

    Unit* PlasmaVictim();
    bool TankHasDefensive(Unit* victim);
};

// Phase 1 threat redirect. The MK II has to stay on the main tank through a 53 yd walk west, and the
// rogue's own Tricks picker aims at the hardest-hitting melee during the opener instead - which is
// what pulled the boss off the tank 2 to 3 s in on every measured pull.
class MimironRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "mimiron redirect threat action";

    MimironRedirectThreatAction(PlayerbotAI* ai)
        : RaidRedirectThreatAction(ai, Name) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

// Owns "current target" for every non-tank while any mech is up. Raid icons stay cosmetic here: they
// are group-global, and reading one back is what made the old rti round-trip fail whenever a bot had
// a different "rti" string from the one the marking bot wrote.
class MimironSetDpsPriorityAction : public AttackAction
{
public:
    static constexpr char const* Name = "mimiron set dps priority action";

    MimironSetDpsPriorityAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;

private:
    // Ordered candidates for this bot's role, most urgent first. Entries the role must not touch are
    // left out entirely rather than filtered later.
    std::vector<std::pair<uint32, Unit*>> BuildPriorityList();

    // Nearest live candidate of `entry`, preferring the current target so two identical adds cannot
    // make the bot alternate between them every tick.
    Unit* SelectByEntry(Unit* currentTarget, uint32 entry, std::vector<Unit*> const& candidates) const;

    bool IsAllowedTarget(Unit* unit) const;

    // nullptr when nothing is allowed and `allowFallback` is false; otherwise the generic picker's
    // answer stands in.
    Unit* ResolveTarget(Unit* currentTarget, bool allowFallback);

    static char const* DescribeTargetRule(Unit* unit);
};

// Steps the shortest distance that clears every mine, rather than the safest point in the room. The
// generic MoveAwayFromCreatureAction maximises min-distance-to-any-mine over a fan reaching 30 yd,
// which in a fresh ten-mine field means a long run to a different answer every tick.
class MimironProximityMineAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron proximity mine action";

    MimironProximityMineAction(PlayerbotAI* ai)
        : MimironFleeAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironBombBotAction : public MoveAwayFromCreatureAction
{
public:
    static constexpr char const* Name = "mimiron bomb bot action";

    MimironBombBotAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, Name, NPC_BOMB_BOT,
                                     ULDUAR_MIMIRON_BOMB_BOT_RADIUS) {}
};

// Where a pet goes in phases 3 and 4: the adds while the Aerial Command Unit hovers out of reach, the
// unit itself while a Magnetic Core has it down, and whatever the melee are on in phase 4.
class MimironPetControlAction : public Action
{
public:
    static constexpr char const* Name = "mimiron pet control action";

    MimironPetControlAction(PlayerbotAI* ai) : Action(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode (Firefighter): step out of the persistent ground fire before it burns the bot down.
// Hard mode (Firefighter): close on the target by the clear side of it. "reach melee" and
// "reach spell" walk the straight line and stop where it ends, which in a Firefighter phase 3 is
// inside a burning node two thirds of the time; they cannot be screened per encounter, because
// the action names are registered once for every bot on the server.
class MimironApproachTargetAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron approach target action";

    MimironApproachTargetAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}

    bool Execute(Event event) override;
};

class MimironDodgeFlamesAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron dodge flames action";

    MimironDodgeFlamesAction(PlayerbotAI* ai) : MimironFleeAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode (Firefighter): clear VX-001's Frost Bomb radius before it detonates. On the shared fan
// rather than MoveAwayFromCreatureAction, whose own fan reaches 30 yd and which refuses to move at
// all when no candidate clears the full radius - which at 30 yd is most of them.
class MimironFrostBombAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron frost bomb action";

    MimironFrostBombAction(PlayerbotAI* ai) : MimironFleeAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode (Firefighter): step out of an Emergency Fire Bot's Water Spray line, sideways since the
// line runs 15 yd ahead of it, or for a caster or healer in 25-man out of its silence aura.
class MimironFireBotAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron fire bot action";

    MimironFireBotAction(PlayerbotAI* ai) : MimironFleeAction(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Bomb Bots are the one Mimiron add that takes a snare - immunity set -263 leaves SNARE, ROOT, STUN,
// FREEZE, GRIP and KNOCKOUT off, all of which the Assault Bot's -285 carries. 20000 HP at 8.0 yd/s,
// so every second of extra approach is most of a cast.
class MimironSlowBombBotAction : public Action
{
public:
    static constexpr char const* Name = "mimiron slow bomb bot action";

    MimironSlowBombBotAction(PlayerbotAI* ai) : Action(ai, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
