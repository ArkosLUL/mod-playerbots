/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Svalna.h"

#include "Playerbots.h"

bool IccSvalnaSpearTrigger::IsActive() { return GetSsSpearToPull(botAI) != nullptr; }

bool IccSvalnaHurlTrigger::IsActive() { return GetSsSpearToThrow(botAI) != nullptr; }

bool IccSvalnaTauntTrigger::IsActive() { return GetSsCaptainToTaunt(botAI) != nullptr; }
