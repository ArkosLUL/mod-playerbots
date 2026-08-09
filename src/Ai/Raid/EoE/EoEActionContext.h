/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEACTIONCONTEXT_H
#define PLAYERBOTS_EOEACTIONCONTEXT_H

#include "Action.h"
#include "EoEActions.h"
#include "NamedObjectContext.h"

class RaidEoEActionContext : public NamedObjectContext<Action>
{
public:
    RaidEoEActionContext()
    {
        creators["malygos position"] = &RaidEoEActionContext::position;
        creators["malygos target"] = &RaidEoEActionContext::target;
        creators["pull power spark"] = &RaidEoEActionContext::pull_power_spark;
        creators["kill power spark"] = &RaidEoEActionContext::kill_power_spark;
        creators["malygos spellsteal"] = &RaidEoEActionContext::malygos_spellsteal;
        creators["malygos seek bubble"] = &RaidEoEActionContext::malygos_seek_bubble;
        creators["malygos board disk"] = &RaidEoEActionContext::malygos_board_disk;
        creators["malygos ride disk"] = &RaidEoEActionContext::malygos_ride_disk;
        creators["avoid surge of power"] = &RaidEoEActionContext::avoid_surge_of_power;
        creators["eoe fly drake"] = &RaidEoEActionContext::eoe_fly_drake;
        creators["eoe drake attack"] = &RaidEoEActionContext::eoe_drake_attack;
        creators["avoid static field"] = &RaidEoEActionContext::avoid_static_field;
        creators["drake dodge surge"] = &RaidEoEActionContext::drake_dodge_surge;
    }

private:
    static Action* position(PlayerbotAI* ai) { return new MalygosPositionAction(ai); }
    static Action* target(PlayerbotAI* ai) { return new MalygosTargetAction(ai); }
    static Action* pull_power_spark(PlayerbotAI* ai) { return new PullPowerSparkAction(ai); }
    static Action* kill_power_spark(PlayerbotAI* ai) { return new KillPowerSparkAction(ai); }
    static Action* malygos_spellsteal(PlayerbotAI* ai) { return new MalygosSpellstealAction(ai); }
    static Action* malygos_seek_bubble(PlayerbotAI* ai) { return new MalygosSeekBubbleAction(ai); }
    static Action* malygos_board_disk(PlayerbotAI* ai) { return new MalygosBoardDiskAction(ai); }
    static Action* malygos_ride_disk(PlayerbotAI* ai) { return new MalygosRideDiskAction(ai); }
    static Action* avoid_surge_of_power(PlayerbotAI* ai) { return new AvoidSurgeOfPowerAction(ai); }
    static Action* eoe_fly_drake(PlayerbotAI* ai) { return new EoEFlyDrakeAction(ai); }
    static Action* eoe_drake_attack(PlayerbotAI* ai) { return new EoEDrakeAttackAction(ai); }
    static Action* avoid_static_field(PlayerbotAI* ai) { return new AvoidStaticFieldAction(ai); }
    static Action* drake_dodge_surge(PlayerbotAI* ai) { return new DrakeDodgeSurgeAction(ai); }
};

#endif
