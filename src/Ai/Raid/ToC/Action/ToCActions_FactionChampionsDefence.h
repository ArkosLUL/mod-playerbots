#ifndef PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSDEFENCE_H
#define PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSDEFENCE_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "Position.h"

// Walks out of a Bladestorm or Hellfire, the two champion AoEs a bot can see coming
class FactionChampionsAvoidAoeAction : public MovementAction
{
public:
    FactionChampionsAvoidAoeAction(PlayerbotAI* botAI) : MovementAction(botAI, "faction champions avoid aoe") {}
    bool Execute(Event event) override;

private:
    bool IssueLeg(Position const& spot, uint32 now);

    Position _spot;
    Position _lastIssued;
    bool _hasSpot = false;
    uint32 _issuedMs = 0;
};

class FactionChampionsMassDispelAction : public Action
{
public:
    FactionChampionsMassDispelAction(PlayerbotAI* botAI) : Action(botAI, "faction champions mass dispel") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FactionChampionsDispelCcAction : public Action
{
public:
    FactionChampionsDispelCcAction(PlayerbotAI* botAI) : Action(botAI, "faction champions dispel cc") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FactionChampionsPhysicalSwitchAction : public AttackAction
{
public:
    FactionChampionsPhysicalSwitchAction(
        PlayerbotAI* botAI) : AttackAction(botAI, "faction champions physical switch") {}
    bool Execute(Event event) override;
};

class FactionChampionsPurgeKillTargetAction : public Action
{
public:
    FactionChampionsPurgeKillTargetAction(
        PlayerbotAI* botAI) : Action(botAI, "faction champions purge kill target") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ToCFactionChampionsDefenceActionContext : public NamedObjectContext<Action>
{
public:
    ToCFactionChampionsDefenceActionContext()
    {
        creators["faction champions avoid aoe"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_avoid_aoe;
        creators["faction champions mass dispel"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_mass_dispel;
        creators["faction champions dispel cc"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_dispel_cc;
        creators["faction champions physical switch"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_physical_switch;
        creators["faction champions purge kill target"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_purge_kill_target;
    }

private:
    static Action* faction_champions_avoid_aoe(PlayerbotAI* botAI) {
        return new FactionChampionsAvoidAoeAction(botAI);
    }

    static Action* faction_champions_mass_dispel(PlayerbotAI* botAI) {
        return new FactionChampionsMassDispelAction(botAI);
    }

    static Action* faction_champions_dispel_cc(PlayerbotAI* botAI) {
        return new FactionChampionsDispelCcAction(botAI);
    }

    static Action* faction_champions_physical_switch(PlayerbotAI* botAI) {
        return new FactionChampionsPhysicalSwitchAction(botAI);
    }

    static Action* faction_champions_purge_kill_target(PlayerbotAI* botAI) {
        return new FactionChampionsPurgeKillTargetAction(botAI);
    }
};

#endif
