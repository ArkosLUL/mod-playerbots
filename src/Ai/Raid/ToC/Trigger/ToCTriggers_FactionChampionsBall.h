#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONSBALL_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class FactionChampionsHoldTrigger : public Trigger
{
public:
    FactionChampionsHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions hold") {}
    bool IsActive() override;
};

class FactionChampionsHoldWalkTrigger : public Trigger
{
public:
    FactionChampionsHoldWalkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions hold walk") {}
    bool IsActive() override;
};

class FactionChampionsHoldControlTrigger : public Trigger
{
public:
    FactionChampionsHoldControlTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions hold control") {}
    bool IsActive() override;
};

class FactionChampionsBallStandTrigger : public Trigger
{
public:
    FactionChampionsBallStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions ball stand") {}
    bool IsActive() override;
};

class FactionChampionsChasedTrigger : public Trigger
{
public:
    FactionChampionsChasedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions chased") {}
    bool IsActive() override;
};

class FactionChampionsOpenerTrigger : public Trigger
{
public:
    FactionChampionsOpenerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions opener") {}
    bool IsActive() override;
};

class ToCFactionChampionsBallTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCFactionChampionsBallTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["faction champions hold"] = &ToCFactionChampionsBallTriggerContext::faction_champions_hold;
        creators["faction champions hold walk"] = &ToCFactionChampionsBallTriggerContext::faction_champions_hold_walk;
        creators["faction champions hold control"] =
            &ToCFactionChampionsBallTriggerContext::faction_champions_hold_control;
        creators["faction champions ball stand"] = &ToCFactionChampionsBallTriggerContext::faction_champions_ball_stand;
        creators["faction champions opener"] = &ToCFactionChampionsBallTriggerContext::faction_champions_opener;
        creators["faction champions chased"] = &ToCFactionChampionsBallTriggerContext::faction_champions_chased;
    }

private:
    static Trigger* faction_champions_hold(PlayerbotAI* botAI) { return new FactionChampionsHoldTrigger(botAI); }

    static Trigger* faction_champions_hold_walk(PlayerbotAI* botAI)
    {
        return new FactionChampionsHoldWalkTrigger(botAI);
    }

    static Trigger* faction_champions_hold_control(PlayerbotAI* botAI)
    {
        return new FactionChampionsHoldControlTrigger(botAI);
    }

    static Trigger* faction_champions_ball_stand(PlayerbotAI* botAI) {
        return new FactionChampionsBallStandTrigger(botAI);
    }

    static Trigger* faction_champions_opener(PlayerbotAI* botAI) { return new FactionChampionsOpenerTrigger(botAI); }

    static Trigger* faction_champions_chased(PlayerbotAI* botAI) { return new FactionChampionsChasedTrigger(botAI); }
};

void AddToCFactionChampionsBallTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
