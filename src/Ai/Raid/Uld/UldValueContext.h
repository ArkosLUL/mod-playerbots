/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDVALUECONTEXT_H
#define PLAYERBOTS_ULDVALUECONTEXT_H

#include "NamedObjectContext.h"
#include "UldEncounter_Razorscale.h"
#include "Value.h"

// Owns the bot's RazorscaleScan. A value because that's the one per-bot store triggers, actions and
// multipliers can all reach.
class RazorscaleScanValue : public ManualSetValue<RazorscaleScan*>
{
public:
    RazorscaleScanValue(PlayerbotAI* botAI)
        : ManualSetValue<RazorscaleScan*>(botAI, nullptr, "razorscale scan"), scan(botAI)
    {
        value = defaultValue = &scan;
    }

private:
    RazorscaleScan scan;
};

class RaidUlduarValueContext : public NamedObjectContext<UntypedValue>
{
public:
    RaidUlduarValueContext() { creators["razorscale scan"] = &RaidUlduarValueContext::razorscale_scan; }

private:
    static UntypedValue* razorscale_scan(PlayerbotAI* botAI) { return new RazorscaleScanValue(botAI); }
};

#endif
