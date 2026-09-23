#include "ToCHelpers_Icehowl.h"
#include "Playerbots.h"
#include "Unit.h"

#include <cmath>

namespace TrialOfTheCrusaderHelpers
{

bool IsBotInChargeCorridor(Player* bot, Unit* icehowl, float halfWidth)
{
    if (!bot || !icehowl)
        return false;

    float const orientation = icehowl->GetOrientation();
    float const dirX = std::cos(orientation);
    float const dirY = std::sin(orientation);

    float const relX = bot->GetPositionX() - icehowl->GetPositionX();
    float const relY = bot->GetPositionY() - icehowl->GetPositionY();

    // Distance along the charge direction (in front of Icehowl) and perpendicular offset from the lane
    float const along = relX * dirX + relY * dirY;
    float const perpendicular = std::fabs(relX * dirY - relY * dirX);

    return along > -5.0f && along < 120.0f && perpendicular < halfWidth;
}

bool HasMassiveCrashAura(Player* bot)
{
    if (!bot)
        return false;

    return bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MASSIVE_CRASH_10N)) ||
           bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MASSIVE_CRASH_25N)) ||
           bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MASSIVE_CRASH_10H)) ||
           bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MASSIVE_CRASH_25H));
}

}
