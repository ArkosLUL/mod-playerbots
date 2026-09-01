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
    AuriayaFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, "auriaya fall from floor action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Steps just far enough to clear a pool and no further, leashed to the bot's anchor. The generic
// MoveAwayFromCreatureAction maximises distance from the nearest pool instead, which walks bots out
// of the room once the pools have piled up.
class AuriayaSeepingEssenceAction : public MovementAction
{
public:
    AuriayaSeepingEssenceAction(PlayerbotAI* botAI) : MovementAction(botAI, "auriaya seeping essence action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuriayaSentryTauntAction : public Action
{
public:
    AuriayaSentryTauntAction(PlayerbotAI* botAI) : Action(botAI, "auriaya sentry taunt action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuriayaRaidPositionAction : public MovementAction
{
public:
    AuriayaRaidPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "auriaya raid position action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuriayaSetDpsPriorityAction : public AttackAction
{
public:
    AuriayaSetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, "auriaya set dps priority action") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool IsAllowedPriorityTarget(Unit* boss, Unit* candidate);
};

class AuriayaAntiFearAction : public RaidAntiFearAction
{
public:
    AuriayaAntiFearAction(PlayerbotAI* botAI) : RaidAntiFearAction(botAI, "auriaya anti fear action") {}

protected:
    bool FearWindowActive() override { return AuriayaFearWindowActive(botAI); }
};

#endif
