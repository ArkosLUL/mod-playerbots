#ifndef PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONS_H

#include "NamedObjectContext.h"
#include "RaidAntiFear.h"
#include "ToCActions_Shared.h"

class FactionChampionsMarkTargetsAction : public Action
{
public:
    FactionChampionsMarkTargetsAction(PlayerbotAI* botAI) : Action(botAI, "faction champions mark targets") {}
    bool Execute(Event event) override;
};

class FactionChampionsAntiFearAction : public RaidAntiFearAction
{
public:
    FactionChampionsAntiFearAction(PlayerbotAI* botAI) : RaidAntiFearAction(botAI, "faction champions anti fear") {}

protected:
    bool FearWindowActive() override;
};

class FactionChampionsFocusPriorityAction : public AttackAction
{
public:
    FactionChampionsFocusPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, "faction champions focus priority") {}
    bool Execute(Event event) override;
};

class FactionChampionsSetCcIconAction : public Action
{
public:
    FactionChampionsSetCcIconAction(PlayerbotAI* botAI) : Action(botAI, "faction champions set cc icon") {}
    bool Execute(Event event) override;
};

class FactionChampionsCounterspellKillTargetAction : public Action
{
public:
    FactionChampionsCounterspellKillTargetAction(
        PlayerbotAI* botAI) : Action(botAI, "faction champions counterspell kill target") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ToCRestoreRtiCcAction : public Action
{
public:
    ToCRestoreRtiCcAction(PlayerbotAI* botAI) : Action(botAI, "toc restore rti cc") {}
    bool Execute(Event event) override;
};

class ToCFactionChampionsActionContext : public NamedObjectContext<Action>
{
public:
    ToCFactionChampionsActionContext()
    {
        creators["faction champions mark targets"] =
            &ToCFactionChampionsActionContext::faction_champions_mark_targets;
        creators["faction champions anti fear"] =
            &ToCFactionChampionsActionContext::faction_champions_anti_fear;
        creators["faction champions focus priority"] =
            &ToCFactionChampionsActionContext::faction_champions_focus_priority;
        creators["faction champions set cc icon"] =
            &ToCFactionChampionsActionContext::faction_champions_set_cc_icon;
        creators["faction champions counterspell kill target"] =
            &ToCFactionChampionsActionContext::faction_champions_counterspell_kill_target;
        creators["toc restore rti cc"] =
            &ToCFactionChampionsActionContext::toc_restore_rti_cc;
    }

private:
    static Action* faction_champions_mark_targets(PlayerbotAI* botAI) {
        return new FactionChampionsMarkTargetsAction(botAI);
    }

    static Action* faction_champions_anti_fear(PlayerbotAI* botAI) {
        return new FactionChampionsAntiFearAction(botAI);
    }

    static Action* faction_champions_focus_priority(PlayerbotAI* botAI) {
        return new FactionChampionsFocusPriorityAction(botAI);
    }

    static Action* faction_champions_set_cc_icon(PlayerbotAI* botAI) {
        return new FactionChampionsSetCcIconAction(botAI);
    }

    static Action* faction_champions_counterspell_kill_target(PlayerbotAI* botAI) {
        return new FactionChampionsCounterspellKillTargetAction(botAI);
    }

    static Action* toc_restore_rti_cc(PlayerbotAI* botAI) {
        return new ToCRestoreRtiCcAction(botAI);
    }
};

#endif
