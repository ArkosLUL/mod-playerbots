#ifndef PLAYERBOTS_RAID_TOCENCOUNTERGATE_H
#define PLAYERBOTS_RAID_TOCENCOUNTERGATE_H

#include <string>
#include <vector>

#include "Define.h"
#include "Trigger.h"

class PlayerbotAI;

// Mirrors trial_of_the_crusader.h (DataTypes, Progress, NPCs), which lives in the core's scripts and
// can't be included from here. The script keys the twins' guids by NPC entry, not by a data type.
enum ToCInstanceData : uint32
{
    TOC_DATA_INSTANCE_PROGRESS = 1,
    TOC_DATA_GORMOK = 4,
    TOC_DATA_DREADSCALE = 6,
    TOC_DATA_ACIDMAW = 7,
    TOC_DATA_ANUBARAK = 13,
    TOC_DATA_FJOLA = 34497,  // NPC_LIGHTBANE
    TOC_DATA_EYDIS = 34496,  // NPC_DARKBANE
};

// GetData(TOC_DATA_INSTANCE_PROGRESS). Never moves back on a wipe.
enum ToCInstanceProgress : uint32
{
    TOC_PROGRESS_INITIAL = 0,
    TOC_PROGRESS_INTRO_DONE = 1,
    TOC_PROGRESS_BEASTS_DEAD = 2,
    TOC_PROGRESS_JARAXXUS_INTRO_DONE = 3,
    TOC_PROGRESS_JARAXXUS_DEAD = 4,
    TOC_PROGRESS_FACTION_CHAMPIONS_DEAD = 6,
    TOC_PROGRESS_VALKYR_DEAD = 8,
    TOC_PROGRESS_ANUB_ARAK = 9,
    TOC_PROGRESS_DONE = 10,
};

enum class ToCEncounter : uint8
{
    None,
    NorthrendBeasts,
    Jaraxxus,
    FactionChampions,
    TwinValkyr,
    Anubarak
};

// Open for the whole stage, pulled or not, so pre-pull prep like taking an essence before the twins
// still runs. Beasts and Jaraxxus also stay open the stage after their kill until the next
// encounter is live, for their leftover adds. True off a ToC instance: ungated beats silently dead.
bool ToCEncounterGateOpen(PlayerbotAI* botAI, ToCEncounter encounter);

// Stage matches, IsEncounterInProgress, and one of its units alive and in combat. The script flags
// most encounters in progress before the pull (walk-ins, the Lich King scene), so the unit check is
// what keeps those closed. False off a ToC instance.
bool ToCEncounterIsLive(PlayerbotAI* botAI, ToCEncounter encounter);

// None between pulls and off a ToC instance.
ToCEncounter ToCLiveEncounter(PlayerbotAI* botAI);

// False for a name with no encounter prefix, which then stays ungated.
bool ToCEncounterOfTrigger(std::string const& triggerName, ToCEncounter& encounter);

// Trace slug the readers expect. Null for None.
char const* ToCEncounterSlug(ToCEncounter encounter);

// Wraps where the context builds each trigger, so no stem trigger class needs editing.
class ToCGatedTrigger : public Trigger
{
public:
    ToCGatedTrigger(PlayerbotAI* botAI, Trigger* inner, ToCEncounter encounter);
    ~ToCGatedTrigger() override;

    Event Check() override;
    bool IsActive() override;
    bool IsBuffTrigger() override;
    bool IsDebuffTrigger() override;
    std::vector<NextAction> getHandlers() override;
    void Reset() override;
    Unit* GetTarget() override;
    Value<Unit*>* GetTargetValue() override;
    std::string const GetTargetName() override;
    void ExternalEvent(std::string const param, Player* owner = nullptr) override;
    void ExternalEvent(WorldPacket& packet, Player* owner = nullptr) override;

private:
    Trigger* inner;
    ToCEncounter encounter;
};

#endif
