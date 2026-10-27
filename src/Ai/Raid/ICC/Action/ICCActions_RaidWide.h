/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_RAIDWIDE_H
#define PLAYERBOTS_ICCACTIONS_RAIDWIDE_H

#include <string>

#include "ICCSeams.h"
#include "RaidDirectTargeting.h"
#include "RaidOpener.h"
#include "RaidResistAura.h"

// Base for an encounter's direct-targeting action: attacks its resolver's pick, sending the pet along.
class IccDirectAttackAction : public RaidDirectAttackAction
{
public:
    IccDirectAttackAction(PlayerbotAI* botAI, std::string const name, IccResolver resolve)
        : RaidDirectAttackAction(botAI, name), _resolve(resolve) {}
    bool Execute(Event event) override;

private:
    IccResolver _resolve;
};

class IccOpenerStopSwingAction : public RaidOpenerStopSwingAction
{
public:
    static constexpr char const* Name = "icc opener stop swing";

    IccOpenerStopSwingAction(PlayerbotAI* botAI) : RaidOpenerStopSwingAction(botAI, Name, ICC_OPENER) {}
};

class IccOpenerRedirectAction : public RaidOpenerRedirectAction
{
public:
    static constexpr char const* Name = "icc opener redirect";

    IccOpenerRedirectAction(PlayerbotAI* botAI) : RaidOpenerRedirectAction(botAI, Name, ICC_OPENER) {}
};

class IccRaiseResistAuraAction : public RaidRaiseResistAuraAction
{
public:
    static constexpr char const* Name = "icc raise resistance aura";

    IccRaiseResistAuraAction(PlayerbotAI* botAI) : RaidRaiseResistAuraAction(botAI, Name, ICC_RESIST_AURA) {}
};

#endif
