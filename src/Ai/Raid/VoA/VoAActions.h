#ifndef PLAYERBOTS_VOAACTIONS_H
#define PLAYERBOTS_VOAACTIONS_H

#include "Action.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Event.h"

//
//  Emalon the Storm Watcher
//

class EmalonMarkBossAction : public MovementAction
{
public:
    EmalonMarkBossAction(PlayerbotAI* botAI) : MovementAction(botAI, "emalon mark boss action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class EmalonLightingNovaAction : public MovementAction
{
public:
    EmalonLightingNovaAction(PlayerbotAI* botAI) : MovementAction(botAI, "emalon lighting nova action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class EmalonOverchargeAction : public Action
{
public:
    EmalonOverchargeAction(PlayerbotAI* botAI) : Action(botAI, "emalon overcharge action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class EmalonFallFromFloorAction : public Action
{
public:
    EmalonFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, "emalon fall from floor action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

//
//  Archavon the Stone Watcher
//

class ArchavonMarkBossAction : public MovementAction
{
public:
    ArchavonMarkBossAction(PlayerbotAI* botAI) : MovementAction(botAI, "archavon mark boss action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ArchavonRockShardsSpreadAction : public MovementAction
{
public:
    ArchavonRockShardsSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "archavon rock shards spread action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

//
//  Koralon the Flame Watcher
//

class KoralonMarkBossAction : public MovementAction
{
public:
    KoralonMarkBossAction(PlayerbotAI* botAI) : MovementAction(botAI, "koralon mark boss action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KoralonBurningBreathAction : public MovementAction
{
public:
    KoralonBurningBreathAction(PlayerbotAI* botAI) : MovementAction(botAI, "koralon burning breath action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KoralonFlamingCinderSpreadAction : public MovementAction
{
public:
    KoralonFlamingCinderSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "koralon flaming cinder spread action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
