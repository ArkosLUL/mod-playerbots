/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_SINDRAGOSA_H
#define PLAYERBOTS_ICCMULTIPLIERS_SINDRAGOSA_H

#include "Multiplier.h"
#include "ObjectGuid.h"

class IccSindragosaMultiplier : public Multiplier
{
public:
    IccSindragosaMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc sindragosa") {}
    float GetValue(Action* action) override;

private:
    uint32 _checkedMs = 0;
    ObjectGuid _bossGuid;
    bool _lastPhase = false;
    bool _overBombStation = false;
};

#endif
