/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldStrategy.h"

#include "UldMultipliers.h"

void RaidUlduarStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    //
    // Flame Leviathan
    //
    triggers.push_back(new TriggerNode(
        "flame leviathan vehicle near",
        { NextAction("flame leviathan enter vehicle", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "flame leviathan on vehicle",
        { NextAction("flame leviathan vehicle", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "flame leviathan tower hazard",
        { NextAction("flame leviathan tower hazard", ACTION_RAID + 3) }));

    //
    // Razorscale
    //
    triggers.push_back(new TriggerNode(
        "razorscale avoid devouring flames",
        { NextAction("razorscale avoid devouring flames", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "razorscale avoid sentinel",
        { NextAction("razorscale avoid sentinel", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "razorscale flying alone",
    { NextAction("razorscale ignore flying alone", ACTION_MOVE + 5) }));

    triggers.push_back(new TriggerNode(
        "razorscale avoid whirlwind",
        { NextAction("razorscale avoid whirlwind", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "razorscale grounded",
        { NextAction("razorscale grounded", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "razorscale harpoon trigger",
        { NextAction("razorscale harpoon action", ACTION_MOVE) }));

    triggers.push_back(new TriggerNode(
        "razorscale fuse armor trigger",
        { NextAction("razorscale fuse armor action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "razorscale focus caster trigger",
        { NextAction("razorscale focus caster action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "razorscale flame breath trigger",
        { NextAction("razorscale flame breath action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "razorscale fire resistance trigger",
        { NextAction("razorscale fire resistance action", ACTION_RAID) }));

    //
    // Ignis
    //
    triggers.push_back(new TriggerNode(
        "ignis fire resistance trigger",
        { NextAction("ignis fire resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "ignis scorched ground trigger",
        { NextAction("ignis scorched ground action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "ignis iron construct trigger",
        { NextAction("ignis iron construct action", ACTION_RAID) }));

    //
    // Iron Assembly
    //
    triggers.push_back(new TriggerNode(
        "iron assembly lightning tendrils trigger",
        { NextAction("iron assembly lightning tendrils action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "iron assembly overload trigger",
        { NextAction("iron assembly overload action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "iron assembly rune of power trigger",
        { NextAction("iron assembly rune of power action", ACTION_RAID) }));

    // Hard mode (config-gated): enforce Steelbreaker-last kill order and tank-swap his empowered kit.
    triggers.push_back(new TriggerNode(
        "iron assembly kill order trigger",
        { NextAction("iron assembly kill order action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "iron assembly fusion punch swap trigger",
        { NextAction("iron assembly fusion punch swap action", ACTION_RAID + 2) }));

    //
    // Kologarn
    //
    triggers.push_back(new TriggerNode(
        "kologarn fall from floor trigger",
        { NextAction("kologarn fall from floor action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "kologarn rti target trigger",
        { NextAction("kologarn rti target action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "kologarn eyebeam trigger",
        { NextAction("kologarn eyebeam action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "kologarn attack dps target trigger",
        { NextAction("attack rti target", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "kologarn mark dps target trigger",
        { NextAction("kologarn mark dps target action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "kologarn nature resistance trigger",
        { NextAction("kologarn nature resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "kologarn rubble slowdown trigger",
        { NextAction("kologarn rubble slowdown action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "kologarn crunch armor trigger",
        { NextAction("kologarn crunch armor action", ACTION_RAID) }));

    //
    // Auriaya
    //
    triggers.push_back(new TriggerNode(
        "auriaya fall from floor trigger",
        { NextAction("auriaya fall from floor action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "auriaya seeping essence trigger",
        { NextAction("auriaya seeping essence action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "auriaya sonic screech trigger",
        { NextAction("auriaya sonic screech action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "auriaya mark dps target trigger",
        { NextAction("auriaya mark dps target action", ACTION_RAID) }));

    //
    // Hodir
    //
    triggers.push_back(new TriggerNode(
        "hodir near snowpacked icicle",
        { NextAction("hodir move snowpacked icicle", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "hodir biting cold",
        { NextAction("hodir biting cold jump", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "hodir frost resistance trigger",
        { NextAction("hodir frost resistance action", ACTION_RAID) }));

    // Hard mode (config-gated): win the 3-minute Rare Cache race - free the helpers, spread Storm
    // Cloud, and sit in a Toasty Fire when Biting Cold stacks. Helper-freeing stays below the
    // snowpacked-icicle move (ACTION_RAID + 1) so surviving a Flash Freeze always wins over running
    // off to a helper block when both want the bot at once.
    triggers.push_back(new TriggerNode(
        "hodir free frozen helper",
        { NextAction("hodir free frozen helper", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "hodir spread storm cloud",
        { NextAction("hodir spread storm cloud", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "hodir move to toasty fire",
        { NextAction("hodir move to toasty fire", ACTION_RAID) }));

    //
    // Freya
    //
    triggers.push_back(new TriggerNode(
        "freya near nature bomb",
        { NextAction("freya move away nature bomb", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "freya nature resistance trigger",
        { NextAction("freya nature resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "freya fire resistance trigger",
        { NextAction("freya fire resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "freya mark dps target trigger",
        { NextAction("freya mark dps target action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "freya move to healing spore trigger",
        { NextAction("freya move to healing spore action", ACTION_RAID) }));

    // Hard mode (config-gated): break out of Iron Roots and dodge the Unstable Sun Beam. Breaking the
    // root outranks the dodge - a rooted bot can't move, so it has to free itself before it can step out.
    triggers.push_back(new TriggerNode(
        "freya break iron roots",
        { NextAction("freya break iron roots", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "freya dodge unstable sun beam",
        { NextAction("freya dodge unstable sun beam", ACTION_RAID + 2) }));

    //
    // Thorim
    //
    triggers.push_back(new TriggerNode(
        "thorim nature resistance trigger",
        { NextAction("thorim nature resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "thorim frost resistance trigger",
        { NextAction("thorim frost resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "thorim unbalancing strike trigger",
        { NextAction("thorim unbalancing strike action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "thorim unbalancing strike swap trigger",
        { NextAction("thorim unbalancing strike swap action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "thorim mark dps target trigger",
        { NextAction("thorim mark dps target action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "thorim gauntlet positioning trigger",
        { NextAction("thorim gauntlet positioning action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "thorim arena positioning trigger",
        { NextAction("thorim arena positioning action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "thorim fall from floor trigger",
        { NextAction("thorim fall from floor action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "thorim phase 2 positioning trigger",
        { NextAction("thorim phase 2 positioning action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "thorim sif blizzard trigger",
        { NextAction("thorim sif blizzard action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "thorim sif frost nova trigger",
        { NextAction("thorim sif frost nova action", ACTION_RAID + 3) }));

    //
    // Mimiron
    //
    triggers.push_back(new TriggerNode(
        "mimiron p3wx2 laser barrage trigger",
        { NextAction("mimiron p3wx2 laser barrage action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "mimiron shock blast trigger",
        { NextAction("mimiron shock blast action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "mimiron fire resistance trigger",
        { NextAction("mimiron fire resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron phase 1 positioning trigger",
        { NextAction("mimiron phase 1 positioning action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron rapid burst trigger",
        { NextAction("mimiron rapid burst action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron aerial command unit trigger",
        { NextAction("mimiron aerial command unit action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron rocket strike trigger",
        { NextAction("mimiron rocket strike action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron phase 4 mark dps trigger",
        { NextAction("mimiron phase 4 mark dps action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron cheat trigger",
        { NextAction("mimiron cheat action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron proximity mine trigger",
        { NextAction("mimiron proximity mine action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "mimiron bomb bot trigger",
        { NextAction("mimiron bomb bot action", ACTION_RAID + 2) }));

    // Hard mode (config-gated): step out of the persistent ground fire and clear the Frost Bomb.
    triggers.push_back(new TriggerNode(
        "mimiron dodge flames trigger",
        { NextAction("mimiron dodge flames action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "mimiron frost bomb trigger",
        { NextAction("mimiron frost bomb action", ACTION_RAID + 3) }));

    //
    // General Vezax
    //
    triggers.push_back(new TriggerNode(
        "vezax cheat trigger",
        { NextAction("vezax cheat action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "vezax shadow crash trigger",
        { NextAction("vezax shadow crash action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "vezax saronite vapors trigger",
        { NextAction("vezax saronite vapors action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "vezax mark of the faceless trigger",
        { NextAction("vezax mark of the faceless action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "vezax shadow resistance trigger",
        { NextAction("vezax shadow resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "vezax saronite animus trigger",
        { NextAction("vezax saronite animus action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "vezax profound darkness trigger",
        { NextAction("vezax profound darkness action", ACTION_RAID + 2) }));

    //
    // Yogg-Saron
    //
    triggers.push_back(new TriggerNode(
        "sara shadow resistance trigger",
        { NextAction("sara shadow resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron shadow resistance trigger",
        { NextAction("yogg-saron shadow resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron ominous cloud cheat trigger",
        { NextAction("yogg-saron ominous cloud cheat action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron guardian positioning trigger",
        { NextAction("yogg-saron guardian positioning action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron sanity trigger",
        { NextAction("yogg-saron sanity action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron death orb trigger",
        { NextAction("yogg-saron death orb action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron malady of the mind trigger",
        { NextAction("yogg-saron malady of the mind action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron mark target trigger",
        { NextAction("yogg-saron mark target action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron brain link trigger",
        { NextAction("yogg-saron brain link action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron move to enter portal trigger",
        { NextAction("yogg-saron move to enter portal action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron use portal trigger",
        { NextAction("yogg-saron use portal action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron fall from floor trigger",
        { NextAction("yogg-saron fall from floor action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron boss room movement cheat trigger",
        { NextAction("yogg-saron boss room movement cheat action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron illusion room trigger",
        { NextAction("yogg-saron illusion room action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron move to exit portal trigger",
        { NextAction("yogg-saron move to exit portal action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron lunatic gaze trigger",
        { NextAction("yogg-saron lunatic gaze action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron phase 3 positioning trigger",
        { NextAction("yogg-saron phase 3 positioning action", ACTION_RAID) }));

    // Reduced-Keeper (Thorim-only) hard mode
    triggers.push_back(new TriggerNode(
        "yogg-saron crusher tentacle trigger",
        { NextAction("yogg-saron crusher tentacle action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron guardian control trigger",
        { NextAction("yogg-saron guardian control action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron sanity conservation trigger",
        { NextAction("yogg-saron sanity conservation action", ACTION_RAID + 5) }));

    //
    // Algalon the Observer
    //
    triggers.push_back(new TriggerNode(
        "algalon cosmic smash trigger",
        { NextAction("algalon cosmic smash action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "algalon big bang trigger",
        { NextAction("algalon big bang hide action", ACTION_EMERGENCY + 1) }));

    triggers.push_back(new TriggerNode(
        "algalon big bang soak trigger",
        { NextAction("algalon big bang soak action", ACTION_EMERGENCY + 1) }));

    triggers.push_back(new TriggerNode(
        "algalon phase punch swap trigger",
        { NextAction("algalon phase punch swap action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "algalon constellation kite trigger",
        { NextAction("algalon constellation kite action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "algalon dark matter trigger",
        { NextAction("algalon dark matter mark action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "algalon collapsing star trigger",
        { NextAction("algalon collapsing star mark action", ACTION_RAID) }));
}

void RaidUlduarStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    // Reserve the Big Bang soaker priest's Dispersion for the Big Bang cast
    multipliers.push_back(new AlgalonMultiplier(botAI));
}
