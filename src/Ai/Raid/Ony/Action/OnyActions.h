/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYACTIONS_H
#define PLAYERBOTS_ONYACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "RaidAntiFear.h"
#include "RaidRedirectThreat.h"

// Issues at MOVEMENT_FORCED so it can cut through the tank's walk and the layout moves. Several
// reasons at once share one destination clear of all of them.
class OnyxiaDodgeAction : public MovementAction
{
public:
    static constexpr char const* Name = "onyxia dodge";

    OnyxiaDodgeAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OnyxiaMainTankCooldownAction : public Action
{
public:
    static constexpr char const* Name = "onyxia main tank cooldown";

    OnyxiaMainTankCooldownAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

// Takes her, walks her to the tank spot with his back to the north wall and keeps her there.
class OnyxiaMainTankAction : public AttackAction
{
public:
    static constexpr char const* Name = "onyxia main tank";

    OnyxiaMainTankAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OnyxiaAntiFearAction : public RaidAntiFearAction
{
public:
    static constexpr char const* Name = "onyxia anti fear";

    OnyxiaAntiFearAction(PlayerbotAI* botAI) : RaidAntiFearAction(botAI, Name) {}

protected:
    bool FearWindowActive() override;
};

class OnyxiaRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "onyxia redirect threat";

    OnyxiaRedirectThreatAction(PlayerbotAI* botAI) : RaidRedirectThreatAction(botAI, Name) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

class OnyxiaChooseTargetAction : public AttackAction
{
public:
    static constexpr char const* Name = "onyxia choose target";

    OnyxiaChooseTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OnyxiaGroundSlotAction : public MovementAction
{
public:
    static constexpr char const* Name = "onyxia ground slot";

    OnyxiaGroundSlotAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OnyxiaAirSpotAction : public MovementAction
{
public:
    static constexpr char const* Name = "onyxia air spot";

    OnyxiaAirSpotAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OnyxiaRearFlankAction : public MovementAction
{
public:
    static constexpr char const* Name = "onyxia rear flank";

    OnyxiaRearFlankAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
