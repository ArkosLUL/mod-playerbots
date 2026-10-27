/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_RAIDWIDE_H
#define PLAYERBOTS_ICCTRIGGERS_RAIDWIDE_H

#include <string>

#include "ICCSeams.h"
#include "RaidOpener.h"
#include "RaidResistAura.h"
#include "Trigger.h"

// Base for an encounter's direct-targeting trigger: its resolver picks a unit the bot, or its pet, isn't on.
class IccDirectTargetTrigger : public Trigger
{
public:
    IccDirectTargetTrigger(PlayerbotAI* botAI, std::string const name, IccResolver resolve)
        : Trigger(botAI, name), _resolve(resolve) {}
    bool IsActive() override;

private:
    IccResolver _resolve;
};

// Asks nothing, so notes nothing, while the live encounter lists no opener boss.
class IccOpenerSwingTrigger : public RaidOpenerSwingTrigger
{
public:
    static constexpr char const* Name = "icc opener swing";

    IccOpenerSwingTrigger(PlayerbotAI* botAI) : RaidOpenerSwingTrigger(botAI, Name, ICC_OPENER) {}
    bool IsActive() override;
};

class IccOpenerRedirectTrigger : public RaidOpenerRedirectTrigger
{
public:
    static constexpr char const* Name = "icc opener redirect";

    IccOpenerRedirectTrigger(PlayerbotAI* botAI) : RaidOpenerRedirectTrigger(botAI, Name, ICC_OPENER) {}
};

// Asks nothing, so notes nothing, while the live encounter has no resistance aura hook.
class IccResistAuraTrigger : public RaidResistAuraTrigger
{
public:
    static constexpr char const* Name = "icc resistance aura";

    IccResistAuraTrigger(PlayerbotAI* botAI) : RaidResistAuraTrigger(botAI, Name, ICC_RESIST_AURA) {}
    bool IsActive() override;
};

#endif
