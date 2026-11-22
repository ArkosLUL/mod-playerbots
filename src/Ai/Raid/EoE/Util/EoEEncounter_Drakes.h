/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEENCOUNTER_DRAKES_H
#define PLAYERBOTS_EOEENCOUNTER_DRAKES_H

#include "EoEData.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Unit.h"

#include <vector>

void GetStaticFields(Player* bot, std::vector<Unit*>& fields);

bool IsClearOfStaticFields(float x, float y, std::vector<Unit*> const& fields, float safeRadius);

bool GetDrakeStackPoint(Player* bot, std::vector<Unit*> const& fields, float& x, float& y, float& z);

void GetDrakeApproachPoint(Unit* drake, Unit* boss, float destX, float destY, float& x, float& y);

void GetDrakeHealerGuids(PlayerbotAI* botAI, std::vector<ObjectGuid>& out);

bool IsDrakeHealer(PlayerbotAI* botAI, std::vector<ObjectGuid> const& healers);

void GetDrakeFlightAndHealerRank(PlayerbotAI* botAI, std::vector<ObjectGuid> const& healers,
    std::vector<Unit*>& drakes, uint8& rank);

bool DrakeCanAfford(Unit* drake, uint32 spellId);

bool DrakeCanAffordWithShield(Unit* drake, uint32 spellId);

uint32 DrakeAuraRemainingMs(Unit* drake, uint32 spellId);

bool IsDrakeSurgeTarget(PlayerbotAI* botAI);

bool GetDrakeSurgeElapsedMs(PlayerbotAI* botAI, uint32& elapsedMs);

#endif
