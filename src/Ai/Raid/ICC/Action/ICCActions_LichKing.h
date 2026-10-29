/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_LICHKING_H
#define PLAYERBOTS_ICCACTIONS_LICHKING_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_LichKing.h"
#include "LastMovementValue.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands. Arrival is judged in 2D.
class IccLkWalkAction : public MovementAction
{
public:
    IccLkWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    // Walks to the spot the getter hands out, booking `priority`. `interrupt` drops a cast that pins the
    // bot's feet, `nitro` spends Nitro Boosts on a walk past 15 yd.
    bool WalkFor(bool (*getter)(PlayerbotAI*, Position&), MovementPriority priority, bool interrupt, bool nitro);

private:
    Position const* WalkInFlight();
    bool WalkTo(Position const& spot, MovementPriority priority, bool interrupt);
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class IccLichKingQuakeAction : public IccLkWalkAction
{
public:
    static constexpr char const* Name = "icc lich king quake";

    IccLichKingQuakeAction(PlayerbotAI* botAI) : IccLkWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingShadowTrapAction : public IccLkWalkAction
{
public:
    static constexpr char const* Name = "icc lich king shadow trap";

    IccLichKingShadowTrapAction(PlayerbotAI* botAI) : IccLkWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc lich king cast tank defensive";

    IccLichKingCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc lich king taunt";

    IccLichKingTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingPlagueCureAction : public Action
{
public:
    static constexpr char const* Name = "icc lich king plague cure";

    IccLichKingPlagueCureAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingPlagueWalkAction : public IccLkWalkAction
{
public:
    static constexpr char const* Name = "icc lich king plague walk";

    IccLichKingPlagueWalkAction(PlayerbotAI* botAI) : IccLkWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingTranquilizeAction : public Action
{
public:
    static constexpr char const* Name = "icc lich king tranquilize";

    IccLichKingTranquilizeAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingStunAction : public Action
{
public:
    static constexpr char const* Name = "icc lich king stun";

    IccLichKingStunAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingSilenceDispelAction : public Action
{
public:
    static constexpr char const* Name = "icc lich king silence dispel";

    IccLichKingSilenceDispelAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc lich king target";

    IccLichKingTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetLkTarget) {}
};

// A braked bot stops swinging or shooting at him: the resolver hands it nothing, which leaves its attack
// running
class IccLichKingBrakeStopAction : public Action
{
public:
    static constexpr char const* Name = "icc lich king brake stop";

    IccLichKingBrakeStopAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingStandAction : public IccLkWalkAction
{
public:
    static constexpr char const* Name = "icc lich king stand";

    IccLichKingStandAction(PlayerbotAI* botAI) : IccLkWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
