/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_FROSTFIREMAGESTRATEGY_H
#define PLAYERBOTS_FROSTFIREMAGESTRATEGY_H

#include "FireMageStrategy.h"

class PlayerbotAI;

// Frostfire is a Fire build with a different filler, so it inherits Fire's whole trigger list.
class FrostFireMageStrategy : public FireMageStrategy
{
public:
    FrostFireMageStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "frostfire"; }
    std::vector<NextAction> getDefaultActions() override;
};

#endif
