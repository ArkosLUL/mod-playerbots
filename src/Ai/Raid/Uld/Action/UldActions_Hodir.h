#ifndef PLAYERBOTS_ULDACTIONS_HODIR_H
#define PLAYERBOTS_ULDACTIONS_HODIR_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidRedirectThreat.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class HodirMoveSnowpackedIcicleAction : public MovementAction
{
public:
    static constexpr char const* Name = "hodir move snowpacked icicle";

    HodirMoveSnowpackedIcicleAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class HodirFrostResistanceAction : public Action
{
public:
    static constexpr char const* Name = "hodir frost resistance action";

    HodirFrostResistanceAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class HodirIcicleDodgeAction : public MovementAction
{
public:
    static constexpr char const* Name = "hodir icicle dodge action";

    HodirIcicleDodgeAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    Position _dest;
    float _destDist = 0.0f;
};

class HodirBitingColdShedAction : public MovementAction
{
public:
    static constexpr char const* Name = "hodir biting cold shed";

    HodirBitingColdShedAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    void StopShedding();

    bool _shedding = false;
    Position _leg;
    float _legDist = 0.0f;
};

class HodirRaidPositionAction : public MovementAction
{
public:
    static constexpr char const* Name = "hodir raid position action";

    HodirRaidPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class HodirSetDpsPriorityAction : public AttackAction
{
public:
    static constexpr char const* Name = "hodir set dps priority action";

    HodirSetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    Unit* ResolveTarget();
};

class HodirFrozenBlowsSwapAction : public AttackAction
{
public:
    static constexpr char const* Name = "hodir frozen blows swap action";

    HodirFrozenBlowsSwapAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class HodirTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "hodir tank defensive action";

    HodirTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class HodirRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "hodir redirect threat action";

    HodirRedirectThreatAction(PlayerbotAI* botAI) : RaidRedirectThreatAction(botAI, Name) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

class HodirSpreadStormCloudAction : public MovementAction
{
public:
    static constexpr char const* Name = "hodir spread storm cloud";

    HodirSpreadStormCloudAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class HodirCollectStormPowerAction : public MovementAction
{
public:
    static constexpr char const* Name = "hodir collect storm power";

    HodirCollectStormPowerAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
