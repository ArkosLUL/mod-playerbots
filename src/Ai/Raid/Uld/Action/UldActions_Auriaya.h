#ifndef PLAYERBOTS_ULDACTIONS_AURIAYA_H
#define PLAYERBOTS_ULDACTIONS_AURIAYA_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidAntiFear.h"
#include "UldEncounter_Auriaya.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class AuriayaFallFromFloorAction : public Action
{
public:
    static constexpr char const* Name = "auriaya fall from floor action";

    AuriayaFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Steps just far enough to clear a pool and no further, leashed to the bot's anchor. The generic
// MoveAwayFromCreatureAction maximises distance from the nearest pool instead, which walks bots out
// of the room once the pools have piled up.
class AuriayaSeepingEssenceAction : public MovementAction
{
public:
    static constexpr char const* Name = "auriaya seeping essence action";

    AuriayaSeepingEssenceAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuriayaSentryTauntAction : public Action
{
public:
    static constexpr char const* Name = "auriaya sentry taunt action";

    AuriayaSentryTauntAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuriayaRaidPositionAction : public MovementAction
{
public:
    static constexpr char const* Name = "auriaya raid position action";

    AuriayaRaidPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuriayaSetDpsPriorityAction : public AttackAction
{
public:
    static constexpr char const* Name = "auriaya set dps priority action";

    AuriayaSetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool IsAllowedPriorityTarget(Unit* boss, Unit* candidate);
};

class AuriayaAntiFearAction : public RaidAntiFearAction
{
public:
    static constexpr char const* Name = "auriaya anti fear action";

    AuriayaAntiFearAction(PlayerbotAI* botAI) : RaidAntiFearAction(botAI, Name) {}

protected:
    bool FearWindowActive() override { return AuriayaFearWindowActive(botAI); }
};

#endif
