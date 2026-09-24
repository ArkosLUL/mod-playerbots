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
    // Mimiron
    //
    UldMimironDefinition().AddTriggerNodes(triggers);

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

    UldMimironDefinition().AddMultipliers(botAI, multipliers);

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
