/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_ALGALON_H
#define PLAYERBOTS_ULDMULTIPLIERS_ALGALON_H

#include "Define.h"
#include "Multiplier.h"

#include <unordered_map>

class AlgalonTargetGuardMultiplier : public Multiplier
{
public:
    AlgalonTargetGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "algalon target guard") {}
    float GetValue(Action* action) override;

private:
    enum class Aim : uint8
    {
        None,
        Current,
        Own,
    };
    std::unordered_map<Action*, Aim> aims;
    std::unordered_map<Action*, bool> lingers;
};

class AlgalonCooldownReserveMultiplier : public Multiplier
{
public:
    AlgalonCooldownReserveMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "algalon cooldown reserve") {}
    float GetValue(Action* action) override;

private:
    struct Reserve
    {
        uint32 spellId = 0;
        bool bubble = false;
        bool dispersion = false;
        bool swapTanksOnly = false;
        bool alwaysForSwapTanks = false;
        bool holderOwn = false;
        bool holderExternal = false;
    };

    std::unordered_map<Action*, Reserve> reserves;
};

class AlgalonHideCastMultiplier : public Multiplier
{
public:
    AlgalonHideCastMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "algalon hide cast") {}
    float GetValue(Action* action) override;

private:
    std::unordered_map<Action*, uint32> spellIds;
};

class AlgalonTankHealerMultiplier : public Multiplier
{
public:
    AlgalonTankHealerMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "algalon tank healer focus") {}
    float GetValue(Action* action) override;

private:
    enum class Heal : uint8
    {
        None,
        Party,
        Targeted,
    };

    std::unordered_map<Action*, Heal> heals;
};

class AlgalonSplashHoldMultiplier : public Multiplier
{
public:
    AlgalonSplashHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "algalon splash hold") {}
    float GetValue(Action* action) override;

private:
    enum class Splash : uint8
    {
        None,
        Always,
        Near,
        GroundAtBot,
        GroundAtTarget,
    };

    std::unordered_map<Action*, Splash> splashes;
};

class AlgalonWalkGuardMultiplier : public Multiplier
{
public:
    AlgalonWalkGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "algalon walk guard") {}
    float GetValue(Action* action) override;
};

class AlgalonLeaveFieldMultiplier : public Multiplier
{
public:
    AlgalonLeaveFieldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "algalon leave field") {}
    float GetValue(Action* action) override;
};

#endif
