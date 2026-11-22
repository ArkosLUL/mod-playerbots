#ifndef PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H

#include "Action.h"
#include "RaidDirectTargeting.h"
#include "ToCActions_NorthrendBeasts.h"

class FactionChampionsIceTargetAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "faction champions ice target";

    FactionChampionsIceTargetAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class FactionChampionsChainsOfIceAction : public Action
{
public:
    static constexpr char const* Name = "faction champions chains of ice";

    FactionChampionsChainsOfIceAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class FactionChampionsIceReachAction : public NorthrendBeastsWalkAction
{
public:
    static constexpr char const* Name = "faction champions ice reach";

    FactionChampionsIceReachAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class FactionChampionsCycloneStandAction : public NorthrendBeastsWalkAction
{
public:
    static constexpr char const* Name = "faction champions cyclone stand";

    FactionChampionsCycloneStandAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class FactionChampionsStepOffAction : public NorthrendBeastsWalkAction
{
public:
    static constexpr char const* Name = "faction champions step off";

    FactionChampionsStepOffAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class FactionChampionsChasedAction : public NorthrendBeastsWalkAction
{
public:
    static constexpr char const* Name = "faction champions chased";

    FactionChampionsChasedAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class FactionChampionsKiteAction : public NorthrendBeastsWalkAction
{
public:
    static constexpr char const* Name = "faction champions kite";

    FactionChampionsKiteAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    Position _hop;
};

class FactionChampionsSpeedBoostAction : public Action
{
public:
    static constexpr char const* Name = "faction champions speed boost";

    FactionChampionsSpeedBoostAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class FactionChampionsOpenerAction : public Action
{
public:
    static constexpr char const* Name = "faction champions opener";

    FactionChampionsOpenerAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class FactionChampionsEscapeAction : public Action
{
public:
    static constexpr char const* Name = "faction champions escape";

    FactionChampionsEscapeAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class FactionChampionsDemonicCircleAction : public Action
{
public:
    static constexpr char const* Name = "faction champions demonic circle";

    FactionChampionsDemonicCircleAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
