#ifndef PLAYERBOTS_RAID_TOCACTIONS_GORMOK_H
#define PLAYERBOTS_RAID_TOCACTIONS_GORMOK_H

#include "Action.h"
#include "AttackAction.h"
#include "NamedObjectContext.h"
#include "ToCActions_Shared.h"

class GormokMainTankHoldBossAction : public ToCMainTankHoldAction
{
public:
    GormokMainTankHoldBossAction(PlayerbotAI* botAI, std::string const name = "gormok main tank hold boss")
        : ToCMainTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;
};

class GormokFocusSnoboldAction : public AttackAction
{
public:
    GormokFocusSnoboldAction(
        PlayerbotAI* botAI, std::string const name = "gormok focus snobold") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class GormokTankSwapTauntAction : public AttackAction
{
public:
    GormokTankSwapTauntAction(
        PlayerbotAI* botAI, std::string const name = "gormok tank swap taunt") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCGormokActionContext : public NamedObjectContext<Action>
{
public:
    ToCGormokActionContext()
    {
        creators["gormok main tank hold boss"] =
            &ToCGormokActionContext::gormok_main_tank_hold_boss;
        creators["gormok focus snobold"] =
            &ToCGormokActionContext::gormok_focus_snobold;
        creators["gormok tank swap taunt"] =
            &ToCGormokActionContext::gormok_tank_swap_taunt;
    }

private:
    static Action* gormok_main_tank_hold_boss(PlayerbotAI* botAI) {
        return new GormokMainTankHoldBossAction(botAI);
    }

    static Action* gormok_focus_snobold(PlayerbotAI* botAI) {
        return new GormokFocusSnoboldAction(botAI);
    }

    static Action* gormok_tank_swap_taunt(PlayerbotAI* botAI) {
        return new GormokTankSwapTauntAction(botAI);
    }
};

#endif
