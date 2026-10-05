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

// Algalon: the target exclusions keep the pickers off stars, constellations and Dark Matter that
// aren't the bot's job. This catches damage already aimed at one, and nothing else: heals, movement
// and the pickers that would switch the bot back all pass.
class AlgalonTargetGuardMultiplier : public Multiplier
{
public:
    AlgalonTargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon target guard") {}
    float GetValue(Action* action) override;

private:
    std::unordered_map<Action*, bool> damagesCurrentTarget;
};

// Algalon: the soak buttons are kept for Big Bang. Divine Protection, Shield Wall, Pain Suppression and
// the rest cool down for two minutes or more against a 90.5 s cast, so one spent on the way is missing at
// the hit, and the raid is phased for a second after it. A button that will be back in time stays free. The
// soak, external and rescue actions cast directly and never meet this.
class AlgalonCooldownReserveMultiplier : public Multiplier
{
public:
    AlgalonCooldownReserveMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon cooldown reserve") {}
    float GetValue(Action* action) override;

private:
    struct Reserve
    {
        uint32 spellId = 0;
        bool swapTanksOnly = false;
        // A bubble or a self Lay on Hands puts Forbearance on the tank, which blocks Divine Protection.
        bool alwaysForSwapTanks = false;
        // Released to a holder nobody can take him from: its own button, or one cast on it.
        bool holderOwn = false;
        bool holderExternal = false;
    };

    std::unordered_map<Action*, Reserve> reserves;
};

// Algalon: a tank healer heals the swap pair and itself, nobody else. Its class rotation still runs whenever
// that's who it would heal; party heals and heals on anyone else are left to the raid's other healers.
class AlgalonTankHealerMultiplier : public Multiplier
{
public:
    AlgalonTankHealerMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon tank healer focus") {}
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

// Algalon: stars die one at a time on purpose, each death 16-21k to the whole raid. Splash that lands on
// one kills it on its own clock, near whoever stands there, and can bring two down together. The threat
// type misses most of it, so the spell and the name decide.
class AlgalonSplashHoldMultiplier : public Multiplier
{
public:
    AlgalonSplashHoldMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon splash hold") {}
    float GetValue(Action* action) override;

private:
    enum class Splash : uint8
    {
        None,
        // Self auras that hit whatever comes near for their whole duration.
        Always,
        Near,
        // Ground AoE that sits there for 8-10 s while stars wander into it, dropped at the feet or at the target.
        GroundAtBot,
        GroundAtTarget,
    };

    std::unordered_map<Action*, Splash> splashes;
};

// Algalon: reach, set behind and the healers' reach know nothing of the black holes, and a hole beside him
// is where melee stand. A bot that would stop at a field's edge or walk through one waits instead.
class AlgalonWalkGuardMultiplier : public Multiplier
{
public:
    AlgalonWalkGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon walk guard") {}
    float GetValue(Action* action) override;
};

// Algalon: a phased bot still inside a field when Big Bang strips the phase is phased again for 10 s, and the
// strip comes a second after the hit. A cast started there pins the feet and an item use stops the walk, so
// until it is out only the exit runs. Exclusive rules only hold movement.
class AlgalonLeaveFieldMultiplier : public Multiplier
{
public:
    AlgalonLeaveFieldMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon leave field") {}
    float GetValue(Action* action) override;
};

#endif
