#ifndef PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSDEFENCE_H
#define PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSDEFENCE_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "Position.h"
#include "ToCActions_NorthrendBeasts.h"
#include "ToCActions_Shared.h"

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

class FactionChampionsKillTotemAction : public ToCDirectAttackAction
{
public:
    FactionChampionsKillTotemAction(
        PlayerbotAI* botAI) : ToCDirectAttackAction(botAI, "faction champions kill totem") {}
    bool Execute(Event event) override;
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

// Walks the DK to its latched stand, then grips the kill target to it
class FactionChampionsDeathGripAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsDeathGripAction(
        PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, "faction champions death grip") {}
    bool Execute(Event event) override;
};

class FactionChampionsSnareAction : public Action
{
public:
    FactionChampionsSnareAction(PlayerbotAI* botAI) : Action(botAI, "faction champions snare") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FactionChampionsProtectAction : public Action
{
public:
    FactionChampionsProtectAction(PlayerbotAI* botAI) : Action(botAI, "faction champions protect") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FactionChampionsBattleResAction : public Action
{
public:
    FactionChampionsBattleResAction(PlayerbotAI* botAI) : Action(botAI, "faction champions battle res") {}
    bool Execute(Event event) override;
    bool isUseful() override;
    std::vector<NextAction> getPrerequisites() override;
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
        creators["faction champions kill totem"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_kill_totem;
        creators["faction champions physical switch"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_physical_switch;
        creators["faction champions purge kill target"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_purge_kill_target;
        creators["faction champions death grip"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_death_grip;
        creators["faction champions snare"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_snare;
        creators["faction champions protect"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_protect;
        creators["faction champions battle res"] =
            &ToCFactionChampionsDefenceActionContext::faction_champions_battle_res;
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

    static Action* faction_champions_kill_totem(PlayerbotAI* botAI) {
        return new FactionChampionsKillTotemAction(botAI);
    }

    static Action* faction_champions_physical_switch(PlayerbotAI* botAI) {
        return new FactionChampionsPhysicalSwitchAction(botAI);
    }

    static Action* faction_champions_purge_kill_target(PlayerbotAI* botAI) {
        return new FactionChampionsPurgeKillTargetAction(botAI);
    }

    static Action* faction_champions_death_grip(PlayerbotAI* botAI) {
        return new FactionChampionsDeathGripAction(botAI);
    }

    static Action* faction_champions_snare(PlayerbotAI* botAI) {
        return new FactionChampionsSnareAction(botAI);
    }

    static Action* faction_champions_protect(PlayerbotAI* botAI) {
        return new FactionChampionsProtectAction(botAI);
    }

    static Action* faction_champions_battle_res(PlayerbotAI* botAI) {
        return new FactionChampionsBattleResAction(botAI);
    }
};

#endif
