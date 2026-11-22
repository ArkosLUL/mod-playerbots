/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSACTIONS_H
#define PLAYERBOTS_OSACTIONS_H

#include "AttackAction.h"
#include "MovementActions.h"
#include "OSHelpers.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidRedirectThreat.h"

class OsPositioningAction : public MovementAction
{
public:
    OsPositioningAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    bool MoveToClamped(float x, float y, float tolerance = OsHelpers::CORRIDOR_ARRIVAL_TOLERANCE,
                       MovementPriority priority = MovementPriority::MOVEMENT_COMBAT);

    bool IssueMove(float x, float y, MovementPriority priority, bool rejectOnCollision);
};

class OsTsunamiCorridorAction : public OsPositioningAction
{
public:
    static constexpr char const* Name = "os tsunami corridor";

    OsTsunamiCorridorAction(PlayerbotAI* botAI) : OsPositioningAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsAvoidTwilightFissureAction : public OsPositioningAction
{
public:
    static constexpr char const* Name = "os avoid twilight fissure";

    OsAvoidTwilightFissureAction(PlayerbotAI* botAI) : OsPositioningAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsHoldAction : public AttackAction
{
public:
    OsHoldAction(PlayerbotAI* botAI, std::string const name) : AttackAction(botAI, name) {}

protected:
    bool IssueMove(float x, float y, MovementPriority priority, bool rejectOnCollision);
};

class OsMainTankHoldAction : public OsHoldAction
{
public:
    static constexpr char const* Name = "os main tank hold";

    OsMainTankHoldAction(PlayerbotAI* botAI) : OsHoldAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsSartharionFlankAction : public OsPositioningAction
{
public:
    static constexpr char const* Name = "os sartharion flank";

    OsSartharionFlankAction(PlayerbotAI* botAI) : OsPositioningAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsDrakeRearAction : public OsPositioningAction
{
public:
    static constexpr char const* Name = "os drake rear";

    OsDrakeRearAction(PlayerbotAI* botAI) : OsPositioningAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsTankShapeshiftAction : public Action
{
public:
    static constexpr char const* Name = "os tank shapeshift";

    OsTankShapeshiftAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsReturnToPlatformAction : public OsPositioningAction
{
public:
    static constexpr char const* Name = "os return to platform";

    OsReturnToPlatformAction(PlayerbotAI* botAI) : OsPositioningAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsDrakeLandingPositionAction : public OsPositioningAction
{
public:
    static constexpr char const* Name = "os drake landing position";

    OsDrakeLandingPositionAction(PlayerbotAI* botAI) : OsPositioningAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsOffTankHoldAction : public OsHoldAction
{
public:
    static constexpr char const* Name = "os offtank hold";

    OsOffTankHoldAction(PlayerbotAI* botAI) : OsHoldAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsRaidHoldAction : public OsPositioningAction
{
public:
    static constexpr char const* Name = "os raid hold";

    OsRaidHoldAction(PlayerbotAI* botAI) : OsPositioningAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "os redirect threat";

    OsRedirectThreatAction(PlayerbotAI* botAI) : RaidRedirectThreatAction(botAI, Name) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

class OsMainTankCooldownAction : public Action
{
public:
    static constexpr char const* Name = "os main tank cooldown";

    OsMainTankCooldownAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class OsTranquilizeEnrageAction : public Action
{
public:
    static constexpr char const* Name = "os tranquilize enrage";

    OsTranquilizeEnrageAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class SartharionAttackPriorityAction : public AttackAction
{
public:
    SartharionAttackPriorityAction(PlayerbotAI* botAI, std::string const name = "sartharion attack priority")
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

class EnterTwilightPortalAction : public MovementAction
{
public:
    static constexpr char const* Name = "enter twilight portal";

    EnterTwilightPortalAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class ExitTwilightPortalAction : public MovementAction
{
public:
    ExitTwilightPortalAction(PlayerbotAI* botAI, std::string const name = "exit twilight portal")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

#endif
