/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BOSSRESISTANCEMULTIPLIERS_H
#define PLAYERBOTS_BOSSRESISTANCEMULTIPLIERS_H

#include "Multiplier.h"

#include <string>

class Action;
class PlayerbotAI;

// Holds the one designated hunter in Aspect of the Wild while a nature-damage boss is up. Without it
// the hunter's own "bdps" node re-casts Dragonhawk on the next GCD and the two trade the aspect slot
// for the whole fight - the same failure HodirPaladinAuraMultiplier exists to prevent for the paladin
// resistance aura. Only the chosen hunter is held; every other hunter keeps Dragonhawk and stays free
// to swap to Aspect of the Viper for mana.
class BossNatureAspectHoldMultiplier : public Multiplier
{
public:
    BossNatureAspectHoldMultiplier(PlayerbotAI* botAI, std::string const bossName)
        : Multiplier(botAI, bossName + " nature aspect hold multiplier"), bossName(bossName)
    {
    }

    float GetValue(Action* action) override;

private:
    std::string const bossName;
};

#endif
