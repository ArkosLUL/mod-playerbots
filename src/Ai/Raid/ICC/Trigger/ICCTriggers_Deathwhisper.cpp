/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Deathwhisper.h"

#include "ICCHelpers_Deathwhisper.h"
#include "Playerbots.h"

bool IccDarkReckoningTrigger::IsActive() { return LdwDarkReckoningDodging(botAI); }

bool IccLadyDeathwhisperDodgeTrigger::IsActive() { return LdwDodgeKind(botAI) != LdwHazardKind::None; }

bool IccLadyDeathwhisperFrostboltTrigger::IsActive() { return LdwIsFrostboltInterrupter(botAI); }

bool IccLadyDeathwhisperDominateMindTrigger::IsActive()
{
    char const* spell = nullptr;
    return LdwCcDuty(botAI, spell) != nullptr;
}

bool IccLadyDeathwhisperTauntTrigger::IsActive()
{
    return LdwLooseAddToTaunt(botAI) || LdwSwapTaunt(botAI);
}

bool IccLadyDeathwhisperRedirectTrigger::IsActive()
{
    uint8 const cls = bot->getClass();
    return (cls == CLASS_HUNTER || cls == CLASS_ROGUE) && LdwRedirectTank(botAI);
}
