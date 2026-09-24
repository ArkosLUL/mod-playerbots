/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDTRIGGERCONTEXT_H
#define PLAYERBOTS_ULDTRIGGERCONTEXT_H

#include "BossAuraTriggers.h"
#include "NamedObjectContext.h"
#include "UldDefinitions.h"
#include "UldEncounterGate.h"
#include "UldTriggers.h"

class RaidUlduarTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidUlduarTriggerContext()
    {
        creators["mimiron reset encounter state trigger"] = &RaidUlduarTriggerContext::mimiron_reset_encounter_state_trigger;
        creators["mimiron fire resistance trigger"] = &RaidUlduarTriggerContext::mimiron_fire_resistance_trigger;
        creators["mimiron frost resistance trigger"] = &RaidUlduarTriggerContext::mimiron_frost_resistance_trigger;
        creators["mimiron shock blast trigger"] = &RaidUlduarTriggerContext::mimiron_shock_blast_trigger;
        creators["mimiron phase 1 positioning trigger"] = &RaidUlduarTriggerContext::mimiron_phase_1_positioning_trigger;
        creators["mimiron p3wx2 laser barrage trigger"] = &RaidUlduarTriggerContext::mimiron_p3wx2_laser_barrage_trigger;
        creators["mimiron arc spread trigger"] = &RaidUlduarTriggerContext::mimiron_arc_spread_trigger;
        creators["mimiron aerial command unit trigger"] = &RaidUlduarTriggerContext::mimiron_aerial_command_unit_trigger;
        creators["mimiron rocket strike trigger"] = &RaidUlduarTriggerContext::mimiron_rocket_strike_trigger;
        creators["mimiron phase 4 focus trigger"] = &RaidUlduarTriggerContext::mimiron_phase_4_focus_trigger;
        creators["mimiron magnetic core trigger"] = &RaidUlduarTriggerContext::mimiron_magnetic_core_trigger;
        creators["mimiron plasma blast defensive trigger"] =
            &RaidUlduarTriggerContext::mimiron_plasma_blast_defensive_trigger;
        creators["mimiron redirect threat trigger"] =
            &RaidUlduarTriggerContext::mimiron_redirect_threat_trigger;
        creators["mimiron set dps priority trigger"] = &RaidUlduarTriggerContext::mimiron_set_dps_priority_trigger;
        creators["mimiron proximity mine trigger"] = &RaidUlduarTriggerContext::mimiron_proximity_mine_trigger;
        creators["mimiron bomb bot trigger"] = &RaidUlduarTriggerContext::mimiron_bomb_bot_trigger;
        creators["mimiron pet control trigger"] = &RaidUlduarTriggerContext::mimiron_pet_control_trigger;
        creators["mimiron slow bomb bot trigger"] = &RaidUlduarTriggerContext::mimiron_slow_bomb_bot_trigger;
        creators["mimiron approach target trigger"] =
            &RaidUlduarTriggerContext::mimiron_approach_target_trigger;
        creators["mimiron dodge flames trigger"] = &RaidUlduarTriggerContext::mimiron_dodge_flames_trigger;
        creators["mimiron frost bomb trigger"] = &RaidUlduarTriggerContext::mimiron_frost_bomb_trigger;
        creators["mimiron fire bot trigger"] = &RaidUlduarTriggerContext::mimiron_fire_bot_trigger;

        // Applied over the whole table rather than in every trigger class: every name here carries its
        // encounter, so one pass can gate them all. RaidEncounterRules::GateOpen has what it closes.
        for (auto& entry : creators)
        {
            uint32 bossId = 0;
            if (!UldEncounterOfTrigger(entry.first, bossId))
                continue;

            ObjectCreator inner = entry.second;
            entry.second = [inner, bossId](PlayerbotAI* ai) -> Trigger*
            { return new UldGatedTrigger(ai, inner(ai), bossId); };
        }

        // After the wrap above, which would gate these a second time.
        for (EncounterDefinition const* encounter : UldEncounterDefinitions())
            encounter->RegisterTriggers(creators);
    }

private:
    static Trigger* mimiron_fire_resistance_trigger(PlayerbotAI* ai) { return new BossFireResistanceTrigger(ai, "mimiron"); }
    static Trigger* mimiron_frost_resistance_trigger(PlayerbotAI* ai) { return new MimironFrostResistanceTrigger(ai); }
    static Trigger* mimiron_shock_blast_trigger(PlayerbotAI* ai) { return new MimironShockBlastTrigger(ai); }
    static Trigger* mimiron_reset_encounter_state_trigger(PlayerbotAI* ai) { return new MimironResetEncounterStateTrigger(ai); }
    static Trigger* mimiron_phase_1_positioning_trigger(PlayerbotAI* ai) { return new MimironPhase1PositioningTrigger(ai); }
    static Trigger* mimiron_p3wx2_laser_barrage_trigger(PlayerbotAI* ai) { return new MimironP3Wx2LaserBarrageTrigger(ai); }
    static Trigger* mimiron_arc_spread_trigger(PlayerbotAI* ai) { return new MimironArcSpreadTrigger(ai); }
    static Trigger* mimiron_aerial_command_unit_trigger(PlayerbotAI* ai) { return new MimironAerialCommandUnitTrigger(ai); }
    static Trigger* mimiron_rocket_strike_trigger(PlayerbotAI* ai) { return new MimironRocketStrikeTrigger(ai); }
    static Trigger* mimiron_phase_4_focus_trigger(PlayerbotAI* ai) { return new MimironPhase4FocusTrigger(ai); }
    static Trigger* mimiron_magnetic_core_trigger(PlayerbotAI* ai) { return new MimironMagneticCoreTrigger(ai); }
    static Trigger* mimiron_plasma_blast_defensive_trigger(PlayerbotAI* ai)
    {
        return new MimironPlasmaBlastDefensiveTrigger(ai);
    }
    static Trigger* mimiron_redirect_threat_trigger(PlayerbotAI* ai)
    {
        return new MimironRedirectThreatTrigger(ai);
    }
    static Trigger* mimiron_set_dps_priority_trigger(PlayerbotAI* ai) { return new MimironSetDpsPriorityTrigger(ai); }
    static Trigger* mimiron_proximity_mine_trigger(PlayerbotAI* ai) { return new MimironProximityMineTrigger(ai); }
    static Trigger* mimiron_bomb_bot_trigger(PlayerbotAI* ai) { return new MimironBombBotTrigger(ai); }
    static Trigger* mimiron_pet_control_trigger(PlayerbotAI* ai) { return new MimironPetControlTrigger(ai); }
    static Trigger* mimiron_slow_bomb_bot_trigger(PlayerbotAI* ai) { return new MimironSlowBombBotTrigger(ai); }
    static Trigger* mimiron_approach_target_trigger(PlayerbotAI* ai)
    {
        return new MimironApproachTargetTrigger(ai);
    }
    static Trigger* mimiron_dodge_flames_trigger(PlayerbotAI* ai) { return new MimironDodgeFlamesTrigger(ai); }
    static Trigger* mimiron_frost_bomb_trigger(PlayerbotAI* ai) { return new MimironFrostBombTrigger(ai); }
    static Trigger* mimiron_fire_bot_trigger(PlayerbotAI* ai) { return new MimironFireBotTrigger(ai); }
};

#endif
