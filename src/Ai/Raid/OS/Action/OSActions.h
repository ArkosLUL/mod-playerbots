/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSACTIONS_H
#define PLAYERBOTS_OSACTIONS_H

#include "AttackAction.h"
#include "MovementActions.h"
#include "NaxxActions.h"
#include "OSHelpers.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

// Every positioning action here anchors on a constant (Sartharion's home X, a drake landing coord)
// plus the raid-wide corridor Y, never on a moving reference, so there is nothing to freeze into slot
// state. What stops the re-issued move each tick is the arrival tolerance below plus the movement
// lock MoveTo stamps - not IsDuplicateMove, which needs the destination within 0.01yd of the last one
// and never fires here.
//
// MovementPriority is a separate ladder from the ACTION_* one. ACTION_* decides which action runs;
// MovementPriority decides whether its MoveTo is accepted at all, because IsWaitingForLastMove
// compares with a strict >. So a dodge issued at the same priority as the hold it has to interrupt is
// silently refused for as long as the hold's lock lasts - up to MaxWaitForMove, which is longer than
// the 3.6s a Flame Tsunami gives. The three emergency dodges issue at MOVEMENT_FORCED for that
// reason; everything else stays at MOVEMENT_COMBAT. Precedence between the three FORCED dodges cannot
// come from this ladder (FORCED > FORCED is false) and is settled by OsMechanicPriorityMultiplier.
class OsPositioningAction : public MovementAction
{
public:
    OsPositioningAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    // Clamps to the platform and then inside a Range Marker circle, drops the move when already
    // within tolerance, and issues it otherwise.
    bool MoveToClamped(float x, float y, float tolerance = OsHelpers::CORRIDOR_ARRIVAL_TOLERANCE,
                       MovementPriority priority = MovementPriority::MOVEMENT_COMBAT);

    // Resolves the destination's own ground Z rather than reusing the bot's, validates it against
    // collision, and issues the move. Callers that clamp and test the destination themselves use this
    // directly. rejectOnCollision is true for the dodges, where a blocked path means try elsewhere,
    // and false for routine holds, which take the clamped coordinates and walk.
    bool IssueMove(float x, float y, MovementPriority priority, bool rejectOnCollision);
};

class OsTsunamiCorridorAction : public OsPositioningAction
{
public:
    OsTsunamiCorridorAction(PlayerbotAI* botAI, std::string const name = "os tsunami corridor")
        : OsPositioningAction(botAI, name) {}
    bool Execute(Event event) override;
};

class OsAvoidTwilightFissureAction : public OsPositioningAction
{
public:
    OsAvoidTwilightFissureAction(PlayerbotAI* botAI, std::string const name = "os avoid twilight fissure")
        : OsPositioningAction(botAI, name) {}
    bool Execute(Event event) override;
};

// The two holds that keep a victim as well as a position. They need Attack(), so they cannot share
// OsPositioningAction's base, but their moves go through the same destination gate.
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
    OsMainTankHoldAction(PlayerbotAI* botAI, std::string const name = "os main tank hold")
        : OsHoldAction(botAI, name) {}
    bool Execute(Event event) override;
};

// Melee who are on Sartharion himself, and only while a corridor swap has turned him onto them.
// "rear flank" is wrong here: his rear is the Tail Lash cone. It stays right against the drakes,
// whose dangerous cone is the frontal one.
class OsSartharionFlankAction : public OsPositioningAction
{
public:
    OsSartharionFlankAction(PlayerbotAI* botAI, std::string const name = "os sartharion flank")
        : OsPositioningAction(botAI, name) {}
    bool Execute(Event event) override;
};

// Melee on a drake, which is the mirror of the class above: a drake's cone is frontal only, so the
// safe ground is straight behind it rather than off its flank.
class OsDrakeRearAction : public OsPositioningAction
{
public:
    OsDrakeRearAction(PlayerbotAI* botAI, std::string const name = "os drake rear")
        : OsPositioningAction(botAI, name) {}
    bool Execute(Event event) override;
};

// Puts a druid tank back in bear form. The class strategy already does this, but only from the combat
// engine, and the form is lost in the window where the off-tank is out of combat.
class OsTankShapeshiftAction : public Action
{
public:
    OsTankShapeshiftAction(PlayerbotAI* botAI, std::string const name = "os tank shapeshift")
        : Action(botAI, name) {}
    bool Execute(Event event) override;
};

// Last resort, above every dodge. A bot that has left the platform cannot be hit by a tsunami and
// cannot do anything useful either, so getting it back outranks the mechanics.
class OsReturnToPlatformAction : public OsPositioningAction
{
public:
    OsReturnToPlatformAction(PlayerbotAI* botAI, std::string const name = "os return to platform")
        : OsPositioningAction(botAI, name) {}
    bool Execute(Event event) override;
};

class OsDrakeLandingPositionAction : public OsPositioningAction
{
public:
    OsDrakeLandingPositionAction(PlayerbotAI* botAI, std::string const name = "os drake landing position")
        : OsPositioningAction(botAI, name) {}
    bool Execute(Event event) override;
};

class OsOffTankHoldAction : public OsHoldAction
{
public:
    OsOffTankHoldAction(PlayerbotAI* botAI, std::string const name = "os offtank hold")
        : OsHoldAction(botAI, name) {}
    bool Execute(Event event) override;
};

class OsRaidHoldAction : public OsPositioningAction
{
public:
    OsRaidHoldAction(PlayerbotAI* botAI, std::string const name = "os raid hold")
        : OsPositioningAction(botAI, name) {}
    bool Execute(Event event) override;
};

class OsRedirectThreatAction : public NaxxRedirectThreatAction
{
public:
    OsRedirectThreatAction(PlayerbotAI* botAI) : NaxxRedirectThreatAction(botAI, "os redirect threat") {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

class OsMainTankCooldownAction : public Action
{
public:
    OsMainTankCooldownAction(PlayerbotAI* botAI, std::string const name = "os main tank cooldown")
        : Action(botAI, name) {}
    bool Execute(Event event) override;
};

class OsTranquilizeEnrageAction : public Action
{
public:
    OsTranquilizeEnrageAction(PlayerbotAI* botAI, std::string const name = "os tranquilize enrage")
        : Action(botAI, name) {}
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
    EnterTwilightPortalAction(PlayerbotAI* botAI, std::string const name = "enter twilight portal")
        : MovementAction(botAI, name) {}
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
