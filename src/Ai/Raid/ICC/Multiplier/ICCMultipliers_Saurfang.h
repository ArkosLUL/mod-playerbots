/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_SAURFANG_H
#define PLAYERBOTS_ICCMULTIPLIERS_SAURFANG_H

#include "Multiplier.h"
#include "RaidSplash.h"

class IccDbsAreaDamageMultiplier : public Multiplier
{
public:
    IccDbsAreaDamageMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc dbs area damage") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class IccDbsBeaconDutyMultiplier : public Multiplier
{
public:
    IccDbsBeaconDutyMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc dbs beacon duty") {}
    float GetValue(Action* action) override;
};

#endif
