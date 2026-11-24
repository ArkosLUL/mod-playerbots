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

#include <optional>

class MimironResetEncounterStateAction : public Action
{
public:
    static constexpr char const* Name = "mimiron reset encounter state action";

    MimironResetEncounterStateAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class MimironFleeAction : public MovementAction
{
public:
    MimironFleeAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    bool MoveAwayClearOfMines(Unit* from, float distance,
                              MovementPriority priority = MovementPriority::MOVEMENT_COMBAT,
                              bool fallbackUnfiltered = true, bool interrupt = false,
                              char const* what = "flee", float clearRadius = 0.0f,
                              bool sectorScreen = false, bool allowFire = false);

    bool MoveAwayClearOfMines(Position const& from, float distance,
                              MovementPriority priority = MovementPriority::MOVEMENT_COMBAT,
                              bool fallbackUnfiltered = true, bool interrupt = false,
                              char const* what = "flee", float clearRadius = 0.0f,
                              std::optional<float> toward = std::nullopt);

    bool MoveTowardClearOfMines(Position const& dest,
                                MovementPriority priority = MovementPriority::MOVEMENT_COMBAT,
                                bool fallbackUnfiltered = true, bool interrupt = false,
                                char const* what = "step", bool allowFire = false,
                                bool sirenScreen = true);

private:
    bool FleeFan(Position const& from, Unit* fallbackFrom, float distance, MovementPriority priority,
                 bool fallbackUnfiltered, bool interrupt, char const* what, float clearRadius = 0.0f,
                 bool allowFire = false, bool sectorScreen = false,
                 std::optional<float> toward = std::nullopt, bool sirenScreen = true);

    bool MoveAwayOnFloor(Unit* from, float distance);

    void NoteFleeOutcome(char const* what, char const* outcome, float const* taken, uint32 refusedBack,
                         uint32 refusedMine, uint32 refusedCone, uint32 refusedFire, uint32 refusedBomb,
                         uint32 refusedRocket, uint32 refusedBurst, uint32 refusedShock, uint32 refusedSpray,
                         uint32 refusedMove, uint32 refusedHold, uint32 refusedBombBot);
};

class MimironUnreachableStepAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron unreachable step action";

    MimironUnreachableStepAction(PlayerbotAI* botAI) : MimironFleeAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironShockBlastAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron shock blast action";

    MimironShockBlastAction(PlayerbotAI* botAI) : MimironFleeAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool FleeShockToAnchor();
};

class MimironPhase1PositioningAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron phase 1 positioning action";

    MimironPhase1PositioningAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironP3Wx2LaserBarrageAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron p3wx2 laser barrage action";

    MimironP3Wx2LaserBarrageAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    void NoteBarrageDecision(char const* branch, char const* direction, float cw);
};

class MimironArcSpreadAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron arc spread action";

    MimironArcSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironAerialCommandUnitAction : public Action
{
public:
    static constexpr char const* Name = "mimiron aerial command unit action";

    MimironAerialCommandUnitAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class MimironRocketStrikeAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron rocket strike action";

    MimironRocketStrikeAction(PlayerbotAI* botAI) : MimironFleeAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironPhase4FocusAction : public AttackAction
{
public:
    static constexpr char const* Name = "mimiron phase 4 focus action";

    MimironPhase4FocusAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class MimironMagneticCoreAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron magnetic core action";

    MimironMagneticCoreAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    void NoteCoreStep(char const* step);

    bool chaining = false;
};

class MimironFrostResistanceAction : public Action
{
public:
    static constexpr char const* Name = "mimiron frost resistance action";

    MimironFrostResistanceAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class MimironPlasmaBlastDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "mimiron plasma blast defensive action";

    MimironPlasmaBlastDefensiveAction(PlayerbotAI* botAI)
        : Action(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    static constexpr char const* HEALER_EXTERNALS[] = {"pain suppression", "guardian spirit",
                                                       "hand of sacrifice"};

    Unit* PlasmaVictim();
    bool TankHasDefensive(Unit* victim);
};

class MimironRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "mimiron redirect threat action";

    MimironRedirectThreatAction(PlayerbotAI* botAI)
        : RaidRedirectThreatAction(botAI, Name) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

class MimironSetDpsPriorityAction : public AttackAction
{
public:
    static constexpr char const* Name = "mimiron set dps priority action";

    MimironSetDpsPriorityAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;

private:
    std::vector<std::pair<uint32, Unit*>> BuildPriorityList();

    Unit* SelectByEntry(Unit* currentTarget, uint32 entry, std::vector<Unit*> const& candidates) const;

    bool IsAllowedTarget(Unit* unit) const;

    Unit* ResolveTarget(Unit* currentTarget, bool allowFallback);

    bool ApproachOutOfSight(Unit* target);

    static char const* DescribeTargetRule(Unit* unit);
};

class MimironProximityMineAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron proximity mine action";

    MimironProximityMineAction(PlayerbotAI* botAI)
        : MimironFleeAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironBombBotAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron bomb bot action";

    MimironBombBotAction(PlayerbotAI* botAI)
        : MimironFleeAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironPetControlAction : public Action
{
public:
    static constexpr char const* Name = "mimiron pet control action";

    MimironPetControlAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironApproachTargetAction : public MovementAction
{
public:
    static constexpr char const* Name = "mimiron approach target action";

    MimironApproachTargetAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class MimironDodgeFlamesAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron dodge flames action";

    MimironDodgeFlamesAction(PlayerbotAI* botAI) : MimironFleeAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironFrostBombAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron frost bomb action";

    MimironFrostBombAction(PlayerbotAI* botAI) : MimironFleeAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironFireBotAction : public MimironFleeAction
{
public:
    static constexpr char const* Name = "mimiron fire bot action";

    MimironFireBotAction(PlayerbotAI* botAI) : MimironFleeAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironSlowBombBotAction : public Action
{
public:
    static constexpr char const* Name = "mimiron slow bomb bot action";

    MimironSlowBombBotAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
