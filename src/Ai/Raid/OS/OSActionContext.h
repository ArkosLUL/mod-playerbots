/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSACTIONCONTEXT_H
#define PLAYERBOTS_OSACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "OSActions.h"

// One node short of the trigger count on purpose: "sartharion melee positioning" runs "rear flank",
// which comes from the base ActionContext.
class RaidOsActionContext : public NamedObjectContext<Action>
{
public:
    RaidOsActionContext()
    {
        creators["os tsunami corridor"] = &RaidOsActionContext::os_tsunami_corridor;
        creators["os avoid twilight fissure"] = &RaidOsActionContext::os_avoid_twilight_fissure;
        creators["os main tank hold"] = &RaidOsActionContext::os_main_tank_hold;
        creators["os drake landing position"] = &RaidOsActionContext::os_drake_landing_position;
        creators["os offtank hold"] = &RaidOsActionContext::os_offtank_hold;
        creators["os raid hold"] = &RaidOsActionContext::os_raid_hold;
        creators["os return to platform"] = &RaidOsActionContext::os_return_to_platform;
        creators["os sartharion flank"] = &RaidOsActionContext::os_sartharion_flank;
        creators["os drake rear"] = &RaidOsActionContext::os_drake_rear;
        creators["os tank shapeshift"] = &RaidOsActionContext::os_tank_shapeshift;
        creators["os redirect threat"] = &RaidOsActionContext::os_redirect_threat;
        creators["os main tank cooldown"] = &RaidOsActionContext::os_main_tank_cooldown;
        creators["os tranquilize enrage"] = &RaidOsActionContext::os_tranquilize_enrage;
        creators["sartharion attack priority"] = &RaidOsActionContext::attack_priority;
        creators["enter twilight portal"] = &RaidOsActionContext::enter_twilight_portal;
        creators["exit twilight portal"] = &RaidOsActionContext::exit_twilight_portal;
    }

private:
    static Action* os_tsunami_corridor(PlayerbotAI* ai) { return new OsTsunamiCorridorAction(ai); }
    static Action* os_avoid_twilight_fissure(PlayerbotAI* ai) { return new OsAvoidTwilightFissureAction(ai); }
    static Action* os_main_tank_hold(PlayerbotAI* ai) { return new OsMainTankHoldAction(ai); }
    static Action* os_drake_landing_position(PlayerbotAI* ai) { return new OsDrakeLandingPositionAction(ai); }
    static Action* os_offtank_hold(PlayerbotAI* ai) { return new OsOffTankHoldAction(ai); }
    static Action* os_raid_hold(PlayerbotAI* ai) { return new OsRaidHoldAction(ai); }
    static Action* os_return_to_platform(PlayerbotAI* ai) { return new OsReturnToPlatformAction(ai); }
    static Action* os_sartharion_flank(PlayerbotAI* ai) { return new OsSartharionFlankAction(ai); }
    static Action* os_drake_rear(PlayerbotAI* ai) { return new OsDrakeRearAction(ai); }
    static Action* os_tank_shapeshift(PlayerbotAI* ai) { return new OsTankShapeshiftAction(ai); }
    static Action* os_redirect_threat(PlayerbotAI* ai) { return new OsRedirectThreatAction(ai); }
    static Action* os_main_tank_cooldown(PlayerbotAI* ai) { return new OsMainTankCooldownAction(ai); }
    static Action* os_tranquilize_enrage(PlayerbotAI* ai) { return new OsTranquilizeEnrageAction(ai); }
    static Action* attack_priority(PlayerbotAI* ai) { return new SartharionAttackPriorityAction(ai); }
    static Action* enter_twilight_portal(PlayerbotAI* ai) { return new EnterTwilightPortalAction(ai); }
    static Action* exit_twilight_portal(PlayerbotAI* ai) { return new ExitTwilightPortalAction(ai); }
};

#endif
