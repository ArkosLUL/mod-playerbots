/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldStrategy.h"

#include "BossResistanceMultipliers.h"
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
    UldRazorscaleDefinition().AddTriggerNodes(triggers);

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
    UldXT002Definition().AddTriggerNodes(triggers);

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
    UldKologarnDefinition().AddTriggerNodes(triggers);

    //
    // Auriaya
    //
    UldAuriayaDefinition().AddTriggerNodes(triggers);

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
        "hodir collect storm power",
        { NextAction("hodir collect storm power", ACTION_RAID + 2) }));

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
    UldFreyaDefinition().AddTriggerNodes(triggers);

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

    // Charge Orb: it only ever fires while Thorim is still on the balcony. 3k a second for 15s across a
    // 32 yd circle is phase 1's largest avoidable damage source, so it has to beat the ring and the add
    // chase both. Lightning Charge has no node of its own - the cone answer is baked into the phase 2
    // spot, and a second mover for the same point only fought the first one at the movement gate.
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
    triggers.push_back(new TriggerNode(
        "sara shadow resistance trigger",
        { NextAction("sara shadow resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron shadow resistance trigger",
        { NextAction("yogg-saron shadow resistance action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron guardian positioning trigger",
        { NextAction("yogg-saron guardian positioning action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron phase 1 spacing trigger",
        { NextAction("yogg-saron phase 1 spacing action", ACTION_RAID + 2) }));

    // Under the spacing node, so a nova still wins the tick. It never contends with guardian
    // positioning at the same relevance because that one only fires on melee and tanks.
    triggers.push_back(new TriggerNode(
        "yogg-saron phase 1 station trigger",
        { NextAction("yogg-saron phase 1 station action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron dark volley trigger",
        { NextAction("yogg-saron dark volley interrupt action", ACTION_EMERGENCY + 1) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron sanity trigger",
        { NextAction("yogg-saron sanity action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron malady of the mind trigger",
        { NextAction("yogg-saron malady of the mind action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron anti fear trigger",
        { NextAction("yogg-saron anti fear action", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron phase 3 control trigger",
        { NextAction("yogg-saron phase 3 control action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron set dps priority trigger",
        { NextAction("yogg-saron set dps priority action", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron phase 2 spacing trigger",
        { NextAction("yogg-saron phase 2 spacing action", ACTION_RAID + 3) }));

    // Over the dps resolver and the Sanity Well walk, under the hazard dodges and the fear counter: a
    // link costs 2 Sanity and a shared hit a second, a Death Ray costs the bot.
    triggers.push_back(new TriggerNode(
        "yogg-saron brain link trigger",
        { NextAction("yogg-saron brain link action", ACTION_RAID + 1.5f) }));

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
        "yogg-saron stop following trigger",
        { NextAction("yogg-saron stop following action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron illusion room trigger",
        { NextAction("yogg-saron illusion room action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron move to exit portal trigger",
        { NextAction("yogg-saron move to exit portal action", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron lunatic gaze trigger",
        { NextAction("yogg-saron lunatic gaze action", ACTION_EMERGENCY) }));

    // Same relevance as the gaze node above and they cannot share a bot, which costs nothing: Yogg's
    // own Lunatic Gaze is a phase 3 self aura on the platform and the skulls only exist below it.
    triggers.push_back(new TriggerNode(
        "yogg-saron laughing skull trigger",
        { NextAction("yogg-saron laughing skull action", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron phase 3 positioning trigger",
        { NextAction("yogg-saron phase 3 positioning action", ACTION_RAID) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron guardian control trigger",
        { NextAction("yogg-saron guardian control action", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron sanity conservation trigger",
        { NextAction("yogg-saron sanity conservation action", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode(
        "yogg-saron squeeze escape trigger",
        { NextAction("yogg-saron squeeze escape action", ACTION_RAID + 1) }));

    // Above the sanity conservation retreat: cutting somebody out of 7.5k a second beats walking
    // somebody else to a well.
    triggers.push_back(new TriggerNode(
        "yogg-saron squeeze rescue trigger",
        { NextAction("yogg-saron squeeze rescue action", ACTION_RAID + 6) }));

    // Level with the phase 2 dodge and above the dps resolver: where the bot stands has to be settled
    // before the reach node is asked to close on the target from there.
    triggers.push_back(new TriggerNode(
        "yogg-saron illusion facing trigger",
        { NextAction("yogg-saron illusion facing action", ACTION_RAID + 3) }));

    // Above the dps resolver for the same reason, though it never claims the tick: the pet has to be
    // pulled off before it swings again, not after the bot has picked its own target.
    triggers.push_back(new TriggerNode(
        "yogg-saron pet guard trigger",
        { NextAction("yogg-saron pet guard action", ACTION_RAID + 2) }));

    // Under every raid node and over reach melee, reach spell and charge: it is the last thing asked
    // before the plain walk it exists to replace, and it stands down the moment the route is clear.
    triggers.push_back(new TriggerNode(
        "yogg-saron body detour trigger",
        { NextAction("yogg-saron body detour action", ACTION_RAID - 1) }));

    // An instant cast, so under every raid walk and dodge, and well over the paladin's own rotation,
    // which would only ever judge its current target.
    triggers.push_back(new TriggerNode(
        "yogg-saron diminish power judgement trigger",
        { NextAction("yogg-saron diminish power judgement action", ACTION_RAID - 0.5f) }));

    // Level with the room walk it replaces for the healer. Heal reach still outranks it, so a mate out of
    // range from the middle gets a step toward it first.
    triggers.push_back(new TriggerNode(
        "yogg-saron illusion healer station trigger",
        { NextAction("yogg-saron illusion healer station action", ACTION_RAID) }));

    // Over reach melee and set behind, so a bot arriving in front of the Brain goes round to the spot
    // rather than being walked out of range and back. Under the exit walk.
    triggers.push_back(new TriggerNode(
        "yogg-saron brain spot trigger",
        { NextAction("yogg-saron brain spot action", ACTION_RAID) }));

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

    // The Iron Assembly owns every target in the fight, its hazard dodges must not be undone by a
    // generic mover walking the bot back into the blast or by a gap-closer teleporting it there, and
    // in hard mode the burst is saved for the member that reaches phase 3
    multipliers.push_back(new IronAssemblyDisableAutomaticTargetingMultiplier(botAI));
    multipliers.push_back(new IronAssemblyMovementGuardMultiplier(botAI));
    multipliers.push_back(new IronAssemblyChargeGuardMultiplier(botAI));
    multipliers.push_back(new IronAssemblyDisableTankFaceMultiplier(botAI));
    multipliers.push_back(new IronAssemblyHoldDpsCooldownsMultiplier(botAI));
    multipliers.push_back(new IronAssemblyTauntGuardMultiplier(botAI));

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
    multipliers.push_back(new ThorimTauntGuardMultiplier(botAI));

    // Hold the class-generic threat redirects on the bosses where the main tank is the wrong sink
    multipliers.push_back(new UldThreatRedirectMultiplier(botAI));

    // Hold the burst cooldowns on the bosses whose DPS check is not the pull
    multipliers.push_back(new UlduarBurstWindowMultiplier(botAI));

    UldRazorscaleDefinition().AddMultipliers(botAI, multipliers);

    // Let the Ignis construct tank stand in the fire, and stop a Slag Pot victim fighting the ride
    multipliers.push_back(new IgnisMultiplier(botAI));
    multipliers.push_back(new IgnisTankMovementMultiplier(botAI));
    multipliers.push_back(new IgnisDisableDefaultTargetingMultiplier(botAI));
    multipliers.push_back(new IgnisFlameJetsHoldCastMultiplier(botAI));

    UldKologarnDefinition().AddMultipliers(botAI, multipliers);

    UldFreyaDefinition().AddMultipliers(botAI, multipliers);

    // Flame Leviathan is fought entirely from vehicles: let its drive action own the MotionMaster
    multipliers.push_back(new FlameLeviathanVehicleMovementMultiplier(botAI));

    UldVezaxDefinition().AddMultipliers(botAI, multipliers);

    UldAuriayaDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new HodirGuardMultiplier(botAI));
    multipliers.push_back(new HodirPaladinAuraMultiplier(botAI));

    // Hold the one designated hunter in Aspect of the Wild on the nature bosses, so every other
    // hunter keeps Dragonhawk and can still drop to Aspect of the Viper for mana.
    multipliers.push_back(new BossNatureAspectHoldMultiplier(botAI, "thorim"));

    multipliers.push_back(new YoggSaronDpsTargetGuardMultiplier(botAI));
    multipliers.push_back(new YoggSaronDisplacementGuardMultiplier(botAI));
    multipliers.push_back(new YoggSaronMovementGuardMultiplier(botAI));
    multipliers.push_back(new YoggSaronPhase1AoeHoldMultiplier(botAI));
    multipliers.push_back(new YoggSaronStackFoodGuardMultiplier(botAI));
    multipliers.push_back(new YoggSaronPhase1WalkGuardMultiplier(botAI));

    // Keep Tremor Totem in the earth slot for as long as Yogg-Saron can fear
    multipliers.push_back(new YoggSaronAntiFearTotemGuardMultiplier(botAI));
}
