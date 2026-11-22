#ifndef PLAYERBOTS_RAID_TOCENCOUNTERGATE_H
#define PLAYERBOTS_RAID_TOCENCOUNTERGATE_H

#include <string>

#include "Define.h"
#include "RaidEncounter.h"

class PlayerbotAI;

enum ToCInstanceData : uint32
{
    TOC_DATA_INSTANCE_PROGRESS = 1,
    TOC_DATA_GORMOK = 5,
    TOC_DATA_DREADSCALE = 6,
    TOC_DATA_ACIDMAW = 7,
    TOC_DATA_ICEHOWL = 8,
    TOC_DATA_JARAXXUS = 10,
    TOC_DATA_FJOLA = 11,
    TOC_DATA_EYDIS = 12,
    TOC_DATA_ANUBARAK = 14,
};

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

ToCEncounter ToCEncounterOfStage(uint32 stage);

bool ToCEncounterGateOpen(PlayerbotAI* botAI, ToCEncounter encounter);

bool ToCEncounterIsLive(PlayerbotAI* botAI, ToCEncounter encounter);

ToCEncounter ToCLiveEncounter(PlayerbotAI* botAI);

bool ToCGateOpen(PlayerbotAI* botAI, uint32 encounterId);
bool ToCGateLive(PlayerbotAI* botAI, uint32 encounterId);
inline constexpr EncounterGate ToCGate{&ToCGateOpen, &ToCGateLive};

bool ToCRaidWideGateOpen(PlayerbotAI* botAI, uint32 encounterId);
bool ToCRaidWideGateLive(PlayerbotAI* botAI, uint32 encounterId);
inline constexpr EncounterGate ToCRaidWideGate{&ToCRaidWideGateOpen, &ToCRaidWideGateLive};

bool ToCEncounterOfName(std::string const& name, ToCEncounter& encounter);

#endif
