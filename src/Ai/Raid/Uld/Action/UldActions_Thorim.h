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

class ThorimMarkDpsTargetAction : public Action
{
public:
    ThorimMarkDpsTargetAction(PlayerbotAI* ai) : Action(ai, "thorim mark dps target action") {}

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

class ThorimGauntletPositioningAction : public MovementAction
{
public:
    ThorimGauntletPositioningAction(PlayerbotAI* ai) : MovementAction(ai, "thorim gauntlet positioning action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
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
