/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_DEATHWHISPER_H
#define PLAYERBOTS_ICCTRIGGERS_DEATHWHISPER_H

#include "ICCHelpers_Deathwhisper.h"
#include "ICCTriggers_RaidWide.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccDarkReckoningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dark reckoning";

    IccDarkReckoningTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLadyDeathwhisperDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lady deathwhisper dodge";

    IccLadyDeathwhisperDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLadyDeathwhisperFrostboltTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lady deathwhisper frostbolt";

    IccLadyDeathwhisperFrostboltTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLadyDeathwhisperDominateMindTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lady deathwhisper dominate mind";

    IccLadyDeathwhisperDominateMindTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLadyDeathwhisperTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lady deathwhisper taunt";

    IccLadyDeathwhisperTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLadyDeathwhisperRedirectTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lady deathwhisper redirect";

    IccLadyDeathwhisperRedirectTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLadyDeathwhisperTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc lady deathwhisper target";

    IccLadyDeathwhisperTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &LdwTarget) {}
};

#endif
