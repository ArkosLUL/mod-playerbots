#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONSDEFENCE_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONSDEFENCE_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class FactionChampionsAvoidAoeTrigger : public Trigger
{
public:
    FactionChampionsAvoidAoeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions avoid aoe") {}
    bool IsActive() override;
};

class FactionChampionsMassDispelTrigger : public Trigger
{
public:
    FactionChampionsMassDispelTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions mass dispel") {}
    bool IsActive() override;
};

class FactionChampionsDispelCcTrigger : public Trigger
{
public:
    FactionChampionsDispelCcTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions dispel cc") {}
    bool IsActive() override;
};

class FactionChampionsPhysicalSwitchTrigger : public Trigger
{
public:
    FactionChampionsPhysicalSwitchTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "faction champions physical switch") {}
    bool IsActive() override;
};

class FactionChampionsPurgeKillTargetTrigger : public Trigger
{
public:
    FactionChampionsPurgeKillTargetTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "faction champions purge kill target") {}
    bool IsActive() override;
};

class ToCFactionChampionsDefenceTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCFactionChampionsDefenceTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["faction champions avoid aoe"] =
            &ToCFactionChampionsDefenceTriggerContext::faction_champions_avoid_aoe;
        creators["faction champions mass dispel"] =
            &ToCFactionChampionsDefenceTriggerContext::faction_champions_mass_dispel;
        creators["faction champions dispel cc"] =
            &ToCFactionChampionsDefenceTriggerContext::faction_champions_dispel_cc;
        creators["faction champions physical switch"] =
            &ToCFactionChampionsDefenceTriggerContext::faction_champions_physical_switch;
        creators["faction champions purge kill target"] =
            &ToCFactionChampionsDefenceTriggerContext::faction_champions_purge_kill_target;
    }

private:
    static Trigger* faction_champions_avoid_aoe(PlayerbotAI* botAI) {
        return new FactionChampionsAvoidAoeTrigger(botAI);
    }

    static Trigger* faction_champions_mass_dispel(PlayerbotAI* botAI) {
        return new FactionChampionsMassDispelTrigger(botAI);
    }

    static Trigger* faction_champions_dispel_cc(PlayerbotAI* botAI) {
        return new FactionChampionsDispelCcTrigger(botAI);
    }

    static Trigger* faction_champions_physical_switch(PlayerbotAI* botAI) {
        return new FactionChampionsPhysicalSwitchTrigger(botAI);
    }

    static Trigger* faction_champions_purge_kill_target(PlayerbotAI* botAI) {
        return new FactionChampionsPurgeKillTargetTrigger(botAI);
    }
};

void AddToCFactionChampionsDefenceTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
