/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DKVALUES_H
#define PLAYERBOTS_DKVALUES_H

#include "Value.h"

class PlayerbotAI;
class Player;
class Unit;

// Hysteria is +20% physical damage for a 1% max health drain per second, so it goes to the
// hardest-hitting physical dps and never to a tank or the DK while grouped.
class HysteriaTargetValue : public UnitCalculatedValue
{
public:
    HysteriaTargetValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "hysteria target", 2 * 1000) {}

    Unit* Calculate() override;

private:
    static constexpr uint32 SPELL_HYSTERIA = 49016;
    // the drain takes 30% over the buff
    static constexpr float MIN_HEALTH_PCT = 50.0f;

    static uint8 PhysicalTier(Player* member);
};

#endif
