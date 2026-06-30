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

// Lord Jaraxxus

class JaraxxusMainTankHoldBossAction : public AttackAction
{
public:
    JaraxxusMainTankHoldBossAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus main tank hold boss") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusAssistTankHoldAddAction : public AttackAction
{
public:
    JaraxxusAssistTankHoldAddAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus assist tank hold add") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusAssistTankHoldSecondAddAction : public AttackAction
{
public:
    JaraxxusAssistTankHoldSecondAddAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus assist tank hold second add") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusFocusAddAction : public AttackAction
{
public:
    JaraxxusFocusAddAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus focus add") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusAvoidLegionFlameAction : public MovementAction
{
public:
    JaraxxusAvoidLegionFlameAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus avoid legion flame") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusHealIncinerateTargetAction : public Action
{
public:
    JaraxxusHealIncinerateTargetAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus heal incinerate target") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusRemoveNetherPowerAction : public Action
{
public:
    JaraxxusRemoveNetherPowerAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus remove nether power") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusInterruptFelFireballAction : public AttackAction
{
public:
    JaraxxusInterruptFelFireballAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus interrupt fel fireball") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

// Anub'arak

class AnubarakMainTankHoldBossAction : public AttackAction
{
public:
    AnubarakMainTankHoldBossAction(
        PlayerbotAI* botAI, std::string const name = "anubarak main tank hold boss") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakAssistTankHoldBurrowerAction : public AttackAction
{
public:
    AnubarakAssistTankHoldBurrowerAction(
        PlayerbotAI* botAI, std::string const name = "anubarak assist tank hold burrower") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakFocusBurrowerAction : public AttackAction
{
public:
    AnubarakFocusBurrowerAction(
        PlayerbotAI* botAI, std::string const name = "anubarak focus burrower") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakFocusScarabAction : public AttackAction
{
public:
    AnubarakFocusScarabAction(
        PlayerbotAI* botAI, std::string const name = "anubarak focus scarab") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakKiteSpikeToPermafrostAction : public MovementAction
{
public:
    AnubarakKiteSpikeToPermafrostAction(
        PlayerbotAI* botAI, std::string const name = "anubarak kite spike to permafrost") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakDestroyFrostSphereAction : public AttackAction
{
public:
    AnubarakDestroyFrostSphereAction(
        PlayerbotAI* botAI, std::string const name = "anubarak destroy frost sphere") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

// Faction Champions

class FactionChampionsFocusPriorityAction : public AttackAction
{
public:
    FactionChampionsFocusPriorityAction(
        PlayerbotAI* botAI, std::string const name = "faction champions focus priority") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

#endif
