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

#include <utility>
#include <vector>

// One instance-wide cache in place of a grid sweep per bot per tick. Guids are cached, not
// pointers, so a creature that despawns inside the window cannot come back dangling.
void GetEoECreatures(Player* bot, uint32 entry, std::vector<Unit*>& out);
Unit* GetNearestEoECreature(Player* bot, uint32 entry, float maxDist = 1000.0f);
bool AnyEoECreature(Player* bot, uint32 entry);

// Finds Malygos even while he is flagged non-attackable (P2 flight / phase transitions).
Unit* GetMalygos(Player* bot);

// 0 = inactive/pre-pull, 1 = P1, 2 = P2 (adds up), 3 = drake phase, 4 = phase transition hold.
uint8 GetMalygosPhase(Player* bot);

struct MalygosP1Layout
{
    std::pair<float, float> tank;
    std::pair<float, float> stack;
    std::pair<float, float> hunter;
    std::pair<float, float> grip;
};

// The P1 offsets rotated onto the landing bearing, latched per instance for the whole pull.
MalygosP1Layout const& GetMalygosP1Layout(Player* bot);

// A killed Power Spark never dies - the core zeroes the damage, flags the corpse unselectable and
// leaves it lying there for 60 s while its ground buff ticks, still IsAlive() the whole time. So
// everything that reasons about sparks comes through here or spends the next minute on a corpse.
void GetLivePowerSparks(Player* bot, std::vector<Unit*>& out);

// Anchored on a point rather than on the bot: the grip spot is what the answer is wanted for, and
// the spark nearest the DK is regularly not the spark nearest where he parks.
Unit* GetNearestPowerSparkTo(PlayerbotAI* botAI, float x, float y);

// The spark this bot should hit: of the ones it can reach standing still, the one nearest Malygos.
// currentTarget keeps it from swapping off a spark that has drifted just past reach.
Unit* GetPowerSparkToKill(PlayerbotAI* botAI, Unit* currentTarget);

// The spark a DK should chain: pulled in close and not snared yet.
Unit* GetPowerSparkToSnare(PlayerbotAI* botAI);

// Read by both the position action and the grip, so they cannot disagree about where the DK is.
bool IsOnPowerSparkGripDuty(PlayerbotAI* botAI);

float GetBubbleShrinkFactor(Unit* bubble);

bool IsSafelySheltered(Player* bot);

// Landed, selectable and free doubles as "its Nexus Lord is dead".
Unit* FindFreeHoverDisk(Player* bot);

// Disk duty is melee dps only. Everything that reasons about disks has to agree on this.
bool IsEligibleDiskRider(Player* bot);

bool AnyScionAlive(Player* bot);

#endif
