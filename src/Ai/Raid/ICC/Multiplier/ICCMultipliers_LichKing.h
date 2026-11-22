/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_LICHKING_H
#define PLAYERBOTS_ICCMULTIPLIERS_LICHKING_H

#include <unordered_map>
#include <vector>

#include "Multiplier.h"
#include "Position.h"
#include "RaidSplash.h"

class Creature;

class IccLichKingBrakeMultiplier : public Multiplier
{
public:
    IccLichKingBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc lich king brake") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
    uint32 _checkedMs = 0;
    Creature* _boss = nullptr;
};

class IccLichKingSplashMultiplier : public Multiplier
{
public:
    IccLichKingSplashMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc lich king splash") {}
    float GetValue(Action* action) override;

private:
    bool Splashes(Action* action);

    std::unordered_map<Action*, bool> _splashes;
    uint32 _checkedMs = 0;
    std::vector<Position> _adds;
};

class IccLichKingCureHoldMultiplier : public Multiplier
{
public:
    IccLichKingCureHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc lich king cure hold") {}
    float GetValue(Action* action) override;

private:
    uint8 CureKind(Action* action);

    std::unordered_map<Action*, uint8> _kinds;
};

#endif
