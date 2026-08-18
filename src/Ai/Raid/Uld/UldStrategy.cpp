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
        "flame leviathan flame vents",
        { NextAction("flame leviathan interrupt vents", ACTION_RAID + 4) }));

    // Same action as the routine node below. The engine caches actions by name, so both nodes drive
    // one instance and one latched destination - still a single owner of the MotionMaster, just
    // promoted above the rotation while being chased or standing in a hazard.
    triggers.push_back(new TriggerNode(
        "flame leviathan drive urgent",
        { NextAction("flame leviathan drive", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "flame leviathan vehicle near",
        { NextAction("flame leviathan enter vehicle", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "flame leviathan on vehicle",
        { NextAction("flame leviathan vehicle", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "flame leviathan on vehicle",
        { NextAction("flame leviathan drive", ACTION_RAID + 0.5f) }));

    //
    // Razorscale
    //
    // Above the sentinel and whirlwind spacing moves: those step 8yd on a bearing that knows nothing
    // about the fire, and a Devouring Flame patch is the one thing here that kills a bot outright.
    triggers.push_back(new TriggerNode(
        "razorscale avoid devouring flames",
        { NextAction("razorscale avoid devouring flames", ACTION_RAID + 4) }));

    // Never consumes the tick - the pet order is a side effect, so this sits on top harmlessly.
    triggers.push_back(new TriggerNode(
        "razorscale pet control trigger",
        { NextAction("razorscale pet control action", ACTION_RAID + 5) }));

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
        "razorscale kill target trigger",
        { NextAction("razorscale kill target action", ACTION_RAID) }));

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

    // An Iron Construct only dies to the Molten -> Brittle -> Shatter chain, and every one left alive
    // is another stack of Strength of the Creator on the boss, so the kite outranks the raid's damage.
    // The mark sits above it so the raid's kill target is already current when the construct turns
    // Brittle. Standing next to a Molten construct and sitting in a Slag Pot both kill a bot outright.
    triggers.push_back(new TriggerNode(
        "ignis brittle construct mark trigger",
        { NextAction("ignis brittle construct mark action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "ignis construct tank trigger",
        { NextAction("ignis construct tank action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "ignis attack brittle construct trigger",
        { NextAction("attack rti target", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "ignis molten construct avoid trigger",
        { NextAction("ignis molten construct avoid action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "ignis slag pot heal trigger",
        { NextAction("ignis slag pot heal action", ACTION_EMERGENCY + 1) }));

    //
    // XT-002 Deconstructor
    //
    // Dodging the debuff splash and the Boombot blast outranks everything else: both one-shot a bot
    // that stands in them. Each carrier's own run outranks the neighbours' step-out because the raid
    // cannot spread away from someone who is walking into it.
    triggers.push_back(new TriggerNode(
        "xt002 searing light spread trigger",
        { NextAction("xt002 searing light spread action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "xt002 gravity bomb spread trigger",
        { NextAction("xt002 gravity bomb spread action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "xt002 boombot avoid trigger",
        { NextAction("xt002 boombot avoid action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "xt002 void zone trigger",
        { NextAction("xt002 void zone action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "xt002 gravity bomb carrier trigger",
        { NextAction("xt002 gravity bomb carrier action", ACTION_EMERGENCY + 1) }));

    triggers.push_back(new TriggerNode(
        "xt002 searing light carrier trigger",
        { NextAction("xt002 searing light carrier action", ACTION_EMERGENCY + 1) }));

    // One action owns every non-tank's target, so nothing is marked - a raid icon is group-global and
    // would overwrite whatever the player and the other bots are using. The Pummeller taunt sits above
    // it so add control keeps running through the Heart window.
    //
    // Positioning is deliberately the lowest node here. The engine ends a tick at the first action
    // that succeeds, so during an add wave the priority action starves it - which is the behaviour we
    // want, since killing a Scrapbot beats standing on a spot and the anchor has yards of tolerance.
    triggers.push_back(new TriggerNode(
        "xt002 pummeller taunt trigger",
        { NextAction("xt002 pummeller taunt action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "xt002 set dps priority trigger",
        { NextAction("xt002 set dps priority action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "xt002 redirect threat trigger",
        { NextAction("xt002 redirect threat action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "xt002 raid position trigger",
        { NextAction("xt002 raid position action", ACTION_RAID) }));

    //
    // Iron Assembly
    //
    // The engine stops at the first action that returns true, so this order is a survival ranking.
    // The Overwhelming Power carrier leads: they are already dead, and the only question left is
    // whether Meltdown takes the melee with them - every death it causes is another permanent +25%
    // on Steelbreaker. Then the three hazards, which are 20,000 nature, 5000 a second, and 5500 a
    // second respectively.
    //
    // The interrupt has to sit above the RAID band, because every class interrupt lives at
    // ACTION_INTERRUPT (40): Lightning Whirl reaches 100 yd and has no positional answer at all, so
    // nothing else in the fight can substitute for stopping the cast.
    //
    // Below that, tanking outranks damage and damage outranks standing still. Position is last on
    // purpose and yields as soon as it is parked, which is what leaves a bot free to take the kick.
    triggers.push_back(new TriggerNode(
        "iron assembly reset encounter state trigger",
        { NextAction("iron assembly reset encounter state action", ACTION_EMERGENCY + 10) }));

    triggers.push_back(new TriggerNode(
        "iron assembly overwhelming power run out trigger",
        { NextAction("iron assembly overwhelming power run out action", ACTION_EMERGENCY + 7) }));

    triggers.push_back(new TriggerNode(
        "iron assembly overload trigger",
        { NextAction("iron assembly overload action", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode(
        "iron assembly lightning tendrils trigger",
        { NextAction("iron assembly lightning tendrils action", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode(
        "iron assembly rune of death trigger",
        { NextAction("iron assembly rune of death action", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode(
        "iron assembly interrupt trigger",
        { NextAction("iron assembly interrupt action", ACTION_EMERGENCY + 4) }));

    triggers.push_back(new TriggerNode(
        "iron assembly tank assignment trigger",
        { NextAction("iron assembly tank assignment action", ACTION_RAID + 6) }));

    triggers.push_back(new TriggerNode(
        "iron assembly overwhelming power swap trigger",
        { NextAction("iron assembly overwhelming power swap action", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "iron assembly shield of runes trigger",
        { NextAction("iron assembly shield of runes action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "iron assembly fusion punch dispel trigger",
        { NextAction("iron assembly fusion punch dispel action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "iron assembly redirect threat trigger",
        { NextAction("iron assembly redirect threat action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "iron assembly rune of power trigger",
        { NextAction("iron assembly rune of power action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "iron assembly rune of power soak trigger",
        { NextAction("iron assembly rune of power soak action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "iron assembly set dps priority trigger",
        { NextAction("iron assembly set dps priority action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "iron assembly raid position trigger",
        { NextAction("iron assembly raid position action", ACTION_RAID) }));

    //
    // Kologarn
    //
    // Targets are picked per role in code, with no raid icons: Skull means "everyone DPS this" and
    // Moon is the CC channel, so borrowing them for a per-role split leaks into the generic engine.
    triggers.push_back(new TriggerNode(
        "kologarn body tank trigger",
        { NextAction("kologarn body tank action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "kologarn off tank trigger",
        { NextAction("kologarn off tank action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "kologarn dps target trigger",
        { NextAction("kologarn dps target action", ACTION_RAID) }));

    // Rubble outrun players, so the off-tank holds them clear of the raid instead of kiting.
    triggers.push_back(new TriggerNode(
        "kologarn rubble tank trigger",
        { NextAction("kologarn rubble tank action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "kologarn rubble slowdown trigger",
        { NextAction("kologarn rubble slowdown action", ACTION_RAID) }));

    // Overhead Smash stacks Crunch Armor on whoever holds the body; the pair trade it at 2 stacks.
    triggers.push_back(new TriggerNode(
        "kologarn smash swap trigger",
        { NextAction("kologarn smash swap action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "kologarn nature resistance trigger",
        { NextAction("kologarn nature resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "kologarn fall from floor trigger",
        { NextAction("kologarn fall from floor action", ACTION_RAID + 1) }));

    // An uncovered body means Petrifying Breath on the whole raid, so this outranks the dodges.
    triggers.push_back(new TriggerNode(
        "kologarn body uncovered trigger",
        { NextAction("kologarn body uncovered action", ACTION_EMERGENCY + 1) }));

    triggers.push_back(new TriggerNode(
        "kologarn eyebeam trigger",
        { NextAction("kologarn eyebeam action", ACTION_EMERGENCY) }));

    //
    // Auriaya
    //
    // Sonic Screech is deliberately soaked, not dodged: it shares its damage across everyone in the
    // cone, so there is no dodge node here and the anchors exist to put the raid in the arc. Position
    // sits at the bottom because the engine stops at the first action that succeeds - killing a sentry
    // beats standing on a spot, and the anchor tolerances make the drift cheap.
    triggers.push_back(new TriggerNode(
        "auriaya fall from floor trigger",
        { NextAction("auriaya fall from floor action", ACTION_RAID + 4) }));

    // A loose Sanctum Sentry is the worst state the fight has: it buffs Auriaya while it lives and
    // pounces anything 8-25 yd away, which holding it in melee prevents outright. It outranks the pool
    // dodge, which is one step and can wait a tick.
    triggers.push_back(new TriggerNode(
        "auriaya sentry taunt trigger",
        { NextAction("auriaya sentry taunt action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "auriaya seeping essence trigger",
        { NextAction("auriaya seeping essence action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "auriaya anti fear trigger",
        { NextAction("auriaya anti fear action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "auriaya set dps priority trigger",
        { NextAction("auriaya set dps priority action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "auriaya raid position trigger",
        { NextAction("auriaya raid position action", ACTION_RAID) }));

    //
    // Hodir
    //
    // The engine stops at the first action that succeeds, so this order is a survival ranking, not a
    // preference. Sheltering is the only node whose failure is an outright death - Flash Freeze
    // encases everyone without the Safe Area aura, and a second one instakills whoever is still
    // trapped. Dodging is next because an icicle lands every 2s for 14000. The jump sits above the
    // targeting node so a bot parked on an ice block still sheds Biting Cold. Position is last on
    // purpose: killing the block that is about to get an ally killed beats standing on a dot.
    triggers.push_back(new TriggerNode(
        "hodir near snowpacked icicle",
        { NextAction("hodir move snowpacked icicle", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "hodir icicle dodge",
        { NextAction("hodir icicle dodge action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "hodir frozen blows swap",
        { NextAction("hodir frozen blows swap action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "hodir spread storm cloud",
        { NextAction("hodir spread storm cloud", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "hodir biting cold",
        { NextAction("hodir biting cold jump", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "hodir set dps priority",
        { NextAction("hodir set dps priority action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "hodir frost resistance trigger",
        { NextAction("hodir frost resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "hodir raid position",
        { NextAction("hodir raid position action", ACTION_RAID) }));

    //
    // Freya
    //
    // Survival first, then the pacify counter, then targeting. A bot that is dead, blown up or
    // silenced contributes nothing to the trio wave it is being steered at.
    triggers.push_back(new TriggerNode(
        "freya near nature bomb",
        { NextAction("freya move away nature bomb", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "freya avoid detonating lasher",
        { NextAction("freya avoid detonating lasher", ACTION_RAID + 3) }));

    // Conservator's Grip is raid-wide and cannot be outranged, so a spore outranks attacking: a
    // pacified bot cannot swing at anything anyway.
    triggers.push_back(new TriggerNode(
        "freya move to healing spore trigger",
        { NextAction("freya move to healing spore action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "freya tank adds",
        { NextAction("freya tank adds", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "freya redirect threat",
        { NextAction("freya redirect threat", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "freya set dps priority",
        { NextAction("freya set dps priority", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "freya nature resistance trigger",
        { NextAction("freya nature resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "freya fire resistance trigger",
        { NextAction("freya fire resistance action", ACTION_RAID) }));

    // Hard mode (config-gated): break out of Iron Roots and dodge the Unstable Sun Beam. Breaking the
    // root outranks the dodge - a rooted bot can't move, so it has to free itself before it can step out.
    triggers.push_back(new TriggerNode(
        "freya break iron roots",
        { NextAction("freya break iron roots", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "freya dodge unstable sun beam",
        { NextAction("freya dodge unstable sun beam", ACTION_RAID + 4) }));

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

    // Lightning Charge is ~17k instant with only the orb lighting up as warning, and it grows 10% per
    // stack, so it outranks the two Sif nodes. The corridor smash sits below it because the two can
    // never be live at the same time, and the barrier bail below that: it costs health, not a life.
    triggers.push_back(new TriggerNode(
        "thorim lightning charge trigger",
        { NextAction("thorim lightning charge action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "thorim runic smash trigger",
        { NextAction("thorim runic smash action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "thorim runic barrier bail trigger",
        { NextAction("thorim runic barrier bail action", ACTION_RAID + 2) }));

    // Top of the Thorim ladder. An arena squad member outside the box is not a positioning problem:
    // one 5 second scan finding nobody in there summons the Lightning Orb and kills the raid.
    triggers.push_back(new TriggerNode(
        "thorim arena leash trigger",
        { NextAction("thorim arena leash action", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "thorim reset encounter state trigger",
        { NextAction("thorim reset encounter state action", ACTION_RAID) }));

    //
    // Mimiron. Laser Barrage outranks everything else here: its cone one-shots. Ranked below it by
    // what a hit actually costs - Shock Blast is 100000 damage in a 15 yd circle, a Proximity Mine
    // is 3 yd and healable, which is why the mine dodge sits under the whole rest of the ladder.
    //
    triggers.push_back(new TriggerNode(
        "mimiron p3wx2 laser barrage trigger",
        { NextAction("mimiron p3wx2 laser barrage action", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "mimiron shock blast trigger",
        { NextAction("mimiron shock blast action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "mimiron fire resistance trigger",
        { NextAction("mimiron fire resistance action", ACTION_RAID) }));

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
        { NextAction("mimiron rocket strike action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "mimiron phase 4 focus trigger",
        { NextAction("mimiron phase 4 focus action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "mimiron magnetic core trigger",
        { NextAction("mimiron magnetic core action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "mimiron plasma blast trigger",
        { NextAction("mimiron plasma blast action", ACTION_RAID + 1) }));

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
    triggers.push_back(new TriggerNode(
        "mimiron dodge flames trigger",
        { NextAction("mimiron dodge flames action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "mimiron frost bomb trigger",
        { NextAction("mimiron frost bomb action", ACTION_RAID + 4) }));

    //
    // General Vezax
    //
    // The engine stops at the first action that returns true, so this order is a survival ranking.
    // Mark of the Faceless leads because it is the only node whose failure heals the boss while it
    // drains the raid. The two hazard exits come next - a bot riding a puddle past the doubling point
    // dies to it, and a healer left in a Shadow Crash field is at a quarter of its output. Soaking
    // that same field is the reward half of the mechanic, so it sits down in the RAID band where it
    // cannot pre-empt anything that keeps a bot alive.
    //
    // The interrupt has to sit above the RAID band: every class interrupt lives at ACTION_INTERRUPT
    // (40), and Searing Flames is 13875-16125 to the whole raid plus 75% of the tank's armour every
    // 8s in 25-man. Position is last on purpose, and yields as soon as it is parked, so those class
    // interrupts still get a tick.
    triggers.push_back(new TriggerNode(
        "vezax reset encounter state",
        { NextAction("vezax reset encounter state action", ACTION_EMERGENCY + 10) }));

    triggers.push_back(new TriggerNode(
        "vezax mark of the faceless",
        { NextAction("vezax mark of the faceless action", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode(
        "vezax vapor puddle clear",
        { NextAction("vezax vapor puddle clear action", ACTION_EMERGENCY + 7) }));

    triggers.push_back(new TriggerNode(
        "vezax shadow crash clear",
        { NextAction("vezax shadow crash clear action", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode(
        "vezax searing flames interrupt",
        { NextAction("vezax searing flames interrupt action", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode(
        "vezax surge of darkness",
        { NextAction("vezax surge of darkness action", ACTION_EMERGENCY + 4) }));

    triggers.push_back(new TriggerNode(
        "vezax saronite animus",
        { NextAction("vezax saronite animus action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "vezax vapor soak",
        { NextAction("vezax vapor soak action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "vezax kill vapor",
        { NextAction("vezax kill vapor action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "vezax shadow crash soak",
        { NextAction("vezax shadow crash soak action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "vezax shadow resistance",
        { NextAction("vezax shadow resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "vezax raid position",
        { NextAction("vezax raid position action", ACTION_RAID) }));

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
        "yogg-saron anti fear trigger",
        { NextAction("yogg-saron anti fear action", ACTION_RAID + 2) }));

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

    triggers.push_back(new TriggerNode(
        "yogg-saron squeeze escape trigger",
        { NextAction("yogg-saron squeeze escape action", ACTION_RAID + 1) }));

    //
    // Algalon the Observer
    //
    // Big Bang outranks everything because missing it is 76312 or a boss reset, Cosmic Smash comes
    // next on its hard 4s fuse, and stepping out of a hole beats both once neither is happening.
    // Position is last and yields as soon as it is parked.
    triggers.push_back(new TriggerNode(
        "algalon reset encounter state",
        { NextAction("algalon reset encounter state action", ACTION_EMERGENCY + 10) }));

    triggers.push_back(new TriggerNode(
        "algalon big bang hide",
        { NextAction("algalon big bang hide action", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode(
        "algalon big bang soak",
        { NextAction("algalon big bang soak action", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode(
        "algalon cosmic smash",
        { NextAction("algalon cosmic smash action", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode(
        "algalon leave black hole",
        { NextAction("algalon leave black hole action", ACTION_EMERGENCY + 4) }));

    triggers.push_back(new TriggerNode(
        "algalon phase punch swap",
        { NextAction("algalon phase punch swap action", ACTION_RAID + 7) }));

    triggers.push_back(new TriggerNode(
        "algalon constellation taunt",
        { NextAction("algalon constellation taunt action", ACTION_RAID + 6) }));

    triggers.push_back(new TriggerNode(
        "algalon dark matter tank",
        { NextAction("algalon dark matter tank action", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "algalon constellation kite",
        { NextAction("algalon constellation kite action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "algalon collapsing star focus",
        { NextAction("algalon collapsing star focus action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "algalon dark matter mark",
        { NextAction("algalon dark matter mark action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "algalon raid position",
        { NextAction("algalon raid position action", ACTION_RAID) }));
}

void RaidUlduarStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    // Algalon: hold the soaker's escape cooldown for Big Bang, keep the raid off the Living
    // Constellations and off area attacks while the stars are being killed one at a time, and keep
    // the generic movers away from the formation
    multipliers.push_back(new AlgalonSoakCooldownReserveMultiplier(botAI));
    multipliers.push_back(new AlgalonCollapsingStarAoeMultiplier(botAI));
    multipliers.push_back(new AlgalonTargetGuardMultiplier(botAI));
    multipliers.push_back(new AlgalonControlMovementMultiplier(botAI));

    // XT-002: hold the burst cooldowns for the mode's real damage window, and in normal mode stop
    // damage on the exposed Heart before it dies and flips the raid into hard mode
    multipliers.push_back(new XT002BurstWindowMultiplier(botAI));
    multipliers.push_back(new XT002TargetGuardMultiplier(botAI));

    // Mimiron picks every non-tank target in code, so the generic picker has to be shut out
    multipliers.push_back(new MimironTargetGuardMultiplier(botAI));
    multipliers.push_back(new MimironChargeGuardMultiplier(botAI));
    multipliers.push_back(new MimironThreatRedirectGuardMultiplier(botAI));

    // The Iron Assembly owns every target in the fight, and its hazard dodges must not be undone by
    // a generic mover walking the bot back into the blast
    multipliers.push_back(new IronAssemblyDisableAutomaticTargetingMultiplier(botAI));
    multipliers.push_back(new IronAssemblyMovementGuardMultiplier(botAI));

    // Thorim keeps a bailing melee out of the Runic Barrier damage shield, and stops the generic
    // movers collapsing the three phase 2 melee stacks back into one Chain Lightning arc
    multipliers.push_back(new ThorimRunicBarrierMultiplier(botAI));
    multipliers.push_back(new ThorimMovementGuardMultiplier(botAI));
    multipliers.push_back(new ThorimArenaLeashMultiplier(botAI));
    multipliers.push_back(new ThorimArenaTargetGuardMultiplier(botAI));
    multipliers.push_back(new ThorimArenaAnchorGuardMultiplier(botAI));

    // Hold the class-generic threat redirects on the bosses where the main tank is the wrong sink
    multipliers.push_back(new UldThreatRedirectMultiplier(botAI));

    // Hold the burst cooldowns on the bosses whose DPS check is not the pull
    multipliers.push_back(new UlduarBurstWindowMultiplier(botAI));

    // Keep the generic movers off a bot that is clearing a Razorscale Devouring Flame patch
    multipliers.push_back(new RazorscaleMultiplier(botAI));

    // Let the Ignis construct tank stand in the fire, and stop a Slag Pot victim fighting the ride
    multipliers.push_back(new IgnisMultiplier(botAI));

    // Kologarn picks every target per role in code, so the generic pickers have to be shut out, and
    // a Stone Grip victim is a passenger who cannot walk
    multipliers.push_back(new KologarnDisableAutomaticTargetingMultiplier(botAI));
    multipliers.push_back(new KologarnMultiplier(botAI));

    // Freya splits the DPS across the trio wave in code, so the generic pickers stand down, and the
    // floor stops a stray hit killing one member well ahead of the other two
    multipliers.push_back(new FreyaDisableAutomaticTargetingMultiplier(botAI));
    multipliers.push_back(new FreyaTrioSyncMultiplier(botAI));

    // Flame Leviathan is fought entirely from vehicles: let its drive action own the MotionMaster
    multipliers.push_back(new FlameLeviathanVehicleMovementMultiplier(botAI));

    // Vezax owns where the ranged half stands, so the generic movers have to stand down or the
    // formation is re-derived and abandoned on alternate ticks.
    multipliers.push_back(new VezaxControlMovementMultiplier(botAI));

    multipliers.push_back(new AuriayaMovementGuardMultiplier(botAI));
    multipliers.push_back(new HodirGuardMultiplier(botAI));

    // Keep Tremor Totem in the earth slot for as long as these two can fear
    multipliers.push_back(new AuriayaAntiFearTotemGuardMultiplier(botAI));
    multipliers.push_back(new YoggSaronAntiFearTotemGuardMultiplier(botAI));
}
