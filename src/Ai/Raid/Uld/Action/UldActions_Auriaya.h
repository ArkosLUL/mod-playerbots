#ifndef PLAYERBOTS_ULDACTIONS_AURIAYA_H
#define PLAYERBOTS_ULDACTIONS_AURIAYA_H

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

class AuriayaFallFromFloorAction : public Action
{
public:
    AuriayaFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, "auriaya fall from floor action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuriayaSonicScreechAction : public MovementAction
{
public:
    AuriayaSonicScreechAction(PlayerbotAI* botAI) : MovementAction(botAI, "auriaya sonic screech action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuriayaSeepingEssenceAction : public MoveAwayFromCreatureAction
{
public:
    AuriayaSeepingEssenceAction(PlayerbotAI* botAI)
        : MoveAwayFromCreatureAction(botAI, "auriaya seeping essence action", NPC_AURIAYA_SEEPING_FERAL_ESSENCE,
                                     ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS)
    {
    }
};

class AuriayaMarkDpsTargetAction : public Action
{
public:
    AuriayaMarkDpsTargetAction(PlayerbotAI* botAI) : Action(botAI, "auriaya mark dps target action") {}
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

class AuriayaTankFacingAction : public MovementAction
{
public:
    AuriayaTankFacingAction(PlayerbotAI* botAI) : MovementAction(botAI, "auriaya tank facing action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AuriayaAntiFearAction : public RaidAntiFearAction
{
public:
    AuriayaAntiFearAction(PlayerbotAI* botAI) : RaidAntiFearAction(botAI, "auriaya anti fear action") {}

protected:
    bool FearWindowActive() override { return AuriayaFearWindowActive(botAI); }
};

#endif
