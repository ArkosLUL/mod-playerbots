/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_SINDRAGOSAADDS_H
#define PLAYERBOTS_ICCACTIONS_SINDRAGOSAADDS_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_SindragosaAdds.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidAntiFear.h"
#include "RaidDirectTargeting.h"

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands.
class IccSgAddsWalkAction : public MovementAction
{
public:
    IccSgAddsWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    // The latched spot while the bot is still walking to it, else nullptr
    Position const* WalkInFlight();
    // False without booking anything while a cast pins the bot's feet, unless `interrupt` drops the cast.
    // A walk of its own still in flight at MOVEMENT_FORCED gives way to the new one.
    bool WalkTo(Position const& spot, MovementPriority priority, bool interrupt = false);
    void DropWalk() { walking = false; }

private:
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class IccSgDrakeDodgeAction : public IccSgAddsWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa drake dodge";

    IccSgDrakeDodgeAction(PlayerbotAI* botAI) : IccSgAddsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSgDrakeCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc sindragosa drake cast tank defensive";

    IccSgDrakeCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSgDrakeAntiFearAction : public RaidAntiFearAction
{
public:
    static constexpr char const* Name = "icc sindragosa drake anti fear";

    IccSgDrakeAntiFearAction(PlayerbotAI* botAI) : RaidAntiFearAction(botAI, Name) {}

protected:
    bool FearWindowActive() override;
};

class IccSgDrakeBrakeStopAction : public Action
{
public:
    static constexpr char const* Name = "icc sindragosa drake brake stop";

    IccSgDrakeBrakeStopAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSgDrakeTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc sindragosa drake target";

    IccSgDrakeTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetSgDrakeTarget) {}
};

class IccSgDrakeStandAction : public IccSgAddsWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa drake stand";

    IccSgDrakeStandAction(PlayerbotAI* botAI) : IccSgAddsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSgGauntletDodgeAction : public IccSgAddsWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa gauntlet dodge";

    IccSgGauntletDodgeAction(PlayerbotAI* botAI) : IccSgAddsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSgGauntletTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc sindragosa gauntlet taunt";

    IccSgGauntletTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSgGauntletTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc sindragosa gauntlet target";

    IccSgGauntletTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetSgGauntletTarget) {}
};

#endif
