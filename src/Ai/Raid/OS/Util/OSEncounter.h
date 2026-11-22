/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_OSENCOUNTER_H
#define _PLAYERBOT_OSENCOUNTER_H

#include "OSData.h"
#include "PlayerbotAI.h"
#include "RaidTankDefensive.h"
#include <string>

class Player;
class Unit;

namespace OsHelpers
{

Unit* GetSartharion(Player* bot);
bool SartharionEncounterActive(Player* bot);

struct SartharionSnapshot
{
    Unit* boss = nullptr;
    bool encounterActive = false;
    bool dodgeLive = false;
};
SartharionSnapshot const& SartharionSnapshotFor(PlayerbotAI* botAI);

void TickSartharionObs(Player* bot);
bool SartharionDamageImmune(Player* bot);
uint32 EncounterElapsedMs(Player* bot);

bool HasTwilightShift(Unit const* unit);
bool TwilightRealmWorthEntering(Player* bot);
bool HasMoltenFury(Unit const* unit);

bool OnThePlatform(Player* bot);

bool NeedsPlatformReturn(PlayerbotAI* botAI, Player* bot);
bool NeedsTsunamiDodge(Player* bot);
bool NeedsFissureDodge(Player* bot);

Player* GetOffTank(PlayerbotAI* botAI, Player* bot);
bool IsOffTank(Player* bot);
bool RequireOffTank(PlayerbotAI* botAI, Player* bot);
Player* RedirectTarget(PlayerbotAI* botAI, Player* bot);
Player* RedirectTankFor(PlayerbotAI* botAI, Player* bot);
Unit* OffTankTauntTarget(Player* bot);
char const* TauntSpellFor(Player* bot);

constexpr size_t PORTAL_SQUAD_HEALERS = 2;

bool PortalSquadMember(Player* bot);
bool TwilightAddsAlive(Player* bot);

Unit* LandedShadron(Player* bot);
bool ShadronGone(Player* bot);

bool SartharionBurstWindowOpen(Player* bot);

bool MainTankCooldownWindowOpen(Player* bot);

bool MainTankDragDone(Unit* boss);
void SetMainTankDragged(Unit* boss);
uint32 MainTankDragArrivedMs(Unit* boss);
void SetMainTankDragArrivedMs(Unit* boss, uint32 arrivedMs);
uint32 MainTankDragStartedMs(Unit* boss);
void SetMainTankDragStartedMs(Unit* boss, uint32 startedMs);

Unit* PriorityTarget(Player* bot);
Unit* TranquilizeTargetFor(Player* bot);

}

#endif
