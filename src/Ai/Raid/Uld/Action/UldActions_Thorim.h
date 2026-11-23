#ifndef PLAYERBOTS_ULDACTIONS_THORIM_H
#define PLAYERBOTS_ULDACTIONS_THORIM_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Thorim.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class ThorimTakeBossAction : public AttackAction
{
public:
    ThorimTakeBossAction(PlayerbotAI* botAI, std::string const name) : AttackAction(botAI, name) {}

    bool Execute(Event event) override;
};

class ThorimUnbalancingStrikeSwapAction : public ThorimTakeBossAction
{
public:
    static constexpr char const* Name = "thorim unbalancing strike swap action";

    ThorimUnbalancingStrikeSwapAction(PlayerbotAI* botAI)
        : ThorimTakeBossAction(botAI, Name)
    {
    }

    bool isUseful() override;
};

class ThorimTankPickupAction : public ThorimTakeBossAction
{
public:
    static constexpr char const* Name = "thorim tank pickup action";

    ThorimTankPickupAction(PlayerbotAI* botAI) : ThorimTakeBossAction(botAI, Name) {}

    bool isUseful() override;
};

class ThorimDpsPriorityAction : public AttackAction
{
public:
    static constexpr char const* Name = "thorim dps priority action";

    ThorimDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimArenaPositioningAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim arena positioning action";

    ThorimArenaPositioningAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimLaneMovementAction : public MovementAction
{
public:
    ThorimLaneMovementAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    bool MoveToGauntletWaypoint(bool leftLane, uint8 index, bool forceCombatPriority);
};

class ThorimGauntletPositioningAction : public ThorimLaneMovementAction
{
public:
    static constexpr char const* Name = "thorim gauntlet positioning action";

    ThorimGauntletPositioningAction(PlayerbotAI* botAI)
        : ThorimLaneMovementAction(botAI, Name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimGauntletLeverAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim gauntlet lever action";

    ThorimGauntletLeverAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimGauntletLeadAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim gauntlet lead action";

    ThorimGauntletLeadAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimBalconyAdvanceAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim balcony advance action";

    ThorimBalconyAdvanceAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimRunicSmashAction : public ThorimLaneMovementAction
{
public:
    static constexpr char const* Name = "thorim runic smash action";

    ThorimRunicSmashAction(PlayerbotAI* botAI) : ThorimLaneMovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimRunicBarrierBailAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim runic barrier bail action";

    ThorimRunicBarrierBailAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimFrostAuraAction : public Action
{
public:
    static constexpr char const* Name = "thorim frost aura action";

    ThorimFrostAuraAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimCastPersonalDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "thorim cast personal defensive action";

    ThorimCastPersonalDefensiveAction(PlayerbotAI* ai) : Action(ai, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimPetLeashAction : public Action
{
public:
    static constexpr char const* Name = "thorim pet leash action";

    ThorimPetLeashAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimChargedOrbAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim charged orb action";

    ThorimChargedOrbAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimArenaLeashAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim arena leash action";

    ThorimArenaLeashAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimResetEncounterStateAction : public Action
{
public:
    static constexpr char const* Name = "thorim reset encounter state action";

    ThorimResetEncounterStateAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class ThorimFallFromFloorAction : public Action
{
public:
    static constexpr char const* Name = "thorim fall from floor action";

    ThorimFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimPhase2PositioningAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim phase 2 positioning action";

    ThorimPhase2PositioningAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimUnreachableStepAction : public MovementAction
{
public:
    static constexpr char const* Name = "thorim unreachable step action";

    ThorimUnreachableStepAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimSifBlizzardAction : public MoveAwayFromCreatureAction
{
public:
    static constexpr char const* Name = "thorim sif blizzard action";

    ThorimSifBlizzardAction(PlayerbotAI* botAI)
        : MoveAwayFromCreatureAction(botAI, Name, NPC_SIF_BLIZZARD,
                                     ULDUAR_THORIM_SIF_BLIZZARD_RADIUS)
    {
    }

    bool Execute(Event event) override;
};

#endif
