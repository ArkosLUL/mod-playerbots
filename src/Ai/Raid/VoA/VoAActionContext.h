/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOAACTIONCONTEXT_H
#define PLAYERBOTS_VOAACTIONCONTEXT_H

#include "Action.h"
#include "BossAuraActions.h"
#include "NamedObjectContext.h"
#include "PlayerbotAI.h"
#include "VoAActions.h"

class RaidVoAActionContext : public NamedObjectContext<Action>
{
public:
    RaidVoAActionContext()
    {
        creators["emalon mark boss action"] = &RaidVoAActionContext::emalon_mark_boss_action;
        creators["emalon lighting nova action"] = &RaidVoAActionContext::emalon_lighting_nova_action;
        creators["emalon overcharge action"] = &RaidVoAActionContext::emalon_overcharge_action;
        creators["emalon fall from floor action"] = &RaidVoAActionContext::emalon_fall_from_floor_action;
        creators["emalon nature resistance action"] = &RaidVoAActionContext::emalon_nature_resistance_action;
        creators["koralon fire resistance action"] = &RaidVoAActionContext::koralon_fire_resistance_action;
        creators["archavon mark boss action"] = &RaidVoAActionContext::archavon_mark_boss_action;
        creators["archavon rock shards spread action"] = &RaidVoAActionContext::archavon_rock_shards_spread_action;
        creators["archavon nature resistance action"] = &RaidVoAActionContext::archavon_nature_resistance_action;
        creators["koralon mark boss action"] = &RaidVoAActionContext::koralon_mark_boss_action;
        creators["koralon burning breath action"] = &RaidVoAActionContext::koralon_burning_breath_action;
        creators["koralon flaming cinder spread action"] = &RaidVoAActionContext::koralon_flaming_cinder_spread_action;
        creators["toravon mark boss action"] = &RaidVoAActionContext::toravon_mark_boss_action;
        creators["toravon frost resistance action"] = &RaidVoAActionContext::toravon_frost_resistance_action;
        creators["toravon freezing ground action"] = &RaidVoAActionContext::toravon_freezing_ground_action;
        creators["toravon frozen orb avoid action"] = &RaidVoAActionContext::toravon_frozen_orb_avoid_action;
    }

private:
    static Action* emalon_mark_boss_action(PlayerbotAI* ai) { return new BossMarkSkullAction(ai, "emalon the storm watcher"); }
    static Action* emalon_lighting_nova_action(PlayerbotAI* ai) { return new EmalonLightingNovaAction(ai); }
    static Action* emalon_overcharge_action(PlayerbotAI* ai) { return new EmalonOverchargeAction(ai); }
    static Action* emalon_fall_from_floor_action(PlayerbotAI* ai) { return new EmalonFallFromFloorAction(ai); }
    static Action* emalon_nature_resistance_action(PlayerbotAI* ai) { return new BossNatureResistanceAction(ai, "emalon the storm watcher"); }
    static Action* koralon_fire_resistance_action(PlayerbotAI* ai) { return new BossFireResistanceAction(ai, "koralon the flame watcher"); }
    static Action* archavon_mark_boss_action(PlayerbotAI* ai) { return new BossMarkSkullAction(ai, "archavon the stone watcher"); }
    static Action* archavon_rock_shards_spread_action(PlayerbotAI* ai) { return new ArchavonRockShardsSpreadAction(ai); }
    static Action* archavon_nature_resistance_action(PlayerbotAI* ai) { return new BossNatureResistanceAction(ai, "archavon the stone watcher"); }
    static Action* koralon_mark_boss_action(PlayerbotAI* ai) { return new BossMarkSkullAction(ai, "koralon the flame watcher"); }
    static Action* koralon_burning_breath_action(PlayerbotAI* ai) { return new KoralonBurningBreathAction(ai); }
    static Action* koralon_flaming_cinder_spread_action(PlayerbotAI* ai) { return new KoralonFlamingCinderSpreadAction(ai); }
    static Action* toravon_mark_boss_action(PlayerbotAI* ai) { return new BossMarkSkullAction(ai, "toravon the ice watcher"); }
    static Action* toravon_frost_resistance_action(PlayerbotAI* ai) { return new BossFrostResistanceAction(ai, "toravon the ice watcher"); }
    static Action* toravon_freezing_ground_action(PlayerbotAI* ai) { return new ToravonFreezingGroundAction(ai); }
    static Action* toravon_frozen_orb_avoid_action(PlayerbotAI* ai) { return new ToravonFrozenOrbAvoidAction(ai); }
};

#endif
