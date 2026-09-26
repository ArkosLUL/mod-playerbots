#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONS_H

#include <vector>

#include "NamedObjectContext.h"
#include "RaidAntiFear.h"
#include "Trigger.h"

class FactionChampionsMarkTargetsTrigger : public Trigger
{
public:
    FactionChampionsMarkTargetsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions mark targets") {}
    bool IsActive() override;
};

class FactionChampionsAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    FactionChampionsAntiFearTrigger(PlayerbotAI* botAI) : RaidAntiFearTrigger(botAI, "faction champions anti fear") {}

protected:
    bool FearWindowActive() override;
};

class FactionChampionsShouldFocusTrigger : public Trigger
{
public:
    FactionChampionsShouldFocusTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions should focus") {}
    bool IsActive() override;
};

class FactionChampionsCcIconTrigger : public Trigger
{
public:
    FactionChampionsCcIconTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions cc icon") {}
    bool IsActive() override;
};

class FactionChampionsKillTargetHealingTrigger : public Trigger
{
public:
    FactionChampionsKillTargetHealingTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "faction champions kill target healing") {}
    bool IsActive() override;
};

class ToCRestoreRtiCcTrigger : public Trigger
{
public:
    ToCRestoreRtiCcTrigger(PlayerbotAI* botAI) : Trigger(botAI, "toc restore rti cc") {}
    bool IsActive() override;
};

class ToCFactionChampionsTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCFactionChampionsTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["faction champions mark targets"] =
            &ToCFactionChampionsTriggerContext::faction_champions_mark_targets;
        creators["faction champions anti fear"] =
            &ToCFactionChampionsTriggerContext::faction_champions_anti_fear;
        creators["faction champions should focus"] =
            &ToCFactionChampionsTriggerContext::faction_champions_should_focus;
        creators["faction champions cc icon"] =
            &ToCFactionChampionsTriggerContext::faction_champions_cc_icon;
        creators["faction champions kill target healing"] =
            &ToCFactionChampionsTriggerContext::faction_champions_kill_target_healing;
        creators["toc restore rti cc"] =
            &ToCFactionChampionsTriggerContext::toc_restore_rti_cc;
    }

private:
    static Trigger* faction_champions_mark_targets(PlayerbotAI* botAI) {
        return new FactionChampionsMarkTargetsTrigger(botAI);
    }

    static Trigger* faction_champions_anti_fear(PlayerbotAI* botAI) {
        return new FactionChampionsAntiFearTrigger(botAI);
    }

    static Trigger* faction_champions_should_focus(PlayerbotAI* botAI) {
        return new FactionChampionsShouldFocusTrigger(botAI);
    }

    static Trigger* faction_champions_cc_icon(PlayerbotAI* botAI) {
        return new FactionChampionsCcIconTrigger(botAI);
    }

    static Trigger* faction_champions_kill_target_healing(PlayerbotAI* botAI) {
        return new FactionChampionsKillTargetHealingTrigger(botAI);
    }

    static Trigger* toc_restore_rti_cc(PlayerbotAI* botAI) {
        return new ToCRestoreRtiCcTrigger(botAI);
    }
};

void AddToCFactionChampionsTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
