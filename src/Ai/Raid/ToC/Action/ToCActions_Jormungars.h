#ifndef PLAYERBOTS_RAID_TOCACTIONS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCACTIONS_JORMUNGARS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidRedirectThreat.h"
#include "ToCActions_NorthrendBeasts.h"
#include "ToCHelpers_NorthrendBeasts.h"

class NorthrendWormsTankHoldAction : public AttackAction
{
public:
    bool Execute(Event event) override;

protected:
    NorthrendWormsTankHoldAction(PlayerbotAI* botAI, std::string const name,
                                 TrialOfTheCrusaderHelpers::BeastsTankDuty holdDuty)
        : AttackAction(botAI, name), _duty(holdDuty) {}

private:
    enum class WalkKind : uint8
    {
        None,
        Approach,
        Drag
    };

    bool ApproachSubmerged(Unit* worm);
    bool DragOffHazards(Unit* worm);
    Position const* WalkInFlight(WalkKind kind);
    bool WalkTo(WalkKind kind, Position const& spot);
    bool BookedOnWalkSpot();

    TrialOfTheCrusaderHelpers::BeastsTankDuty const _duty;
    Position _walkSpot;
    Position _approachFrom;
    uint32 _walkIssuedMs = 0;
    WalkKind _walking = WalkKind::None;
};

class NorthrendWormsTankHoldMobileWormAction : public NorthrendWormsTankHoldAction
{
public:
    static constexpr char const* Name = "northrend worms tank hold mobile worm";

    NorthrendWormsTankHoldMobileWormAction(PlayerbotAI* botAI)
        : NorthrendWormsTankHoldAction(botAI, Name, TrialOfTheCrusaderHelpers::BeastsTankDuty::WormMobile) {}
};

class NorthrendWormsTankHoldStationaryWormAction : public NorthrendWormsTankHoldAction
{
public:
    static constexpr char const* Name = "northrend worms tank hold stationary worm";

    NorthrendWormsTankHoldStationaryWormAction(PlayerbotAI* botAI)
        : NorthrendWormsTankHoldAction(botAI, Name, TrialOfTheCrusaderHelpers::BeastsTankDuty::WormStationary) {}
};

class NorthrendWormsRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "northrend worms redirect threat";

    NorthrendWormsRedirectThreatAction(PlayerbotAI* botAI) : RaidRedirectThreatAction(botAI, Name) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

class NorthrendWormsRepositionAction : public MovementAction
{
public:
    static constexpr char const* Name = "northrend worms reposition";

    NorthrendWormsRepositionAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    Position _issuedSpot;
    bool _hasIssuedSpot = false;
};

class NorthrendWormsLeavePoolAction : public MovementAction
{
public:
    static constexpr char const* Name = "northrend worms leave pool";

    NorthrendWormsLeavePoolAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    Position _exitSpot;
    uint32 _exitMs = 0;
    bool _hasExit = false;
};

class NorthrendWormsHealToxinCarrierAction : public Action
{
public:
    static constexpr char const* Name = "northrend worms heal toxin carrier";

    NorthrendWormsHealToxinCarrierAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class NorthrendWormsPurgeToxinAction : public Action
{
public:
    static constexpr char const* Name = "northrend worms purge toxin";

    NorthrendWormsPurgeToxinAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
