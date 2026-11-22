/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldStrategy.h"

#include "Playerbots.h"
#include "UldDefinitions.h"
#include "UldEncounter_Algalon.h"
#include "UldEncounter_Mimiron.h"
#include "UldEncounter_YoggSaron.h"
#include "UldMultipliers.h"

void RaidUlduarStrategy::AppendTargetExclusionsFor(GuidSet& exclusions, TargetValueExclusionType type,
                                                   GuidVector const& candidates)
{
    for (ObjectGuid const& guid : GetMimironKeptFireBots(botAI, botAI->GetBot()))
        exclusions.insert(guid);

    AppendAlgalonTargetExclusions(botAI, exclusions);

    if (type == TargetValueExclusionType::DpsTarget || type == TargetValueExclusionType::DebuffTarget)
        AppendYoggSaronTargetExclusions(botAI, exclusions, candidates);
}

void RaidUlduarStrategy::OnTick()
{
    for (EncounterDefinition const* encounter : UldEncounterDefinitions())
        encounter->OnTick(botAI);
}

void RaidUlduarStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    UldFlameLeviathanDefinition().AddTriggerNodes(triggers);

    UldRazorscaleDefinition().AddTriggerNodes(triggers);

    UldIgnisDefinition().AddTriggerNodes(triggers);

    UldXT002Definition().AddTriggerNodes(triggers);

    UldIronAssemblyDefinition().AddTriggerNodes(triggers);

    UldKologarnDefinition().AddTriggerNodes(triggers);

    UldAuriayaDefinition().AddTriggerNodes(triggers);

    UldHodirDefinition().AddTriggerNodes(triggers);

    UldFreyaDefinition().AddTriggerNodes(triggers);

    UldThorimDefinition().AddTriggerNodes(triggers);

    UldMimironDefinition().AddTriggerNodes(triggers);

    UldVezaxDefinition().AddTriggerNodes(triggers);

    UldYoggSaronDefinition().AddTriggerNodes(triggers);

    UldAlgalonDefinition().AddTriggerNodes(triggers);
}

void RaidUlduarStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    UldAlgalonDefinition().AddMultipliers(botAI, multipliers);

    UldXT002Definition().AddMultipliers(botAI, multipliers);

    UldMimironDefinition().AddMultipliers(botAI, multipliers);

    UldIronAssemblyDefinition().AddMultipliers(botAI, multipliers);

    UldThorimDefinition().AddMultipliers(botAI, multipliers);

    multipliers.push_back(new UldThreatRedirectMultiplier(botAI));

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
