/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ROGUEVALUES_H
#define PLAYERBOTS_ROGUEVALUES_H

#include "Value.h"

class PlayerbotAI;
class Unit;

// Tricks of the Trade goes to the tank while threat is still being built, and to the hardest-hitting
// melee afterwards, which is where the 15% damage buff is worth the most.
class TricksOfTheTradeTargetValue : public UnitCalculatedValue
{
public:
    TricksOfTheTradeTargetValue(PlayerbotAI* botAI)
        : UnitCalculatedValue(botAI, "tricks of the trade target", 2 * 1000) {}

    Unit* Calculate() override;

private:
    static constexpr uint32 OPENER_SECONDS = 10;
    static constexpr float REDIRECT_RANGE = 20.0f;

    bool TankNeedsRedirect(Unit* mainTank);
};

#endif
