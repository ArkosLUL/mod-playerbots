/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxStrategy.h"
#include "NaxxDefinitions.h"
#include "NaxxMultipliers.h"

void RaidNaxxStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Grobbulus
    triggers.push_back(new TriggerNode("mutating injection melee",
        { NextAction("grobbulus move away", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("mutating injection ranged",
        { NextAction("grobbulus go behind the boss", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("mutating injection removed",
        { NextAction("grobbulus move center", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("grobbulus cloud",
        { NextAction("rotate grobbulus", ACTION_RAID + 1) }
    ));

    NaxxHeiganDefinition().AddTriggerNodes(triggers);

    // Kel'Thuzad
    triggers.push_back(
        new TriggerNode("kel'thuzad",
        {
            NextAction("kel'thuzad misdirect boss to main tank", ACTION_RAID + 3),
            NextAction("kel'thuzad position", ACTION_RAID + 2),
            NextAction("kel'thuzad choose target", ACTION_RAID + 1)
        })
    );

    // Emergency priority so the flee beats every P2 positioning action by construction.
    triggers.push_back(new TriggerNode("kel'thuzad shadow fissure",
        { NextAction("kel'thuzad flee shadow fissure", ACTION_EMERGENCY + 6) }
    ));

    triggers.push_back(new TriggerNode("kel'thuzad chains",
        { NextAction("kel'thuzad cyclone chained", ACTION_EMERGENCY + 5) }
    ));

    NaxxAnubrekhanDefinition().AddTriggerNodes(triggers);
    NaxxFaerlinaDefinition().AddTriggerNodes(triggers);
    NaxxMaexxnaDefinition().AddTriggerNodes(triggers);

    NaxxGothikDefinition().AddTriggerNodes(triggers);

    // Patchwerk
    // triggers.push_back(new TriggerNode("patchwerk tank",
    //     { NextAction("tank face", ACTION_RAID + 2) }
    // ));

    // triggers.push_back(new TriggerNode("patchwerk ranged",
    //     { NextAction("patchwerk ranged position", ACTION_RAID + 2) }
    // ));

    // triggers.push_back(new TriggerNode("patchwerk non-tank",
    //     { NextAction("rear flank", ACTION_RAID + 1) }
    // ));

    // Thaddius
    // Pre-pull only. Splits the raid onto the two adds on room entry so the pull does not start
    // with everyone stacked in one blob - phase 1 is on a 5 minute enrage.
    triggers.push_back(new TriggerNode("thaddius prepull split",
        { NextAction("thaddius prepull split", ACTION_RAID) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase pet",
        { NextAction("thaddius attack nearest pet", ACTION_RAID + 6) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase pet lose aggro",
        { NextAction("taunt spell", ACTION_RAID + 7) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase transition",
        { NextAction("thaddius move to platform", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase thaddius",
        { NextAction("thaddius move polarity", ACTION_RAID + 1) }
    ));

    // Below the pet-phase taunt. It only ever casts the redirect buff, so outranking the
    // positioning nodes costs a GCD, not a Polarity Shift.
    triggers.push_back(new TriggerNode("thaddius redirect threat",
        { NextAction("thaddius redirect threat", ACTION_RAID + 3) }
    ));

    NaxxRazuviousDefinition().AddTriggerNodes(triggers);
    NaxxFourHorsemenDefinition().AddTriggerNodes(triggers);

    // sapphiron
    triggers.push_back(new TriggerNode("sapphiron ground",
        { NextAction("sapphiron ground position", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("sapphiron flight",
        { NextAction("sapphiron flight position", ACTION_RAID + 1) }
    ));

    // Gluth
    triggers.push_back(
        new TriggerNode("gluth",
        {
            NextAction("gluth choose target", ACTION_RAID + 1),
            NextAction("gluth position", ACTION_RAID + 1),
            NextAction("gluth slowdown", ACTION_RAID)
        })
    );

    triggers.push_back(new TriggerNode("gluth main tank mortal wound",
        { NextAction("taunt spell", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("gluth redirect threat",
        { NextAction("gluth redirect threat", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("gluth frenzy",
        { NextAction("gluth tranquilizing shot", ACTION_RAID + 4) }
    ));

    triggers.push_back(new TriggerNode("gluth low health zombie aoe",
        {
            NextAction("starfall", ACTION_RAID + 1),
            NextAction("blizzard", ACTION_RAID + 1),
            NextAction("volley", ACTION_RAID + 1),
            NextAction("rain of fire", ACTION_RAID + 1)
        })
    );

    NaxxLoathebDefinition().AddTriggerNodes(triggers);
    NaxxNothDefinition().AddTriggerNodes(triggers);
}

void RaidNaxxStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new GrobbulusMultiplier(botAI));
    NaxxHeiganDefinition().AddMultipliers(botAI, multipliers);
    NaxxLoathebDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new ThaddiusPrepullMultiplier(botAI));
    multipliers.push_back(new ThaddiusGenericMultiplier(botAI));
    multipliers.push_back(new SapphironGenericMultiplier(botAI));
    NaxxRazuviousDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new KelthuzadGenericMultiplier(botAI));
    NaxxAnubrekhanDefinition().AddMultipliers(botAI, multipliers);
    NaxxFourHorsemenDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new GluthGenericMultiplier(botAI));
    NaxxGothikDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new NaxxThreatRedirectMultiplier(botAI));
    multipliers.push_back(new NaxxBurstWindowMultiplier(botAI));
    NaxxNothDefinition().AddMultipliers(botAI, multipliers);
}
