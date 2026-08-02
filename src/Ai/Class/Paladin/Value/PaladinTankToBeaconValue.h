/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PALADINTANKTOBEACONVALUE_H
#define PLAYERBOTS_PALADINTANKTOBEACONVALUE_H

#include "ObjectGuid.h"
#include "PartyMemberValue.h"

class PlayerbotAI;

// The tank Beacon of Light and Sacred Shield should sit on: the one actually taking damage, which in
// a two-tank raid is not always the main tank.
class PaladinTankToBeaconValue : public PartyMemberValue
{
public:
    PaladinTankToBeaconValue(PlayerbotAI* botAI) : PartyMemberValue(botAI, "tank to beacon", 2 * 1000) {}

    Unit* Calculate() override;

private:
    ObjectGuid lastTank;
};

#endif
