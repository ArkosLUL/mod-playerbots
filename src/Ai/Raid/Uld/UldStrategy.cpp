/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldStrategy.h"

#include "Playerbots.h"
#include "UldDefinitions.h"
#include "UldEncounter_Mimiron.h"
#include "UldMultipliers.h"

// The kept Emergency Fire Bots are the raid's fire suppression, and only Mimiron's own dps picker
// knew it: that node stands down for tanks, and the target guard beside it only zeroes
// DpsAssistAction, so the generic tank picker killed the protected ones in one Firefighter pull.
// Excluding them here covers every picker at once - tank, dps, dps aoe and the attackers.
void RaidUlduarStrategy::AppendTargetExclusions(GuidSet& exclusions,
                                               TargetValueExclusionType /*type*/)
{
    for (ObjectGuid const& guid : GetMimironKeptFireBots(botAI, botAI->GetBot()))
        exclusions.insert(guid);
}

void RaidUlduarStrategy::OnTick()
{
    for (EncounterDefinition const* encounter : UldEncounterDefinitions())
        encounter->OnTick(botAI);
}

void RaidUlduarStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    //
    // Flame Leviathan
    //
    UldFlameLeviathanDefinition().AddTriggerNodes(triggers);

    //
    // Razorscale
    //
    UldRazorscaleDefinition().AddTriggerNodes(triggers);

    //
    // Ignis
    //
    UldIgnisDefinition().AddTriggerNodes(triggers);

    //
    // XT-002 Deconstructor
    //
    UldXT002Definition().AddTriggerNodes(triggers);

    //
    // Iron Assembly
    //
    UldIronAssemblyDefinition().AddTriggerNodes(triggers);

    //
    // Kologarn
    //
    UldKologarnDefinition().AddTriggerNodes(triggers);

    //
    // Auriaya
    //
    UldAuriayaDefinition().AddTriggerNodes(triggers);

    //
    // Hodir
    //
    UldHodirDefinition().AddTriggerNodes(triggers);

    //
    // Freya
    //
    UldFreyaDefinition().AddTriggerNodes(triggers);

    //
    // Thorim
    //
    UldThorimDefinition().AddTriggerNodes(triggers);

    //
    // Mimiron. Laser Barrage outranks everything else here: its cone one-shots. Ranked below it by
    // what a hit actually costs - Shock Blast is 100000 damage in a 15 yd circle, a Proximity Mine
    // is 3 yd and healable, which is why the mine dodge sits under the whole rest of the ladder.
    //
    // No Rapid Burst node. A bot steps 1.1 to 1.8 s into the 3 s cone and still takes a hit after
    // it, so the step saved ~0.3 ticks and cost a walk back to the slot. Destinations are screened
    // against the live cone instead, so nothing walks into one.

    triggers.push_back(new TriggerNode(
        "mimiron p3wx2 laser barrage trigger",
        { NextAction("mimiron p3wx2 laser barrage action", ACTION_RAID + 7) }));

    triggers.push_back(new TriggerNode(
        "mimiron shock blast trigger",
        { NextAction("mimiron shock blast action", ACTION_RAID + 5.5f) }));

    triggers.push_back(new TriggerNode(
        "mimiron fire resistance trigger",
        { NextAction("mimiron fire resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron frost resistance trigger",
        { NextAction("mimiron frost resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron phase 1 positioning trigger",
        { NextAction("mimiron phase 1 positioning action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron arc spread trigger",
        { NextAction("mimiron arc spread action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron aerial command unit trigger",
        { NextAction("mimiron aerial command unit action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron rocket strike trigger",
        { NextAction("mimiron rocket strike action", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "mimiron phase 4 focus trigger",
        { NextAction("mimiron phase 4 focus action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron magnetic core trigger",
        { NextAction("mimiron magnetic core action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "mimiron plasma blast defensive trigger",
        { NextAction("mimiron plasma blast defensive action", ACTION_RAID + 6) }));

    triggers.push_back(new TriggerNode(
        "mimiron redirect threat trigger",
        { NextAction("mimiron redirect threat action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "mimiron set dps priority trigger",
        { NextAction("mimiron set dps priority action", ACTION_RAID) }));

    // Last of the Mimiron nodes and tied with none of them, so it only ever takes a tick no other
    // mechanic wants. A mine can never cost the raid a dodge, a taunt or a core delivery.
    triggers.push_back(new TriggerNode(
        "mimiron proximity mine trigger",
        { NextAction("mimiron proximity mine action", ACTION_RAID - 1) }));

    triggers.push_back(new TriggerNode(
        "mimiron bomb bot trigger",
        { NextAction("mimiron bomb bot action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "mimiron slow bomb bot trigger",
        { NextAction("mimiron slow bomb bot action", ACTION_RAID + 2) }));

    // The action always returns false, so this only ever redirects the pet - the bot keeps its tick.
    triggers.push_back(new TriggerNode(
        "mimiron pet control trigger",
        { NextAction("mimiron pet control action", ACTION_RAID) }));

    // Hard mode (config-gated): step out of the persistent ground fire and clear the Frost Bomb.
    //
    // The bomb outranks the fire, and the gap between them is the point. Both used to sit on
    // ACTION_RAID + 4 alongside the rocket strike, the queue breaks an exact tie by push order, and
    // the engine stops the tick at the first action that returns true - so the fire step, which wins
    // that tie, ended the tick 143 to 221 times a pull and the bomb node was reached 6 to 13. Fire is
    // 3000 a second and healable; the explosion is 47000 in 30 yd against a 24000 health pool.
    triggers.push_back(new TriggerNode(
        "mimiron dodge flames trigger",
        { NextAction("mimiron dodge flames action", ACTION_RAID + 4) }));

    // Below every Mimiron node and above the generic reach nodes at ACTION_HIGH, which is the whole
    // point: the formation and the dodges answer first, and only a bot they left with nothing to do
    // but walk at its target gets steered round the fire instead of into it.
    triggers.push_back(new TriggerNode(
        "mimiron approach target trigger",
        { NextAction("mimiron approach target action", ACTION_RAID - 2) }));

    triggers.push_back(new TriggerNode(
        "mimiron frost bomb trigger",
        { NextAction("mimiron frost bomb action", ACTION_RAID + 6) }));

    // Below the fire dodge, whose fan already refuses the spray line and the silence. A fire bot only
    // sprays when it reaches a flame; the fire ticks every second.
    triggers.push_back(new TriggerNode(
        "mimiron fire bot trigger",
        { NextAction("mimiron fire bot action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "mimiron reset encounter state trigger",
        { NextAction("mimiron reset encounter state action", ACTION_RAID) }));

    //
    // General Vezax
    //
    UldVezaxDefinition().AddTriggerNodes(triggers);

    //
    // Yogg-Saron
    //
    UldYoggSaronDefinition().AddTriggerNodes(triggers);

    //
    // Algalon the Observer
    //
    UldAlgalonDefinition().AddTriggerNodes(triggers);
}

void RaidUlduarStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    UldAlgalonDefinition().AddMultipliers(botAI, multipliers);

    UldXT002Definition().AddMultipliers(botAI, multipliers);

    // Mimiron picks every non-tank target in code, so the generic picker has to be shut out
    multipliers.push_back(new MimironTargetGuardMultiplier(botAI));
    multipliers.push_back(new MimironChargeGuardMultiplier(botAI));
    multipliers.push_back(new MimironAvoidAoeGuardMultiplier(botAI));
    multipliers.push_back(new MimironFormationGuardMultiplier(botAI));
    multipliers.push_back(new MimironGenericRedirectGuardMultiplier(botAI));
    multipliers.push_back(new MimironTankAnchorGuardMultiplier(botAI));
    multipliers.push_back(new MimironDrinkGuardMultiplier(botAI));
    multipliers.push_back(new MimironFrostBombGuardMultiplier(botAI));
    multipliers.push_back(new MimironFireHoldGuardMultiplier(botAI));
    multipliers.push_back(new MimironFireBotAoeGuardMultiplier(botAI));
    multipliers.push_back(new MimironPlasmaDefensiveHoldMultiplier(botAI));
    multipliers.push_back(new MimironPaladinAuraMultiplier(botAI));
    multipliers.push_back(new MimironStormCooldownHoldMultiplier(botAI));
    multipliers.push_back(new MimironVx001FacingGuardMultiplier(botAI));

    UldIronAssemblyDefinition().AddMultipliers(botAI, multipliers);

    UldThorimDefinition().AddMultipliers(botAI, multipliers);

    // Hold the class-generic threat redirects on the bosses where the main tank is the wrong sink
    multipliers.push_back(new UldThreatRedirectMultiplier(botAI));

    // Hold the burst cooldowns on the bosses whose DPS check is not the pull
    multipliers.push_back(new UlduarBurstWindowMultiplier(botAI));

    UldRazorscaleDefinition().AddMultipliers(botAI, multipliers);

    UldIgnisDefinition().AddMultipliers(botAI, multipliers);

    UldKologarnDefinition().AddMultipliers(botAI, multipliers);

    UldFreyaDefinition().AddMultipliers(botAI, multipliers);

    UldFlameLeviathanDefinition().AddMultipliers(botAI, multipliers);

    UldVezaxDefinition().AddMultipliers(botAI, multipliers);

    UldAuriayaDefinition().AddMultipliers(botAI, multipliers);
    UldHodirDefinition().AddMultipliers(botAI, multipliers);

    UldYoggSaronDefinition().AddMultipliers(botAI, multipliers);
}
