#ifndef PLAYERBOTS_RAID_TOCACTIONS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCACTIONS_JORMUNGARS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "Position.h"
#include "RaidRedirectThreat.h"
#include "ToCHelpers_NorthrendBeasts.h"

// Holds the worm of one duty through its submerges. Its walks are latched: every MoveTo clears the
// MotionMaster, so a spot re-derived each tick from a bot that's still walking never lands.
class NorthrendWormsTankHoldAction : public AttackAction
{
public:
    bool Execute(Event event) override;

protected:
    NorthrendWormsTankHoldAction(PlayerbotAI* botAI, std::string const name,
                                 TrialOfTheCrusaderHelpers::BeastsTankDuty holdDuty)
        : AttackAction(botAI, name), duty(holdDuty) {};

private:
    enum class WalkKind : uint8
    {
        None,
        Approach,
        Drag
    };

    bool ApproachSubmerged(Unit* worm);
    bool DragOffHazards(Unit* worm);
    // The latched spot while the bot is still walking a walk of this kind, else nullptr
    Position const* WalkInFlight(WalkKind kind);
    // False without booking anything while a cast pins the bot's feet
    bool WalkTo(WalkKind kind, Position const& spot);
    bool BookedOnWalkSpot();

    TrialOfTheCrusaderHelpers::BeastsTankDuty const duty;
    Position walkSpot;
    Position approachFrom;  // where the worm stood when the approach was issued
    uint32 walkIssuedMs = 0;
    WalkKind walking = WalkKind::None;
};

class NorthrendWormsTankHoldMobileWormAction : public NorthrendWormsTankHoldAction
{
public:
    NorthrendWormsTankHoldMobileWormAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms tank hold mobile worm")
        : NorthrendWormsTankHoldAction(botAI, name, TrialOfTheCrusaderHelpers::BeastsTankDuty::WormMobile) {};
};

class NorthrendWormsTankHoldStationaryWormAction : public NorthrendWormsTankHoldAction
{
public:
    NorthrendWormsTankHoldStationaryWormAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms tank hold stationary worm")
        : NorthrendWormsTankHoldAction(botAI, name, TrialOfTheCrusaderHelpers::BeastsTankDuty::WormStationary) {};
};

class NorthrendWormsRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    NorthrendWormsRedirectThreatAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms redirect threat")
        : RaidRedirectThreatAction(botAI, name) {};

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

class NorthrendWormsRepositionAction : public MovementAction
{
public:
    NorthrendWormsRepositionAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms reposition") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCJormungarsActionContext : public NamedObjectContext<Action>
{
public:
    ToCJormungarsActionContext()
    {
        creators["northrend worms tank hold mobile worm"] =
            &ToCJormungarsActionContext::northrend_worms_tank_hold_mobile_worm;
        creators["northrend worms tank hold stationary worm"] =
            &ToCJormungarsActionContext::northrend_worms_tank_hold_stationary_worm;
        creators["northrend worms redirect threat"] =
            &ToCJormungarsActionContext::northrend_worms_redirect_threat;
        creators["northrend worms reposition"] =
            &ToCJormungarsActionContext::northrend_worms_reposition;
    }

private:
    static Action* northrend_worms_tank_hold_mobile_worm(PlayerbotAI* botAI) {
        return new NorthrendWormsTankHoldMobileWormAction(botAI);
    }

    static Action* northrend_worms_tank_hold_stationary_worm(PlayerbotAI* botAI) {
        return new NorthrendWormsTankHoldStationaryWormAction(botAI);
    }

    static Action* northrend_worms_redirect_threat(PlayerbotAI* botAI) {
        return new NorthrendWormsRedirectThreatAction(botAI);
    }

    static Action* northrend_worms_reposition(PlayerbotAI* botAI) {
        return new NorthrendWormsRepositionAction(botAI);
    }
};

#endif
