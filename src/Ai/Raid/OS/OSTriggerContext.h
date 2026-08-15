/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSTRIGGERCONTEXT_H
#define PLAYERBOTS_OSTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "OSTriggers.h"

class RaidOsTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidOsTriggerContext()
    {
        creators["os tsunami corridor"] = &RaidOsTriggerContext::os_tsunami_corridor;
        creators["os twilight fissure"] = &RaidOsTriggerContext::os_twilight_fissure;
        creators["os main tank hold"] = &RaidOsTriggerContext::os_main_tank_hold;
        creators["os drake landing"] = &RaidOsTriggerContext::os_drake_landing;
        creators["os offtank hold"] = &RaidOsTriggerContext::os_offtank_hold;
        creators["os redirect threat"] = &RaidOsTriggerContext::os_redirect_threat;
        creators["os main tank cooldown"] = &RaidOsTriggerContext::os_main_tank_cooldown;
        creators["os tranquilize"] = &RaidOsTriggerContext::os_tranquilize;
        creators["os raid hold"] = &RaidOsTriggerContext::os_raid_hold;
        creators["os off platform"] = &RaidOsTriggerContext::os_off_platform;
        creators["os sartharion flank"] = &RaidOsTriggerContext::os_sartharion_flank;
        creators["os drake rear"] = &RaidOsTriggerContext::os_drake_rear;
        creators["os tank shapeshift"] = &RaidOsTriggerContext::os_tank_shapeshift;
        creators["sartharion dps"] = &RaidOsTriggerContext::sartharion_dps;
        creators["sartharion melee positioning"] = &RaidOsTriggerContext::sartharion_melee;
        creators["twilight portal enter"] = &RaidOsTriggerContext::twilight_portal_enter;
        creators["twilight portal exit"] = &RaidOsTriggerContext::twilight_portal_exit;
    }

private:
    static Trigger* os_tsunami_corridor(PlayerbotAI* ai) { return new OsTsunamiCorridorTrigger(ai); }
    static Trigger* os_twilight_fissure(PlayerbotAI* ai) { return new OsTwilightFissureTrigger(ai); }
    static Trigger* os_main_tank_hold(PlayerbotAI* ai) { return new OsMainTankHoldTrigger(ai); }
    static Trigger* os_drake_landing(PlayerbotAI* ai) { return new OsDrakeLandingTrigger(ai); }
    static Trigger* os_offtank_hold(PlayerbotAI* ai) { return new OsOffTankHoldTrigger(ai); }
    static Trigger* os_redirect_threat(PlayerbotAI* ai) { return new OsRedirectThreatTrigger(ai); }
    static Trigger* os_main_tank_cooldown(PlayerbotAI* ai) { return new OsMainTankCooldownTrigger(ai); }
    static Trigger* os_tranquilize(PlayerbotAI* ai) { return new OsTranquilizeTrigger(ai); }
    static Trigger* os_raid_hold(PlayerbotAI* ai) { return new OsRaidHoldTrigger(ai); }
    static Trigger* os_off_platform(PlayerbotAI* ai) { return new OsOffPlatformTrigger(ai); }
    static Trigger* os_sartharion_flank(PlayerbotAI* ai) { return new OsSartharionFlankTrigger(ai); }
    static Trigger* os_drake_rear(PlayerbotAI* ai) { return new OsDrakeRearTrigger(ai); }
    static Trigger* os_tank_shapeshift(PlayerbotAI* ai) { return new OsTankShapeshiftTrigger(ai); }
    static Trigger* sartharion_dps(PlayerbotAI* ai) { return new SartharionDpsTrigger(ai); }
    static Trigger* sartharion_melee(PlayerbotAI* ai) { return new SartharionMeleePositioningTrigger(ai); }
    static Trigger* twilight_portal_enter(PlayerbotAI* ai) { return new TwilightPortalEnterTrigger(ai); }
    static Trigger* twilight_portal_exit(PlayerbotAI* ai) { return new TwilightPortalExitTrigger(ai); }
};

#endif
