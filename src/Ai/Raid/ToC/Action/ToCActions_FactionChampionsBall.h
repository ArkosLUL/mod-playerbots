#ifndef PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "RaidDirectTargeting.h"
#include "ToCActions_NorthrendBeasts.h"

class FactionChampionsHoldAction : public RaidDirectAttackAction
{
public:
    FactionChampionsHoldAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, "faction champions hold") {}
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

// A bot's hop away from the melee champions on it, till they re-pick someone else
class FactionChampionsKiteAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsKiteAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, "faction champions kite") {}
    bool Execute(Event event) override;
};

// Sprint or Dash: no cast time and no GCD, so the kite's walk goes on
class FactionChampionsSpeedBoostAction : public Action
{
public:
    FactionChampionsSpeedBoostAction(PlayerbotAI* botAI) : Action(botAI, "faction champions speed boost") {}
    bool Execute(Event event) override;
};

// The AoE fear or Frost Nova that makes the champions burn their trinket
class FactionChampionsOpenerAction : public Action
{
public:
    FactionChampionsOpenerAction(PlayerbotAI* botAI) : Action(botAI, "faction champions opener") {}
    bool Execute(Event event) override;
};

// A caster's Nova, Blink, circle teleport or fear off the melee champions on it
class FactionChampionsEscapeAction : public Action
{
public:
    FactionChampionsEscapeAction(PlayerbotAI* botAI) : Action(botAI, "faction champions escape") {}
    bool Execute(Event event) override;
};

// The warlock's circle on its staging stand, for the teleport back once a melee champion reaches it
class FactionChampionsDemonicCircleAction : public Action
{
public:
    FactionChampionsDemonicCircleAction(PlayerbotAI* botAI) : Action(botAI, "faction champions demonic circle") {}
    bool Execute(Event event) override;
};

class ToCFactionChampionsBallActionContext : public NamedObjectContext<Action>
{
public:
    ToCFactionChampionsBallActionContext()
    {
        creators["faction champions escape"] = &ToCFactionChampionsBallActionContext::faction_champions_escape;
        creators["faction champions demonic circle"] =
            &ToCFactionChampionsBallActionContext::faction_champions_demonic_circle;
        creators["faction champions hold"] = &ToCFactionChampionsBallActionContext::faction_champions_hold;
        creators["faction champions hold control"] =
            &ToCFactionChampionsBallActionContext::faction_champions_hold_control;
        creators["faction champions cyclone stand"] =
            &ToCFactionChampionsBallActionContext::faction_champions_cyclone_stand;
        creators["faction champions step off"] = &ToCFactionChampionsBallActionContext::faction_champions_step_off;
        creators["faction champions opener"] = &ToCFactionChampionsBallActionContext::faction_champions_opener;
        creators["faction champions chased"] = &ToCFactionChampionsBallActionContext::faction_champions_chased;
        creators["faction champions kite"] = &ToCFactionChampionsBallActionContext::faction_champions_kite;
        creators["faction champions speed boost"] =
            &ToCFactionChampionsBallActionContext::faction_champions_speed_boost;
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

    static Action* faction_champions_kite(PlayerbotAI* botAI) { return new FactionChampionsKiteAction(botAI); }

    static Action* faction_champions_speed_boost(PlayerbotAI* botAI)
    {
        return new FactionChampionsSpeedBoostAction(botAI);
    }

    static Action* faction_champions_escape(PlayerbotAI* botAI) { return new FactionChampionsEscapeAction(botAI); }

    static Action* faction_champions_demonic_circle(PlayerbotAI* botAI)
    {
        return new FactionChampionsDemonicCircleAction(botAI);
    }
};

#endif
