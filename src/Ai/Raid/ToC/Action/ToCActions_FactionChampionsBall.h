#ifndef PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "ToCActions_NorthrendBeasts.h"
#include "ToCActions_Shared.h"

class FactionChampionsHoldAction : public ToCDirectAttackAction
{
public:
    FactionChampionsHoldAction(PlayerbotAI* botAI) : ToCDirectAttackAction(botAI, "faction champions hold") {}
    bool Execute(Event event) override;
};

class FactionChampionsHoldControlAction : public Action
{
public:
    FactionChampionsHoldControlAction(PlayerbotAI* botAI) : Action(botAI, "faction champions hold control") {}
    bool Execute(Event event) override;
};

class FactionChampionsCycloneStandAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsCycloneStandAction(PlayerbotAI* botAI)
        : NorthrendBeastsWalkAction(botAI, "faction champions cyclone stand") {}
    bool Execute(Event event) override;
};

// A healer's walk off the melee champions next to it
class FactionChampionsStepOffAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsStepOffAction(PlayerbotAI* botAI)
        : NorthrendBeastsWalkAction(botAI, "faction champions step off") {}
    bool Execute(Event event) override;
};

// Boosts away from the champions focusing it
class FactionChampionsChasedAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsChasedAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, "faction champions chased") {}
    bool Execute(Event event) override;
};

// The AoE fear or Frost Nova that makes the champions burn their trinket
class FactionChampionsOpenerAction : public Action
{
public:
    FactionChampionsOpenerAction(PlayerbotAI* botAI) : Action(botAI, "faction champions opener") {}
    bool Execute(Event event) override;
};

class ToCFactionChampionsBallActionContext : public NamedObjectContext<Action>
{
public:
    ToCFactionChampionsBallActionContext()
    {
        creators["faction champions hold"] = &ToCFactionChampionsBallActionContext::faction_champions_hold;
        creators["faction champions hold control"] =
            &ToCFactionChampionsBallActionContext::faction_champions_hold_control;
        creators["faction champions cyclone stand"] =
            &ToCFactionChampionsBallActionContext::faction_champions_cyclone_stand;
        creators["faction champions step off"] = &ToCFactionChampionsBallActionContext::faction_champions_step_off;
        creators["faction champions opener"] = &ToCFactionChampionsBallActionContext::faction_champions_opener;
        creators["faction champions chased"] = &ToCFactionChampionsBallActionContext::faction_champions_chased;
    }

private:
    static Action* faction_champions_hold(PlayerbotAI* botAI) { return new FactionChampionsHoldAction(botAI); }

    static Action* faction_champions_hold_control(PlayerbotAI* botAI)
    {
        return new FactionChampionsHoldControlAction(botAI);
    }

    static Action* faction_champions_cyclone_stand(PlayerbotAI* botAI)
    {
        return new FactionChampionsCycloneStandAction(botAI);
    }

    static Action* faction_champions_step_off(PlayerbotAI* botAI) { return new FactionChampionsStepOffAction(botAI); }

    static Action* faction_champions_opener(PlayerbotAI* botAI) { return new FactionChampionsOpenerAction(botAI); }

    static Action* faction_champions_chased(PlayerbotAI* botAI) { return new FactionChampionsChasedAction(botAI); }
};

#endif
