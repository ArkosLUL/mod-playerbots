/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_SAURFANG_H
#define PLAYERBOTS_ICCMULTIPLIERS_SAURFANG_H

#include "Multiplier.h"

// Holds a damage dealer's area damage: the boss stands alone and Resistant Skin takes 95% off it on the
// beasts, so it only pulls them. Tanks keep theirs for threat.
class IccDbsAreaDamageMultiplier : public Multiplier
{
public:
    IccDbsAreaDamageMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc dbs area damage") {}
    float GetValue(Action* action) override;
};

// A paladin on Mark duty keeps its Beacon of Light on the mark, so its class beacon nodes would move it
// back to a tank. Those nodes are named after the spell, which no rule can list.
class IccDbsBeaconDutyMultiplier : public Multiplier
{
public:
    IccDbsBeaconDutyMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc dbs beacon duty") {}
    float GetValue(Action* action) override;
};

#endif
