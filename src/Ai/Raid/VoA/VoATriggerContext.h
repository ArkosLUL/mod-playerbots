/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOATRIGGERCONTEXT_H
#define PLAYERBOTS_VOATRIGGERCONTEXT_H

#include "BossAuraTriggers.h"
#include "NamedObjectContext.h"
#include "VoATriggers.h"

class RaidVoATriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidVoATriggerContext()
    {
        creators["emalon mark boss trigger"] = &RaidVoATriggerContext::emalon_mark_boss_trigger;
        creators["emalon lighting nova trigger"] = &RaidVoATriggerContext::emalon_lighting_nova_trigger;
        creators["emalon overcharge trigger"] = &RaidVoATriggerContext::emalon_overcharge_trigger;
        creators["emalon fall from floor trigger"] = &RaidVoATriggerContext::emalon_fall_from_floor_trigger;
        creators["emalon main tank hold trigger"] = &RaidVoATriggerContext::emalon_main_tank_hold_trigger;
        creators["emalon ring hold trigger"] = &RaidVoATriggerContext::emalon_ring_hold_trigger;
        creators["emalon offtank hold trigger"] = &RaidVoATriggerContext::emalon_offtank_hold_trigger;
        creators["emalon attack priority trigger"] = &RaidVoATriggerContext::emalon_attack_priority_trigger;
        creators["emalon redirect threat trigger"] = &RaidVoATriggerContext::emalon_redirect_threat_trigger;
        creators["emalon nature resistance trigger"] = &RaidVoATriggerContext::emalon_nature_resistance_trigger;
        creators["koralon fire resistance trigger"] = &RaidVoATriggerContext::koralon_fire_resistance_trigger;
        creators["archavon mark boss trigger"] = &RaidVoATriggerContext::archavon_mark_boss_trigger;
        creators["archavon rock shards spread trigger"] = &RaidVoATriggerContext::archavon_rock_shards_spread_trigger;
        creators["archavon nature resistance trigger"] = &RaidVoATriggerContext::archavon_nature_resistance_trigger;
        creators["koralon mark boss trigger"] = &RaidVoATriggerContext::koralon_mark_boss_trigger;
        creators["koralon burning breath trigger"] = &RaidVoATriggerContext::koralon_burning_breath_trigger;
        creators["koralon flaming cinder spread trigger"] = &RaidVoATriggerContext::koralon_flaming_cinder_spread_trigger;
        creators["toravon mark boss trigger"] = &RaidVoATriggerContext::toravon_mark_boss_trigger;
        creators["toravon frost resistance trigger"] = &RaidVoATriggerContext::toravon_frost_resistance_trigger;
        creators["toravon freezing ground trigger"] = &RaidVoATriggerContext::toravon_freezing_ground_trigger;
        creators["toravon frozen orb avoid trigger"] = &RaidVoATriggerContext::toravon_frozen_orb_avoid_trigger;
    }

private:
    static Trigger* emalon_mark_boss_trigger(PlayerbotAI* botAI) { return new EmalonMarkBossTrigger(botAI); }
    static Trigger* emalon_lighting_nova_trigger(PlayerbotAI* botAI) { return new EmalonLightingNovaTrigger(botAI); }
    static Trigger* emalon_overcharge_trigger(PlayerbotAI* botAI) { return new EmalonOverchargeTrigger(botAI); }
    static Trigger* emalon_fall_from_floor_trigger(PlayerbotAI* botAI) { return new EmalonFallFromFloorTrigger(botAI); }
    static Trigger* emalon_main_tank_hold_trigger(PlayerbotAI* botAI) { return new EmalonMainTankHoldTrigger(botAI); }
    static Trigger* emalon_ring_hold_trigger(PlayerbotAI* botAI) { return new EmalonRingHoldTrigger(botAI); }
    static Trigger* emalon_offtank_hold_trigger(PlayerbotAI* botAI) { return new EmalonOffTankHoldTrigger(botAI); }
    static Trigger* emalon_attack_priority_trigger(PlayerbotAI* botAI) { return new EmalonAttackPriorityTrigger(botAI); }
    static Trigger* emalon_redirect_threat_trigger(PlayerbotAI* botAI) { return new EmalonRedirectThreatTrigger(botAI); }
    static Trigger* emalon_nature_resistance_trigger(PlayerbotAI* botAI) { return new BossNatureResistanceTrigger(botAI, "emalon the storm watcher"); }
    static Trigger* koralon_fire_resistance_trigger(PlayerbotAI* botAI) { return new BossFireResistanceTrigger(botAI, "koralon the flame watcher"); }
    static Trigger* archavon_mark_boss_trigger(PlayerbotAI* botAI) { return new BossMarkSkullTrigger(botAI, "archavon the stone watcher"); }
    static Trigger* archavon_rock_shards_spread_trigger(PlayerbotAI* botAI) { return new ArchavonRockShardsSpreadTrigger(botAI); }
    static Trigger* archavon_nature_resistance_trigger(PlayerbotAI* botAI) { return new BossNatureResistanceTrigger(botAI, "archavon the stone watcher"); }
    static Trigger* koralon_mark_boss_trigger(PlayerbotAI* botAI) { return new BossMarkSkullTrigger(botAI, "koralon the flame watcher"); }
    static Trigger* koralon_burning_breath_trigger(PlayerbotAI* botAI) { return new KoralonBurningBreathTrigger(botAI); }
    static Trigger* koralon_flaming_cinder_spread_trigger(PlayerbotAI* botAI) { return new KoralonFlamingCinderSpreadTrigger(botAI); }
    static Trigger* toravon_mark_boss_trigger(PlayerbotAI* botAI) { return new BossMarkSkullTrigger(botAI, "toravon the ice watcher"); }
    static Trigger* toravon_frost_resistance_trigger(PlayerbotAI* botAI) { return new BossFrostResistanceTrigger(botAI, "toravon the ice watcher"); }
    static Trigger* toravon_freezing_ground_trigger(PlayerbotAI* botAI) { return new ToravonFreezingGroundTrigger(botAI); }
    static Trigger* toravon_frozen_orb_avoid_trigger(PlayerbotAI* botAI) { return new ToravonFrozenOrbAvoidTrigger(botAI); }
};

#endif
