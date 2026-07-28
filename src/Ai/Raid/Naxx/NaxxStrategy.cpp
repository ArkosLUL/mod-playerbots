/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxStrategy.h"

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

    // Heigan the Unclean
    triggers.push_back(new TriggerNode("heigan melee",
        { NextAction("heigan dance melee", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("heigan ranged",
        { NextAction("heigan dance ranged", ACTION_RAID + 1) }
    ));

    // Priority: dispel Decrepit Fever ASAP (tank first) during Phase 1.
    triggers.push_back(new TriggerNode("heigan decrepit fever",
        { NextAction("heigan dispel decrepit fever", ACTION_RAID + 5) }
    ));

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

    // Anub'Rekhan
    triggers.push_back(new TriggerNode("anub'rekhan",
        {
            NextAction("anub'rekhan redirect threat", ACTION_RAID + 3),
            NextAction("anub'rekhan position", ACTION_RAID + 2),
            NextAction("anub'rekhan choose target", ACTION_RAID + 1)
        }
    ));

    // The swarm is the one window where losing the formation wipes the raid, so holding it outranks
    // everything else the engine might want to do.
    triggers.push_back(new TriggerNode("anub'rekhan locust swarm",
        { NextAction("anub'rekhan position", ACTION_EMERGENCY + 5) }
    ));

     // Grand Widow Faerlina
     triggers.push_back(new TriggerNode("faerlina",
        { NextAction("avoid aoe", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("faerlina frenzy",
        {
            NextAction("tranquilizing shot", ACTION_RAID + 4),
            NextAction("faerlina sacrifice worshipper", ACTION_RAID + 3)
        }
    ));

    // Maexxna
    triggers.push_back(new TriggerNode("maexxna web wrap",
        { NextAction("maexxna attack web wrap", ACTION_RAID + 5) }
    ));

    triggers.push_back(new TriggerNode("maexxna spiderlings",
        { NextAction("maexxna tank spiderlings", ACTION_RAID + 2) }
    ));

    triggers.push_back(
        new TriggerNode("maexxna",
        {
            NextAction("rear flank", ACTION_RAID + 1),
            NextAction("avoid aoe", ACTION_RAID + 1)
        })
    );

    // Gothik the Harvester
    // triggers.push_back(new TriggerNode("gothik move to assigned side",
    //     { NextAction("gothik move to assigned side", ACTION_RAID + 4) }
    // ));

    // triggers.push_back(new TriggerNode("gothik choose target",
    //     { NextAction("gothik choose target", ACTION_RAID + 1) }
    // ));

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

    // Instructor Razuvious
    triggers.push_back(new TriggerNode("razuvious tank",
        { NextAction("razuvious use obedience crystal", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("razuvious nontank",
        { NextAction("razuvious target", ACTION_RAID + 1) }
    ));

    // four horseman
    triggers.push_back(new TriggerNode("horseman attractors",
        { NextAction("horseman attract alternatively", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("horseman except attractors",
        { NextAction("horseman attack in order", ACTION_RAID + 1) }
    ));

    // Only live for the pull window, so it can outrank the attractor rotation while it lasts.
    triggers.push_back(new TriggerNode("four horsemen redirect threat",
        { NextAction("four horsemen redirect threat", ACTION_RAID + 4) }
    ));

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

    // Loatheb
    triggers.push_back(
        new TriggerNode("loatheb",
        {
            NextAction("loatheb position", ACTION_RAID + 1),
            NextAction("loatheb choose target", ACTION_RAID + 1)
        })
    );

    // Noth the Plaguebringer
    // triggers.push_back(
    //     new TriggerNode("noth",
    //     {
    //         NextAction("noth position", ACTION_RAID + 1),
    //         NextAction("noth choose target", ACTION_RAID + 1)
    //     })
    // );
    // triggers.push_back(
    //     new TriggerNode("noth curse",
    //     {
    //         NextAction("cure party member", ACTION_RAID + 2)
    //     })
    // );
}

void RaidNaxxStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new GrobbulusMultiplier(botAI));
    multipliers.push_back(new HeiganDanceMultiplier(botAI));
    multipliers.push_back(new LoathebGenericMultiplier(botAI));
    multipliers.push_back(new ThaddiusGenericMultiplier(botAI));
    multipliers.push_back(new SapphironGenericMultiplier(botAI));
    multipliers.push_back(new InstructorRazuviousGenericMultiplier(botAI));
    multipliers.push_back(new KelthuzadGenericMultiplier(botAI));
    multipliers.push_back(new AnubrekhanGenericMultiplier(botAI));
    multipliers.push_back(new FourhorsemanGenericMultiplier(botAI));
    multipliers.push_back(new GluthGenericMultiplier(botAI));
    multipliers.push_back(new NaxxThreatRedirectMultiplier(botAI));
    multipliers.push_back(new NaxxBurstWindowMultiplier(botAI));
    // multipliers.push_back(new NothGenericMultiplier(botAI));
}
