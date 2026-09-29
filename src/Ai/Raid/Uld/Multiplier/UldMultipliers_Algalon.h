/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_ALGALON_H
#define PLAYERBOTS_ULDMULTIPLIERS_ALGALON_H

#include "Define.h"
#include "Multiplier.h"

#include <unordered_map>

// Algalon: the target exclusions keep the pickers off stars, constellations and Dark Matter that
// aren't the bot's job. This catches damage already aimed at one, and nothing else: heals, movement
// and the pickers that would switch the bot back all pass.
class AlgalonTargetGuardMultiplier : public Multiplier
{
public:
    AlgalonTargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon target guard") {}
    float GetValue(Action* action) override;

private:
    std::unordered_map<Action*, bool> damagesCurrentTarget;
};

// Algalon: stars die one at a time on purpose, each death 16-21k to the whole raid, and splash that
// clips a second one undoes the pacing.
class AlgalonStarAoeMultiplier : public Multiplier
{
public:
    AlgalonStarAoeMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon star aoe") {}
    float GetValue(Action* action) override;
};

#endif
