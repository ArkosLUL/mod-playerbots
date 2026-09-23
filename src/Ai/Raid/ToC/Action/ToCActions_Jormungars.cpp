#include "ToCActions_Jormungars.h"
#include "ToCData.h"
#include "ToCHelpers_Jormungars.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"

#include <cmath>

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{
// Return the alive worm (Acidmaw or Dreadscale) currently in the requested mobility state
Unit* FindWorm(PlayerbotAI* botAI, bool mobile)
{
    Unit* acidmaw = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ACIDMAW));
    if (acidmaw && IsWormMobile(acidmaw) == mobile)
        return acidmaw;

    Unit* dreadscale = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_DREADSCALE));
    if (dreadscale && IsWormMobile(dreadscale) == mobile)
        return dreadscale;

    return nullptr;
}
}

bool WormsMainTankHoldMobileWormAction::Execute(Event /*event*/)
{
    Unit* worm = FindWorm(botAI, true);
    if (!worm)
        return false;

    MarkTargetWithSkull(bot, worm);
    SetRtiTarget(botAI, "skull", worm);

    if (AI_VALUE(Unit*, "current target") != worm)
        return Attack(worm);

    return false;
}

bool WormsAssistTankHoldStationaryWormAction::Execute(Event /*event*/)
{
    Unit* worm = FindWorm(botAI, false);
    if (!worm)
        return false;

    MarkTargetWithCross(bot, worm);

    if (AI_VALUE(Unit*, "current target") != worm)
        return Attack(worm);

    return false;
}

bool WormsSpreadAction::Execute(Event /*event*/)
{
    constexpr float minSpreadDistance = 8.0f;
    constexpr uint32 minInterval = 1000;
    if (Unit* nearestPlayer = GetNearestPlayerInRadius(bot, minSpreadDistance))
        return FleePosition(nearestPlayer->GetPosition(), minSpreadDistance, minInterval);

    return false;
}

bool WormsKeepMovingAction::Execute(Event /*event*/)
{
    // Burning damage ramps up while standing still, so keep the bot in motion around the arena
    Position const& center = ARENA_CENTER;
    const float distToCenter = bot->GetExactDist2d(center.GetPositionX(), center.GetPositionY());

    float destX;
    float destY;
    if (distToCenter > 8.0f)
    {
        const float dX = center.GetPositionX() - bot->GetPositionX();
        const float dY = center.GetPositionY() - bot->GetPositionY();
        destX = bot->GetPositionX() + (dX / distToCenter) * 5.0f;
        destY = bot->GetPositionY() + (dY / distToCenter) * 5.0f;
    }
    else
    {
        const float angle = bot->GetOrientation() + static_cast<float>(M_PI) / 2.0f;
        destX = bot->GetPositionX() + std::cos(angle) * 6.0f;
        destY = bot->GetPositionY() + std::sin(angle) * 6.0f;
    }

    return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, destX, destY, center.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool WormsAvoidSlimePoolAction::Execute(Event /*event*/)
{
    return FleeFromCreatureCluster(static_cast<uint32>(ToCNpcs::NPC_SLIME_POOL));
}

bool WormsAvoidSweepAction::Execute(Event /*event*/)
{
    Unit* worm = GetWormCastingSweep(botAI);
    if (!worm)
        return false;

    // Step perpendicular to the worm's facing to clear the frontal Sweep cone, fleeing toward whichever
    // side the bot is already on.
    const float orientation = worm->GetOrientation();
    const float dirX = std::cos(orientation);
    const float dirY = std::sin(orientation);

    const float relX = bot->GetPositionX() - worm->GetPositionX();
    const float relY = bot->GetPositionY() - worm->GetPositionY();

    const float perpendicular = relX * dirY - relY * dirX;
    const float side = perpendicular >= 0.0f ? 1.0f : -1.0f;

    const float escapeX = dirY * side;
    const float escapeY = -dirX * side;

    bot->CastStop();

    constexpr float clearance = 12.0f;
    const float destX = bot->GetPositionX() + escapeX * clearance;
    const float destY = bot->GetPositionY() + escapeY * clearance;

    return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, destX, destY, bot->GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}
