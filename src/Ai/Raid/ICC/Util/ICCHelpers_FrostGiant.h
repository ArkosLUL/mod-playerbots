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

// Death Plague's tick (72864) reaches 8 yd from the carrier: a player's area spell adds no reach to
// another player. 10 is the guide's spacing.
constexpr float RFG_PLAGUE_REACH = 10.0f;
constexpr float RFG_PLAGUE_CLEAR = 12.5f;
// The carrier stands this close to its partner, well inside the reach.
constexpr float RFG_PAIR_DISTANCE = 3.0f;

// The giant holding the encounter live, or null.
Creature* RfgGiant(PlayerbotAI* botAI);

// Gives each Death Plague carrier one partner to stand by when it ticks: a player who already carries
// Recently Infected first, so the plague stops there, else the nearest clean one. A carrying tank gets
// a bot, since its partner walks. Runs from the tick.
void RfgUpdatePartners(PlayerbotAI* botAI);
// Who this bot walks to so the pair stands together when the plague ticks: a carrier's partner, or for
// the partner of a carrying tank or player that carrier, who holds no duty. Null otherwise.
Player* RfgPairWalk(PlayerbotAI* botAI);
// The carriers this bot keeps out of reach of: every one but the carrier it partners.
std::vector<EncounterHelpers::HazardCircle> RfgCarrierHazards(PlayerbotAI* botAI);
// The carrier still has to reach its partner, or someone else stands in a carrier's reach.
bool RfgPlagueDuty(PlayerbotAI* botAI);
// Inside some carrier's clearance, where a generic mover would walk the bot back into its reach.
bool RfgNearCarrier(PlayerbotAI* botAI);

void RfgTick(PlayerbotAI* botAI);

#endif
