#ifndef PLAYERBOTS_TOCACTIONS_H
#define PLAYERBOTS_TOCACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"

// Gormok the Impaler

class GormokMainTankHoldBossAction : public AttackAction
{
public:
    GormokMainTankHoldBossAction(
        PlayerbotAI* botAI, std::string const name = "gormok main tank hold boss") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class GormokFocusSnoboldAction : public AttackAction
{
public:
    GormokFocusSnoboldAction(
        PlayerbotAI* botAI, std::string const name = "gormok focus snobold") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

// Acidmaw & Dreadscale

class WormsMainTankHoldMobileWormAction : public AttackAction
{
public:
    WormsMainTankHoldMobileWormAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms main tank hold mobile worm") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class WormsAssistTankHoldStationaryWormAction : public AttackAction
{
public:
    WormsAssistTankHoldStationaryWormAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms assist tank hold stationary worm") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class WormsSpreadAction : public MovementAction
{
public:
    WormsSpreadAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms spread") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class WormsKeepMovingAction : public MovementAction
{
public:
    WormsKeepMovingAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms keep moving") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

// Icehowl

class IcehowlMainTankHoldBossAction : public AttackAction
{
public:
    IcehowlMainTankHoldBossAction(
        PlayerbotAI* botAI, std::string const name = "icehowl main tank hold boss") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class IcehowlClearChargePathAction : public MovementAction
{
public:
    IcehowlClearChargePathAction(
        PlayerbotAI* botAI, std::string const name = "icehowl clear charge path") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

#endif
