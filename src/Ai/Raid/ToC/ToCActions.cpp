#include "ToCActions.h"
#include "ToCHelpers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;

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

// Gormok the Impaler

bool GormokMainTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* gormok = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK));
    if (!gormok)
        return false;

    MarkTargetWithSkull(bot, gormok);
    SetRtiTarget(botAI, "skull", gormok);

    if (AI_VALUE(Unit*, "current target") != gormok)
        return Attack(gormok);

    // Keep the boss near the centre of the arena so ranged can spread and melee have room
    if (gormok->GetVictim() == bot)
    {
        const Position& position = ARENA_CENTER;
        const float distToPosition =
            bot->GetExactDist2d(position.GetPositionX(), position.GetPositionY());

        if (distToPosition > 12.0f)
        {
            const float dX = position.GetPositionX() - bot->GetPositionX();
            const float dY = position.GetPositionY() - bot->GetPositionY();
            const float moveDist = std::min(5.0f, distToPosition);
            const float moveX = bot->GetPositionX() + (dX / distToPosition) * moveDist;
            const float moveY = bot->GetPositionY() + (dY / distToPosition) * moveDist;

            return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, moveX, moveY, position.GetPositionZ(),
                          false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true, true);
        }
    }

    return false;
}

bool GormokFocusSnoboldAction::Execute(Event /*event*/)
{
    Unit* snobold = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_SNOBOLD_VASSAL));
    if (!snobold)
        return false;

    MarkTargetWithCross(bot, snobold);

    if (AI_VALUE(Unit*, "current target") != snobold)
        return Attack(snobold);

    return false;
}

// Acidmaw & Dreadscale

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
    const Position& center = ARENA_CENTER;
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

// Icehowl

bool IcehowlMainTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* icehowl = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
    if (!icehowl)
        return false;

    MarkTargetWithSkull(bot, icehowl);
    SetRtiTarget(botAI, "skull", icehowl);

    if (AI_VALUE(Unit*, "current target") != icehowl)
        return Attack(icehowl);

    if (icehowl->GetVictim() == bot)
    {
        const Position& position = ARENA_CENTER;
        const float distToPosition =
            bot->GetExactDist2d(position.GetPositionX(), position.GetPositionY());

        if (distToPosition > 12.0f)
        {
            const float dX = position.GetPositionX() - bot->GetPositionX();
            const float dY = position.GetPositionY() - bot->GetPositionY();
            const float moveDist = std::min(5.0f, distToPosition);
            const float moveX = bot->GetPositionX() + (dX / distToPosition) * moveDist;
            const float moveY = bot->GetPositionY() + (dY / distToPosition) * moveDist;

            return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, moveX, moveY, position.GetPositionZ(),
                          false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true, true);
        }
    }

    return false;
}

bool IcehowlClearChargePathAction::Execute(Event /*event*/)
{
    Unit* icehowl = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
    if (!icehowl)
        return false;

    // Self-contained guard: only dodge when actually standing in the charge lane
    constexpr float corridorHalfWidth = 14.0f;
    if (!IsBotInChargeCorridor(bot, icehowl, corridorHalfWidth))
        return false;

    const float orientation = icehowl->GetOrientation();
    const float dirX = std::cos(orientation);
    const float dirY = std::sin(orientation);

    const float relX = bot->GetPositionX() - icehowl->GetPositionX();
    const float relY = bot->GetPositionY() - icehowl->GetPositionY();

    // Signed perpendicular offset from the charge lane; flee toward whichever side the bot is on
    const float perpendicular = relX * dirY - relY * dirX;
    const float side = perpendicular >= 0.0f ? 1.0f : -1.0f;

    // Unit vector perpendicular to the charge direction, pointing away from the lane
    const float escapeX = dirY * side;
    const float escapeY = -dirX * side;

    // Inside the corridor (guaranteed by the guard above), so this is always positive
    const float clearance = corridorHalfWidth - std::fabs(perpendicular) + 3.0f;

    botAI->InterruptSpell();

    const float destX = bot->GetPositionX() + escapeX * clearance;
    const float destY = bot->GetPositionY() + escapeY * clearance;

    return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, destX, destY, bot->GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}
