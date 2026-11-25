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
#include <unordered_map>
#include <vector>

class YoggSaronGuardianPositioningAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron guardian positioning action";

    YoggSaronGuardianPositioningAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronSanityAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron sanity action";

    YoggSaronSanityAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronMaladyOfTheMindAction : public MoveAwayFromPlayerWithDebuffAction
{
public:
    static constexpr char const* Name = "yogg-saron malady of the mind action";

    YoggSaronMaladyOfTheMindAction(PlayerbotAI* botAI)
        : MoveAwayFromPlayerWithDebuffAction(botAI, Name, SPELL_MALADY_OF_THE_MIND,
                                             ULDUAR_YOGG_SARON_MALADY_SPREAD_RADIUS)
    {
    }
};

class YoggSaronSpacingAction : public MovementAction
{
public:
    YoggSaronSpacingAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

    bool Execute(Event event) override;

protected:
    struct HazardSet
    {
        std::vector<EncounterHelpers::HazardCircle> hazards;

        std::vector<EncounterHelpers::HazardCircle> fallback;
        std::function<bool(float, float)> fallbackClear;

        std::function<bool(float, float)> clear;

        std::function<bool(float, float)> preferred;
    };

    virtual bool Collect(HazardSet& set) = 0;
    virtual float SearchRadius() const = 0;

    virtual float MaxFromMiddle() const { return ULDUAR_YOGG_SARON_SPACING_MAX_FROM_MIDDLE; }

    virtual Position Anchor() const { return ULDUAR_YOGG_SARON_MIDDLE; }

    virtual bool RouteAcceptable(float /*x*/, float /*y*/) const { return true; }

    virtual bool OverridesWalkInFlight() const { return false; }

    virtual bool HeldRouteAcceptable(float /*x*/, float /*y*/) const { return true; }

private:
    Position heldSpot;
    uint32 heldSpotMs = 0;
};

class YoggSaronPhase1SpacingAction : public YoggSaronSpacingAction
{
public:
    static constexpr char const* Name = "yogg-saron phase 1 spacing action";

    YoggSaronPhase1SpacingAction(PlayerbotAI* botAI) : YoggSaronSpacingAction(botAI, Name) {}

protected:
    bool Collect(HazardSet& set) override;
    float SearchRadius() const override { return ULDUAR_YOGG_SARON_SPACING_SEARCH_RADIUS; }
    bool RouteAcceptable(float x, float y) const override;

private:
    std::vector<Position> clouds;
};

class YoggSaronPhase1StationAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron phase 1 station action";

    YoggSaronPhase1StationAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronPhase2SpacingAction : public YoggSaronSpacingAction
{
public:
    static constexpr char const* Name = "yogg-saron phase 2 spacing action";

    YoggSaronPhase2SpacingAction(PlayerbotAI* botAI) : YoggSaronSpacingAction(botAI, Name) {}

protected:
    bool Collect(HazardSet& set) override;
    float SearchRadius() const override { return ULDUAR_YOGG_SARON_P2_SPACING_SEARCH_RADIUS; }
    float MaxFromMiddle() const override { return ULDUAR_YOGG_SARON_P2_SPACING_MAX_FROM_MIDDLE; }
    bool RouteAcceptable(float x, float y) const override;
    bool OverridesWalkInFlight() const override { return inDeathRay; }
    bool HeldRouteAcceptable(float x, float y) const override;

private:
    bool inDeathRay = false;

    std::vector<Position> rays;
};

class YoggSaronIllusionFacingAction : public YoggSaronSpacingAction
{
public:
    static constexpr char const* Name = "yogg-saron illusion facing action";

    YoggSaronIllusionFacingAction(PlayerbotAI* botAI)
        : YoggSaronSpacingAction(botAI, Name)
    {
    }

protected:
    bool Collect(HazardSet& set) override;
    float SearchRadius() const override { return ULDUAR_YOGG_SARON_ILLUSION_FACING_SEARCH_RADIUS; }
    float MaxFromMiddle() const override { return ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS; }
    Position Anchor() const override { return roomMiddle; }

private:
    Position roomMiddle;
};

class YoggSaronBodyDetourAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron body detour action";

    YoggSaronBodyDetourAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronPetGuardAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron pet guard action";

    YoggSaronPetGuardAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;

private:
    void Hush(Creature* pet);

    void Unhush(Creature* pet);

    std::unordered_map<ObjectGuid, ReactStates> hushed;
};

class YoggSaronSetDpsPriorityAction : public AttackAction
{
public:
    static constexpr char const* Name = "yogg-saron set dps priority action";

    YoggSaronSetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;

private:
    static size_t TierOf(Unit* unit, bool brainLevel, bool phaseThreeRanged = false, bool beaconHealFirst = false);

    struct BrainApproach
    {
        bool brainLevel = false;
        bool phaseThreeRanged = false;
        bool burn = false;
        std::optional<bool> approachable;
        std::optional<bool> phaseThreeMelee;
        std::optional<bool> thorim;
        std::optional<bool> gaze;
    };

    bool IsAllowedTarget(Unit* candidate, BrainApproach& brain) const;
    Unit* ResolveTarget(Unit* currentTarget);

    Unit* SpreadTarget(std::vector<Unit*> const& candidates, Unit* currentTarget, BrainApproach& brain);

    void DropTarget(Unit* target, bool interruptCasts = true);
};

class YoggSaronDarkVolleyInterruptAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron dark volley interrupt action";

    YoggSaronDarkVolleyInterruptAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;

private:
    bool CastClassInterrupt(Unit* target);
    int32 GetInterrupterIndex();
};

class YoggSaronDiminishPowerJudgementAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron diminish power judgement action";

    YoggSaronDiminishPowerJudgementAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronDiminishPowerBreakAction : public AttackAction
{
public:
    static constexpr char const* Name = "yogg-saron diminish power break action";

    YoggSaronDiminishPowerBreakAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronMortalStrikeAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron mortal strike action";

    YoggSaronMortalStrikeAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronWarlockPetAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron warlock pet action";

    YoggSaronWarlockPetAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronPhase3ControlAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 control action";

    YoggSaronPhase3ControlAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronBrainLinkAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron brain link action";

    YoggSaronBrainLinkAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronMoveToEnterPortalAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron move to enter portal action";

    YoggSaronMoveToEnterPortalAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronFallFromFloorAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron fall from floor action";

    YoggSaronFallFromFloorAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronStopFollowingAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron stop following action";

    YoggSaronStopFollowingAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronUsePortalAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron use portal action";

    YoggSaronUsePortalAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronIllusionRoomAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron illusion room action";

    YoggSaronIllusionRoomAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;

private:
    bool SetRtiMark(YoggSaronTrigger yoggSaronTrigger);
    bool GoToBrainRoom(YoggSaronTrigger yoggSaronTrigger);
    bool WalkIntoRoom();
    bool WalkToStage();
};

class YoggSaronIllusionHealerStationAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron illusion healer station action";

    YoggSaronIllusionHealerStationAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronReprisalRegroupAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron reprisal regroup action";

    YoggSaronReprisalRegroupAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronReprisalHealAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron reprisal heal action";

    YoggSaronReprisalHealAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronBrainSpotAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron brain spot action";

    YoggSaronBrainSpotAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronNitroBoostsAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron nitro boosts action";

    YoggSaronNitroBoostsAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronBrainDashAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron brain dash action";

    YoggSaronBrainDashAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronBrainChargeAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron brain charge action";

    YoggSaronBrainChargeAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronMoveToExitPortalAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron move to exit portal action";

    YoggSaronMoveToExitPortalAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronLaughingSkullAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron laughing skull action";

    YoggSaronLaughingSkullAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronLunaticGazeAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron lunatic gaze action";

    YoggSaronLunaticGazeAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronBeaconDragAction : public AttackAction
{
public:
    static constexpr char const* Name = "yogg-saron beacon drag action";

    YoggSaronBeaconDragAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;

private:
    bool HunterDrag(Unit* guardian);
    bool ReturnHealed();
};

class YoggSaronBeaconStageAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron beacon stage action";

    YoggSaronBeaconStageAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronBeaconBearFormAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron beacon bear form action";

    YoggSaronBeaconBearFormAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronPhase3LustAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 lust action";

    YoggSaronPhase3LustAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronPhase3YoggDotAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 yogg dot action";

    YoggSaronPhase3YoggDotAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronPhase3PositioningAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 positioning action";

    YoggSaronPhase3PositioningAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronGuardianControlAction : public AttackAction
{
public:
    static constexpr char const* Name = "yogg-saron guardian control action";

    YoggSaronGuardianControlAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;

private:
    bool ControlPhaseOne();
    bool ControlPhaseThree();
    bool Taunt(Unit* guardian);
    bool Defend(Unit* guardian);
    bool TakeFrom(Player* holder);
};

class YoggSaronSanityConservationAction : public MovementAction
{
public:
    static constexpr char const* Name = "yogg-saron sanity conservation action";

    YoggSaronSanityConservationAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronSqueezeRescueAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron squeeze rescue action";

    YoggSaronSqueezeRescueAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronNovaDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron nova defensive action";

    YoggSaronNovaDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronNovaExternalAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron nova external action";

    YoggSaronNovaExternalAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;

    static char const* PickExternal(PlayerbotAI* botAI, Player* victim);

private:
    static constexpr char const* HEALER_EXTERNALS[] = {"pain suppression", "guardian spirit",
                                                       "hand of sacrifice"};
};

class YoggSaronRoarAuraAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron roar aura action";

    YoggSaronRoarAuraAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronRoarAuraMasteryAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron roar aura mastery action";

    YoggSaronRoarAuraMasteryAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronBeaconDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron beacon defensive action";

    YoggSaronBeaconDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronBeaconExternalAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron beacon external action";

    YoggSaronBeaconExternalAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronTankLayOnHandsAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron tank lay on hands action";

    YoggSaronTankLayOnHandsAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronVigilanceAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron vigilance action";

    YoggSaronVigilanceAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronTankPileFacingAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron tank pile facing action";

    YoggSaronTankPileFacingAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronHealerCombatEngineAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron healer combat engine action";

    YoggSaronHealerCombatEngineAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronShadowAuraAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron shadow aura action";

    YoggSaronShadowAuraAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronNovaRaidCooldownAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron nova raid cooldown action";

    YoggSaronNovaRaidCooldownAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronSqueezeEscapeAction : public Action
{
public:
    static constexpr char const* Name = "yogg-saron squeeze escape action";

    YoggSaronSqueezeEscapeAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class YoggSaronAntiFearAction : public RaidAntiFearAction
{
public:
    static constexpr char const* Name = "yogg-saron anti fear action";

    YoggSaronAntiFearAction(PlayerbotAI* botAI) : RaidAntiFearAction(botAI, Name) {}

protected:
    bool FearWindowActive() override { return YoggSaronFearWindowActive(botAI); }
};

#endif
