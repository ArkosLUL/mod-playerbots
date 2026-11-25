/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAID_ULDYOGGSARONPILE_H
#define PLAYERBOTS_RAID_ULDYOGGSARONPILE_H

#include <cmath>
#include <optional>
#include <vector>

namespace YoggSaronPile
{
inline constexpr float PI = 3.14159265f;
inline constexpr float DEG_TO_RAD = PI / 180.0f;
inline constexpr float FRONT_ARC = 80.0f * DEG_TO_RAD;
inline constexpr float TARGET_ARC = 40.0f * DEG_TO_RAD;
inline constexpr float GAZE_CLEAR = 100.0f * DEG_TO_RAD;
inline constexpr int STEPS = 72;

struct Facing
{
    float heading = 0.0f;
    int covered = 0;
};

inline float Gap(float a, float b)
{
    float const turn = std::fmod(std::fabs(a - b), 2.0f * PI);
    return turn > PI ? 2.0f * PI - turn : turn;
}

inline int Covered(std::vector<float> const& bearings, float heading)
{
    int covered = 0;
    for (float const bearing : bearings)
        if (Gap(bearing, heading) <= FRONT_ARC)
            ++covered;

    return covered;
}

inline std::optional<Facing> Best(std::vector<float> const& bearings, std::optional<float> target,
                                  std::optional<float> yogg, float prefer)
{
    std::optional<Facing> best;
    float bestTurn = 0.0f;
    auto const consider = [&](float heading)
    {
        if (target && Gap(*target, heading) > TARGET_ARC)
            return;

        if (yogg && Gap(*yogg, heading) < GAZE_CLEAR)
            return;

        int const covered = Covered(bearings, heading);
        float const turn = Gap(heading, prefer);
        if (!best || covered > best->covered || (covered == best->covered && turn < bestTurn))
        {
            best = Facing{heading, covered};
            bestTurn = turn;
        }
    };

    consider(prefer);
    for (int step = 0; step < STEPS; ++step)
        consider(2.0f * PI * static_cast<float>(step) / static_cast<float>(STEPS));

    return best;
}
}

#endif
