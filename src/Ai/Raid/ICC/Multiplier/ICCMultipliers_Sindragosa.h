/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_SINDRAGOSA_H
#define PLAYERBOTS_ICCMULTIPLIERS_SINDRAGOSA_H

#include <unordered_map>
#include <vector>

#include "ICCHelpers_Sindragosa.h"
#include "Multiplier.h"
#include "RaidSplash.h"

class IccSindragosaBrakeMultiplier : public Multiplier
{
public:
    IccSindragosaBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc sindragosa brake") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class IccSindragosaTombMultiplier : public Multiplier
{
public:
    IccSindragosaTombMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc sindragosa tomb") {}
    float GetValue(Action* action) override;

private:
    uint8 KindOf(Action* action);

    std::unordered_map<Action*, uint8> _kinds;
    uint32 _checkedMs = 0;
    std::vector<SgHeldTomb> _held;
};

#endif
