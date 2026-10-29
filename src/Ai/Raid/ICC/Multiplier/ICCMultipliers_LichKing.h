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

class Creature;

// A braked bot's attacks on him and its area damage next to him: his melee one-shots anyone but a tank
class IccLichKingBrakeMultiplier : public Multiplier
{
public:
    IccLichKingBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc lich king brake") {}
    float GetValue(Action* action) override;

private:
    uint32 _checkedMs = 0;
    Creature* _boss = nullptr;
};

// Phase one: no area damage within reach of a ghoul or Shambling Horror off a damage dealer, so the
// adds live to take Necrotic Plague's jumps
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

// The class disease cures and Cleansing Totem while Necrotic Plague is out, and the poison, curse and magic
// cures that strip a disease too, Stoneform included, off a carrier: a cure off the elected curer's timing
// jumps the plague onto the raid
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
