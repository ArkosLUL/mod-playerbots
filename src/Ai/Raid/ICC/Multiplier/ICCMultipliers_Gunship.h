/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_GUNSHIP_H
#define PLAYERBOTS_ICCMULTIPLIERS_GUNSHIP_H

#include "Multiplier.h"

class IccGunshipMultiplier : public Multiplier
{
public:
    IccGunshipMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc gunship") {}
    float GetValue(Action* action) override;

private:
    uint32 _captainCheckedMs = 0;
    bool _inGunship = false;
    bool _hordeSide = false;
};

#endif
