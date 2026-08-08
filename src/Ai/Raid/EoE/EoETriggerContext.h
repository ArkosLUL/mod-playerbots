/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOETRIGGERCONTEXT_H
#define PLAYERBOTS_EOETRIGGERCONTEXT_H

#include "EoETriggers.h"
#include "NamedObjectContext.h"

class RaidEoETriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidEoETriggerContext()
    {
        creators["malygos"] = &RaidEoETriggerContext::malygos;
        creators["power spark"] = &RaidEoETriggerContext::power_spark;
        creators["deep breath"] = &RaidEoETriggerContext::deep_breath;
        creators["surge of power"] = &RaidEoETriggerContext::surge_of_power;
        creators["static field"] = &RaidEoETriggerContext::static_field;
        creators["drake surge"] = &RaidEoETriggerContext::drake_surge;
    }

private:
    static Trigger* malygos(PlayerbotAI* ai) { return new MalygosTrigger(ai); }
    static Trigger* power_spark(PlayerbotAI* ai) { return new PowerSparkTrigger(ai); }
    static Trigger* deep_breath(PlayerbotAI* ai) { return new DeepBreathTrigger(ai); }
    static Trigger* surge_of_power(PlayerbotAI* ai) { return new SurgeOfPowerTrigger(ai); }
    static Trigger* static_field(PlayerbotAI* ai) { return new StaticFieldTrigger(ai); }
    static Trigger* drake_surge(PlayerbotAI* ai) { return new DrakeSurgeTrigger(ai); }
};

#endif
