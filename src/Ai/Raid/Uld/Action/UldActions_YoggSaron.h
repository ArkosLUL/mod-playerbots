#ifndef PLAYERBOTS_ULDACTIONS_YOGGSARON_H
#define PLAYERBOTS_ULDACTIONS_YOGGSARON_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidAntiFear.h"
#include "UldBossHelper.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class YoggSaronOminousCloudCheatAction : public Action
{
public:
    YoggSaronOminousCloudCheatAction(PlayerbotAI* ai) : Action(ai, "yogg-saron ominous cloud cheat action") {}

    bool Execute(Event event) override;
};

class YoggSaronGuardianPositioningAction : public MovementAction
{
public:
    YoggSaronGuardianPositioningAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron guardian positioning action") {}

    bool Execute(Event event) override;
};

class YoggSaronSanityAction : public MovementAction
{
public:
    YoggSaronSanityAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron sanity action") {}

    bool Execute(Event event) override;
};

class YoggSaronDeathOrbAction : public MoveAwayFromCreatureAction
{
public:
    YoggSaronDeathOrbAction(PlayerbotAI* ai) : MoveAwayFromCreatureAction(ai, "yogg-saron death orb action", NPC_DEATH_ORB, 10.0f) {}
};

class YoggSaronMaladyOfTheMindAction : public MoveAwayFromPlayerWithDebuffAction
{
public:
    YoggSaronMaladyOfTheMindAction(PlayerbotAI* ai) : MoveAwayFromPlayerWithDebuffAction(ai, "yogg-saron malady of the mind action", SPELL_MALADY_OF_THE_MIND, 15.0f) {}
};

class YoggSaronMarkTargetAction : public Action
{
public:
    YoggSaronMarkTargetAction(PlayerbotAI* ai) : Action(ai, "yogg-saron mark target action") {}

    bool Execute(Event event) override;
};

class YoggSaronBrainLinkAction : public MovementAction
{
public:
    YoggSaronBrainLinkAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron brain link action") {}

    bool Execute(Event event) override;
};

class YoggSaronMoveToEnterPortalAction : public MovementAction
{
public:
    YoggSaronMoveToEnterPortalAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron move to enter portal action") {}

    bool Execute(Event event) override;
};

class YoggSaronFallFromFloorAction : public MovementAction
{
public:
    YoggSaronFallFromFloorAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron fall from floor action") {}

    bool Execute(Event event) override;
};

class YoggSaronBossRoomMovementCheatAction : public MovementAction
{
public:
    YoggSaronBossRoomMovementCheatAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron boss room movement cheat action") {}

    bool Execute(Event event) override;
};

class YoggSaronUsePortalAction : public Action
{
public:
    YoggSaronUsePortalAction(PlayerbotAI* ai) : Action(ai, "yogg-saron use portal action") {}

    bool Execute(Event event) override;
};

class YoggSaronIllusionRoomAction : public MovementAction
{
public:
    YoggSaronIllusionRoomAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron illusion room action") {}

    bool Execute(Event event) override;

private:
    bool SetRtiMark(YoggSaronTrigger yoggSaronTrigger);
    bool SetIllusionRtiTarget(YoggSaronTrigger yoggSaronTrigger);
    bool SetBrainRtiTarget(YoggSaronTrigger yoggSaronTrigger);
};

class YoggSaronMoveToExitPortalAction : public MovementAction
{
public:
    YoggSaronMoveToExitPortalAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron move to exit portal action") {}

    bool Execute(Event event) override;
};

class YoggSaronLunaticGazeAction : public MovementAction
{
public:
    YoggSaronLunaticGazeAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron lunatic gaze action") {}

    bool Execute(Event event) override;
};

class YoggSaronPhase3PositioningAction : public MovementAction
{
public:
    YoggSaronPhase3PositioningAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron phase 3 positioning action") {}

    bool Execute(Event event) override;
};

// Reduced-Keeper hard mode: a ranged DPS bot attacks the nearest Crusher Tentacle directly (no group
// marker, so melee keep their target).
class YoggSaronCrusherTentacleAction : public AttackAction
{
public:
    YoggSaronCrusherTentacleAction(PlayerbotAI* ai) : AttackAction(ai, "yogg-saron crusher tentacle action") {}

    bool Execute(Event event) override;
};

// Reduced-Keeper hard mode: the tank holds the melee stack and taunts loose Immortal Guardians to it.
class YoggSaronGuardianControlAction : public MovementAction
{
public:
    YoggSaronGuardianControlAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron guardian control action") {}

    bool Execute(Event event) override;
};

// Reduced-Keeper hard mode: retreat to a safe ranged spot and face away from Yogg to conserve sanity.
class YoggSaronSanityConservationAction : public MovementAction
{
public:
    YoggSaronSanityConservationAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron sanity conservation action") {}

    bool Execute(Event event) override;
};

class YoggSaronAntiFearAction : public RaidAntiFearAction
{
public:
    YoggSaronAntiFearAction(PlayerbotAI* ai) : RaidAntiFearAction(ai, "yogg-saron anti fear action") {}

protected:
    bool FearWindowActive() override { return YoggSaronFearWindowActive(botAI); }
};

#endif
