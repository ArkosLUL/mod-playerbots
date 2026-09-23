#include "ToCActions_Icehowl.h"
#include "ToCData.h"
#include "ToCHelpers_Icehowl.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"

#include <cmath>

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

bool IcehowlMainTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* icehowl = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
    if (!icehowl)
        return false;

    MarkTargetWithSkull(bot, icehowl);
    SetRtiTarget(botAI, "skull", icehowl);

    if (AI_VALUE(Unit*, "current target") != icehowl)
        return Attack(icehowl);

    return DragBossToAnchor(icehowl, ARENA_CENTER);
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

    bot->CastStop();

    const float destX = bot->GetPositionX() + escapeX * clearance;
    const float destY = bot->GetPositionY() + escapeY * clearance;

    return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, destX, destY, bot->GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
}
