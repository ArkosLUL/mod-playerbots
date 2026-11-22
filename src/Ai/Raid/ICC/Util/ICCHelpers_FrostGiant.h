/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_FROSTGIANT_H
#define PLAYERBOTS_ICCHELPERS_FROSTGIANT_H

#include <vector>

#include "EncounterHelpers.h"
#include "ICCData.h"
#include "Playerbots.h"

constexpr uint32 SPELL_RFG_RECENTLY_INFECTED = 72884;

constexpr float RFG_PLAGUE_REACH = 10.0f;
constexpr float RFG_PLAGUE_CLEAR = 12.5f;
constexpr float RFG_PAIR_DISTANCE = 3.0f;

constexpr float RFG_SPOT_SWEEP = 30.0f;
constexpr float RFG_SPOT_GIANT_MAX = 30.0f;
constexpr float RFG_SPOT_ARRIVE = 3.0f;
constexpr uint32 RFG_SPOT_BUDGET_MS = 7000;

Creature* RfgGiant(PlayerbotAI* botAI);

void RfgUpdatePartners(PlayerbotAI* botAI);

struct RfgWalk
{
    Player* mate = nullptr;
    Position spot;
    bool toSpot = false;

    explicit operator bool() const { return mate || toSpot; }
};
RfgWalk RfgPairWalk(PlayerbotAI* botAI);
std::vector<EncounterHelpers::HazardCircle> RfgCarrierHazards(PlayerbotAI* botAI);
bool RfgPlagueDuty(PlayerbotAI* botAI);
bool RfgNearCarrier(PlayerbotAI* botAI);

void RfgTick(PlayerbotAI* botAI);

#endif
