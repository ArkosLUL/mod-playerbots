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

    // Where the main tank stands is where every fire patch lands, so his spot outranks everything
    // else the raid does. The construct tanks share the band because no bot is ever both.
    //
    // An Iron Construct only dies to the Molten -> Brittle -> Shatter chain, and every one left alive
    // is another stack of Strength of the Creator on the boss, so the kite outranks the raid's damage.
    // Standing next to a Molten construct and sitting in a Slag Pot both kill a bot outright.
    triggers.push_back(new TriggerNode(
        "ignis main tank position trigger",
        { NextAction("ignis main tank position action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "ignis construct tank trigger",
        { NextAction("ignis construct tank action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "ignis attack brittle construct trigger",
        { NextAction("ignis attack brittle construct action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "ignis attack boss trigger",
        { NextAction("ignis attack boss action", ACTION_RAID + 0.5f) }));

    triggers.push_back(new TriggerNode(
        "ignis molten construct avoid trigger",
        { NextAction("ignis molten construct avoid action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "ignis slag pot heal trigger",
        { NextAction("ignis slag pot heal action", ACTION_EMERGENCY + 1) }));

    triggers.push_back(new TriggerNode(
        "ignis flame jets trigger",
        { NextAction("ignis flame jets hold cast action", ACTION_EMERGENCY + 2) }));

    //
    // XT-002 Deconstructor
    //
    // Stepping out of a Boombot blast or a Void Zone outranks everything else: both one-shot a bot that
    // stands in them. The debuff carrier sits above that because a carrier walking through the raid is
    // a mechanic nobody else can answer - the raid holds still and the carrier leaves on its own.
    //
    // Both are single nodes covering two mechanics apiece, and that is load-bearing. Two nodes on equal
    // relevance cannot share a bot: the engine ends the tick at the first action returning true and
    // leaves the loser queued, so the pair trade the tick and the bot walks the line between their
    // destinations. Nothing below shares a relevance with anything else here either.
    triggers.push_back(new TriggerNode(
        "xt002 avoid hazard trigger",
        { NextAction("xt002 avoid hazard action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "xt002 debuff carrier trigger",
        { NextAction("xt002 debuff carrier action", ACTION_EMERGENCY + 1) }));

    // One action owns every bot's target, tanks included, so nothing is marked - a raid icon is
    // group-global and would overwrite whatever the player and the other bots are using. The Pummeller
    // taunt sits above it so add control keeps running through the Heart window.
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
    // The three hazards lead, being 20,000 nature, 5000 a second, and 5500 a second respectively.
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
        "iron assembly shield of runes trigger",
        { NextAction("iron assembly shield of runes action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "iron assembly fusion punch dispel trigger",
        { NextAction("iron assembly fusion punch dispel action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "iron assembly redirect threat trigger",
        { NextAction("iron assembly redirect threat action", ACTION_RAID + 2) }));

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
    // trapped. Dodging is next because an icicle lands every 2s for 14000. Targeting sits above the
    // Biting Cold shed, which moves for seconds at a time and would otherwise starve it; that costs
    // nothing, because the targeting action returns false whenever the current target is already
    // right. Position is last on purpose: killing the block that is about to get an ally killed beats
    // standing on a dot.
    triggers.push_back(new TriggerNode(
        "hodir near snowpacked icicle",
        { NextAction("hodir move snowpacked icicle", ACTION_RAID + 6) }));

    triggers.push_back(new TriggerNode(
        "hodir icicle dodge",
        { NextAction("hodir icicle dodge action", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "hodir frozen blows swap",
        { NextAction("hodir frozen blows swap action", ACTION_RAID + 4) }));

    // Shares the swap's relevance because the two can never contend: the swap only fires for the two
    // tanks and the redirect only for hunters and rogues. It has to outrank targeting, or a bot
    // switches to a block before spending charges the tank is waiting on.
    triggers.push_back(new TriggerNode(
        "hodir redirect threat",
        { NextAction("hodir redirect threat action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "hodir set dps priority",
        { NextAction("hodir set dps priority action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "hodir spread storm cloud",
        { NextAction("hodir spread storm cloud", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "hodir biting cold",
        { NextAction("hodir biting cold shed", ACTION_RAID + 1) }));

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

    // The main tank answers a bomb by moving Freya, not itself. Bombs land at players' feet and the
    // melee stack is on the boss, so a volley buries her melee ring and the rest of the melee lose it.
    triggers.push_back(new TriggerNode(
        "freya tank nature bomb",
        { NextAction("freya tank nature bomb", ACTION_RAID + 4) }));

    // The two 8 yd circles, and the whole reason the raid has to be able to come apart: Nature's Fury
    // fires five of them around one bot, Sunbeam drops one wherever its target is standing when the cast
    // ends. Both sit with the other escapes, above the spore node - which would otherwise walk the
    // carrier straight back into the ball it just left.
    triggers.push_back(new TriggerNode(
        "freya nature fury bail",
        { NextAction("freya nature fury bail", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "freya step out of sunbeam",
        { NextAction("freya step out of sunbeam", ACTION_RAID + 4) }));

    // Detonating Lasher wave. Order is the doctrine: nothing here can be tanked, kited or outrun, so
    // the raid answers the wave with crowd control and a camp it can AoE. Leave the blast of anything
    // about to go off first - a bot that is dead does no crowd control - then root what has already
    // closed, then snare what has not, then the ghouls, which are the one thing on the encounter that
    // takes a lasher off a bot at all - and gather last, only when nothing urgent is asking.
    triggers.push_back(new TriggerNode(
        "freya lasher about to blow",
        { NextAction("freya lasher about to blow", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "freya frost nova lashers",
        { NextAction("freya frost nova lashers", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "freya trap lashers",
        { NextAction("freya trap lashers", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "freya summon army",
        { NextAction("freya summon army", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "freya ranged camp",
        { NextAction("freya ranged camp", ACTION_RAID) }));

    // Conservator's Grip is raid-wide and cannot be outranged, so a spore outranks attacking: a
    // pacified bot cannot swing at anything anyway.
    triggers.push_back(new TriggerNode(
        "freya move to healing spore trigger",
        { NextAction("freya move to healing spore action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "freya tank adds",
        { NextAction("freya tank adds", ACTION_RAID + 1) }));

    // Freya walks after whoever holds her, so every escape the tank takes moves her and nothing used to
    // move her back. Under the bomb escape, which has to be able to leave the leash.
    triggers.push_back(new TriggerNode(
        "freya tank hold freya",
        { NextAction("freya tank hold freya", ACTION_RAID + 1) }));

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

    triggers.push_back(new TriggerNode(
        "freya ground tremor hold cast",
        { NextAction("freya ground tremor hold cast", ACTION_EMERGENCY + 2) }));

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
        "thorim tank pickup trigger",
        { NextAction("thorim tank pickup action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "thorim dps priority trigger",
        { NextAction("thorim dps priority action", ACTION_RAID) }));

    // Recalls a pet, not the bot, so it competes with nothing and sits at the base rank.
    triggers.push_back(new TriggerNode(
        "thorim pet leash trigger",
        { NextAction("thorim pet leash action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "thorim gauntlet positioning trigger",
        { NextAction("thorim gauntlet positioning action", ACTION_RAID) }));

    // Above the corridor walk, because on the balcony that one has no waypoint to offer and the
    // hallway is where the two Paralytic Field bunnies are.
    triggers.push_back(new TriggerNode(
        "thorim balcony advance trigger",
        { NextAction("thorim balcony advance action", ACTION_RAID + 2) }));

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

    // Charge Orb shares that rank, and can: it only ever fires while Thorim is still on the balcony,
    // and Lightning Charge only after he has come down. 3k a second for 15s across a 32 yd circle is
    // phase 1's largest avoidable damage source, so it has to beat the ring and the add chase both.
    triggers.push_back(new TriggerNode(
        "thorim charged orb trigger",
        { NextAction("thorim charged orb action", ACTION_RAID + 4) }));

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
    // Rapid Burst tops the ladder, and it costs nothing to put it there: it is scheduled only when
    // phase 2 starts, so it can never contend with the barrage below, and the two lethal nodes it
    // outranks inside phase 2 - the Frost Bomb on a 10 s fuse and the Rocket Strike on a 5 s one -
    // both have the seconds to spare that a 3 s cone does not.
    triggers.push_back(new TriggerNode(
        "mimiron rapid burst trigger",
        { NextAction("mimiron rapid burst action", ACTION_RAID + 8) }));

    triggers.push_back(new TriggerNode(
        "mimiron p3wx2 laser barrage trigger",
        { NextAction("mimiron p3wx2 laser barrage action", ACTION_RAID + 7) }));

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
        { NextAction("mimiron rocket strike action", ACTION_RAID + 5) }));

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
    //
    // The bomb outranks the fire, and the gap between them is the point. Both used to sit on
    // ACTION_RAID + 4 alongside the rocket strike, the queue breaks an exact tie by push order, and
    // the engine stops the tick at the first action that returns true - so the fire step, which wins
    // that tie, ended the tick 143 to 221 times a pull and the bomb node was reached 6 to 13. Fire is
    // 3000 a second and healable; the explosion is 47000 in 30 yd against a 24000 health pool.
    triggers.push_back(new TriggerNode(
        "mimiron dodge flames trigger",
        { NextAction("mimiron dodge flames action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "mimiron frost bomb trigger",
        { NextAction("mimiron frost bomb action", ACTION_RAID + 6) }));

    triggers.push_back(new TriggerNode(
        "mimiron reset encounter state trigger",
        { NextAction("mimiron reset encounter state action", ACTION_RAID) }));

    //
    // General Vezax
    //
    // The engine stops at the first action that returns true, so this order is a survival ranking,
    // and no two nodes share a number - a tie falls to vector insertion order, which is not a
    // decision anyone made.
    //
    // The dodge leads: Shadow Crash is 11310 plus a knockback, and it is the only hazard here with a
    // deadline, about 1.8-2.6s of missile flight. The interrupt comes next and outranks the puddle
    // exit, because losing a kick costs the whole raid 13875-16125 fire while riding one more puddle
    // tick costs one bot a survivable hit - the interrupt trigger yields on its own when the next
    // tick is not survivable. Mark of the Faceless is the only node whose failure heals the boss, but
    // it drains over 10s rather than landing at once, so it sits under both.
    //
    // The RAID band is the reward half. Soaking a field or a puddle is worth nothing to a bot that is
    // already dying, and kill-vapor outranks the field soak because the handler is by definition out
    // of mana and a cost reduction buys it nothing. Position is last on purpose, and yields as soon as
    // it is parked, so the class interrupts at ACTION_INTERRUPT (40) still get a tick.
    triggers.push_back(new TriggerNode(
        "vezax reset encounter state",
        { NextAction("vezax reset encounter state action", ACTION_EMERGENCY + 10) }));

    triggers.push_back(new TriggerNode(
        "vezax shadow crash dodge",
        { NextAction("vezax shadow crash dodge action", ACTION_EMERGENCY + 9) }));

    triggers.push_back(new TriggerNode(
        "vezax searing flames interrupt",
        { NextAction("vezax searing flames interrupt action", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode(
        "vezax vapor puddle clear",
        { NextAction("vezax vapor puddle clear action", ACTION_EMERGENCY + 7) }));

    triggers.push_back(new TriggerNode(
        "vezax mark of the faceless",
        { NextAction("vezax mark of the faceless action", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode(
        "vezax surge of darkness",
        { NextAction("vezax surge of darkness action", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode(
        "vezax saronite animus",
        { NextAction("vezax saronite animus action", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "vezax vapor soak",
        { NextAction("vezax vapor soak action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "vezax kill vapor",
        { NextAction("vezax kill vapor action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "vezax shadow crash soak",
        { NextAction("vezax shadow crash soak action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "vezax shadow resistance",
        { NextAction("vezax shadow resistance action", ACTION_RAID + 1) }));

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
    multipliers.push_back(new MimironAvoidAoeGuardMultiplier(botAI));
    multipliers.push_back(new MimironFormationGuardMultiplier(botAI));
    multipliers.push_back(new MimironThreatRedirectGuardMultiplier(botAI));

    // The Iron Assembly owns every target in the fight, its hazard dodges must not be undone by a
    // generic mover walking the bot back into the blast or by a gap-closer teleporting it there, and
    // in hard mode the burst is saved for the member that reaches phase 3
    multipliers.push_back(new IronAssemblyDisableAutomaticTargetingMultiplier(botAI));
    multipliers.push_back(new IronAssemblyMovementGuardMultiplier(botAI));
    multipliers.push_back(new IronAssemblyChargeGuardMultiplier(botAI));
    multipliers.push_back(new IronAssemblyHoldDpsCooldownsMultiplier(botAI));

    // Thorim keeps a bailing melee out of the Runic Barrier damage shield, picks every non-tank
    // target in code so the generic pickers have to be shut out, and stops the generic movers
    // collapsing the three phase 2 melee stacks back into one Chain Lightning arc
    multipliers.push_back(new ThorimRunicBarrierMultiplier(botAI));
    multipliers.push_back(new ThorimDisableAutomaticTargetingMultiplier(botAI));
    multipliers.push_back(new ThorimMovementGuardMultiplier(botAI));
    multipliers.push_back(new ThorimArenaLeashMultiplier(botAI));
    multipliers.push_back(new ThorimArenaTargetGuardMultiplier(botAI));
    multipliers.push_back(new ThorimArenaAnchorGuardMultiplier(botAI));
    multipliers.push_back(new ThorimBalconyGuardMultiplier(botAI));

    // Hold the class-generic threat redirects on the bosses where the main tank is the wrong sink
    multipliers.push_back(new UldThreatRedirectMultiplier(botAI));

    // Hold the burst cooldowns on the bosses whose DPS check is not the pull
    multipliers.push_back(new UlduarBurstWindowMultiplier(botAI));

    // Keep the generic movers off a bot that is clearing a Razorscale Devouring Flame patch
    multipliers.push_back(new RazorscaleMultiplier(botAI));

    // Let the Ignis construct tank stand in the fire, and stop a Slag Pot victim fighting the ride
    multipliers.push_back(new IgnisMultiplier(botAI));
    multipliers.push_back(new IgnisTankMovementMultiplier(botAI));
    multipliers.push_back(new IgnisDisableDefaultTargetingMultiplier(botAI));
    multipliers.push_back(new IgnisFlameJetsHoldCastMultiplier(botAI));

    // Kologarn picks every target per role in code, so the generic pickers have to be shut out, and
    // a Stone Grip victim is a passenger who cannot walk
    multipliers.push_back(new KologarnDisableAutomaticTargetingMultiplier(botAI));
    multipliers.push_back(new KologarnMultiplier(botAI));

    // Freya splits the DPS across the trio wave in code, so the generic pickers stand down, and the
    // floor stops a stray hit killing one member well ahead of the other two
    multipliers.push_back(new FreyaDisableAutomaticTargetingMultiplier(botAI));
    multipliers.push_back(new FreyaTrioSyncMultiplier(botAI));
    multipliers.push_back(new FreyaLasherFinishAoeMultiplier(botAI));
    multipliers.push_back(new FreyaLasherTrapReserveMultiplier(botAI));
    multipliers.push_back(new FreyaGroundTremorCastGateMultiplier(botAI));
    multipliers.push_back(new FreyaAvoidAoeHoldMultiplier(botAI));

    // Flame Leviathan is fought entirely from vehicles: let its drive action own the MotionMaster
    multipliers.push_back(new FlameLeviathanVehicleMovementMultiplier(botAI));

    // Vezax owns where the ranged half stands, so the generic movers have to stand down or the
    // formation is re-derived and abandoned on alternate ticks.
    multipliers.push_back(new VezaxControlMovementMultiplier(botAI));

    multipliers.push_back(new AuriayaMovementGuardMultiplier(botAI));
    multipliers.push_back(new HodirGuardMultiplier(botAI));
    multipliers.push_back(new HodirPaladinAuraMultiplier(botAI));

    // Keep Tremor Totem in the earth slot for as long as these two can fear
    multipliers.push_back(new AuriayaAntiFearTotemGuardMultiplier(botAI));
    multipliers.push_back(new YoggSaronAntiFearTotemGuardMultiplier(botAI));
}
