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
#include <optional>
#include <set>
#include <vector>

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

        // The retry, swept when nothing satisfies everything at once: whichever half of the set is
        // the one that kills. Empty means no retry - the bot stays where it is rather than moving to
        // a spot that is no better. fallbackClear is the retry's own shape test, null to drop it.
        std::vector<EncounterHelpers::HazardCircle> fallback;
        std::function<bool(float, float)> fallbackClear;

        // Shapes a circle cannot describe, like the Crush wedge. True means the spot is safe.
        std::function<bool(float, float)> clear;

        // Where the bot would rather end up, on top of being safe. Tried first and dropped when
        // nothing satisfies both, so it can never leave a bot standing in a hazard. Null means the
        // first spot that clears everything will do.
        std::function<bool(float, float)> preferred;
    };

    // False skips the tick outright.
    virtual bool Collect(HazardSet& set) = 0;
    virtual float SearchRadius() const = 0;

    // How far out a destination may sit from the body. The load-bearing half of the dodge: too tight
    // and a bot needing a sidestep has every outward candidate rejected.
    virtual float MaxFromMiddle() const { return ULDUAR_YOGG_SARON_SPACING_MAX_FROM_MIDDLE; }

    // What MaxFromMiddle is measured against, and what the sweep biases toward. The boss platform for
    // both phases that happen on it; an illusion room is 93 yd below it and its own middle away.
    virtual Position Anchor() const { return ULDUAR_YOGG_SARON_MIDDLE; }

    // Whether the walk to a candidate is acceptable, not just the candidate itself. No opinion by
    // default: for most hazards crossing one to leave another still beats standing still.
    virtual bool RouteAcceptable(float /*x*/, float /*y*/) const { return true; }

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
    bool RouteAcceptable(float x, float y) const override;

private:
    // Where the clouds stood on this tick's Collect. Their 14 yd clear circles are already in the
    // hazard set; this is for the tighter summon radius the walk itself has to miss.
    std::vector<Position> clouds;
};

// Ranged and healers hold one spot on the second orbit. Everyone walks to the same point, as the
// phase 3 stations do; the trigger's stack radius is what stops the re-issue and shapes the blob.
class YoggSaronPhase1StationAction : public MovementAction
{
public:
    YoggSaronPhase1StationAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron phase 1 station action") {}

    bool Execute(Event event) override;
};

class YoggSaronPhase2SpacingAction : public YoggSaronSpacingAction
{
public:
    YoggSaronPhase2SpacingAction(PlayerbotAI* ai) : YoggSaronSpacingAction(ai, "yogg-saron phase 2 spacing action") {}

protected:
    bool Collect(HazardSet& set) override;
    float SearchRadius() const override { return ULDUAR_YOGG_SARON_P2_SPACING_SEARCH_RADIUS; }
    float MaxFromMiddle() const override { return ULDUAR_YOGG_SARON_P2_SPACING_MAX_FROM_MIDDLE; }
    bool RouteAcceptable(float x, float y) const override;
};

// Stand so that facing the tentacle faces away from the skulls. Facing itself cannot be held - set
// facing, AttackAction and CastSpell each turn the bot back at its target inside the same tick - so
// the only lever is which side of the tentacle the bot fights from. One pull ate 122,980 damage and
// 272 Sanity with the tentacle and the nearest skull inside 90 degrees of each other in 459 of 640
// samples, and the bot nearer the tentacle in 73% of those, which is a sidestep of a few yards.
class YoggSaronIllusionFacingAction : public YoggSaronSpacingAction
{
public:
    YoggSaronIllusionFacingAction(PlayerbotAI* ai)
        : YoggSaronSpacingAction(ai, "yogg-saron illusion facing action")
    {
    }

protected:
    bool Collect(HazardSet& set) override;
    float SearchRadius() const override { return ULDUAR_YOGG_SARON_ILLUSION_FACING_SEARCH_RADIUS; }
    float MaxFromMiddle() const override { return ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS; }
    Position Anchor() const override { return roomMiddle; }

private:
    // Filled by Collect, which is what decides the room the bot is standing in. Anchor() is called
    // after it and is const, so it cannot work that out for itself.
    Position roomMiddle;
};

// Walk around the body instead of through it. ReachCombatTo shortens its path to halfway, so a target
// on the far side of Yogg puts the destination on top of him: one pull sent eight ranged and healers
// to 7.6-9.4 yd of the middle in a single second and the knock back caught thirteen of them.
class YoggSaronBodyDetourAction : public MovementAction
{
public:
    YoggSaronBodyDetourAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron body detour action") {}

    bool Execute(Event event) override;
};

// Keep pets and guardians out of a Crusher Tentacle's melee range. Nothing else can put one there:
// melee bots are already barred from targeting a Crusher, and every Crush cone in one pull was
// procced by somebody's pet while the nearest player stood 12 yd clear.
class YoggSaronPetGuardAction : public Action
{
public:
    YoggSaronPetGuardAction(PlayerbotAI* ai) : Action(ai, "yogg-saron pet guard action") {}

    bool Execute(Event event) override;

private:
    // Give a pet its stance back, once it is clear and only if this node took it away.
    void Unhush(Creature* pet);

    // Pets currently held passive because there was nothing but a Crusher for them to hit.
    std::set<ObjectGuid> hushed;
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

    // Whether the Brain is open to hit. Only worked out once a Brain turns up, most calls never meet one.
    struct BrainApproach
    {
        bool brainLevel = false;
        std::optional<bool> approachable;
    };

    bool IsAllowedTarget(Unit* candidate, BrainApproach& brain) const;
    Unit* ResolveTarget(Unit* currentTarget);

    // An illusion room's tentacles, one bot each while any is untaken. Nearest-first sends the whole
    // team, which walks in as one pack, to the same tentacle every time. nullptr when none is allowed.
    Unit* SpreadTarget(std::vector<Unit*> const& candidates, Unit* currentTarget, BrainApproach& brain);

    // Stops the bot, and any of its pets, hitting `target`. The healer's cast in progress is a heal,
    // so it keeps it.
    void DropTarget(Unit* target, bool interruptCasts = true);
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

// A paladin's Judgement breaks a Crusher's Diminish Power channel from outside its swing. Each break
// buys 1.5-3 s without the raid-wide -21%, once per 10 s cooldown.
class YoggSaronDiminishPowerJudgementAction : public Action
{
public:
    YoggSaronDiminishPowerJudgementAction(PlayerbotAI* ai) : Action(ai, "yogg-saron diminish power judgement action") {}

    bool Execute(Event event) override;
};

// The TankAssist strategy swap one designated bot tank does for the raid at the start of phase 3.
// TankFace goes with it: it would turn the tank back into Lunatic Gaze, which the face-away answers.
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

// Following a master is wrong in every part of this fight, and the bots that go down to the brain
// level are exactly the ones that used to idle behind a human there.
class YoggSaronStopFollowingAction : public Action
{
public:
    YoggSaronStopFollowingAction(PlayerbotAI* ai) : Action(ai, "yogg-saron stop following action") {}

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
    bool GoToBrainRoom(YoggSaronTrigger yoggSaronTrigger);
    bool WalkIntoRoom();
};

class YoggSaronIllusionHealerStationAction : public MovementAction
{
public:
    YoggSaronIllusionHealerStationAction(PlayerbotAI* ai)
        : MovementAction(ai, "yogg-saron illusion healer station action") {}

    bool Execute(Event event) override;
};

class YoggSaronBrainSpotAction : public MovementAction
{
public:
    YoggSaronBrainSpotAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron brain spot action") {}

    bool Execute(Event event) override;
};

class YoggSaronMoveToExitPortalAction : public MovementAction
{
public:
    YoggSaronMoveToExitPortalAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron move to exit portal action") {}

    bool Execute(Event event) override;
};

// Face away from the Laughing Skulls in the bot's front arc. The skull cannot be killed and its gaze
// picks targets by facing alone, so this is the only defence the room has.
class YoggSaronLaughingSkullAction : public MovementAction
{
public:
    YoggSaronLaughingSkullAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron laughing skull action") {}

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

// The tank holds a stack spot and taunts loose Guardians onto it. Phase 3 does it so the raid cleaves
// Immortal Guardians to Weakened for Thorim's Titanic Storm; phase 1 does it because a Guardian's
// death location is the phase, and one that dies out among the casters neither hurts Sara nor spares
// the back line.
class YoggSaronGuardianControlAction : public MovementAction
{
public:
    YoggSaronGuardianControlAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron guardian control action") {}

    bool Execute(Event event) override;

private:
    bool ControlPhaseOne();
    bool Taunt(Unit* guardian);
};

// Reduced-Keeper hard mode: retreat to a safe ranged spot and face away from Yogg to conserve sanity.
class YoggSaronSanityConservationAction : public MovementAction
{
public:
    YoggSaronSanityConservationAction(PlayerbotAI* ai) : MovementAction(ai, "yogg-saron sanity conservation action") {}

    bool Execute(Event event) override;
};

// Hand of Protection on somebody else's Constrictor Tentacle. The main tank is rescued like anyone
// else: he is riding a vehicle and holding nothing while he is held, so the threat wipe costs less
// than the grip does. Forbearance blocking his own Divine Shield and Lay on Hands for two minutes
// afterwards is the price, and it is deliberate.
class YoggSaronSqueezeRescueAction : public Action
{
public:
    YoggSaronSqueezeRescueAction(PlayerbotAI* ai) : Action(ai, "yogg-saron squeeze rescue action") {}

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
