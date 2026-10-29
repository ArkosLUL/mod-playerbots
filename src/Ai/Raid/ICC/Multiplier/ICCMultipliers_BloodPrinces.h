/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_BLOODPRINCES_H
#define PLAYERBOTS_ICCMULTIPLIERS_BLOODPRINCES_H

#include "Multiplier.h"

// Holds a damage dealer's area damage near a prince without the Invocation, whose damage turns into threat
// on the dealer, and near a Dark Nucleus, which a cleave or a chain takes off its tank.
class IccBpcSplashMultiplier : public Multiplier
{
public:
    IccBpcSplashMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bpc splash") {}
    float GetValue(Action* action) override;
};

// A braked bot's attacks on the empowered prince and its area damage next to him: its threat would take
// him off his owner
class IccBpcBrakeMultiplier : public Multiplier
{
public:
    IccBpcBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bpc brake") {}
    float GetValue(Action* action) override;
};

// On a bomb shot or a nucleus grab, a spell that deals no damage on landing: neither Unstable nor the
// nucleus answers a DoT's application or a debuff
class IccBpcDirectHitMultiplier : public Multiplier
{
public:
    IccBpcDirectHitMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bpc direct hit") {}
    float GetValue(Action* action) override;
};

#endif
