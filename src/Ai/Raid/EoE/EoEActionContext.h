#ifndef PLAYERBOTS_EOEACTIONCONTEXT_H
#define PLAYERBOTS_EOEACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "EoEActions.h"

class RaidEoEActionContext : public NamedObjectContext<Action>
{
public:
    RaidEoEActionContext()
    {
        creators["malygos position"] = &RaidEoEActionContext::position;
        creators["malygos target"] = &RaidEoEActionContext::target;
        creators["pull power spark"] = &RaidEoEActionContext::pull_power_spark;
        creators["kill power spark"] = &RaidEoEActionContext::kill_power_spark;
        creators["deep breath dodge"] = &RaidEoEActionContext::deep_breath_dodge;
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
    static Action* deep_breath_dodge(PlayerbotAI* ai) { return new DeepBreathDodgeAction(ai); }
    static Action* avoid_surge_of_power(PlayerbotAI* ai) { return new AvoidSurgeOfPowerAction(ai); }
    static Action* eoe_fly_drake(PlayerbotAI* ai) { return new EoEFlyDrakeAction(ai); }
    static Action* eoe_drake_attack(PlayerbotAI* ai) { return new EoEDrakeAttackAction(ai); }
    static Action* avoid_static_field(PlayerbotAI* ai) { return new AvoidStaticFieldAction(ai); }
    static Action* drake_dodge_surge(PlayerbotAI* ai) { return new DrakeDodgeSurgeAction(ai); }
};

#endif
