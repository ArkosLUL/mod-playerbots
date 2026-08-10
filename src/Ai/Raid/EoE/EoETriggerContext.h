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
        creators["malygos bubble"] = &RaidEoETriggerContext::malygos_bubble;
        creators["malygos free disk"] = &RaidEoETriggerContext::malygos_free_disk;
        creators["malygos on disk"] = &RaidEoETriggerContext::malygos_on_disk;
        creators["surge of power"] = &RaidEoETriggerContext::surge_of_power;
        creators["malygos drake flight"] = &RaidEoETriggerContext::malygos_drake_flight;
        creators["drake surge"] = &RaidEoETriggerContext::drake_surge;
    }

private:
    static Trigger* malygos(PlayerbotAI* ai) { return new MalygosTrigger(ai); }
    static Trigger* power_spark(PlayerbotAI* ai) { return new PowerSparkTrigger(ai); }
    static Trigger* malygos_bubble(PlayerbotAI* ai) { return new MalygosBubbleTrigger(ai); }
    static Trigger* malygos_free_disk(PlayerbotAI* ai) { return new MalygosFreeDiskTrigger(ai); }
    static Trigger* malygos_on_disk(PlayerbotAI* ai) { return new MalygosOnDiskTrigger(ai); }
    static Trigger* surge_of_power(PlayerbotAI* ai) { return new SurgeOfPowerTrigger(ai); }
    static Trigger* malygos_drake_flight(PlayerbotAI* ai) { return new MalygosDrakeFlightTrigger(ai); }
    static Trigger* drake_surge(PlayerbotAI* ai) { return new DrakeSurgeTrigger(ai); }
};

#endif
