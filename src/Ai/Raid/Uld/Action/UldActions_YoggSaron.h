#ifndef PLAYERBOTS_ULDACTIONS_YOGGSARON_H
#define PLAYERBOTS_ULDACTIONS_YOGGSARON_H

#include "Action.h"
#include "AttackAction.h"
#include "EncounterHelpers.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidAntiFear.h"
#include "UldEncounter_YoggSaron.h"
#include "UldTriggers.h"
#include "Vehicle.h"

#include <functional>
#include <vector>

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

class YoggSaronMaladyOfTheMindAction : public MoveAwayFromPlayerWithDebuffAction
{
public:
    YoggSaronMaladyOfTheMindAction(PlayerbotAI* ai) : MoveAwayFromPlayerWithDebuffAction(ai, "yogg-saron malady of the mind action", SPELL_MALADY_OF_THE_MIND, 15.0f) {}
};

// Latch and sweep shared by the two spacing nodes. Each phase puts everything it has to clear into one
// node: two nodes at the same relevance cannot share a bot - the engine ends the tick at the first
// action returning true - so a pair would trade ticks and walk the bot down the line between their
// destinations.
class YoggSaronSpacingAction : public MovementAction
{
public:
    YoggSaronSpacingAction(PlayerbotAI* ai, std::string const name) : MovementAction(ai, name) {}

    bool Execute(Event event) override;

protected:
    struct HazardSet
    {
        std::vector<EncounterHelpers::HazardCircle> hazards;

        // Swept again with `clear` dropped when nothing satisfies everything at once. Empty means no
        // retry: the bot stays where it is rather than moving to a spot that is no better.
        std::vector<EncounterHelpers::HazardCircle> fallback;

        // Shapes a circle cannot describe, like the Crush wedge. True means the spot is safe.
        std::function<bool(float, float)> clear;
    };

    // False skips the tick outright.
    virtual bool Collect(HazardSet& set) = 0;
    virtual float SearchRadius() const = 0;

private:
    // Held destination. The hazards move - clouds orbit at 3 yd/s, a Crusher re-faces onto whoever it
    // is hitting - so a fresh sweep every tick answers a different question every tick and the bot
    // never arrives.
    Position heldSpot;
    uint32 heldSpotMs = 0;
};

class YoggSaronPhase1SpacingAction : public YoggSaronSpacingAction
{
public:
    YoggSaronPhase1SpacingAction(PlayerbotAI* ai) : YoggSaronSpacingAction(ai, "yogg-saron phase 1 spacing action") {}

protected:
    bool Collect(HazardSet& set) override;
    float SearchRadius() const override { return ULDUAR_YOGG_SARON_SPACING_SEARCH_RADIUS; }
};

class YoggSaronPhase2SpacingAction : public YoggSaronSpacingAction
{
public:
    YoggSaronPhase2SpacingAction(PlayerbotAI* ai) : YoggSaronSpacingAction(ai, "yogg-saron phase 2 spacing action") {}

protected:
    bool Collect(HazardSet& set) override;
    float SearchRadius() const override { return ULDUAR_YOGG_SARON_P2_SPACING_SEARCH_RADIUS; }
};

// One owner of every non-tank's target for the whole encounter, in place of the raid icons this fight
// used to target through.
class YoggSaronSetDpsPriorityAction : public AttackAction
{
public:
    YoggSaronSetDpsPriorityAction(PlayerbotAI* ai) : AttackAction(ai, "yogg-saron set dps priority action") {}

    bool Execute(Event event) override;

private:
    // Kill order for wherever the bot is standing, as a tier index. npos means "not a target here".
    static size_t TierOf(Unit* unit, bool brainLevel);
    bool IsAllowedTarget(Unit* candidate, bool tentaclesCleared) const;
    Unit* ResolveTarget(Unit* currentTarget);
};

class YoggSaronDarkVolleyInterruptAction : public Action
{
public:
    YoggSaronDarkVolleyInterruptAction(PlayerbotAI* ai) : Action(ai, "yogg-saron dark volley interrupt action") {}

    bool Execute(Event event) override;

private:
    bool CastClassInterrupt(Unit* target);
    int32 GetInterrupterIndex();
};

// Phase 3 housekeeping one designated bot tank does for the raid: the TankAssist strategy swap, and
// the cheat that finishes an Immortal Guardian nothing else can kill.
class YoggSaronPhase3ControlAction : public Action
{
public:
    YoggSaronPhase3ControlAction(PlayerbotAI* ai) : Action(ai, "yogg-saron phase 3 control action") {}

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
    bool KillIllusionAdd(YoggSaronTrigger yoggSaronTrigger);
    bool GoToBrainRoom(YoggSaronTrigger yoggSaronTrigger);
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

// Pop an immunity to shed Squeeze, which kills the Constrictor Tentacle holding the bot.
class YoggSaronSqueezeEscapeAction : public Action
{
public:
    YoggSaronSqueezeEscapeAction(PlayerbotAI* ai) : Action(ai, "yogg-saron squeeze escape action") {}

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
