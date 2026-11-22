/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEENCOUNTER_MALYGOS_H
#define PLAYERBOTS_EOEENCOUNTER_MALYGOS_H

#include "EoEData.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Unit.h"

#include <limits>
#include <utility>
#include <vector>

void GetEoECreatures(Player* bot, uint32 entry, std::vector<Unit*>& out);
Unit* GetNearestEoECreature(Player* bot, uint32 entry, float maxDist = std::numeric_limits<float>::max());
bool AnyEoECreature(Player* bot, uint32 entry);

Unit* GetMalygos(Player* bot);

enum class MalygosPhase : uint8
{
    None,
    P1,
    P2,
    P3,
    Transition,
};

MalygosPhase GetMalygosPhase(Player* bot);

struct MalygosAggro
{
    bool mainTank = false;
    bool victim = false;
    bool victimIsTank = false;
};

MalygosAggro const& GetMalygosAggro(PlayerbotAI* botAI);

struct MalygosP1Layout
{
    std::pair<float, float> tank;
    std::pair<float, float> stack;
    std::pair<float, float> hunter;
    std::pair<float, float> grip;
};

MalygosP1Layout const& GetMalygosP1Layout(Player* bot);

void GetLivePowerSparks(Player* bot, std::vector<Unit*>& out);
bool AnyLivePowerSpark(Player* bot);

Unit* GetNearestPowerSparkTo(PlayerbotAI* botAI, float x, float y);

Unit* GetPowerSparkToKill(PlayerbotAI* botAI, Unit* currentTarget);

Unit* GetPowerSparkToSnare(PlayerbotAI* botAI);

bool IsOnPowerSparkGripDuty(PlayerbotAI* botAI);

float GetBubbleShrinkFactor(Unit* bubble);

bool IsSafelySheltered(Player* bot);

Unit* FindFreeHoverDisk(Player* bot);

bool IsEligibleDiskRider(Player* bot);

bool AnyScionAlive(Player* bot);

#endif
