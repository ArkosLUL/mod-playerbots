#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONS_H

#include <functional>
#include <vector>

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

enum class ToCFactionChampions : uint32
{
    NPC_ALLIANCE_DRUID_RESTORATION   = 34469,
    NPC_ALLIANCE_SHAMAN_RESTORATION  = 34470,
    NPC_ALLIANCE_PALADIN_HOLY        = 34465,
    NPC_ALLIANCE_PRIEST_DISCIPLINE   = 34466,
    NPC_HORDE_DRUID_RESTORATION      = 34459,
    NPC_HORDE_SHAMAN_RESTORATION     = 34444,
    NPC_HORDE_PALADIN_HOLY           = 34445,
    NPC_HORDE_PRIEST_DISCIPLINE      = 34447,

    NPC_ALLIANCE_DEATH_KNIGHT        = 34461,
    NPC_ALLIANCE_DRUID_BALANCE       = 34460,
    NPC_ALLIANCE_HUNTER              = 34467,
    NPC_ALLIANCE_MAGE                = 34468,
    NPC_ALLIANCE_PALADIN_RETRIBUTION = 34471,
    NPC_ALLIANCE_PRIEST_SHADOW       = 34473,
    NPC_ALLIANCE_ROGUE               = 34472,
    NPC_ALLIANCE_SHAMAN_ENHANCEMENT  = 34463,
    NPC_ALLIANCE_WARLOCK             = 34474,
    NPC_ALLIANCE_WARRIOR             = 34475,
    NPC_HORDE_DEATH_KNIGHT           = 34458,
    NPC_HORDE_DRUID_BALANCE          = 34451,
    NPC_HORDE_HUNTER                 = 34448,
    NPC_HORDE_MAGE                   = 34449,
    NPC_HORDE_PALADIN_RETRIBUTION    = 34456,
    NPC_HORDE_PRIEST_SHADOW          = 34441,
    NPC_HORDE_ROGUE                  = 34454,
    NPC_HORDE_SHAMAN_ENHANCEMENT     = 34455,
    NPC_HORDE_WARLOCK                = 34450,
    NPC_HORDE_WARRIOR                = 34453,
};

inline constexpr uint32 SPELL_COUNTERSPELL = 2139;
inline constexpr int32 LATE_INTERRUPT_MS = 600;
inline constexpr uint32 SPELL_CYCLONE = 33786;
inline constexpr uint32 SPELL_PVP_TRINKET = 65547;

bool IsFactionChampion(uint32 entry);

bool IsFactionChampionHealer(uint32 entry);

bool IsFactionChampionMelee(uint32 entry);

float FactionChampionsSwingReach(Unit* champion, Unit* victim);

Unit* FactionChampionsKillTarget(PlayerbotAI* botAI);

Unit* FactionChampionsSuspendedTarget(PlayerbotAI* botAI);

Unit* FactionChampionsNextTarget(PlayerbotAI* botAI, std::function<bool(Unit*)> const& accept);

std::vector<Unit*> FactionChampionsInCombat(PlayerbotAI* botAI);

bool FactionChampionsFocusBot(PlayerbotAI* botAI);

bool FactionChampionsIsPackTank(Player* player);

Unit* FactionChampionsIceTarget(PlayerbotAI* botAI);

bool FactionChampionsHasIceDuty(PlayerbotAI* botAI);

ObjectGuid FactionChampionsIceDutyOf(Player* member);

bool FactionChampionsIced(PlayerbotAI* botAI, Unit* champion);

bool FactionChampionsTrinketReady(Unit* champion);

bool FactionChampionsPack(PlayerbotAI* botAI, Position& anchor, Position& centre);

bool FactionChampionsIsPulled(PlayerbotAI* botAI, Unit* champion);

bool FactionChampionsKillTargetOut(PlayerbotAI* botAI);

bool FactionChampionsIsEscapee(PlayerbotAI* botAI, Unit* champion);

bool FactionChampionsFearWindowActive(PlayerbotAI* botAI);

Unit* FactionChampionsCcTarget(PlayerbotAI* botAI, uint32& spellId);

bool FactionChampionsHasCcDuty(PlayerbotAI* botAI, ObjectGuid member);

bool FactionChampionsCcProtected(PlayerbotAI* botAI, Unit* unit);

bool FactionChampionsHeldByBreakableCc(Unit* unit);

bool FactionChampionsBreaksOnDamage(uint32 spellId);

bool FactionChampionsCarriesGroupDot(Unit* unit);

bool FactionChampionsCounterspellDuty(PlayerbotAI* botAI);

bool FactionChampionsCounterspellHold(PlayerbotAI* botAI);

bool FactionChampionsCycloneBot(Player* player);

Unit* FactionChampionsBlobHealer(PlayerbotAI* botAI);

Unit* FactionChampionsDangerTarget(PlayerbotAI* botAI);

}

#endif
