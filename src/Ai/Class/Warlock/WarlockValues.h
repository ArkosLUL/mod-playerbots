/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_WARLOCKVALUES_H
#define PLAYERBOTS_WARLOCKVALUES_H

#include "Value.h"

class PlayerbotAI;
class Unit;

// Own Seed landing on a target wipes own Corruption there, so Seed stays off bosses and prefers adds
// we haven't Corrupted yet. The explosion still hits everything around the seeded add.
class SeedOfCorruptionTargetValue : public UnitCalculatedValue
{
public:
    SeedOfCorruptionTargetValue(PlayerbotAI* botAI)
        : UnitCalculatedValue(botAI, "seed of corruption target", 1 * 1000) {}

    Unit* Calculate() override;
};

// Not a boss and not carrying our own Corruption.
bool IsSeedOfCorruptionTarget(PlayerbotAI* botAI, Unit* target);

// Our own Seed is on the target, or was cast at it recently and may still be flying.
bool SeedOfCorruptionPending(PlayerbotAI* botAI, Unit* target);

#endif
