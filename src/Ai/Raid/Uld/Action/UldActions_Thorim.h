#ifndef PLAYERBOTS_ULDACTIONS_THORIM_H
#define PLAYERBOTS_ULDACTIONS_THORIM_H

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

class ThorimUnbalancingStrikeAction : public Action
{
public:
    ThorimUnbalancingStrikeAction(PlayerbotAI* ai) : Action(ai, "thorim unbalancing strike action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimUnbalancingStrikeSwapAction : public AttackAction
{
public:
    ThorimUnbalancingStrikeSwapAction(PlayerbotAI* ai) : AttackAction(ai, "thorim unbalancing strike swap action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Attacks the add the encounter wants dead, rather than putting a raid icon on it. An icon is a sticky
// override that RtiTargetValue hands back before the smart picker ever runs, and nothing clears it
// until the bot leaves combat, so a mark that lands on the wrong unit cannot be taken back.
class ThorimDpsPriorityAction : public AttackAction
{
public:
    ThorimDpsPriorityAction(PlayerbotAI* ai) : AttackAction(ai, "thorim dps priority action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimArenaPositioningAction : public MovementAction
{
public:
    ThorimArenaPositioningAction(PlayerbotAI* ai) : MovementAction(ai, "thorim arena positioning action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Shared by the two nodes that walk the corridor lanes, so the six-waypoint walk exists once.
class ThorimLaneMovementAction : public MovementAction
{
public:
    ThorimLaneMovementAction(PlayerbotAI* ai, std::string const name) : MovementAction(ai, name) {}

protected:
    bool MoveToGauntletWaypoint(bool leftLane, uint8 index, bool forceCombatPriority);
};

class ThorimGauntletPositioningAction : public ThorimLaneMovementAction
{
public:
    ThorimGauntletPositioningAction(PlayerbotAI* ai)
        : ThorimLaneMovementAction(ai, "thorim gauntlet positioning action")
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Cross to the same progress point in the lane the Runic Colossus is not smashing.
class ThorimRunicSmashAction : public ThorimLaneMovementAction
{
public:
    ThorimRunicSmashAction(PlayerbotAI* ai) : ThorimLaneMovementAction(ai, "thorim runic smash action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Step out of the damage shield's reach without dropping the target, so ranged and instant abilities
// keep landing while the bot heals back up.
class ThorimRunicBarrierBailAction : public MovementAction
{
public:
    ThorimRunicBarrierBailAction(PlayerbotAI* ai) : MovementAction(ai, "thorim runic barrier bail action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimLightningChargeAction : public MovementAction
{
public:
    ThorimLightningChargeAction(PlayerbotAI* ai) : MovementAction(ai, "thorim lightning charge action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Steps out of the charged Thunder Orb's field, by the shortest way out rather than across the room.
class ThorimChargedOrbAction : public MovementAction
{
public:
    ThorimChargedOrbAction(PlayerbotAI* ai) : MovementAction(ai, "thorim charged orb action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Walks an arena squad member back inside the box boss_thorim.cpp scans for a living player.
class ThorimArenaLeashAction : public MovementAction
{
public:
    ThorimArenaLeashAction(PlayerbotAI* ai) : MovementAction(ai, "thorim arena leash action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimResetEncounterStateAction : public Action
{
public:
    ThorimResetEncounterStateAction(PlayerbotAI* ai) : Action(ai, "thorim reset encounter state action") {}

    bool Execute(Event event) override;
};

class ThorimFallFromFloorAction : public Action
{
public:
    ThorimFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, "thorim fall from floor action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThorimPhase2PositioningAction : public MovementAction
{
public:
    ThorimPhase2PositioningAction(PlayerbotAI* ai) : MovementAction(ai, "thorim phase 2 positioning action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode: clear Sif's moving Blizzard ground AoE.
class ThorimSifBlizzardAction : public MoveAwayFromCreatureAction
{
public:
    ThorimSifBlizzardAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "thorim sif blizzard action", NPC_SIF_BLIZZARD,
                                     ULDUAR_THORIM_SIF_BLIZZARD_RADIUS)
    {
    }
};

// Hard mode: ranged/healers back off so Sif's point-blank Frost Nova misses.
class ThorimSifFrostNovaAction : public MoveAwayFromCreatureAction
{
public:
    ThorimSifFrostNovaAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "thorim sif frost nova action", NPC_SIF,
                                     ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS)
    {
    }
};

#endif
