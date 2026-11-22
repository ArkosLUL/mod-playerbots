/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_LICHKING_H
#define PLAYERBOTS_ICCHELPERS_LICHKING_H

#include <vector>

#include "Define.h"
#include "ICCData.h"
#include "Position.h"
#include "RaidBurstWindow.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

constexpr uint32 LK_DATA_THE_LICH_KING = 12;
constexpr uint32 LK_DATA_ARTHAS_PLATFORM = 38;

constexpr uint32 NPC_LK_DRUDGE_GHOUL = 37695;
constexpr uint32 NPC_LK_SHAMBLING_HORROR = 37698;
constexpr uint32 NPC_LK_RAGING_SPIRIT = 36701;
constexpr uint32 NPC_LK_ICE_SPHERE = 36633;
constexpr uint32 NPC_LK_SHADOW_TRAP = 39137;
constexpr uint32 NPC_LK_VALKYR = 36609;
constexpr uint32 NPC_LK_VILE_SPIRIT = 37799;
constexpr uint32 NPC_LK_WICKED_SPIRIT = 39190;
constexpr uint32 NPC_LK_SPIRIT_WARDEN = 36824;

constexpr uint32 SPELL_LK_NECROTIC_PLAGUE = 70337;
constexpr uint32 SPELL_LK_NECROTIC_PLAGUE_JUMP = 70338;
constexpr uint32 SPELL_LK_SOUL_REAPER = 69409;
constexpr uint32 SPELL_LK_SOUL_SHRIEK = 69242;
constexpr uint32 SPELL_LK_SHAMBLING_ENRAGE = 72143;
constexpr uint32 SPELL_LK_PLAGUE_SIPHON = 74074;
constexpr uint32 SPELL_LK_SHAMBLING_FRENZY = 28747;
constexpr uint32 SPELL_LK_TRANQUILIZING_SHOT = 19801;
constexpr uint32 SPELL_LK_SIT = 73220;

inline Position const LK_CENTER(503.63f, -2124.66f, 840.857f);
inline Position const LK_FROSTMOURNE(495.71f, -2523.76f, 1049.95f);

inline constexpr uint32 ICC_LK_FIRST_WINTER[]  = {68981, 74270, 74271, 74272};
inline constexpr uint32 ICC_LK_SECOND_WINTER[] = {72259, 74273, 74274, 74275};
inline constexpr uint32 ICC_LK_HARVEST_SOUL[]  = {68980, 74325, 74326, 74327};
inline constexpr uint32 ICC_LK_HARVEST_SOULS[] = {73654, 74295, 74296, 74297};

enum class IccLichKingPhaseId : uint8
{
    One,
    Winter1,
    Two,
    Winter2,
    Three,
    Frostmourne,
    Outro,
};

IccLichKingPhaseId IccLichKingPhase(Player* bot, Unit* boss);
bool IccLichKingInFrostmourne(Unit const* unit);

uint8 IccRemorselessWinterOf(Unit* boss);
bool IccBossHasRemorselessWinter(Unit* boss);
bool IccBossCastingRemorselessWinter(Unit* boss);

Creature* GetLkBoss(PlayerbotAI* botAI);
bool IsLkLive(PlayerbotAI* botAI);

bool IsLkRingStandable(Player* bot);
bool LkFloorAccepts(Player* bot, float x, float y);

enum class LkTankRole : uint8
{
    None = 0,
    Holder = 1,
    Free = 2,
    StandIn = 3
};

LkTankRole GetLkTankRole(PlayerbotAI* botAI, Player* player);
Player* GetLkHolder(PlayerbotAI* botAI);
Player* GetLkFreeTank(PlayerbotAI* botAI);

std::vector<Player*> LkMembersOnMap(Player* bot);
bool IsLkDutyBot(Player* player);
bool IsLkTankPlayer(Player* player);
bool IsLkHealer(Player* player);

uint32 LkReadyStun(Player* member, Unit* target);
uint32 LkReadySlow(Player* member, Unit* target);
bool LkCastControl(PlayerbotAI* botAI, Unit* target, uint32 spellId);

Unit* GetLkTarget(PlayerbotAI* botAI);
bool IsLkTargetHeld(PlayerbotAI* botAI);
bool IsLkMovementOwned(PlayerbotAI* botAI);
bool IsLkTankDefensiveHeld(PlayerbotAI* botAI);

bool GetLkQuakeWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkTrapWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkPlagueWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkStandWalk(PlayerbotAI* botAI, Position& spot);
bool IsLkStandUrgent(PlayerbotAI* botAI);

uint32 GetLkTankDefensive(PlayerbotAI* botAI, char const* noteKind);
Unit* GetLkTauntTarget(PlayerbotAI* botAI, uint32& aoeSpell);
Unit* GetLkCureTarget(PlayerbotAI* botAI, uint32& spellId);
Unit* GetLkTranquilizeTarget(PlayerbotAI* botAI);
Unit* GetLkStunTarget(PlayerbotAI* botAI, uint32& spellId);
Unit* GetLkSilenceDispelTarget(PlayerbotAI* botAI, uint32& spellId);
bool IsLkBraked(PlayerbotAI* botAI);
bool IsLkCureHeld(PlayerbotAI* botAI);
char const* GetLkTotemSwap(PlayerbotAI* botAI);
bool IsLkPlagueCarrier(Unit* unit);
void GetLkSplashAdds(PlayerbotAI* botAI, std::vector<Position>& out);

void TickLkEncounter(PlayerbotAI* botAI);

RaidBurstWindow IccLichKingBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetLkOpenerBosses(PlayerbotAI* botAI);
uint8 IccLichKingResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
