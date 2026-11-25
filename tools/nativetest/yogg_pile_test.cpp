/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Uld/Util/UldYoggSaronPile.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <vector>

// Not assert: these have to fail under NDEBUG too.
#define CHECK(cond)                                                                \
    do                                                                             \
    {                                                                              \
        if (!(cond))                                                               \
        {                                                                          \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            std::exit(1);                                                          \
        }                                                                          \
    } while (0)

namespace
{
using YoggSaronPile::Best;
using YoggSaronPile::Covered;
using YoggSaronPile::Facing;
using YoggSaronPile::Gap;

float Deg(float degrees) { return degrees * YoggSaronPile::DEG_TO_RAD; }

void GapWrapsRound()
{
    CHECK(std::fabs(Gap(Deg(350.0f), Deg(10.0f)) - Deg(20.0f)) < 0.001f);
    CHECK(std::fabs(Gap(Deg(-30.0f), Deg(30.0f)) - Deg(60.0f)) < 0.001f);
    CHECK(std::fabs(Gap(Deg(0.0f), Deg(180.0f)) - Deg(180.0f)) < 0.001f);
}

void FreeHeadingCoversAOneSidedPile()
{
    std::vector<float> const pile = {Deg(80.0f), Deg(120.0f), Deg(160.0f), Deg(200.0f)};
    std::optional<Facing> const best = Best(pile, std::nullopt, std::nullopt, 0.0f);
    CHECK(best && best->covered == 4);
    CHECK(Covered(pile, 0.0f) == 1);
}

void TargetKeepsItsArc()
{
    std::vector<float> const pile = {Deg(0.0f), Deg(110.0f), Deg(150.0f), Deg(190.0f)};
    std::optional<Facing> const best = Best(pile, Deg(0.0f), std::nullopt, 0.0f);
    CHECK(best);
    CHECK(Gap(best->heading, 0.0f) <= YoggSaronPile::TARGET_ARC + 0.001f);
    CHECK(best->covered == 2);
}

void GazeKeepsYoggBehind()
{
    std::vector<float> const pile = {Deg(120.0f), Deg(185.0f), Deg(230.0f)};
    std::optional<Facing> const best = Best(pile, std::nullopt, Deg(150.0f), Deg(330.0f));
    CHECK(best);
    CHECK(Gap(best->heading, Deg(150.0f)) >= YoggSaronPile::GAZE_CLEAR - 0.001f);
    CHECK(best->covered == 2);
    CHECK(Covered(pile, Deg(330.0f)) == 0);
}

void TargetTooNearYoggHasNoHeading()
{
    std::vector<float> const pile = {Deg(10.0f)};
    CHECK(!Best(pile, Deg(10.0f), Deg(30.0f), Deg(210.0f)));
}

void TiesStayOnThePreferredHeading()
{
    std::vector<float> const none;
    std::optional<Facing> const best = Best(none, std::nullopt, Deg(0.0f), Deg(183.0f));
    CHECK(best && std::fabs(best->heading - Deg(183.0f)) < 0.001f);
}
}

int main()
{
    GapWrapsRound();
    FreeHeadingCoversAOneSidedPile();
    TargetKeepsItsArc();
    GazeKeepsYoggBehind();
    TargetTooNearYoggHasNoHeading();
    TiesStayOnThePreferredHeading();
    std::printf("yogg_pile_test: ok\n");
    return 0;
}
