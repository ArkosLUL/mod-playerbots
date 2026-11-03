#!/usr/bin/env python3
"""Score an Anub'arak pull from a RaidObs trace: phases, the landing, the spike, spheres, burrowers.

    anub_arak.py <file>              every section
    anub_arak.py <file> --phases     anub.phase spans, his health at each edge, deaths per phase
    anub_arak.py <file> --pull       who was locked out or never landed, stage 9 to the pull, and at the
                                     engage the player nearest his wait point and each bot tank's yards from
                                     it, one inside his 23 yd aggro flagged
    anub_arak.py <file> --spike      every Pursued by Anub'arak window: kiter, kite branch, outcome, protection,
                                     time in each kite stage, the kiter's boosts, its own immunities and
                                     whether anub.protect asked for them, a kiter marked inside a patch's slow,
                                     the tank patch it spent, one spent in the run stage flagged, the spike's
                                     closest approach to each latched tank patch; at the mark's end the spike's
                                     distance from the gather centre and any unlatched patch alive, a death or
                                     own immunity within 20 yd of the gather flagged; the spike's speed and the
                                     kiter's at each Hand of Protection and own immunity, one on a spike no
                                     faster flagged, and the seconds the kiter stood still while marked; a walk
                                     flip-flopping between two move targets; the nearest Hand of Protection-ready
                                     paladin as the spike first came within 12 yd; a Feign Death dropped at once;
                                     per submerge the living players within 8, 12 and 20 yd of where he went
                                     down, by role, his distance from the gather and the first kiter's distance
                                     from the spike at its mark;
                                     each kite's anub.kiteplan terminals and whether the last one was used
                                     before the mark ended, and a kiter dead with it unused; its anub.kitepred
                                     at the mark against the real end, its anub.kitepath picks with the seconds
                                     each held, one turning over 90 degrees within 1 s of the pick before
                                     flagged, and the first hazard anub.kitepred foresaw against the first real
                                     one, as the kite planner counts them; each patch kite's
                                     turn for its patch, its anub.kitemove then, the patch it went for with the
                                     kiter's and the spike's closest approach after the turn, and whether the
                                     spike failed on it; for patch+hand whether the Hand landed at the turn, read
                                     off anub.bridge, else off the Hand of Protection casts, each Hand against the
                                     catch predicted as it went out with the spike's distance and speed then and
                                     whether the patch followed under it, and at an anub.bridge turn the nearest
                                     Hand-ready paladin; the first snapshot before the turn with the spike within
                                     14.5 yd of a patch it was not already inside at the mark; Impale on a side
                                     tank flagged; per submerge the seconds into phase 2 its first tank patch went;
                                     per kite the escort's distance at the claim, at the kiter's boost before any
                                     Hand and at the end, its seconds in Permafrost and its walk and dodge swaps
                                     within 1 s, a kiter dead to anything but the spike with its own immunity
                                     unused, claim to cast for each Hand, the
                                     kiter within 15.5 yd of a tank patch under a Hand, an own immunity after a
                                     Hand ran out, a boost in a Hand's first 8 s, a boost the kiter cast after its
                                     Hand, a patch kite turning within 3 s of a boost, and for an escort that noted
                                     anub.escort patch where it stood with its yards to the kiter and the nearest
                                     kite patch as the Hand went out, else at the kite's end, anub.second grants; the
                                     Hand's cooldown off anub.hands, else 300 s; per submerge the seconds kited and
                                     idle, the terminals, the seconds after the first tank patch, kiters dead;
                                     heroic, each kite tool (Sprint, Nitro, Divine Shield, Ice Block, Vanish,
                                     Feign Death) a bot spent off its own mark before phase 3 and still on its
                                     cooldown as a submerge's spike came up, per phase; each Demonic Circle
                                     teleport with the spike off the kiter and the circle, the boost before it
                                     and its pass by the circle, and the seconds stood on landing; per submerge
                                     the bystanders' gather and dodge swaps within 1.5 s, total and top 5 bots,
                                     and each bot's dodge walks; each Sprint, Dash and Nitro Boosts with the
                                     spike's speed then and the kiter's run before it, one on a spike no faster
                                     flagged, and a kite's first against the +boost@ second of its anub.kitepath
                                     pick in force; the anub.escort standby paladin's yards to the kiter at the
                                     claim; after an own immunity or a patch end, the spike's next leg to the
                                     next kiter or its spot at the mark, and the first patch it passed within
                                     11.5 yd, spent or not; per kite each stretch of 2 s or more its gap to a
                                     spike no faster than itself shrank, ending within 12 yd, with the gap and
                                     speeds over it, summed per pull; per submerge the kite patches
                                     anub.kitespent counted, over 1 before the third flagged, and each Hand of
                                     Protection cast with its caster's cooldown off anub.hands
    anub_arak.py <file> --spheres    spheres over time, patch supply at each mark, sphere duty and its shots,
                                     each dead sphere's killer and landing against its side's spot, Death Grips
                                     against the DK's spot at the cast, whether the grip duty held to kill and
                                     a gripped sphere back up, a grip that triggered no pull, per far grip each
                                     link with the sphere's distance from its side's spot at the cast and where
                                     it came down, Death Grips with no pull per caster, anub.relay rows,
                                     each side's first tank patch, each anub.spot0 and anub.spot1 change; per
                                     submerge the spheres spent and the tank patches gone against the kite
                                     allowance (heroic 0 with no anub.allow), each with its kiter, and when that
                                     side's patch came back; heroic, per submerge each kite sphere downed while
                                     he was down, its spend and the kite it ended, under 15 s into it flagged, and
                                     the patches no side held on the floor as he went down, under 2 flagged
    anub_arak.py <file> --burrowers  each burrower: life, submerges, Permafrost, Shadow Strike, its exit, side
                                     and pickup, DPS focus per side, DPS casts at a burrower no focus held;
                                     whether it died inside its first 30 s up; per wave those deaths, per side
                                     the seconds from its burrowers coming up to that side's first focus pick
                                     and the side tank's peak Expose Weakness, in phase 3 its anub.p3wave rows
                                     and whether the side tanks lived until it died; on
                                     older traces when the focus came due and each time it stopped with the
                                     focus alive; each focus pick against its tank and the patches, labelled
                                     by side, each burrower's distance from the patch centre while
                                     on a side tank; each submerge where it went under: the nearest patch, the
                                     patches alive, its side tank alive and how far, its target; each one's
                                     opener and the three players nearest its exit at the spawn; each one pulled
                                     off its side tank by a non-tank within 15 s of coming up: who, the seconds
                                     since it came up and on the side tank, the puller's casts on it in the 3 s
                                     before, triggered ones marked *, and the seconds until it was back on a tank
                                     with the taunts at it, or those it stayed on non-tanks and never came back;
                                     each Bladestorm with the nearest burrower,
                                     within 25 yd flagged, the burrower guard's Bladestorm vetoes and the burrower
                                     threat cap's per bot; anub.dps capped rows per ranged bot; per melee bot the
                                     seconds it targeted a burrower no tank within 5 yd held, by its anub.dps
                                     value then
    anub_arak.py <file> --frost      seconds carrying Permafrost per group and bot, he up and he down, and every
                                     stint over 3 s with the bot's target at its start
    anub_arak.py <file> --threat     per surfaced window: his target, his drag legs, taunts, threat cap, the
                                     main tank's time in Permafrost, his submerge spot against the tank
                                     patches, and the longest stretch he stood 10+ yd off the midpoint of the
                                     side spots with no drag leg; the drag's pre-steps, each walk and what came
                                     after it; anub.subspot rows and how far he went down off the last spot;
                                     when the drag to the submerge spot left
    anub_arak.py <file> --dps        per phase and DPS role, bots only: samples by target kind, moving, yards
                                     walked by move order and damage by target kind; his health lost per
                                     phase 1, under 20% flagged; per phase 1 its first anub.push with his health
                                     and the seconds left then, and whether phase 3 or a submerge ended it;
                                     anub.dps rows per role; each anub.dside group per wave, and melee walks
                                     across the midline between the side spots
    anub_arak.py <file> --swarm      phase 3: Leeching Swarm per bot, lust; per 5 s his health, the median
                                     tank and non-tank health, the swarm on each and the healers' heals onto
                                     tanks, carriers and others; bot heals it cast onto a non-tank with no
                                     Penetrating Cold at 10%+; DPS bot samples on him, on burrowers up as it
                                     began and on newer ones; when those burrowers went against lust and
                                     anub.burst open; bot burst cast from his 40% to phase 3, flagged unless
                                     anub.burst read behind; Penetrating Cold, and per carrier window in phase 3
                                     the seconds it carried it with no HoT or shield on it; anub.swarmheal rows,
                                     each healer bot's anub.healduty and the carriers its anub.coldheal kept up;
                                     per phase 3 Penetrating Cold wave the first ticks survived, the living
                                     toc.aura frost holders at the landing, Aura Mastery up at the first two
                                     ticks, anub.mastery rows to the second tick, the resisted share per tick, and
                                     per carrier its health at the landing, at risk when that less 3 s of the
                                     swarm is under the tick, the first tick's margin and fate, its anub.coldheal
                                     healers and the heals and absorbs on it before the tick by source; a herd,
                                     two healers or more landing on one carrier within 1.5 s while another at
                                     risk had nothing from a healer; per non-tank its effective heals received a
                                     second alive by kind (healers, own, Judgement of Light, Vampiric Embrace,
                                     Healing Stream Totem, other) and the swarm's drain; Judgement of Light's
                                     uptime on him off the casts, each paladin's judgements, anub.judge rows,
                                     each shaman's water totem at the start and 10 s in; deaths
    anub_arak.py <file> --healers    per healer and phase its median distance to the main tank and each side
                                     tank, the seconds over 35 yd from a side tank a burrower was on, follow
                                     acts in the pull, its engine passes, anub.hstand; per healer its first
                                     anub.hbehind go and the seconds until it stood still at its stand; each
                                     burrower within 5 yd of a healer in phase 3 with no step from its exit or
                                     emerge within 7.5 yd of a Permafrost centre; per side tank and the main tank
                                     the seconds alive with no healer within 40 yd; raid healer engine in the
                                     coverage; per phase healing done on Warcraft Logs' rules, pets and totems
                                     folded into their owner: effective healing plus the absorbs credited to the
                                     shield's caster, overheal as a share of that plus overheal, HPS over the
                                     phase and the share of the raid's, and the sources with no owner in the
                                     trace; per healer its mana at phase 3's start and 30 s in, and per player
                                     its Divine Plea, Innervate with its target and Hymn of Hope casts with
                                     their phase; per warlock and mage bot its mana the same way and its Life
                                     Taps and Evocations from his 40% to phase 3
    anub_arak.py <file> --tanks      Expose Weakness peak per burrower wave per side tank; per run of it on the
                                     main tank its peak, the burrowers on him at a stack and his area casts (Swipe,
                                     Challenging Roar, ...) from 5 s before the first stack to the peak; per side
                                     tank and surfaced phase its area casts by spell, anub.saoe rows; each tank
                                     defensive cast with the stack then, from its cast row; each tank form or
                                     stance (bear forms, Defensive Stance, Righteous Fury, Frost Presence) lost
                                     while he was up, not to a death, with the act row that dropped it, the
                                     action it ran for and when the form came back; each tank death with the
                                     damage taken and the heals landed in the 12 s before
    anub_arak.py <file> --scarabs    the scarabs alive 0, 10, 20 and 30 s after each emerge with their targets,
                                     the main tank's Mandibles stack at the emerge, scarab lives per phase, per
                                     scarab victim its stacks gained moving or standing with the gap to the
                                     scarab and the seconds one within 15 yd chased it by anub.scarabkite, and
                                     per phase each player's Mandibles peak stack and tick damage
    anub_arak.py <file> --redirects  each Misdirection and Tricks of the Trade: caster, recipient, the caster's
                                     target, and a burrower's target 1 to 8 s after; totals per recipient
    anub_arak.py <file> --pursuit    pure pursuit replayed against the real spike per kite: end and patch-pass
                                     errors 3 and 6 s on, a p90 patch-pass error over 1.5 yd flagged
    anub_arak.py <file> --kitesim    25H: a port of the kite planner replayed from each bot kite's start, closed
                                     loop as the native test plays it, with no Hands, own immunity or speed
                                     boost, a patch on the spike's next leg toward the gather after a patch
                                     end or a catch counted as the kite's hazard at its end, and a catch whose
                                     spike reaches the gather's clearance before it despawns as a raid pass at
                                     once: its
                                     first pick as anub.kitepred, where the replay ended against where the real
                                     kite did, and for the fresh-spike kites, base speed and over 2 s to the
                                     replay's first event, the patch ends under 10 s, the tank patch passes and
                                     the raid passes, real against replayed

The banner names the probes the pull never wrote, a side left without a side tank, and a human on one.

What the generic views get wrong here, and what this reads instead:

- **Two units are called Anub'arak.** The Pursuing Spike (34660) carries his name and the boss flag,
  so anything keyed on the name or the flag can pick the spike. He is keyed on entry 34564 here.
- **The `anub-arak` slug is also Azjol-Nerub's boss**, so the banner checks map 649 as well.
- **One entry, two states.** A flying Frost Sphere hovers at z 155.67 and a patch sits at 142.7, and
  neither carries Permafrost itself, so spheres are split by height and Permafrost is read off
  `snap.hz`. A low sphere with no Permafrost centre on it is still falling: it casts Permafrost
  1.5 s after it drops.
- **Creature auras and deaths are never recorded**, so a burrower's submerge only shows as its health
  going back to full, which the core sets as it emerges, its time on Permafrost as its distance from a
  Permafrost centre, and its death as its last sample before the trace ends.
- **`anub.kite`, `anub.dodge`, `anub.sphere` and the other derived keys are per bot and change only**:
  a bot that takes the same branch twice writes one row. `anub.kite` goes back to `none` when the mark
  moves on, so a window never opens on the branch the last one ended with. `anub.gather` never does,
  so a bot still reads `hold` while it kites, after it dies and into the next submerge: the gather
  centre counts only living bots off the mark whose hold began in this submerge. `anub.kiteplan`,
  `anub.kitepred` and `anub.kitepath` never reset either, so a kiter whose first pick reads the same as its
  last writes nothing: a mark reads the value carried in only when no row of its own lands within a second
  of it. A held plan writes no kitepath, so each kitepath row is a new pick, and one that fires the kiter's
  ready speed boost ends with `+boost@<s>`, its second from the pick, after any `+hand` or `+own`; older
  traces never write it. `anub.kitemove` goes back to
  `none` as the mark moves on, so a turn reads the kitemove held then. Older traces also write
  `anub.bridge`, back to `none` the same way, so a bridge counts only from rows inside the window: an `ask`
  at the turn and `hand` as the Hand lands there; with no `ask`, `hand` is the Hand landing at the catch and
  the turn is the patch kitestage after it. Newer ones have no bridge, and a turn is the patch kitestage.
- **`anub.p3wave` is written by whichever bot reads the wave**, change-only per bot, so its rows are read as
  one stream with a value two bots wrote counted once.
- **`anub.focus0` and `anub.focus1` are raid-wide latches, one per side**, so the `g` on their rows is
  whichever bot's read moved them, and whether the DPS followed is read off their snapshot targets, not
  off `anub.dps`. So are `anub.side0`, `anub.side1`, `anub.wave`, `anub.grip`, `anub.allow`, and
  `anub.spot0` and `anub.spot1`: each side's tank spot as `x,y,z`, its latched patch while it has one,
  else where the open side wants one; z is left out here. Older traces carry one raid-wide `anub.focus`
  and its `anub.focusdue` instead, read here as a focus with no side. `anub.bside` is per burrower, its
  `g` the burrower, and `anub.dside` per DPS bot, the burrower side it serves, written at a wave's split
  only for a bot whose side changed. `anub.hstand` is per healer, `walk` or `hold`, and so is
  `anub.hbehind`, `wait`, `go` or `none`. `anub.escort` is per paladin bot, `walk`, `hold`, `post`, `patch`,
  `standby` for one shadowing a bot kiter whose plan lands no other paladin's Hand, or `none`, older traces
  never writing `standby`. `anub.saoe` is per side tank: the spell its area threat node
  cast, else `cap`, `other` or `none`. In heroic phase 3, `anub.swarmheal` is each healer's pick: `self`,
  `risk`, `tank`, `low`, `bare`, `cold` or `none`, older traces never writing `self` or `risk`.
  `anub.healduty` is per healer bot from the phase's start, `1` cold and `2` tank, and `anub.coldheal` is
  keyed on the healer bot with the carrier it keeps up as its value, `0` when that ends. `anub.mastery` is
  per paladin bot, `none`, `hold N` with N carriers under the tick, or `cast`, and `toc.aura` per paladin
  and hunter bot, the resistance it holds or `none`. `anub.push` is written once per phase 1 by the bot
  whose read latched it, `on:<seconds needed>:<seconds left>`, and `anub.kitespent` by the bot whose read
  saw a kite patch go, the count this submerge. `anub.scarabkite` is per bot, `walk`, `hold` or `none`, and
  written only while a scarab is on it, so it reads per snapshot a scarab chases the bot. `anub.topup` is
  per warlock and mage bot, `tap`, `evocation` or `none`, and `anub.judge` is written at each Judgement of
  Light hand-over in phase 3, `<name>:<mana%>`. A Judgement of Light on him is a creature aura, so its
  uptime is read off the casts.
- **Health in points is read off the pool at first sight.** Snapshots and combat rows carry a percentage,
  so a carrier under a health buff reads short. A first tick it lived through still reads a margin of 0
  or more.
- **No aura row says whether it heals over time or absorbs.** A HoT is read off a hand-kept list of
  names and a shield off the trace's `abs` rows, so a shield that never absorbed reads as no cover.
- **No row carries a speed.** The spike's speed and the kiter's are the median of their moving
  snapshot steps around the moment, and the spike's speed auras are creature auras, never recorded.
- **No `act` row names its engine.** `cov` counts each engine's passes per bot from the session's open,
  so the non-combat engine's passes while he is in combat are a pull total, not a timeline.
- **A sphere's killer is rarely a hit.** `dmg` rows only land on the roster, so the killer is the last
  `dmg` row on the sphere when there is one, else the last cast aimed at it. The killing blow makes it
  unselectable and it casts Permafrost 1.5 s later, so its death is timed off the patch, not its
  height: a gripped sphere sits low while it's still alive.
"""
from __future__ import annotations

import bisect
import collections
import functools
import math
import pathlib
import re
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, map_of  # noqa: E402
from raidobs.geometry import anchor, dist2, frames, guids_of_entry, radius  # noqa: E402
from raidobs.paths import RAID_ROOT  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.space import threat_share  # noqa: E402
from raidobs.trace import COVERAGE_COLUMNS, Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402

MAP_TOC = 649
DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}
HEROIC = (2, 3)

NPC_ANUBARAK = 34564
NPC_FROST_SPHERE = 34606
NPC_BURROWER = 34607
NPC_SPIKE = 34660
NPC_SCARAB = 34605
ENCOUNTER_NPCS = (NPC_ANUBARAK, NPC_FROST_SPHERE, NPC_BURROWER, NPC_SPIKE, NPC_SCARAB)

SPELL_MARK = 67574
SPELL_SHADOW_STRIKE = 66134
SPELL_LEECHING_SWARM_DMG = 66240
# 10N, 25N, 10H, 25H
IMPALE = (65919, 67858, 67859, 67860)
PERMAFROST = (66193, 67855, 67856, 67857)
PENETRATING_COLD = (66013, 67700, 68509, 68510)
LEECHING_SWARM = (66118, 67630, 68646, 68647)
# Bloodlust, Heroism
LUST_SPELLS = (2825, 32182)
# What the burst gate holds (BurstCooldowns.cpp names), by the spell each name casts; trinkets and
# Killing Machine are procs with no one id
BURST_SPELLS = {26297: "Berserking", 20572: "Blood Fury", 33697: "Blood Fury", 33702: "Blood Fury",
                54758: "Hyperspeed Acceleration", 1719: "Recklessness", 12292: "Death Wish",
                13750: "Adrenaline Rush", 13877: "Blade Flurry", 51690: "Killing Spree", 12042: "Arcane Power",
                12472: "Icy Veins", 11129: "Combustion", 55342: "Mirror Image", 12043: "Presence of Mind",
                47241: "Metamorphosis", 3045: "Rapid Fire", 19574: "Bestial Wrath", 23989: "Readiness",
                10060: "Power Infusion", 50334: "Berserk", 33831: "Force of Nature", 31884: "Avenging Wrath",
                42650: "Army of the Dead", 49206: "Summon Gargoyle", 2894: "Fire Elemental Totem",
                16166: "Elemental Mastery", 53908: "Potion of Speed", 53909: "Potion of Wild Magic"}
# The stem's heroic phase 3 numbers: burst waits from BURST_HOLD_PCT, and a non-tank without Penetrating
# Cold gets a heal only under SWARM_FLOOR_PCT
BURST_HOLD_PCT = 40.0
SWARM_FLOOR_PCT = 10.0
SWARM_BIN_MS = 5000
# Leeching Swarm's share of current health a second, 10N, 25N, 10H, 25H
SWARM_DRAIN = (0.1, 0.1, 0.2, 0.3)
# Penetrating Cold's first tick lands COLD_TICK_MS after the aura and the rest COLD_TICK_MS apart. A wave's
# carriers get it in the same server tick, and a stall can push a tick row up to COLD_TICK_SLACK_MS late.
COLD_TICK_MS = 3000
COLD_WAVE_MS = 1000
COLD_TICK_SLACK_MS = 1000
SPELL_AURA_MASTERY = 31821
# two healers or more landing on one carrier this close together while another carrier at risk got nothing
HERD_MS = 1500
# AnubarakHealDuty, as anub.healduty writes it
HEAL_DUTIES = {"0": "none", "1": "cold", "2": "tank"}
# Hand-kept, since no aura row says whether it heals over time: the HoTs a Penetrating Cold carrier can wear,
# any rank. A shield is any aura the trace shows absorbing.
HOT_NAMES = {"Renew", "Rejuvenation", "Regrowth", "Lifebloom", "Wild Growth", "Riptide", "Earthliving",
             "Gift of the Naaru", "Lifeblood", "Lightwell Renew", "Enraged Regeneration", "Flash of Light"}
SPELL_RIGHTEOUS_DEFENSE = 31789
# what CastClassTaunt casts, plus Righteous Defense, which targets the friend he is hitting
TAUNT_SPELLS = {355: "Taunt", 62124: "Hand of Reckoning", 56222: "Dark Command", 6795: "Growl",
                SPELL_RIGHTEOUS_DEFENSE: "Righteous Defense"}
HAND_OF_PROTECTION = (1022, 5599, 10278)
SPELL_FEIGN_DEATH = 5384
KITER_OWN = {642: "Divine Shield", 45438: "Ice Block", SPELL_FEIGN_DEATH: "Feign Death", 26889: "Vanish"}
SPELL_FORBEARANCE = 25771
CLASS_PALADIN = 2
# The stem casts these as the kiter's last resort, once its anub.protect reads self
LAST_RESORT_LEAD_MS = 2000
# 55004 is the Nitro Boosts use, 54861 the speed it gives. A druid out of Cat Form shifts into it for Dash,
# and Feral Charge onto a bug is its last tool.
SPELL_CAT_FORM = 768
KITE_BOOSTS = {11305: "Sprint", 33357: "Dash", 55004: "Nitro Boosts", 54861: "Nitro Boosts", 1953: "Blink",
               48020: "Demonic Circle: Teleport", SPELL_CAT_FORM: "Cat Form", 16979: "Feral Charge",
               49376: "Feral Charge"}
# what speeds a kiter up, Cat Form only readies Dash
RUN_BOOSTS = {spell: name for spell, name in KITE_BOOSTS.items() if spell != SPELL_CAT_FORM}
# the boosts the kite planner times, the others jump
SPEED_BOOSTS = ("Sprint", "Dash", "Nitro Boosts")
# a boosted kiter's own run is read off this much before the boost
BOOST_RUN_MS = 1500
# the game re-plans a kite every 250 ms and casts the boost on the AI tick after its second comes
BOOST_ON_TIME_MS = 500
NITRO_SPEED = 54861
# one boost read twice, a use and its aura or a cast and its triggered copy
BOOST_PAIR_MS = 1000
SPELL_DEATH_GRIP = 49576
# 49576 is magic-class and can miss: one that hits casts this triggered pull the same ms
SPELL_DEATH_GRIP_PULL = 49560
GRIP_PULL_MS = 5
SPELL_BLADESTORM = 46924
THREAT_CAP = "anubarak threat cap"
BURROWER_THREAT_CAP = "anubarak burrower threat cap"
# its veto on a Bladestorm is the hold with a burrower near, the action named bladestorm
BURROWER_GUARD = "anubarak burrower guard"
BLADESTORM_BURROWER_REACH = radius("ANUBARAK_BLADESTORM_BURROWER_REACH")
# 10-man and 25-man; 25H wears 67847
EXPOSE_WEAKNESS = (67721, 67847)
# ANUBARAK_SIDE_DEFENSIVE_STACKS, a uint32 radius() doesn't read
DEFENSIVE_STACKS = 5
MAGIC_ONLY_DEFENSIVES = ("anti magic shell", "cloak of shadows")
TANK_FORMS = {9634: "Dire Bear Form", 5487: "Bear Form", 71: "Defensive Stance", 25780: "Righteous Fury",
              48263: "Frost Presence"}
# Dying strips every aura in the same tick
FORM_DEATH_MS = 500
# The act row that dropped a form lands within FORM_ACT_MS of the removal, and a PREREQ row naming the action
# it ran for lands within PREREQ_MS before that
FORM_ACT_MS = 500
PREREQ_MS = 100
# NextTankDefensive's tank rows, named the way anub.defensive writes them
TANK_DEFENSIVES = {12975: "last stand", 871: "shield wall", 498: "divine protection", 22812: "barkskin",
                   22842: "frenzied regeneration", 61336: "survival instincts", 48707: "anti magic shell",
                   55233: "vampiric blood", 48792: "icebound fortitude"}
# the "anubarak side tank area threat" node's spells, named the way anub.saoe writes them
SIDE_AOE_SPELLS = ("death and decay", "consecration", "blood boil", "thunder clap", "swipe (bear)", "holy wrath",
                   "hammer of the righteous", "heart strike", "cleave", "avenger's shield", "pestilence")
# 10N, 25N, 10H, 25H
MANDIBLES = (65775, 67861, 67862, 67863)
REDIRECTS = {34477: "Misdirection", 57934: "Tricks of the Trade"}

# Hand-kept, since no dmg or cast row says whether a spell ticks or splashes: level 80 ranks of what
# bots aim at a sphere.
PERIODIC = {47813, 47864, 47811, 47843, 47867, 48468, 48463, 48125, 48160, 48300, 48156, 49001, 60053, 53352,
            63672, 49233, 47857, 47855, 42846, 55360, 47836, 48819, 49800, 48672, 48676, 47465, 42926}
AREA = {53190, 53195, 42940, 47820, 47823, 58434, 48467, 49938, 51411, 49941, 49048, 49271, 42921, 42926,
        51723, 47520, 47502, 48562, 62078, 48817, 53385, 59159, 61657, 47836, 55360, 48819, 1680, 53595,
        47897, 61290, 42950, 42945, 46924}
# aimed at spheres in real pulls but deal no damage, so never the killer
NO_DAMAGE = {SPELL_DEATH_GRIP, 53338, 47865}

PHASE_LABELS = {0: "none", 1: "P1", 2: "P2", 3: "P3"}

FOCUS = "anub.focus"
FOCUS_KEYS = ("anub.focus0", "anub.focus1")
DPS_ROLES = ("melee", "ranged")
SIDE_TANKS_NEEDED = {0: 1, 1: 2, 2: 1, 3: 2}
GRIP_STATES = {"0": "none", "1": "gripping", "2": "shooting"}
# A side this long without a tank patch while he's up with burrowers out: its pair can submerge to full
UNPATCHED_FLAG_MS = 15000

# Every key a section reads, so the banner flags one this trace lacks even while the source doesn't
# declare it in a shape the probe parse finds.
READ_KEYS = ("toc.progress", "anub.phase", "anub.patch0", "anub.patch1", "anub.sphere", "anub.kite",
             "anub.dodge", "anub.protect", "anub.interrupter", "anub.pickup", "anub.defensive", "anub.focus0",
             "anub.focus1", "anub.dps", "anub.side0", "anub.side1", "anub.wave", "anub.grip",
             "anub.bside", "anub.dside", "anub.gripstep", "anub.sidetank", "anub.kitestage", "anub.kiteplan",
             "anub.gather", "anub.spot0", "anub.spot1", "anub.hstand", "anub.kitemove", "anub.kitepath",
             "anub.kitepred", "anub.relay", "anub.subspot", "anub.saoe", "anub.hbehind", "anub.swarmheal",
             "anub.healduty", "anub.coldheal", "anub.mastery", "anub.p3wave", "toc.aura")
SPOT_KEYS = ("anub.spot0", "anub.spot1")

# Read from the source so a retune shows up here without a second edit.
ROOM_CENTER = anchor("ANUBARAK_ROOM_CENTER")
TANK_PATCH_OFFSET = radius("ANUBARAK_TANK_PATCH_OFFSET")
PERMAFROST_SLOW_REACH = radius("ANUBARAK_PERMAFROST_SLOW_REACH")
GRIP_LANDED = radius("ANUBARAK_GRIP_LANDED")
GRIP_LANDED_HEIGHT = radius("ANUBARAK_GRIP_LANDED_HEIGHT")
# once one side has a patch, the other side's spot sits the two spots' gap off it in y
SIDE_SPOT_GAP = 2.0 * TANK_PATCH_OFFSET
# ANUBARAK_GRIP_JUDGE_MS, a uint32 radius() doesn't read
GRIP_JUDGE_MS = 3000
# ANUBARAK_GRIP_RELAY_FROM, an alias radius() doesn't read: a grip cast further off its side's spot gets a
# relay link
GRIP_RELAY_FROM = radius("ANUBARAK_SIDE_SHOOT_FAR")
# The second of ANUBARAK_SUBMERGE_CLEARANCES, an array radius() doesn't read: a submerge spot nearer a
# tank patch is a fallback
SUBMERGE_PATCH_CLEARANCE = 25.0
SPIKE_PATCH_REACH = radius("ANUBARAK_SPIKE_PATCH_REACH")
SPIKE_ROUTE_CLEARANCE = radius("ANUBARAK_SPIKE_ROUTE_CLEARANCE")
FOCUS_SETTLE = radius("ANUBARAK_FOCUS_SETTLE")
FOCUS_LOW_PCT = radius("ANUBARAK_FOCUS_OFF_FROST_PCT")
# ANUBARAK_KITE_GATHER_CLEARANCE, a sum radius() doesn't read: a kite ending nearer hands the spike to the
# gather
KITE_END_GATHER_YD = 20.0
# Permafrost's target search adds only the target's size. A burrower's display has no combat reach, so
# it gets the core's 0.389 default, not the 1.5 of ANUBARAK_PERMAFROST_SLOW_REACH's player.
BURROWER_PERMAFROST_REACH = 6.0 + 0.389

# Not constants in the module. The Web Door is GO 195485 and shuts at the engage. Before the floor
# breaks the raid stands near z 395, the pit floor is 142. Spheres fly at 155.67 and land at 142.7.
WEB_DOOR_X = 661.6
LANDED_Z = 200.0
# Locs[17], where he waits before the pull, and his proximity aggro: detection_range 20 plus the level gap
ANUB_WAIT = (785.90, 133.42)
AGGRO_YD = 23.0
SPHERE_FLYING_Z = 150.0
SPHERE_PATCH_Z = 146.0
# a patch's Permafrost dynobject sits on the sphere itself
PATCH_MATCH_YD = 1.0
# (x, y, side) per burrow exit: the north pair feeds side 0, the south pair side 1
BURROW_EXITS = ((740.2, 193.4, 0), (694.5, 185.4, 0), (732.0, 83.4, 1), (694.9, 102.5, 1))
# the killing blow sets the sphere falling and it casts Permafrost this long after
PERMAFROST_DELAY_MS = 1500
# a wave's burrowers spawn in one tick and waves come 45 s apart
WAVE_SPLIT_MS = 10000

LEG_MIN_YD = 2.0
REVERSAL_DEGREES = 120.0
# he stood this far off the midpoint of the side spots with no drag leg: held off the anchor
OFF_ANCHOR_YD = 10.0
HELD_OFF_FLAG_MS = 10000
# a pre-step is the tank walking round him before its leg, so a drag too
DRAG_LEGS = ("drag", "submerge", "prestep")

# the spike's fail despawns the patch 1.5 s after the mark comes off
CONSUMED_MS = 3000
# the spike despawns 2 s before he turns selectable, so a mark ended by the emerge lands near that edge
EMERGE_SLACK_MS = 3000
IMPALE_SLACK_MS = 500
# Divine Shield, Ice Block, Feign Death and Vanish hand the mark on at once, so one cast this close
# before the mark comes off ended it
IMMUNE_END_MS = 1000
SUBMERGE_GAP_MS = 5000
# npc_nerubian_burrowerAI runs its first submerge check 30 s after it spawns or emerges
BURROWER_SAFE_MS = 30000
NEAR_FULL = 95.0
SUBMERGE_JUMP = 15.0
SHADOW_STRIKE_CAST_MS = 8000
LAND_SLACK_MS = 1000
# a row this far after the asked time still answers it when nothing was sampled before
NEAR_MS = 2000
# one snapshot gap longer than this is a hole in the trace, not time spent anywhere
MAX_STEP_MS = 2000
TIMELINE_SHOWN = 30
# a Permafrost stint longer than this is listed on its own
FROST_STINT_MS = 3000
FROST_GROUPS = ("main tank", "side tank", "tank", "melee", "ranged", "heal")

# A kiter that moved less than STALL_YD over a STALL_MS stretch of its mark stood still
STALL_YD = 1.5
STALL_MS = 1000
# A snapshot step slower than this is a stop or a turn, not the unit's speed. Steps read 0.1 yd/s either
# side of the true speed, so a spike within SPEED_SLACK of the kiter counts as no faster.
MOVING_YD_S = 1.0
SPEED_SLACK = 0.2
# playerBaseMoveSpeed[MOVE_RUN], for a kiter the snapshots never caught moving
BASE_RUN_YD_S = 7.0
# A kiter whose gap to a spike it outruns shrank this long, ending inside KITE_PROTECT_RANGE, walked into it
CLOSING_MS = 2000
CROWD_YD = (8.0, 12.0, 20.0)
CROWD_ROLES = ("tank", "heal", "melee", "ranged")
# Three burrowers on one side tank kill it at 9 Expose Weakness stacks; the taunts in the second before say
# how it got them. Righteous Defense's triggered taunt goes out once per attacker it took.
TANK_CROWD = 3
TANK_CROWD_CAST_MS = 1000
SPELL_RIGHTEOUS_DEFENSE_TAUNT = 31790
BURROWER_TAUNTS = {355: "Taunt", 62124: "Hand of Reckoning", 56222: "Dark Command", 6795: "Growl",
                   SPELL_RIGHTEOUS_DEFENSE_TAUNT: "Righteous Defense", 49576: "Death Grip",
                   5209: "Challenging Roar", 1161: "Challenging Shout"}
# Spider Frenzy (66129) hastes burrowers within this of each other
SPIDER_FRENZY_YD = 12.0
# a kiter's move targets jumping FLIP_YD or more and back FLIP_COUNT times inside FLIP_MS: two walks
# cancelling each other out
FLIP_YD = 5.0
FLIP_COUNT = 3
FLIP_MS = 2000
# Feign Death breaks on move, so cast while running it comes off the same tick
FEIGN_DROP_MS = 100
# the plan is judged at the mark, so a kiter's row this close to it is that mark's first pick
KITEPLAN_SETTLE_MS = 1000
# what each anub.kiteplan terminal spends; `none` spends nothing
PLAN_NEEDS = {"patch": ("patch",), "hand": ("hand",), "patch+hand": ("patch", "hand"), "own": ("own",)}
PATCH_PLANS = ("patch", "patch+hand")
# what an anub.kitepred pick names first when it foresees one, worst first
KITEPRED_HAZARDS = ("tank", "kite", "raid", "catch")
# the planner holds a turn this sharp until its plan is this old, else the kiter stands between two headings
PICK_REVERSAL_DEGREES = 90.0
PICK_REVERSAL_MS = 1000
# what a kite runs on, any rank; 55004 is the Nitro Boosts use
KITE_TOOLS = {11305: "Sprint", 8696: "Sprint", 2983: "Sprint", 33357: "Dash", 9821: "Dash", 1850: "Dash",
              55004: "Nitro Boosts", 642: "Divine Shield", 45438: "Ice Block", 26889: "Vanish", 1857: "Vanish",
              1856: "Vanish", SPELL_FEIGN_DEATH: "Feign Death"}
# their base cooldowns: one spent off the mark costs a kite only while still out as a submerge's spike comes up
KITE_TOOL_COOLDOWN_MS = {11305: 180000, 8696: 180000, 2983: 180000, 33357: 180000, 9821: 180000, 1850: 180000,
                         55004: 180000, 642: 300000, 45438: 300000, 26889: 180000, 1857: 180000, 1856: 180000,
                         SPELL_FEIGN_DEATH: 30000}
# the spike is summoned on him this long after he goes down
SPIKE_SUMMON_MS = 2500
SPELL_CIRCLE_SUMMON = 48018
SPELL_CIRCLE_TELEPORT = 48020
# on the warlock while its circle is within the teleport's 40 yd
SPELL_CIRCLE_IN_RANGE = 62388
CLASS_WARLOCK = 9
# heroic downs two kite spheres a submerge, so one spent this soon into its kite left the rest uncovered
KITE_PATCH_EARLY_MS = 15000
# a teleported warlock has walked on once this far from where it landed, and it landed within this of its
# circle
LANDED_MOVE_YD = 1.5
LANDED_ON_CIRCLE_YD = 3.0
# most bots hold their phase 2 gather slot this long after he goes down
GATHER_FORMED_MS = 10000
# anub.bridge values a patch+hand kite writes at its turn. Older traces ask for the Hand there, so their
# hand row follows the ask; with no ask the hand row is the Hand landing at the catch, and the kite turns
# under it later.
BRIDGE_TURNS = ("ask", "turn")
# rows one tick writes can land a few ms apart
NOTE_TICK_MS = 200
# the bridge waits up to 1 s at the turn for its Hand, and the immune stage follows the cast by a tick
HAND_AT_TURN_MS = 1500
BRIDGE_TEXT = {"hand": "Hand at the turn", "turn": "turned without a Hand",
               "ask": "Hand asked for, never landed or turned", "early": "Hand before the turn",
               "late": "Hand after the turn", "never": "never turned",
               "catch": "Hand at the catch, the turn under it"}
HOP_RANGE = radius("ANUBARAK_HOP_RANGE")
# HoP's base cooldown, for a trace with no anub.hands to read it off: a paladin bot read 180 s after its
# last Hand stayed unclaimable for 277-293 s, so the bots carry no Guardian's Favor
HOP_COOLDOWN_MS = 300000
ESCORT_BY = "anubarak hop escort"
# anub.escort for a paladin shadowing a bot kiter whose plan lands no other paladin's Hand
ESCORT_STANDBY = "standby"
DODGE_BY = "anubarak avoid spike"
GATHER_BY = "anubarak gather"
# a walk to the escort spot and a dodge this close together are one swap
ALTERNATE_MS = 1000
# a bystander's gather walk and dodge this close together are one swap, and a dodge this far off its last
# spot is a new dodge walk
DODGE_SWAP_MS = 1500
DODGE_SPOT_YD = 1.0
DODGE_SWAPPERS_SHOWN = 5
# a claim shows as the claimer's first anub.protect hop row, at most this long before its cast
CLAIM_LOOKBACK_MS = 5000
CLAIM_CAST_FLAG_MS = 300
# a boost this early in a Hand runs while the spike sits on the kiter anyway
HAND_BOOST_MS = 8000
# a patch kite turning this close to a boost spent the boost on the walk in
BOOST_TURN_MS = 3000
# the spike this close sends a paladin's Hand of Protection on older traces; under a Hand the spike trails
# 2-3 yd, so a kiter this close to a tank patch walks it into its 11.5 yd test
KITE_PROTECT_RANGE = 12.0
HAND_PATCH_CLEARANCE = 15.5
# heroic shoots a submerge's first kite sphere from this long before he goes down
KITE_PRESHOT_MS = int(radius("ANUBARAK_KITE_PRESHOT_LEFT_S") * 1000)
# ANUBARAK_KITE_SPHERES_PER_SUBMERGE, a uint32 radius() doesn't read
KITE_SPHERES_PER_SUBMERGE = 2
# a stray patch this close to a tank patch is no kite patch: the spike's restart test plus its first walk
KITE_PATCH_TANK_YD = SPIKE_PATCH_REACH + 5.25
# the emerge despawns the spike a minute after he goes down
SPIKE_LIFE_MS = 60000
# rank 3, for a cast whose aura the trace doesn't hold
HOP_DURATION_MS = 10000
IMPALE_REACH = radius("ANUBARAK_IMPALE_REACH")
KITE_RING_RADIUS = radius("ANUBARAK_KITE_RING_RADIUS")
CATCH_HORIZON_S = radius("ANUBARAK_KITE_CATCH_HORIZON_S")
# AnubarakPursuit's SPEEDS, STEP_S and TICK_S, an array and names too short for radius()
SPIKE_SPEEDS = (3.5, 3.5 * 1.75, 3.5 * 3.25)
SPIKE_STEP_S = 7.0
CATCH_TICK_S = 0.1
# a fail idles the spike 4 s and it restarts at its base speed; a death or an immunity hands the mark on
# within 300 ms at the speed it had
SPIKE_RESTART_GAP_MS = 2000
SCARAB_OFFSETS_MS = (0, 10000, 20000, 30000)
# when a redirect's burrower is read again for its target
REDIRECT_AFTER_MS = (1000, 2000, 3000, 5000, 8000)
OPENER_NEAREST = 3
# pursuit replay: a model spike every PURSUIT_STEP_MS of a chase, a snapshot gap this long ends the chase,
# patches within PURSUIT_PATCH_YD of the start judged
PURSUIT_HORIZONS_MS = (3000, 6000)
PURSUIT_STEP_MS = 500
PURSUIT_GAP_MS = 600
PURSUIT_PATCH_YD = 30.0
PURSUIT_FLAG_YD = 1.5
# a submerged burrower keeps its spot and its health until it comes back up full
FROZEN_YD = 0.3
# a submerged burrower targets nobody, so its victim is the last target it had this far back
VICTIM_LOOKBACK_MS = 5000
# a burrower pulled off its side tank this soon after it came up, and the puller's casts on it this far back
PULL_WINDOW_MS = 15000
PULL_CASTS_MS = 3000
HEALER_FAR_YD = 35.0
HEALER_REACH_YD = 40.0
# NOMINAL_MELEE_RANGE: a burrower this close to a healer can hit it
MELEE_YD = 5.0
TANK_DEATH_LOOKBACK_MS = 12000
# Challenging Roar and Challenging Shout: taunts that reach every enemy around the tank
AREA_TAUNTS = (5209, 1161)
# the main tank's area casts this far before a burrower's first Expose Weakness on him
EXPOSE_LEAD_MS = 5000
HEALER_ENGINE = "raid healer engine"
MANA_COOLDOWNS = {54428: "Divine Plea", 29166: "Innervate", 64901: "Hymn of Hope"}
SPELL_INNERVATE = 29166
# phase 3's mana is read at its start and again this far in
SWARM_MANA_MS = 30000
CLASS_SHAMAN = 7
CLASS_MAGE = 8
# the warlocks' taps and the mages' Evocations are counted from his health this low
TOPUP_FROM_PCT = radius("ANUBARAK_TOPUP_PCT")
TOPUP_SPELLS = ("Life Tap", "Evocation")
# the cast spells; each one's debuff lasts 20 s, and a paladin's next judgement anywhere replaces it
JUDGEMENTS = {20271: "Light", 53408: "Wisdom", 53407: "Justice"}
JUDGEMENT_MS = 20000
SUSTAIN_KINDS = ("healers", "own", "JoL", "VE", "HST", "other")
WATER_TOTEMS = ("Healing Stream Totem", "Mana Spring Totem", "Mana Tide Totem", "Cleansing Totem",
                "Fire Resistance Totem")
# the water totem is read at phase 3's start and this far in
WATER_TOTEM_LATER_MS = 10000
# a scarab this close on its victim counts as chasing it
SCARAB_KITE_YD = radius("ANUBARAK_SCARAB_KITE_REACH")
# ANUBARAK_LAST_KITE_SUBMERGE: the first two submerges keep a kite patch for the third
LAST_KITE_SUBMERGE = 3
ENGINE_NAMES = {"c": "combat", "n": "non-combat", "d": "dead"}
# Every node of an engine lands in exactly one of these columns on every pass
PASS_COLUMNS = ("checks", "shared", "throttled", "minimal", "dead")
DPS_KINDS = ("boss", "burrower", "scarab", "other", "none")
# a kill needs about this much of his health per phase 1, with Berserk at 10:00
BOSS_PACE_PCT = 20.0
# a snapshot step under this is jitter and over that a release or a teleport, so neither is a walk
WALK_MIN_YD = 0.3
WALK_MAX_YD = 30.0
# a step counts toward the bot's last move row only this long after it
MOVE_ROW_MS = 4000
WALKED_SHOWN = 6
# a melee bot this close to the midline between the side spots stands on neither side's half
MIDLINE_BAND_YD = 5.0


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def boss_guid(trace: Trace) -> int | None:
    guids = sorted(guids_of_entry(trace, NPC_ANUBARAK))
    return guids[0] if guids else None


def is_heroic(trace: Trace) -> bool:
    return trace.header.get("diff") in HEROIC


def short(trace: Trace, guid: int) -> str:
    """A creature's name repeats across every burrower and sphere, so those print by counter."""
    if guid in trace.roles:
        return trace.name(guid)
    return f"#{guid & 0xFFFFFFFF}"


def unit_label(trace: Trace, guid) -> str:
    """A target as a reader recognises it: a roster name, a pet or a ghoul by name and owner, him, or a
    creature's kind and counter."""
    entry = trace.entries.get(guid)
    if not guid or guid in trace.roles or guid in trace.owners or entry is None:
        return trace.name(guid)
    if entry == NPC_ANUBARAK:
        return "Anub'arak"
    kind = {NPC_BURROWER: "burrower", NPC_FROST_SPHERE: "sphere", NPC_SPIKE: "spike"}.get(entry, trace.name(guid))
    return f"{kind} {short(trace, guid)}"


def per_bot(trace: Trace, key: str) -> dict[int, list[tuple[int, str]]]:
    out: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, key):
        out[rec.get("g", 0)].append((rec["t"], str(rec.get("txt", ""))))
    return out


def value_counts(trace: Trace, key: str) -> dict[int, collections.Counter]:
    """Rows per value per bot. A derived key writes one row per change, so this counts entries."""
    return {guid: collections.Counter(value for _, value in rows)
            for guid, rows in per_bot(trace, key).items()}


def show_counts(trace: Trace, key: str) -> None:
    counts = value_counts(trace, key)
    if not counts:
        print(f"  {key}: no rows")
        return
    print(f"  {key}: " + "; ".join(
        f"{trace.name(guid)} " + ", ".join(f"{value} {n}" for value, n in counter.most_common())
        for guid, counter in sorted(counts.items(), key=lambda item: trace.name(item[0]))))


def aura_spans(trace: Trace, spells) -> dict[int, list[tuple[int, int, bool]]]:
    """Per target, `(apply, remove, closed)`. One still up when the file ends runs to the end, open."""
    end = pull_end(trace)
    opened: dict[int, int] = {}
    out: dict[int, list[tuple[int, int, bool]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") not in spells:
            continue
        guid = rec.get("d", 0)
        if rec.get("r"):
            if guid in opened:
                out[guid].append((opened.pop(guid), rec["t"], True))
        elif guid not in opened:
            opened[guid] = rec["t"]
    for guid, start in opened.items():
        out[guid].append((start, max(start, end), False))
    return out


class Samples:
    """Snapshot rows by guid, plus each snapshot's Permafrost centres and sphere counts."""

    def __init__(self, trace: Trace):
        spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
        self.stamps: list[int] = []
        self.frost: list[list[tuple[float, float]]] = []
        self.spheres: list[tuple[int, int, int]] = []
        self.sphere_rows: list[list[list]] = []
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
            when = snap["t"]
            self.stamps.append(when)
            frost = [(hz[1], hz[2]) for hz in snap.get("hz", []) if len(hz) >= 3 and hz[0] in PERMAFROST]
            self.frost.append(frost)
            flying = patches = 0
            mine = []
            for row in snap.get("u", []):
                stamps, rows = self.rows.setdefault(row[0], ([], []))
                stamps.append(when)
                rows.append(row)
                if row[0] in spheres and len(row) > 3:
                    mine.append(row)
                    if row[3] > SPHERE_FLYING_Z:
                        flying += 1
                    elif row[3] < SPHERE_PATCH_Z:
                        patches += 1
            self.spheres.append((flying, patches, len(frost)))
            self.sphere_rows.append(mine)

    def track(self, guid) -> tuple[list[int], list[list]]:
        return self.rows.get(guid, ([], []))

    def row(self, guid, when: int):
        """The last row at or before `when`, else the first within NEAR_MS after it."""
        stamps, rows = self.track(guid)
        index = bisect.bisect_right(stamps, when)
        if index:
            return rows[index - 1]
        if stamps and stamps[0] - when <= NEAR_MS:
            return rows[0]
        return None

    def recent(self, guid, when: int):
        """The last row at or before `when`, only if it is no older than one snapshot gap."""
        stamps, rows = self.track(guid)
        index = bisect.bisect_right(stamps, when)
        if index and when - stamps[index - 1] <= MAX_STEP_MS:
            return rows[index - 1]
        return None

    def between(self, guid, low: int, high: int) -> list[list]:
        stamps, rows = self.track(guid)
        return rows[bisect.bisect_left(stamps, low):bisect.bisect_right(stamps, high)]

    def index_at(self, when: int) -> int:
        return bisect.bisect_right(self.stamps, when) - 1

    def frost_at(self, when: int) -> list[tuple[float, float]]:
        index = self.index_at(when)
        return self.frost[index] if index >= 0 else []

    def sphere_kinds(self, when: int) -> dict[int, str]:
        """Each sphere in the snapshot at or before `when`: `flying`, `falling` or `patch`."""
        index = self.index_at(when)
        if index < 0:
            return {}
        out = {}
        for row in self.sphere_rows[index]:
            if row[3] > SPHERE_FLYING_Z:
                out[row[0]] = "flying"
            elif any(dist2(row[1:3], centre) <= PATCH_MATCH_YD for centre in self.frost[index]):
                out[row[0]] = "patch"
            else:
                out[row[0]] = "falling"
        return out


def health(samples: Samples, guid: int | None, when: int) -> float | None:
    row = samples.row(guid, when) if guid else None
    return row[5] if row else None


def patch_latches(trace: Trace) -> list[list[tuple[str, int, int]]]:
    """`anub.patch0` and `anub.patch1` spans, indexed by side."""
    end = pull_end(trace)
    return [latch_spans(trace, key, end) for key in ("anub.patch0", "anub.patch1")]


def latched_patch(spans, samples: Samples, when: int) -> tuple[float, float] | None:
    """Where the patch a side had latched at `when` sat, None with no latch or none sampled low."""
    guid = as_guid(held_at(spans, when))
    row = samples.recent(guid, when) if guid else None
    return (row[1], row[2]) if row and row[3] < SPHERE_PATCH_Z else None


def parse_spot(text) -> tuple[float, float] | None:
    """An `anub.spotN` value, `x,y,z` or `x,y`, as its ground point."""
    parts = str(text).split(",")
    if len(parts) not in (2, 3):
        return None
    try:
        return float(parts[0]), float(parts[1])
    except ValueError:
        return None


def spot_spans(trace: Trace) -> list[list[tuple[tuple[float, float] | None, int, int]]]:
    """`anub.spot0` and `anub.spot1` spans, indexed by side, each value parsed to its ground point."""
    end = pull_end(trace)
    return [[(parse_spot(value), start, stop) for value, start, stop in latch_spans(trace, key, end)]
            for key in SPOT_KEYS]


def side_spot(latches, samples: Samples, side: int, when: int, noted=None) -> tuple[float, float]:
    """Where a side's tank patch belongs: the `anub.spotN` it held at `when` when `noted` has one, else the
    room centre's y plus TANK_PATCH_OFFSET for side 0, minus for side 1, or SIDE_SPOT_GAP off the other
    side's patch toward this side once that one latched."""
    spot = held_at(noted[side], when) if noted else None
    if spot is not None:
        return spot
    sign = 1.0 if side == 0 else -1.0
    other = latched_patch(latches[1 - side], samples, when)
    if other is not None:
        return other[0], other[1] + sign * SIDE_SPOT_GAP
    return ROOM_CENTER[0], ROOM_CENTER[1] + sign * TANK_PATCH_OFFSET


def missing_probes(trace: Trace) -> list[str]:
    emitted = emitted_keys(trace)
    declared = {key for key, _, _ in silent_keys(emitted, encounter_of(trace))}
    return sorted(declared | {key for key in READ_KEYS if key not in emitted})


def side_tanks(trace: Trace) -> tuple[int, dict[int, list[int]]]:
    """Side tanks the raid size needs, and each side's holders in the order `anub.side0` and
    `anub.side1` latched them, humans included: a human side tank swaps sides with the bot one."""
    needed = SIDE_TANKS_NEEDED.get(trace.header.get("diff"), 2)
    latched = {}
    for side in (0, 1):
        holders: list[int] = []
        for rec in notes(trace, f"anub.side{side}"):
            guid = as_guid(rec.get("txt"))
            if guid and (not holders or holders[-1] != guid):
                holders.append(guid)
        if holders:
            latched[side] = holders
    return needed, latched


def covered(spans: list[tuple[int, int]], lo: int, hi: int) -> int:
    """Milliseconds of `[lo, hi)` inside the union of `spans`."""
    total = 0
    reach = lo
    for start, stop in sorted(spans):
        start, stop = max(start, reach), min(stop, hi)
        if start < stop:
            total += stop - start
            reach = stop
    return total


def unpatched_ms(trace: Trace, samples: Samples) -> dict[int, int]:
    """Per side with a side tank, how long he was up with a burrower alive and no tank patch latched."""
    lives = []
    for guid in guids_of_entry(trace, NPC_BURROWER):
        stamps, _ = samples.track(guid)
        if stamps:
            lives.append((stamps[0], stamps[-1]))
    up = [(start, stop) for start, stop, phase, _ in phase_spans(trace) if phase in (1, 3)]
    _, holders = side_tanks(trace)
    out = {}
    for side, spans in enumerate(patch_latches(trace)):
        if side not in holders:
            continue
        bare = [(start, stop) for value, start, stop in spans if not as_guid(value)]
        out[side] = sum(covered(lives, max(start, up_start), min(stop, up_stop))
                        for start, stop in bare for up_start, up_stop in up)
    return out


def is_toc_anubarak(trace: Trace) -> bool:
    return (encounter_of(trace) == "anub-arak"
            and trace.header.get("map", map_of(trace.path)) == MAP_TOC)


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    difficulty = DIFFICULTIES.get(trace.header.get("diff"), "?")
    print(f"{encounter_of(trace)} {difficulty}  {outcome} at {clock(end)}"
          f"  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if not is_toc_anubarak(trace):
        print("  not a Trial of the Crusader Anub'arak pull, every section below reads empty")
        return

    gone = missing_probes(trace)
    print("all anub.* and toc.* probes present" if not gone
          else f"probes absent from this trace: {', '.join(gone)}")

    # with neither key written the absent line above already says so
    if notes(trace, "anub.side0") or notes(trace, "anub.side1"):
        needed, latched = side_tanks(trace)
        human = any(guid in trace.humans for holders in latched.values() for guid in holders)
        if len(latched) < needed or human:
            def label(guid: int) -> str:
                return trace.name(guid) + (" (human)" if guid in trace.humans else "")

            print(f"side tanks latched: {len(latched)} of {needed} needed ("
                  + ", ".join(f"side {side} "
                              + (" > ".join(label(guid) for guid in latched[side]) if side in latched else "none")
                              for side in (0, 1)) + ")")

    if notes(trace, "anub.patch0") or notes(trace, "anub.patch1"):
        bare = {side: ms for side, ms in unpatched_ms(trace, Samples(trace)).items() if ms >= UNPATCHED_FLAG_MS}
        if bare:
            print("no tank patch with burrowers out while he was up: "
                  + ", ".join(f"side {side} {ms / 1000:.1f} s" for side, ms in sorted(bare.items())))


# --------------------------------------------------------------------------------------------- phases

def phase_spans(trace: Trace) -> list[tuple[int, int, int, str]]:
    """`(start, stop, phase, label)` per stretch `anub.phase` held one value. A phase that comes round
    again gets a letter, P1b, P2b, so each span keeps its own deaths."""
    seen: collections.Counter = collections.Counter()
    out = []
    for value, start, stop in latch_spans(trace, "anub.phase", pull_end(trace)):
        try:
            phase = int(value)
        except ValueError:
            continue
        label = PHASE_LABELS.get(phase, value)
        seen[label] += 1
        if phase and seen[label] > 1:
            label += chr(ord("a") + seen[label] - 1)
        out.append((start, max(start, stop), phase, label))
    return out


def phase_at(spans, when: int) -> str:
    for start, stop, _, label in spans:
        if start <= when < stop:
            return label
    if spans and when >= spans[-1][1]:
        return spans[-1][3]
    return "pre"


def phase_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    spans = phase_spans(trace)
    dead: dict[str, list[dict]] = collections.defaultdict(list)
    for rec in combat_deaths(trace):
        dead[phase_at(spans, rec["t"])].append(rec)
    return [{"label": label, "phase": phase, "start": start, "stop": stop,
             "hp": health(samples, boss, start), "deaths": dead.get(label, [])}
            for start, stop, phase, label in spans]


def show_phases(trace: Trace) -> None:
    print("PHASES")
    samples = Samples(trace)
    rows = phase_rows(trace, samples)
    if not rows:
        print("  no anub.phase rows")
        return

    for row in rows:
        hp = "-" if row["hp"] is None else f"{row['hp']:5.1f}%"
        print(f"  {row['label']:5} {clock(row['start']):>9} .. {clock(row['stop']):>9}"
              f"  {(row['stop'] - row['start']) / 1000:6.1f} s  boss {hp:>6}  {len(row['deaths']):2} dead")
        for rec in row["deaths"]:
            blow = rec.get("blow") or [0, 0]
            print(f"          {clock(rec['t'])}  {trace.name(rec['g'])[:14]:14} {trace.role(rec['g']):6}"
                  f"  by {trace.name(rec.get('killer'))} ({blow[1] if len(blow) > 1 else 0})")

    submerges = sum(1 for row in rows if row["phase"] == 2)
    final = health(samples, boss_guid(trace), pull_end(trace))
    print(f"\n  {submerges} submerge(s)"
          + ("" if final is None else f", boss {final:.1f}% at {clock(pull_end(trace))}"))


# ----------------------------------------------------------------------------------------------- pull

def stage_nine(trace: Trace) -> tuple[int | None, bool]:
    """When `toc.progress` first read 9, and whether another stage came before it in this trace. The
    container restates its value when a trace opens, so a 9 with nothing before it may be older."""
    rows = sorted(notes(trace, "toc.progress"), key=lambda rec: rec["t"])
    for index, rec in enumerate(rows):
        if str(rec.get("txt", "")) == "9":
            return rec["t"], index > 0
    return None, False


def pull_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Roster members west of the Web Door or above the pit at the pull, and when each first stood in
    the room after it. Nobody sampled near the pull is listed too."""
    # the door and the pit heights only mean anything in this room
    if not is_toc_anubarak(trace):
        return []
    samples = samples or Samples(trace)
    out = []
    for guid in sorted(roster_guids(trace), key=trace.name):
        row = samples.row(guid, 0)
        if row is None:
            out.append({"guid": guid, "why": "unsampled", "spot": None, "arrived": None})
            continue
        locked, high = row[1] < WEB_DOOR_X, row[3] > LANDED_Z
        if not (locked or high):
            continue
        stamps, rows = samples.track(guid)
        start = bisect.bisect_left(stamps, 0)
        arrived = next((stamps[i] for i in range(start, len(stamps))
                        if rows[i][1] >= WEB_DOOR_X and rows[i][3] <= LANDED_Z), None)
        out.append({"guid": guid, "why": "locked out" if locked else "not landed",
                    "spot": tuple(row[1:4]), "arrived": arrived})
    return out


def engage_spread(trace: Trace, samples: Samples | None = None) -> dict | None:
    """At the engage, the living roster player nearest his wait point and each bot tank, 2D yards from it,
    off the snapshot at or before it."""
    pulls = trace.of("pull")
    if not pulls:
        return None
    samples = samples or Samples(trace)
    when = pulls[0]["t"]
    yards = {}
    for guid in roster_guids(trace):
        row = samples.row(guid, when)
        if row is not None and row[5] > 0:
            yards[guid] = dist2(row[1:3], ANUB_WAIT)
    if not yards:
        return None
    nearest = min(yards, key=yards.get)
    tanks = sorted(((guid, gap) for guid, gap in yards.items()
                    if trace.role(guid) == "tank" and guid not in trace.humans), key=lambda pair: pair[1])
    return {"t": when, "nearest": nearest, "yards": yards[nearest], "tanks": tanks}


def show_pull(trace: Trace) -> None:
    print("PULL")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return

    when, edge = stage_nine(trace)
    if when is None:
        print("  no toc.progress 9 in this trace")
    elif edge:
        print(f"  stage 9 at {clock(when)}, {-when / 1000:.1f} s before the pull")
    else:
        print(f"  toc.progress read 9 on its first row at {clock(when)}: a restate, the floor broke"
              " before the trace opened")

    spread = engage_spread(trace)
    if spread is not None:
        nearest = spread["nearest"]
        tanks = ", ".join(f"{trace.name(guid)} {gap:.1f}" for guid, gap in spread["tanks"]) or "none"
        inside = [trace.name(guid) for guid, gap in spread["tanks"] if gap < AGGRO_YD]
        print(f"  at the engage {clock(spread['t'])}: nearest his wait point {trace.name(nearest)}"
              f" ({'human' if nearest in trace.humans else 'bot'} {trace.role(nearest)}) {spread['yards']:.1f} yd;"
              f" bot tanks {tanks} yd"
              + (f"; inside his {AGGRO_YD:.0f} yd aggro: {', '.join(inside)}" if inside else ""))

    rows = pull_rows(trace)
    if not rows:
        print("  everyone stood in the pit at the pull")
        return
    print(f"  west of x {WEB_DOOR_X} or above z {LANDED_Z:.0f} at the pull:")
    for row in rows:
        spot = "-" if row["spot"] is None else ", ".join(f"{value:.1f}" for value in row["spot"])
        came = "never in the room" if row["arrived"] is None else f"in at {clock(row['arrived'])}"
        print(f"    {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['why']:10}"
              f" {spot:>22}  {came if row['spot'] else ''}")


# ---------------------------------------------------------------------------------------------- spike

def mark_spans(trace: Trace) -> list[tuple[int, int, int, bool]]:
    """`(guid, start, stop, closed)` per Pursued by Anub'arak window on the roster, in time order."""
    roster = roster_guids(trace)
    return sorted(((guid, start, stop, closed)
                   for guid, spans in aura_spans(trace, (SPELL_MARK,)).items() if guid in roster
                   for start, stop, closed in spans), key=lambda span: span[1])


def patch_lives(trace: Trace, samples: Samples) -> dict[int, tuple[int, int]]:
    """Per sphere that ever sat as a patch, `(landed, last seen)`."""
    out = {}
    for guid in guids_of_entry(trace, NPC_FROST_SPHERE):
        stamps, rows = samples.track(guid)
        low = [when for when, row in zip(stamps, rows) if len(row) > 3 and row[3] < SPHERE_PATCH_Z]
        if low:
            out[guid] = (low[0], stamps[-1])
    return out


def consumed_patches(trace: Trace, samples: Samples) -> dict[int, tuple[int, int]]:
    """Patch guid to `(last seen, mark end)` for every patch gone within CONSUMED_MS of a mark ending.
    One still sampled in the last snapshot never went."""
    last = samples.stamps[-1] if samples.stamps else 0
    stops = [(stop, guid) for guid, _, stop, closed in mark_spans(trace) if closed]
    out = {}
    for patch, (_, seen) in patch_lives(trace, samples).items():
        if seen >= last:
            continue
        near = [stop for stop, _ in stops if abs(seen - stop) <= CONSUMED_MS]
        if near:
            out[patch] = (seen, min(near, key=lambda stop: abs(seen - stop)))
    return out


def branches_in(rows: list[tuple[int, str]], start: int, stop: int) -> list[str]:
    """The branch held when a window opened, then every change inside it. A carried `none` is the
    close of an earlier window, and a row at `stop` is this one's close."""
    held = [value for when, value in rows if when <= start]
    out = [value for value in held[-1:] if value != "none"]
    for when, value in rows:
        if start < when < stop and (not out or out[-1] != value):
            out.append(value)
    return out


def noted_in(rows: list[tuple[int, str]], start: int, stop: int) -> list[tuple[int, str]]:
    """A kite's rows of a per-bot key written at each pick, `(when, value)` in turn. The pick is made at the
    mark, so the value carried in from an earlier mark, stamped `start`, counts only when no row lands within
    KITEPLAN_SETTLE_MS of this one: the key wrote nothing because the pick read the same."""
    fresh = [(when, value) for when, value in rows if start - KITEPLAN_SETTLE_MS <= when < stop]
    out: list[tuple[int, str]] = []
    if not fresh or fresh[0][0] > start + KITEPLAN_SETTLE_MS:
        out = [(start, value) for when, value in rows if when < start - KITEPLAN_SETTLE_MS][-1:]
    for when, value in fresh:
        if not out or out[-1][1] != value:
            out.append((when, value))
    return out


def plan_terminals(rows: list[tuple[int, str]], start: int, stop: int) -> list[str]:
    """A kite's `anub.kiteplan` terminals in turn."""
    return [value for _, value in noted_in(rows, start, stop)]


def spike_near(samples: Samples, spikes, guid: int, start: int, stop: int) -> int | None:
    """Milliseconds into the window the spike first came within KITE_PROTECT_RANGE of the kiter."""
    stamps, rows = samples.track(guid)
    for index in range(bisect.bisect_left(stamps, start), bisect.bisect_right(stamps, stop)):
        for spike in spikes:
            row = samples.recent(spike, stamps[index])
            if row and dist2(row[1:3], rows[index][1:3]) <= KITE_PROTECT_RANGE:
                return stamps[index] - start
    return None


def tank_patch_approach(samples: Samples, latches, spikes, start: int, stop: int) -> dict[int, float]:
    """Per side, the spike's closest approach in the window to the tank patch that side had latched at each
    sample. A spent patch stops being sampled, so its last read is the spike on top of it."""
    out: dict[int, float] = {}
    for spike in spikes:
        stamps, rows = samples.track(spike)
        for index in range(bisect.bisect_left(stamps, start), bisect.bisect_right(stamps, stop)):
            for side, spans in enumerate(latches):
                patch = latched_patch(spans, samples, stamps[index])
                if patch:
                    yards = dist2(rows[index][1:3], patch)
                    out[side] = min(out.get(side, yards), yards)
    return out


def stage_time(spans, start: int, stop: int) -> dict[str, int]:
    """Milliseconds per `anub.kitestage` value inside the window, `none` left out, in the order first held."""
    out: dict[str, int] = {}
    for value, low, high in spans:
        overlap = min(high, stop) - max(low, start)
        if value != "none" and overlap > 0:
            out[value] = out.get(value, 0) + overlap
    return out


def stage_name(trace: Trace, value: str) -> str:
    kind, _, spell = value.partition(":")
    if kind == "boost" and spell.isdigit():
        return f"boost {KITE_BOOSTS.get(int(spell)) or trace.spell(int(spell))}"
    return value


def boost_rows(trace: Trace) -> list[dict]:
    """Kite boost casts, triggered ones too, and Nitro Boosts' speed aura going on, as `{t, s, sp}`."""
    rows = [rec for rec in trace.of("cast") if rec.get("sp") in KITE_BOOSTS]
    rows += [{"t": rec["t"], "s": rec.get("d"), "sp": NITRO_SPEED} for rec in trace.of("aura")
             if rec.get("sp") == NITRO_SPEED and not rec.get("r")]
    return sorted(rows, key=lambda rec: rec["t"])


def kiter_boosts(rows: list[dict], guid: int, start: int, stop: int) -> list[dict]:
    """The kiter's boosts in the window, a boost read again within BOOST_PAIR_MS kept once."""
    out: list[dict] = []
    for rec in rows:
        if rec.get("s") != guid or not start <= rec["t"] <= stop:
            continue
        if out and KITE_BOOSTS[out[-1]["sp"]] == KITE_BOOSTS[rec["sp"]] and rec["t"] - out[-1]["t"] <= BOOST_PAIR_MS:
            continue
        out.append(rec)
    return out


def submerged_spans(trace: Trace) -> list[tuple[int, int]]:
    return [(start, stop) for start, stop, phase, _ in phase_spans(trace) if phase == 2]


def gather_centre(holds, samples: Samples, submerged, when: int, skip=frozenset()) -> tuple[float, float] | None:
    """The mean spot of the living bots whose `anub.gather` read `hold` at `when` while he was under, `skip`
    left out, None with nobody holding. The note never clears, so a hold that began before this submerge
    is stale."""
    under = next((start for start, stop in submerged if start <= when <= stop), None)
    if under is None:
        return None
    points = []
    for guid, spans in holds.items():
        began = next((start for value, start, stop in spans if value == "hold" and start <= when < stop), None)
        if guid in skip or began is None or began < under:
            continue
        row = samples.recent(guid, when)
        if row and row[5] > 0:
            points.append(row[1:3])
    if not points:
        return None
    return statistics.fmean(point[0] for point in points), statistics.fmean(point[1] for point in points)


def spike_at(samples: Samples, spikes, kiter: int, when: int):
    """The spike's last row at `when`, the one chasing `kiter` first should two be sampled."""
    rows = [row for row in (samples.recent(spike, when) for spike in spikes) if row]
    rows.sort(key=lambda row: len(row) <= 7 or row[7] != kiter)
    return rows[0] if rows else None


def unlatched_patches(samples: Samples, latches, when: int) -> list[int]:
    """Patches alive at `when` that no side held then."""
    held = {as_guid(held_at(spans, when)) for spans in latches}
    return sorted(guid for guid, kind in samples.sphere_kinds(when).items() if kind == "patch" and guid not in held)


def unit_speed(samples: Samples, guid: int, low: int, high: int) -> float | None:
    """The median yards per second over the unit's snapshot steps inside `[low, high]`, holes and steps
    under MOVING_YD_S left out; 0 for one sampled there that never moved, None for one never sampled."""
    stamps, rows = samples.track(guid)
    first, last = bisect.bisect_left(stamps, low), bisect.bisect_right(stamps, high)
    if first >= last:
        return None
    speeds = []
    for index in range(first + 1, last):
        span = stamps[index] - stamps[index - 1]
        if 0 < span <= MAX_STEP_MS:
            speed = dist2(rows[index][1:3], rows[index - 1][1:3]) * 1000.0 / span
            if speed >= MOVING_YD_S:
                speeds.append(speed)
    return statistics.median(speeds) if speeds else 0.0


def cover_speeds(samples: Samples, spikes, window: dict) -> list[dict]:
    """Each Hand of Protection on the kiter and each own immunity in the window, with the spike's speed over
    the second after it and the kiter's around it, else over the whole window. `slow` is a spike no faster
    than the kiter, so the cover went on while the kiter still outran it; None when either went unread."""
    guid, start, stop = window["guid"], window["start"], window["stop"]
    out = []
    for rec in sorted(window["hop"] + window["own"], key=lambda rec: rec["t"]):
        when = rec["t"]
        spike = spike_at(samples, spikes, guid, when) or spike_at(samples, spikes, guid, when + NEAR_MS)
        spike_speed = unit_speed(samples, spike[0], when - 500, when + 1000) if spike else None
        kiter_speed = unit_speed(samples, guid, when - 1000, when + 2000) or unit_speed(samples, guid, start, stop)
        slow = None
        if spike_speed is not None and kiter_speed:
            slow = spike_speed <= kiter_speed + SPEED_SLACK
        out.append({"rec": rec, "spike": spike_speed, "kiter": kiter_speed, "slow": slow})
    return out


def closing_spans(samples: Samples, spikes, window: dict) -> list[dict]:
    """Each stretch of CLOSING_MS or more the kiter's gap to its spike shrank at every snapshot step while the spike
    ran no faster than the kiter's moving speed in the second either side, ending within KITE_PROTECT_RANGE.
    `speeds` are the spike's speed steps over it in turn, `kiter` the kiter's moving speed over it."""
    guid, start, stop = window["guid"], window["start"], window["stop"]
    out: list[dict] = []
    run: list[tuple[int, float, float | None]] = []

    def flush() -> None:
        if len(run) > 1 and run[-1][0] - run[0][0] >= CLOSING_MS and run[-1][1] <= KITE_PROTECT_RANGE:
            steps = [min(SPIKE_SPEEDS, key=lambda speed: abs(speed - step)) for _, _, step in run[1:]]
            out.append({"start": run[0][0], "stop": run[-1][0], "from": run[0][1], "to": run[-1][1],
                        "speeds": list(dict.fromkeys(steps)),
                        "kiter": unit_speed(samples, guid, run[0][0], run[-1][0])})
        run.clear()

    last = None
    for index in range(bisect.bisect_left(samples.stamps, start), bisect.bisect_right(samples.stamps, stop)):
        when = samples.stamps[index]
        kiter = sampled_at(samples, guid, index)
        rows = [row for row in (sampled_at(samples, spike, index) for spike in spikes) if row]
        rows.sort(key=lambda row: len(row) <= 7 or row[7] != guid)
        spike = rows[0] if rows and kiter else None
        gap = dist2(kiter[1:3], spike[1:3]) if spike else None
        step = pace = None
        if spike and last and last[3] == spike[0] and 0 < when - last[0] <= MAX_STEP_MS:
            step = dist2(spike[1:3], last[2]) * 1000.0 / (when - last[0])
            pace = unit_speed(samples, guid, when - 1000, when + 1000)
        if step is not None and pace and gap < last[1] and step <= pace + SPEED_SLACK:
            if not run:
                run.append((last[0], last[1], None))
            run.append((when, gap, step))
        else:
            flush()
        last = (when, gap, spike[1:3], spike[0]) if spike else None
    flush()
    return out


def merged(spans: list[tuple[int, int]]) -> list[tuple[int, int]]:
    out: list[tuple[int, int]] = []
    for start, stop in sorted(spans):
        if out and start <= out[-1][1]:
            out[-1] = (out[-1][0], max(out[-1][1], stop))
        else:
            out.append((start, stop))
    return out


def stalled(samples: Samples, guid: int, start: int, stop: int) -> tuple[int, int]:
    """`(ms, longest)` of the window the kiter stood still: every STALL_MS stretch inside it, alive at both
    ends, over which it moved under STALL_YD, overlaps counted once."""
    stamps, rows = samples.track(guid)
    spans = []
    for index in range(bisect.bisect_left(stamps, start + STALL_MS), bisect.bisect_right(stamps, stop)):
        back = bisect.bisect_right(stamps, stamps[index] - STALL_MS) - 1
        if back < 0 or stamps[back] < start or stamps[index] - stamps[back] > STALL_MS + MAX_STEP_MS:
            continue
        if rows[index][5] > 0 and rows[back][5] > 0 and dist2(rows[index][1:3], rows[back][1:3]) < STALL_YD:
            spans.append((stamps[back], stamps[index]))
    runs = merged(spans)
    return covered(runs, start, stop), max((high - low for low, high in runs), default=0)


def flip_flops(moves: list[dict], start: int, stop: int) -> list[tuple[int, int, int]]:
    """`(first, last, reversals)` per stretch of the window the kiter's walk kept reversing. A jump is a move
    row FLIP_YD or more off the one before it, a reversal a jump heading back against the jump before it, and
    a stretch FLIP_COUNT or more reversals inside FLIP_MS, overlaps merged."""
    reversals = []
    last = jump = None
    for rec in moves:
        if not start <= rec["t"] <= stop:
            continue
        point = (rec["x"], rec["y"])
        if last is not None:
            step = (point[0] - last[0], point[1] - last[1])
            if math.hypot(*step) >= FLIP_YD:
                if jump is not None and step[0] * jump[0] + step[1] * jump[1] < 0:
                    reversals.append(rec["t"])
                jump = step
        last = point
    spans = [(reversals[index], reversals[index + FLIP_COUNT - 1])
             for index in range(len(reversals) - FLIP_COUNT + 1)
             if reversals[index + FLIP_COUNT - 1] - reversals[index] <= FLIP_MS]
    return [(low, high, sum(1 for when in reversals if low <= when <= high)) for low, high in merged(spans)]


def hand_reads(trace: Trace) -> list[tuple[int, int, int, int]]:
    """`(t, paladin, cooldown left ms, yards)` for each paladin in each `anub.hands` row."""
    out = []
    for rec in notes(trace, "anub.hands"):
        for part in str(rec.get("txt", "")).split():
            fields = part.split(":")
            if len(fields) == 3 and all(field.lstrip("-").isdigit() for field in fields):
                out.append((rec["t"], int(fields[0]), int(fields[1]) * 1000, int(fields[2])))
    return out


def hop_cooldown(trace: Trace, casts: dict[int, list[int]] | None = None) -> tuple[int, str]:
    """Hand of Protection's cooldown and where it came from: the median over the `anub.hands` reads of a
    cooldown still running of the time since that paladin's last Hand plus what was left, to the second;
    else HOP_COOLDOWN_MS, `default`."""
    if casts is None:
        casts = collections.defaultdict(list)
        for rec in trace.of("cast"):
            if rec.get("sp") in HAND_OF_PROTECTION and not rec.get("tr"):
                casts[rec.get("s")].append(rec["t"])
    spans = []
    for when, paladin, left, _ in hand_reads(trace):
        last = [cast for cast in casts.get(paladin, []) if cast <= when]
        if left > 0 and last:
            spans.append(when - last[-1] + left)
    if not spans:
        return HOP_COOLDOWN_MS, "default"
    return int(round(statistics.median(spans) / 1000.0)) * 1000, "anub.hands"


def hop_readiness(trace: Trace) -> dict:
    """What `ready_paladin` reads: the bot paladins on the roster, each one's Hand of Protection casts, the
    marks, Forbearance and the cooldown."""
    paladins = {member["g"] for member in trace.header.get("roster", [])
                if member.get("c") == CLASS_PALADIN and not member.get("h")}
    casts: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("sp") in HAND_OF_PROTECTION and not rec.get("tr") and rec.get("s") in paladins:
            casts[rec["s"]].append(rec["t"])
    cooldown, source = hop_cooldown(trace, casts)
    return {"paladins": paladins, "casts": casts, "marks": mark_spans(trace),
            "forbearance": aura_spans(trace, (SPELL_FORBEARANCE,)), "cooldown": cooldown, "cooldown_by": source}


def ready_paladin(samples: Samples, ready: dict, kiter: int, when: int) -> dict:
    """The nearest bot paladin that could Hand of Protection the kiter at `when`: alive, unmarked, its last one
    the cooldown or more back. `guid` is None with none, and `forbearance` says the kiter carried it, which
    rules out every paladin."""
    forbearance = any(low <= when < high for low, high, _ in ready["forbearance"].get(kiter, []))
    out = {"t": when, "guid": None, "yards": None, "forbearance": forbearance}
    spot = samples.recent(kiter, when)
    if spot is None or forbearance:
        return out
    for guid in sorted(ready["paladins"] - {kiter}):
        row = samples.recent(guid, when)
        if row is None or row[5] <= 0:
            continue
        if any(marked == guid and low <= when < high for marked, low, high, _ in ready["marks"]):
            continue
        if any(when - ready.get("cooldown", HOP_COOLDOWN_MS) < cast < when for cast in ready["casts"].get(guid, [])):
            continue
        gap = dist2(row[1:3], spot[1:3])
        if out["yards"] is None or gap < out["yards"]:
            out.update(guid=guid, yards=gap)
    return out


def spike_restart(marks, submerged, when: int) -> int | None:
    """When the spike chasing at `when` last started from its base speed: the submerge's first mark, or a mark
    SPIKE_RESTART_GAP_MS or more after the one before it. None while he is up."""
    under = next((start for start, stop in submerged if start <= when <= stop), None)
    if under is None:
        return None
    restart = last = None
    for _, start, stop, _ in marks:
        if not under <= start <= when:
            continue
        if last is None or start - last >= SPIKE_RESTART_GAP_MS:
            restart = start
        last = stop if last is None else max(last, stop)
    return restart


def ring_turn(kiter, spike) -> float:
    """The stem's RingTurn: round the room away from the spike's bearing, 1 counter-clockwise."""
    opening = math.remainder(math.atan2(kiter[1] - ROOM_CENTER[1], kiter[0] - ROOM_CENTER[0])
                             - math.atan2(spike[1] - ROOM_CENTER[1], spike[0] - ROOM_CENTER[0]), 2.0 * math.pi)
    return 1.0 if opening >= 0.0 else -1.0


def kiter_path(start, heading, speed: float, fallback: float):
    """AnubarakPursuit's KiterPath as a function of seconds: straight along the unit `heading` until
    KITE_RING_RADIUS off the room centre, then round that circle, `fallback` the way round when it meets the
    circle square on."""
    px, py = start[0] - ROOM_CENTER[0], start[1] - ROOM_CENTER[1]
    ring = max(KITE_RING_RADIUS, math.hypot(px, py))
    along = px * heading[0] + py * heading[1]
    straight = max(0.0, -along + math.sqrt(max(0.0, along * along - (px * px + py * py) + ring * ring)))
    hx, hy = px + heading[0] * straight, py + heading[1] * straight
    cross = hx * heading[1] - hy * heading[0]
    way = (1.0 if cross > 0.0 else -1.0) if abs(cross) > 0.1 * ring else fallback

    def at(seconds: float) -> tuple[float, float]:
        run = speed * seconds
        if run <= straight:
            return start[0] + heading[0] * run, start[1] + heading[1] * run
        angle = math.atan2(hy, hx) + way * (run - straight) / ring
        return ROOM_CENTER[0] + ring * math.cos(angle), ROOM_CENTER[1] + ring * math.sin(angle)

    return at


def predicted_catch(spike, elapsed_s: float, kiter, heading, speed: float) -> float | None:
    """Seconds until the spike comes within IMPALE_REACH of the kiter, as the stem's ImpaleInMs: pure pursuit
    whose speed steps every SPIKE_STEP_S from its restart, `elapsed_s` ago, after a kiter running `heading` at
    `speed` and then round the ring. None past CATCH_HORIZON_S."""
    if dist2(spike, kiter) <= IMPALE_REACH:
        return 0.0
    path = kiter_path(kiter, heading, speed, ring_turn(kiter, spike))
    step = min(len(SPIKE_SPEEDS) - 1, int(elapsed_s // SPIKE_STEP_S))
    due = SPIKE_STEP_S * (step + 1) - elapsed_s
    for tick in range(math.ceil(CATCH_HORIZON_S / CATCH_TICK_S - 1e-3)):
        seconds = tick * CATCH_TICK_S
        while step < len(SPIKE_SPEEDS) - 1 and seconds >= due:
            step += 1
            due += SPIKE_STEP_S
        gap = dist2(spike, kiter)
        if gap > 1e-3:
            share = min(SPIKE_SPEEDS[step] * CATCH_TICK_S, gap) / gap
            spike = (spike[0] + (kiter[0] - spike[0]) * share, spike[1] + (kiter[1] - spike[1]) * share)
        kiter = path(seconds + CATCH_TICK_S)
        if dist2(spike, kiter) <= IMPALE_REACH:
            return seconds + CATCH_TICK_S
    return None


def run_heading(samples: Samples, guid: int, spike, when: int) -> tuple[float, float] | None:
    """The unit heading the kiter ran on into `when`, off its last snapshot step, else away from the spike as the
    stem's RunDir takes it for a kiter standing. None for one unsampled."""
    stamps, rows = samples.track(guid)
    index = bisect.bisect_right(stamps, when) - 1
    if index < 0:
        return None
    here = rows[index][1:3]
    span = stamps[index] - stamps[index - 1] if index else 0
    step = (here[0] - rows[index - 1][1], here[1] - rows[index - 1][2]) if index else (0.0, 0.0)
    if 0 < span <= MAX_STEP_MS and math.hypot(*step) * 1000.0 / span >= MOVING_YD_S:
        length = math.hypot(*step)
    else:
        step = (here[0] - spike[0], here[1] - spike[1])
        length = math.hypot(*step)
        if length < 0.1:
            return None
    return step[0] / length, step[1] / length


def patch_spots(samples: Samples, when: int) -> dict[int, tuple[float, float]]:
    """Each patch in the snapshot at or before `when` and where it sits."""
    index = samples.index_at(when)
    if index < 0:
        return {}
    kinds = samples.sphere_kinds(when)
    return {row[0]: (row[1], row[2]) for row in samples.sphere_rows[index] if kinds.get(row[0]) == "patch"}


def patch_side(latches, guid: int, when: int) -> int | None:
    """The side whose `anub.patchN` held the patch at `when`, None for one no side held."""
    return next((side for side, spans in enumerate(latches) if as_guid(held_at(spans, when)) == guid), None)


def kite_turn(bridge: list[tuple[int, str]], stages, start: int, stop: int,
              walks=("patch",)) -> tuple[int | None, str | None]:
    """When a patch kite turned for its patch and what says so: its first `anub.bridge` ask or turn in the
    window; else, after a hand row, the first patch kitestage from it on, since the kite runs on under the
    Hand; else the first of its `walks` kitestages there. None for one that never turned."""
    rows = [(when, value) for when, value in bridge if start <= when <= stop]
    when = next((when for when, value in rows if value in BRIDGE_TURNS), None)
    if when is not None:
        return when, "anub.bridge"
    landed = next((when for when, value in rows if value == "hand"), None)
    if landed is not None:
        start, walks = landed, ("patch",)
    when = min((max(low, start) for value, low, high in stages if value in walks and low < stop and high > start),
               default=None)
    return (when, "kitestage") if when is not None else (None, None)


def moves_from(rows: list[tuple[int, str]], turn: int, stop: int) -> list[str]:
    """The kiter's `anub.kitemove` at the turn, carried in when no row lands there, then each change before the
    mark's end, where the reset to `none` lands."""
    out = [value for when, value in rows if when <= turn + NOTE_TICK_MS][-1:]
    for when, value in rows:
        if turn + NOTE_TICK_MS < when < stop and (not out or out[-1] != value):
            out.append(value)
    return out


def turned_for(samples: Samples, spikes, latches, taken, guid: int, turn: int, stop: int) -> dict | None:
    """The patch a kite turned for: the one the spike failed on as the mark ended when it was alive at the turn,
    else the one alive then the kiter came nearest after it. Where it sat, the side holding it at the turn, the
    kiter's and the spike's closest approach to it from the turn to the mark's end, and `failed`, the spike
    failing on it. None with no patch alive at the turn."""
    spots = patch_spots(samples, turn)
    stamps, rows = samples.track(guid)
    first, last = bisect.bisect_left(stamps, turn), bisect.bisect_right(stamps, stop)
    if not spots or first >= last:
        return None
    ended = next((patch for patch, (_, end) in taken.items() if end == stop and patch in spots), None)
    nearest = {patch: min(dist2(row[1:3], spot) for row in rows[first:last]) for patch, spot in spots.items()}
    patch = ended or min(nearest, key=nearest.get)
    chase = [row for row in (spike_at(samples, spikes, guid, when) for when in stamps[first:last]) if row]
    return {"guid": patch, "spot": spots[patch], "side": patch_side(latches, patch, turn), "kiter": nearest[patch],
            "spike": min((dist2(row[1:3], spots[patch]) for row in chase), default=None), "failed": ended is not None}


def first_patch_pass(samples: Samples, spikes, latches, kiter: int, start: int, stop: int) -> dict | None:
    """The first snapshot in `[start, stop]` with the spike chasing the kiter within SPIKE_ROUTE_CLEARANCE of a
    patch: when, the nearest such patch, where it sat, how far, and the side holding it then. A patch the spike
    was already inside at its first sample counts only once it got out, since the run never led it there."""
    spawned = None
    for when in samples.stamps[bisect.bisect_left(samples.stamps, start):bisect.bisect_right(samples.stamps, stop)]:
        spike = spike_at(samples, spikes, kiter, when)
        if spike is None:
            continue
        inside = sorted((dist2(spike[1:3], spot), guid, spot) for guid, spot in patch_spots(samples, when).items()
                        if dist2(spike[1:3], spot) <= SPIKE_ROUTE_CLEARANCE)
        if spawned is None:
            spawned = {guid for _, guid, _ in inside}
            continue
        spawned &= {guid for _, guid, _ in inside}
        passed = [row for row in inside if row[1] not in spawned]
        if passed:
            yards, guid, spot = passed[0]
            return {"t": when, "guid": guid, "spot": spot, "yards": yards, "side": patch_side(latches, guid, when)}
    return None


def bridge_verdict(window: dict) -> str | None:
    """How a patch+hand kite's turn went, a BRIDGE_TEXT key: from its `anub.bridge` rows, a hand row with no
    ask before it being the Hand at the catch, else from a Hand of Protection's cast against the turn. None for
    a kite that never planned one."""
    if "patch+hand" not in window["plan"]:
        return None
    for value in ("hand", "turn", "ask"):
        if value in window["bridge"]:
            return "catch" if value == "hand" and "ask" not in window["bridge"] else value
    if window["turn"] is None:
        return "never"
    gaps = [rec["t"] - window["turn"] for rec in window["hop"]]
    if any(abs(gap) <= HAND_AT_TURN_MS for gap in gaps):
        return "hand"
    if not gaps:
        return "turn"
    return "early" if gaps[0] < 0 else "late"


def hand_catches(samples: Samples, spikes, window: dict, marks, submerged, hands) -> list[dict]:
    """Each Hand of Protection on a kiter against the catch predicted as it went out: the seconds to that catch,
    None past the horizon, the spike's distance and speed then and the kiter's, the Hand's end off its aura on
    the kiter, else HOP_DURATION_MS on, and `under`, the spike failing on a patch as the mark ended before it."""
    guid = window["guid"]
    out = []
    for row in window["cover"]:
        rec = row["rec"]
        if rec.get("sp") not in HAND_OF_PROTECTION:
            continue
        when = rec["t"]
        spike, unit = spike_at(samples, spikes, guid, when), samples.recent(guid, when)
        restart = spike_restart(marks, submerged, when)
        speed = row["kiter"] or BASE_RUN_YD_S
        heading = run_heading(samples, guid, spike[1:3], when) if spike and unit else None
        before = None
        if heading is not None:
            before = predicted_catch(spike[1:3], (when - (restart or when)) / 1000.0, unit[1:3], heading, speed)
        end = next((high for low, high, _ in hands.get(guid, []) if abs(low - when) <= NEAR_MS),
                   when + HOP_DURATION_MS)
        out.append({"rec": rec, "before": before, "yards": dist2(spike[1:3], unit[1:3]) if spike and unit else None,
                    "spike": row["spike"], "kiter": row["kiter"], "end": end,
                    "under": window["consumed"] and when <= window["stop"] <= end + IMPALE_SLACK_MS})
    return out


def parse_kitepred(text) -> tuple[str, float] | None:
    """An `anub.kitepred` value as `(what, seconds from its pick)`."""
    what, _, seconds = str(text).rpartition(":")
    try:
        return what, float(seconds)
    except ValueError:
        return None


def kitepath_heading(text) -> float | None:
    """The heading in degrees an `anub.kitepath` pick runs on first, None for a human's `motion`."""
    shape, _, rest = str(text).partition(":")
    if shape not in ("ring", "arc"):
        return None
    try:
        return float(rest.split(">")[0].split(":")[0])
    except ValueError:
        return None


def kitepath_patch(text) -> int | None:
    """The guid counter of the patch an `anub.kitepath` pick ends on, None for none."""
    counter = str(text).partition(">patch:")[2].split("@")[0]
    return int(counter) if counter.isdigit() else None


def kitepath_changes(rows: list[tuple[int, str]], start: int, stop: int) -> list[dict]:
    """A kite's `anub.kitepath` picks in turn: `at` its row, `held` from the mark or its row to the next pick or
    the mark's end, and `reversal`, its heading over PICK_REVERSAL_DEGREES off the pick before it, that pick
    under PICK_REVERSAL_MS old."""
    picks = noted_in(rows, start, stop)
    out: list[dict] = []
    for index, (when, value) in enumerate(picks):
        until = picks[index + 1][0] if index + 1 < len(picks) else stop
        reversal = False
        if out:
            before, here = kitepath_heading(out[-1]["value"]), kitepath_heading(value)
            reversal = (before is not None and here is not None and when - out[-1]["at"] < PICK_REVERSAL_MS
                        and abs(math.remainder(here - before, 360.0)) > PICK_REVERSAL_DEGREES)
        out.append({"at": when, "value": value, "held": until - max(when, start), "reversal": reversal})
    return out


def predicted_hazard(picks: list[tuple[int, str]]) -> tuple[str, int] | None:
    """The first `anub.kitepred` pick of a kite that foresaw a hazard: what, and when it foresaw it landing."""
    for when, value in picks:
        parsed = parse_kitepred(value)
        if parsed and parsed[0] in KITEPRED_HAZARDS:
            return parsed[0], when + int(round(parsed[1] * 1000.0))
    return None


def formed_gather(holds, samples: Samples, submerged, when: int, kiter: int) -> tuple[float, float] | None:
    """The gather as formed GATHER_FORMED_MS into the submerge, or at `when` once past that: bots still walking to
    it at an early mark put its mean on the spike."""
    under = next(((low, high) for low, high in submerged if low <= when <= high), None)
    if under is None:
        return None
    return gather_centre(holds, samples, submerged, min(max(when, under[0] + GATHER_FORMED_MS), under[1]), {kiter})


def real_hazard(samples: Samples, spikes, latches, window: dict, gather, marks, submerged,
                rules: dict) -> tuple[str, int] | None:
    """The real kite's first hazard as the kite planner counts them: the spike within its fail reach of a tank
    patch, of a kite patch other than the one the `anub.kitepath` pick in force ends on or of that one under
    earlyPatchS since the spike's restart, nearer the gather than its clearance, or nearer than its start less
    the slack when it began nearer, or Impale or a death on the kiter. A patch the spike was inside at its first
    sample counts once it got out. `(what, when)`, None with none."""
    guid, start, stop = window["guid"], window["start"], window["stop"]
    caught = min((when for when in (window["impale_at"], window["died"]) if when is not None), default=None)
    paths = window["kitepath"]
    need = spawned = None
    for when in samples.stamps[bisect.bisect_left(samples.stamps, start):bisect.bisect_right(samples.stamps, stop)]:
        if caught is not None and when >= caught:
            break
        spike = spike_at(samples, spikes, guid, when)
        if spike is None:
            continue
        near = {patch for patch, spot in patch_spots(samples, when).items()
                if dist2(spike[1:3], spot) <= rules["patchReach"]}
        if spawned is None:
            spawned = set(near)
            if gather is not None:
                need = min(rules["gatherClearance"], dist2(spike[1:3], gather) - rules["startSlack"])
        spawned &= near
        planned = next((kitepath_patch(pick["value"]) for pick in reversed(paths) if pick["at"] <= when), None)
        restart = spike_restart(marks, submerged, when)
        early = restart is None or when - restart < rules["earlyPatchS"] * 1000.0
        for patch in sorted(near - spawned):
            if patch_side(latches, patch, when) is not None:
                return "tank", when
            if planned != patch & 0xFFFFFFFF or early:
                return "kite", when
        if need is not None and dist2(spike[1:3], gather) < need:
            return "raid", when
    return None if caught is None else ("catch", caught)


def alternations(moves: list[dict], start: int, stop: int) -> int:
    """How often the paladin's walk to its escort spot and its dodge swapped within ALTERNATE_MS."""
    rows = [rec for rec in moves if start <= rec["t"] <= stop and rec.get("by") in (ESCORT_BY, DODGE_BY)]
    return sum(1 for one, two in zip(rows, rows[1:])
               if one.get("by") != two.get("by") and two["t"] - one["t"] <= ALTERNATE_MS)


def escort_yards(samples: Samples, guid: int, kiter: int, when: int) -> float | None:
    mine, theirs = samples.recent(guid, when), samples.recent(kiter, when)
    return dist2(mine[1:3], theirs[1:3]) if mine and theirs else None


def frost_seconds(spans: list[tuple[int, int, bool]], start: int, stop: int) -> float:
    return sum(max(0, min(high, stop) - max(low, start)) for low, high, _ in spans) / 1000.0


def escort_read(samples: Samples, escorts: dict[int, list[tuple[int, str]]], window: dict,
                moves: dict[int, list[dict]], frost: dict[int, list[tuple[int, int, bool]]]) -> dict | None:
    """The escort in a window, the paladin whose `anub.escort` reads anything but none there, most rows first:
    its distance to the kiter at the claim, the first `anub.protect` hop row in the window or, with none, as
    the spike came within KITE_PROTECT_RANGE; at the kiter's first boost before any Hand on it, and at the
    window's end; the seconds it carried Permafrost; and its walk and dodge swaps. None with no escort."""
    start, stop = window["start"], window["stop"]
    counts = {guid: sum(1 for when, value in rows if start <= when <= stop and value != "none")
              for guid, rows in escorts.items()}
    counts = {guid: n for guid, n in counts.items() if n}
    if not counts:
        return None
    guid = max(sorted(counts), key=counts.get)
    claim = next((rec["t"] for rec in window["protect"] if str(rec.get("txt")) == "hop"), None)
    if claim is None and window["near"] is not None:
        claim = start + window["near"]
    yards = None if claim is None else escort_yards(samples, guid, window["guid"], claim)
    first_hand = min((rec["t"] for rec in window["hop"]), default=stop + 1)
    boost = next((rec["t"] for rec in window["boosts"] if rec["t"] < first_hand), None)
    return {"guid": guid, "t": claim, "yards": yards, "swaps": alternations(moves.get(guid, []), start, stop),
            "boost": boost, "boost_yd": None if boost is None else escort_yards(samples, guid, window["guid"], boost),
            "end_yd": escort_yards(samples, guid, window["guid"], stop),
            "frost_s": frost_seconds(frost.get(guid, []), start, stop)}


def claim_casts(window: dict) -> list[tuple[dict, int | None]]:
    """Each Hand of Protection on the kiter with its claimer's first hop row before it, in ms, None with none."""
    out = []
    for rec in window["hop"]:
        rows = [note["t"] for note in window["protect"]
                if note.get("g") == rec.get("s") and str(note.get("txt")) == "hop"
                and rec["t"] - CLAIM_LOOKBACK_MS <= note["t"] <= rec["t"]]
        out.append((rec, rec["t"] - rows[0] if rows else None))
    return out


def hand_patch_pass(samples: Samples, latches, window: dict, hands) -> dict | None:
    """The kiter's closest approach to a latched tank patch while its Hand of Protection held, before its turn for
    a patch: when, how far and the side. None when it kept HAND_PATCH_CLEARANCE off every one."""
    guid = window["guid"]
    stop = window["turn"] if window["turn"] is not None else window["stop"]
    stamps, rows = samples.track(guid)
    best = None
    for low, high, _ in hands.get(guid, []):
        for index in range(bisect.bisect_left(stamps, max(low, window["start"])), bisect.bisect_right(stamps, min(high, stop))):
            for side, spans in enumerate(latches):
                patch = latched_patch(spans, samples, stamps[index])
                if patch is None:
                    continue
                yards = dist2(rows[index][1:3], patch)
                if best is None or yards < best["yards"]:
                    best = {"t": stamps[index], "yards": yards, "side": side}
    return best if best and best["yards"] < HAND_PATCH_CLEARANCE else None


def own_after_hand(window: dict, hands) -> list[tuple[dict, int]]:
    """Each own immunity cast after a Hand of Protection on the kiter ran out inside the window, and how long
    after."""
    ended = [high for low, high, closed in hands.get(window["guid"], []) if closed and window["start"] <= high]
    out = []
    for rec in window["own"]:
        before = [high for high in ended if high <= rec["t"]]
        if before:
            out.append((rec, rec["t"] - before[-1]))
    return out


def hand_boosts(window: dict, hands) -> list[dict]:
    """The kiter's boosts in the first HAND_BOOST_MS of a Hand of Protection on it."""
    spans = hands.get(window["guid"], [])
    return [rec for rec in window["boosts"]
            if any(low <= rec["t"] < min(high, low + HAND_BOOST_MS) for low, high, _ in spans)]


def boosts_after_hand(window: dict) -> list[dict]:
    """The kiter's RUN_BOOSTS cast after the first Hand of Protection on it landed, inside the kite."""
    if not window["hop"]:
        return []
    landed = window["hop"][0]["t"]
    return [rec for rec in window["boosts"] if rec["sp"] in RUN_BOOSTS and rec["t"] > landed]


def boost_turn(window: dict) -> dict | None:
    """The kiter's RUN_BOOSTS cast nearest its turn for a patch, within BOOST_TURN_MS either side; None with no
    turn or none that close."""
    turn = window["turn"]
    if turn is None:
        return None
    near = [rec for rec in window["boosts"] if rec["sp"] in RUN_BOOSTS and abs(rec["t"] - turn) <= BOOST_TURN_MS]
    return min(near, key=lambda rec: abs(rec["t"] - turn), default=None)


def patch_escort(samples: Samples, escorts: dict[int, list[tuple[int, str]]], latches, window: dict) -> dict | None:
    """The paladin whose `anub.escort` first read `patch` in the kite: as the first Hand of Protection on the kiter
    went out, else at the kite's end, where it stood, its yards to the kiter and to the nearest kite patch, None
    with none down. None when no escort waited by the patch."""
    start, stop = window["start"], window["stop"]
    rows = [(when, guid) for guid, values in escorts.items() for when, value in values
            if start <= when <= stop and value == "patch"]
    if not rows:
        return None
    guid = min(rows)[1]
    when = window["hop"][0]["t"] if window["hop"] else stop
    row = samples.recent(guid, when)
    spots = patch_spots(samples, when)
    kite = [spots[patch] for patch in unlatched_patches(samples, latches, when) if patch in spots]
    return {"guid": guid, "t": when, "hand": bool(window["hop"]), "spot": (row[1], row[2]) if row else None,
            "kiter_yd": escort_yards(samples, guid, window["guid"], when),
            "patch_yd": min((dist2(row[1:3], spot) for spot in kite), default=None) if row else None}


def speed_boosts(window: dict) -> list[dict]:
    return [rec for rec in window["boosts"] if KITE_BOOSTS[rec["sp"]] in SPEED_BOOSTS]


def boost_speeds(samples: Samples, spikes, window: dict) -> list[dict]:
    """Each Sprint, Dash and Nitro Boosts in the window with the spike's speed over the second after it and the
    kiter's run over BOOST_RUN_MS before it, BASE_RUN_YD_S for one that stood. `slow` is a spike no faster than
    that run, None when the spike went unread."""
    guid = window["guid"]
    out = []
    for rec in speed_boosts(window):
        when = rec["t"]
        spike = spike_at(samples, spikes, guid, when) or spike_at(samples, spikes, guid, when + NEAR_MS)
        spike_speed = unit_speed(samples, spike[0], when - 500, when + 1000) if spike else None
        kiter_speed = unit_speed(samples, guid, when - BOOST_RUN_MS, when) or BASE_RUN_YD_S
        out.append({"rec": rec, "spike": spike_speed, "kiter": kiter_speed,
                    "slow": None if spike_speed is None else spike_speed <= kiter_speed + SPEED_SLACK})
    return out


def kitepath_boost(text) -> float | None:
    """The second from its pick an `anub.kitepath` pick fires the kiter's speed boost, None for none."""
    _, found, seconds = str(text).rpartition("+boost@")
    try:
        return float(seconds) if found else None
    except ValueError:
        return None


def boost_plan(window: dict) -> dict | None:
    """The kite's first speed boost against the `anub.kitepath` pick in force as it went out: `cast` the boost or
    None, `pick` that pick's row or None, `planned` when the pick fires it, None for a pick that fires none. With no
    boost, the last pick's, when it came due inside the mark. None with neither."""
    casts = speed_boosts(window)
    cast = casts[0] if casts else None
    picks = window["kitepath"]
    if cast is not None:
        picks = [pick for pick in picks if pick["at"] <= cast["t"]]
    pick = picks[-1] if picks else None
    seconds = None if pick is None else kitepath_boost(pick["value"])
    planned = None if seconds is None else pick["at"] + int(round(seconds * 1000.0))
    if cast is None and (planned is None or planned > window["stop"]):
        return None
    return {"cast": cast, "pick": pick, "planned": planned}


def standby_read(samples: Samples, escorts: dict[int, list[tuple[int, str]]], window: dict) -> dict | None:
    """The paladin whose `anub.escort` first read standby in the kite, its yards to the kiter at the claim, the
    window's first `anub.protect` hop row, and the paladin that wrote it. None with no standby."""
    start, stop = window["start"], window["stop"]
    rows = [(when, guid) for guid, values in escorts.items() for when, value in values
            if start <= when <= stop and value == ESCORT_STANDBY]
    if not rows:
        return None
    guid = min(rows)[1]
    claim = next((rec for rec in window["protect"] if str(rec.get("txt")) == "hop"), None)
    when = None if claim is None else claim["t"]
    return {"guid": guid, "t": when, "claimer": None if claim is None else claim.get("g"),
            "yards": None if when is None else escort_yards(samples, guid, window["guid"], when)}


def next_leg_pass(samples: Samples, spikes, latches, taken, windows: list[dict], index: int, submerged) -> dict | None:
    """After a kite ended by its own immunity or on a patch, the spike's leg to the next mark under the same
    submerge, from that mark to the spike's first snapshot within KITE_PROTECT_RANGE of its kiter or of where the
    kiter stood at the mark, else to that mark's end: the first patch whose centre a step between two snapshots
    came within SPIKE_PATCH_REACH of and that the spike wasn't inside as the leg began, else the patch that mark
    ended on, `yards` None then; with the side holding it and `spent`, the next mark ending on it. `next` None with
    no next mark; None for a kite that ended otherwise."""
    window = windows[index]
    own = window["ended"] in KITER_OWN.values()
    if not own and not (window["outcome"] == "patch" and window["consumed"]):
        return None
    out = {"after": window["ended"] if own else "patch", "next": None, "pass": None}
    under = next(((low, high) for low, high in submerged if low <= window["stop"] <= high), None)
    later = next((other for other in windows[index + 1:] if other["start"] >= window["stop"] - NOTE_TICK_MS), None)
    if under is None or later is None or later["start"] > under[1]:
        return out
    out["next"] = later
    kiter, start, stop = later["guid"], later["start"], later["stop"]
    marked = samples.row(kiter, start)
    began = spike_at(samples, spikes, kiter, start)
    last = None if began is None else (began[1], began[2])
    inside = None if last is None else {guid for guid, spot in patch_spots(samples, start).items()
                                        if dist2(last, spot) <= SPIKE_PATCH_REACH}
    for when in samples.stamps[bisect.bisect_right(samples.stamps, start):bisect.bisect_right(samples.stamps, stop)]:
        spike, unit = spike_at(samples, spikes, kiter, when), samples.recent(kiter, when)
        if spike is None:
            continue
        here = (spike[1], spike[2])
        near = sorted((segment_yd(last or here, here, spot), guid, spot)
                      for guid, spot in patch_spots(samples, when).items()
                      if segment_yd(last or here, here, spot) <= SPIKE_PATCH_REACH)
        if inside is None:
            inside = {guid for _, guid, _ in near}
        passed = [row for row in near if row[1] not in inside]
        if passed:
            yards, guid, spot = passed[0]
            out["pass"] = {"t": when, "guid": guid, "spot": spot, "yards": yards,
                           "side": patch_side(latches, guid, when), "spent": taken.get(guid, (0, None))[1] == stop}
            return out
        if any(row is not None and dist2(here, row[1:3]) <= KITE_PROTECT_RANGE for row in (unit, marked)):
            return out
        last = here
    ended = next((guid for guid, (seen, end) in taken.items() if end == stop and guid not in (inside or ())), None)
    row = None if ended is None else samples.row(ended, taken[ended][0])
    if row is not None:
        out["pass"] = {"t": stop, "guid": ended, "spot": (row[1], row[2]), "yards": None,
                       "side": patch_side(latches, ended, taken[ended][0]), "spent": True}
    return out


def submerge_kites(trace: Trace, windows: list[dict], spent: list[dict]) -> list[dict]:
    """Per submerge: the seconds someone carried the mark and the seconds the spike idled between marks, from the
    first mark to its despawn; the terminal each kite ended on; the seconds the spike lived on after the
    submerge's first tank patch went, None with none; kiters dead."""
    firsts = {first["start"]: first["row"] for first in first_spends(trace, spent)}
    out = []
    for start, stop, phase, label in phase_spans(trace):
        if phase != 2:
            continue
        end = min(stop, start + SPIKE_LIFE_MS)
        inside = [window for window in windows if start <= window["start"] < end]
        kite = sum(min(window["stop"], end) - window["start"] for window in inside)
        idle = max(0, end - inside[0]["start"] - kite) if inside else 0
        row = firsts.get(start)
        out.append({"label": label, "start": start, "kite": kite, "idle": idle,
                    "terminals": collections.Counter(window["plan"][-1] if window["plan"] else "-"
                                                     for window in inside),
                    "after": end - row["cleared"] if row and row["cleared"] < end else None,
                    "deaths": sum(1 for window in inside if window["outcome"] == "death")})
    return out


def mark_windows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    impales = [rec for rec in trace.of("dmg") if rec.get("sp") in IMPALE]
    deaths = combat_deaths(trace)
    kite = per_bot(trace, "anub.kite")
    plans = per_bot(trace, "anub.kiteplan")
    stages = holder_spans(trace, "anub.kitestage", pull_end(trace))
    holds = holder_spans(trace, "anub.gather", pull_end(trace))
    boosts = boost_rows(trace)
    taken = consumed_patches(trace, samples)
    used = {stop for _, stop in taken.values()}
    submerged = submerged_spans(trace)
    emerges = [stop for _, stop in submerged]
    casts = [rec for rec in trace.of("cast") if not rec.get("tr")]
    hops = [rec for rec in casts if rec.get("sp") in HAND_OF_PROTECTION]
    own = [rec for rec in casts if rec.get("sp") in KITER_OWN]
    protect = notes(trace, "anub.protect")
    spikes = guids_of_entry(trace, NPC_SPIKE)
    latches = patch_latches(trace)
    moves: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("x") is not None and rec.get("y") is not None:
            moves[rec.get("g")].append(rec)
    feigns = aura_spans(trace, (SPELL_FEIGN_DEATH,))
    ready = hop_readiness(trace)
    bridges = per_bot(trace, "anub.bridge")
    kitemoves = per_bot(trace, "anub.kitemove")
    sides = side_spans(trace)
    marks = mark_spans(trace)
    hand_auras = aura_spans(trace, HAND_OF_PROTECTION)
    preds = per_bot(trace, "anub.kitepred")
    paths = per_bot(trace, "anub.kitepath")
    # older binaries never planned a boost, so every boost there would read as one its pick didn't fire
    plans_boosts = any(kitepath_boost(value) is not None for rows in paths.values() for _, value in rows)
    escorts = per_bot(trace, "anub.escort")
    seconds = per_bot(trace, "anub.second")
    frost_auras = aura_spans(trace, PERMAFROST)

    out = []
    for guid, start, stop, closed in marks:
        hits = [rec for rec in impales
                if rec.get("d") == guid and start <= rec["t"] <= stop + IMPALE_SLACK_MS]
        death = next((rec for rec in deaths
                      if rec.get("g") == guid and start <= rec["t"] <= stop + IMPALE_SLACK_MS), None)
        died = None if death is None else death["t"]
        consumed = closed and stop in used
        by_emerge = any(abs(stop - edge) <= EMERGE_SLACK_MS for edge in emerges)
        if died is not None:
            outcome = "death"
        elif hits:
            outcome = "impale"
        elif not closed:
            outcome = "open"
        elif not consumed and by_emerge:
            outcome = "emerge"
        else:
            # nothing hit the kiter and it lived, so a patch or a falling sphere took the spike
            outcome = "patch"
        immune = next((rec for rec in own if rec.get("s") == guid
                       and stop - IMMUNE_END_MS <= rec["t"] <= stop + IMPALE_SLACK_MS), None)
        ended = "death" if died is not None else None
        # the emerge despawns the spike, so a last-moment immunity there handed nothing on
        if ended is None and immune and closed and not by_emerge:
            ended = KITER_OWN[immune["sp"]]
        spike = spike_at(samples, spikes, guid, stop)
        centre = gather_centre(holds, samples, submerged, stop, {guid})
        to_gather = dist2(spike[1:3], centre) if spike and centre is not None else None
        stand = samples.row(guid, start)
        frost = [dist2(stand[1:3], point) for point in samples.frost_at(start)] if stand else []
        near = spike_near(samples, spikes, guid, start, stop)
        hands = [rec for rec in hops if rec.get("tgt") == guid and start <= rec["t"] <= stop]
        # read just before a hand that landed by then, its Forbearance would rule out every paladin
        asked = None if near is None else min([start + near] + [rec["t"] - 1 for rec in hands
                                                                  if rec["t"] <= start + near])
        out.append({"guid": guid, "start": start, "stop": stop,
                    "branches": branches_in(kite.get(guid, []), start, stop),
                    "plan": plan_terminals(plans.get(guid, []), start, stop),
                    "impales": len(hits), "damage": sum(rec.get("a", 0) for rec in hits),
                    "impale_at": min((rec["t"] for rec in hits), default=None),
                    "outcome": outcome, "consumed": consumed, "died": died,
                    "killer": None if death is None else death.get("killer"),
                    "killer_spike": death is not None and death.get("killer") in spikes,
                    "near": near,
                    "escort": None if asked is None else ready_paladin(samples, ready, guid, asked),
                    "flips": flip_flops(moves.get(guid, []), start, stop),
                    "feign_drops": [low for low, high, done in feigns.get(guid, [])
                                    if done and start <= low <= stop + IMPALE_SLACK_MS
                                    and high - low <= FEIGN_DROP_MS],
                    "hop": hands,
                    "own": [rec for rec in own if rec.get("s") == guid and start <= rec["t"] <= stop],
                    "protect": [rec for rec in protect if start <= rec["t"] <= stop],
                    "stages": stage_time(stages.get(guid, []), start, stop),
                    "boosts": kiter_boosts(boosts, guid, start, stop),
                    "tank_near": tank_patch_approach(samples, latches, spikes, start, stop),
                    "ended": ended, "gather": centre, "to_gather": to_gather,
                    "kite_patches": unlatched_patches(samples, latches, stop),
                    "near_gather": ended is not None and to_gather is not None and to_gather <= KITE_END_GATHER_YD,
                    "frost_yd": min(frost) if frost else None,
                    "in_frost": bool(frost) and min(frost) <= PERMAFROST_SLOW_REACH,
                    "stalled": stalled(samples, guid, start, stop)})
        window = out[-1]
        window["cover"] = cover_speeds(samples, spikes, window)
        window["closing"] = closing_spans(samples, spikes, window)
        window["patch_kite"] = (any(terminal in PATCH_PLANS for terminal in window["plan"])
                                or "patch" in window["stages"])
        # under the Hand a patch+hand kite walks its patch in the immune stage
        walks = ("patch", "immune") if "patch+hand" in window["plan"] else ("patch",)
        turn, by = (kite_turn(bridges.get(guid, []), stages.get(guid, []), start, stop, walks)
                    if window["patch_kite"] else (None, None))
        window.update({
            "turn": turn, "turn_by": by,
            "bridge": [value for when, value in bridges.get(guid, [])
                       if start <= when <= stop + NOTE_TICK_MS and value != "none"],
            "moves": moves_from(kitemoves.get(guid, []), turn, stop) if turn is not None else [],
            "turned_for": (turned_for(samples, spikes, latches, taken, guid, turn, stop)
                           if turn is not None else None),
            "first_pass": first_patch_pass(samples, spikes, latches, guid, start, stop if turn is None else turn),
            "side_impales": [(rec, held_side(sides, rec.get("d"), rec["t"])) for rec in impales
                             if rec.get("d") != guid and start <= rec["t"] <= stop + IMPALE_SLACK_MS
                             and held_side(sides, rec.get("d"), rec["t"]) is not None]})
        window["catch"] = (hand_catches(samples, spikes, window, marks, submerged, hand_auras)
                           if "patch+hand" in window["plan"] else [])
        window["turn_escort"] = (ready_paladin(samples, ready, guid, turn)
                                 if "turn" in window["bridge"] and turn is not None else None)
        window.update({
            "kitepred": noted_in(preds.get(guid, []), start, stop),
            "kitepath": kitepath_changes(paths.get(guid, []), start, stop),
            "escort_read": escort_read(samples, escorts, window, moves, frost_auras),
            "claim_casts": claim_casts(window),
            "hand_patch": hand_patch_pass(samples, latches, window, hand_auras),
            "own_after_hand": own_after_hand(window, hand_auras),
            "hand_boosts": hand_boosts(window, hand_auras),
            "after_hand": boosts_after_hand(window),
            "boost_turn": boost_turn(window),
            "patch_escort": patch_escort(samples, escorts, latches, window),
            "second": [when for when, value in seconds.get(guid, [])
                       if start <= when <= stop + NOTE_TICK_MS and value == "grant"],
            "boost_speeds": boost_speeds(samples, spikes, window),
            "standby": standby_read(samples, escorts, window)})
        window["boost_plan"] = boost_plan(window) if plans_boosts else None
    for index, window in enumerate(out):
        window["next_leg"] = next_leg_pass(samples, spikes, latches, taken, out, index, submerged)
    return out


def stray_impales(trace: Trace, windows: list[dict]) -> list[dict]:
    """Impale hits on anyone but the kiter of the window they landed in."""
    def kiter_hit(rec):
        return any(window["guid"] == rec.get("d")
                   and window["start"] <= rec["t"] <= window["stop"] + IMPALE_SLACK_MS for window in windows)

    return [rec for rec in trace.of("dmg") if rec.get("sp") in IMPALE and not kiter_hit(rec)]


def submerge_crowds(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per submerge: his last surfaced spot, the living roster within each CROWD_YD of it by role, and the
    first mark under him with the kiter's distance from the spike at the first snapshot of its mark."""
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    spikes = guids_of_entry(trace, NPC_SPIKE)
    marks = mark_spans(trace)
    roster = roster_guids(trace)
    holds = holder_spans(trace, "anub.gather", pull_end(trace))
    submerged = submerged_spans(trace)
    out = []
    for start, stop, phase, label in phase_spans(trace):
        row = samples.row(boss, start - 1) if boss and phase == 2 else None
        if row is None:
            continue
        spot = (row[1], row[2])
        gather = gather_centre(holds, samples, submerged, min(start + GATHER_FORMED_MS, stop))
        near = {yards: collections.Counter() for yards in CROWD_YD}
        for guid in roster:
            unit = samples.recent(guid, start - 1)
            if unit is None or unit[5] <= 0:
                continue
            gap = dist2(unit[1:3], spot)
            for yards in CROWD_YD:
                if gap <= yards:
                    near[yards][trace.role(guid)] += 1
        first = next((span for span in marks if start <= span[1] < stop), None)
        kiter = mark = spike_yd = None
        if first:
            kiter, mark = first[0], first[1]
            index = bisect.bisect_left(samples.stamps, mark)
            at = samples.stamps[index] if index < len(samples.stamps) else mark
            spike, unit = spike_at(samples, spikes, kiter, at), samples.recent(kiter, at)
            spike_yd = dist2(spike[1:3], unit[1:3]) if spike and unit else None
        out.append({"label": label, "start": start, "spot": spot, "near": near, "kiter": kiter, "mark": mark,
                    "spike_yd": spike_yd, "gather_yd": None if gather is None else dist2(spot, gather)})
    return out


def run_spend(row: dict | None) -> bool:
    """A tank patch spent while its kiter ran or boosted, before the kite had come down to a patch."""
    stage = (row or {}).get("stage") or ""
    return stage == "run" or stage.startswith("boost")


def kite_hazards(trace: Trace, samples: Samples, windows: list[dict]) -> dict:
    """Per kite with `anub.kitepred` picks, keyed `(kiter, mark)`: `(predicted, real)` first hazards, each
    `(what, when)` or None. Empty when the kite planner's Rules can't be read."""
    if not any(window["kitepred"] for window in windows):
        return {}
    try:
        rules = planner_rules()
    except LookupError:
        return {}
    spikes = guids_of_entry(trace, NPC_SPIKE)
    latches = patch_latches(trace)
    holds = holder_spans(trace, "anub.gather", pull_end(trace))
    submerged = submerged_spans(trace)
    marks = mark_spans(trace)
    out = {}
    for window in windows:
        if window["kitepred"]:
            gather = formed_gather(holds, samples, submerged, window["start"], window["guid"])
            out[(window["guid"], window["start"])] = (
                predicted_hazard(window["kitepred"]),
                real_hazard(samples, spikes, latches, window, gather, marks, submerged, rules))
    return out


def planner_lines(window: dict, spend: dict | None, hazard) -> list[str]:
    """A kite's `anub.kitepred` at the mark against the real end, its `anub.kitepath` picks with the seconds each
    held, a reversal flagged, and the first predicted hazard against the first real one."""
    def hazard_text(found) -> str:
        return "none" if found is None else f"{found[0]} {rel_s(window, found[1])}"

    lines = []
    if window["kitepred"]:
        lines.append(f"kitepred at the mark {window['kitepred'][0][1]}; real end {real_end(window, spend)}"
                     f" {(window['stop'] - window['start']) / 1000:.1f} s")
    if window["kitepath"]:
        lines.append("kitepath " + ", ".join(
            f"{pick['value']} {pick['held'] / 1000:.1f} s" + (" reversal" if pick["reversal"] else "")
            for pick in window["kitepath"]))
    if hazard is not None:
        lines.append(f"first hazard: predicted {hazard_text(hazard[0])}, real {hazard_text(hazard[1])}")
    return lines


def paladin_hop_cooldowns(trace: Trace) -> dict[int, int]:
    """Per paladin, its Hand of Protection cooldown as `hop_cooldown` reads it, from its own casts and reads only."""
    casts: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("sp") in HAND_OF_PROTECTION and not rec.get("tr"):
            casts[rec.get("s")].append(rec["t"])
    spans: dict[int, list[int]] = collections.defaultdict(list)
    for when, paladin, left, _ in hand_reads(trace):
        last = [cast for cast in casts.get(paladin, []) if cast <= when]
        if left > 0 and last:
            spans[paladin].append(when - last[-1] + left)
    return {paladin: int(round(statistics.median(found) / 1000.0)) * 1000 for paladin, found in spans.items()}


def submerge_spends(trace: Trace) -> list[dict]:
    """Per submerge, the most kite patches `anub.kitespent` counted in it (`patches`, None with no row) and each
    Hand of Protection cast in it as `(t, caster, cooldown ms)`, the caster's own cooldown, else HOP_COOLDOWN_MS."""
    cooldowns = paladin_hop_cooldowns(trace)
    spent = [(rec["t"], int(rec["txt"])) for rec in notes(trace, "anub.kitespent") if str(rec.get("txt", "")).isdigit()]
    hands = [rec for rec in trace.of("cast") if rec.get("sp") in HAND_OF_PROTECTION and not rec.get("tr")]
    out = []
    for index, (start, stop) in enumerate(submerged_spans(trace), 1):
        counts = [count for when, count in spent if start <= when <= stop]
        out.append({"index": index, "start": start, "patches": max(counts) if counts else None,
                    "hands": [(rec["t"], rec.get("s"), cooldowns.get(rec.get("s"), HOP_COOLDOWN_MS))
                              for rec in hands if start <= rec["t"] < stop]})
    return out


def show_submerge_spends(trace: Trace) -> None:
    rows = submerge_spends(trace)
    if not rows:
        return
    print(f"  per submerge the kite patches anub.kitespent counted, over 1 before submerge {LAST_KITE_SUBMERGE}"
          " flagged, and the Hands of Protection\n  cast with the caster's cooldown:")
    for row in rows:
        patches = "no anub.kitespent rows" if row["patches"] is None else f"patches {row['patches']}"
        if row["patches"] is not None and row["patches"] > 1 and row["index"] < LAST_KITE_SUBMERGE:
            patches += " over the reserve"
        hands = ", ".join(f"{trace.name(caster)} {clock(when)} ({cooldown // 1000} s)"
                          for when, caster, cooldown in row["hands"]) or "none"
        print(f"    {row['index']} {clock(row['start']):>9}  {patches}; Hands {hands}")


def show_spike(trace: Trace) -> None:
    print("PURSUING SPIKE")
    samples = Samples(trace)
    windows = mark_windows(trace, samples)
    spent = spent_patches(trace, samples)
    spends = {(row["kiter"], row["mark"]): row for row in spent if row["kiter"]}
    hazards = kite_hazards(trace, samples, windows)
    if not windows:
        print("  nobody carried Pursued by Anub'arak")
    else:
        print(f"  {'at':>9} {'kiter':14} {'role':6} {'secs':>5} {'kite branches':24} {'outcome':7}"
              f" {'impales':>7} {'damage':>8} {'near':>5}")
        for window in windows:
            branches = ">".join(window["branches"]) or "-"
            near = "-" if window["near"] is None else f"{window['near'] / 1000:.1f}"
            spend = spends.get((window["guid"], window["start"]))
            gone = (f"  tank patch {spend['side']} spent" if spend
                    else "  patch seen going" if window["consumed"] else "")
            if run_spend(spend):
                gone += f" in {stage_name(trace, spend['stage'])}"
            print(f"  {clock(window['start']):>9} {trace.name(window['guid'])[:14]:14}"
                  f" {trace.role(window['guid']):6} {(window['stop'] - window['start']) / 1000:5.1f}"
                  f" {branches[:24]:24} {window['outcome']:7} {window['impales']:7} {window['damage']:8,}"
                  f" {near:>5}" + gone)
            plan = plan_line(window, spend)
            if plan:
                print(f"            {plan}")
            for line in (turn_line(trace, window), bridge_line(trace, window), pass_line(trace, window),
                         side_impale_line(trace, window), fallback_line(trace, window), catch_line(trace, window)):
                if line:
                    print(f"            {line}")
            if window["in_frost"]:
                print(f"            marked {window['frost_yd']:.1f} yd from a Permafrost centre, inside its slow")
            cover = protection(trace, window)
            if cover:
                print(f"            {cover}")
            if window["cover"]:
                print(f"            speeds at the cover: {speed_line(window)}")
            if window["boost_speeds"]:
                print(f"            speeds at the boosts: {boost_speed_line(window)}")
            for span in window["closing"]:
                kiter = "-" if span["kiter"] is None else f"{span['kiter']:.1f}"
                print(f"            closed on a spike no faster than it from {rel_s(window, span['start'])}:"
                      f" {closing_text(span)}, kiter {kiter} yd/s")
            escort = escort_line(trace, window)
            if escort:
                print(f"            {escort}")
            still, longest = window["stalled"]
            if still:
                print(f"            stood still {still / 1000:.1f} s while marked, longest {longest / 1000:.1f} s")
            for low, high, reversals in window["flips"]:
                print(f"            walk flip-flopped from {(low - window['start']) / 1000:+.1f} s for"
                      f" {(high - low) / 1000:.1f} s, {reversals} reversals")
            for low in window["feign_drops"]:
                print(f"            Feign Death dropped at once {(low - window['start']) / 1000:+.1f} s")
            run = kite_line(trace, window)
            if run:
                print(f"            {run}")
            for line in planner_lines(window, spend, hazards.get((window["guid"], window["start"]))):
                print(f"            {line}")
            approach = tank_line(window)
            if approach:
                print(f"            spike to tank patch: {approach}")
            print(f"            at the end: {end_line(trace, window)}")
            for line in rework_lines(trace, window):
                print(f"            {line}")
        outcomes = collections.Counter(window["outcome"] for window in windows)
        print(f"\n  {len(windows)} window(s): "
              + ", ".join(f"{name} {n}" for name, n in outcomes.most_common()))
        show_plans(trace, windows, spends)
        turned = [(window, pick) for window in windows for pick in window["kitepath"] if pick["reversal"]]
        print(f"  anub.kitepath picks turning over {PICK_REVERSAL_DEGREES:.0f} degrees within"
              f" {PICK_REVERSAL_MS / 1000:.0f} s of the pick before: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(window['start'])} {rel_s(window, pick['at'])}"
                           for window, pick in turned) or "none")
              + ("" if notes(trace, "anub.kitepath") else ", no anub.kitepath rows"))
        show_turns(trace, windows)
        show_catches(trace, windows, spent)
        reached =sum(1 for window in windows
                      if any(yards < SPIKE_PATCH_REACH for yards in window["tank_near"].values()))
        print(f"  spike within {SPIKE_PATCH_REACH} yd of a latched tank patch, its fail test's reach:"
              f" {reached} window(s)")
        near = [window for window in windows if window["near"] is not None]
        print(f"  near is the seconds in when the spike first came within {KITE_PROTECT_RANGE:.0f} yd of the"
              f" kiter: {len(near)} window(s), Hand of Protection in"
              f" {sum(1 for window in near if window['hop'])}, an own immunity in"
              f" {sum(1 for window in near if window['own'])}")
        stages: collections.Counter = collections.Counter()
        for window in windows:
            stages.update(window["stages"])
        if stages:
            print("  kitestage over every window: " + ", ".join(
                f"{stage_name(trace, value)} {ms / 1000:.1f} s" for value, ms in stages.most_common()))
        unasked_own = collections.Counter(KITER_OWN[rec["sp"]] for window in windows for rec in window["own"]
                                          if unasked(window, rec))
        print("  own immunity on a kiter anub.protect never asked for: "
              + (", ".join(f"{name} {n}" for name, n in unasked_own.most_common()) if unasked_own else "none"))
        flagged = [window for window in windows if window["near_gather"]]
        print(f"  kite ended by a death or an own immunity within {KITE_END_GATHER_YD:.0f} yd of the gather: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(window['stop'])} {window['ended']}"
                           for window in flagged) or "none"))
        print("  the gather is the mean spot of the living bots holding anub.gather since he went under, the"
              " kiter left out")
        inside = [window for window in windows if window["in_frost"]]
        print(f"  marked within {PERMAFROST_SLOW_REACH} yd of a Permafrost centre: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(window['start'])} {window['frost_yd']:.1f} yd"
                           for window in inside) or "none"))
        slow = [(window, row) for window in windows for row in window["cover"] if row["slow"]]
        print("  Hand of Protection or an own immunity on a spike no faster than the kiter: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(row['rec']['t'])} {cover_name(row['rec'])}"
                           f" ({row['spike']:.1f} vs {row['kiter']:.1f} yd/s)" for window, row in slow) or "none"))
        slow_boosts = [(window, row) for window in windows for row in window["boost_speeds"] if row["slow"]]
        print(f"  Sprint, Dash or Nitro Boosts on a spike no faster than the kiter's run over the"
              f" {BOOST_RUN_MS / 1000:.1f} s before it: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(row['rec']['t'])} {KITE_BOOSTS[row['rec']['sp']]}"
                           f" ({row['spike']:.1f} vs {row['kiter']:.1f} yd/s)" for window, row in slow_boosts)
                 or "none"))
        closing = [(window, span) for window in windows for span in window["closing"]]
        print(f"  kiter closing on a spike no faster than itself for {CLOSING_MS / 1000:.0f}+ s, ending within"
              f" {KITE_PROTECT_RANGE:.0f} yd: {len(closing)} stretch(es),"
              f" {sum(span['stop'] - span['start'] for _, span in closing) / 1000:.1f} s"
              + "".join(f"; {trace.name(window['guid'])} {clock(span['start'])} {closing_text(span)}"
                        for window, span in closing))
        still =[window for window in windows if window["stalled"][0]]
        print(f"  stood still while marked, under {STALL_YD} yd in {STALL_MS / 1000:.0f} s and alive: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(window['start'])}"
                           f" {window['stalled'][0] / 1000:.1f} s" for window in still) or "none"))
        ran = [row for row in spends.values() if run_spend(row)]
        print("  tank patch spent while the kiter ran or boosted: "
              + (", ".join(f"{trace.name(row['kiter'])} {clock(row['gone'])} side {row['side']}"
                           f" {stage_name(trace, row['stage'])}" for row in ran) or "none"))
        print("  speeds are the median of the moving snapshot steps, the spike's over the second after the cover,"
              "\n  the kiter's around it")
        flipped = [window for window in windows if window["flips"]]
        print(f"  walk flip-flopped, move targets {FLIP_YD:.0f}+ yd apart reversing {FLIP_COUNT}+ times in"
              f" {FLIP_MS / 1000:.0f} s: " + (", ".join(
                  f"{trace.name(window['guid'])} {clock(window['start'])}"
                  f" from {(window['flips'][0][0] - window['start']) / 1000:+.1f} s" for window in flipped) or "none"))
        dropped = [(window, low) for window in windows for low in window["feign_drops"]]
        print(f"  Feign Death dropped within {FEIGN_DROP_MS} ms: " + (", ".join(
            f"{trace.name(window['guid'])} {clock(window['start'])} {(low - window['start']) / 1000:+.1f} s"
            for window, low in dropped) or "none"))
        neared = [window for window in windows if window["escort"]]
        far = [window for window in neared if window["escort"]["guid"] is None
               or window["escort"]["yards"] > HOP_RANGE]
        print(f"  nearest Hand of Protection-ready paladin as the spike came within {KITE_PROTECT_RANGE:.0f} yd:"
              f" within {HOP_RANGE:.0f} yd in {len(neared) - len(far)} of {len(neared)}; further or none: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(window['start'])} ({escort_brief(trace, window)})"
                           for window in far) or "none"))
        cooldown, source = hop_cooldown(trace)
        print(f"  ready is a bot paladin alive and unmarked, its last Hand of Protection {cooldown // 1000} s"
              f" or more back, the\n  cooldown {'read off anub.hands' if source == 'anub.hands' else 'with no anub.hands'},"
              " with no Forbearance on the kiter")
        show_rework(trace, windows, spent)
        teleports = circle_teleports(trace, windows, samples)
        print("  Demonic Circle teleports: " + ("; ".join(teleport_text(trace, row) for row in teleports) or "none"))
        print("  Hand of Protection on a bot warlock kiter, its circle in reach (62388) flagged: " + (", ".join(
            f"{trace.name(row['guid'])} {clock(row['window']['start'])} {rel_s(row['window'], row['t'])}"
            f" by {trace.name(row['caster'])}" + (", circle in reach" if row["reach"] else "")
            for row in warlock_hands(trace, windows)) or "none"))
    show_submerge_spends(trace)
    show_crowds(trace, samples)
    show_dodges(trace)

    stray = stray_impales(trace, windows)
    if stray:
        hit = collections.Counter(rec.get("d") for rec in stray)
        print(f"  Impale on anyone else: {len(stray)} hit(s) for {sum(rec.get('a', 0) for rec in stray):,}: "
              + ", ".join(f"{trace.name(guid)} ({trace.role(guid)}) {n}" for guid, n in hit.most_common()))
    else:
        print("  Impale hit nobody but a kiter")
    show_counts(trace, "anub.dodge")
    show_counts(trace, "anub.protect")
    show_counts(trace, "anub.gather")


def rel_s(window: dict, when: int) -> str:
    return f"{(when - window['start']) / 1000:+.1f} s"


def closing_text(span: dict) -> str:
    return (f"{(span['stop'] - span['start']) / 1000:.1f} s, gap {span['from']:.1f} to {span['to']:.1f} yd,"
            f" spike {' to '.join(f'{speed:.1f}' for speed in span['speeds'])} yd/s")


def own_unused_death(window: dict) -> bool:
    """Dead to something other than the spike with its own immunity planned and never cast."""
    return (window["outcome"] == "death" and not window["killer_spike"] and "own" in window["plan"]
            and not window["own"])


def spent_kite_tools(trace: Trace, windows: list[dict]) -> list[tuple[str, dict]]:
    """Heroic: each kite tool a bot cast outside its own marks before phase 3 and still on its base cooldown
    when the spike of a submerge then running or to come is summoned, with the phase it went in. Feign Death
    in phase 1 is back by then."""
    if not is_heroic(trace):
        return []
    spans = phase_spans(trace)
    roster = roster_guids(trace)
    marked = collections.defaultdict(list)
    for window in windows:
        marked[window["guid"]].append((window["start"], window["stop"]))
    out = []
    for rec in trace.of("cast"):
        guid = rec.get("s")
        if rec.get("tr") or rec.get("sp") not in KITE_TOOLS or guid in trace.humans or guid not in roster:
            continue
        if any(low <= rec["t"] <= high for low, high in marked.get(guid, [])):
            continue
        phase = next((phase for start, stop, phase, _ in spans if start <= rec["t"] < stop), None)
        if phase == 3:
            continue
        due = next((max(start + SPIKE_SUMMON_MS, rec["t"]) for start, stop, kind, _ in spans
                    if kind == 2 and stop > rec["t"]), None)
        if due is None or rec["t"] + KITE_TOOL_COOLDOWN_MS[rec["sp"]] <= due:
            continue
        out.append((phase_at(spans, rec["t"]) or "-", rec))
    return out


def circle_teleports(trace: Trace, windows: list[dict], samples: Samples | None = None) -> list[dict]:
    """Each Demonic Circle teleport a bot cast: its circle, where it stood as its last summon before went
    down, else, summoned before the trace began, where it stood in the first row after the teleport; at the
    cast the spike's yards from the kiter and from the circle; the kiter's closest pass by the circle from
    its last boost in the kite to the cast; and how long it stood after landing on the circle, to its first
    step off or its death."""
    samples = samples or Samples(trace)
    spikes = guids_of_entry(trace, NPC_SPIKE)
    casts = [rec for rec in trace.of("cast") if not rec.get("tr")]
    boosts = boost_rows(trace)
    out = []
    for rec in casts:
        guid, when = rec.get("s"), rec["t"]
        if rec.get("sp") != SPELL_CIRCLE_TELEPORT or guid in trace.humans:
            continue
        summon = next((prior for prior in reversed(casts) if prior.get("s") == guid
                       and prior.get("sp") == SPELL_CIRCLE_SUMMON and prior["t"] + prior.get("ct", 0) <= when), None)
        if summon:
            placed = samples.row(guid, summon["t"] + summon.get("ct", 0))
        else:
            stamps, rows = samples.track(guid)
            after = bisect.bisect_right(stamps, when)
            placed = rows[after] if after < len(stamps) and stamps[after] - when <= NEAR_MS else None
        kiter, spike = samples.row(guid, when), spike_at(samples, spikes, guid, when)
        if placed is None or kiter is None:
            continue
        circle = (placed[1], placed[2])
        window = next((window for window in windows if window["guid"] == guid
                       and window["start"] <= when <= window["stop"]), None)
        since = window["start"] if window else when
        boost = next((prior for prior in reversed(boosts) if prior.get("s") == guid
                      and prior["sp"] != SPELL_CIRCLE_TELEPORT and since <= prior["t"] < when), None)
        passed = None
        if boost:
            passed = min((dist2(row[1:3], circle) for row in samples.between(guid, boost["t"], when)), default=None)
        stamps, rows = samples.track(guid)
        stood = None
        first = bisect.bisect_right(stamps, when)
        landed = next((index for index in range(first, len(stamps))
                       if stamps[index] - when <= NEAR_MS and dist2(rows[index][1:3], circle) <= LANDED_ON_CIRCLE_YD),
                      None)
        if landed is not None:
            spot = rows[landed][1:3]
            stood = next((stamps[index] - stamps[landed] for index in range(landed, len(stamps))
                          if dist2(rows[index][1:3], spot) > LANDED_MOVE_YD or rows[index][5] <= 0), None)
        out.append({"guid": guid, "t": when, "circle": circle,
                    "spike_yd": dist2(spike[1:3], kiter[1:3]) if spike else None,
                    "circle_yd": dist2(spike[1:3], circle) if spike else None,
                    "boost": boost, "passed": passed, "stood": stood})
    return out


def circle_in_reach(trace: Trace, guid: int, when: int) -> bool:
    """The warlock's 62388 at `when`: the last row's state before it, else the opposite of the first row
    after it, a removal first meaning it stood in reach from the start; false with no row."""
    rows = [rec for rec in trace.of("aura") if rec.get("d") == guid and rec.get("sp") == SPELL_CIRCLE_IN_RANGE]
    before = [rec for rec in rows if rec["t"] <= when]
    if before:
        return not before[-1].get("r")
    return bool(rows) and bool(rows[0].get("r"))


def warlock_hands(trace: Trace, windows: list[dict]) -> list[dict]:
    """Each Hand of Protection onto a bot warlock inside its own mark, with whether its circle was in reach."""
    locks = {member["g"] for member in trace.header.get("roster", [])
             if member.get("c") == CLASS_WARLOCK and not member.get("h")}
    out = []
    for rec in trace.of("cast"):
        guid = rec.get("tgt")
        if rec.get("tr") or rec.get("sp") not in HAND_OF_PROTECTION or guid not in locks:
            continue
        window = next((window for window in windows if window["guid"] == guid
                       and window["start"] <= rec["t"] <= window["stop"]), None)
        if window:
            out.append({"guid": guid, "t": rec["t"], "window": window, "caster": rec.get("s"),
                        "reach": circle_in_reach(trace, guid, rec["t"])})
    return out


def kite_patches_ready(samples: Samples, latches, when: int) -> int:
    """Patches a heroic kite may end on at `when`: held by no side, and KITE_PATCH_TANK_YD or more off every
    latched tank patch."""
    spots = patch_spots(samples, when)
    tanks = [spot for spot in (latched_patch(spans, samples, when) for spans in latches) if spot]
    return sum(1 for guid in unlatched_patches(samples, latches, when)
               if guid in spots and all(dist2(spots[guid], tank) >= KITE_PATCH_TANK_YD for tank in tanks))


def kite_sphere_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Heroic, per submerge: each sphere downed from KITE_PRESHOT_MS before he went down to his emerge that no
    side ever latched, at the blow and at its spend, with the mark that ended on it and the milliseconds into
    that kite; the patches no side held on the floor as he went down (`down`); and the kite patches down as the
    spike came up."""
    samples = samples or Samples(trace)
    used = consumed_patches(trace, samples)
    marks = mark_spans(trace)
    deaths = sphere_deaths(trace, samples)
    latches = patch_latches(trace)
    out = []
    for start, stop, phase, label in phase_spans(trace):
        if phase != 2:
            continue
        spheres = []
        for row in deaths:
            if row["latched"] or not start - KITE_PRESHOT_MS <= row["killed"] < stop:
                continue
            entry = {"guid": row["guid"], "killed": row["killed"], "spent": None, "kiter": None, "into": None}
            seen = used.get(row["guid"])
            if seen:
                entry["spent"] = seen[0]
                mark = next((span for span in marks if span[3] and span[2] == seen[1]), None)
                if mark:
                    entry.update({"kiter": mark[0], "into": seen[0] - mark[1]})
            spheres.append(entry)
        out.append({"label": label, "start": start, "spheres": spheres,
                    "down": unlatched_patches(samples, latches, start),
                    "ready": kite_patches_ready(samples, latches, start + SPIKE_SUMMON_MS)})
    return out


def show_kite_spheres(trace: Trace, samples: Samples) -> None:
    if not is_heroic(trace):
        return
    rows = kite_sphere_rows(trace, samples)
    if not rows:
        return
    print(f"\n  per submerge, the kite spheres downed from {KITE_PRESHOT_MS / 1000:.0f} s before he went down"
          f" (heroic downs both of a\n  submerge's from then): the blow and the spend in seconds into phase 2,"
          f" the kite it ended, under {KITE_PATCH_EARLY_MS / 1000:.0f} s\n  into it flagged; a spike that came up"
          f" with no kite patch down flagged:")
    for row in rows:
        parts = []
        for sphere in row["spheres"]:
            text = f"{short(trace, sphere['guid'])} down {(sphere['killed'] - row['start']) / 1000:+.1f} s"
            if sphere["spent"] is None:
                text += ", never spent"
            else:
                text += f", spent {(sphere['spent'] - row['start']) / 1000:+.1f} s"
                if sphere["kiter"] is not None:
                    text += f" by {trace.name(sphere['kiter'])} {sphere['into'] / 1000:.1f} s into its kite"
                    if sphere["into"] < KITE_PATCH_EARLY_MS:
                        text += ", early"
            parts.append(text)
        bare = "" if row["ready"] else "  no kite patch down as the spike came up"
        print(f"    {row['label']:5} {clock(row['start']):>9}  {len(row['spheres'])}: " + ("; ".join(parts) or "none")
              + bare)
    print(f"  kite patches on the floor as he went down, under {KITE_SPHERES_PER_SUBMERGE} flagged: " + ", ".join(
        f"{row['label']} {len(row['down'])}" + (" under" if len(row["down"]) < KITE_SPHERES_PER_SUBMERGE else "")
        for row in rows))


def teleport_text(trace: Trace, row: dict) -> str:
    text = f"{trace.name(row['guid'])} {clock(row['t'])}"
    if row["spike_yd"] is not None:
        text += f" spike {row['spike_yd']:.1f} yd off, the circle {row['circle_yd']:.1f} yd from it"
    if row["boost"]:
        passed = "-" if row["passed"] is None else f"{row['passed']:.1f}"
        text += (f"; {KITE_BOOSTS[row['boost']['sp']]} {(row['t'] - row['boost']['t']) / 1000:.1f} s before,"
                 f" passing the circle at {passed} yd")
    if row["stood"] is not None:
        text += f"; stood {row['stood'] / 1000:.1f} s on landing"
    return text


def boost_turn_text(window: dict) -> str:
    rec = window["boost_turn"]
    gap = window["turn"] - rec["t"]
    if abs(gap) <= NOTE_TICK_MS:
        return f"turned with {RUN_BOOSTS[rec['sp']]}"
    return f"turned {abs(gap) / 1000:.1f} s {'after' if gap > 0 else 'before'} {RUN_BOOSTS[rec['sp']]}"


def patch_escort_text(trace: Trace, window: dict) -> str:
    escort = window["patch_escort"]
    spot = "unsampled" if escort["spot"] is None else f"{escort['spot'][0]:.1f},{escort['spot'][1]:.1f}"
    patch = "no kite patch down" if escort["patch_yd"] is None else f"{escort['patch_yd']:.1f} yd off the kite patch"
    kiter = "" if escort["kiter_yd"] is None else f", {escort['kiter_yd']:.1f} yd from the kiter"
    return (f"{trace.name(escort['guid'])} at {spot}, {patch}{kiter} at the"
            f" {'Hand' if escort['hand'] else 'end'} {rel_s(window, escort['t'])}")


def boost_speed_line(window: dict) -> str:
    """The spike's speed and the kiter's run before it at each speed boost in one window, a spike no faster
    flagged."""
    def speed(value) -> str:
        return "-" if value is None else f"{value:.1f} yd/s"

    return "; ".join(f"{KITE_BOOSTS[row['rec']['sp']]} {rel_s(window, row['rec']['t'])}: spike {speed(row['spike'])},"
                     f" kiter run {speed(row['kiter'])}" + (", no faster" if row["slow"] else "")
                     for row in window["boost_speeds"])


def boost_plan_text(window: dict) -> str:
    """The kite's first speed boost against the `+boost@` second of the pick in force as it went out."""
    plan = window["boost_plan"]
    cast, pick, planned = plan["cast"], plan["pick"], plan["planned"]
    if cast is None:
        return f"its pick's boost {rel_s(window, planned)} never cast"
    text = f"{KITE_BOOSTS[cast['sp']]} {rel_s(window, cast['t'])}"
    if pick is None:
        return text + ", no anub.kitepath pick before it"
    if planned is None:
        return text + f", its pick {rel_s(window, pick['at'])} firing none"
    gap = cast["t"] - planned
    timing = "on time" if abs(gap) <= BOOST_ON_TIME_MS else f"{abs(gap) / 1000:.1f} s {'late' if gap > 0 else 'early'}"
    return text + f" against its pick's {rel_s(window, planned)}, {timing}"


def boost_plan_slip(window: dict) -> bool:
    """A speed boost off its pick's second, cast with none planned, or planned and never cast."""
    plan = window["boost_plan"]
    if plan is None:
        return False
    if plan["cast"] is None or plan["planned"] is None:
        return True
    return abs(plan["cast"]["t"] - plan["planned"]) > BOOST_ON_TIME_MS


def standby_text(trace: Trace, window: dict) -> str:
    standby = window["standby"]
    text = f"standby escort {trace.name(standby['guid'])}"
    if standby["t"] is None:
        return text + " with no claim"
    yards = "-" if standby["yards"] is None else f"{standby['yards']:.1f}"
    text += f" {yards} yd off at the claim {rel_s(window, standby['t'])}"
    if standby["yards"] is not None and standby["yards"] > HOP_RANGE:
        text += f", past its {HOP_RANGE:.0f} yd reach"
    if standby["claimer"] != standby["guid"]:
        text += f", claimed by {trace.name(standby['claimer'])}"
    return text


def next_leg_text(trace: Trace, window: dict) -> str:
    """After an own immunity or a patch end, the spike's leg to the next kiter and the patch it passed."""
    leg = window["next_leg"]
    text = f"next leg after {'the patch' if leg['after'] == 'patch' else leg['after']}"
    later = leg["next"]
    if later is None:
        return text + ": no mark after it in the submerge"
    text += f" to {trace.name(later['guid'])}"
    row = leg["pass"]
    if row is None:
        return text + f": no patch within {SPIKE_PATCH_REACH} yd"
    if row["yards"] is None:
        return text + f": {patch_label(trace, row)} spent {rel_s(later, row['t'])} into its mark"
    return (text + f": {patch_label(trace, row)} {row['yards']:.1f} yd {rel_s(later, row['t'])} into its mark"
            + (", spent" if row["spent"] else ""))


def rework_lines(trace: Trace, window: dict) -> list[str]:
    """One kite's escort at the claim and its swaps, claim to cast, the kiter near a tank patch under a Hand, an
    own immunity after a Hand, a boost early in a Hand, a boost after the Hand, a turn close to a boost, the
    escort waiting by the patch, and anub.second grants."""
    lines = []
    escort = window["escort_read"]
    if escort:
        yards = "-" if escort["yards"] is None else f"{escort['yards']:.1f}"
        at = "no claim" if escort["t"] is None else f"{yards} yd off at the claim {rel_s(window, escort['t'])}"
        text = f"escort {trace.name(escort['guid'])} {at}"
        if escort["yards"] is not None and escort["yards"] > HOP_RANGE:
            text += f", past its {HOP_RANGE:.0f} yd reach"
        if escort["swaps"]:
            text += f"; walk and dodge swapped {escort['swaps']} time(s) within {ALTERNATE_MS / 1000:.0f} s"
        if escort["boost_yd"] is not None:
            text += f"; {escort['boost_yd']:.1f} yd at the boost {rel_s(window, escort['boost'])}"
        if escort["end_yd"] is not None:
            text += f"; {escort['end_yd']:.1f} yd at the end"
        if escort["frost_s"]:
            text += f"; carried Permafrost {escort['frost_s']:.1f} s"
        lines.append(text)
    if own_unused_death(window):
        lines.append(f"died to {trace.name(window['killer'])} with its own immunity unused")
    casts = [f"{trace.name(rec.get('s'))} " + ("no hop row" if gap is None else f"{gap / 1000:.1f} s")
             for rec, gap in window["claim_casts"]]
    if casts:
        lines.append("claim to cast: " + ", ".join(casts))
    near = window["hand_patch"]
    if near:
        lines.append(f"under the Hand {near['yards']:.1f} yd from the side {near['side']} tank patch"
                     f" {rel_s(window, near['t'])}, inside {HAND_PATCH_CLEARANCE} yd")
    for rec, gap in window["own_after_hand"]:
        lines.append(f"{KITER_OWN[rec['sp']]} {gap / 1000:.1f} s after the Hand ran out")
    boosts = window["hand_boosts"]
    if boosts:
        lines.append(f"boost in a Hand's first {HAND_BOOST_MS // 1000} s: "
                     + ", ".join(f"{KITE_BOOSTS[rec['sp']]} {rel_s(window, rec['t'])}" for rec in boosts))
    if window["after_hand"]:
        lines.append("boost after the Hand: "
                     + ", ".join(f"{RUN_BOOSTS[rec['sp']]} {rel_s(window, rec['t'])}" for rec in window["after_hand"]))
    if window["boost_turn"]:
        lines.append(boost_turn_text(window))
    if window["patch_escort"]:
        lines.append(f"escort by the patch: {patch_escort_text(trace, window)}")
    for when in window["second"]:
        lines.append(f"anub.second grant {rel_s(window, when)}")
    if window["boost_plan"]:
        lines.append(f"boost against the plan: {boost_plan_text(window)}")
    if window["standby"]:
        lines.append(standby_text(trace, window))
    if window["next_leg"]:
        lines.append(next_leg_text(trace, window))
    return lines


def show_rework(trace: Trace, windows: list[dict], spent: list[dict]) -> None:
    def who(window: dict) -> str:
        return f"{trace.name(window['guid'])} {clock(window['start'])}"

    escorted =[window for window in windows if window["escort_read"] and window["escort_read"]["yards"] is not None]
    far = [window for window in escorted if window["escort_read"]["yards"] > HOP_RANGE]
    print(f"  escort within {HOP_RANGE:.0f} yd of the kiter at the claim: {len(escorted) - len(far)} of {len(escorted)}"
          + ("; further: " + ", ".join(f"{who(window)} {window['escort_read']['yards']:.1f} yd" for window in far)
             if far else ""))
    swapped = [window for window in windows if window["escort_read"] and window["escort_read"]["swaps"]]
    print(f"  escort walk and dodge swapped within {ALTERNATE_MS / 1000:.0f} s: "
          + (", ".join(f"{who(window)} {window['escort_read']['swaps']}" for window in swapped) or "none"))
    later = [(window, rec) for window in windows for rec in window["after_hand"]]
    print("  Hand of Protection with a boost the kiter cast later in its kite: "
          + (", ".join(f"{who(window)} {RUN_BOOSTS[rec['sp']]} {(rec['t'] - window['hop'][0]['t']) / 1000:.1f} s"
                       " after the Hand" for window, rec in later) or "none"))
    turned = [window for window in windows if window["boost_turn"]]
    print(f"  patch kites turned within {BOOST_TURN_MS // 1000} s of a boost going out: "
          + (", ".join(f"{who(window)} {boost_turn_text(window)}" for window in turned) or "none"))
    waited = [window for window in windows if window["patch_escort"]]
    print("  escort waiting by the patch (anub.escort patch), at the Hand or the kite's end: "
          + (", ".join(f"{who(window)} {patch_escort_text(trace, window)}" for window in waited) or "none"))
    frozen = [window for window in windows if window["escort_read"] and window["escort_read"]["frost_s"]]
    print("  escort carrying Permafrost in the kite: "
          + (", ".join(f"{who(window)} {window['escort_read']['frost_s']:.1f} s" for window in frozen) or "none"))
    unused = [window for window in windows if own_unused_death(window)]
    print("  kiter dead to anything but the spike with its own immunity unused: "
          + (", ".join(f"{who(window)} by {trace.name(window['killer'])}" for window in unused) or "none"))
    tools = spent_kite_tools(trace, windows)
    if is_heroic(trace):
        print("  kite tools a bot spent off its own mark, per phase: "
              + ("; ".join(f"{label} " + ", ".join(f"{trace.name(rec.get('s'))} {KITE_TOOLS[rec['sp']]} {clock(rec['t'])}"
                                                   for at, rec in tools if at == label)
                           for label in dict.fromkeys(at for at, _ in tools)) or "none"))
    gaps = [(window, rec, gap) for window in windows for rec, gap in window["claim_casts"] if gap is not None]
    slow = [(window, rec, gap) for window, rec, gap in gaps if gap > CLAIM_CAST_FLAG_MS]
    if gaps:
        print(f"  claim to cast, Hand of Protection: median {statistics.median(gap for _, _, gap in gaps) / 1000:.1f} s,"
              f" over {CLAIM_CAST_FLAG_MS / 1000:.1f} s: "
              + (", ".join(f"{trace.name(rec.get('s'))} for {who(window)} {gap / 1000:.1f} s" for window, rec, gap in slow)
                 or "none"))
    near = [window for window in windows if window["hand_patch"]]
    print(f"  kiter within {HAND_PATCH_CLEARANCE} yd of a tank patch under a Hand: "
          + (", ".join(f"{who(window)} {window['hand_patch']['yards']:.1f} yd" for window in near) or "none"))
    late = [(window, rec, gap) for window in windows for rec, gap in window["own_after_hand"]]
    print("  own immunity after a Hand ran out: "
          + (", ".join(f"{who(window)} {KITER_OWN[rec['sp']]} +{gap / 1000:.1f} s" for window, rec, gap in late)
             or "none"))
    boosted = [(window, rec) for window in windows for rec in window["hand_boosts"]]
    print(f"  boost in a Hand's first {HAND_BOOST_MS // 1000} s: "
          + (", ".join(f"{who(window)} {KITE_BOOSTS[rec['sp']]}" for window, rec in boosted) or "none"))
    granted = [(window, when) for window in windows for when in window["second"]]
    print("  anub.second grants: " + (", ".join(f"{who(window)} {rel_s(window, when)}" for window, when in granted)
                                      or "none"))
    planned = [window for window in windows if window["boost_plan"]]
    if planned:
        slips = [window for window in planned if boost_plan_slip(window)]
        late = ", ".join(f"{who(window)} {boost_plan_text(window)}" for window in slips)
        print(f"  first speed boost against its anub.kitepath pick's +boost@ second, on time within"
              f" {BOOST_ON_TIME_MS / 1000:.1f} s: {len(planned) - len(slips)} of {len(planned)}"
              + (f"; else: {late}" if late else ""))
    else:
        print("  first speed boost against its anub.kitepath pick's +boost@ second: no +boost@ picks")
    standbys = [window for window in windows if window["standby"]]
    claimed = [window for window in standbys if window["standby"]["yards"] is not None]
    far = [window for window in claimed if window["standby"]["yards"] > HOP_RANGE]
    unclaimed = [window for window in standbys if window["standby"]["t"] is None]
    if standbys:
        print(f"  anub.escort standby within {HOP_RANGE:.0f} yd of the kiter at the claim: {len(claimed) - len(far)}"
              f" of {len(claimed)}" + ("; further: " + ", ".join(f"{who(window)} {standby_text(trace, window)}"
                                                                 for window in far) if far else "")
              + ("; no claim: " + ", ".join(who(window) for window in unclaimed) if unclaimed else ""))
    else:
        print(f"  anub.escort standby within {HOP_RANGE:.0f} yd of the kiter at the claim: no standby rows")
    legs = [window for window in windows if window["next_leg"] and window["next_leg"]["next"] is not None]
    passed = [window for window in legs if window["next_leg"]["pass"]]
    print(f"  the spike's next leg after an own immunity or a patch end, from the next mark to the spike within"
          f" {KITE_PROTECT_RANGE:.0f} yd\n  of its kiter or of its spot at the mark, past a patch: {len(passed)} of"
          f" {len(legs)}"
          + (": " + ", ".join(f"{who(window)} {next_leg_text(trace, window)}" for window in passed) if passed else ""))
    rows = submerge_kites(trace, windows, spent)
    if rows:
        print("  per submerge, the seconds kited and idle from the first mark to the spike's end, the terminals the"
              "\n  kites ended on, the seconds the spike lived on after the first tank patch went, and kiters dead:")
        for row in rows:
            after = "no tank patch gone" if row["after"] is None else f"{row['after'] / 1000:.1f} s after the first tank patch"
            print(f"    {row['label']:5} {clock(row['start']):>9}  kite {row['kite'] / 1000:.1f} s, idle"
                  f" {row['idle'] / 1000:.1f} s; " + ", ".join(f"{name} {n}" for name, n in row["terminals"].most_common())
                  + f"; {after}; {row['deaths']} kiter death(s)")


def unasked(window: dict, rec: dict) -> bool:
    """An own immunity with no `anub.protect` self from the kiter in the 2 s before it."""
    return not any(str(note.get("txt")) == "self" and note.get("g") == window["guid"]
                   and 0 <= rec["t"] - note["t"] <= LAST_RESORT_LEAD_MS for note in window["protect"])


def protection(trace: Trace, window: dict) -> str:
    """Hand of Protection on the kiter, its own immunities and the `anub.protect` duties in one window."""
    def rel(rec) -> str:
        return f"{(rec['t'] - window['start']) / 1000:+.1f} s"

    parts = [f"Hand of Protection by {trace.name(rec.get('s'))} {rel(rec)}" for rec in window["hop"]]
    parts += [f"{KITER_OWN[rec['sp']]} {rel(rec)}" + (" unasked" if unasked(window, rec) else "")
              for rec in window["own"]]
    parts += [f"anub.protect {trace.name(rec.get('g'))} {rec.get('txt')} {rel(rec)}"
              for rec in window["protect"] if str(rec.get("txt")) != "none"]
    return "; ".join(parts)


def plan_missing(terminal: str, window: dict, spend: dict | None) -> list[str] | None:
    """What a kite's terminal still lacked when its mark ended, empty when it was used: a tank patch the
    spike spent for `patch`, Hand of Protection on the kiter for `hand`, both for `patch+hand`, the kiter's
    own immunity for `own`. None for `none`, which has nothing to use."""
    if terminal not in PLAN_NEEDS:
        return None
    have = {"patch": spend is not None, "hand": bool(window["hop"]), "own": bool(window["own"])}
    names = {"patch": "no tank patch spent", "hand": "no Hand of Protection", "own": "no own immunity"}
    return [names[need] for need in PLAN_NEEDS[terminal] if not have[need]]


def plan_line(window: dict, spend: dict | None) -> str:
    """A kite's anub.kiteplan terminals and whether the last one was used before the mark ended."""
    if not window["plan"]:
        return ""
    missing = plan_missing(window["plan"][-1], window, spend)
    if missing is None:
        verdict = "nothing to use"
    else:
        verdict = "unused, " + ", ".join(missing) if missing else "used"
    return f"kiteplan {'>'.join(window['plan'])}: {verdict}"


def show_plans(trace: Trace, windows: list[dict], spends: dict) -> None:
    planned = [(window, plan_missing(window["plan"][-1], window, spends.get((window["guid"], window["start"]))))
               for window in windows if window["plan"]]
    if not planned:
        print("  no anub.kiteplan rows")
        return
    tally: dict[str, list[int]] = {}
    for window, missing in planned:
        counts = tally.setdefault(window["plan"][-1], [0, 0])
        counts[0] += 1
        counts[1] += missing == []
    print("  anub.kiteplan terminal at the mark's end, used before it ended: " + ", ".join(
        f"{terminal} {used} of {n}" if terminal in PLAN_NEEDS else f"{terminal} {n}"
        for terminal, (n, used) in sorted(tally.items(), key=lambda item: -item[1][0])))
    dead = [window for window, missing in planned if missing and window["outcome"] == "death"]
    print("  kiter dead with its terminal unused: " + (", ".join(
        f"{trace.name(window['guid'])} {clock(window['start'])} {window['plan'][-1]}" for window in dead) or "none"))


def show_turns(trace: Trace, windows: list[dict]) -> None:
    def who(window: dict) -> str:
        return f"{trace.name(window['guid'])} {clock(window['start'])}"

    kites = [window for window in windows if window["patch_kite"]]
    turned = [window for window in kites if window["turn"] is not None]
    failed = sum(1 for window in turned if window["turned_for"] and window["turned_for"]["failed"])
    moves = collections.Counter(window["moves"][0] if window["moves"] else "-" for window in turned)
    inside = sum(1 for window in turned
                 if window["turned_for"] and window["turned_for"]["kiter"] <= PERMAFROST_SLOW_REACH)
    print(f"  patch kites turned for a patch: {len(turned)} of {len(kites)}, the spike failed on it in {failed},"
          f" the kiter inside its {PERMAFROST_SLOW_REACH} yd slow after the turn in {inside}; anub.kitemove at the"
          " turn: " + (", ".join(f"{move} {n}" for move, n in moves.most_common()) or "none")
          + ("" if notes(trace, "anub.kitemove") else ", no anub.kitemove rows"))
    verdicts: dict[str, list[dict]] = collections.defaultdict(list)
    for window in windows:
        verdict = bridge_verdict(window)
        if verdict:
            verdicts[verdict].append(window)
    print("  patch+hand kites at the turn: " + ("; ".join(
        f"{BRIDGE_TEXT[verdict]} {len(rows)}" + ("" if verdict == "hand" else ": " + ", ".join(map(who, rows)))
        for verdict, rows in sorted(verdicts.items(), key=lambda item: -len(item[1]))) or "none")
          + ("" if notes(trace, "anub.bridge") else "; no anub.bridge rows, so read off Hand of Protection casts"))
    passed = [window for window in windows if window["first_pass"]]
    print(f"  spike within {SPIKE_ROUTE_CLEARANCE} yd of a patch, its route clearance, before the turn or in a kite"
          " with none: " + (", ".join(
              f"{who(window)} +{(window['first_pass']['t'] - window['start']) / 1000:.1f} s" for window in passed)
              or "none"))
    hits = collections.Counter((window["guid"], window["start"], rec.get("d"), side)
                               for window in windows for rec, side in window["side_impales"])
    print("  Impale on a side tank: " + (", ".join(
        f"{trace.name(tank)} (side {side}) {n} in {trace.name(kiter)}'s kite {clock(start)}"
        for (kiter, start, tank, side), n in hits.items()) or "none"))


def show_catches(trace: Trace, windows: list[dict], spent: list[dict]) -> None:
    def who(window: dict) -> str:
        return f"{trace.name(window['guid'])} {clock(window['start'])}"

    hands = [(window, row) for window in windows for row in window["catch"]]
    print("  patch+hand kites' Hand against the catch predicted as it went out: " + (", ".join(
        f"{who(window)} " + ("no catch predicted" if row["before"] is None else f"{row['before']:.1f} s before")
        + (", patch under it" if row["under"] else "") for window, row in hands) or "none"))
    plans = {(window["guid"], window["start"]): window["plan"] for window in windows}
    parts = []
    for first in first_spends(trace, spent):
        row = first["row"]
        if row is None:
            parts.append(f"{first['label']} none")
            continue
        kite = "nobody marked"
        if row["kiter"] is not None:
            kite = f"{trace.name(row['kiter'])} {'>'.join(plans.get((row['kiter'], row['mark'])) or ['-'])}"
        parts.append(f"{first['label']} {(row['cleared'] - first['start']) / 1000:.1f} side {row['side']} {kite}")
    if parts:
        print("  first tank patch gone per submerge, seconds into phase 2 at its anub.patchN clear, its side, kiter"
              " and kiteplan:\n    " + ", ".join(parts))


def cover_name(rec: dict) -> str:
    return KITER_OWN.get(rec.get("sp")) or "Hand of Protection"


def speed_line(window: dict) -> str:
    """The spike's speed and the kiter's at each cover in one window, a spike no faster flagged."""
    def speed(value) -> str:
        return "-" if value is None else f"{value:.1f} yd/s"

    return "; ".join(f"{cover_name(row['rec'])} {(row['rec']['t'] - window['start']) / 1000:+.1f} s: spike"
                     f" {speed(row['spike'])}, kiter {speed(row['kiter'])}" + (", no faster" if row["slow"] else "")
                     for row in window["cover"])


def ready_brief(trace: Trace, escort: dict) -> str:
    """A `ready_paladin` read in a few words."""
    if escort["forbearance"]:
        return "Forbearance on the kiter"
    if escort["guid"] is None:
        return "none ready"
    return f"{trace.name(escort['guid'])} {escort['yards']:.1f} yd"


def escort_brief(trace: Trace, window: dict) -> str:
    """The nearest Hand of Protection-ready paladin as the spike first came within KITE_PROTECT_RANGE."""
    return ready_brief(trace, window["escort"])


def catch_brief(row: dict, window: dict) -> str:
    """One Hand against its predicted catch, and what followed under it."""
    text = (f"no catch predicted within {CATCH_HORIZON_S:.0f} s" if row["before"] is None
            else f"{row['before']:.1f} s before the predicted catch")
    if row["yards"] is not None:
        text += f", spike {row['yards']:.1f} yd off" + ("" if row["spike"] is None else f" at {row['spike']:.1f} yd/s")
    if row["under"]:
        return text + f"; the patch followed under it, {(row['end'] - window['stop']) / 1000:.1f} s of it left"
    if window["consumed"] and window["stop"] > row["end"]:
        return text + "; the patch only after it ran out"
    return text + f"; no patch under it, {window['outcome']}"


def catch_line(trace: Trace, window: dict) -> str:
    """A patch+hand kite's Hand of Protection against the catch predicted as it went out."""
    return "; ".join(f"Hand against the catch, by {trace.name(row['rec'].get('s'))}"
                     f" {(row['rec']['t'] - window['start']) / 1000:+.1f} s: {catch_brief(row, window)}"
                     for row in window["catch"])


def fallback_line(trace: Trace, window: dict) -> str:
    """Who could have given an anub.bridge turn its Hand when it turned without one."""
    escort = window["turn_escort"]
    if escort is None:
        return ""
    far = escort["guid"] is not None and escort["yards"] > HOP_RANGE
    return (f"no Hand at the turn, nearest Hand of Protection-ready paladin then: {ready_brief(trace, escort)}"
            + (f", past its {HOP_RANGE:.0f} yd reach" if far else ""))


def first_spends(trace: Trace, spent: list[dict]) -> list[dict]:
    """Per submerge, the first latched tank patch that went while he was down, by its anub.patchN clear; `row`
    None for a submerge that spent none."""
    return [{"label": label, "start": start,
             "row": min((row for row in spent if row["start"] == start), key=lambda row: row["cleared"], default=None)}
            for start, _, phase, label in phase_spans(trace) if phase == 2]


def escort_line(trace: Trace, window: dict) -> str:
    if window["escort"] is None:
        return ""
    escort = window["escort"]
    far = escort["guid"] is not None and escort["yards"] > HOP_RANGE
    return (f"spike within {KITE_PROTECT_RANGE:.0f} yd +{window['near'] / 1000:.1f} s, nearest"
            f" Hand of Protection-ready paladin: {escort_brief(trace, window)}"
            + (f", past its {HOP_RANGE:.0f} yd reach" if far else ""))


def show_crowds(trace: Trace, samples: Samples) -> None:
    rows = submerge_crowds(trace, samples)
    if not rows:
        return
    print(f"\n  per submerge, the living players within {', '.join(f'{yards:.0f}' for yards in CROWD_YD)} yd of where"
          " he went down, by role, his distance\n  from the gather, and the first kiter's distance from the spike"
          " at its mark:")
    for row in rows:
        rings = "; ".join(f"{yards:.0f} yd {sum(counts.values())}"
                          + (" (" + ", ".join(f"{role} {counts[role]}" for role in CROWD_ROLES if counts[role]) + ")"
                             if counts else "")
                          for yards, counts in row["near"].items())
        first = "no mark"
        if row["kiter"] is not None:
            gap = "spike unsampled" if row["spike_yd"] is None else f"{row['spike_yd']:.1f} yd from the spike"
            first = f"first mark {trace.name(row['kiter'])} {clock(row['mark'])}, {gap}"
        gather = "" if row["gather_yd"] is None else f", {row['gather_yd']:.1f} yd from the gather"
        print(f"    {row['label']:5} {clock(row['start']):>9} at {row['spot'][0]:.1f}, {row['spot'][1]:.1f}: {rings}"
              f"\n          {first}{gather}")


def dodge_swaps(trace: Trace) -> list[dict]:
    """Per submerge, each bot's gather and dodge move rows: `swaps`, the row switching from one to the other
    within DODGE_SWAP_MS of the row before, and `walks`, a dodge row more than DODGE_SPOT_YD off the bot's
    last dodge spot that submerge, or its first."""
    rows: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("by") in (GATHER_BY, DODGE_BY):
            rows[rec["g"]].append(rec)
    out = []
    for start, stop, phase, label in phase_spans(trace):
        if phase != 2:
            continue
        swaps: collections.Counter = collections.Counter()
        walks: collections.Counter = collections.Counter()
        for guid, mine in rows.items():
            inside = [rec for rec in mine if start <= rec["t"] < stop]
            for one, two in zip(inside, inside[1:]):
                if one["by"] != two["by"] and two["t"] - one["t"] <= DODGE_SWAP_MS:
                    swaps[guid] += 1
            spot = None
            for rec in inside:
                if rec["by"] != DODGE_BY:
                    continue
                here = (rec["x"], rec["y"])
                if spot is None or dist2(spot, here) > DODGE_SPOT_YD:
                    walks[guid] += 1
                spot = here
        out.append({"label": label, "start": start, "swaps": swaps, "walks": walks})
    return out


def show_dodges(trace: Trace) -> None:
    rows = dodge_swaps(trace)
    if not rows:
        return
    print(f"\n  per submerge, the bystanders' {GATHER_BY} and {DODGE_BY} move rows: swaps from one to the other"
          f" within\n  {DODGE_SWAP_MS / 1000:.1f} s of the row before, and dodge walks, a dodge row over"
          f" {DODGE_SPOT_YD:.0f} yd off the bot's last dodge spot:")
    for row in rows:
        top = ", ".join(f"{trace.name(guid)} {n}" for guid, n in row["swaps"].most_common(DODGE_SWAPPERS_SHOWN))
        walks = ", ".join(f"{trace.name(guid)} {n}" for guid, n in row["walks"].most_common())
        print(f"    {row['label']:5} {clock(row['start']):>9} swaps {sum(row['swaps'].values())}"
              + (f": {top}" if top else "")
              + f"\n          dodge walks {sum(row['walks'].values())}" + (f": {walks}" if walks else ""))


def tank_line(window: dict) -> str:
    """The spike's closest approach to each latched tank patch in one window."""
    return ", ".join(f"side {side} {yards:.1f} yd" + (" in reach" if yards < SPIKE_PATCH_REACH else "")
                     for side, yards in sorted(window["tank_near"].items()))


def end_line(trace: Trace, window: dict) -> str:
    """Where the mark's end left the spike against the gather, the kite patches alive then, and what ended it."""
    if window["gather"] is None:
        parts = ["nobody holding the gather"]
    elif window["to_gather"] is None:
        parts = ["spike unsampled"]
    else:
        parts = [f"spike {window['to_gather']:.1f} yd from the gather"]
    patches = ", ".join(short(trace, guid) for guid in window["kite_patches"])
    parts.append(f"kite patch {patches}" if patches else "no kite patch")
    if window["ended"]:
        parts.append(f"ended by {window['ended']}"
                     + (f" within {KITE_END_GATHER_YD:.0f} yd of the gather" if window["near_gather"] else ""))
    return "; ".join(parts)


def kite_line(trace: Trace, window: dict) -> str:
    """Time in each kite stage and the kiter's boosts in one window."""
    stages = ", ".join(f"{stage_name(trace, value)} {ms / 1000:.1f} s" for value, ms in window["stages"].items())
    boosts = ", ".join(f"{KITE_BOOSTS[rec['sp']]} {(rec['t'] - window['start']) / 1000:+.1f} s"
                       for rec in window["boosts"])
    return "; ".join(part for part in (stages and f"kitestage {stages}", boosts and f"boosts {boosts}") if part)


def patch_label(trace: Trace, patch: dict) -> str:
    kind = "kite patch" if patch["side"] is None else f"side {patch['side']} patch"
    return f"{kind} {short(trace, patch['guid'])} ({patch['spot'][0]:.1f},{patch['spot'][1]:.1f})"


def turn_line(trace: Trace, window: dict) -> str:
    """A patch kite's turn: when and what says so, its anub.kitemove, the patch it went for, and whether the
    spike failed on it."""
    if not window["patch_kite"]:
        return ""
    if window["turn"] is None:
        return "never turned for a patch"
    text = (f"turned +{(window['turn'] - window['start']) / 1000:.1f} s ({window['turn_by']}),"
            f" kitemove {'>'.join(window['moves']) or '-'}")
    patch = window["turned_for"]
    if patch is None:
        return text + "; no patch alive then"
    spike = "unsampled" if patch["spike"] is None else f"{patch['spike']:.1f} yd"
    return (text + f"; {patch_label(trace, patch)}, closest after the turn: kiter {patch['kiter']:.1f} yd,"
            f" spike {spike}; " + ("the spike failed on it" if patch["failed"] else f"unspent, {window['outcome']}"))


def bridge_line(trace: Trace, window: dict) -> str:
    """How a patch+hand kite's turn went, with each Hand of Protection on the kiter against the turn. The turn
    line already says a kite never turned."""
    verdict = bridge_verdict(window)
    if verdict in (None, "never"):
        return ""
    source = "anub.bridge" if window["bridge"] else "Hand of Protection casts"
    text = f"patch+hand: {BRIDGE_TEXT[verdict]}, read off {source}"
    if window["turn"] is None or not window["hop"]:
        return text
    # adding 0.0 turns a -0.0 that rounding leaves into 0.0
    hands = ", ".join(f"by {trace.name(rec.get('s'))} {round((rec['t'] - window['turn']) / 1000, 1) + 0.0:+.1f} s"
                      for rec in window["hop"])
    return f"{text}; Hand of Protection {hands} from the turn"


def pass_line(trace: Trace, window: dict) -> str:
    """The first snapshot before the turn with the spike within SPIKE_ROUTE_CLEARANCE of a patch."""
    row = window["first_pass"]
    if row is None:
        return ""
    when = "before the turn" if window["turn"] is not None else "never turned" if window["patch_kite"] else ""
    return (f"spike within {SPIKE_ROUTE_CLEARANCE} yd of {patch_label(trace, row)}: {row['yards']:.1f} yd"
            f" +{(row['t'] - window['start']) / 1000:.1f} s" + (f", {when}" if when else ""))


def side_impale_line(trace: Trace, window: dict) -> str:
    hits: dict[tuple[int, int], list[dict]] = collections.defaultdict(list)
    for rec, side in window["side_impales"]:
        hits[(rec.get("d"), side)].append(rec)
    return "; ".join(f"Impale on side {side} tank {trace.name(guid)}: {len(recs)} hit(s) for"
                     f" {sum(rec.get('a', 0) for rec in recs):,} from +{(recs[0]['t'] - window['start']) / 1000:.1f} s"
                     for (guid, side), recs in hits.items())


# -------------------------------------------------------------------------------------------- spheres

def sphere_timeline(samples: Samples) -> list[tuple[int, int, int, int]]:
    """`(t, flying, patches, permafrost)` at every snapshot where one of the three changed."""
    out: list[tuple[int, int, int, int]] = []
    for when, counts in zip(samples.stamps, samples.spheres):
        if not out or out[-1][1:] != counts:
            out.append((when, *counts))
    return out


def flying_at(samples: Samples, when: int) -> int | None:
    index = samples.index_at(when)
    return samples.spheres[index][0] if index >= 0 else None


def spheres_left(trace: Trace, samples: Samples) -> list[tuple[str, int, int | None]]:
    """Flying spheres at each submerge, at phase 3 and at the end: heroic never replaces one."""
    out = [(label, start, flying_at(samples, start))
           for start, _, phase, label in phase_spans(trace) if phase == 2 or label == "P3"]
    if samples.stamps:
        out.append(("end", samples.stamps[-1], samples.spheres[-1][0]))
    return out


def sphere_shots(trace: Trace) -> collections.Counter:
    """Casts per caster aimed at a sphere."""
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    return collections.Counter(rec.get("s") for rec in trace.of("cast")
                               if rec.get("tgt") in spheres and not rec.get("tr"))


def supply_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per mark window, the spheres at the mark: patches split by whether a side latched them,
    flying and falling ones, and every bot that held a sphere duty during the window."""
    samples = samples or Samples(trace)
    latches = patch_latches(trace)
    duty = holder_spans(trace, "anub.sphere", pull_end(trace))
    out = []
    for window in mark_windows(trace, samples):
        start, stop = window["start"], window["stop"]
        kinds = samples.sphere_kinds(start)
        tank = {as_guid(held_at(spans, start)) for spans in latches} - {0}
        patches = [guid for guid, kind in kinds.items() if kind == "patch"]
        holders = sorted({(guid, label) for guid, spans in duty.items() for label, low, high in spans
                          if label != "none" and low < stop and high > start})
        out.append({"guid": window["guid"], "start": start, "outcome": window["outcome"],
                    "tank": sum(1 for guid in patches if guid in tank),
                    "other": sum(1 for guid in patches if guid not in tank),
                    "flying": sum(1 for kind in kinds.values() if kind == "flying"),
                    "falling": sum(1 for kind in kinds.values() if kind == "falling"),
                    "holders": holders})
    return out


def duty_spans(trace: Trace) -> list[dict]:
    """Each stretch one bot held one `anub.sphere` duty, with its casts at a sphere and its `drop target`
    rows meanwhile. The note names the duty, not the sphere, so a cast at any sphere counts."""
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    casts = [rec for rec in trace.of("cast") if rec.get("tgt") in spheres and not rec.get("tr")]
    drops = [rec for rec in trace.of("act") if rec.get("a") == "drop target"]
    out = []
    for guid, spans in holder_spans(trace, "anub.sphere", pull_end(trace)).items():
        for label, start, stop in spans:
            if label == "none":
                continue
            out.append({"guid": guid, "label": label, "start": start, "stop": stop,
                        "casts": sum(1 for rec in casts if rec.get("s") == guid and start <= rec["t"] < stop),
                        "drops": sum(1 for rec in drops if rec.get("g") == guid and start <= rec["t"] < stop)})
    return sorted(out, key=lambda row: (row["start"], row["guid"]))


def patch_formed(samples: Samples, guid: int) -> int | None:
    """When a Permafrost centre first sat on the sphere."""
    stamps, rows = samples.track(guid)
    for when, row in zip(stamps, rows):
        if row[3] < SPHERE_PATCH_Z and any(dist2(row[1:3], centre) <= PATCH_MATCH_YD
                                           for centre in samples.frost_at(when)):
            return when
    return None


def kill_time(samples: Samples, guid: int) -> tuple[int, list] | None:
    """When the sphere died and the row it lay on: PERMAFROST_DELAY_MS before its patch formed, else the
    first row of the low run the trace ended it in. None for one still flying."""
    formed = patch_formed(samples, guid)
    if formed is not None:
        return formed - PERMAFROST_DELAY_MS, samples.row(guid, formed)
    stamps, rows = samples.track(guid)
    if not rows or rows[-1][3] > SPHERE_FLYING_Z:
        return None
    index = len(rows) - 1
    while index and rows[index - 1][3] <= SPHERE_FLYING_Z:
        index -= 1
    return stamps[index], rows[-1]


def duty_side(label: str | None) -> int | None:
    """The side an `anub.sphere` duty serves: `0` and `grip0` side 0, `1` and `grip1` side 1."""
    side = (label or "").removeprefix("grip")
    return int(side) if side in ("0", "1") else None


def spell_shape(spell) -> tuple[str, ...]:
    return tuple(name for name, ids in (("periodic", PERIODIC), ("area", AREA)) if spell in ids)


def sphere_deaths(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each sphere that died: when, what killed it, where it landed, and how far that is from its side's
    spot at the kill. Its side is the latch that named it, else the killer's sphere duty at the blow."""
    samples = samples or Samples(trace)
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    latches = patch_latches(trace)
    noted = spot_spans(trace)
    duty = holder_spans(trace, "anub.sphere", pull_end(trace))
    hits = [rec for rec in trace.of("dmg") if rec.get("d") in spheres]
    # a triggered cast is a proc or a debuff, but Starfall's stars and Explosive Shot's ticks come in
    # triggered too
    casts = [rec for rec in trace.of("cast") if rec.get("tgt") in spheres and rec.get("sp") not in NO_DAMAGE
             and (not rec.get("tr") or spell_shape(rec.get("sp")))]
    out = []
    for guid in sorted(spheres):
        death = kill_time(samples, guid)
        if death is None:
            continue
        killed, row = death
        source = "dmg"
        mine = [rec for rec in hits if rec["d"] == guid and rec["t"] <= killed]
        if not mine:
            source = "cast"
            mine = [rec for rec in casts if rec["tgt"] == guid and rec["t"] <= killed]
        blow = mine[-1] if mine else None
        caster = blow.get("s") if blow else None
        killer = trace.owners.get(caster, caster)
        held = held_at(duty.get(killer, []), blow["t"]) if blow else None
        named = [side for side, spans in enumerate(latches) if any(as_guid(value) == guid for value, _, _ in spans)]
        side = named[0] if named else duty_side(held)
        spot = None if side is None else side_spot(latches, samples, side, killed, noted)
        landed = (row[1], row[2])
        out.append({"guid": guid, "killed": killed, "landed": landed,
                    "killer": killer, "pet": caster if killer != caster else None,
                    "spell": blow.get("sp") if blow else None, "source": source if blow else None,
                    "shape": spell_shape(blow.get("sp")) if blow else (),
                    "duty": held, "side": side, "latched": bool(named),
                    "spot": spot, "off_spot": None if spot is None else dist2(landed, spot)})
    return sorted(out, key=lambda row: row["killed"])


def grip_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each Death Grip at a sphere, and how long after it the sphere first sat within GRIP_LANDED yd and
    GRIP_LANDED_HEIGHT of where the DK stood at the cast, inside GRIP_JUDGE_MS: the grip pulls it to that
    spot, wherever the DK walks after. `closest` is its nearest ground gap meanwhile. `served` is the side
    its grip duty was for, `latched` the side whose `anub.patchN` later named it. `step` is the DK's first
    `anub.gripstep` of `kill` or `none` from the cast to its next grip and `step_ms` how long after the cast:
    `kill` held the duty to the kill, `none` dropped it. `rose` is when the sphere was first back above
    SPHERE_FLYING_Z after it came down, before the next grip at it. `pulled` is False when the cast
    triggered no SPELL_DEATH_GRIP_PULL: it missed, and the sphere stayed where it was."""
    samples = samples or Samples(trace)
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    duty = holder_spans(trace, "anub.sphere", pull_end(trace))
    steps = per_bot(trace, "anub.gripstep")
    latches = patch_latches(trace)
    casts = [rec for rec in trace.of("cast")
             if rec.get("sp") == SPELL_DEATH_GRIP and rec.get("tgt") in spheres and not rec.get("tr")]
    pulls = grip_pulls(trace)
    out = []
    for index, rec in enumerate(casts):
        dk, sphere, start = rec.get("s"), rec["tgt"], rec["t"]
        upto = next((other["t"] for other in casts[index + 1:] if other.get("s") == dk), None)
        again = next((other["t"] for other in casts[index + 1:] if other["tgt"] == sphere), None)
        step = next(((value, when - start) for when, value in steps.get(dk, [])
                     if when >= start and (upto is None or when < upto) and value in ("kill", "none")), (None, None))
        landed = closest = None
        stand = samples.recent(dk, start)
        stamps, rows = samples.track(sphere)
        judged = range(bisect.bisect_left(stamps, start), bisect.bisect_right(stamps, start + GRIP_JUDGE_MS))
        for index in judged if stand else ():
            gap = dist2(rows[index][1:3], stand[1:3])
            closest = gap if closest is None else min(closest, gap)
            if gap <= GRIP_LANDED and abs(rows[index][3] - stand[3]) <= GRIP_LANDED_HEIGHT:
                landed = stamps[index] - start
                break
        label = held_at(duty.get(dk, []), start)
        served = int(label[-1]) if label in ("grip0", "grip1") else None
        latched = next((side for side in (0, 1) for value, begin, _ in latches[side]
                        if begin >= start and as_guid(value) == sphere), None)
        out.append({"t": start, "dk": dk, "sphere": sphere, "duty": label, "landed": landed, "closest": closest,
                    "cast_at": (stand[1], stand[2]) if stand else None, "served": served, "latched": latched,
                    "step": step[0], "step_ms": step[1], "rose": back_up(samples, sphere, start, again),
                    "pulled": pulled(pulls, rec)})
    return out


def grip_pulls(trace: Trace) -> dict[int, list[int]]:
    """Per caster, the sorted stamps of its triggered SPELL_DEATH_GRIP_PULL casts."""
    out: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("sp") == SPELL_DEATH_GRIP_PULL and rec.get("tr"):
            out[rec.get("s")].append(rec["t"])
    for stamps in out.values():
        stamps.sort()
    return out


def pulled(pulls: dict[int, list[int]], cast: dict) -> bool:
    """Whether the Death Grip cast row triggered its pull within GRIP_PULL_MS."""
    stamps = pulls.get(cast.get("s"), [])
    index = bisect.bisect_left(stamps, cast["t"] - GRIP_PULL_MS)
    return index < len(stamps) and stamps[index] <= cast["t"] + GRIP_PULL_MS


def grip_misses(trace: Trace) -> dict[int, tuple[int, int]]:
    """Per caster, its Death Grip casts at anything and how many of them triggered no pull."""
    pulls = grip_pulls(trace)
    out: dict[int, tuple[int, int]] = {}
    for rec in trace.of("cast"):
        if rec.get("sp") == SPELL_DEATH_GRIP and not rec.get("tr"):
            total, missed = out.get(rec.get("s"), (0, 0))
            out[rec.get("s")] = (total + 1, missed + (not pulled(pulls, rec)))
    return out


def back_up(samples: Samples, sphere: int, start: int, until: int | None = None) -> int | None:
    """Milliseconds from `start` to the sphere's first sample back above SPHERE_FLYING_Z once it had come
    below it, before `until`. A killed sphere falls to a patch and never rises."""
    stamps, rows = samples.track(sphere)
    low = False
    for index in range(bisect.bisect_left(stamps, start), len(stamps)):
        if until is not None and stamps[index] >= until:
            break
        if rows[index][3] <= SPHERE_FLYING_Z:
            low = True
        elif low:
            return stamps[index] - start
    return None


def last_grip(grips: list[dict], sphere: int, before: int, since: int | None = None) -> dict | None:
    """The last Death Grip at the sphere cast at or before `before`, and not before `since`."""
    return next((row for row in reversed(grips) if row["sphere"] == sphere and row["t"] <= before
                 and (since is None or row["t"] >= since)), None)


def far_grips(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each sphere whose first Death Grip was cast more than GRIP_RELAY_FROM off its side's spot, the grip duty's
    side else the side that latched it: every grip at it in turn as a link, each with the sphere's distance from
    that side's spot at the cast (`before`) and where the grip brought it down (`after`), its landing sample or
    else its last inside GRIP_JUDGE_MS, both against the spot at the cast; then its death row."""
    samples = samples or Samples(trace)
    latches = patch_latches(trace)
    noted = spot_spans(trace)
    deaths = {row["guid"]: row for row in sphere_deaths(trace, samples)}
    by_sphere: dict[int, list[dict]] = collections.defaultdict(list)
    for row in grip_rows(trace, samples):
        by_sphere[row["sphere"]].append(row)
    out = []
    for sphere, grips in by_sphere.items():
        side = next((row["served"] for row in grips if row["served"] is not None),
                    next((row["latched"] for row in grips if row["latched"] is not None), None))
        first = grips[0]
        if side is None or first["cast_at"] is None:
            continue
        if dist2(first["cast_at"], side_spot(latches, samples, side, first["t"], noted)) <= GRIP_RELAY_FROM:
            continue
        links = []
        for row in grips:
            spot = side_spot(latches, samples, side, row["t"], noted)
            at = samples.recent(sphere, row["t"])
            down = samples.row(sphere, row["t"] + (GRIP_JUDGE_MS if row["landed"] is None else row["landed"]))
            links.append({**row, "spot": spot, "before": dist2(at[1:3], spot) if at else None,
                          "after": dist2(down[1:3], spot) if down else None})
        out.append({"sphere": sphere, "side": side, "links": links, "death": deaths.get(sphere)})
    return sorted(out, key=lambda row: row["links"][0]["t"])


def first_patches(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per side, the first sphere its `anub.patchN` latched: when, the last Death Grip at it before then and its
    death row. `guid` is None for a side that never latched one."""
    samples = samples or Samples(trace)
    grips = grip_rows(trace, samples)
    deaths = {row["guid"]: row for row in sphere_deaths(trace, samples)}
    out = []
    for side, spans in enumerate(patch_latches(trace)):
        when, guid = next(((start, as_guid(value)) for value, start, _ in spans if as_guid(value)), (None, None))
        out.append({"side": side, "t": when, "guid": guid,
                    "grip": last_grip(grips, guid, when) if guid else None, "death": deaths.get(guid)})
    return out


def spent_patches(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each tank patch a side held as it went while he was down: its side and submerge, its last sample, when
    the side's latch let it go in `cleared` (its last sample for a latch that never did), the mark that ended
    on it (else the one running then) with that kiter's last kite stage and the seconds into its window, and
    in `back` the side's next latch: when, a Death Grip at it since, its death row."""
    samples = samples or Samples(trace)
    end = pull_end(trace)
    latches = patch_latches(trace)
    lives = patch_lives(trace, samples)
    used = consumed_patches(trace, samples)
    last = samples.stamps[-1] if samples.stamps else 0
    spans = phase_spans(trace)
    marks = mark_spans(trace)
    stages = holder_spans(trace, "anub.kitestage", end)
    grips = grip_rows(trace, samples)
    deaths = {row["guid"]: row for row in sphere_deaths(trace, samples)}
    out = []
    for side, latch in enumerate(latches):
        for guid in dict.fromkeys(as_guid(value) for value, _, _ in latch):
            if not guid or guid not in lives or lives[guid][1] >= last:
                continue
            gone = lives[guid][1]
            # the latch clearing and the patch's last sample can land a tick apart, either way round
            held = next((stop for value, start, stop in latch
                         if as_guid(value) == guid and start <= gone < stop + NEAR_MS), None)
            if held is None:
                continue
            under = next(((start, stop, label) for start, stop, phase, label in spans
                          if phase == 2 and start <= gone < stop), None)
            if under is None:
                continue
            row = {"side": side, "guid": guid, "gone": gone, "cleared": held if held < end else gone,
                   "label": under[2], "start": under[0], "emerge": under[1], "kiter": None, "mark": None, "into": None,
                   "stage": None}
            if guid in used:
                hit = used[guid][1]
                mark = next((span for span in marks if span[3] and span[2] == hit), None)
            else:
                hit = gone
                mark = next((span for span in marks if span[1] <= gone <= span[2]), None)
            if mark:
                kiter, low, high, _ = mark
                held = [value for value, begin, stop in stages.get(kiter, [])
                        if value != "none" and begin < high and stop > low]
                row.update({"kiter": kiter, "mark": low, "into": hit - low, "stage": held[-1] if held else None})
            when, again = next(((start, as_guid(value)) for value, start, _ in latch
                                if start > gone and as_guid(value) not in (0, guid)), (None, None))
            row["back"] = None if again is None else {
                "t": when, "guid": again, "grip": last_grip(grips, again, when, gone), "death": deaths.get(again)}
            out.append(row)
    return sorted(out, key=lambda row: row["gone"])


def allowance_rows(trace: Trace, deaths: list[dict], spent: list[dict]) -> list[dict]:
    """Per submerge: the `anub.allow` written as he went under and when it next read 0 inside it, the spheres
    that died while he was down (`tank` the ones a side owned, `other` the rest), and the tank patches gone.
    `over` is heroic only, where the allowance is the tank patches the kiter may spend: 0 on a trace with no
    `anub.allow`, written before heroic kites stopped ending on tank patches."""
    spans = latch_spans(trace, "anub.allow", pull_end(trace))
    heroic = is_heroic(trace)
    out = []
    for start, stop, phase, label in phase_spans(trace):
        if phase != 2:
            continue
        # a row from another bot's tick can trail the phase edge
        value = held_at(spans, min(start + NEAR_MS, stop - 1))
        allow = int(value) if value and value.isdigit() else 0 if heroic and not spans else None
        dropped = next((low for held, low, _ in spans if held == "0" and start + NEAR_MS < low < stop),
                       None) if allow else None
        made = [row for row in deaths if start <= row["killed"] < stop]
        mine = [row for row in spent if row["start"] == start]
        out.append({"label": label, "start": start, "allow": allow, "dropped": dropped,
                    "tank": sum(1 for row in made if row["side"] is not None),
                    "other": sum(1 for row in made if row["side"] is None), "spent": mine,
                    "over": heroic and allow is not None and len(mine) > allow})
    return out


def patch_source(trace: Trace, grip: dict | None, death: dict | None) -> str:
    """How a patch came down: a Death Grip at it and whether it landed, else shots; then its killer and its
    landing against its side's spot."""
    text = "from shots"
    if grip:
        verdict = "missed" if grip["landed"] is None else f"landed +{grip['landed'] / 1000:.1f} s"
        text = f"from a grip by {trace.name(grip['dk'])} at {clock(grip['t'])}, {verdict}"
    if death and death["killer"]:
        text += (f", killed by {trace.name(death['killer'])}" + ("'s pet" if death["pet"] else "")
                 + ("" if death["spell"] is None else f" {trace.spell(death['spell'])}"))
    if death and death["off_spot"] is not None:
        text += f", {death['off_spot']:.1f} yd off its spot"
    return text


def spend_line(trace: Trace, row: dict) -> str:
    head = f"side {row['side']} {short(trace, row['guid'])} gone {clock(row['gone'])}"
    if row["kiter"] is None:
        return head + ", nobody marked"
    stage = stage_name(trace, row["stage"]) if row["stage"] else "-"
    return head + f", {trace.name(row['kiter'])}'s mark {row['into'] / 1000:.1f} s in, kitestage {stage}"


def back_line(trace: Trace, row: dict) -> str:
    back = row["back"]
    if back is None:
        return "not back by the end"
    after = (back["t"] - row["emerge"]) / 1000
    edge = f"{after:.1f} s after the emerge" if after >= 0 else f"{-after:.1f} s before the emerge"
    return (f"back {clock(back['t'])}, {edge}: {short(trace, back['guid'])}"
            f" {patch_source(trace, back['grip'], back['death'])}")


def show_tank_patches(trace: Trace, samples: Samples) -> None:
    print("\n  first tank patch per side, at its anub.patchN latch:")
    for row in first_patches(trace, samples):
        if row["guid"] is None:
            print(f"    side {row['side']}  never latched one")
        else:
            print(f"    side {row['side']}  {clock(row['t']):>9}  {short(trace, row['guid'])}"
                  f" {patch_source(trace, row['grip'], row['death'])}")

    rows = allowance_rows(trace, sphere_deaths(trace, samples), spent_patches(trace, samples))
    if not rows:
        return
    print("\n  per submerge: anub.allow as he went under, the spheres that died while he was down (tank the ones a"
          "\n  side owned, other the rest) and the latched tank patches gone meanwhile, over the allowance in heroic"
          " flagged:")
    for row in rows:
        allow = "-" if row["allow"] is None else str(row["allow"])
        print(f"    {row['label']:5} {clock(row['start']):>9}  allowance {allow:>2}  tank {row['tank']}"
              f"  other {row['other']}  spent {len(row['spent'])}" + ("  over" if row["over"] else "")
              + ("" if row["dropped"] is None else f", allowance 0 from {clock(row['dropped'])}"))
        for spend in row["spent"]:
            print(f"      {spend_line(trace, spend)}\n        {back_line(trace, spend)}")
    print(f"  a patch is gone at its last sample; its mark is the one that ended within {CONSUMED_MS / 1000:.0f} s"
          " of it, else the one\n  running then, with the seconds into it and its last kitestage; back is that"
          " side's next anub.patchN latch")


def show_deaths(trace: Trace, samples: Samples) -> None:
    deaths = sphere_deaths(trace, samples)
    if not deaths:
        print("  no sphere died")
        return
    print("\n  each sphere that died, killed by its last dmg row, else the last cast at it before the kill:")
    print(f"  {'sphere':8} {'died':>9} {'killer':18} {'spell':26} {'from':4} {'shape':13} {'side':10}"
          f" {'off spot':>8}")
    for row in deaths:
        killer = "-"
        if row["killer"]:
            killer = trace.name(row["killer"]) + ("'s pet" if row["pet"] else "")
        spell = "-" if row["spell"] is None else trace.spell(row["spell"])
        side = row["duty"] if row["duty"] not in (None, "none") else "-"
        if row["side"] is not None:
            side = f"{row['side']}" + (" latched" if row["latched"] else "")
        off = "-" if row["off_spot"] is None else f"{row['off_spot']:.1f} yd"
        print(f"  {short(trace, row['guid']):8} {clock(row['killed']):>9} {killer[:18]:18} {spell[:26]:26}"
              f" {row['source'] or '-':4} {'+'.join(row['shape']) or '-':13} {side[:10]:10} {off:>8}")
    print(f"  died is {PERMAFROST_DELAY_MS / 1000:.1f} s before its patch formed; side is the latch that named"
          " it, else the killer's\n  sphere duty; off spot is how far it landed from that side's spot at the"
          f" kill: {TANK_PATCH_OFFSET:.0f} yd off the\n  room centre in y, or {SIDE_SPOT_GAP:.0f} yd off the"
          " other side's patch once that one latched,\n  or the anub.spotN the side held then where the trace"
          " has one")

    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    shaped = collections.Counter((trace.owners.get(rec.get("s"), rec.get("s")), rec.get("sp"))
                                 for rec in trace.of("cast")
                                 if rec.get("tgt") in spheres and spell_shape(rec.get("sp")))
    print("  periodic or area spells aimed at a sphere: " + (", ".join(
        f"{trace.name(guid)} {trace.spell(spell)} {n}" for (guid, spell), n in shaped.most_common()) or "none"))


def grip_after(row: dict) -> str:
    """What the DK's grip duty did after the cast, and a sphere that floated back up."""
    parts = []
    if row["step"] == "kill":
        parts.append(f"duty held to kill +{row['step_ms'] / 1000:.1f} s")
    elif row["step"] == "none":
        parts.append(f"duty dropped to none +{row['step_ms'] / 1000:.1f} s, never at kill")
    if row["rose"] is not None:
        parts.append(f"sphere back above z {SPHERE_FLYING_Z:.0f} +{row['rose'] / 1000:.1f} s")
    return "; ".join(parts)


def spot_changes(trace: Trace) -> list[dict]:
    """Each `anub.spot0` and `anub.spot1` change in time order: the side, when, the spot, and how far it
    moved from that side's last one."""
    out = []
    for side, spans in enumerate(spot_spans(trace)):
        last = None
        for spot, start, _ in spans:
            out.append({"side": side, "t": start, "spot": spot,
                        "moved": dist2(spot, last) if spot and last else None})
            last = spot or last
    return sorted(out, key=lambda row: (row["t"], row["side"]))


def show_spots(trace: Trace) -> None:
    rows = spot_changes(trace)
    if not rows:
        print("  anub.spot0 and anub.spot1: no rows")
        return
    print("\n  tank spots from anub.spot0 and anub.spot1, each change:")
    for row in rows:
        spot = "unparsed" if row["spot"] is None else f"{row['spot'][0]:.1f}, {row['spot'][1]:.1f}"
        moved = "" if row["moved"] is None else f"  {row['moved']:.1f} yd on"
        print(f"    side {row['side']}  {clock(row['t']):>9}  {spot}{moved}")


def show_grips(trace: Trace, samples: Samples) -> None:
    grips = grip_rows(trace, samples)
    if not grips:
        print("  no Death Grip at a sphere")
    else:
        print(f"\n  Death Grip at a sphere, landed once it sat within {GRIP_LANDED:.0f} yd of the DK's spot at the"
              f" cast inside {GRIP_JUDGE_MS / 1000:.0f} s:")
        for row in grips:
            verdict = "missed, sphere unsampled"
            if row["cast_at"] is None:
                verdict = "unjudged, DK unsampled at the cast"
            elif row["landed"] is not None:
                verdict = f"landed +{row['landed'] / 1000:.1f} s"
            elif row["closest"] is not None:
                verdict = f"missed, closest {row['closest']:.1f} yd"
            if not row["pulled"]:
                verdict += ", no pull"
            if row["latched"] is not None:
                verdict += f", latched side {row['latched']}"
                if row["served"] is not None and row["latched"] != row["served"]:
                    verdict += f", not its grip's side {row['served']}"
            print(f"    {clock(row['t']):>9} {trace.name(row['dk'])[:14]:14} {row['duty'] or '-':6}"
                  f" {short(trace, row['sphere']):8} {verdict}")
            after = grip_after(row)
            if after:
                print(f"              {after}")
        dropped = [row for row in grips if row["step"] != "kill"]
        rose = [row for row in grips if row["rose"] is not None]
        print(f"  grip duty never at kill: {len(dropped)} of {len(grips)}; gripped sphere back above z"
              f" {SPHERE_FLYING_Z:.0f}: " + (", ".join(f"{short(trace, row['sphere'])} {clock(row['t'] + row['rose'])}"
                                                       for row in rose) or "none"))
        show_far_grips(trace, samples)
    misses = grip_misses(trace)
    if misses:
        print("  Death Grip at anything with no pull, its 49576 triggering no 49560: "
              + ", ".join(f"{trace.name(dk)} {missed} of {total}"
                          for dk, (total, missed) in sorted(misses.items(), key=lambda item: -item[1][1])))
    spans = latch_spans(trace, "anub.grip", pull_end(trace))
    if spans:
        print("  anub.grip: " + ", ".join(f"{GRIP_STATES.get(value, value)} from {clock(start)}"
                                          for value, start, _ in spans))
    show_counts(trace, "anub.gripstep")
    relays = [rec for rec in notes(trace, "anub.relay") if str(rec.get("txt")) != "none"]
    print("  anub.relay: " + (", ".join(f"{clock(rec['t'])} {trace.name(rec.get('g'))} {rec.get('txt')}"
                                        for rec in relays) or "no rows"))


def show_far_grips(trace: Trace, samples: Samples) -> None:
    chains = far_grips(trace, samples)
    if not chains:
        print(f"  no far grip, the first at a sphere cast over {GRIP_RELAY_FROM:.0f} yd off its side's spot")
        return
    print(f"\n  far grips, the first at a sphere cast over {GRIP_RELAY_FROM:.0f} yd off its side's spot, each link with"
          " the sphere's\n  distance from that spot at the cast and where the grip brought it down:")
    for chain in chains:
        death = chain["death"]
        died = ("alive at the end" if death is None else f"died {clock(death['killed'])}"
                + ("" if death["off_spot"] is None else f", {death['off_spot']:.1f} yd off its spot"))
        print(f"    {short(trace, chain['sphere'])} side {chain['side']}, {died}")
        for index, link in enumerate(chain["links"], 1):
            verdict = (f"landed +{link['landed'] / 1000:.1f} s" if link["landed"] is not None
                       else "no pull" if not link["pulled"]
                       else "missed" if link["closest"] is None else f"missed, closest {link['closest']:.1f} yd")
            yards = " > ".join("-" if value is None else f"{value:.1f}" for value in (link["before"], link["after"]))
            dk = trace.name(link["dk"]) + (" (human)" if link["dk"] in trace.humans else "")
            print(f"      link {index} {clock(link['t']):>9} {dk[:22]:22} {verdict:24} {yards} yd")


def show_spheres(trace: Trace) -> None:
    print("FROST SPHERES")
    samples = Samples(trace)
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    if not spheres:
        print("  no Frost Sphere named")
        return

    lives = patch_lives(trace, samples)
    used = consumed_patches(trace, samples)
    print(f"  {len(spheres)} sphere(s) named, {len(lives)} ever a patch, {len(used)} used by the spike")
    timeline = [row for row in sphere_timeline(samples) if any(row[1:])]
    print(f"  {'at':>9} {'flying':>6} {'patches':>7} {'permafrost':>10}")
    for when, flying, patches, frost in timeline[:TIMELINE_SHOWN]:
        print(f"  {clock(when):>9} {flying:6} {patches:7} {frost:10}")
    if len(timeline) > TIMELINE_SHOWN:
        print(f"  ... {len(timeline) - TIMELINE_SHOWN} more change(s)")

    for patch, (seen, stop) in sorted(used.items(), key=lambda item: item[1]):
        print(f"  patch {short(trace, patch)} last seen {clock(seen)}, a mark ended {clock(stop)}")
    if is_heroic(trace):
        print("  heroic, no respawn, flying left: "
              + ", ".join(f"{label} {clock(when)} {'-' if n is None else n}"
                          for label, when, n in spheres_left(trace, samples)))
    show_counts(trace, "anub.sphere")
    shots = sphere_shots(trace)
    if shots:
        print("  cast at a sphere: "
              + ", ".join(f"{trace.name(guid)} {n}" for guid, n in shots.most_common()))

    supply = supply_rows(trace, samples)
    if supply:
        print("\n  spheres at each mark, tank being the patches a side had latched and other the rest:")
        print(f"  {'at':>9} {'kiter':14} {'tank':>4} {'other':>5} {'flying':>6} {'falling':>7}"
              f" {'outcome':7} sphere duty in the window")
        for row in supply:
            holders = ", ".join(f"{trace.name(guid)} {label}" for guid, label in row["holders"]) or "-"
            print(f"  {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} {row['tank']:4} {row['other']:5}"
                  f" {row['flying']:6} {row['falling']:7} {row['outcome']:7} {holders}")

    spans = duty_spans(trace)
    if not spans:
        print("  nobody held a sphere duty")
    else:
        print(f"\n  {'duty from':>11} {'to':>9} {'holder':14} {'duty':5} {'casts':>5} {'drops':>5}")
        for row in spans[:TIMELINE_SHOWN]:
            print(f"  {clock(row['start']):>11} {clock(row['stop']):>9} {trace.name(row['guid'])[:14]:14}"
                  f" {row['label']:5} {row['casts']:5} {row['drops']:5}")
        if len(spans) > TIMELINE_SHOWN:
            print(f"  ... {len(spans) - TIMELINE_SHOWN} more span(s)")
        by_label: dict[str, list[dict]] = collections.defaultdict(list)
        for row in spans:
            by_label[row["label"]].append(row)
        print("  spans with a cast: " + ", ".join(
            f"{'side ' + label if label.isdigit() else label} {sum(1 for row in rows if row['casts'])} of {len(rows)}"
            for label, rows in sorted(by_label.items())))
        print("  casts are the holder's at any sphere; drops are its drop target rows, one per verdict change")

    show_deaths(trace, samples)
    show_grips(trace, samples)
    show_spots(trace)
    show_tank_patches(trace, samples)
    show_kite_spheres(trace, samples)


# ------------------------------------------------------------------------------------------ burrowers

def returned(before: tuple[int, float], after: tuple[int, float]) -> bool:
    """A submerge, read off two samples: gone at least SUBMERGE_GAP_MS and back near full, or a jump."""
    (then, low), (now, high) = before, after
    if now - then >= SUBMERGE_GAP_MS and high >= NEAR_FULL and high > low:
        return True
    return high - low >= SUBMERGE_JUMP


def on_permafrost(samples: Samples, row, when: int) -> bool:
    return any(dist2(row[1:3], centre) <= BURROWER_PERMAFROST_REACH for centre in samples.frost_at(when))


def burrower_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower, its life and Shadow Strikes. `ups` is each time it came up: first sight, then each
    emerge. `died` is its last sample, None for one still sampled in the trace's last snapshot."""
    samples = samples or Samples(trace)
    strikes = [rec for rec in trace.of("cast") if rec.get("sp") == SPELL_SHADOW_STRIKE]
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_SHADOW_STRIKE]
    last = samples.stamps[-1] if samples.stamps else 0
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_BURROWER)):
        stamps, rows = samples.track(guid)
        if not stamps:
            continue
        points = [(when, row[5]) for when, row in zip(stamps, rows)]
        mine = [rec for rec in strikes if rec.get("s") == guid]
        landed = sum(1 for rec in mine if any(
            hit.get("s") == guid
            and rec["t"] <= hit["t"] <= rec["t"] + (rec.get("ct") or SHADOW_STRIKE_CAST_MS) + LAND_SLACK_MS
            for hit in hits))
        emerges = [points[index][0] for index in range(1, len(points)) if returned(points[index - 1], points[index])]
        out.append({
            "guid": guid,
            "first": stamps[0],
            "last": stamps[-1],
            "low": min(hp for _, hp in points),
            "end": points[-1][1],
            "submerges": len(emerges),
            "ups": [stamps[0], *emerges],
            "died": stamps[-1] if stamps[-1] < last else None,
            "frost": sum(1 for when, row in zip(stamps, rows)
                         if on_permafrost(samples, row, when)) / len(rows),
            "strikes": len(mine),
            "landed": landed,
        })
    return out


def burrower_windows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower, its wave, the last time it came up, whether that was an emerge, and whether it died
    inside BURROWER_SAFE_MS of it, before the core could submerge it. `inside` is None for one alive at
    the end."""
    samples = samples or Samples(trace)
    waves = wave_starts(trace, samples)
    out = []
    for row in burrower_rows(trace, samples):
        up, died = row["ups"][-1], row["died"]
        out.append({"guid": row["guid"], "wave": wave_of(waves, row["first"]), "up": up,
                    "emerged": len(row["ups"]) > 1, "died": died,
                    "inside": None if died is None else died - up <= BURROWER_SAFE_MS})
    return out


def as_guid(value) -> int:
    text = str(value).strip()
    return int(text) if text.isdigit() else 0


def focus_latches(trace: Trace) -> list[tuple[int | None, list[tuple[str, int, int]]]]:
    """`(side, spans)` per burrower focus latch: `anub.focus0` and `anub.focus1`, else the one raid-wide
    `anub.focus` of older traces, side None."""
    end = pull_end(trace)
    if any(notes(trace, key) for key in FOCUS_KEYS):
        return [(side, latch_spans(trace, key, end)) for side, key in enumerate(FOCUS_KEYS)]
    return [(None, latch_spans(trace, FOCUS, end))]


def focus_label(side: int | None) -> str:
    return "-" if side is None else str(side)


def dside_at(groups: dict[int, list[tuple[int, str]]], guid: int, when: int) -> int | None:
    """The burrower side `anub.dside` gave a DPS bot at `when`, None before its first row."""
    side = next((value for start, value in reversed(groups.get(guid, [])) if start <= when), None)
    return int(side) if side in ("0", "1") else None


def focus_rows(trace: Trace) -> list[dict]:
    """Per side and burrower a focus latch held: the time it held it, and the share of living DPS bot
    samples (melee and ranged roles, humans left out, only that side's `anub.dside` group when the trace
    has one) whose target it was meanwhile."""
    dps = {guid for guid in roster_guids(trace) if trace.role(guid) in DPS_ROLES and guid not in trace.humans}
    groups = per_bot(trace, "anub.dside")
    held: collections.Counter = collections.Counter()
    latches = []
    for side, values in focus_latches(trace):
        spans = [(as_guid(value), start, stop) for value, start, stop in values]
        latches.append((side, spans, [start for _, start, _ in spans]))
        for guid, start, stop in spans:
            if guid:
                held[(side, guid)] += stop - start

    on: collections.Counter = collections.Counter()
    total: collections.Counter = collections.Counter()
    for snap in frames(trace):
        when = snap["t"]
        rows = [row for row in snap.get("u", []) if row[0] in dps and len(row) > 7 and row[5] > 0]
        for side, spans, starts in latches:
            index = bisect.bisect_right(starts, when) - 1
            if index < 0:
                continue
            focus, _, stop = spans[index]
            if not focus or when >= stop:
                continue
            for row in rows:
                if side is not None and groups and dside_at(groups, row[0], when) != side:
                    continue
                total[(side, focus)] += 1
                on[(side, focus)] += row[7] == focus

    return [{"side": side, "guid": guid, "held": held[(side, guid)],
             "share": on[(side, guid)] / total[(side, guid)] if total[(side, guid)] else None}
            for side, guid in sorted(held)]


def wave_starts(trace: Trace, samples: Samples | None = None) -> list[tuple[int, int]]:
    """`(wave, spawn)` per burrower wave from `anub.wave`, else from burrower spawns WAVE_SPLIT_MS apart."""
    waves = [(int(value), start) for value, start, _ in latch_spans(trace, "anub.wave", pull_end(trace))
             if value.isdigit() and int(value) > 0]
    if waves:
        return waves
    samples = samples or Samples(trace)
    spawns = sorted(samples.track(guid)[0][0] for guid in guids_of_entry(trace, NPC_BURROWER)
                    if samples.track(guid)[0])
    out: list[tuple[int, int]] = []
    for index, when in enumerate(spawns):
        if not index or when - spawns[index - 1] >= WAVE_SPLIT_MS:
            out.append((len(out) + 1, when))
    return out


def wave_of(waves: list[tuple[int, int]], spawn: int) -> int | None:
    return next((wave for wave, start in reversed(waves) if start <= spawn + NEAR_MS), None)


def owner_at(changes: list[tuple[int, int]], first: int, when: int) -> int:
    """The side an `anub.bside` history had at `when`, `first` before its first row."""
    return next((owner for start, owner in reversed(changes) if start <= when), first)


def pickup_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower: its wave, exit and side at the spawn, every owner it had in turn, the time from its
    first sample to the first with any tank on it and with its owning side's tank on it then, and how
    far that side tank stood at the spawn from its side's patch, or from the side's spot before one
    latched. The side is the first `anub.bside`: a dead side tank's burrowers all move, so the last says
    little. `opener` is `(ms after the spawn, target)` for its first target that isn't one of the encounter's
    own creatures, since it spawns aimed at a sphere for a tick, and `nearest` the OPENER_NEAREST living
    roster players nearest its exit at the spawn, `(guid, yards)`."""
    samples = samples or Samples(trace)
    end = pull_end(trace)
    sides = [latch_spans(trace, f"anub.side{side}", end) for side in (0, 1)]
    latches = patch_latches(trace)
    noted = spot_spans(trace)
    owned = per_bot(trace, "anub.bside")
    waves = wave_starts(trace, samples)
    roster = roster_guids(trace)
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_BURROWER)):
        stamps, rows = samples.track(guid)
        if not stamps:
            continue
        spawn = stamps[0]
        x, y, exit_side = min(BURROW_EXITS, key=lambda spot: dist2(spot, rows[0][1:3]))
        changes = [(when, int(value)) for when, value in owned.get(guid, []) if value in ("0", "1")]
        side = changes[0][1] if changes else exit_side
        owners = [owner for index, (_, owner) in enumerate(changes) if not index or owner != changes[index - 1][1]]
        targets = [(when, row[7]) for when, row in zip(stamps, rows) if len(row) > 7 and row[7]]
        to_tank = next((when - spawn for when, target in targets if trace.role(target) == "tank"), None)
        to_side = next((when - spawn for when, target in targets
                        if as_guid(held_at(sides[owner_at(changes, side, when)], when)) == target), None)
        tank = as_guid(held_at(sides[side], spawn))
        stand = samples.row(tank, spawn) if tank else None
        gap = kind = None
        if stand is not None:
            point = latched_patch(latches[side], samples, spawn)
            kind = "patch" if point else "spot"
            gap = dist2(stand[1:3], point or side_spot(latches, samples, side, spawn, noted))
        opener = next(((when - spawn, target) for when, target in targets
                       if trace.entries.get(target) not in ENCOUNTER_NPCS), None)
        players = [(member, samples.recent(member, spawn)) for member in roster]
        nearest = sorted(((member, dist2(row[1:3], (x, y))) for member, row in players if row and row[5] > 0),
                         key=lambda item: item[1])[:OPENER_NEAREST]
        out.append({"guid": guid, "spawn": spawn, "exit": (x, y), "exit_side": exit_side,
                    "wave": wave_of(waves, spawn),
                    "side": side, "probed": bool(changes), "owners": owners or [side], "to_tank": to_tank,
                    "to_side": to_side, "tank": tank or None, "gap": gap, "from": kind,
                    "opener": opener, "nearest": nearest})
    return out


def focus_due_rows(trace: Trace, waves: list[tuple[int, int]]) -> list[dict]:
    """Per wave, when `anub.focusdue` first read 1 from its spawn on, kept only when that came before the
    next wave's spawn."""
    due = [(start, stop) for value, start, stop in latch_spans(trace, "anub.focusdue", pull_end(trace))
           if value == "1"]
    out = []
    for index, (wave, spawn) in enumerate(waves):
        upto = waves[index + 1][1] if index + 1 < len(waves) else None
        when = next((max(start, spawn) for start, stop in due if stop > spawn), None)
        if when is not None and upto is not None and when >= upto:
            when = None
        out.append({"wave": wave, "spawn": spawn, "due": when, "next": upto})
    return out


def stray_burrower_casts(trace: Trace) -> list[dict]:
    """DPS bot casts aimed at a burrower no focus latch held then, either side's counting, triggered ones and
    humans left out, and a cast landing while its target cast Shadow Strike, since interrupts may go
    anywhere."""
    burrowers = guids_of_entry(trace, NPC_BURROWER)
    dps = {guid for guid in roster_guids(trace) if trace.role(guid) in DPS_ROLES and guid not in trace.humans}
    latches = [spans for _, spans in focus_latches(trace)]
    strikes = [(rec.get("s"), rec["t"], rec["t"] + (rec.get("ct") or SHADOW_STRIKE_CAST_MS))
               for rec in trace.of("cast") if rec.get("sp") == SPELL_SHADOW_STRIKE]
    out = []
    for rec in trace.of("cast"):
        target = rec.get("tgt")
        if rec.get("tr") or rec.get("s") not in dps or target not in burrowers:
            continue
        if any(as_guid(held_at(spans, rec["t"])) == target for spans in latches):
            continue
        if any(source == target and low <= rec["t"] <= high for source, low, high in strikes):
            continue
        out.append(rec)
    return out


def bladestorm_casts(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each Bladestorm from the roster, triggered ones left out: the caster and the nearest living burrower
    sampled within one snapshot gap of the cast, how far, both None with none sampled."""
    samples = samples or Samples(trace)
    burrowers = guids_of_entry(trace, NPC_BURROWER)
    roster = roster_guids(trace)
    out = []
    for rec in trace.of("cast"):
        if rec.get("sp") != SPELL_BLADESTORM or rec.get("tr") or rec.get("s") not in roster:
            continue
        caster = samples.recent(rec["s"], rec["t"])
        gaps = [] if caster is None else [
            (dist2(row[1:3], caster[1:3]), guid)
            for guid, row in ((guid, samples.recent(guid, rec["t"])) for guid in burrowers) if row and row[5] > 0]
        yards, burrower = min(gaps, default=(None, None))
        out.append({"t": rec["t"], "guid": rec["s"], "burrower": burrower, "yards": yards})
    return out


def burrower_vetoes(trace: Trace) -> tuple[collections.Counter, collections.Counter]:
    """Per bot, its `anubarak burrower threat cap` veto rows, and its `anubarak burrower guard` ones on
    Bladestorm: one row per verdict change. Older traces' guard held it only near a burrower still settling."""
    capped: collections.Counter = collections.Counter()
    held: collections.Counter = collections.Counter()
    for rec in trace.of("veto"):
        if rec.get("m") == BURROWER_THREAT_CAP:
            capped[rec.get("g", 0)] += 1
        elif rec.get("m") == BURROWER_GUARD and str(rec.get("a", "")) == "bladestorm":
            held[rec.get("g", 0)] += 1
    return capped, held


def show_burrower_threat(trace: Trace, samples: Samples) -> None:
    def cast_text(row: dict) -> str:
        if row["yards"] is None:
            return f"{trace.name(row['guid'])} {clock(row['t'])} no burrower sampled"
        return (f"{trace.name(row['guid'])} {clock(row['t'])} {short(trace, row['burrower'])} {row['yards']:.1f} yd"
                + (" near" if row["yards"] <= BLADESTORM_BURROWER_REACH else ""))

    casts = bladestorm_casts(trace, samples)
    near = sum(1 for row in casts if row["yards"] is not None and row["yards"] <= BLADESTORM_BURROWER_REACH)
    print(f"  Bladestorm, the nearest living burrower at the cast, near within {BLADESTORM_BURROWER_REACH:.0f} yd: "
          + (", ".join(map(cast_text, casts)) + f"; near in {near} of {len(casts)}" if casts else "none"))
    capped, held = burrower_vetoes(trace)

    def per_bot_text(counts: collections.Counter) -> str:
        return ", ".join(f"{trace.name(guid)} {n}" for guid, n in counts.most_common()) or "no rows"

    print(f"  {BURROWER_GUARD} vetoes on bladestorm: {per_bot_text(held)}")
    print(f"  {BURROWER_THREAT_CAP} vetoes per bot, one per verdict change: {per_bot_text(capped)}")


def member_of(trace: Trace, guid) -> int:
    """The roster player a pet, guardian or totem belongs to, else the guid itself."""
    return trace.owners.get(guid, guid)


def burrower_pulls(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each burrower pulled off a side tank by a non-tank within PULL_WINDOW_MS of coming up, first sight or an
    emerge: its target going from the tank holding a side to a player off the tank role or one's pet, snapshots
    with no target skipped. When, the side tank and its side, the puller, when the side tank first had it in
    that run, and the puller's casts on it in the PULL_CASTS_MS before, triggered ones too: a proc, a Blade Flurry
    or Sweeping Strikes extra hit, a Death and Decay tick and Holy Vengeance all pull."""
    samples = samples or Samples(trace)
    sides = side_spans(trace)
    waves = wave_starts(trace, samples)
    casts: dict[tuple, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        casts[(rec.get("s"), rec.get("tgt"))].append(rec)
    out = []
    for row in burrower_rows(trace, samples):
        guid = row["guid"]
        stamps, rows = samples.track(guid)
        for up in row["ups"]:
            tank = since = side = None
            for index in range(bisect.bisect_left(stamps, up), bisect.bisect_right(stamps, up + PULL_WINDOW_MS)):
                when, unit = stamps[index], rows[index]
                target = unit[7] if len(unit) > 7 else 0
                if not target or unit[5] <= 0:
                    continue
                held = held_side(sides, target, when)
                if held is not None:
                    if target != tank:
                        tank, since, side = target, when, held
                    continue
                puller = member_of(trace, target)
                if tank is not None and puller in trace.roles and trace.role(puller) != "tank":
                    out.append({"guid": guid, "wave": wave_of(waves, row["first"]), "up": up,
                                "emerged": up != row["ups"][0], "t": when, "tank": tank, "side": side, "on": since,
                                "puller": target, "casts": [rec for rec in casts[(target, guid)]
                                                            if when - PULL_CASTS_MS <= rec["t"] <= when]})
                tank = None
    return sorted(out, key=lambda pull: (pull["t"], pull["guid"]))


def focus_due_drops(trace: Trace, samples: Samples) -> list[dict]:
    """Each time `anub.focusdue` went from 1 to 0 with the `anub.focus` burrower still alive, and its
    health then. Only older traces write either."""
    end = pull_end(trace)
    focus = latch_spans(trace, FOCUS, end)
    out = []
    for value, _, stop in latch_spans(trace, "anub.focusdue", end):
        if value != "1" or stop >= end:
            continue
        guid = as_guid(held_at(focus, stop - 1))
        row = samples.recent(guid, stop) if guid else None
        if row and row[5] > 0:
            out.append({"t": stop, "guid": guid, "hp": row[5]})
    return out


def side_spans(trace: Trace) -> list[list[tuple[str, int, int]]]:
    """`anub.side0` and `anub.side1` spans, indexed by side."""
    end = pull_end(trace)
    return [latch_spans(trace, f"anub.side{side}", end) for side in (0, 1)]


def focus_picks(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per focus latch pick of a burrower, with the latch's side: its health, the milliseconds since it came
    up (first sight or its last emerge), its target, and how far it stood from the nearest living side tank
    and the nearest Permafrost centre. `settled` is a tank as its target within FOCUS_SETTLE of it."""
    samples = samples or Samples(trace)
    sides = side_spans(trace)
    holders = {as_guid(value) for spans in sides for value, _, _ in spans} - {0}
    ups = {row["guid"]: row["ups"] for row in burrower_rows(trace, samples)}
    out = []
    picks = sorted(((start, side, value) for side, spans in focus_latches(trace) for value, start, _ in spans),
                   key=lambda pick: (pick[0], pick[1] or 0))
    for start, side, value in picks:
        guid = as_guid(value)
        row = samples.row(guid, start) if guid else None
        if row is None:
            continue
        came = [when for when in ups.get(guid, []) if when <= start] or ups.get(guid, [start])[:1]
        target = row[7] if len(row) > 7 else 0
        victim = samples.recent(target, start) if target else None
        tanks = [samples.recent(tank, start) for tank in {as_guid(held_at(spans, start)) for spans in sides} - {0}]
        to_tank = [dist2(row[1:3], tank[1:3]) for tank in tanks if tank and tank[5] > 0]
        to_frost = [dist2(row[1:3], point) for point in samples.frost_at(start)]
        out.append({"t": start, "side": side, "guid": guid, "hp": row[5], "up": max(0, start - came[-1]),
                    "target": target,
                    "side_tank": min(to_tank) if to_tank else None, "patch": min(to_frost) if to_frost else None,
                    "settled": victim is not None and (trace.role(target) == "tank" or target in holders)
                    and dist2(row[1:3], victim[1:3]) <= FOCUS_SETTLE})
    return out


def held_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower ever on a side tank, its target the tank holding a side then and within FOCUS_SETTLE of it:
    those samples, and its median distance from the patch that side had latched, over the ones with a patch.
    A side with none would read the other side's patch as the nearest."""
    samples = samples or Samples(trace)
    sides = side_spans(trace)
    latches = patch_latches(trace)
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_BURROWER)):
        stamps, rows = samples.track(guid)
        held = 0
        gaps = []
        for when, row in zip(stamps, rows):
            target = row[7] if len(row) > 7 else 0
            side = next((side for side, spans in enumerate(sides)
                         if target and as_guid(held_at(spans, when)) == target), None)
            if side is None:
                continue
            tank = samples.recent(target, when)
            if tank is None or dist2(row[1:3], tank[1:3]) > FOCUS_SETTLE:
                continue
            held += 1
            patch = latched_patch(latches[side], samples, when)
            if patch:
                gaps.append(dist2(row[1:3], patch))
        if held:
            out.append({"guid": guid, "held": held, "gaps": gaps, "median": statistics.median(gaps) if gaps else None})
    return out


def burrower_submerges(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each burrower submerge, read where it went under: the first sample of the still run, same spot and
    health, before it came back full. There its health, the nearest Permafrost centre and how many were
    alive, its `anub.bside` side, else its exit's, that side's tank, alive or not and how far, and its
    target, else the last it had within VICTIM_LOOKBACK_MS."""
    samples = samples or Samples(trace)
    sides = side_spans(trace)
    owned = per_bot(trace, "anub.bside")
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_BURROWER)):
        stamps, rows = samples.track(guid)
        if not stamps:
            continue
        exit_side = min(BURROW_EXITS, key=lambda spot: dist2(spot, rows[0][1:3]))[2]
        changes = [(when, int(value)) for when, value in owned.get(guid, []) if value in ("0", "1")]
        for index in range(1, len(rows)):
            if not returned((stamps[index - 1], rows[index - 1][5]), (stamps[index], rows[index][5])):
                continue
            under = index - 1
            while (under and dist2(rows[under - 1][1:3], rows[under][1:3]) < FROZEN_YD
                   and rows[under - 1][5] == rows[under][5]):
                under -= 1
            when, row = stamps[under], rows[under]
            frost = [dist2(row[1:3], centre) for centre in samples.frost_at(when)]
            side = owner_at(changes, changes[0][1] if changes else exit_side, when)
            tank = as_guid(held_at(sides[side], when))
            stand = samples.recent(tank, when) if tank else None
            victim = next((rows[back][7] for back in range(under, -1, -1)
                           if when - stamps[back] <= VICTIM_LOOKBACK_MS and len(rows[back]) > 7 and rows[back][7]), 0)
            out.append({"guid": guid, "t": when, "back": stamps[index], "spot": (row[1], row[2]), "hp": row[5],
                        "frost": min(frost) if frost else None, "patches": len(frost), "side": side,
                        "tank": tank or None, "alive": bool(stand) and stand[5] > 0,
                        "tank_yd": dist2(row[1:3], stand[1:3]) if stand else None, "victim": victim})
    return sorted(out, key=lambda row: (row["t"], row["guid"]))


def submerge_reasons(row: dict) -> list[str]:
    """What let a burrower go under: no side tank, a dead one, no patch alive, or a target off its side tank."""
    out = []
    if row["tank"] is None:
        out.append("no side tank")
    elif not row["alive"]:
        out.append("side tank dead")
    if not row["patches"]:
        out.append("no patch")
    if row["tank"] is not None and row["victim"] != row["tank"]:
        out.append("off its side tank")
    return out


def side_tank_gather(trace: Trace) -> list[tuple[str, int, dict[int, int]]]:
    """Per submerge, the milliseconds each bot held `anub.sidetank` gather while he was down."""
    spans = holder_spans(trace, "anub.sidetank", pull_end(trace))
    out = []
    for start, stop, phase, label in phase_spans(trace):
        if phase == 2:
            held = {guid: covered([(low, high) for value, low, high in rows if value == "gather"], start, stop)
                    for guid, rows in spans.items()}
            out.append((label, start, {guid: ms for guid, ms in held.items() if ms}))
    return out


def show_focus_picks(trace: Trace, samples: Samples) -> None:
    picks = focus_picks(trace, samples)
    if picks:
        print("\n  each focus pick: the burrower's health, seconds since it came up, target, its distance to the"
              "\n  nearest living side tank and Permafrost centre, and the side whose latch picked it, - for the"
              " single\n  anub.focus of older traces:")
        print(f"  {'at':>9} {'focus':10} {'hp':>4} {'up':>6} {'target':14} {'side tank':>9} {'patch':>7} side")
        for row in picks:
            tank = "-" if row["side_tank"] is None else f"{row['side_tank']:.1f} yd"
            patch = "-" if row["patch"] is None else f"{row['patch']:.1f} yd"
            early = not row["settled"] and row["hp"] >= FOCUS_LOW_PCT
            print(f"  {clock(row['t']):>9} {short(trace, row['guid']):10} {row['hp']:3.0f}% {row['up'] / 1000:4.1f} s"
                  f" {unit_label(trace, row['target'])[:14]:14} {tank:>9} {patch:>7} {focus_label(row['side']):>4}"
                  + ("  before its tank" if early else ""))
        early = sum(1 for row in picks if not row["settled"] and row["hp"] >= FOCUS_LOW_PCT)
        print(f"  {early} of {len(picks)} pick(s) before its target was a tank within {FOCUS_SETTLE:.0f} yd, at"
              f" {FOCUS_LOW_PCT:.0f}% or more health; up runs from its first sample or its last emerge")

    held = held_rows(trace, samples)
    if not held:
        print(f"  no burrower sampled on a side tank within {FOCUS_SETTLE:.0f} yd")
        return
    print(f"\n  on a side tank, its target the tank holding a side and within {FOCUS_SETTLE:.0f} yd: median distance"
          " from\n  that side's latched tank patch over those samples with one")
    print(f"  {'burrower':10} {'samples':>7} {'patched':>7} {'median':>7}")
    for row in held:
        median = "-" if row["median"] is None else f"{row['median']:.1f} yd"
        print(f"  {short(trace, row['guid']):10} {row['held']:7} {len(row['gaps']):7} {median:>7}")
    gaps = [gap for row in held for gap in row["gaps"]]
    if gaps:
        print(f"  all: median {statistics.median(gaps):.1f} yd over {len(gaps)} sample(s), a burrower's own reach on"
              f" the slow being {BURROWER_PERMAFROST_REACH:.1f} yd")


def show_burrower_windows(trace: Trace, samples: Samples) -> None:
    rows = burrower_windows(trace, samples)
    print(f"\n  dead inside {BURROWER_SAFE_MS / 1000:.0f} s of coming up, before the core's first submerge check:")
    print(f"  {'burrower':10} {'wave':>4} {'came up':>9} {'from':6} {'died':>9} {'after':>7} inside")
    tally: dict[int | None, collections.Counter] = collections.defaultdict(collections.Counter)
    for row in rows:
        died = "alive" if row["died"] is None else clock(row["died"])
        after = "-" if row["died"] is None else f"{(row['died'] - row['up']) / 1000:.1f} s"
        verdict = {None: "alive", True: "yes", False: "no"}[row["inside"]]
        tally[row["wave"]][verdict] += 1
        print(f"  {short(trace, row['guid']):10} {row['wave'] or '-':>4} {clock(row['up']):>9}"
              f" {'emerge' if row['emerged'] else 'sight':6} {died:>9} {after:>7}"
              f" {'-' if row['inside'] is None else verdict}")
    for wave, counts in sorted(tally.items(), key=lambda item: (item[0] is None, item[0] or 0)):
        print(f"  wave {wave or '-'}: {counts['yes']} of {sum(counts.values())} dead inside, {counts['no']} later,"
              f" {counts['alive']} alive")


def show_burrower_submerges(trace: Trace, samples: Samples) -> None:
    rows = burrower_submerges(trace, samples)
    if not rows:
        print("  no burrower submerged")
        return
    print("\n  each submerge, at the first sample of the still run before it came back full:")
    print(f"  {'burrower':10} {'under':>9} {'at':13} {'hp':>4} {'patch':>8} {'patches':>7} {'side':>4}"
          f" {'side tank':22} target")
    for row in rows:
        patch = "-" if row["frost"] is None else f"{row['frost']:.1f} yd"
        tank = "none"
        if row["tank"] is not None:
            state = "dead" if not row["alive"] else "-" if row["tank_yd"] is None else f"{row['tank_yd']:.1f} yd"
            tank = f"{trace.name(row['tank'])[:14]} {state}"
        why = ", ".join(submerge_reasons(row))
        print(f"  {short(trace, row['guid']):10} {clock(row['t']):>9} {row['spot'][0]:6.1f},{row['spot'][1]:6.1f}"
              f" {row['hp']:3.0f}% {patch:>8} {row['patches']:7} {row['side']:4} {tank:22}"
              f" {unit_label(trace, row['victim'])}" + (f"  {why}" if why else ""))
    up = sum(1 for row in rows if row["alive"] and row["patches"])
    print(f"  {up} of {len(rows)} went under with its side tank alive and a patch alive; patch is the nearest"
          " Permafrost\n  centre, patches the centres alive then, side its anub.bside owner, else its exit's")


def show_openers(trace: Trace, rows: list[dict]) -> None:
    print(f"\n  each burrower's opener, its first target off the encounter's own creatures, and the {OPENER_NEAREST}"
          " living\n  players nearest its exit at the spawn:")
    for row in rows:
        if row["opener"] is None:
            opener = "never on the raid"
        else:
            after, target = row["opener"]
            role = f" ({trace.role(target)})" if target in trace.roles else ""
            opener = f"{unit_label(trace, target)[:30]}{role} +{after / 1000:.1f} s"
        near = ", ".join(f"{trace.name(guid)} {trace.role(guid)} {yards:.0f}" for guid, yards in row["nearest"])
        print(f"  {short(trace, row['guid']):10} {row['wave'] or '-':>4} {opener:40} nearest {near or '-'}")
    untanked = [row for row in rows if row["opener"] and trace.role(row["opener"][1]) != "tank"]
    times = [row["to_tank"] for row in untanked if row["to_tank"] is not None]
    print(f"  opened on a non-tank: {len(untanked)} of {len(rows)}"
          + (f"; to a tank after it: median {statistics.median(times) / 1000:.1f} s, longest"
             f" {max(times) / 1000:.1f} s" if times else "")
          + (f", {len(untanked) - len(times)} never" if len(untanked) > len(times) else ""))


def show_pickups(trace: Trace, samples: Samples) -> None:
    def secs(ms):
        return "never" if ms is None else f"{ms / 1000:.1f} s"

    rows = pickup_rows(trace, samples)
    if not rows:
        return
    print(f"\n  {'burrower':10} {'wave':>4} {'exit':13} {'side':7} {'to tank':>7} {'to side':>7}"
          f" {'side tank':14} at the spawn")
    for row in rows:
        exit_name = f"{'N' if row['exit_side'] == 0 else 'S'} {row['exit'][0]:.1f},{row['exit'][1]:.1f}"
        tank = "-" if row["tank"] is None else trace.name(row["tank"])
        gap = "-" if row["gap"] is None else f"{row['gap']:.1f} yd off its {row['from']}"
        owners = ">".join(str(owner) for owner in row["owners"]) + ("" if row["probed"] else "*")
        print(f"  {short(trace, row['guid']):10} {row['wave'] or '-':>4} {exit_name:13}"
              f" {owners[:7]:7} {secs(row['to_tank']):>7}"
              f" {secs(row['to_side']):>7} {tank[:14]:14} {gap}")
    print("  side is its owner at the spawn, then each one after; to tank and to side run from its first"
          " sample\n  to the first with any tank, then its owning side's tank, on it; a * side is read off"
          " its exit,\n  with no anub.bside row; side tank is the spawn side's")
    show_openers(trace, rows)

    waves = wave_starts(trace, samples)
    if not notes(trace, "anub.wave"):
        print(f"  no anub.wave rows, so waves are burrower spawns {WAVE_SPLIT_MS / 1000:.0f} s or more apart")
    if not notes(trace, "anub.focusdue"):
        print("  no anub.focusdue rows, so no focus due per wave: only older traces write it")
    else:
        for row in focus_due_rows(trace, waves):
            if row["next"] is None:
                verdict = "focus never due" if row["due"] is None else f"focus due {clock(row['due'])}"
                verdict += ", the last wave"
            elif row["due"] is None:
                verdict = f"focus never due before wave {row['wave'] + 1} at {clock(row['next'])}"
            else:
                verdict = (f"focus due {clock(row['due'])}, {(row['next'] - row['due']) / 1000:.1f} s before"
                           f" wave {row['wave'] + 1} at {clock(row['next'])}")
            print(f"  wave {row['wave']} {clock(row['spawn'])}: {verdict}")
    show_counts(trace, "anub.sidetank")
    if any(str(rec.get("txt")) == "gather" for rec in notes(trace, "anub.sidetank")):
        print("  anub.sidetank gather while he was down: " + "; ".join(
            f"{label} " + (", ".join(f"{trace.name(guid)} {ms / 1000:.1f} s" for guid, ms in sorted(held.items()))
                           or "none")
            for label, _, held in side_tank_gather(trace)))


def pull_returns(trace: Trace, pulls: list[dict], samples: Samples) -> list[dict]:
    """Per burrower_pulls row: `back` the first snapshot after it with the burrower on a tank again, a side's
    holder or a tank-role player, and `on_tank` that tank, else None with `last` its last sample off a tank before it
    went under (`under`) or stopped being sampled; `taunts` the BURROWER_TAUNTS cast at it from the snapshot before
    the pull to `back`, and an area taunt by the tank it came back to."""
    sides = side_spans(trace)
    taunts = [rec for rec in trace.of("cast") if rec.get("sp") in BURROWER_TAUNTS]
    ups = {row["guid"]: row["ups"] for row in burrower_rows(trace, samples)}
    out = []
    for pull in pulls:
        guid = pull["guid"]
        stamps, rows = samples.track(guid)
        index = bisect.bisect_left(stamps, pull["t"])
        since = stamps[index - 1] if index else pull["t"]
        later = [when for when in ups.get(guid, ()) if when > pull["t"]]
        back = on = None
        last = pull["t"]
        for at in range(index + 1, len(stamps)):
            when, unit = stamps[at], rows[at]
            if later and when >= later[0]:
                break
            target = unit[7] if len(unit) > 7 else 0
            if unit[5] <= 0:
                break
            if not target:
                continue
            if trace.role(target) == "tank" or held_side(sides, target, when) is not None:
                back, on = when, target
                break
            last = when
        stop = back if back is not None else last
        out.append({**pull, "back": back, "on_tank": on, "last": last, "under": back is None and bool(later),
                    "taunts": [rec for rec in taunts if since <= rec["t"] <= stop
                               and (rec.get("tgt") == guid or (rec["sp"] in AREA_TAUNTS and rec.get("s") == on))]})
    return out


def pull_return_text(trace: Trace, row: dict) -> str:
    taunts = ", ".join(f"{BURROWER_TAUNTS[rec['sp']]} by {trace.name(rec.get('s'))}"
                       f" {(rec['t'] - row['t']) / 1000:+.1f} s" for rec in row["taunts"])
    if row["back"] is None:
        return (f"never back on a tank, on non-tanks {(row['last'] - row['t']) / 1000:.1f} s to its last sample"
                + (" before it went under" if row["under"] else "") + (f"; taunts at it: {taunts}" if taunts else ""))
    return (f"back on {trace.name(row['on_tank'])} after {(row['back'] - row['t']) / 1000:.1f} s, "
            + (f"taunted: {taunts}" if taunts else "no taunt"))


def show_pulls(trace: Trace, samples: Samples) -> None:
    rows = pull_returns(trace, burrower_pulls(trace, samples), samples)
    print(f"\n  pulled off its side tank by a non-tank within {PULL_WINDOW_MS / 1000:.0f} s of coming up: the seconds"
          f" since it came up and\n  on the side tank, and the puller's casts on it in the {PULL_CASTS_MS / 1000:.0f} s"
          " before, triggered ones marked *:" + ("" if rows else " none"))
    for row in rows:
        casts = ", ".join(f"{trace.spell(rec['sp'])}{'*' if rec.get('tr') else ''}"
                          f" +{(rec['t'] - row['up']) / 1000:.1f} s" for rec in row["casts"])
        print(f"    {short(trace, row['guid']):10} wave {row['wave'] or '-'} {'emerge' if row['emerged'] else 'sight'}"
              f" {clock(row['up'])}: {trace.name(row['tank'])} (side {row['side']}) > {trace.name(row['puller'])}"
              f" ({trace.role(member_of(trace, row['puller']))}) +{(row['t'] - row['up']) / 1000:.1f} s,"
              f" {(row['t'] - row['on']) / 1000:.1f} s on the side tank; casts on it: {casts or 'none'}")
        print(f"      {pull_return_text(trace, row)}")
    if rows:
        pullers = collections.Counter(row["puller"] for row in rows)
        print("  pulls per puller: " + ", ".join(f"{trace.name(guid)} {n}" for guid, n in pullers.most_common()))


def burrower_frames(trace: Trace, samples: Samples) -> list[tuple[int, list[list]]]:
    """Each snapshot's living burrower rows, in time order."""
    by_stamp: dict[int, list[list]] = collections.defaultdict(list)
    for guid in guids_of_entry(trace, NPC_BURROWER):
        for when, row in zip(*samples.track(guid)):
            if len(row) > 7 and row[5] > 0:
                by_stamp[when].append(row)
    return sorted(by_stamp.items())


def side_tank_crowds(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each run of snapshots with TANK_CROWD or more living burrowers on one side tank, read off their
    targets: the tank, the run's first and last snapshot, the most at once, `alone` when no other side tank
    lived at its start, and the tank's BURROWER_TAUNTS in the TANK_CROWD_CAST_MS before it by name."""
    samples = samples or Samples(trace)
    tanks = side_tank_list(trace)
    casts = [rec for rec in trace.of("cast") if rec.get("s") in tanks and rec.get("sp") in BURROWER_TAUNTS]
    out: list[dict] = []
    running: dict[int, dict] = {}
    for when, rows in burrower_frames(trace, samples):
        on = collections.Counter(row[7] for row in rows)
        for tank in tanks:
            count = on.get(tank, 0)
            if count < TANK_CROWD:
                running.pop(tank, None)
                continue
            run = running.get(tank)
            if run is None:
                run = running[tank] = {"tank": tank, "start": when, "stop": when, "peak": count}
                out.append(run)
            run["stop"] = when
            run["peak"] = max(run["peak"], count)

    for run in out:
        run["alone"] = not any((health(samples, tank, run["start"]) or 0.0) > 0.0
                               for tank in tanks if tank != run["tank"])
        run["taunts"] = collections.Counter(
            BURROWER_TAUNTS[rec["sp"]] for rec in casts
            if rec["s"] == run["tank"] and run["start"] - TANK_CROWD_CAST_MS <= rec["t"] <= run["start"])
    return out


def frenzy_samples(trace: Trace, samples: Samples | None = None) -> list[tuple[str, int, int]]:
    """Per phase span, `(label, both, close)`: the snapshots with burrowers on both latched side tanks, and
    those where one on each sits within SPIDER_FRENZY_YD of one on the other."""
    samples = samples or Samples(trace)
    end = pull_end(trace)
    sides = [latch_spans(trace, f"anub.side{side}", end) for side in (0, 1)]
    spans = phase_spans(trace)
    counts: dict[str, list[int]] = {}
    for when, rows in burrower_frames(trace, samples):
        tanks = [as_guid(held_at(sides[side], when)) for side in (0, 1)]
        if not all(tanks):
            continue
        groups = [[row[1:3] for row in rows if row[7] == tank] for tank in tanks]
        if not all(groups):
            continue
        tally = counts.setdefault(phase_at(spans, when), [0, 0])
        tally[0] += 1
        if min(dist2(a, b) for a in groups[0] for b in groups[1]) < SPIDER_FRENZY_YD:
            tally[1] += 1
    return [(label, both, close) for label, (both, close) in counts.items()]


# a tank's taunts onto one burrower, Righteous Defense's once per burrower it took. A taunt lasts TAUNT_MS,
# and one still on the tank TAUNT_KEPT_MS after that was kept. anubarak side tank taunts wait for the
# burrowers within TAUNT_ARRIVE_YD (ANUBARAK_TAUNT_ARRIVE).
PICKUP_TAUNTS = {spell: name for spell, name in BURROWER_TAUNTS.items()
                 if spell in (355, 62124, 56222, 6795, SPELL_RIGHTEOUS_DEFENSE_TAUNT)}
TAUNT_MS = 3000
TAUNT_KEPT_MS = 1500
TAUNT_ARRIVE_YD = 40.0


def pickup_taunts(trace: Trace, samples: Samples) -> list[dict]:
    """Each tank taunt onto a burrower: the yards between them at the cast, and the burrower's target
    TAUNT_KEPT_MS after the taunt's TAUNT_MS (`to`, None when it has no fresh sample then)."""
    burrowers = guids_of_entry(trace, NPC_BURROWER)
    out = []
    for rec in trace.of("cast"):
        tank, target = rec.get("s"), rec.get("tgt")
        if rec.get("sp") not in PICKUP_TAUNTS or target not in burrowers or trace.role(tank) != "tank":
            continue
        mine, its = samples.recent(tank, rec["t"]), samples.recent(target, rec["t"])
        if not mine or not its:
            continue
        later = samples.recent(target, rec["t"] + TAUNT_MS + TAUNT_KEPT_MS)
        out.append({"t": rec["t"], "tank": tank, "guid": target, "spell": rec["sp"],
                    "yd": dist2(mine[1:3], its[1:3]), "to": later[7] if later and later[5] > 0 else None})
    return out


def show_pickup_taunts(trace: Trace, samples: Samples) -> None:
    rows = pickup_taunts(trace, samples)
    if not rows:
        print("  tank taunts onto a burrower: none")
        return

    def band(group: list[dict]) -> str:
        seen = [row for row in group if row["to"] is not None]
        if not group:
            return "none"
        lost = collections.Counter(trace.name(row["to"]) for row in seen if row["to"] != row["tank"])
        text = f"kept {sum(1 for row in seen if row['to'] == row['tank'])} of {len(seen)}"
        if lost:
            text += " (lost to " + ", ".join(f"{name} {n}" for name, n in lost.most_common()) + ")"
        if len(seen) < len(group):
            text += f", {len(group) - len(seen)} gone"
        return text

    print(f"  tank taunts onto a burrower, kept when still on the tank {TAUNT_KEPT_MS / 1000:.1f} s after the"
          f" taunt's {TAUNT_MS / 1000:.0f} s, by the yards at the cast:")
    keys = sorted({(trace.name(row["tank"]), row["tank"], row["spell"]) for row in rows})
    for name, tank, spell in keys:
        mine = [row for row in rows if row["tank"] == tank and row["spell"] == spell]
        print(f"    {name[:14]:14} {PICKUP_TAUNTS[spell]:17} within {TAUNT_ARRIVE_YD:.0f} yd"
              f" {band([row for row in mine if row['yd'] <= TAUNT_ARRIVE_YD])}; beyond"
              f" {band([row for row in mine if row['yd'] > TAUNT_ARRIVE_YD])}")


def show_side_tank_crowds(trace: Trace, samples: Samples) -> None:
    crowds = side_tank_crowds(trace, samples)
    print(f"\n  {TANK_CROWD} or more burrowers on one side tank, read off their targets: the most at once, and the"
          f" tank's taunts in\n  the {TANK_CROWD_CAST_MS / 1000:.0f} s before, Righteous Defense once per burrower it"
          " took:" + ("" if crowds else " none"))
    for run in crowds:
        taunts = ", ".join(f"{name} {n}" for name, n in run["taunts"].most_common()) or "no taunt"
        print(f"    {clock(run['start'])}-{clock(run['stop'])} {trace.name(run['tank'])} {run['peak']}"
              + (", the only side tank alive" if run["alone"] else "") + f"; {taunts}")
    frenzy = frenzy_samples(trace, samples)
    print(f"  the two side tanks' burrowers within Spider Frenzy's {SPIDER_FRENZY_YD:.0f} yd of each other, per phase,"
          " of the samples\n  with both holding one: "
          + (", ".join(f"{label} {close} of {both}" for label, both, close in frenzy) or "none"))


def p3wave_rows(trace: Trace) -> list[tuple[int, str]]:
    """`anub.p3wave` as one stream: whichever bot reads it writes it, so a value two bots wrote counts once."""
    out: list[tuple[int, str]] = []
    for rec in sorted(notes(trace, "anub.p3wave"), key=lambda rec: rec["t"]):
        value = str(rec.get("txt", ""))
        if not out or out[-1][1] != value:
            out.append((rec["t"], value))
    return out


def wave_reads(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower wave and side its burrowers came up on: the first of them up, the milliseconds from there to
    that side's first focus pick of one, None with none, and the side tank then with its peak Expose Weakness
    to the next wave. In phase 3 also the wave's `anub.p3wave` rows, when its last burrower died, None for one
    alive at the end, and each side tank's death from the wave's spawn to then, None while it lived."""
    samples = samples or Samples(trace)
    waves = wave_starts(trace, samples)
    if not waves:
        return []
    end = pull_end(trace)
    pickups = pickup_rows(trace, samples)
    died = {row["guid"]: row["died"] for row in burrower_rows(trace, samples)}
    focus = dict(focus_latches(trace))
    sides = side_spans(trace)
    tracks = expose_tracks(trace)
    phases = phase_spans(trace)
    p3waves = p3wave_rows(trace)
    deaths = combat_deaths(trace)
    out = []
    for index, (wave, spawn) in enumerate(waves):
        upto = waves[index + 1][1] if index + 1 < len(waves) else end
        mine = [row for row in pickups if row["wave"] == wave]
        phase = next((number for low, high, number, _ in phases if low <= spawn < high), None)
        row = {"wave": wave, "start": spawn, "phase": phase, "sides": []}
        for side in (0, 1):
            bugs = {pickup["guid"]: pickup["spawn"] for pickup in mine if pickup["side"] == side}
            if not bugs:
                continue
            up = min(bugs.values())
            picks = [start for value, start, _ in focus.get(side, focus.get(None, [])) if as_guid(value) in bugs]
            tank = as_guid(held_at(sides[side], up)) or None
            row["sides"].append({"side": side, "up": up, "pick": max(0, min(picks) - up) if picks else None,
                                 "tank": tank,
                                 "peak": stack_window(tracks.get(tank, []), spawn, upto)[0] if tank else None})
        if phase == 3:
            lasts = [died.get(pickup["guid"]) for pickup in mine]
            gone = None if not lasts or None in lasts else max(lasts)
            row["p3wave"] = [(when, value) for when, value in p3waves if spawn <= when < upto]
            row["died"] = gone
            for entry in row["sides"]:
                entry["death"] = next((rec["t"] for rec in deaths if rec.get("g") == entry["tank"]
                                       and spawn <= rec["t"] <= (gone if gone is not None else end)), None)
        out.append(row)
    return out


def show_wave_reads(trace: Trace, samples: Samples) -> None:
    rows = wave_reads(trace, samples)
    if not rows:
        return
    print("\n  per wave and side: seconds from its burrowers coming up to that side's first focus pick of one, and the"
          "\n  side tank's peak Expose Weakness to the next wave; in phase 3 the anub.p3wave rows and whether the side"
          "\n  tanks lived until the wave died:")
    for row in rows:
        parts = []
        for entry in row["sides"]:
            pick = "no pick" if entry["pick"] is None else f"pick +{entry['pick'] / 1000:.1f} s"
            tank = "no side tank" if entry["tank"] is None else f"{trace.name(entry['tank'])} peak {entry['peak']}"
            parts.append(f"side {entry['side']} {pick}, {tank}")
        text = f"    wave {row['wave']:<3} {clock(row['start']):>9}  " + ("; ".join(parts) or "no burrower read")
        if row["phase"] == 3:
            rows3 = ", ".join(f"{value} {(when - row['start']) / 1000:+.1f} s" for when, value in row["p3wave"])
            gone = "outlived the pull" if row["died"] is None else f"died {clock(row['died'])}"
            fates = ", ".join(f"{trace.name(entry['tank'])} " + (
                "lived" if entry["death"] is None else f"died {(entry['death'] - row['start']) / 1000:+.1f} s")
                for entry in row["sides"] if entry["tank"] is not None)
            text += (f"\n          P3: anub.p3wave {rows3 or 'none'}; the wave {gone}"
                     + (f"; {fates}" if fates else ""))
        print(text)
    if any(row["phase"] == 3 for row in rows) and not notes(trace, "anub.p3wave"):
        print("  no anub.p3wave rows")


def loose_burrower_time(trace: Trace, samples: Samples | None = None) -> dict[int, collections.Counter]:
    """Per melee bot, the milliseconds of the pull it targeted a living burrower that no living tank within
    MELEE_YD held, by its `anub.dps` value then, `-` before its first. A snapshot stands for the time since the
    bot's one before, capped at MAX_STEP_MS."""
    samples = samples or Samples(trace)
    burrowers = guids_of_entry(trace, NPC_BURROWER)
    tanks = {guid for guid in roster_guids(trace) if trace.role(guid) == "tank"}
    branches = per_bot(trace, "anub.dps")
    end = pull_end(trace)
    out = {}
    for guid, role in dps_bots(trace).items():
        if role != "melee":
            continue
        stamps, rows = samples.track(guid)
        picks = branches.get(guid, [])
        times = [when for when, _ in picks]
        held: collections.Counter = collections.Counter()
        for index in range(1, len(stamps)):
            when, row = stamps[index], rows[index]
            if not 0 <= when <= end or row[5] <= 0 or len(row) <= 7 or row[7] not in burrowers:
                continue
            bug = samples.recent(row[7], when)
            if bug is None or bug[5] <= 0:
                continue
            victim = bug[7] if len(bug) > 7 else 0
            holder = samples.recent(victim, when) if victim in tanks else None
            if holder is not None and holder[5] > 0 and dist2(holder[1:3], bug[1:3]) <= MELEE_YD:
                continue
            at = bisect.bisect_right(times, when) - 1
            held[picks[at][1] if at >= 0 else "-"] += min(when - stamps[index - 1], MAX_STEP_MS)
        if held:
            out[guid] = held
    return out


def show_loose_burrowers(trace: Trace, samples: Samples) -> None:
    rows = loose_burrower_time(trace, samples)
    print(f"  melee bot seconds on a burrower no tank within {MELEE_YD:.0f} yd held, by anub.dps then: " + ("; ".join(
        f"{trace.name(guid)} {sum(held.values()) / 1000:.1f} s ("
        + ", ".join(f"{branch} {ms / 1000:.1f}" for branch, ms in held.most_common()) + ")"
        for guid, held in sorted(rows.items(), key=lambda item: -sum(item[1].values()))) or "none"))


def ranged_capped(trace: Trace) -> dict[int, int]:
    """`anub.dps` `capped` rows per ranged DPS bot: each a pick moved onto him off a burrower it was capped on."""
    ranged = {guid for guid, role in dps_bots(trace).items() if role == "ranged"}
    return {guid: counts["capped"] for guid, counts in value_counts(trace, "anub.dps").items()
            if guid in ranged and counts.get("capped")}


def show_burrowers(trace: Trace) -> None:
    print("NERUBIAN BURROWERS")
    samples = Samples(trace)
    rows = burrower_rows(trace, samples)
    if not rows:
        print("  no burrower sampled")
    else:
        print(f"  {'burrower':10} {'first':>9} {'last':>9} {'low':>6} {'end':>6} {'submerged':>9}"
              f" {'on frost':>8} {'strikes':>7} {'landed':>6}")
        for row in rows:
            print(f"  {short(trace, row['guid']):10} {clock(row['first']):>9} {clock(row['last']):>9}"
                  f" {row['low']:5.1f}% {row['end']:5.1f}% {row['submerges']:9} {row['frost'] * 100:7.0f}%"
                  f" {row['strikes']:7} {row['landed']:6}")
        print(f"\n  on frost is the share of its samples within {BURROWER_PERMAFROST_REACH:.1f} yd of a"
              " Permafrost centre\n  a submerge is its health going back to full")
        show_burrower_windows(trace, samples)
        show_wave_reads(trace, samples)
        show_pickups(trace, samples)
        show_pickup_taunts(trace, samples)
        show_burrower_submerges(trace, samples)
        show_side_tank_crowds(trace, samples)

    held = sorted({trace.name(guid) for guid, counts in value_counts(trace, "anub.interrupter").items()
                   if counts.get("1")})
    print(f"  ever held Shadow Strike duty: {', '.join(held) if held else 'nobody'}")

    focus = focus_rows(trace)
    if not focus:
        print("  no focus latch ever held a burrower")
    else:
        print(f"\n  {'focus':10} {'held':>7} {'dps on it':>9} side")
        for row in focus:
            share = "-" if row["share"] is None else f"{row['share'] * 100:.0f}%"
            print(f"  {short(trace, row['guid']):10} {row['held'] / 1000:6.1f}s {share:>9}"
                  f" {focus_label(row['side']):>4}")
        print("  dps on it is the share of living DPS bot samples whose target was the focus while it held, over"
              " the\n  side's anub.dside group when the trace has one; side - is the single anub.focus of older"
              " traces")
    show_focus_picks(trace, samples)

    stray = stray_burrower_casts(trace)
    if stray:
        casters = collections.Counter(rec["s"] for rec in stray)
        spells = collections.Counter(rec["sp"] for rec in stray)
        print(f"  DPS casts at a burrower other than the focus: {len(stray)}, by "
              + ", ".join(f"{trace.name(guid)} {n}" for guid, n in casters.most_common())
              + "; most: " + ", ".join(f"{trace.spell(spell)} {n}" for spell, n in spells.most_common(5)))
    else:
        print("  no DPS cast at a burrower other than the focus")
    show_burrower_threat(trace, samples)
    if notes(trace, "anub.focusdue"):
        drops = focus_due_drops(trace, samples)
        print("  anub.focusdue dropped with the focus alive: "
              + (", ".join(f"{clock(row['t'])} {short(trace, row['guid'])} at {row['hp']:.0f}%" for row in drops)
                 if drops else "never"))
    else:
        print("  anub.focusdue dropped with the focus alive: no anub.focusdue rows")
    show_counts(trace, "anub.dps")
    capped = ranged_capped(trace)
    print("  anub.dps capped rows per ranged DPS bot, a pick moved onto him off a burrower it was capped on: "
          + (", ".join(f"{trace.name(guid)} {n}" for guid, n in sorted(capped.items(), key=lambda item: -item[1]))
             or "none"))
    show_loose_burrowers(trace, samples)
    if notes(trace, "anub.mstand"):
        show_counts(trace, "anub.mstand")
    if notes(trace, "anub.lead"):
        show_counts(trace, "anub.lead")
    if rows:
        show_pulls(trace, samples)


# ------------------------------------------------------------------------------------------------ dps

def target_kind(trace: Trace, guid) -> str:
    if not guid:
        return "none"
    return {NPC_ANUBARAK: "boss", NPC_BURROWER: "burrower", NPC_SCARAB: "scarab"}.get(trace.entries.get(guid), "other")


def dps_bots(trace: Trace) -> dict[int, str]:
    """Each melee and ranged bot on the roster, by role. Humans are left out."""
    return {guid: trace.role(guid) for guid in roster_guids(trace)
            if trace.role(guid) in DPS_ROLES and guid not in trace.humans}


def move_orders(trace: Trace) -> dict[int, tuple[list[int], list[str]]]:
    """Per bot, its `move` rows' stamps and the action each one names in `by`, in time order."""
    out: dict[int, tuple[list[int], list[str]]] = {}
    for rec in sorted(trace.of("move"), key=lambda rec: rec["t"]):
        stamps, orders = out.setdefault(rec.get("g"), ([], []))
        stamps.append(rec["t"])
        orders.append(rec.get("by") or "?")
    return out


def dps_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per phase span and DPS role, over its bots' living samples: the samples per target kind and those
    flagged moving, and per step to the next sample the damage dealt, charged to the target kind held at
    the step's start, and the yards walked, charged to the `by` of the bot's last `move` row within
    MOVE_ROW_MS. A step under WALK_MIN_YD or over WALK_MAX_YD, or across a snapshot hole, walks nothing.
    `hp` is his health at the span's two edges."""
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    bots = dps_bots(trace)
    orders = move_orders(trace)
    out = []
    for start, stop, phase, label in phase_spans(trace):
        if not phase:
            continue
        roles = {role: {"bots": set(), "samples": 0, "moving": 0, "kinds": collections.Counter(),
                        "dealt": collections.Counter(), "walked": collections.Counter(), "yards": 0.0}
                 for role in DPS_ROLES}
        for guid, role in bots.items():
            group = roles[role]
            stamps, rows = samples.track(guid)
            moved, by = orders.get(guid, ([], []))
            for index in range(bisect.bisect_left(stamps, start), bisect.bisect_left(stamps, stop)):
                row = rows[index]
                if row[5] <= 0:
                    continue
                kind = target_kind(trace, row[7] if len(row) > 7 else 0)
                group["bots"].add(guid)
                group["samples"] += 1
                group["kinds"][kind] += 1
                group["moving"] += bool(len(row) > 8 and row[8])
                if index + 1 >= len(rows):
                    continue
                after = rows[index + 1]
                if len(row) > 11 and len(after) > 11 and after[11] > row[11]:
                    group["dealt"][kind] += after[11] - row[11]
                step = dist2(row[1:3], after[1:3])
                if stamps[index + 1] - stamps[index] > MAX_STEP_MS or not WALK_MIN_YD <= step <= WALK_MAX_YD:
                    continue
                last = bisect.bisect_right(moved, stamps[index]) - 1
                order = by[last] if last >= 0 and stamps[index] - moved[last] <= MOVE_ROW_MS else "no move row"
                group["walked"][order] += step
                group["yards"] += step
        out.append({"label": label, "phase": phase, "start": start, "stop": stop,
                    "hp": (health(samples, boss, start), health(samples, boss, stop - 1)), "roles": roles})
    return out


def boss_lost(row: dict) -> float | None:
    before, after = row["hp"]
    return None if before is None or after is None else before - after


def side_groups(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower wave, each side's `anub.dside` group NEAR_MS after the wave landed: its living bots by
    role. Empty without anub.dside rows."""
    groups = per_bot(trace, "anub.dside")
    if not groups:
        return []
    samples = samples or Samples(trace)
    waves = wave_starts(trace, samples)
    end = pull_end(trace)
    out = []
    for index, (wave, spawn) in enumerate(waves):
        upto = waves[index + 1][1] if index + 1 < len(waves) else end
        when = max(spawn, min(spawn + NEAR_MS, upto - 1))
        sides: dict[int, dict[str, list[int]]] = {side: {role: [] for role in DPS_ROLES} for side in (0, 1)}
        for guid in sorted(groups, key=trace.name):
            side = dside_at(groups, guid, when)
            row = samples.recent(guid, when)
            if side is None or row is None or row[5] <= 0 or trace.role(guid) not in DPS_ROLES:
                continue
            sides[side][trace.role(guid)].append(guid)
        out.append({"wave": wave, "t": spawn, "sides": sides})
    return out


def midline_half(noted, point, when: int) -> int | None:
    """Which side spot's half of the room `point` stands in at `when`: None within MIDLINE_BAND_YD of the
    midline between them, or while either spot is unread."""
    spots = [held_at(spans, when) for spans in noted]
    if None in spots:
        return None
    (x0, y0), (x1, y1) = spots
    gap = math.hypot(x1 - x0, y1 - y0)
    if gap < 1e-6:
        return None
    along = ((point[0] - (x0 + x1) / 2.0) * (x1 - x0) + (point[1] - (y0 + y1) / 2.0) * (y1 - y0)) / gap
    if abs(along) < MIDLINE_BAND_YD:
        return None
    return 1 if along > 0 else 0


def side_crossings(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each walk of a melee DPS bot from one side spot's half of the room to the other's while he was up.
    A sample on the midline keeps the half the bot last stood in, and a death, a snapshot hole or a new
    window forgets it. `group` is the bot's `anub.dside` side then, None without one."""
    samples = samples or Samples(trace)
    noted = spot_spans(trace)
    groups = per_bot(trace, "anub.dside")
    melee = [guid for guid, role in dps_bots(trace).items() if role == "melee"]
    up = [(start, stop, label) for start, stop, phase, label in phase_spans(trace) if phase in (1, 3)]
    out = []
    for guid in melee:
        stamps, rows = samples.track(guid)
        for start, stop, label in up:
            half = last = None
            for index in range(bisect.bisect_left(stamps, start), bisect.bisect_left(stamps, stop)):
                when, row = stamps[index], rows[index]
                if row[5] <= 0 or (last is not None and when - last > MAX_STEP_MS):
                    half = None
                last = when
                now = midline_half(noted, row[1:3], when) if row[5] > 0 else None
                if now is None:
                    continue
                if half is not None and now != half:
                    out.append({"guid": guid, "t": when, "label": label, "frm": half, "to": now,
                                "group": dside_at(groups, guid, when)})
                half = now
    return sorted(out, key=lambda row: (row["t"], row["guid"]))


def show_dps_role(role: str, group: dict, seconds: float) -> None:
    samples = group["samples"]
    bots = len(group["bots"])
    kinds = ", ".join(f"{kind} {group['kinds'][kind] * 100 / samples:.0f}%" for kind in DPS_KINDS)
    print(f"    {role:6} {bots:2} bot(s): target {kinds}; moving {group['moving'] * 100 / samples:.0f}%;"
          f" {group['yards'] / bots:.0f} yd a bot")
    walked = group["walked"].most_common()
    if walked:
        rest = sum(yards for _, yards in walked[WALKED_SHOWN:])
        print(f"           walked {group['yards']:.0f} yd: "
              + ", ".join(f"{order} {yards:.0f}" for order, yards in walked[:WALKED_SHOWN])
              + (f", the rest {rest:.0f}" if rest else ""))
    dealt = sum(group["dealt"].values())
    if dealt:
        print(f"           dealt {dealt / seconds / 1000:.1f}k/s: "
              + ", ".join(f"{kind} {group['dealt'][kind] * 100 / dealt:.0f}%" for kind in DPS_KINDS))
    else:
        print("           dealt nothing")


def show_side_groups(trace: Trace, samples: Samples) -> None:
    waves = side_groups(trace, samples)
    if not waves:
        print("  no anub.dside rows, so no side groups")
        return
    print(f"\n  each anub.dside group {NEAR_MS / 1000:.0f} s after the wave landed, living bots:")
    previous = None
    for row in waves:
        head = f"    wave {row['wave']} {clock(row['t']):>9}"
        if row["sides"] == previous:
            print(f"{head}  as before")
            continue
        previous = row["sides"]
        for side in (0, 1):
            members = "; ".join(f"{role} " + ", ".join(trace.name(guid) for guid in row["sides"][side][role])
                                for role in DPS_ROLES if row["sides"][side][role])
            print(f"{head if side == 0 else ' ' * len(head)}  side {side}: {members or 'nobody'}")


def show_crossings(trace: Trace, samples: Samples) -> None:
    if not all(spot_spans(trace)):
        print("  no anub.spot0 and anub.spot1 rows, so no walks across the midline")
        return
    rows = side_crossings(trace, samples)
    print("\n  melee walks from one side spot's half to the other's while he was up: the halves meet at the"
          f" midline\n  between anub.spot0 and anub.spot1, and {MIDLINE_BAND_YD:.0f} yd either side of it belongs to"
          " neither; astray is a walk into\n  the half of the side its anub.dside group doesn't serve:")
    for start, _, phase, label in phase_spans(trace):
        if phase not in (1, 3):
            continue
        mine = [row for row in rows if row["label"] == label]
        per: dict[int, list[dict]] = collections.defaultdict(list)
        for row in mine:
            per[row["guid"]].append(row)
        bots = ", ".join(
            f"{trace.name(guid)} {len(walks)}"
            + (f" ({sum(1 for row in walks if row['group'] is not None and row['to'] != row['group'])} astray)"
               if any(row["group"] is not None for row in walks) else "")
            for guid, walks in sorted(per.items(), key=lambda item: (-len(item[1]), trace.name(item[0]))))
        print(f"    {label:5} {clock(start):>9}  {len(mine)} walk(s)" + (f": {bots}" if bots else ""))


def push_reads(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per phase 1, the first `anub.push` row in it (`t` and `txt` None without one) with his health then, and
    `then`, what ended it: `phase 3`, `submerged` or `pull ended`."""
    samples = samples or Samples(trace)
    spans = phase_spans(trace)
    boss = boss_guid(trace)
    rows = notes(trace, "anub.push")
    out = []
    for index, (start, stop, value, label) in enumerate(spans):
        if value != 1:
            continue
        after = spans[index + 1][2] if index + 1 < len(spans) else None
        first = next((rec for rec in rows if start <= rec["t"] < stop), None)
        out.append({"label": label, "start": start, "stop": stop,
                    "then": {3: "phase 3", 2: "submerged"}.get(after, "pull ended"),
                    "t": first["t"] if first else None, "txt": str(first.get("txt", "")) if first else None,
                    "hp": health(samples, boss, first["t"]) if first else None})
    return out


def push_text(row: dict) -> str:
    if row["t"] is None:
        return f"{row['label']} no push, {row['then']}"
    hp = "-" if row["hp"] is None else f"{row['hp']:.1f}%"
    return (f"{row['label']} {row['txt']} at {hp}, {(row['stop'] - row['t']) / 1000:.0f} s to its end,"
            f" {row['then']}")


def show_dps(trace: Trace) -> None:
    print("DPS")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    samples = Samples(trace)
    rows = dps_rows(trace, samples)
    if not rows:
        print("  no anub.phase rows")
        return
    end = pull_end(trace)
    for row in rows:
        seconds = max(row["stop"] - row["start"], 1) / 1000
        lost = boss_lost(row)
        health_text = "boss unsampled"
        if lost is not None:
            health_text = f"boss {row['hp'][0]:.1f}% -> {row['hp'][1]:.1f}%, {lost:.1f}% lost"
            if row["phase"] == 1 and lost < BOSS_PACE_PCT:
                health_text += f"  under {BOSS_PACE_PCT:.0f}%"
                if row["stop"] >= end:
                    health_text += ", the pull ended in it"
        print(f"  {row['label']:5} {clock(row['start']):>9} {seconds:6.1f} s  {health_text}")
        for role in DPS_ROLES:
            if row["roles"][role]["samples"]:
                show_dps_role(role, row["roles"][role], seconds)

    surfaced = [row for row in rows if row["phase"] == 1 and boss_lost(row) is not None]
    print("\n  boss health lost per phase 1: " + (", ".join(
        f"{row['label']} {boss_lost(row):.1f}%" + (" under" if boss_lost(row) < BOSS_PACE_PCT else "")
        for row in surfaced) or "none sampled"))
    print("  anub.push per phase 1 (on:<seconds needed>:<seconds left>), his health as it latched, and what"
          " ended the phase: " + ("; ".join(push_text(row) for row in push_reads(trace, samples)) or "none"))
    bots = dps_bots(trace)
    branches = {role: collections.Counter() for role in DPS_ROLES}
    for guid, counts in value_counts(trace, "anub.dps").items():
        if guid in bots:
            branches[bots[guid]].update(counts)
    print("  anub.dps rows per role: " + ("; ".join(
        f"{role} " + ", ".join(f"{value} {n}" for value, n in counts.most_common())
        for role, counts in branches.items() if counts) or "none"))
    print("  target is the share of living samples by the bot's target, moving the share flagged moving; walked"
          f"\n  charges each snapshot step to the action of the bot's last move row within {MOVE_ROW_MS // 1000} s,"
          f" steps under\n  {WALK_MIN_YD} or over {WALK_MAX_YD:.0f} yd left out; dealt is the bot's damage, its pets'"
          " included, charged to the target\n  it held at the step's start")
    show_side_groups(trace, samples)
    show_crossings(trace, samples)


# ---------------------------------------------------------------------------------------------- frost

def frost_groups(trace: Trace, samples: Samples) -> dict[int, str]:
    """Each roster member's group: `side tank` for any holder of `anub.side0` or `anub.side1`, `main tank` for
    the other tank he targeted most, else its role."""
    _, holders = side_tanks(trace)
    sides = {guid for guids in holders.values() for guid in guids}
    _, rows = samples.track(boss_guid(trace))
    hit = collections.Counter(row[7] for row in rows
                              if len(row) > 7 and row[7] not in sides and trace.role(row[7]) == "tank")
    main = hit.most_common(1)[0][0] if hit else None
    return {guid: "side tank" if guid in sides else "main tank" if guid == main else trace.role(guid)
            for guid in roster_guids(trace)}


def frost_stints(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each Permafrost stint on the roster: the bot and its group, the milliseconds of it while he was down,
    the phase it began in and the bot's target then. One still on when the trace ends runs to the end."""
    samples = samples or Samples(trace)
    groups = frost_groups(trace, samples)
    spans = phase_spans(trace)
    down = [(start, stop) for start, stop, phase, _ in spans if phase == 2]
    out = []
    for guid, stints in aura_spans(trace, PERMAFROST).items():
        if guid not in groups:
            continue
        for start, stop, closed in stints:
            row = samples.row(guid, start)
            out.append({"guid": guid, "group": groups[guid], "start": start, "stop": stop, "closed": closed,
                        "down": covered(down, start, stop), "phase": phase_at(spans, start),
                        "target": row[7] if row and len(row) > 7 else 0})
    return sorted(out, key=lambda row: (row["start"], row["guid"]))


def frost_totals(stints: list[dict]) -> dict[int, dict]:
    """Per bot: its group, milliseconds carrying Permafrost up and down, stint count and longest stint."""
    out: dict[int, dict] = {}
    for row in stints:
        bot = out.setdefault(row["guid"], {"group": row["group"], "up": 0, "down": 0, "stints": 0, "longest": 0})
        span = row["stop"] - row["start"]
        bot["up"] += span - row["down"]
        bot["down"] += row["down"]
        bot["stints"] += 1
        bot["longest"] = max(bot["longest"], span)
    return out


def show_frost(trace: Trace) -> None:
    print("PERMAFROST")
    stints = frost_stints(trace)
    if not stints:
        print("  nobody on the roster carried Permafrost")
    else:
        totals = frost_totals(stints)

        def rank(group: str) -> int:
            return FROST_GROUPS.index(group) if group in FROST_GROUPS else len(FROST_GROUPS)

        groups: dict[str, list[dict]] = collections.defaultdict(list)
        for guid, bot in totals.items():
            if guid not in trace.humans:
                groups[bot["group"]].append(bot)
        print(f"  seconds carrying it, bots only:\n  {'group':10} {'bots':>4} {'up':>7} {'down':>7} {'total':>7}")
        for group in sorted(groups, key=rank):
            up = sum(bot["up"] for bot in groups[group]) / 1000
            down = sum(bot["down"] for bot in groups[group]) / 1000
            print(f"  {group:10} {len(groups[group]):4} {up:7.1f} {down:7.1f} {up + down:7.1f}")

        print(f"\n  {'bot':14} {'group':10} {'up':>7} {'down':>7} {'total':>7} {'stints':>6} {'longest':>7}")
        for guid, bot in sorted(totals.items(), key=lambda item: (rank(item[1]["group"]),
                                                                    -(item[1]["up"] + item[1]["down"]))):
            print(f"  {trace.name(guid)[:14]:14} {bot['group']:10} {bot['up'] / 1000:7.1f} {bot['down'] / 1000:7.1f}"
                  f" {(bot['up'] + bot['down']) / 1000:7.1f} {bot['stints']:6} {bot['longest'] / 1000:7.1f}"
                  + ("  human" if guid in trace.humans else ""))

        long = [row for row in stints if row["stop"] - row["start"] > FROST_STINT_MS]
        if long:
            print(f"\n  stints over {FROST_STINT_MS / 1000:.0f} s:\n  {'at':>9} {'bot':14} {'group':10} {'secs':>7}"
                  f" {'phase':5} target at its start")
        else:
            print(f"\n  no stint over {FROST_STINT_MS / 1000:.0f} s")
        for row in long:
            print(f"  {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} {row['group']:10}"
                  f" {(row['stop'] - row['start']) / 1000:5.1f} s {row['phase']:5} {unit_label(trace, row['target'])}"
                  + ("  human" if row["guid"] in trace.humans else "")
                  + ("" if row["closed"] else "  on at the end"))
        print("  up is every moment outside a phase 2 anub.phase span; side tanks are every anub.side0 and"
              " anub.side1\n  holder, the main tank the other tank he targeted most")
    if notes(trace, "anub.leave"):
        show_counts(trace, "anub.leave")


# --------------------------------------------------------------------------------------------- threat

def surfaced_windows(spans) -> list[dict]:
    """Each P1 or P3 span, and what came before it: the pull, an emerge, or the swarm starting."""
    out = []
    for index, (start, stop, phase, label) in enumerate(spans):
        if phase not in (1, 3):
            continue
        before = spans[index - 1][2] if index else 0
        kind = "emerge" if before == 2 else ("swarm" if before == 1 else "pull")
        out.append({"label": label, "kind": kind, "start": start, "stop": stop})
    return out


def held_at(spans, when: int) -> str | None:
    return next((value for value, start, stop in spans if start <= when < stop), None)


def drag_anchor(latches, samples: Samples, when: int) -> tuple[float, float] | None:
    """Where the main tank drags him: between both latched tank patches. None with one or none, since
    then the tank holds him where it picked him up."""
    spots = [spot for spot in (latched_patch(spans, samples, when) for spans in latches) if spot]
    if len(spots) == 2:
        return (spots[0][0] + spots[1][0]) / 2.0, (spots[0][1] + spots[1][1]) / 2.0
    return None


def spot_anchor(noted, when: int) -> tuple[float, float] | None:
    """The midpoint of `anub.spot0` and `anub.spot1` at `when`, None while either is unread."""
    spots = [held_at(spans, when) for spans in noted]
    if None in spots:
        return None
    return (spots[0][0] + spots[1][0]) / 2.0, (spots[0][1] + spots[1][1]) / 2.0


def held_off(samples: Samples, boss: int, noted, pickup, start: int, stop: int) -> tuple[int, int | None]:
    """`(ms, from)` of the longest stretch of `[start, stop)` he stood OFF_ANCHOR_YD or more off the midpoint
    of the side spots while `anub.pickup` named no drag leg. Each sample holds to the next one, and a
    snapshot hole ends a stretch."""
    stamps, rows = samples.track(boss)
    best: tuple[int, int | None] = (0, None)
    run, began = 0, None
    for index in range(bisect.bisect_left(stamps, start), bisect.bisect_left(stamps, stop)):
        when = stamps[index]
        upto = min(stamps[index + 1], stop) if index + 1 < len(stamps) else stop
        point = spot_anchor(noted, when)
        off = (point is not None and dist2(rows[index][1:3], point) >= OFF_ANCHOR_YD
               and held_at(pickup, when) not in DRAG_LEGS)
        if not off or upto - when > MAX_STEP_MS:
            run, began = 0, None
            continue
        began = when if began is None else began
        run += upto - when
        if run > best[0]:
            best = (run, began)
    return best


def turn(before, after) -> float:
    """Degrees between two ground headings, 0 when either is no move at all."""
    first, second = math.hypot(*before), math.hypot(*after)
    if not first or not second:
        return 0.0
    cos = (before[0] * after[0] + before[1] * after[1]) / (first * second)
    return math.degrees(math.acos(max(-1.0, min(1.0, cos))))


def boss_legs(samples: Samples, boss: int, start: int, stop: int) -> list[dict]:
    """His walks of LEG_MIN_YD or more, `{start, stop, frm, to, yards}`. A walk is a run of samples with
    his moving flag set, from its first sample to the first still one after it, or to its last when the
    window or a snapshot hole cuts it. Yards are the straight line, and a turn inside a run stays one
    leg: the flag is his `isMoving()`, which the drag waits on."""
    stamps, rows = samples.track(boss)
    legs: list[dict] = []
    leg = None
    last = None
    for when, row in zip(stamps, rows):
        if not start <= when < stop:
            continue
        if leg is not None and when - last > MAX_STEP_MS:
            leg = None
        last = when
        moving = len(row) > 8 and bool(row[8])
        if leg is None:
            if not moving:
                continue
            leg = {"start": when, "frm": row[1:3]}
            legs.append(leg)
        leg["stop"], leg["to"] = when, row[1:3]
        if not moving:
            leg = None
    for leg in legs:
        leg["yards"] = dist2(leg["frm"], leg["to"])
    return [leg for leg in legs if leg["yards"] >= LEG_MIN_YD]


def leg_reversals(legs: list[dict]) -> int:
    """Legs heading more than REVERSAL_DEGREES off the one before."""
    headings = [(leg["to"][0] - leg["frm"][0], leg["to"][1] - leg["frm"][1]) for leg in legs]
    return sum(1 for before, after in zip(headings, headings[1:]) if turn(before, after) > REVERSAL_DEGREES)


def boss_taunts(trace: Trace, samples: Samples, boss: int) -> list[dict]:
    """Taunts cast on him, and Righteous Defense on the friend he was hitting at the time: it taunts that
    friend's attackers, so cast on anyone else it never reached him."""
    out = []
    for rec in trace.of("cast"):
        spell = rec.get("sp")
        if rec.get("tr") or spell not in TAUNT_SPELLS:
            continue
        if spell == SPELL_RIGHTEOUS_DEFENSE:
            row = samples.row(boss, rec["t"])
            if not row or len(row) <= 7 or row[7] != rec.get("tgt"):
                continue
        elif rec.get("tgt") != boss:
            continue
        out.append(rec)
    return out


def patch_share(samples: Samples, guid: int, start: int, stop: int) -> tuple[int, int]:
    """`(inside, living)`: the unit's living samples in the window, and those within
    PERMAFROST_SLOW_REACH of a Permafrost centre."""
    stamps, rows = samples.track(guid)
    inside = living = 0
    for index in range(bisect.bisect_left(stamps, start), bisect.bisect_left(stamps, stop)):
        row = rows[index]
        if row[5] <= 0:
            continue
        living += 1
        inside += any(dist2(row[1:3], centre) <= PERMAFROST_SLOW_REACH for centre in samples.frost_at(stamps[index]))
    return inside, living


def submerge_gaps(latches, samples: Samples, boss: int, edge: int) -> dict | None:
    """His last surfaced spot before the phase 2 edge, and its distance from each latched tank patch."""
    row = samples.row(boss, edge - 1)
    if row is None:
        return None
    gaps = {}
    for side, spans in enumerate(latches):
        point = latched_patch(spans, samples, edge - 1)
        if point is not None:
            gaps[side] = dist2(row[1:3], point)
    return {"spot": (row[1], row[2]), "patches": gaps}


def parse_subspot(text) -> tuple[tuple[float, float] | None, int | None]:
    """An `anub.subspot` value, `x,y,z rN`, as its ground point and N, its clearance in yards; `none` reads
    (None, None)."""
    spot, _, clearance = str(text).partition(" r")
    return parse_spot(spot), int(clearance) if clearance.isdigit() else None


def prestep_rows(samples: Samples, pickups: list[dict], start: int, stop: int) -> list[dict]:
    """Each `anub.pickup` prestep in `[start, stop)`: its tank, the yards it walked from there to its next
    pickup row, else to `stop`, and that row's value and how long after, None with none before `stop`."""
    out = []
    for index, rec in enumerate(pickups):
        if not start <= rec["t"] < stop or str(rec.get("txt", "")) != "prestep":
            continue
        tank = rec.get("g", 0)
        after = next((other for other in pickups[index + 1:] if other.get("g", 0) == tank), None)
        if after is not None and after["t"] >= stop:
            after = None
        here, there = samples.row(tank, rec["t"]), samples.row(tank, after["t"] if after else stop)
        out.append({"t": rec["t"], "guid": tank, "next": str(after.get("txt", "")) if after else None,
                    "after": after["t"] - rec["t"] if after else None,
                    "walked": dist2(here[1:3], there[1:3]) if here and there else None})
    return out


def threat_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    if boss is None:
        return []
    taunts = boss_taunts(trace, samples, boss)
    latches = patch_latches(trace)
    pickups = sorted(notes(trace, "anub.pickup"), key=lambda rec: rec["t"])
    pickup = latch_spans(trace, "anub.pickup", pull_end(trace))
    noted = spot_spans(trace)
    subspots = sorted((rec["t"], str(rec.get("txt", ""))) for rec in notes(trace, "anub.subspot"))
    capped = [rec for rec in trace.of("veto") if rec.get("m") == THREAT_CAP]
    stamps, rows = samples.track(boss)
    spans = phase_spans(trace)
    submerges = {start for start, _, phase, _ in spans if phase == 2}

    out = []
    for window in surfaced_windows(spans):
        start, stop = window["start"], window["stop"]
        held, _ = threat_share(trace, {boss}, lambda when, low=start, high=stop: low <= when < high)
        inside = [(when, row) for when, row in zip(stamps, rows) if start <= when < stop]
        tank = next((when for when, row in inside if len(row) > 7 and trace.role(row[7]) == "tank"), None)
        tanks = collections.Counter(row[7] for _, row in inside if len(row) > 7 and trace.role(row[7]) == "tank")
        main = tanks.most_common(1)[0][0] if tanks else None
        in_patch, living = patch_share(samples, main, start, stop) if main else (0, 0)
        # from the first submerge leg on he's led to the submerge spot and stays there, and the tank's
        # rows read hold again once he arrives
        cutoff = next((rec["t"] for rec in pickups
                       if start <= rec["t"] < stop and str(rec.get("txt", "")) == "submerge"), stop)
        spread = []
        for when, row in inside:
            point = drag_anchor(latches, samples, when) if when < cutoff else None
            if point is not None:
                spread.append(dist2(row[1:3], point))
        legs = boss_legs(samples, boss, start, stop)
        searched = next((when for when, _ in subspots if start <= when < stop), None)
        sub_legs = [str(rec.get("txt", "")) for rec in pickups if searched is not None and searched <= rec["t"] < stop]
        out.append({**window,
                    "held": held,
                    "to_tank": None if tank is None else tank - start,
                    "taunts": [rec for rec in taunts if start <= rec["t"] < stop],
                    "off_anchor": statistics.median(spread) if spread else None,
                    "held_off": held_off(samples, boss, noted, pickup, start, cutoff) if all(noted) else None,
                    "legs": len(legs),
                    "yards": sum(leg["yards"] for leg in legs),
                    "reversals": leg_reversals(legs),
                    "pickup": collections.Counter((rec.get("g", 0), str(rec.get("txt", "")))
                                                  for rec in pickups if start <= rec["t"] < stop),
                    "capped": collections.Counter(rec.get("g", 0) for rec in capped if start <= rec["t"] < stop),
                    "main": main, "in_patch": in_patch, "living": living,
                    "presteps": prestep_rows(samples, pickups, start, stop),
                    "subspot": [(when, value) for when, value in subspots if start <= when < stop],
                    "subleg": None if searched is None else any(value in ("submerge", "prestep") for value in sub_legs),
                    "subhold": sub_legs.count("subhold"),
                    "drag_left": cutoff if cutoff < stop else None,
                    "submerge": submerge_gaps(latches, samples, boss, stop) if stop in submerges else None})
    return out


def show_threat(trace: Trace) -> None:
    print("THREAT")
    rows = threat_rows(trace)
    if not rows:
        print("  no surfaced window: Anub'arak never sampled or no anub.phase rows")
        return

    for row in rows:
        total = sum(row["held"].values())
        share = "never sampled"
        if total:
            share = "  ".join(f"{role} {span * 100.0 / total:.0f}%" for role, span in row["held"].most_common())
        to_tank = "never" if row["to_tank"] is None else f"{row['to_tank'] / 1000:.1f} s"
        anchor_gap = "-" if row["off_anchor"] is None else f"{row['off_anchor']:.1f} yd"
        print(f"  {row['label']:5} {row['kind']:6} {clock(row['start']):>9}"
              f" {(row['stop'] - row['start']) / 1000:6.1f} s  to a tank {to_tank:>7}"
              f"  off anchor {anchor_gap:>8}  {share}")
        print(f"          {row['legs']} leg(s), {row['yards']:.0f} yd, {row['reversals']} reversal(s)")
        if row["held_off"] is not None:
            ms, began = row["held_off"]
            print("          held off the anchor with no drag leg: "
                  + (f"longest {ms / 1000:.1f} s from {clock(began)}" if ms else "never"))
        for rec in row["taunts"]:
            print(f"          taunt {clock(rec['t'])} {trace.name(rec.get('s'))[:14]:14}"
                  f" {'human' if rec.get('s') in trace.humans else 'bot':5} {TAUNT_SPELLS[rec['sp']]}")
        if row["taunts"]:
            humans = sum(1 for rec in row["taunts"] if rec.get("s") in trace.humans)
            print(f"          {len(row['taunts'])} taunt(s): bot {len(row['taunts']) - humans}, human {humans}")
        by_bot: dict[int, list[str]] = collections.defaultdict(list)
        for (guid, value), n in row["pickup"].most_common():
            by_bot[guid].append(f"{value} {n}")
        if by_bot:
            print("          anub.pickup: " + "; ".join(
                f"{trace.name(guid)} " + ", ".join(values) for guid, values in by_bot.items()))
        if row["presteps"]:
            print("          drag pre-steps: " + "; ".join(prestep_text(trace, step) for step in row["presteps"]))
        if row["capped"]:
            print(f"          {THREAT_CAP} vetoes: " + ", ".join(
                f"{trace.name(guid)} {n}" for guid, n in row["capped"].most_common()))
        if row["main"] is not None:
            share = f"{row['in_patch'] * 100 / row['living']:.0f}%" if row["living"] else "-"
            print(f"          main tank {trace.name(row['main'])} in a patch {share} of {row['living']} samples")
        if row["submerge"] is not None:
            spot = row["submerge"]["spot"]
            gaps = ", ".join(f"patch{side} {yards:.1f} yd" + (" under" if yards < SUBMERGE_PATCH_CLEARANCE else "")
                             for side, yards in sorted(row["submerge"]["patches"].items()))
            left = ("" if row["drag_left"] is None
                    else f"; the drag left at {clock(row['drag_left'])}, {(row['stop'] - row['drag_left']) / 1000:.1f} s"
                         " before")
            print(f"          submerged {clock(row['stop'])} at {spot[0]:.1f}, {spot[1]:.1f}:"
                  f" {gaps or 'no tank patch latched'}{left}")
        if row["subspot"]:
            print(f"          anub.subspot: {subspot_text(row)}")

    emerges = [row["to_tank"] for row in rows if row["kind"] == "emerge"]
    if emerges:
        known = [ms for ms in emerges if ms is not None]
        print(f"\n  {len(emerges)} emerge(s), {len(emerges) - len(known)} never reached a tank"
              + (f", the rest in {min(known) / 1000:.1f}-{max(known) / 1000:.1f} s" if known else ""))
    living = sum(row["living"] for row in rows)
    if living:
        print(f"  main tank, the tank he targeted most in each window, in a patch (within"
              f" {PERMAFROST_SLOW_REACH:.1f} yd of a Permafrost\n  centre): {sum(row['in_patch'] for row in rows)}"
              f" of {living} surfaced samples; a submerge under {SUBMERGE_PATCH_CLEARANCE} yd from a tank"
              " patch reads under")
    print("  off anchor is his median distance from the midpoint of both latched tank patches, over the"
          " samples\n  that have both, before the window's first submerge leg; - when none does")
    if any(row["held_off"] is not None for row in rows):
        flagged = [row for row in rows if row["held_off"] and row["held_off"][0] >= HELD_OFF_FLAG_MS]
        print(f"  held off is the longest stretch he stood {OFF_ANCHOR_YD:.0f}+ yd off the midpoint of anub.spot0 and"
              " anub.spot1 while\n  anub.pickup named no drag leg, neither drag nor submerge, before the window's"
              f" first submerge leg;\n  {HELD_OFF_FLAG_MS // 1000} s or more: "
              + (", ".join(f"{row['label']} {row['held_off'][0] / 1000:.1f} s" for row in flagged) or "none"))
    else:
        print("  no anub.spot0 and anub.spot1 rows, so no stretch held off the anchor")
    print(f"  a leg is a run of samples flagged moving, {LEG_MIN_YD:.0f} yd or more start to stop; a reversal is"
          f" a leg\n  heading more than {REVERSAL_DEGREES:.0f} degrees off the one before;"
          f" {THREAT_CAP} vetoes are rows, one per verdict change")
    print("  a drag pre-step is an anub.pickup prestep, the tank walking round him before its leg, with its yards to"
          " its\n  next pickup row" + ("" if any(row["presteps"] for row in rows) else "; no prestep rows"))
    print("  anub.subspot is each submerge-spot search, the spot and its clearance r in yards"
          + ("" if notes(trace, "anub.subspot") else "; no anub.subspot rows"))
    print("  no submerge leg is no anub.pickup submerge or prestep after the window's first search")


def prestep_text(trace: Trace, step: dict) -> str:
    walked = "-" if step["walked"] is None else f"{step['walked']:.1f} yd"
    then = ("nothing after it" if step["next"] is None
            else f"then {step['next']} +{step['after'] / 1000:.1f} s")
    return f"{trace.name(step['guid'])} {clock(step['t'])} walked {walked}, {then}"


def subspot_text(row: dict) -> str:
    """A window's anub.subspot rows, `no submerge leg` when the tank walked none after the first, with its
    `subhold` rows, and how far he went down from the last spot it found."""
    text = ", ".join(f"{value} {clock(when)}" for when, value in row["subspot"])
    if row.get("subleg") is False:
        text += "; no submerge leg" + (f", {row['subhold']} subhold row(s)" if row.get("subhold") else "")
    if row["submerge"] is None:
        return text
    spot, _ = parse_subspot(row["subspot"][-1][1])
    if spot is None:
        return text + "; the last search found none"
    return text + f"; he went down {dist2(row['submerge']['spot'], spot):.1f} yd off its last spot"


# ---------------------------------------------------------------------------------------------- swarm

def swarm_span(trace: Trace, spans=None) -> tuple[int, int, str] | None:
    """Phase 3 from `anub.phase`, else from the first Leeching Swarm aura or tick on the raid."""
    spans = phase_spans(trace) if spans is None else spans
    for start, stop, phase, _ in spans:
        if phase == 3:
            return start, stop, "anub.phase"
    starts = [rec["t"] for rec in trace.of("aura") if rec.get("sp") in LEECHING_SWARM and not rec.get("r")]
    starts += [rec["t"] for rec in trace.of("dmg") if rec.get("sp") == SPELL_LEECHING_SWARM_DMG]
    if starts:
        return min(starts), max(min(starts), pull_end(trace)), "Leeching Swarm"
    return None


def swarm_damage(trace: Trace) -> dict[int, tuple[int, int]]:
    """Per victim, `(ticks, damage)` of 66240."""
    out: dict[int, list[int]] = collections.defaultdict(lambda: [0, 0])
    for rec in trace.of("dmg"):
        if rec.get("sp") == SPELL_LEECHING_SWARM_DMG:
            out[rec.get("d")][0] += 1
            out[rec.get("d")][1] += rec.get("a", 0)
    return {guid: (ticks, damage) for guid, (ticks, damage) in out.items()}


def lust_casts(trace: Trace, start: int) -> list[dict]:
    return [{"t": rec["t"], "rel": rec["t"] - start, "caster": rec.get("s"), "spell": rec.get("sp")}
            for rec in trace.of("cast") if rec.get("sp") in LUST_SPELLS and not rec.get("tr")]


def cold_windows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Every Penetrating Cold on the roster, with the heals it drew and how low the carrier got."""
    samples = samples or Samples(trace)
    roster = roster_guids(trace)
    heals = trace.of("heal")
    deaths = combat_deaths(trace)
    spans = phase_spans(trace)
    out = []
    for guid, windows in aura_spans(trace, PENETRATING_COLD).items():
        if guid not in roster:
            continue
        for start, stop, _ in windows:
            mine = [rec for rec in heals if rec.get("d") == guid and start <= rec["t"] <= stop]
            hp = [row[5] for row in samples.between(guid, start, stop)]
            out.append({
                "guid": guid,
                "start": start,
                "stop": stop,
                "phase": phase_at(spans, start),
                "heals": len(mine),
                "amount": sum(rec.get("a", 0) for rec in mine),
                "overheal": sum(rec.get("oh", 0) for rec in mine),
                "healers": {rec.get("s") for rec in mine},
                "low": min(hp) if hp else None,
                "died": any(rec.get("g") == guid and start <= rec["t"] <= stop for rec in deaths),
            })
    return sorted(out, key=lambda row: row["start"])


def cover_spans(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Per unit, the merged spans it wore a HoT or a shield from anyone: a HOT_NAMES aura, or one the trace shows
    absorbing. Each caster's copy runs on its own, and one still up when the file ends runs to the end."""
    shields = {rec.get("sp") for rec in trace.of("abs")}
    end = pull_end(trace)
    opened: dict[tuple, int] = {}
    spans: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        spell = rec.get("sp")
        if spell not in shields and trace.spells.get(spell) not in HOT_NAMES:
            continue
        key = (rec.get("d"), spell, rec.get("s"))
        if rec.get("r"):
            if key in opened:
                spans[key[0]].append((opened.pop(key), rec["t"]))
        elif key not in opened:
            opened[key] = rec["t"]
    for key, start in opened.items():
        spans[key[0]].append((start, max(start, end)))
    return {guid: merged(rows) for guid, rows in spans.items()}


def cold_bare(trace: Trace, start: int, stop: int) -> list[dict]:
    """Each roster Penetrating Cold window cut to `[start, stop)`: the milliseconds carried, those with no HoT or
    shield on the carrier, and whether it died carrying it."""
    roster = roster_guids(trace)
    covers = cover_spans(trace)
    deaths = combat_deaths(trace)
    out = []
    for guid, windows in aura_spans(trace, PENETRATING_COLD).items():
        if guid not in roster:
            continue
        for low, high, _ in windows:
            low, high = max(low, start), min(high, stop)
            if low < high:
                out.append({"guid": guid, "start": low, "carried": high - low,
                            "bare": high - low - covered(covers.get(guid, []), low, high),
                            "died": any(rec.get("g") == guid and low <= rec["t"] <= high for rec in deaths)})
    return sorted(out, key=lambda row: row["start"])


def carrying(spans: dict[int, list[tuple[int, int, bool]]], guid, when: int) -> bool:
    return any(start <= when <= stop for start, stop, _ in spans.get(guid, ()))


def swarm_bins(trace: Trace, start: int, stop: int, samples: Samples | None = None) -> list[dict]:
    """Phase 3 in SWARM_BIN_MS steps: his health at the step's last sample, the median health of the
    living tanks and non-tanks, the swarm on each, and the healers' effective healing onto tanks,
    Penetrating Cold carriers and everyone else."""
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    roster = roster_guids(trace)
    cold = aura_spans(trace, PENETRATING_COLD)
    out = []
    for low in range(start, stop, SWARM_BIN_MS):
        high = min(low + SWARM_BIN_MS, stop)
        row = {"t": low, "boss": None, "tank": None, "other": None, "swarm_tank": 0, "swarm_other": 0,
               "heal_tank": 0, "heal_cold": 0, "heal_other": 0}
        hp = samples.between(boss, low, high - 1) if boss else []
        if hp:
            row["boss"] = hp[-1][5]
        health: dict[str, list[float]] = {"tank": [], "other": []}
        for guid in roster:
            group = "tank" if trace.role(guid) == "tank" else "other"
            health[group] += [sample[5] for sample in samples.between(guid, low, high - 1) if sample[5] > 0]
        for group, values in health.items():
            row[group] = statistics.median(values) if values else None
        out.append(row)

    def step(when: int) -> dict | None:
        return out[(when - start) // SWARM_BIN_MS] if start <= when < stop else None

    for rec in trace.of("dmg"):
        row = step(rec["t"])
        if row is not None and rec.get("sp") == SPELL_LEECHING_SWARM_DMG and rec.get("d") in roster:
            row["swarm_tank" if trace.role(rec["d"]) == "tank" else "swarm_other"] += rec.get("a", 0)
    for rec in trace.of("heal"):
        row, target = step(rec["t"]), rec.get("d")
        if row is None or trace.role(rec.get("s")) != "heal" or target not in roster:
            continue
        kind = ("heal_tank" if trace.role(target) == "tank"
                else "heal_cold" if carrying(cold, target, rec["t"]) else "heal_other")
        row[kind] += rec.get("a", 0) - rec.get("oh", 0)
    return out


def swarm_heal_breaches(trace: Trace, start: int, stop: int,
                        samples: Samples | None = None) -> dict[tuple[int, int], list[int]]:
    """Per bot and heal spell, `[heals, effective]` landing in phase 3 on another non-tank with no
    Penetrating Cold at SWARM_FLOOR_PCT or more by the snapshot before. Only spells the bot cast itself, so
    procs and passive heals (Vampiric Embrace, Judgement of Light, Earthliving) stay out."""
    samples = samples or Samples(trace)
    roster = roster_guids(trace)
    cold = aura_spans(trace, PENETRATING_COLD)
    cast = {(rec.get("s"), rec.get("sp")) for rec in trace.of("cast") if not rec.get("tr")}
    out: dict[tuple[int, int], list[int]] = collections.defaultdict(lambda: [0, 0])
    for rec in trace.of("heal"):
        source, target, when = rec.get("s"), rec.get("d"), rec["t"]
        if (not start <= when < stop or source not in roster or source in trace.humans or target == source
                or target not in roster or trace.role(target) == "tank" or carrying(cold, target, when)
                or (source, rec.get("sp")) not in cast):
            continue
        before = samples.row(target, when)
        if before is None or before[5] < SWARM_FLOOR_PCT:
            continue
        out[(source, rec.get("sp"))][0] += 1
        out[(source, rec.get("sp"))][1] += rec.get("a", 0) - rec.get("oh", 0)
    return dict(out)


def swarm_burrowers(trace: Trace, start: int, samples: Samples) -> tuple[set[int], set[int], int | None]:
    """The burrowers alive as phase 3 began, those first seen after, and when the last of the first went."""
    old, new, last = set(), set(), None
    for guid in guids_of_entry(trace, NPC_BURROWER):
        stamps, rows = samples.track(guid)
        alive = [when for when, row in zip(stamps, rows) if row[5] > 0]
        if not alive:
            continue
        if alive[0] > start:
            new.add(guid)
        elif alive[-1] >= start:
            old.add(guid)
            last = alive[-1] if last is None else max(last, alive[-1])
    return old, new, last


def swarm_dps_targets(trace: Trace, start: int, stop: int, samples: Samples | None = None) -> dict[str, float]:
    """Share of the living DPS bots' phase 3 samples by target: him, a burrower alive as it began, a newer
    one, anything else."""
    samples = samples or Samples(trace)
    old, new, _ = swarm_burrowers(trace, start, samples)
    boss = boss_guid(trace)
    counts: collections.Counter = collections.Counter()
    for guid in roster_guids(trace):
        if trace.role(guid) not in ("melee", "ranged") or guid in trace.humans:
            continue
        for row in samples.between(guid, start, stop - 1):
            if row[5] <= 0:
                continue
            target = row[7] if len(row) > 7 else 0
            counts["boss" if target == boss else "old" if target in old else "new" if target in new
                   else "other"] += 1
    total = sum(counts.values())
    return {kind: counts[kind] / total for kind in ("boss", "old", "new", "other")} if total else {}


def burst_before_swarm(trace: Trace, start: int, samples: Samples | None = None) -> tuple[int | None, list[dict]]:
    """His first sample at or under BURST_HOLD_PCT before phase 3, and each bot burst cast from it to phase 3
    with the caster's anub.burst then."""
    samples = samples or Samples(trace)
    stamps, rows = samples.track(boss_guid(trace))
    crossed = next((when for when, row in zip(stamps, rows) if when < start and 0 < row[5] <= BURST_HOLD_PCT),
                   None)
    if crossed is None:
        return None, []
    readings: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, "anub.burst"):
        readings[rec.get("g")].append((rec["t"], rec.get("txt", "")))
    roster = roster_guids(trace)
    out = []
    for rec in trace.of("cast"):
        source, when = rec.get("s"), rec["t"]
        if (rec.get("tr") or rec.get("sp") not in BURST_SPELLS or not crossed <= when < start
                or source not in roster or source in trace.humans):
            continue
        held = [txt for at, txt in readings.get(source, ()) if at <= when]
        out.append({"t": when, "guid": source, "spell": rec["sp"], "reading": held[-1] if held else None})
    return crossed, out


def show_swarm_race(trace: Trace, start: int, stop: int, samples: Samples) -> None:
    """Phase 3's race against the swarm: the 5 s steps, heals that only fed it, the DPS on burrowers, when
    the burst opened, and the burst spent between 40% and phase 3."""
    def pct(value):
        return "    -" if value is None else f"{value:5.1f}"

    print(f"  per {SWARM_BIN_MS // 1000} s: his health, median tank and non-tank health, the swarm on tanks and"
          " others (k), healer heals onto tanks, Penetrating Cold carriers and others (k)")
    for row in swarm_bins(trace, start, stop, samples):
        print(f"    {clock(row['t']):>9} boss {pct(row['boss'])}  tank {pct(row['tank'])}  other {pct(row['other'])}"
              f"  swarm {row['swarm_tank'] / 1000:6.1f} {row['swarm_other'] / 1000:6.1f}"
              f"  heals {row['heal_tank'] / 1000:6.1f} {row['heal_cold'] / 1000:6.1f} {row['heal_other'] / 1000:6.1f}")

    breaches = swarm_heal_breaches(trace, start, stop, samples)
    print(f"  bot heals cast onto a non-tank with no Penetrating Cold at {SWARM_FLOOR_PCT:.0f}%+: "
          + (", ".join(f"{trace.name(guid)} {trace.spell(spell)} {count} ({amount / 1000:.0f}k)"
                       for (guid, spell), (count, amount) in sorted(breaches.items(), key=lambda item: -item[1][1]))
             or "none"))

    shares = swarm_dps_targets(trace, start, stop, samples)
    if shares:
        print(f"  DPS bot samples by target: him {shares['boss']:.0%}, a burrower up as it began"
              f" {shares['old']:.0%}, a newer burrower {shares['new']:.0%}, else {shares['other']:.0%}")

    old, _, last = swarm_burrowers(trace, start, samples)
    lust = next((row for row in lust_casts(trace, start) if row["rel"] >= 0), None)
    opened = next((rec["t"] for rec in notes(trace, "anub.burst")
                   if rec["t"] >= start and rec.get("txt") == "open"), None)
    parts = [f"{len(old)} burrower(s) up as it began"
             + ("" if last is None else f", the last gone {clock(last)} ({(last - start) / 1000:+.1f} s)")]
    if lust:
        parts.append(f"lust {lust['rel'] / 1000:+.1f} s")
    if opened is not None:
        parts.append(f"anub.burst open {(opened - start) / 1000:+.1f} s")
    print("  " + "; ".join(parts))

    crossed, casts = burst_before_swarm(trace, start, samples)
    if crossed is None:
        print(f"  he never sat at {BURST_HOLD_PCT:.0f}% or under before phase 3")
    else:
        spent = [row for row in casts if row["reading"] != "behind"]
        print(f"  burst cast from his {BURST_HOLD_PCT:.0f}% at {clock(crossed)} to phase 3: {len(casts)}"
              + (f", {len(spent)} not released as behind: " + ", ".join(
                  f"{trace.name(row['guid'])} {BURST_SPELLS[row['spell']]} {clock(row['t'])}" for row in spent)
                 if spent else ""))


def rows_by_target(trace: Trace, event: str, spells=None) -> dict[int, list[dict]]:
    """An event's rows per `d`, in time order, only `spells` when given."""
    out: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of(event):
        if spells is None or rec.get("sp") in spells:
            out[rec.get("d")].append(rec)
    return out


def pct_at(samples: Samples, tables: dict, guid: int, pool: int | None, when: int) -> float | None:
    """A player's health % at `when`, from whichever was written last: its snapshot, what a hit on it left
    (off its pool) or what a heal left. The swarm drains it every second, so a snapshot alone runs a tick
    behind."""
    stamps, rows = samples.track(guid)
    index = bisect.bisect_right(stamps, when)
    latest = (stamps[index - 1], rows[index - 1][5]) if index else None
    for rec in reversed(tables["damage"].get(guid, ())):
        if rec["t"] <= when:
            if pool and rec.get("hp") is not None and (latest is None or rec["t"] >= latest[0]):
                latest = (rec["t"], max(0.0, rec["hp"] - rec.get("a", 0) * 100.0 / pool))
            break
    for rec in reversed(tables["heals"].get(guid, ())):
        if rec["t"] <= when:
            if rec.get("hp") is not None and (latest is None or rec["t"] >= latest[0]):
                latest = (rec["t"], rec["hp"])
            break
    return latest[1] if latest else None


def cold_carrier(trace: Trace, samples: Samples, guid: int, landing: int, tables: dict) -> dict:
    """One carrier of a wave, as cold_waves describes it, before `risk`."""
    pool = trace.maxhp.get(guid)
    pct = pct_at(samples, tables, guid, pool, landing)
    tick = next((rec for rec in tables["ticks"].get(guid, ())
                 if landing < rec["t"] <= landing + COLD_TICK_MS + COLD_TICK_SLACK_MS), None)
    due = tick["t"] if tick else landing + COLD_TICK_MS
    died = any(landing <= when <= due + NOTE_TICK_MS for when in tables["deaths"].get(guid, ()))
    margin = amount = None
    if tick:
        amount = tick.get("a", 0) + tick.get("ab", 0) + tick.get("rs", 0)
        if tick.get("ok", 0) > 0:
            margin = -tick["ok"]
        elif pool and tick.get("hp") is not None:
            # the pool is the one at first sight, so a carrier under a health buff can read short of a tick it lived
            margin = max(0, round(tick["hp"] * pool / 100.0) - tick.get("a", 0))
        fate = "died" if died or tick.get("ok", 0) > 0 else "survived"
    else:
        fate = "before" if died else "none"
    heals: collections.Counter = collections.Counter()
    absorbs: collections.Counter = collections.Counter()
    helped = False
    for rec in tables["heals"].get(guid, ()):
        if landing <= rec["t"] < due:
            heals[member_of(trace, rec.get("s"))] += rec.get("a", 0) - rec.get("oh", 0)
            helped = helped or rec.get("s") in tables["healers"]
    for rec in tables["absorbs"].get(guid, ()):
        if landing <= rec["t"] <= due:
            absorbs[member_of(trace, rec.get("s"))] += rec.get("a", 0)
            helped = helped or member_of(trace, rec.get("s")) in tables["healers"]
    coldheal = []
    for healer, start, stop in tables["assigned"].get(guid, ()):
        if start <= due and stop >= landing - NOTE_TICK_MS and healer not in coldheal:
            coldheal.append(healer)
    return {"guid": guid, "landing": landing, "pct": pct,
            "health": None if pct is None or not pool else pct * pool / 100.0,
            "tick": due if tick else None, "amount": amount, "margin": margin, "fate": fate,
            "heals": heals, "absorbs": absorbs, "helped": helped, "coldheal": coldheal}


def wave_herds(trace: Trace, carriers: list[dict], heals: dict[int, list[dict]], healers: set[int]) -> list[dict]:
    """Per carrier, the first HERD_MS stretch before its tick with two healers or more landing heals on it, kept
    while another carrier at risk had nothing from a healer before its own tick."""
    starved = [row for row in carriers if row["risk"] and not row["helped"]]
    out = []
    for row in carriers:
        due = row["tick"] or row["landing"] + COLD_TICK_MS
        landed = sorted((rec["t"], rec.get("s")) for rec in heals.get(row["guid"], ())
                        if rec.get("s") in healers and row["landing"] <= rec["t"] < due)
        others = [other["guid"] for other in starved if other is not row]
        for index, (first, _) in enumerate(landed):
            inside = [(when, source) for when, source in landed[index:] if when - first <= HERD_MS]
            if len({source for _, source in inside}) >= 2:
                if others:
                    out.append({"carrier": row["guid"], "start": first, "stop": inside[-1][0],
                                "healers": sorted({source for _, source in inside}, key=trace.name),
                                "starved": others})
                break
    return out


def cold_waves(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Phase 3's Penetrating Cold waves, each the carriers landing within COLD_WAVE_MS of its first. Per carrier:
    its health at the landing (`pct`, and `health` in points off its pool), its first tick and the `margin` it
    left (health before it less the damage taken, the overkill as a negative), the effective heals before that
    tick and the absorbs up to it per source with owners folded in, `helped` when one came from a healer, its
    anub.coldheal healers and `fate`: survived, died at the tick, before it, or none with no tick row and alive.
    `risk` is its health at the landing, less the swarm's share each second to the tick, under that tick before
    resists or the wave's largest without one; None with either unknown. Per wave: the living toc.aura frost
    holders at the landing (None with no toc.aura rows), `resists[tick] = [resisted, total]` over every tick of
    the wave, `mastery[tick]` the Aura Mastery auras up at it, for those ticks and the first two, its anub.mastery
    rows to the second tick, and its herds."""
    span = swarm_span(trace)
    if span is None:
        return []
    samples = samples or Samples(trace)
    start, stop, _ = span
    roster = roster_guids(trace)
    landings = sorted((rec["t"], rec.get("d")) for rec in trace.of("aura")
                      if rec.get("sp") in PENETRATING_COLD and not rec.get("r") and rec.get("d") in roster
                      and start <= rec["t"] < stop)
    groups: list[list[tuple[int, int]]] = []
    for when, guid in landings:
        if groups and when - groups[-1][0][0] <= COLD_WAVE_MS:
            if all(guid != seen for _, seen in groups[-1]):
                groups[-1].append((when, guid))
        else:
            groups.append([(when, guid)])
    if not groups:
        return []

    end = pull_end(trace)
    deaths: dict[int, list[int]] = collections.defaultdict(list)
    for rec in combat_deaths(trace):
        deaths[rec.get("g")].append(rec["t"])
    assigned: dict[int, list[tuple[int, int, int]]] = collections.defaultdict(list)
    for healer, spans in holder_spans(trace, "anub.coldheal", end).items():
        for value, low, high in spans:
            if as_guid(value):
                assigned[as_guid(value)].append((healer, low, high))
    damage = rows_by_target(trace, "dmg")
    tables = {"damage": damage, "heals": rows_by_target(trace, "heal"),
              "ticks": {guid: [rec for rec in rows if rec.get("sp") in PENETRATING_COLD]
                        for guid, rows in damage.items()},
              "absorbs": rows_by_target(trace, "abs"), "deaths": deaths, "assigned": assigned,
              "healers": {guid for guid in roster if trace.role(guid) == "heal"}}
    diff = trace.header.get("diff")
    drain = SWARM_DRAIN[diff] if diff in range(len(SWARM_DRAIN)) else SWARM_DRAIN[-1]
    mastery = aura_spans(trace, (SPELL_AURA_MASTERY,))
    frost = holder_spans(trace, "toc.aura", end)
    mastery_rows = notes(trace, "anub.mastery")

    out = []
    for index, group in enumerate(groups):
        first = group[0][0]
        limit = groups[index + 1][0][0] if index + 1 < len(groups) else end + 1
        carriers = [cold_carrier(trace, samples, guid, landing, tables) for landing, guid in group]
        largest = max((row["amount"] for row in carriers if row["amount"]), default=None)
        for row in carriers:
            amount = row["amount"] or largest
            row["risk"] = (None if row["health"] is None or not amount
                           else row["health"] * (1.0 - drain) ** (COLD_TICK_MS // 1000) < amount)
        resists: dict[int, list[int]] = {}
        stamps: dict[int, int] = {}
        for row in carriers:
            for rec in tables["ticks"].get(row["guid"], ()):
                tick = round((rec["t"] - row["landing"]) / COLD_TICK_MS)
                if row["landing"] < rec["t"] < limit and tick >= 1:
                    entry = resists.setdefault(tick, [0, 0])
                    entry[0] += rec.get("rs", 0)
                    entry[1] += rec.get("a", 0) + rec.get("ab", 0) + rec.get("rs", 0)
                    stamps[tick] = min(stamps.get(tick, rec["t"]), rec["t"])
        up = {}
        for tick in sorted({1, 2} | set(resists)):
            when = stamps.get(tick, first + tick * COLD_TICK_MS)
            up[tick] = sorted((guid for guid, spans in mastery.items()
                               if any(low <= when <= high for low, high, _ in spans)), key=trace.name)
        holders = None
        if frost:
            holders = sorted((guid for guid, spans in frost.items()
                              if any(value == "frost" and low <= first < high for value, low, high in spans)
                              and (health(samples, guid, first) or 0.0) > 0.0), key=trace.name)
        out.append({
            "t": first, "rel": first - start, "carriers": carriers, "frost": holders, "mastery": up,
            "resists": dict(sorted(resists.items())),
            "rows": [(rec["t"], rec.get("g"), str(rec.get("txt", ""))) for rec in mastery_rows
                     if first - NOTE_TICK_MS <= rec["t"] <= first + 2 * COLD_TICK_MS + COLD_TICK_SLACK_MS],
            "herds": wave_herds(trace, carriers, tables["heals"], tables["healers"]),
        })
    return out


def kilo(amount: float) -> str:
    return f"{amount / 1000:.1f}k"


def cold_sources(trace: Trace, row: dict) -> str:
    parts = []
    for guid in sorted(set(row["heals"]) | set(row["absorbs"]),
                       key=lambda guid: -(row["heals"].get(guid, 0) + row["absorbs"].get(guid, 0))):
        if row["heals"].get(guid, 0) <= 0 and not row["absorbs"].get(guid):
            continue
        text = trace.name(guid)
        if row["heals"].get(guid):
            text += f" {kilo(row['heals'][guid])}"
        if row["absorbs"].get(guid):
            text += f" abs {kilo(row['absorbs'][guid])}"
        parts.append(text)
    return ", ".join(parts) or "none"


COLD_FATES = {"survived": "survived", "died": "died", "before": "died before it", "none": "no tick"}


def show_cold_waves(trace: Trace, samples: Samples) -> None:
    waves = cold_waves(trace, samples)
    if not waves:
        print("  no Penetrating Cold wave landed in phase 3")
        return
    survived = [sum(1 for row in wave["carriers"] if row["fate"] == "survived") for wave in waves]
    carried = [len(wave["carriers"]) for wave in waves]
    print("  first ticks survived per phase 3 Penetrating Cold wave: "
          + ", ".join(f"{alive} of {total}" for alive, total in zip(survived, carried))
          + f" ({sum(survived)} of {sum(carried)})")
    coldheal = bool(notes(trace, "anub.coldheal"))
    mastery_rows = bool(notes(trace, "anub.mastery"))
    print(f"  per wave and carrier: health at the landing, at risk when that less {COLD_TICK_MS // 1000} s of the"
          " swarm is under the tick,\n  the first tick's margin (health before it less the damage taken) and fate,"
          + (" anub.coldheal healers," if coldheal else "") + " the heals and absorbs on it\n  before the tick by"
          " source; per wave the living toc.aura frost holders, Aura Mastery at the first two\n  ticks, anub.mastery"
          " rows, the resisted share per tick (* under Aura Mastery) and herds"
          + ("" if coldheal else "; anub.coldheal: no rows"))
    for wave, alive in zip(waves, survived):
        frost = ("toc.aura: no rows" if wave["frost"] is None
                 else "frost " + (", ".join(trace.name(guid) for guid in wave["frost"]) or "none"))
        mastery = ", ".join(f"tick {tick} " + (", ".join(trace.name(guid) for guid in wave["mastery"][tick]) or "no")
                            for tick in (1, 2))
        if not mastery_rows:
            rows = "no rows"
        else:
            rows = ", ".join(f"{trace.name(guid)} {value} {(when - wave['t']) / 1000:+.1f} s"
                             for when, guid, value in wave["rows"]) or "none"
        print(f"    {clock(wave['t'])} ({wave['rel'] / 1000:+.1f} s): {alive} of {len(wave['carriers'])} through the"
              f" first tick; {frost}; Aura Mastery {mastery}")
        print(f"      anub.mastery {rows}; resisted per tick " + (" ".join(
            f"{resisted * 100 / total:.0f}%" + ("*" if wave["mastery"][tick] else "")
            for tick, (resisted, total) in wave["resists"].items() if total) or "none"))
        for row in wave["carriers"]:
            health_text = "-" if row["health"] is None else kilo(row["health"])
            pct = "-" if row["pct"] is None else f"{row['pct']:.1f}%"
            risk = {True: "risk", False: "-", None: "?"}[row["risk"]]
            margin = "-" if row["margin"] is None else f"{row['margin'] / 1000:+.1f}k"
            healers = ""
            if coldheal:
                healers = "coldheal " + (", ".join("self" if guid == row["guid"] else trace.name(guid)
                                                   for guid in row["coldheal"]) or "none") + "; "
            print(f"      {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {health_text:>6} {pct:>6}"
                  f" {risk:4} {margin:>6} {COLD_FATES[row['fate']]:14} {healers}{cold_sources(trace, row)}")
        for herd in wave["herds"]:
            print(f"      herd: {', '.join(trace.name(guid) for guid in herd['healers'])} on"
                  f" {trace.name(herd['carrier'])} {clock(herd['start'])}-{clock(herd['stop'])}; at risk with"
                  f" nothing from a healer: {', '.join(trace.name(guid) for guid in herd['starved'])}")


def show_heal_duties(trace: Trace) -> None:
    end = pull_end(trace)
    duties = holder_spans(trace, "anub.healduty", end)
    if not duties:
        print("  anub.healduty: no rows")
    else:
        print("  anub.healduty, each healer bot's duty from phase 3: " + "; ".join(
            f"{trace.name(guid)} " + " > ".join(f"{HEAL_DUTIES.get(value, value)} {clock(start)}"
                                                for value, start, _ in spans)
            for guid, spans in sorted(duties.items(), key=lambda item: trace.name(item[0]))))
    assigned = holder_spans(trace, "anub.coldheal", end)
    if not assigned:
        print("  anub.coldheal: no rows")
        return
    print("  anub.coldheal, the carrier at risk each healer bot kept up, self for itself:")
    for guid, spans in sorted(assigned.items(), key=lambda item: trace.name(item[0])):
        parts = [f"{'self' if as_guid(value) == guid else trace.name(as_guid(value))} {clock(start)}-{clock(stop)}"
                 for value, start, stop in spans if as_guid(value)]
        print(f"    {trace.name(guid)[:14]:14} {', '.join(parts) or 'none'}")


SWARM_EXTERNALS = {48788: "Lay on Hands", 33206: "Pain Suppression", 47788: "Guardian Spirit",
                   6940: "Hand of Sacrifice"}
# a tank under this in phase 3 is owed one of them (anubarak tank rescue)
TANK_RESCUE_PCT = 35.0
# a caster dead this soon after its external likely died of it, as Hand of Sacrifice's casters do under the swarm
EXTERNAL_CASTER_DEATH_MS = 10000


def swarm_externals(trace: Trace, start: int, stop: int, samples: Samples) -> tuple[list[dict], list[dict]]:
    """Phase 3's externals, each with its target's health at the cast and its caster's death within
    EXTERNAL_CASTER_DEATH_MS (`dead`, ms after the cast, None for none), and each tank's dip under
    TANK_RESCUE_PCT with the first external onto it before it rose back or died."""
    deaths: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("death"):
        deaths[rec.get("g")].append(rec["t"])
    casts = []
    for rec in trace.of("cast"):
        if rec.get("tr") or rec.get("sp") not in SWARM_EXTERNALS or not start <= rec["t"] <= stop:
            continue
        row = samples.row(rec.get("tgt"), rec["t"])
        dead = next((when - rec["t"] for when in deaths.get(rec.get("s"), [])
                     if 0 <= when - rec["t"] <= EXTERNAL_CASTER_DEATH_MS), None)
        casts.append({"t": rec["t"], "caster": rec.get("s"), "spell": rec["sp"], "target": rec.get("tgt"),
                      "hp": row[5] if row else None, "dead": dead})
    dips = []
    for guid in sorted(roster_guids(trace)):
        if trace.role(guid) != "tank":
            continue
        stamps, rows = samples.track(guid)
        dip = None
        for index in range(bisect.bisect_left(stamps, start), bisect.bisect_right(stamps, stop)):
            hp = rows[index][5]
            if dip is None and 0 < hp < TANK_RESCUE_PCT:
                dip = {"guid": guid, "start": stamps[index], "end": stop, "died": False}
            elif dip is not None and (hp >= TANK_RESCUE_PCT or hp <= 0):
                dip.update({"end": stamps[index], "died": hp <= 0})
                dips.append(dip)
                dip = None
        if dip is not None:
            dips.append(dip)
    for dip in dips:
        dip["external"] = next((cast for cast in casts if cast["target"] == dip["guid"]
                                and dip["start"] - NEAR_MS <= cast["t"] <= dip["end"]), None)
    return casts, dips


def show_swarm_externals(trace: Trace, start: int, stop: int, samples: Samples) -> None:
    casts, dips = swarm_externals(trace, start, stop, samples)
    print("  phase 3 externals, the target's health at the cast, the caster dead within"
          f" {EXTERNAL_CASTER_DEATH_MS / 1000:.0f} s flagged: " + (", ".join(
              f"{clock(cast['t'])} {trace.name(cast['caster'])} {SWARM_EXTERNALS[cast['spell']]} >"
              f" {trace.name(cast['target'])} ({trace.role(cast['target'])})"
              + ("" if cast["hp"] is None else f" {cast['hp']:.0f}%")
              + ("" if cast["dead"] is None else f" (caster dead +{cast['dead'] / 1000:.1f} s)")
              for cast in casts) or "none"))
    covered = [dip for dip in dips if dip["external"]]
    bare = [dip for dip in dips if not dip["external"]]
    counts = collections.Counter(trace.name(dip["guid"]) for dip in bare)
    print(f"  tank dips under {TANK_RESCUE_PCT:.0f}% in phase 3: {len(dips)}; an external onto it in {len(covered)}"
          + (": " + ", ".join(
              f"{trace.name(dip['guid'])} {clock(dip['start'])} {SWARM_EXTERNALS[dip['external']['spell']]}"
              f" {(dip['external']['t'] - dip['start']) / 1000:+.1f} s" + (", died" if dip["died"] else "")
              for dip in covered) if covered else "")
          + f"; none in {len(bare)}" + (" (" + ", ".join(f"{name} {n}" for name, n in counts.most_common()) + ")"
                                       if bare else "")
          + "; died with none: " + (", ".join(f"{trace.name(dip['guid'])} {clock(dip['end'])}"
                                              for dip in bare if dip["died"]) or "none"))


def sustain_kind(trace: Trace, rec: dict) -> str:
    name = trace.spells.get(rec.get("sp"), "")
    source = trace.owners.get(rec.get("s"), rec.get("s"))
    if "Judgement of Light" in name:
        return "JoL"
    if "Vampiric Embrace" in name:
        return "VE"
    if "Healing Stream" in name:
        return "HST"
    if source == rec.get("d"):
        return "own"
    return "healers" if trace.role(source) == "heal" else "other"


def swarm_sustain(trace: Trace, start: int, stop: int, samples: Samples) -> list[dict]:
    """Per non-tank roster member in [start, stop): its effective heals received by SUSTAIN_KINDS, the swarm's
    damage on it, and the snapshot milliseconds it was alive, each step capped at MAX_STEP_MS."""
    members = [guid for guid in roster_guids(trace) if trace.role(guid) != "tank"]
    out = {guid: {"guid": guid, "heals": collections.Counter(), "swarm": 0, "alive": 0} for guid in members}
    for rec in trace.of("heal", "dmg"):
        row = out.get(rec.get("d"))
        if row is None or not start <= rec["t"] < stop:
            continue
        if rec["e"] == "heal":
            row["heals"][sustain_kind(trace, rec)] += rec.get("a", 0) - rec.get("oh", 0)
        elif rec.get("sp") == SPELL_LEECHING_SWARM_DMG or rec.get("sp") in LEECHING_SWARM:
            row["swarm"] += rec.get("a", 0)
    for guid, row in out.items():
        stamps, rows = samples.track(guid)
        for index in range(bisect.bisect_left(stamps, start), min(bisect.bisect_left(stamps, stop), len(stamps) - 1)):
            if rows[index][5] > 0:
                row["alive"] += min(stamps[index + 1] - stamps[index], MAX_STEP_MS)
    return sorted(out.values(), key=lambda row: (trace.role(row["guid"]), trace.name(row["guid"])))


def light_reads(trace: Trace, start: int, stop: int) -> dict:
    """Judgement of Light on him over [start, stop) (`uptime` ms): each cast on him holds JUDGEMENT_MS, cut at that
    paladin's next judgement. Per paladin its judgements in the span, `(kind, on him)` counted (`casts`)."""
    boss = boss_guid(trace)
    rows = sorted((rec for rec in trace.of("cast") if rec.get("sp") in JUDGEMENTS and not rec.get("tr")),
                  key=lambda rec: rec["t"])
    spans = []
    casts: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for index, rec in enumerate(rows):
        caster = rec.get("s")
        if start <= rec["t"] < stop:
            casts[caster][(JUDGEMENTS[rec["sp"]], rec.get("tgt") == boss)] += 1
        if JUDGEMENTS[rec["sp"]] != "Light" or rec.get("tgt") != boss:
            continue
        end = rec["t"] + JUDGEMENT_MS
        nxt = next((later["t"] for later in rows[index + 1:] if later.get("s") == caster), None)
        spans.append((rec["t"], end if nxt is None else min(end, nxt)))
    uptime = sum(max(0, min(high, stop) - max(low, start)) for low, high in merged(spans))
    return {"uptime": uptime, "casts": dict(casts)}


def water_totems(trace: Trace, start: int) -> dict[int, tuple[str | None, str | None]]:
    """Per shaman on the roster, its last water totem cast at or before phase 3's start and WATER_TOTEM_LATER_MS in,
    None for none."""
    shamans = [member["g"] for member in trace.header.get("roster", []) if member.get("c") == CLASS_SHAMAN]
    casts = [(rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")) for rec in trace.of("cast")
             if not rec.get("tr") and trace.spells.get(rec.get("sp"), "") in WATER_TOTEMS]

    def last(guid: int, when: int) -> str | None:
        return next((name for at, caster, name in reversed(casts) if caster == guid and at <= when), None)

    return {guid: (last(guid, start), last(guid, start + WATER_TOTEM_LATER_MS)) for guid in sorted(shamans,
                                                                                                    key=trace.name)}


def show_swarm_sustain(trace: Trace, start: int, stop: int, samples: Samples) -> None:
    print("\n  heals received per non-tank in phase 3, effective, k a second alive, and the swarm's drain:")
    print(f"    {'name':14} {'role':6} {'alive':>5} " + "".join(f"{kind:>8}" for kind in SUSTAIN_KINDS)
          + f"{'total':>8}{'swarm':>8}")
    for row in swarm_sustain(trace, start, stop, samples):
        seconds = max(row["alive"] / 1000, 1.0)
        heals = row["heals"]
        print(f"    {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['alive'] / 1000:5.0f} "
              + "".join(f"{heals[kind] / seconds / 1000:8.2f}" for kind in SUSTAIN_KINDS)
              + f"{sum(heals.values()) / seconds / 1000:8.2f}{row['swarm'] / seconds / 1000:8.2f}")
    light = light_reads(trace, start, stop)
    print(f"  Judgement of Light on him in phase 3: {light['uptime'] / 1000:.1f} of {(stop - start) / 1000:.1f} s;"
          " judgements per paladin: " + ("; ".join(
              f"{trace.name(guid)} " + ", ".join(f"{kind} {n}" + (" on him" if on else "")
                                                 for (kind, on), n in sorted(counts.items()))
              for guid, counts in sorted(light["casts"].items(), key=lambda item: trace.name(item[0]))) or "none"))
    handovers = [f"{clock(rec['t'])} {rec.get('txt')}" for rec in notes(trace, "anub.judge")]
    print("  anub.judge hand-overs: " + (", ".join(handovers) or "no rows"))
    totems = water_totems(trace, start)
    print(f"  water totem per shaman at phase 3's start and {WATER_TOTEM_LATER_MS // 1000} s in: " + (", ".join(
        f"{trace.name(guid)} {before or 'none'} > {later or 'none'}" for guid, (before, later) in totems.items())
        or "no shaman"))


def show_swarm(trace: Trace) -> None:
    print("PHASE 3")
    samples = Samples(trace)
    span = swarm_span(trace)
    if span is None:
        print("  never reached: no phase 3 in anub.phase and no Leeching Swarm on the raid")
    else:
        start, stop, source = span
        print(f"  from {clock(start)} for {(stop - start) / 1000:.1f} s ({source})")

        damage = swarm_damage(trace)
        ticks = sum(n for n, _ in damage.values())
        print(f"  Leeching Swarm {SPELL_LEECHING_SWARM_DMG}: {sum(a for _, a in damage.values()):,}"
              f" over {ticks} tick(s)")
        for guid, (n, amount) in sorted(damage.items(), key=lambda item: -item[1][1]):
            print(f"    {trace.name(guid)[:14]:14} {trace.role(guid):6} {n:4} {amount:10,}")

        lust = lust_casts(trace, start)
        for row in lust:
            print(f"  {trace.spell(row['spell'])} by {trace.name(row['caster'])} at {clock(row['t'])}"
                  f" ({row['rel'] / 1000:+.1f} s from phase 3)")
        if not lust:
            print("  no lust cast")
        show_swarm_race(trace, start, stop, samples)

    windows = cold_windows(trace, samples)
    if windows:
        print(f"\n  Penetrating Cold, whole pull: {len(windows)} carrier window(s),"
              f" {sum(row['heals'] for row in windows)} heal(s) onto them")
        print(f"    {'at':>9} {'phase':5} {'carrier':14} {'role':6} {'secs':>5} {'heals':>5} {'amount':>8}"
              f" {'over':>7} {'low':>6}")
        for row in windows:
            low = "-" if row["low"] is None else f"{row['low']:5.1f}%"
            print(f"    {clock(row['start']):>9} {row['phase']:5} {trace.name(row['guid'])[:14]:14}"
                  f" {trace.role(row['guid']):6} {(row['stop'] - row['start']) / 1000:5.1f} {row['heals']:5}"
                  f" {row['amount']:8,} {row['overheal']:7,} {low:>6}" + ("  died" if row["died"] else ""))
    else:
        print("  no Penetrating Cold on the raid")
    if span is not None:
        bare = cold_bare(trace, span[0], span[1])
        print("  Penetrating Cold carriers in phase 3, seconds with no HoT or shield on them of those carried: "
              + (", ".join(f"{trace.name(row['guid'])} {clock(row['start'])} {row['bare'] / 1000:.1f} of"
                           f" {row['carried'] / 1000:.1f} s" + (" died" if row["died"] else "") for row in bare)
                 or "none"))
        show_counts(trace, "anub.swarmheal")
        show_heal_duties(trace)
        show_cold_waves(trace, samples)
        show_swarm_externals(trace, span[0], span[1], samples)
        show_swarm_sustain(trace, span[0], span[1], samples)
    show_counts(trace, "anub.defensive")

    if span is not None:
        dead = [rec for rec in combat_deaths(trace) if rec["t"] >= span[0]]
        print(f"  {len(dead)} death(s) in phase 3" + (":" if dead else ""))
        for rec in dead:
            print(f"    {clock(rec['t'])} {trace.name(rec.get('g'))[:14]:14} {trace.role(rec.get('g')):6}"
                  f" by {trace.name(rec.get('killer'))}")


# -------------------------------------------------------------------------------------------- healers

def healer_guids(trace: Trace) -> list[int]:
    return sorted((guid for guid in roster_guids(trace) if trace.role(guid) == "heal"), key=trace.name)


def main_tank(trace: Trace, samples: Samples) -> int | None:
    return next((guid for guid, group in frost_groups(trace, samples).items() if group == "main tank"), None)


def healer_rows(trace: Trace, samples: Samples | None = None) -> dict:
    """From the pull on, per living healer and phase its distances to the main tank and to whoever held each
    side (`gaps[(healer, label)]["main"|0|1]`), the milliseconds over HEALER_FAR_YD from a side tank a living
    burrower targeted (`far[(healer, side, label)]`), and per side, and for the main tank under side "main",
    the milliseconds its holder lived with no living healer within HEALER_REACH_YD (`bare[(side, tank,
    label)]`). A snapshot stands for the time since the one before, capped at MAX_STEP_MS."""
    samples = samples or Samples(trace)
    healers = healer_guids(trace)
    main = main_tank(trace, samples) if healers else None
    sides = side_spans(trace)
    spans = phase_spans(trace)
    burrowers = guids_of_entry(trace, NPC_BURROWER)
    gaps: dict = collections.defaultdict(lambda: collections.defaultdict(list))
    far: collections.Counter = collections.Counter()
    bare: collections.Counter = collections.Counter()
    labels: list[str] = []
    last = None
    for snap in frames(trace) if healers else ():
        when = snap["t"]
        step = 0 if last is None else min(when - last, MAX_STEP_MS)
        last = when
        if when < 0:
            continue
        units = {row[0]: row for row in snap.get("u", [])}
        label = phase_at(spans, when)
        if label not in labels:
            labels.append(label)
        tanks = {side: as_guid(held_at(sides[side], when)) for side in (0, 1)}
        held = {side: bool(tank) and any(row[0] in burrowers and row[5] > 0 and len(row) > 7 and row[7] == tank
                                         for row in units.values())
                for side, tank in tanks.items()}
        living = [units[guid] for guid in healers if guid in units and units[guid][5] > 0]
        for row in living:
            for key, tank in (("main", main), (0, tanks[0]), (1, tanks[1])):
                unit = units.get(tank) if tank else None
                if unit is None or unit[5] <= 0:
                    continue
                gap = dist2(row[1:3], unit[1:3])
                gaps[(row[0], label)][key].append(gap)
                if key != "main" and held[key] and gap > HEALER_FAR_YD:
                    far[(row[0], key, label)] += step
        for side, tank in list(tanks.items()) + [("main", main)]:
            unit = units.get(tank) if tank else None
            if unit is not None and unit[5] > 0 and not any(dist2(row[1:3], unit[1:3]) <= HEALER_REACH_YD
                                                            for row in living):
                bare[(side, tank, label)] += step
    return {"healers": healers, "main": main, "labels": labels, "gaps": gaps, "far": far, "bare": bare}


def healer_follows(trace: Trace, samples: Samples | None = None) -> dict[int, list[int]]:
    """Per healer, its `follow` act rows with an OK verdict while it lived, from the pull to the end. A verdict
    is written when it changes and again every 10 s it holds, so these are rows, not ticks."""
    samples = samples or Samples(trace)
    healers = set(healer_guids(trace))
    end = pull_end(trace)
    out: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("act"):
        guid = rec.get("g")
        if guid not in healers or rec.get("a") != "follow" or rec.get("vd") != "OK" or not 0 <= rec["t"] <= end:
            continue
        row = samples.recent(guid, rec["t"])
        if row is None or row[5] > 0:
            out[guid].append(rec["t"])
    return dict(out)


def engine_passes(trace: Trace) -> dict[int, dict[str, int]]:
    """Per bot, the passes each engine ran from the session's open: every node lands in exactly one of
    PASS_COLUMNS on each pass, so their sum is the count. The most common sum is taken, because a node two
    strategies of one engine both carry shares one id and reads double."""
    sums: dict = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
    for rec in trace.of("cov"):
        for row in rec.get("r", []):
            columns = dict(zip(COVERAGE_COLUMNS, list(row[1:]) + [0] * (len(COVERAGE_COLUMNS) + 1 - len(row))))
            engine = trace.covnodes.get(row[0], {}).get("engine", "?")
            sums[rec.get("g")][engine][row[0]] += sum(columns[name] for name in PASS_COLUMNS)
    return {guid: {engine: collections.Counter(nodes.values()).most_common(1)[0][0]
                   for engine, nodes in engines.items()}
            for guid, engines in sums.items()}


def healer_engine_coverage(trace: Trace) -> dict[str, dict]:
    """Per engine, the bots whose `cov` rows carry a `raid healer engine` node, and its checks, fires and wins
    summed."""
    ids = {node_id for node_id, node in trace.covnodes.items() if node["strategy"] == HEALER_ENGINE}
    out: dict[str, dict] = {}
    for rec in trace.of("cov"):
        for row in rec.get("r", []):
            if row[0] not in ids:
                continue
            columns = dict(zip(COVERAGE_COLUMNS, list(row[1:]) + [0] * (len(COVERAGE_COLUMNS) + 1 - len(row))))
            entry = out.setdefault(trace.covnodes[row[0]]["engine"], {"bots": set(), "checks": 0, "fires": 0, "won": 0})
            entry["bots"].add(rec.get("g"))
            for name in ("checks", "fires", "won"):
                entry[name] += columns[name]
    return out


def hstand_time(trace: Trace) -> dict[int, dict[str, int]]:
    """Per bot, the milliseconds of the pull each `anub.hstand` value held."""
    end = pull_end(trace)
    out: dict[int, dict[str, int]] = {}
    for guid, spans in holder_spans(trace, "anub.hstand", end).items():
        held: dict[str, int] = {}
        for value, start, stop in spans:
            held[value] = held.get(value, 0) + covered([(start, stop)], 0, end)
        out[guid] = held
    return out


def behind_walks(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per healer that noted `anub.hbehind` go: its first go, and the first snapshot from it at which its
    `anub.hstand` read hold and it then moved under STALL_YD over STALL_MS, alive: stood still at its stand. None
    for one that never did."""
    samples = samples or Samples(trace)
    stands = holder_spans(trace, "anub.hstand", pull_end(trace))
    out = []
    for guid, rows in sorted(per_bot(trace, "anub.hbehind").items(), key=lambda item: trace.name(item[0])):
        go = next((when for when, value in rows if value == "go"), None)
        if go is None:
            continue
        holds = [(start, stop) for value, start, stop in stands.get(guid, []) if value == "hold"]
        stamps, track = samples.track(guid)
        arrived = None
        for index in range(bisect.bisect_left(stamps, go), len(stamps)):
            when = stamps[index]
            later = bisect.bisect_left(stamps, when + STALL_MS)
            if (later < len(stamps) and stamps[later] - when <= STALL_MS + MAX_STEP_MS and track[later][5] > 0
                    and any(low <= when < high for low, high in holds)
                    and dist2(track[index][1:3], track[later][1:3]) < STALL_YD):
                arrived = when
                break
        out.append({"guid": guid, "go": go, "arrived": arrived})
    return out


def segment_yd(a, b, point) -> float:
    """2D yards from `point` to the segment from `a` to `b`."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = dx * dx + dy * dy
    share = 0.0 if not length else max(0.0, min(1.0, ((point[0] - a[0]) * dx + (point[1] - a[1]) * dy) / length))
    return math.hypot(a[0] + share * dx - point[0], a[1] + share * dy - point[1])


def healer_reaches(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Phase 3: per burrower and run up, its first sample within MELEE_YD of a living healer, the nearest, and
    whether its path there came within PERMAFROST_SLOW_REACH of a Permafrost centre (`crossed`). A run starts
    at its BURROW_EXITS exit when it is the spawn, else where it emerged; each step is read as a straight line."""
    span = swarm_span(trace)
    healers = healer_guids(trace)
    if span is None or not healers:
        return []
    samples = samples or Samples(trace)
    out = []
    for burrower in burrower_rows(trace, samples):
        stamps, rows = samples.track(burrower["guid"])
        ups = burrower["ups"]
        for index, up in enumerate(ups):
            first = bisect.bisect_left(stamps, up)
            last = bisect.bisect_left(stamps, ups[index + 1]) if index + 1 < len(ups) else len(stamps)
            start = rows[first][1:3]
            if not index:
                start = min(BURROW_EXITS, key=lambda spot: dist2(spot, start))[:2]
            crossed = False
            for at in range(first, last):
                when, row = stamps[at], rows[at]
                crossed = crossed or any(segment_yd(start, row[1:3], centre) <= PERMAFROST_SLOW_REACH
                                         for centre in samples.frost_at(when))
                start = row[1:3]
                if row[5] <= 0 or not span[0] <= when < span[1]:
                    continue
                near = [(dist2(unit[1:3], row[1:3]), guid) for guid in healers
                        if (unit := samples.recent(guid, when)) and unit[5] > 0]
                yards, healer = min(near, default=(None, None))
                if yards is not None and yards <= MELEE_YD:
                    out.append({"guid": burrower["guid"], "t": when, "healer": healer, "emerged": bool(index),
                                "target": row[7] if len(row) > 7 else 0, "crossed": crossed})
                    break
    return sorted(out, key=lambda row: row["t"])


def healer_mana(trace: Trace, samples: Samples | None = None) -> dict:
    """Per healer, its mana at phase 3's start and SWARM_MANA_MS in (`mana[guid]`, each None for one dead or
    unsampled then, or a phase 3 over by then); and per roster member, healer or not, its Divine Plea, Innervate
    and Hymn of Hope casts as `(t, spell, target, phase label)` (`casts[guid]`). `start` None with no phase 3."""
    samples = samples or Samples(trace)
    spans = phase_spans(trace)
    swarm = swarm_span(trace, spans)

    def mana(guid: int, when: int) -> float | None:
        row = samples.recent(guid, when)
        return row[6] if row is not None and len(row) > 6 and row[5] > 0 else None

    out: dict = {"start": None if swarm is None else swarm[0], "mana": {}, "casts": collections.defaultdict(list)}
    if swarm is not None:
        later = swarm[0] + SWARM_MANA_MS
        out["mana"] = {guid: (mana(guid, swarm[0]), mana(guid, later) if later <= swarm[1] else None)
                       for guid in healer_guids(trace)}
    roster = roster_guids(trace)
    for rec in trace.of("cast"):
        if rec.get("sp") in MANA_COOLDOWNS and not rec.get("tr") and rec.get("s") in roster:
            out["casts"][rec["s"]].append((rec["t"], rec["sp"], rec.get("tgt") or 0, phase_at(spans, rec["t"])))
    return out


def caster_mana(trace: Trace, samples: Samples | None = None) -> dict:
    """Per warlock and mage bot, its mana at phase 3's start and SWARM_MANA_MS in (`mana[guid]`, as `healer_mana`
    reads it), and its Life Taps and Evocations from his first sample at TOPUP_FROM_PCT or under to phase 3's
    start, else the pull's end (`topups[guid]`, a Counter by spell name). `from` None while he never got there."""
    samples = samples or Samples(trace)
    swarm = swarm_span(trace)
    casters = sorted((member["g"] for member in trace.header.get("roster", [])
                      if member.get("c") in (CLASS_WARLOCK, CLASS_MAGE) and not member.get("h")), key=trace.name)
    stamps, rows = samples.track(boss_guid(trace))
    low = next((when for when, row in zip(stamps, rows) if 0 < row[5] <= TOPUP_FROM_PCT), None)

    def mana(guid: int, when: int) -> float | None:
        row = samples.recent(guid, when)
        return row[6] if row is not None and len(row) > 6 and row[5] > 0 else None

    out: dict = {"from": low, "mana": {}, "topups": {guid: collections.Counter() for guid in casters}}
    if swarm is not None:
        later = swarm[0] + SWARM_MANA_MS
        out["mana"] = {guid: (mana(guid, swarm[0]), mana(guid, later) if later <= swarm[1] else None)
                       for guid in casters}
    stop = swarm[0] if swarm is not None else pull_end(trace)
    for rec in trace.of("cast"):
        name = trace.spells.get(rec.get("sp"), "")
        if (low is not None and name in TOPUP_SPELLS and not rec.get("tr") and rec.get("s") in out["topups"]
                and low <= rec["t"] < stop):
            out["topups"][rec["s"]][name] += 1
    return out


def mana_cast_text(trace: Trace, guid: int, cast: tuple) -> str:
    when, spell, target, label = cast
    onto = f" on {trace.name(target)}" if spell == SPELL_INNERVATE and target and target != guid else ""
    return f"{MANA_COOLDOWNS[spell]}{onto} {label} {clock(when)}"


def show_healer_mana(trace: Trace, samples: Samples) -> None:
    read = healer_mana(trace, samples)

    def pct(value) -> str:
        return "-" if value is None else f"{value:.0f}%"

    start = "no phase 3" if read["start"] is None else f"phase 3's start {clock(read['start'])}"
    print(f"  mana at {start} and {SWARM_MANA_MS // 1000} s in, - dead or unsampled, and the Divine Plea, Innervate"
          " and Hymn of Hope\n  casts with their phase:")
    healers = healer_guids(trace)
    others = sorted((guid for guid in read["casts"] if guid not in healers), key=trace.name)
    for guid in healers + others:
        mana = read["mana"].get(guid)
        level = "" if mana is None else f"{pct(mana[0])} > {pct(mana[1])}; "
        casts = ", ".join(mana_cast_text(trace, guid, cast) for cast in read["casts"].get(guid, [])) or "no cast"
        print(f"    {trace.name(guid)[:14]:14} {level}{casts}" + ("" if guid in healers else f"  ({trace.role(guid)})"))

    casters = caster_mana(trace, samples)
    if not casters["topups"]:
        return
    since = "he never got there" if casters["from"] is None else clock(casters["from"])
    print(f"  warlock and mage bots, mana the same way, and their Life Taps and Evocations from his"
          f" {TOPUP_FROM_PCT:.0f}% ({since}) to phase 3:")
    for guid, counts in casters["topups"].items():
        mana = casters["mana"].get(guid)
        level = "" if mana is None else f"{pct(mana[0])} > {pct(mana[1])}; "
        spent = ", ".join(f"{name} {n}" for name, n in counts.most_common()) or "none"
        print(f"    {trace.name(guid)[:14]:14} {level}{spent}")


def show_healer_engines(trace: Trace, healers: list[int]) -> None:
    if not trace.of("cov"):
        print("  no cov records, so no engine passes and no raid healer engine coverage")
        return
    passes = engine_passes(trace)
    parts = []
    for guid in healers:
        mine = passes.get(guid, {})
        combat, idle = mine.get("c", 0), mine.get("n", 0)
        share = f" ({idle * 100 / (combat + idle):.0f}%)" if combat + idle else ""
        parts.append(f"{trace.name(guid)} " + ", ".join(
            f"{ENGINE_NAMES[engine]} {mine.get(engine, 0)}" + (share if engine == "n" else "") for engine in "cnd"))
    print("  engine passes from the session's open, the non-combat share of combat and non-combat: "
          + ("; ".join(parts) or "none"))
    coverage = healer_engine_coverage(trace)
    if not coverage:
        print(f"  {HEALER_ENGINE} in the coverage: absent from every engine")
        return
    print(f"  {HEALER_ENGINE} in the coverage: " + "; ".join(
        f"{ENGINE_NAMES.get(engine, engine)} {len(entry['bots'])} bot(s), {entry['checks']} checks,"
        f" {entry['fires']} fires, {entry['won']} won" for engine, entry in sorted(coverage.items())))


def healing_done(trace: Trace) -> list[dict]:
    """Per phase, every span of it added up, healing done on Warcraft Logs' rules: per source with pets, guardians
    and totems folded into their owner, `[effective, absorbed, overheal]`, the effective healing a heal's gross less
    its overheal and the absorbs credited to the shield's caster. `unowned` holds the sources off the roster the
    trace names no owner for, kept as their own. Heals onto pets never reach a trace."""
    spans = [(start, stop, phase) for start, stop, phase, _ in phase_spans(trace) if phase]
    out: dict[int, dict] = {}
    for start, stop, phase in spans:
        entry = out.setdefault(phase, {"label": PHASE_LABELS.get(phase, str(phase)), "ms": 0,
                                       "sources": collections.defaultdict(lambda: [0, 0, 0]), "unowned": set()})
        entry["ms"] += stop - start
    if not out:
        return []
    roster = roster_guids(trace)

    def phase_of(when: int) -> int | None:
        return next((phase for start, stop, phase in spans if start <= when < stop), None)

    for rec in trace.of("heal", "abs"):
        phase = phase_of(rec["t"])
        if phase is None:
            continue
        source = rec.get("s")
        owner = member_of(trace, source)
        entry = out[phase]
        if owner not in roster and owner == source:
            entry["unowned"].add(source)
        totals = entry["sources"][owner]
        if rec["e"] == "abs":
            totals[1] += rec.get("a", 0)
        else:
            totals[0] += rec.get("a", 0) - rec.get("oh", 0)
            totals[2] += rec.get("oh", 0)
    return [out[phase] for phase in sorted(out)]


def show_healing_done(trace: Trace) -> None:
    phases = healing_done(trace)
    print("  healing done per phase on Warcraft Logs' rules: effective healing (heals less overheal) plus the"
          " absorbs\n  credited to the shield's caster, overheal as a share of that plus overheal, HPS over the"
          " phase's length,\n  pets and totems folded into their owner; heals onto pets never reach the trace:"
          + ("" if phases else " no anub.phase rows"))
    for phase in phases:
        seconds = phase["ms"] / 1000
        rows = sorted(((guid, heal + absorbed, absorbed, over) for guid, (heal, absorbed, over)
                       in phase["sources"].items() if heal + absorbed + over > 0), key=lambda row: -row[1])
        total = sum(row[1] for row in rows)
        print(f"    {phase['label']} {seconds:.1f} s, raid {total / seconds / 1000 if seconds else 0:.1f}k HPS")
        print(f"      {'source':14} {'role':6} {'healing':>9} {'absorbs':>8} {'over':>5} {'HPS':>6} {'share':>5}")
        for guid, done, absorbed, over in rows:
            print(f"      {trace.name(guid)[:14]:14} {trace.role(guid):6} {done / 1000:8,.0f}k {absorbed / 1000:7,.0f}k"
                  f" {over * 100 / (done + over) if done + over else 0:4.0f}%"
                  f" {kilo(done / seconds if seconds else 0):>6} {done * 100 / total if total else 0:4.0f}%")
        if phase["unowned"]:
            print("      sources the trace names no owner for, kept as their own: "
                  + ", ".join(trace.name(guid) for guid in sorted(phase["unowned"])))


def show_healers(trace: Trace) -> None:
    print("HEALERS")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    samples = Samples(trace)
    rows = healer_rows(trace, samples)
    if not rows["healers"]:
        print("  no healer on the roster")
        return

    def median(values) -> str:
        return f"{statistics.median(values):.1f}" if values else "-"

    print(f"  median yards from each healer to the main tank ({trace.name(rows['main'])}) and to whoever held each"
          f" side, per phase;\n  far is the seconds over {HEALER_FAR_YD:.0f} yd from a side tank a burrower was on:")
    print(f"  {'healer':14} {'phase':5} {'main':>6} {'side 0':>6} {'side 1':>6} {'far 0':>6} {'far 1':>6}")
    for guid in rows["healers"]:
        for label in rows["labels"]:
            gaps = rows["gaps"].get((guid, label), {})
            far = [rows["far"][(guid, side, label)] / 1000 for side in (0, 1)]
            if not gaps and not any(far):
                continue
            print(f"  {trace.name(guid)[:14]:14} {label:5} {median(gaps.get('main')):>6} {median(gaps.get(0)):>6}"
                  f" {median(gaps.get(1)):>6} {far[0]:6.1f} {far[1]:6.1f}"
                  + ("  human" if guid in trace.humans else ""))

    follows = healer_follows(trace, samples)
    print("  follow acts, OK and alive in the pull: " + (", ".join(
        f"{trace.name(guid)} {len(times)} ({clock(times[0])}-{clock(times[-1])})"
        for guid, times in sorted(follows.items(), key=lambda item: trace.name(item[0]))) or "none")
          + "; rows per verdict change")
    show_healer_engines(trace, rows["healers"])
    show_counts(trace, "anub.hstand")
    held = hstand_time(trace)
    if held:
        print("  anub.hstand seconds: " + "; ".join(
            f"{trace.name(guid)} " + ", ".join(f"{value} {ms / 1000:.1f} s" for value, ms in values.items())
            for guid, values in sorted(held.items(), key=lambda item: trace.name(item[0]))))
    walks = behind_walks(trace, samples)
    print("  anub.hbehind, each healer's first go and the seconds until it stood still at its stand, anub.hstand"
          f" hold and\n  under {STALL_YD} yd in {STALL_MS / 1000:.0f} s: " + (", ".join(
              f"{trace.name(row['guid'])} {clock(row['go'])} "
              + ("never" if row["arrived"] is None else f"{(row['arrived'] - row['go']) / 1000:.1f} s")
              for row in walks) or "none"))
    reaches = healer_reaches(trace, samples)
    clear = [row for row in reaches if not row["crossed"]]
    print(f"  burrowers within {MELEE_YD:.0f} yd of a healer in phase 3: {len(reaches)}; with no step from the exit"
          f" or the emerge\n  within {PERMAFROST_SLOW_REACH} yd of a Permafrost centre: " + (", ".join(
              f"{short(trace, row['guid'])} {trace.name(row['healer'])} {clock(row['t'])}"
              + (" after an emerge" if row["emerged"] else "") + (" on it" if row["target"] == row["healer"] else "")
              for row in clear) or "none"))

    bare: dict[tuple[int, int], list[str]] = collections.defaultdict(list)
    main_bare: list[str] = []
    for (side, tank, label), ms in rows["bare"].items():
        if not ms:
            continue
        if side == "main":
            main_bare.append(f"{label} {ms / 1000:.1f} s")
        else:
            bare[(side, tank)].append(f"{label} {ms / 1000:.1f} s")
    print(f"  side tank alive with no living healer within {HEALER_REACH_YD:.0f} yd:"
          + ("" if bare else " none"))
    for (side, tank), parts in sorted(bare.items()):
        print(f"    side {side} {trace.name(tank)[:14]:14} {', '.join(parts)}")
    print(f"  main tank {trace.name(rows['main']) if rows['main'] else '-'} alive with no living healer within"
          f" {HEALER_REACH_YD:.0f} yd: " + (", ".join(main_bare) or "none"))
    show_healer_mana(trace, samples)
    show_healing_done(trace)


# ---------------------------------------------------------------------------------------------- tanks

def stack_tracks(trace: Trace, spells) -> dict[int, list[tuple[int, int]]]:
    """Per unit, `(t, stacks)` at every aura row under any of `spells`, 0 at a removal."""
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") in spells:
            out[rec.get("d")].append((rec["t"], 0 if rec.get("r") else int(rec.get("st") or 1)))
    return dict(out)


def expose_tracks(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    return stack_tracks(trace, EXPOSE_WEAKNESS)


def stack_at(track: list[tuple[int, int]], when: int) -> int:
    return next((stacks for at, stacks in reversed(track) if at <= when), 0)


def stack_window(track: list[tuple[int, int]], low: int, high: int) -> tuple[int, int]:
    """`(peak, ms)`: the highest stack held inside `[low, high)` and the time at DEFENSIVE_STACKS or more."""
    held = next((stacks for at, stacks in reversed(track) if at < low), 0)
    peak, ms, since = held, 0, low
    for at, stacks in track:
        if not low <= at < high:
            continue
        if held >= DEFENSIVE_STACKS:
            ms += at - since
        held, since = stacks, at
        peak = max(peak, held)
    if held >= DEFENSIVE_STACKS:
        ms += high - since
    return peak, ms


def side_tank_list(trace: Trace) -> list[int]:
    """Every holder of `anub.side0` or `anub.side1`, side 0's first, each once."""
    _, holders = side_tanks(trace)
    return list(dict.fromkeys(guid for side in sorted(holders) for guid in holders[side]))


def expose_waves(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower wave and side tank, the peak Expose Weakness stack and the milliseconds at DEFENSIVE_STACKS
    or more, from the wave's spawn to the next wave's or the end."""
    samples = samples or Samples(trace)
    waves = wave_starts(trace, samples)
    end = pull_end(trace)
    tracks = expose_tracks(trace)
    tanks = side_tank_list(trace)
    out = []
    for index, (wave, spawn) in enumerate(waves):
        upto = waves[index + 1][1] if index + 1 < len(waves) else end
        for tank in tanks:
            peak, ms = stack_window(tracks.get(tank, []), spawn, upto)
            out.append({"wave": wave, "start": spawn, "stop": upto, "tank": tank, "peak": peak, "ms": ms})
    return out


def held_side(sides, guid: int, when: int) -> int | None:
    return next((side for side, spans in enumerate(sides) if as_guid(held_at(spans, when)) == guid), None)


def defensive_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each TANK_DEFENSIVES cast by a tank, from its cast row: the bot, its Expose Weakness stack and health
    then, the phase, and the side it held, None for neither. `anub.defensive` is written once per verdict
    change, so a second cast of the same button never shows there."""
    samples = samples or Samples(trace)
    tracks = expose_tracks(trace)
    sides = side_spans(trace)
    spans = phase_spans(trace)
    out = []
    for rec in trace.of("cast"):
        guid, when = rec.get("s"), rec["t"]
        if rec.get("tr") or rec.get("sp") not in TANK_DEFENSIVES or trace.role(guid) != "tank":
            continue
        row = samples.recent(guid, when)
        out.append({"t": when, "guid": guid, "spell": TANK_DEFENSIVES[rec["sp"]],
                    "stacks": stack_at(tracks.get(guid, []), when),
                    "hp": row[5] if row else None, "phase": phase_at(spans, when),
                    "side": held_side(sides, guid, when)})
    return out


def form_drops(trace: Trace) -> list[dict]:
    """Each TANK_FORMS aura a tank lost while he was up, phase 1 or 3, not to a death: the tank's OK act row
    nearest the removal, `parent` the other action a PREREQ row just before it ran it for, `back` when the form
    went on again and `died` the tank's next death, each None for none."""
    up = [(start, stop, label) for start, stop, phase, label in phase_spans(trace) if phase in (1, 3)]
    deaths: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("death"):
        deaths[rec.get("g")].append(rec["t"])
    acts: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("act"):
        acts[rec.get("g")].append(rec)
    forms = [rec for rec in trace.of("aura") if rec.get("sp") in TANK_FORMS and trace.role(rec.get("d")) == "tank"]
    out = []
    for rec in forms:
        guid, when = rec.get("d"), rec["t"]
        label = next((label for start, stop, label in up if start <= when < stop), None)
        if not rec.get("r") or label is None or any(0 <= dead - when <= FORM_DEATH_MS for dead in deaths[guid]):
            continue
        near = [act for act in acts[guid] if act.get("vd") == "OK" and abs(act["t"] - when) <= FORM_ACT_MS]
        act = min(near, key=lambda act: abs(act["t"] - when), default=None)
        parent = None if act is None else next(
            (row.get("a") for row in reversed(acts[guid]) if row.get("vd") == "PREREQ"
             and act["t"] - PREREQ_MS <= row["t"] <= act["t"] and row.get("a") != act.get("a")), None)
        out.append({"t": when, "guid": guid, "spell": rec["sp"], "phase": label, "act": act, "parent": parent,
                    "back": next((row["t"] for row in forms if row.get("d") == guid and row["sp"] == rec["sp"]
                                  and not row.get("r") and row["t"] > when), None),
                    "died": min((dead for dead in deaths[guid] if dead > when), default=None)})
    return out


def form_drop_text(row: dict) -> str:
    act = row["act"]
    text = (f"no OK act row within {FORM_ACT_MS / 1000:.1f} s" if act is None
            else f"by {act.get('a')}" + (f", run for {row['parent']}" if row["parent"] else ""))
    died = row["died"] is not None and (row["back"] is None or row["died"] < row["back"])
    if died:
        return text + f"; not back before its death {(row['died'] - row['t']) / 1000:.1f} s later"
    if row["back"] is None:
        return text + "; never back"
    return text + f"; back after {(row['back'] - row['t']) / 1000:.1f} s"


def magic_on_side_tank(row: dict) -> bool:
    """A magic-only defensive on a side tank before the swarm: burrowers hit physical."""
    return row["side"] is not None and row["spell"] in MAGIC_ONLY_DEFENSIVES and not row["phase"].startswith("P3")


def source_kind(trace: Trace, guid) -> str:
    kinds = {NPC_BURROWER: "burrower", NPC_SCARAB: "scarab", NPC_ANUBARAK: "Anub'arak", NPC_SPIKE: "spike",
             NPC_FROST_SPHERE: "sphere"}
    return kinds.get(trace.entries.get(guid)) or trace.name(guid)


def tank_deaths(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each tank's death: the damage it took in the TANK_DEATH_LOOKBACK_MS before, by source kind, the heals
    landed on it then, gross and overheal, the absorbs, its peak Expose Weakness stack then, the side it
    held, and whether it was the main tank."""
    samples = samples or Samples(trace)
    main = main_tank(trace, samples)
    tracks = expose_tracks(trace)
    sides = side_spans(trace)
    spans = phase_spans(trace)
    hits, heals, shields = trace.of("dmg"), trace.of("heal"), trace.of("abs")
    out = []
    for rec in combat_deaths(trace):
        guid, when = rec.get("g"), rec["t"]
        if trace.role(guid) != "tank":
            continue
        low = when - TANK_DEATH_LOOKBACK_MS

        def inside(row) -> bool:
            return row.get("d") == guid and low <= row["t"] <= when

        taken: collections.Counter = collections.Counter()
        for hit in filter(inside, hits):
            taken[source_kind(trace, hit.get("s"))] += hit.get("a", 0)
        landed = list(filter(inside, heals))
        out.append({"t": when, "guid": guid, "phase": phase_at(spans, when), "side": held_side(sides, guid, when - 1),
                    "main": guid == main, "taken": taken, "heals": len(landed),
                    "gross": sum(row.get("a", 0) for row in landed),
                    "overheal": sum(row.get("oh", 0) for row in landed),
                    "absorbed": sum(row.get("a", 0) for row in filter(inside, shields)),
                    "expose": stack_window(tracks.get(guid, []), low, when + 1)[0]})
    return out


def main_tank_exposes(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each run of Expose Weakness on the main tank, from its first stack to its removal (`off` None for one up
    at the end): the peak and when it first read so, the burrowers that put stacks on in turn, and his own area
    casts, AREA's and AREA_TAUNTS' with triggered ones left out, from EXPOSE_LEAD_MS before the first stack to
    the peak. Every burrower shares one aura whose row names only its first caster, so each burrower sampled on
    him at a stack counts too."""
    samples = samples or Samples(trace)
    main = main_tank(trace, samples)
    if main is None:
        return []
    burrowers = sorted(guids_of_entry(trace, NPC_BURROWER))
    out: list[dict] = []
    run = None
    for rec in trace.of("aura"):
        if rec.get("sp") not in EXPOSE_WEAKNESS or rec.get("d") != main:
            continue
        stacks = 0 if rec.get("r") else int(rec.get("st") or 1)
        if not stacks:
            if run:
                run["off"] = rec["t"]
                out.append(run)
            run = None
            continue
        if run is None:
            run = {"start": rec["t"], "peak": 0, "at": rec["t"], "off": None, "by": []}
        if stacks > run["peak"]:
            run.update(peak=stacks, at=rec["t"])
        on = [rec.get("s")] + [guid for guid in burrowers if (row := samples.recent(guid, rec["t"]))
                               and row[5] > 0 and len(row) > 7 and row[7] == main]
        run["by"] = list(dict.fromkeys(run["by"] + [guid for guid in on if guid]))
    if run:
        out.append(run)
    area = [rec for rec in trace.of("cast") if rec.get("s") == main and not rec.get("tr")
            and (rec.get("sp") in AREA or rec.get("sp") in AREA_TAUNTS)]
    for run in out:
        run["area"] = [rec for rec in area if run["start"] - EXPOSE_LEAD_MS <= rec["t"] <= run["at"]]
    return out


def side_tank_area_casts(trace: Trace) -> dict[tuple[int, str], collections.Counter]:
    """Per side tank and surfaced phase, its SIDE_AOE_SPELLS casts by the trace's spell name, any rank, triggered
    ones left out."""
    tanks = set(side_tank_list(trace))
    surfaced = [(start, stop, label) for start, stop, phase, label in phase_spans(trace) if phase in (1, 3)]
    out: dict[tuple[int, str], collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in trace.of("cast"):
        name = trace.spells.get(rec.get("sp"), "")
        if rec.get("tr") or rec.get("s") not in tanks or name.lower() not in SIDE_AOE_SPELLS:
            continue
        label = next((label for start, stop, label in surfaced if start <= rec["t"] < stop), None)
        if label is not None:
            out[(rec["s"], label)][name] += 1
    return dict(out)


def show_side_tank_aoe(trace: Trace) -> None:
    casts = side_tank_area_casts(trace)
    print("  side tank area casts per surfaced phase, anub.saoe's spells, triggered ones left out:"
          + ("" if casts else " none"))
    labels = [label for _, _, phase, label in phase_spans(trace) if phase in (1, 3)]
    for guid in side_tank_list(trace):
        for label in labels:
            counts = casts.get((guid, label))
            if counts:
                print(f"    {trace.name(guid)[:14]:14} {label:5} "
                      + ", ".join(f"{name} {n}" for name, n in counts.most_common()))
    show_counts(trace, "anub.saoe")


def show_main_exposes(trace: Trace, samples: Samples) -> None:
    main = main_tank(trace, samples)
    if main is None:
        return
    rows = main_tank_exposes(trace, samples)
    if not rows:
        print(f"  the main tank {trace.name(main)}'s Expose Weakness: none")
        return
    print(f"  the main tank {trace.name(main)}'s Expose Weakness per run of stacks: its peak, the burrowers on him at"
          f" a stack,\n  and his area casts from {EXPOSE_LEAD_MS / 1000:.0f} s before the first stack to the peak,"
          " triggered ones left out:")
    for row in rows:
        off = "up at the end" if row["off"] is None else f"off {clock(row['off'])}"
        casts = ", ".join(f"{trace.spell(rec['sp'])} {round((rec['t'] - row['start']) / 1000, 1) + 0.0:+.1f} s"
                          for rec in row["area"])
        print(f"    {clock(row['start']):>9}  peak {row['peak']} at {clock(row['at'])}, {off};"
              f" by {', '.join(short(trace, guid) for guid in row['by']) or '-'}; area casts: {casts or 'none'}")


def show_tanks(trace: Trace) -> None:
    print("TANKS")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    samples = Samples(trace)
    waves = expose_waves(trace, samples)
    if not waves:
        print("  no burrower wave or no side tank latched")
    else:
        print(f"  Expose Weakness ({', '.join(str(spell) for spell in EXPOSE_WEAKNESS)}) per burrower wave on each"
              f" side tank: peak stack,\n  seconds at {DEFENSIVE_STACKS} or more")
        by_wave: dict[tuple[int, int], list[str]] = collections.defaultdict(list)
        for row in waves:
            by_wave[(row["wave"], row["start"])].append(f"{trace.name(row['tank'])} {row['peak']}"
                                                        f" ({row['ms'] / 1000:.1f} s)")
        for (wave, start), parts in by_wave.items():
            print(f"    wave {wave}  {clock(start):>9}  {', '.join(parts)}")
    tanks = set(side_tank_list(trace))
    others = {guid: max(stacks for _, stacks in track) for guid, track in expose_tracks(trace).items()
              if guid not in tanks and guid in trace.roles}
    print("  Expose Weakness on anyone else, peak over the pull: "
          + (", ".join(f"{trace.name(guid)} {peak}" for guid, peak in sorted(others.items(), key=lambda item: -item[1]))
             or "none"))
    show_main_exposes(trace, samples)
    show_side_tank_aoe(trace)

    rows = defensive_rows(trace, samples)
    if not rows:
        print("  no tank defensive cast")
    else:
        print("\n  each tank defensive cast, from its cast row, with the Expose Weakness stack and health then:")
        for row in rows:
            side = "-" if row["side"] is None else f"side {row['side']}"
            hp = "-" if row["hp"] is None else f"{row['hp']:3.0f}%"
            print(f"    {clock(row['t']):>9} {trace.name(row['guid'])[:14]:14} {side:6} {row['phase']:5}"
                  f" {row['spell'][:22]:22} stack {row['stacks']:2}  hp {hp:>4}"
                  + ("  magic only on a side tank" if magic_on_side_tank(row) else ""))
        print(f"  from {DEFENSIVE_STACKS} stacks: "
              + f"{sum(1 for row in rows if row['side'] is not None and row['stacks'] >= DEFENSIVE_STACKS)} of"
                f" {sum(1 for row in rows if row['side'] is not None)} side tank cast(s); magic only on a side tank"
                f" outside phase 3: {sum(1 for row in rows if magic_on_side_tank(row))}")
        per_tank: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
        for row in rows:
            per_tank[row["guid"]][row["spell"]] += 1
        print("  casts per tank: " + "; ".join(
            f"{trace.name(guid)} " + ", ".join(f"{spell} {n}" for spell, n in counts.most_common())
            for guid, counts in sorted(per_tank.items(), key=lambda item: trace.name(item[0]))))

    drops = form_drops(trace)
    print(f"\n  a tank's form or stance lost while he was up, deaths left out, with the act row within"
          f" {FORM_ACT_MS / 1000:.1f} s that dropped it:" + ("" if drops else " none"))
    for row in drops:
        print(f"    {clock(row['t']):>9} {trace.name(row['guid'])[:14]:14} {row['phase']:5}"
              f" {TANK_FORMS[row['spell']]:16} {form_drop_text(row)}")

    deaths = tank_deaths(trace, samples)
    if not deaths:
        print("  no tank died")
        return
    print(f"\n  each tank death, the {TANK_DEATH_LOOKBACK_MS / 1000:.0f} s before it:")
    for row in deaths:
        side = f"side {row['side']}" if row["side"] is not None else "main" if row["main"] else "-"
        taken = sum(row["taken"].values())
        sources = ", ".join(f"{kind} {amount:,}" for kind, amount in row["taken"].most_common())
        print(f"    {clock(row['t']):>9} {trace.name(row['guid'])[:14]:14} {side:6} {row['phase']:5}"
              f" taken {taken:,}" + (f" ({sources})" if sources else "")
              + f"; healed {row['gross'] - row['overheal']:,} by {row['heals']} heal(s), {row['overheal']:,} over;"
                f" absorbed {row['absorbed']:,}; Expose peak {row['expose']}")


# -------------------------------------------------------------------------------------------- scarabs

def sampled_at(samples: Samples, guid: int, index: int):
    """The unit's row in snapshot `index`, None when that snapshot left it out."""
    stamps, rows = samples.track(guid)
    at = bisect.bisect_right(stamps, samples.stamps[index]) - 1
    return rows[at] if at >= 0 and stamps[at] == samples.stamps[index] else None


def scarab_lives(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per scarab, its first and last living sample, the phase span it came up in, and `past`, how long it
    lived after that span ended, None for one that died inside it."""
    samples = samples or Samples(trace)
    spans = phase_spans(trace)
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_SCARAB)):
        stamps, rows = samples.track(guid)
        living = [when for when, row in zip(stamps, rows) if row[5] > 0]
        if not living:
            continue
        first, last = living[0], living[-1]
        span = next(((start, stop, label) for start, stop, _, label in spans if start <= first < stop), None)
        out.append({"guid": guid, "first": first, "last": last, "label": span[2] if span else "pre",
                    "start": span[0] if span else None,
                    "past": last - span[1] if span and last >= span[1] else None})
    return out


def scarab_emerges(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per emerge, the living scarabs in the snapshot at or before each SCARAB_OFFSETS_MS after it, as a Counter
    of their targets, and the main tank's Mandibles stack at the emerge. An offset past the trace's last
    snapshot is left out."""
    samples = samples or Samples(trace)
    scarabs = sorted(guids_of_entry(trace, NPC_SCARAB))
    main = main_tank(trace, samples)
    track = stack_tracks(trace, MANDIBLES).get(main, []) if main else []
    out = []
    for window in surfaced_windows(phase_spans(trace)):
        if window["kind"] != "emerge":
            continue
        start = window["start"]
        alive = []
        for offset in SCARAB_OFFSETS_MS:
            index = samples.index_at(start + offset)
            if index < 0 or start + offset > samples.stamps[-1]:
                break
            rows = [row for row in (sampled_at(samples, guid, index) for guid in scarabs) if row and row[5] > 0]
            alive.append((offset, collections.Counter(row[7] if len(row) > 7 else 0 for row in rows)))
        out.append({"label": window["label"], "start": start, "alive": alive, "main": main,
                    "stacks": stack_at(track, start)})
    return out


def mandible_windows(trace: Trace) -> list[dict]:
    """Per phase span, `(guid, peak stack, damage)` for each roster member that carried Acid-Drenched Mandibles
    or took its ticks in it, most damage first. A stack carried in from the span before counts as its peak."""
    roster = roster_guids(trace)
    tracks = {guid: track for guid, track in stack_tracks(trace, MANDIBLES).items() if guid in roster}
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") in MANDIBLES and rec.get("d") in roster]
    out = []
    for start, stop, _, label in phase_spans(trace):
        damage: collections.Counter = collections.Counter()
        for rec in hits:
            if start <= rec["t"] < stop:
                damage[rec["d"]] += rec.get("a", 0)
        peaks = {guid: stack_window(track, start, stop)[0] for guid, track in tracks.items()}
        players = [(guid, peaks.get(guid, 0), damage[guid]) for guid in roster if damage[guid] or peaks.get(guid)]
        out.append({"label": label, "start": start, "stop": stop,
                    "players": sorted(players, key=lambda item: (-item[2], -item[1], trace.name(item[0])))})
    return out


def scarab_chases(trace: Trace, samples: Samples | None = None) -> dict[int, dict]:
    """Per roster member a scarab went for: per Acid-Drenched Mandibles stack it gained `(t, moving, gap)`, the gap
    to the nearest scarab on it, None with none sampled (`stacks`); and the snapshot milliseconds a scarab within
    SCARAB_KITE_YD was on it, by its `anub.scarabkite` value then, `-` before its first row (`chased`)."""
    samples = samples or Samples(trace)
    scarabs = sorted(guids_of_entry(trace, NPC_SCARAB))
    roster = roster_guids(trace)
    kites = per_bot(trace, "anub.scarabkite")
    out: dict[int, dict] = collections.defaultdict(lambda: {"stacks": [], "chased": collections.Counter()})

    def gap_to(victim: int, row, when: int) -> float | None:
        gaps = [math.dist(unit[1:3], row[1:3]) for unit in (samples.recent(guid, when) for guid in scarabs)
                if unit is not None and unit[5] > 0 and len(unit) > 7 and unit[7] == victim]
        return min(gaps, default=None)

    for guid, track in stack_tracks(trace, MANDIBLES).items():
        before = 0
        for when, stack in track:
            row = samples.recent(guid, when) if guid in roster else None
            if row is not None and stack > before:
                out[guid]["stacks"].append((when, bool(row[8]) if len(row) > 8 else False, gap_to(guid, row, when)))
            before = stack

    for index, when in enumerate(samples.stamps[:-1]):
        step = min(samples.stamps[index + 1] - when, MAX_STEP_MS)
        chased = set()
        for guid in scarabs:
            unit = sampled_at(samples, guid, index)
            victim = unit[7] if unit is not None and unit[5] > 0 and len(unit) > 7 else 0
            row = samples.recent(victim, when) if victim in roster else None
            if row is not None and row[5] > 0 and math.dist(unit[1:3], row[1:3]) <= SCARAB_KITE_YD:
                chased.add(victim)
        for victim in chased:
            value = next((value for at, value in reversed(kites.get(victim, [])) if at <= when), "-")
            out[victim]["chased"][value] += step
    return dict(out)


def chase_text(row: dict) -> str:
    moving = sum(1 for _, walking, _ in row["stacks"] if walking)
    gaps = [gap for _, _, gap in row["stacks"] if gap is not None]
    stacks = (f"stacks {moving} moving, {len(row['stacks']) - moving} standing"
              + (f", median gap {statistics.median(gaps):.1f} yd" if gaps else "") if row["stacks"] else "no stack")
    chased = row["chased"]
    return stacks + (f"; chased {sum(chased.values()) / 1000:.1f} s ("
                     + ", ".join(f"{value} {ms / 1000:.1f}" for value, ms in chased.most_common()) + ")"
                     if chased else "")


def show_scarabs(trace: Trace) -> None:
    print("SCARABS")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    samples = Samples(trace)
    emerges = scarab_emerges(trace, samples)
    if emerges:
        print("  living scarabs after each emerge, by target, and the main tank's Mandibles stack at the emerge:")
    for row in emerges:
        counts = "; ".join(f"+{offset // 1000} s {sum(targets.values())}"
                           + (" (" + ", ".join(f"{unit_label(trace, guid)} {n}" for guid, n in targets.most_common())
                              + ")" if targets else "")
                           for offset, targets in row["alive"])
        main = f"{trace.name(row['main'])} {row['stacks']}" if row["main"] else "no main tank"
        print(f"    {row['label']:5} {clock(row['start']):>9}  {counts or 'no snapshot after it'}"
              f"\n                     main tank {main}")

    lives = scarab_lives(trace, samples)
    if not lives:
        print("  no scarab sampled")
    else:
        print("\n  scarab lives, first living sample to last, by the phase each came up in:")
        by_label: dict[str, list[dict]] = collections.defaultdict(list)
        for row in sorted(lives, key=lambda row: row["first"]):
            by_label[row["label"]].append(row)
        for label, rows in by_label.items():
            spans = [(row["last"] - row["first"]) / 1000 for row in rows]
            past = [row["past"] / 1000 for row in rows if row["past"] is not None]
            start = "-" if rows[0]["start"] is None else clock(rows[0]["start"])
            print(f"    {label:5} {start:>9}  {len(rows)} scarab(s), median {statistics.median(spans):.1f} s,"
                  f" longest {max(spans):.1f} s; {len(past)} lived past its end"
                  + (f", longest {max(past):.1f} s on" if past else ""))

    chases = scarab_chases(trace, samples)
    if chases:
        print(f"\n  per scarab victim, its Mandibles stacks gained moving or standing with the median gap to the scarab"
              f" on it, and\n  the seconds a scarab within {SCARAB_KITE_YD:.0f} yd was on it by anub.scarabkite then:")
        for guid, row in sorted(chases.items(), key=lambda item: (-len(item[1]["stacks"]), trace.name(item[0]))):
            print(f"    {trace.name(guid)[:14]:14} {chase_text(row)}")

    windows = [row for row in mandible_windows(trace) if row["players"]]
    if not windows:
        print("  nobody carried Acid-Drenched Mandibles")
        return
    print("\n  Acid-Drenched Mandibles per phase: the raid's tick damage, then each player's peak stack and damage:")
    for row in windows:
        total = sum(damage for _, _, damage in row["players"])
        parts = ", ".join(f"{trace.name(guid)} {peak} {damage:,}" for guid, peak, damage in row["players"])
        print(f"    {row['label']:5} {clock(row['start']):>9}  {total:,}: {parts}")


# ------------------------------------------------------------------------------------------ redirects

def redirect_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each Misdirection and Tricks of the Trade cast: caster, recipient and its frost group, the caster's
    target at the cast, and for a burrower `after`, its target at each REDIRECT_AFTER_MS, a repeat left out."""
    samples = samples or Samples(trace)
    groups = frost_groups(trace, samples)
    burrowers = guids_of_entry(trace, NPC_BURROWER)
    out = []
    for rec in trace.of("cast"):
        if rec.get("tr") or rec.get("sp") not in REDIRECTS:
            continue
        caster, when, recipient = rec.get("s"), rec["t"], rec.get("tgt")
        row = samples.row(caster, when)
        target = row[7] if row and len(row) > 7 else 0
        after: list[tuple[int, int]] = []
        if target in burrowers:
            for offset in REDIRECT_AFTER_MS:
                unit = samples.recent(target, when + offset)
                victim = unit[7] if unit and len(unit) > 7 else 0
                if unit is not None and (not after or after[-1][1] != victim):
                    after.append((offset, victim))
        out.append({"t": when, "spell": rec["sp"], "caster": caster, "to": recipient,
                    "group": groups.get(recipient, trace.role(recipient)), "target": target,
                    "on_burrower": target in burrowers, "after": after})
    return out


def show_redirects(trace: Trace) -> None:
    print("REDIRECTS")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    rows = redirect_rows(trace)
    if not rows:
        print("  no Misdirection or Tricks of the Trade cast")
        return
    print("  each Misdirection and Tricks of the Trade: caster, recipient, the caster's target, and a burrower's"
          " target after:")
    for row in rows:
        after = ", ".join(f"+{offset // 1000} s {unit_label(trace, victim)}" for offset, victim in row["after"])
        print(f"    {clock(row['t']):>9} {REDIRECTS[row['spell']]:19} {trace.name(row['caster'])[:12]:12}"
              f" > {trace.name(row['to'])[:14]:14} {row['group'][:9]:9} on {unit_label(trace, row['target'])}"
              + (f": {after}" if after else ""))
    for spell, name in REDIRECTS.items():
        mine = [row for row in rows if row["spell"] == spell]
        if not mine:
            print(f"  {name}: none")
            continue
        to = collections.Counter((row["to"], row["group"]) for row in mine)
        groups = collections.Counter(row["group"] for row in mine)
        print(f"  {name} {len(mine)}: " + ", ".join(f"{trace.name(guid)} ({group}) {n}" for (guid, group), n
                                                     in to.most_common())
              + "; by group " + ", ".join(f"{group} {n}" for group, n in groups.most_common())
              + f"; the caster on a burrower {sum(1 for row in mine if row['on_burrower'])}")
    print("  the group is the --frost one: side tank for any anub.side0 or anub.side1 holder, main tank the other"
          " tank\n  he targeted most, else the role")


# -------------------------------------------------------------------------------------------- pursuit

def chase_runs(samples: Samples, spikes, kiter: int, start: int, stop: int) -> list[list[tuple]]:
    """The snapshots inside the mark where a spike targeted the kiter and both were sampled, as `(t, spike,
    kiter, patches)` with ground points, split where PURSUIT_GAP_MS or more passed between two."""
    kiter_stamps, kiter_rows = samples.track(kiter)
    points = []
    for spike in spikes:
        stamps, rows = samples.track(spike)
        for index in range(bisect.bisect_left(stamps, start), bisect.bisect_right(stamps, stop)):
            row, when = rows[index], stamps[index]
            at = bisect.bisect_left(kiter_stamps, when)
            if len(row) > 7 and row[7] == kiter and at < len(kiter_stamps) and kiter_stamps[at] == when:
                points.append((when, (row[1], row[2]), (kiter_rows[at][1], kiter_rows[at][2]), samples.frost_at(when)))
    runs: list[list[tuple]] = []
    for point in sorted(points, key=lambda item: item[0]):
        if runs and point[0] - runs[-1][-1][0] < PURSUIT_GAP_MS:
            runs[-1].append(point)
        else:
            runs.append([point])
    return runs


def replay_pursuit(run: list[tuple], first: int, horizon: int) -> tuple[float, list[float]] | None:
    """A model spike from the real one's spot at `first`, stepping the real spike's step length each snapshot
    straight at where the kiter is then, for `horizon` ms: its end error against the real spike, and per patch
    within PURSUIT_PATCH_YD of the start its closest pass less the real one's. None when the run ends first."""
    times = [point[0] for point in run]
    last = bisect.bisect_left(times, run[first][0] + horizon)
    if last >= len(run):
        return None
    spot = run[first][1]
    path = [spot]
    for index in range(first, last):
        step = dist2(run[index][1], run[index + 1][1])
        goal = run[index][2]
        gap = dist2(spot, goal)
        if gap > 1e-3:
            share = min(step, gap) / gap
            spot = (spot[0] + (goal[0] - spot[0]) * share, spot[1] + (goal[1] - spot[1]) * share)
        path.append(spot)
    real = [point[1] for point in run[first:last + 1]]
    patches = [patch for patch in run[first][3] if dist2(patch, run[first][1]) < PURSUIT_PATCH_YD]
    passes = [min(dist2(point, patch) for point in path) - min(dist2(point, patch) for point in real)
              for patch in patches]
    return dist2(path[-1], real[-1]), passes


def pursuit_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per mark, pure pursuit replayed against the real spike from every PURSUIT_STEP_MS of each chase run, as
    long as the longest horizon still fits: per horizon the end errors and the signed patch-pass errors."""
    samples = samples or Samples(trace)
    spikes = guids_of_entry(trace, NPC_SPIKE)
    longest = max(PURSUIT_HORIZONS_MS)
    out = []
    for guid, start, stop, _ in mark_spans(trace):
        ends: dict[int, list[float]] = {horizon: [] for horizon in PURSUIT_HORIZONS_MS}
        passes: dict[int, list[float]] = {horizon: [] for horizon in PURSUIT_HORIZONS_MS}
        for run in chase_runs(samples, spikes, guid, start, stop):
            times = [point[0] for point in run]
            begin = run[0][0]
            while begin + longest <= run[-1][0]:
                first = bisect.bisect_left(times, begin)
                for horizon in PURSUIT_HORIZONS_MS:
                    result = replay_pursuit(run, first, horizon)
                    if result is not None:
                        ends[horizon].append(result[0])
                        passes[horizon].extend(result[1])
                begin += PURSUIT_STEP_MS
        out.append({"guid": guid, "start": start, "stop": stop, "ends": ends, "passes": passes})
    return out


def p90(values: list[float]) -> float:
    ordered = sorted(values)
    return ordered[int(len(ordered) * 0.9)]


def pursuit_flagged(passes: list[float]) -> bool:
    return bool(passes) and p90([abs(value) for value in passes]) > PURSUIT_FLAG_YD


def pursuit_text(ends: list[float], passes: list[float]) -> str:
    text = f"end {statistics.median(ends):.1f} / {p90(ends):.1f} yd"
    if passes:
        sizes = [abs(value) for value in passes]
        text += (f", patch pass {statistics.median(sizes):.1f} / {p90(sizes):.1f} yd bias"
                 f" {statistics.median(passes):+.1f} (n {len(passes)})")
    return text


def show_pursuit(trace: Trace) -> None:
    print("PURSUIT")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    rows = [row for row in pursuit_rows(trace) if any(row["ends"].values())]
    if not rows:
        print(f"  no chase lasted the {max(PURSUIT_HORIZONS_MS) / 1000:.0f} s a replay needs")
        return
    print("  pure pursuit replayed per kite, median / p90 per horizon:")
    for row in rows:
        parts = [f"{horizon // 1000} s n {len(row['ends'][horizon])}: "
                 + pursuit_text(row["ends"][horizon], row["passes"][horizon])
                 for horizon in PURSUIT_HORIZONS_MS if row["ends"][horizon]]
        flagged = any(pursuit_flagged(row["passes"][horizon]) for horizon in PURSUIT_HORIZONS_MS)
        print(f"    {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} " + "; ".join(parts)
              + (f"  patch pass p90 over {PURSUIT_FLAG_YD} yd" if flagged else ""))
    flagged = []
    for horizon in PURSUIT_HORIZONS_MS:
        ends = [value for row in rows for value in row["ends"][horizon]]
        passes = [value for row in rows for value in row["passes"][horizon]]
        if not ends:
            continue
        print(f"  all kites, {horizon // 1000} s, n {len(ends)}: {pursuit_text(ends, passes)}")
        if pursuit_flagged(passes):
            flagged.append(f"{horizon // 1000} s")
    print(f"  p90 patch-pass error over {PURSUIT_FLAG_YD} yd over all kites: " + (", ".join(flagged) or "no"))
    print(f"  from every {PURSUIT_STEP_MS / 1000:.1f} s of a chase the model spike steps the real one's step"
          " length each snapshot straight\n  at the kiter's spot then; end is its distance from the real spike"
          f" after the horizon, patch pass its closest\n  pass by each patch within {PURSUIT_PATCH_YD:.0f} yd of the"
          " start less the real spike's, bias that error's signed median")


# -------------------------------------------------------------------------------------------- kitesim
#
# A port of AnubarakKitePlanner (ToCAnubarakKitePlanner.h): its candidates, outcome, Finish and Compare row
# for row. The replay has no Hands, no own immunity, no ready speed boost and no bystanders, so their endings and
# checks are left out but for Finish's own immunity cost and next leg, and its world is heroic's.

KITE_PLANNER_HEADER = RAID_ROOT / "ToC" / "Util" / "ToCAnubarakKitePlanner.h"
PLANNER_FIELDS = ("centre", "floorRadius", "ring", "rim", "headings", "arcStraightS", "arcTurns", "horizonS",
                  "maxHorizonS", "patchReach", "fallingReach", "impaleReach", "slowReach", "edgeRadius", "edgeRunOn",
                  "gatherClearance", "startSlack", "testS", "endingShapes", "nearShapes", "patchNear", "earlyPatchS",
                  "patchWindowS", "edgeWindowS", "turnStepS", "catchMargin", "postCatchS", "idleS", "freshS", "fastS",
                  "ownLateS", "ownLeadS", "costOwn", "costOwnMid", "costPatch",
                  "hazardBandS", "uncoveredBandS", "endBandS", "gatherBand", "marginCap", "marginBand", "keepEndS",
                  "turnWaitDegrees", "turnWaitS")
PLANNER_SCALAR = re.compile(r"\b(?:float|int)\s+(\w+)\s*=\s*(-?\d+(?:\.\d*)?)f?\s*;")
PLANNER_LIST = re.compile(r"(?:std::array<float,\s*\d+>|Point)\s+(\w+)\s*=\s*\{([^}]*)\}\s*;")
SPIKE_LAST_STEP = len(SPIKE_SPEEDS) - 1
# KiteWorld's slowFactor in heroic
KITESIM_SLOW = 0.2
# The native test's Play; the game re-plans every 250 ms
KITESIM_REPLAN_S = 0.3
# the spike's speed step is read off its moving steps over this much from the replay's start
KITESIM_SPEED_MS = 1500
# a kite whose first replayed event came this soon started where no path leaves cleanly
KITESIM_CLEAN_S = 2.0
KITESIM_SHORT_S = 10.0
PATCH_MOVES = ("in", "edge0", "edge1")


@functools.lru_cache(maxsize=1)
def planner_rules() -> dict:
    """AnubarakKitePlanner::Rules' defaults off the header, so a retune shows up here without a second edit.
    LookupError when the header is gone or lacks a field the port reads."""
    try:
        text = KITE_PLANNER_HEADER.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        raise LookupError(f"can't read {KITE_PLANNER_HEADER.name}: {error}") from error
    found = re.search(r"\bstruct Rules\s*\{(.*?)\n\};", text, re.S)
    body = re.sub(r"//[^\n]*", "", found.group(1)) if found else ""
    rules: dict = {name: float(value) for name, value in PLANNER_SCALAR.findall(body)}
    for name, values in PLANNER_LIST.findall(body):
        rules[name] = tuple(float(value.strip().rstrip("f")) for value in values.split(","))
    missing = [name for name in PLANNER_FIELDS if name not in rules]
    if missing:
        raise LookupError(f"{KITE_PLANNER_HEADER.name} Rules lacks {', '.join(missing)}")
    return rules


class KiteWorld:
    """KiteWorld as the replay fills it. `spike` is `(x, y, speed, step, next step s)`, each patch
    `(x, y, tank, usable, falling)`, `gather` a point or None, `own` the kiter's own immunity ready."""

    def __init__(self, kiter, spike, life: float, patches, gather=None, speed: float = BASE_RUN_YD_S,
                 catch_from: float = 0.0, budget: int | None = None, own: bool = False):
        self.kiter = kiter
        self.spike = spike
        self.life = life
        self.patches = list(patches)
        self.gather = gather
        self.speed = speed
        self.catch_from = catch_from
        self.budget = sum(1 for patch in self.patches if patch[3]) if budget is None else budget
        self.own = own

    def moved(self, kiter, spike, seconds: float) -> "KiteWorld":
        """The same world `seconds` on, the kiter and the spike where they got to."""
        x, y, speed, step, due = spike
        return KiteWorld(kiter, (x, y, speed, step, due - seconds), self.life - seconds, self.patches, self.gather,
                         self.speed, max(0.0, self.catch_from - seconds), self.budget, self.own)


class PlanOutcome:
    """Outcome: ticks for its seconds, -1 for none, and `ending` `none` or `patch`."""

    __slots__ = ("ending", "end_tick", "tank", "kite", "raid", "caught", "uncovered", "cost", "margin", "end_point",
                 "end_gather", "catch_tick", "turn_tick")

    @property
    def end_s(self) -> float:
        return self.end_tick * CATCH_TICK_S


class PlanCandidate:
    __slots__ = ("shape", "heading", "radius", "turn", "patch", "move", "turn_tick", "ending", "own", "track")

    def __init__(self, shape: str, heading: float = 0.0, radius: float = 0.0, turn: float = 0.0):
        self.shape, self.heading, self.radius, self.turn = shape, heading, radius, turn
        self.patch, self.move, self.turn_tick, self.ending, self.own = -1, "in", -1, "none", False
        self.track: list[tuple[float, float]] = []

    def like(self, shape: str | None = None) -> "PlanCandidate":
        out = PlanCandidate(shape or self.shape, self.heading, self.radius, self.turn)
        out.patch, out.move, out.turn_tick, out.ending = self.patch, self.move, self.turn_tick, self.ending
        out.own = self.own
        return out


class PlannerRun:
    """Run: the spike and the closest pass per tick from where it began, the first patch fail and raid pass,
    and per patch the spike began inside the tick it first left."""

    __slots__ = ("spikes", "margin", "fail_tick", "fail_patch", "fail_planned", "raid_tick", "left")

    def __init__(self, spike, margin: float, patches: int):
        self.spikes = [spike]
        self.margin = [margin]
        self.fail_tick = self.fail_patch = self.raid_tick = -1
        self.fail_planned = False
        self.left = [-1] * patches


def to_tick(seconds: float) -> int:
    return int(math.floor(seconds / CATCH_TICK_S + 0.5))


def planner_ticks(world: KiteWorld, rules: dict) -> int:
    """To the spike's despawn when that comes by maxHorizonS, else to horizonS."""
    span = max(world.life, CATCH_TICK_S) if world.life <= rules["maxHorizonS"] else rules["horizonS"]
    return math.ceil(span / CATCH_TICK_S - 0.001)


def on_floor(x: float, y: float, rules: dict) -> tuple[float, float]:
    cx, cy = rules["centre"]
    gap = math.hypot(x - cx, y - cy)
    if gap <= rules["floorRadius"]:
        return x, y
    share = rules["floorRadius"] / gap
    return cx + (x - cx) * share, cy + (y - cy) * share


def chase_tick(spike: tuple, kx: float, ky: float, t: float) -> tuple:
    """AnubarakPursuit's ChaseTick: the speed step due by `t`, then straight at the kiter."""
    x, y, speed, step, due = spike
    while step < SPIKE_LAST_STEP and t >= due:
        step += 1
        speed = SPIKE_SPEEDS[step]
        due += SPIKE_STEP_S
    gap = math.hypot(kx - x, ky - y)
    if gap > 0.001:
        share = min(speed * CATCH_TICK_S, gap) / gap
        x, y = x + (kx - x) * share, y + (ky - y) * share
    return x, y, speed, step, due


def spike_age_s(spike: tuple) -> float:
    """Seconds since the spike last restarted at its base speed, read off its speed step."""
    _, _, _, step, due = spike
    if step >= SPIKE_LAST_STEP:
        return SPIKE_STEP_S * SPIKE_LAST_STEP
    return max(0.0, SPIKE_STEP_S * (step + 1) - due)


def planner_watch(world: KiteWorld, rules: dict) -> tuple:
    """`(reach, inside, grace ticks, gather need)`: each patch's fail reach and whether the spike starts in it,
    the ticks to its first patch test, and how near the gather it may come, less when it starts nearer."""
    sx, sy = world.spike[0], world.spike[1]
    reach = [rules["fallingReach"] if patch[4] else rules["patchReach"] for patch in world.patches]
    inside = [math.hypot(sx - patch[0], sy - patch[1]) <= yards for patch, yards in zip(world.patches, reach)]
    step = min(max(world.spike[3], 0), SPIKE_LAST_STEP)
    grace = math.ceil(rules["testS"][step] / CATCH_TICK_S - 0.001)
    need = 0.0
    if world.gather is not None:
        need = min(rules["gatherClearance"],
                   math.hypot(sx - world.gather[0], sy - world.gather[1]) - rules["startSlack"])
    return reach, inside, grace, need


def catch_from_tick(world: KiteWorld) -> int:
    return math.ceil(world.catch_from / CATCH_TICK_S - 0.001)


def planner_walk(world: KiteWorld, rules: dict, track: list, ticks: int, mover) -> None:
    """Walk: extends `track` to ticks + 1 points, each step its length by `mover`, slowed on a patch."""
    slow = [(patch[0], patch[1]) for patch in world.patches if not patch[4]]
    reach = rules["slowReach"]
    while len(track) <= ticks:
        x, y = track[-1]
        length = world.speed * CATCH_TICK_S
        if any(math.hypot(x - px, y - py) < reach for px, py in slow):
            length *= KITESIM_SLOW
        track.append(on_floor(*mover(x, y, length, (len(track) - 1) * CATCH_TICK_S), rules))


def planner_chase(world: KiteWorld, rules: dict, watch: tuple, run: PlannerRun, track: list, first: int,
                  ticks: int, planned: int, turn_tick: int, mover=None, catch_from: int | None = None,
                  caught: int = -1) -> int:
    """Ticks `first` to `ticks` of a run, ChaseOne each: the spike chases track[i], `mover` walking the kiter on
    where `track` ends. Stops at the first patch the spike fails on and, with `catch_from` set, postCatchS past
    the first catch counted from that tick on, or from `caught`, since Impale doesn't hand the spike on.
    Returns that catch's tick, -1 with none."""
    reach, inside, grace, need = watch
    patches = world.patches
    gather = world.gather
    slow = [(patch[0], patch[1]) for patch in patches if not patch[4]]
    slow_reach, cap, clearance = rules["slowReach"], rules["marginCap"], rules["gatherClearance"]
    catch_yd = rules["impaleReach"] + rules["catchMargin"]
    post = to_tick(rules["postCatchS"])
    step_yd = world.speed * CATCH_TICK_S
    spike, margin = run.spikes[-1], run.margin[-1]
    for i in range(first, ticks):
        if caught >= 0 and i >= caught + post:
            break
        kx, ky = track[i]
        spike = chase_tick(spike, kx, ky, i * CATCH_TICK_S)
        tick = i + 1
        if tick >= len(track):
            length = step_yd
            if any(math.hypot(kx - px, ky - py) < slow_reach for px, py in slow):
                length *= KITESIM_SLOW
            track.append(on_floor(*mover(kx, ky, length, i * CATCH_TICK_S), rules))
        run.spikes.append(spike)
        sx, sy = spike[0], spike[1]
        margin = min(margin, cap)
        for k, patch in enumerate(patches):
            gap = math.hypot(sx - patch[0], sy - patch[1])
            if k != planned or tick < turn_tick:
                margin = min(margin, gap - reach[k])
            hit = gap <= reach[k]
            if inside[k] and not hit and run.left[k] < 0:
                run.left[k] = tick
            if hit and run.fail_tick < 0 and (not inside[k] or run.left[k] >= 0 or tick >= grace):
                run.fail_tick, run.fail_patch = tick, k
                run.fail_planned = k == planned and tick >= turn_tick
        if gather is not None:
            gap = math.hypot(sx - gather[0], sy - gather[1])
            margin = min(margin, gap - clearance)
            if run.raid_tick < 0 and gap < need:
                run.raid_tick = tick
        run.margin.append(margin)
        if run.fail_tick >= 0:
            return caught
        if caught < 0 and catch_from is not None and tick >= catch_from:
            kx, ky = track[tick]
            if math.hypot(sx - kx, sy - ky) <= catch_yd:
                caught = tick
    return caught


def uncovered_s(rest: float, fast: bool, patches: int, rules: dict) -> float:
    """Uncovered with no Hands: the spike's life after this kite that the patches left can't cover."""
    if rest <= 0.0:
        return 0.0
    cover = (rules["fastS"] if fast else rules["freshS"]) + max(patches, 0) * (rules["idleS"] + rules["freshS"])
    return max(0.0, rest - cover)


def run_length(spike: tuple, t: float, left_s: float) -> float:
    """RunLength: how far the spike runs in `left_s` from `t` seconds into the plan, its speed stepping up on
    time."""
    _, _, speed, step, due = spike
    length = 0.0
    while left_s > 0.0:
        span = min(max(due - t, 0.0), left_s) if step < SPIKE_LAST_STEP else left_s
        length += speed * span
        left_s -= span
        t += span
        if left_s > 0.0 and step < SPIKE_LAST_STEP:
            step += 1
            speed = SPIKE_SPEEDS[step]
            due += SPIKE_STEP_S
    return length


def handed_on_to_gather(out: PlanOutcome, world: KiteWorld, rules: dict, spike: tuple) -> bool:
    """HandedOnToGather: an immunity or a catch hands the spike on at its speed, mostly to someone at the gather,
    so it passes the raid at once when it gets within the gather's clearance before it despawns."""
    left = world.life - out.end_s
    full = math.hypot(world.gather[0] - spike[0], world.gather[1] - spike[1])
    clearance = rules["gatherClearance"]
    return left > 0.0 and (full <= clearance or run_length(spike, out.end_s, left) >= full - clearance)


def outruns_kiter(spike: tuple, at, world: KiteWorld, rules: dict) -> bool:
    """OutrunsKiter with no boost: the spike runs faster than the kiter's pace at `at`, slowed on a patch."""
    slowed = any(not patch[4] and math.hypot(at[0] - patch[0], at[1] - patch[1]) < rules["slowReach"]
                 for patch in world.patches)
    return spike[2] > world.speed * (KITESIM_SLOW if slowed else 1.0)


def next_leg(out: PlanOutcome, world: KiteWorld, rules: dict, spike: tuple, fail_tick: int, fail_patch: int) -> None:
    """NextLeg: the spike's leg after the kite, toward the gather, on at its speed after an immunity or a catch,
    from the patch it failed on at its base speed after the idle. A tank patch or a usable kite patch within its
    fail reach of that leg is the kite's hazard, at the kite's end."""
    spent = fail_tick >= 0
    t, left = out.end_s, world.life - out.end_s
    if spent:
        t, left = 0.0, world.life - fail_tick * CATCH_TICK_S - rules["idleS"]
        spike = (spike[0], spike[1], SPIKE_SPEEDS[0], 0, SPIKE_STEP_S)
    sx, sy = spike[0], spike[1]
    gx, gy = world.gather
    full = math.hypot(gx - sx, gy - sy)
    if left <= 0.0 or full < 0.01:
        return
    share = min(1.0, run_length(spike, t, left) / full)
    to = (sx + (gx - sx) * share, sy + (gy - sy) * share)
    for k, patch in enumerate(world.patches):
        if ((spent and k == fail_patch) or (not patch[2] and not patch[3])
                or segment_yd((sx, sy), to, patch) > rules["patchReach"]):
            continue
        if patch[2]:
            out.tank = out.end_tick if out.tank < 0 else out.tank
        else:
            out.kite = out.end_tick if out.kite < 0 else out.kite


def planner_finish(world: KiteWorld, rules: dict, ticks: int, end_tick: int, end_point, margin: float,
                   raid_tick: int, spike: tuple, fail_tick: int = -1, fail_patch: int = -1, planned: bool = False,
                   caught: bool = False, catch_tick: int = -1, turn_tick: int = -1,
                   own_used: bool = False) -> PlanOutcome:
    """Finish: a run still going at the horizon with life left counts as caught there. A fail after a catch is
    still a hazard and spends the patch, but the kite ended at the catch. `spike` is the spike at the fail, else at
    the end, whose next leg counts after a fail, an immunity or a catch. An immunity or a catch with no fail
    passes the raid at once when it hands the spike on to the gather."""
    out = PlanOutcome()
    opened = not caught and not own_used and fail_tick < 0 and end_tick >= ticks
    out.caught = caught or (opened and world.life > end_tick * CATCH_TICK_S + CATCH_TICK_S)
    out.end_tick, out.end_point, out.margin = end_tick, end_point, margin
    out.raid = raid_tick if 0 <= raid_tick <= end_tick else -1
    out.tank = out.kite = -1
    spent = fail_tick >= 0
    failed = spent and not caught
    ended = failed and planned
    if spent:
        tank = world.patches[fail_patch][2]
        if tank and not ended:
            out.tank = fail_tick
        elif not tank and (not planned
                           or spike_age_s(world.spike) + fail_tick * CATCH_TICK_S < rules["earlyPatchS"]):
            out.kite = fail_tick
    out.ending = "patch" if ended else "own" if own_used else "none"
    left = world.budget - (1 if spent and world.patches[fail_patch][3] else 0)
    rest = world.life - out.end_s
    if failed:
        rest -= rules["idleS"]
    out.uncovered = uncovered_s(rest, not failed, left, rules)
    out.cost = rules["costPatch"] if ended else 0.0
    if own_used:
        out.cost += rules["costOwn"] if world.life - out.end_s <= rules["ownLateS"] else rules["costOwnMid"]
    if (world.gather is not None and not spent and (own_used or out.caught)
            and handed_on_to_gather(out, world, rules, spike)):
        out.raid = 0
    if world.gather is not None and (spent or own_used or out.caught):
        next_leg(out, world, rules, spike, fail_tick, fail_patch)
    out.end_gather = -1.0
    if world.gather is not None and (own_used or out.caught):
        out.end_gather = math.hypot(end_point[0] - world.gather[0], end_point[1] - world.gather[1])
    out.catch_tick, out.turn_tick = catch_tick, turn_tick
    return out


def fail_counts_after_catch(run: PlannerRun, catch: int, first: int, rules: dict) -> bool:
    """Whether the run's fail still counts after the catch at `catch`, the run's spikes kept from tick `first`. On
    the planned patch only before the spike's second Impale on the kiter, one testS on."""
    if run.fail_tick < 0:
        return False
    late = run.fail_tick - catch
    step = min(max(run.spikes[catch - first][3], 0), SPIKE_LAST_STEP)
    return late <= to_tick(rules["postCatchS"]) and (not run.fail_planned or late < to_tick(rules["testS"][step]))


def planner_score(world: KiteWorld, rules: dict, ticks: int, track: list, run: PlannerRun, catch: int,
                  turn_tick: int = -1, own: bool = False) -> PlanOutcome:
    """Score of a run stopped at its first patch fail, or postCatchS past its first counted catch. `own` casts the
    world's own immunity ownLeadS before the catch, only on a spike faster than the kiter there."""
    if catch >= 0 and own and world.own and outruns_kiter(run.spikes[catch], track[catch], world, rules):
        end = max(catch - to_tick(rules["ownLeadS"]), 0)
        return planner_finish(world, rules, ticks, end, track[end], run.margin[end], run.raid_tick, run.spikes[end],
                              catch_tick=catch, turn_tick=turn_tick, own_used=True)
    if catch >= 0:
        if fail_counts_after_catch(run, catch, 0, rules):
            return planner_finish(world, rules, ticks, catch, track[catch], run.margin[catch], run.raid_tick,
                                  run.spikes[run.fail_tick], run.fail_tick, run.fail_patch, run.fail_planned,
                                  caught=True, catch_tick=catch, turn_tick=turn_tick)
        return planner_finish(world, rules, ticks, catch, track[catch], run.margin[catch], run.raid_tick,
                              run.spikes[catch], caught=True, catch_tick=catch, turn_tick=turn_tick)
    if run.fail_tick >= 0:
        return planner_finish(world, rules, ticks, run.fail_tick, track[run.fail_tick], run.margin[run.fail_tick],
                              run.raid_tick, run.spikes[run.fail_tick], run.fail_tick, run.fail_patch,
                              run.fail_planned, turn_tick=turn_tick)
    last = len(run.spikes) - 1
    return planner_finish(world, rules, ticks, last, track[last], run.margin[last], run.raid_tick, run.spikes[last],
                          turn_tick=turn_tick)


def first_catch(world: KiteWorld, rules: dict, track: list) -> int:
    """0 when the catch already counts at the start, else -1."""
    sx, sy = world.spike[0], world.spike[1]
    reach = rules["impaleReach"] + rules["catchMargin"]
    return 0 if catch_from_tick(world) <= 0 and math.hypot(sx - track[0][0], sy - track[0][1]) <= reach else -1


def planner_compare(a: PlanOutcome, b: PlanOutcome, rules: dict, rows: int = 8) -> int:
    """Compare: negative when `a` ranks first. Rows past `rows` aren't looked at."""
    band = rules["hazardBandS"]
    for x, y in ((a.tank, b.tank), (a.kite, b.kite), (a.raid, b.raid)):
        if (x >= 0) != (y >= 0):
            return 1 if x >= 0 else -1
        if x >= 0:
            bx = math.floor(x * CATCH_TICK_S / band)
            by = math.floor(y * CATCH_TICK_S / band)
            if bx != by:
                return -1 if bx > by else 1
    if a.caught != b.caught:
        return 1 if a.caught else -1

    def uncovered(outcome: PlanOutcome) -> float:
        return -1.0 if outcome.uncovered <= 0.0 else math.floor(outcome.uncovered / rules["uncoveredBandS"])

    if uncovered(a) != uncovered(b):
        return -1 if uncovered(a) < uncovered(b) else 1
    if rows <= 4:
        return 0
    if abs(a.cost - b.cost) > 0.01:
        return -1 if a.cost < b.cost else 1
    if rows <= 5:
        return 0
    end_a, end_b = math.floor(a.end_s / rules["endBandS"]), math.floor(b.end_s / rules["endBandS"])
    if end_a != end_b:
        return -1 if end_a > end_b else 1
    if a.end_gather >= 0.0 and b.end_gather >= 0.0:
        gather_a = math.floor(a.end_gather / rules["gatherBand"])
        gather_b = math.floor(b.end_gather / rules["gatherBand"])
        if gather_a != gather_b:
            return -1 if gather_a > gather_b else 1
    margin_a = math.floor(min(a.margin, rules["marginCap"]) / rules["marginBand"])
    margin_b = math.floor(min(b.margin, rules["marginCap"]) / rules["marginBand"])
    if margin_a != margin_b:
        return -1 if margin_a > margin_b else 1
    return 0


def ring_mover(start, heading_deg: float, radius: float, turn: float, rules: dict):
    """A ring member as a step function: straight on the heading until `radius` off the centre, or the circle
    the kiter stands on when that's further, then round it, turn +1 counter-clockwise."""
    cx, cy = rules["centre"]
    angle = math.radians(heading_deg)
    dx, dy = math.cos(angle), math.sin(angle)
    px, py = start[0] - cx, start[1] - cy
    ring = max(radius, math.hypot(px, py))
    along = px * dx + py * dy
    straight = max(0.0, -along + math.sqrt(max(0.0, along * along - (px * px + py * py) + ring * ring)))
    hit = math.atan2(py + dy * straight, px + dx * straight)
    walked = [0.0]

    def step(x: float, y: float, length: float, t: float) -> tuple[float, float]:
        walked[0] += length
        run = walked[0]
        if run <= straight:
            return start[0] + dx * run, start[1] + dy * run
        bearing = hit + turn * (run - straight) / ring
        return cx + ring * math.cos(bearing), cy + ring * math.sin(bearing)

    return step


def arc_mover(heading, turn_deg: float, straight_s: float, rules: dict):
    """ArcStep: straight on `heading`, a unit vector, turning `turn_deg` a second after `straight_s`; at the rim it
    slides along it the way it turns, or the way nearer its heading."""
    cx, cy = rules["centre"]
    floor_radius = rules["floorRadius"]
    turn = math.radians(turn_deg) * CATCH_TICK_S
    cos_turn, sin_turn = math.cos(turn), math.sin(turn)
    held = list(heading)

    def step(x: float, y: float, length: float, t: float) -> tuple[float, float]:
        hx, hy = held
        if t >= straight_s and turn_deg != 0.0:
            hx, hy = hx * cos_turn - hy * sin_turn, hx * sin_turn + hy * cos_turn
        nx, ny = x + hx * length, y + hy * length
        out = math.hypot(x - cx, y - cy)
        if math.hypot(nx - cx, ny - cy) > floor_radius and out >= 0.01:
            ux, uy = (x - cx) / out, (y - cy) / out
            if turn_deg > 0.0 or (turn_deg == 0.0 and -uy * hx + ux * hy >= uy * hx - ux * hy):
                hx, hy = -uy, ux
            else:
                hx, hy = uy, -ux
            nx, ny = x + hx * length, y + hy * length
        held[0], held[1] = hx, hy
        return nx, ny

    return step


def leg_mover(legs: list):
    """LegStep: through each leg's end in turn, then stands on the last."""
    reached = [0]

    def step(x: float, y: float, length: float, t: float) -> tuple[float, float]:
        while reached[0] < len(legs) and length > 0.0:
            tx, ty = legs[reached[0]]
            gap = math.hypot(tx - x, ty - y)
            if length < gap:
                return x + (tx - x) * length / gap, y + (ty - y) * length / gap
            length -= gap
            x, y = tx, ty
            reached[0] += 1
        return x, y

    return step


def patch_move_legs(move: str, start, patch, rules: dict) -> list | None:
    """PatchMoveLegs: straight to the centre, or past the edge ring on the kiter's left (`edge0`) or right and on
    along the tangent. None for an edge move from inside the edge ring."""
    if move == "in":
        return [(patch[0], patch[1])]
    edge = rules["edgeRadius"]
    gap = math.hypot(start[0] - patch[0], start[1] - patch[1])
    if gap <= edge + 0.01:
        return None
    base = math.atan2(start[1] - patch[1], start[0] - patch[0])
    turn = math.acos(edge / gap) * (1.0 if move == "edge0" else -1.0)
    tangent = (patch[0] + edge * math.cos(base + turn), patch[1] + edge * math.sin(base + turn))
    along = math.hypot(tangent[0] - start[0], tangent[1] - start[1])
    run_on = rules["edgeRunOn"]
    return [tangent, (tangent[0] + (tangent[0] - start[0]) / along * run_on,
                      tangent[1] + (tangent[1] - start[1]) / along * run_on)]


def candidate_mover(candidate: PlanCandidate, start, rules: dict):
    if candidate.shape == "ring":
        return ring_mover(start, candidate.heading, candidate.radius, candidate.turn, rules)
    angle = math.radians(candidate.heading)
    return arc_mover((math.cos(angle), math.sin(angle)), candidate.turn, rules["arcStraightS"], rules)


def base_run(world: KiteWorld, rules: dict, watch: tuple, ticks: int, candidate: PlanCandidate):
    """A base shape walked and chased to its first patch fail or counted catch: `(candidate, run, outcome)`,
    the candidate's track ending there too."""
    candidate.track = [world.kiter]
    run = PlannerRun(world.spike, rules["marginCap"], len(world.patches))
    catch = planner_chase(world, rules, watch, run, candidate.track, 0, ticks, -1, -1,
                          candidate_mover(candidate, world.kiter, rules), catch_from_tick(world),
                          first_catch(world, rules, candidate.track))
    return candidate, run, planner_score(world, rules, ticks, candidate.track, run, catch)


def run_ending(world: KiteWorld, rules: dict, watch: tuple, ticks: int, base, turn: int, patch: int,
               move: str):
    """RunEnding with no Hand: the base shape to tick `turn`, then the move onto the patch, chased from the
    base run's state there. `(candidate, outcome)`, None when it can't be planned."""
    candidate, base_spikes, none = base
    if (turn < 0 or turn >= ticks or (base_spikes.fail_tick >= 0 and turn >= base_spikes.fail_tick)
            or 0 <= none.catch_tick <= turn):
        return None
    legs = patch_move_legs(move, candidate.track[turn], world.patches[patch], rules)
    if legs is None:
        return None
    ending = candidate.like()
    ending.ending, ending.patch, ending.move, ending.turn_tick = "patch", patch, move, turn
    ending.track = candidate.track[:turn + 1]
    run = PlannerRun(base_spikes.spikes[turn], base_spikes.margin[turn], 0)
    run.raid_tick = base_spikes.raid_tick if 0 <= base_spikes.raid_tick <= turn else -1
    run.left = [left if left <= turn else -1 for left in base_spikes.left]
    catch = planner_chase(world, rules, watch, run, ending.track, turn, ticks, patch, turn, leg_mover(legs),
                          catch_from_tick(world))
    end_tick = catch if catch >= 0 else len(ending.track) - 1
    spike = run.spikes[(run.fail_tick if run.fail_tick >= 0 else end_tick) - turn]
    if catch >= 0 and not fail_counts_after_catch(run, catch, turn, rules):
        out = planner_finish(world, rules, ticks, catch, ending.track[catch], run.margin[catch - turn],
                             run.raid_tick, run.spikes[catch - turn], caught=True, catch_tick=catch, turn_tick=turn)
    elif catch >= 0:
        out = planner_finish(world, rules, ticks, catch, ending.track[catch], run.margin[catch - turn],
                             run.raid_tick, spike, run.fail_tick, run.fail_patch, run.fail_planned, caught=True,
                             catch_tick=catch, turn_tick=turn)
    else:
        out = planner_finish(world, rules, ticks, end_tick, ending.track[-1], run.margin[-1], run.raid_tick, spike,
                             run.fail_tick, run.fail_patch, run.fail_planned, turn_tick=turn)
    return ending, out


def patch_endings(world: KiteWorld, rules: dict, watch: tuple, ticks: int, base) -> list:
    """PatchEndings with no Hand: onto each usable patch from each turn second, every move in the last
    edgeWindowS before the base's end and straight in every 2 s before that."""
    _, run, none = base
    fail = run.fail_tick if run.fail_tick >= 0 else ticks
    catch = none.catch_tick if none.catch_tick >= 0 else ticks
    end = min(catch, fail) - 1
    step = max(1, to_tick(rules["turnStepS"]))
    every_move = end - to_tick(rules["edgeWindowS"])
    first = max(0, end - to_tick(rules["patchWindowS"]))
    out = []
    for patch, spot in enumerate(world.patches):
        if not spot[3]:
            continue
        turn = end
        while turn >= first:
            for move in PATCH_MOVES:
                if move == "in" or turn > every_move:
                    ending = run_ending(world, rules, watch, ticks, base, turn, patch, move)
                    if ending:
                        out.append(ending)
            turn -= step if turn > every_move else 2 * step
    return out


def full_track(world: KiteWorld, rules: dict, candidate: PlanCandidate, ticks: int) -> None:
    """The pick's track carried on to the horizon: a base shape walked again from the start, a patch ending
    standing on its patch once its legs run out."""
    if candidate.patch >= 0 and candidate.turn_tick >= 0:
        del candidate.track[candidate.turn_tick + 1:]
        legs = patch_move_legs(candidate.move, candidate.track[-1], world.patches[candidate.patch], rules)
        if legs is not None:
            planner_walk(world, rules, candidate.track, ticks, leg_mover(legs))
        candidate.track += [candidate.track[-1]] * (ticks + 1 - len(candidate.track))
        return
    candidate.track = [world.kiter]
    planner_walk(world, rules, candidate.track, ticks, candidate_mover(candidate, world.kiter, rules))


def nearest_tick(track: list, at, start: int, span: int) -> int:
    last = len(track) - 1
    best = min(max(start, 0), last)
    for index in range(max(start - span, 0), min(last, start + span) + 1):
        if math.hypot(track[index][0] - at[0], track[index][1] - at[1]) < math.hypot(
                track[best][0] - at[0], track[best][1] - at[1]):
            best = index
    return best


def held_candidate(held: PlanCandidate, elapsed_s: float, world: KiteWorld, rules: dict,
                   slack_ticks: int = 15) -> PlanCandidate:
    """HeldCandidate: the held plan from the kiter's spot on. Past its end a patch walk stands on the patch and a
    run goes straight on."""
    keep = held.like("held")
    keep.track = [world.kiter]
    if len(held.track) < 2:
        return keep
    ticks = planner_ticks(world, rules)
    at = nearest_tick(held.track, world.kiter, to_tick(elapsed_s), slack_ticks)
    keep.track += held.track[at + 1:at + 1 + ticks]
    keep.turn_tick = max(held.turn_tick - at, 0) if held.turn_tick >= 0 else -1
    if keep.patch >= 0 and keep.turn_tick >= 0:
        keep.track += [keep.track[-1]] * (ticks + 1 - len(keep.track))
        return keep
    heading = (1.0, 0.0)
    if len(keep.track) >= 2:
        (ax, ay), (bx, by) = keep.track[-2], keep.track[-1]
        gap = math.hypot(bx - ax, by - ay)
        if gap > 0.01:
            heading = ((bx - ax) / gap, (by - ay) / gap)
    planner_walk(world, rules, keep.track, ticks, arc_mover(heading, 0.0, 0.0, rules))
    return keep


def judge(candidate: PlanCandidate, world: KiteWorld, rules: dict) -> PlanOutcome:
    ticks = planner_ticks(world, rules)
    run = PlannerRun(world.spike, rules["marginCap"], len(world.patches))
    catch = planner_chase(world, rules, planner_watch(world, rules), run, candidate.track, 0,
                          min(ticks, len(candidate.track) - 1), candidate.patch, candidate.turn_tick,
                          catch_from=catch_from_tick(world), caught=first_catch(world, rules, candidate.track))
    return planner_score(world, rules, ticks, candidate.track, run, catch, candidate.turn_tick, candidate.own)


def track_heading(track: list) -> tuple[float, float]:
    (ax, ay), (bx, by) = track[0], track[min(10, len(track) - 1)]
    gap = math.hypot(bx - ax, by - ay)
    return ((bx - ax) / gap, (by - ay) / gap) if gap > 0.5 else (0.0, 0.0)


def turn_degrees(a, b) -> float:
    if a == (0.0, 0.0) or b == (0.0, 0.0):
        return 0.0
    return math.degrees(math.acos(min(1.0, max(-1.0, a[0] * b[0] + a[1] * b[1]))))


def plan_kite(world: KiteWorld, rules: dict, held: PlanCandidate | None = None, held_age_s: float = 0.0,
              held_elapsed_s: float = 0.0) -> tuple[PlanCandidate, PlanOutcome, bool]:
    """PlanKite with no Hand or boost: every candidate judged, the best kept, `(candidate, outcome, kept)`. `held` is
    the last re-plan's choice, held `held_age_s` so far, its track started `held_elapsed_s` ago."""
    ticks = planner_ticks(world, rules)
    watch = planner_watch(world, rules)
    bases = []
    headings = int(rules["headings"])
    for index in range(headings):
        degrees = 360.0 * index / headings
        for radius in (rules["ring"], rules["rim"]):
            for turn in (1.0, -1.0):
                bases.append(base_run(world, rules, watch, ticks, PlanCandidate("ring", degrees, radius, turn)))
        for turn in rules["arcTurns"]:
            bases.append(base_run(world, rules, watch, ticks, PlanCandidate("arc", degrees, 0.0, turn)))

    order = sorted(range(len(bases)), key=functools.cmp_to_key(
        lambda a, b: planner_compare(bases[a][2], bases[b][2], rules)))
    ended = set(order[:int(rules["endingShapes"])])
    # the longest runs often end far from every patch, so each patch also gets the shapes that pass it latest
    for spot in world.patches:
        if not spot[3]:
            continue
        near = []
        for index, (candidate, _, none) in enumerate(bases):
            for tick in range(min(none.end_tick, len(candidate.track) - 1), 0, -1):
                point = candidate.track[tick]
                if math.hypot(point[0] - spot[0], point[1] - spot[1]) <= rules["patchNear"]:
                    near.append((tick, index))
                    break
        near.sort(key=lambda item: -item[0])
        ended.update(index for _, index in near[:int(rules["nearShapes"])])

    entries = []
    for candidate, run, none in bases:
        entries.append((none, candidate))
        if world.own and none.catch_tick >= 0:
            own = candidate.like()
            own.own, own.ending, own.track = True, "own", candidate.track
            entries.append((planner_score(world, rules, ticks, candidate.track, run, none.catch_tick, own=True), own))
    for index in sorted(ended):
        entries += [(out, candidate) for candidate, out in patch_endings(world, rules, watch, ticks, bases[index])]
    best = 0
    for index in range(1, len(entries)):
        if planner_compare(entries[index][0], entries[best][0], rules) < 0:
            best = index
    outcome, pick = entries[best]
    full_track(world, rules, pick, ticks)
    if held is None or len(held.track) < 2:
        return pick, outcome, False

    keep = held_candidate(held, held_elapsed_s, world, rules)
    kept = judge(keep, world, rules)
    head = planner_compare(outcome, kept, rules, 5)
    beaten = head < 0 or (head == 0 and outcome.end_s > kept.end_s + rules["keepEndS"])
    waits = (turn_degrees(track_heading(keep.track), track_heading(pick.track)) > rules["turnWaitDegrees"]
             and held_age_s < rules["turnWaitS"])
    if not beaten or waits:
        return keep, kept, True
    return pick, outcome, False


def kitepred_text(outcome: PlanOutcome) -> str:
    """The pick as anub.kitepred writes it: the first present of a tank, kite or raid hazard and an uncovered
    catch at its second, else the ending at its end second."""
    for what, tick in (("tank", outcome.tank), ("kite", outcome.kite), ("raid", outcome.raid)):
        if tick >= 0:
            return f"{what}:{tick * CATCH_TICK_S:.1f}"
    return f"{'catch' if outcome.caught else outcome.ending}:{outcome.end_s:.1f}"


def kitesim_play(world: KiteWorld, rules: dict, replan_s: float = KITESIM_REPLAN_S) -> dict:
    """The kite replayed as the native test's Play does it, re-planned every `replan_s`: the kiter walks the plan
    and the spike chases it until it fails on a patch, catches the kiter, or despawns. `end` is `patch`, `tank`,
    `catch` or `emerge` at `s`, `planned` whether the plan ended on that patch, `raid` the first second the spike
    came nearer the gather than the planner's need, `pred` the first pick as anub.kitepred."""
    per = max(1, to_tick(replan_s))
    patch_reach, impale = rules["patchReach"], rules["impaleReach"]
    need = planner_watch(world, rules)[3]
    held, age, ticks, raid, pred = None, 0.0, 0, None, None
    here = world
    while True:
        candidate, outcome, kept = plan_kite(here, rules, held, age, replan_s)
        if pred is None:
            pred = kitepred_text(outcome)
        age = age + replan_s if kept else replan_s
        held = candidate
        kiter, spike = here.kiter, here.spike
        for index in range(per):
            spike = chase_tick(spike, kiter[0], kiter[1], index * CATCH_TICK_S)
            kiter = candidate.track[min(index + 1, len(candidate.track) - 1)]
            ticks += 1
            seconds = ticks * CATCH_TICK_S
            sx, sy = spike[0], spike[1]
            for number, patch in enumerate(world.patches):
                if math.hypot(sx - patch[0], sy - patch[1]) <= patch_reach:
                    planned = candidate.ending == "patch" and candidate.patch == number
                    return {"end": "tank" if patch[2] else "patch", "s": seconds, "planned": planned,
                            "raid": raid, "pred": pred}
            if (raid is None and world.gather is not None
                    and math.hypot(sx - world.gather[0], sy - world.gather[1]) < need):
                raid = seconds
            if math.hypot(sx - kiter[0], sy - kiter[1]) <= impale:
                return {"end": "catch", "s": seconds, "planned": False, "raid": raid, "pred": pred}
            if seconds >= world.life - 1e-6:
                return {"end": "emerge", "s": seconds, "planned": False, "raid": raid, "pred": pred}
        here = here.moved(kiter, spike, per * CATCH_TICK_S)


def spike_step(speed: float | None, elapsed_s: float | None) -> tuple[int, float]:
    """The spike's speed step and the seconds to its next: the step whose speed is nearest the measured one, else
    the restart clock's; the next step off the clock when the two agree, else a whole step at the base speed and
    half of one at the second."""
    clock_step = 0 if elapsed_s is None else min(SPIKE_LAST_STEP, int(elapsed_s // SPIKE_STEP_S))
    step = clock_step
    if speed:
        step = min(range(len(SPIKE_SPEEDS)), key=lambda index: abs(SPIKE_SPEEDS[index] - speed))
    if elapsed_s is not None and step == clock_step:
        return step, SPIKE_STEP_S * (step + 1) - elapsed_s
    return step, SPIKE_STEP_S if step == 0 else SPIKE_STEP_S / 2.0


def kite_starts(trace: Trace, samples: Samples | None = None, windows: list[dict] | None = None) -> list[dict]:
    """Each bot kiter's mark as the replay starts it: the first snapshot from the mark with the kiter and its spike
    both sampled, the spike's speed step, every patch down then, a tank patch off the latches and a kite patch
    within KITE_PATCH_TANK_YD of one not usable, the gather as formed, and the spike's life left, SPIKE_LIFE_MS
    from the submerge."""
    samples = samples or Samples(trace)
    windows = mark_windows(trace, samples) if windows is None else windows
    rules = planner_rules()
    spikes = guids_of_entry(trace, NPC_SPIKE)
    latches = patch_latches(trace)
    lives = patch_lives(trace, samples)
    holds = holder_spans(trace, "anub.gather", pull_end(trace))
    submerged = submerged_spans(trace)
    marks = mark_spans(trace)
    out = []
    for window in windows:
        guid, start = window["guid"], window["start"]
        under = next(((low, high) for low, high in submerged if low <= start <= high), None)
        if guid in trace.humans or under is None:
            continue
        at = spike = kiter = None
        for when in samples.stamps[bisect.bisect_left(samples.stamps, start):
                                   bisect.bisect_right(samples.stamps, start + NEAR_MS)]:
            spike, kiter = spike_at(samples, spikes, guid, when), samples.recent(guid, when)
            if spike and kiter:
                at = when
                break
        if at is None:
            continue
        restart = spike_restart(marks, submerged, at)
        step, due = spike_step(unit_speed(samples, spike[0], at, at + KITESIM_SPEED_MS),
                               None if restart is None else (at - restart) / 1000.0)
        found = [(samples.row(patch, at), patch_side(latches, patch, at) is not None)
                 for patch, (landed, seen) in lives.items() if landed <= at <= seen]
        tanks = [row[1:3] for row, tank in found if tank]
        patches = [(row[1], row[2], tank, not tank and all(dist2(row[1:3], spot) >= KITE_PATCH_TANK_YD
                                                           for spot in tanks), False) for row, tank in found]
        gap = dist2(spike[1:3], kiter[1:3])
        out.append({"window": window, "at": at, "spike_guid": spike[0], "kiter": (kiter[1], kiter[2]),
                    "spike": (spike[1], spike[2], SPIKE_SPEEDS[step], step, due),
                    "life": (under[0] + SPIKE_LIFE_MS - at) / 1000.0, "patches": patches,
                    "gather": formed_gather(holds, samples, submerged, at, guid),
                    "catch_from": rules["testS"][0] if step == 0 and gap <= IMPALE_REACH else 0.0})
    return out


def real_end(window: dict, spend: dict | None) -> str:
    """How the real kite ended in the replay's words, `+hand` when a Hand of Protection landed on the kiter."""
    if window["outcome"] in ("death", "impale"):
        kind = "catch"
    elif window["ended"] in KITER_OWN.values():
        kind = "own"
    elif spend is not None:
        kind = "tank"
    else:
        kind = window["outcome"]
    return kind + ("+hand" if window["hop"] else "")


def real_raid_s(samples: Samples, start: dict, need: float) -> float | None:
    """Seconds from the replay's start to the real spike's first snapshot nearer the gather than `need`."""
    window = start["window"]
    stamps, rows = samples.track(start["spike_guid"])
    for index in range(bisect.bisect_left(stamps, start["at"]), bisect.bisect_right(stamps, window["stop"])):
        if dist2(rows[index][1:3], start["gather"]) < need:
            return (stamps[index] - start["at"]) / 1000.0
    return None


def kitesim_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per bot kite of a 25H pull, its start, the replay, and the real end, tank pass and raid pass."""
    if trace.header.get("diff") != 3:
        return []
    samples = samples or Samples(trace)
    windows = mark_windows(trace, samples)
    spends = {(row["kiter"], row["mark"]): row for row in spent_patches(trace, samples) if row["kiter"]}
    rules = planner_rules()
    out = []
    for start in kite_starts(trace, samples, windows):
        window = start["window"]
        world = KiteWorld(start["kiter"], start["spike"], start["life"], start["patches"], start["gather"],
                          catch_from=start["catch_from"])
        played = kitesim_play(world, rules)
        lead = (start["at"] - window["start"]) / 1000.0
        need = planner_watch(world, rules)[3]
        raid = real_raid_s(samples, start, need) if start["gather"] is not None else None
        first = min(played["s"], played["raid"]) if played["raid"] is not None else played["s"]
        out.append({**start, "played": played, "lead": lead,
                    "real": real_end(window, spends.get((window["guid"], window["start"]))),
                    "real_s": (window["stop"] - window["start"]) / 1000.0,
                    "real_tank": any(yards < SPIKE_PATCH_REACH for yards in window["tank_near"].values()),
                    "real_raid": None if raid is None else raid + lead,
                    "fresh": start["spike"][3] == 0 and first > KITESIM_CLEAN_S})
    return out


def short_patch_end(kind: str, seconds: float) -> bool:
    return kind.split("+")[0] in ("patch", "tank") and seconds < KITESIM_SHORT_S


def show_kitesim(trace: Trace) -> None:
    print("KITE PLANNER REPLAY")
    if trace.header.get("diff") != 3:
        print("  25H only")
        return
    try:
        planner_rules()
    except LookupError as error:
        print(f"  {error}")
        return
    rows = kitesim_rows(trace)
    if not rows:
        print("  no bot kite with its spike sampled")
        return
    rules = planner_rules()
    print(f"  each bot kite's start replayed through a port of the kite planner, re-planned every"
          f" {KITESIM_REPLAN_S:.1f} s on {CATCH_TICK_S:.1f} s ticks;\n  the kiter runs {BASE_RUN_YD_S:.0f} yd/s"
          " with no Hands, own immunity or speed boost, the patches and the gather fixed as at\n  the start; it ends at"
          f" the first patch within its {rules['patchReach']} yd fail reach, the catch or the spike's despawn; a raid"
          f" pass is\n  the spike nearer the gather than {rules['gatherClearance']:.0f} yd, or than its start less"
          f" {rules['startSlack']} yd; a patch on its next leg toward the gather after a\n  patch end or a catch is the"
          " kite's hazard at its end, and the planner counts a catch whose spike goes on to reach the\n  gather's"
          " clearance as a raid pass at once; pred is the first pick as anub.kitepred:")
    print(f"  {'at':>9} {'kiter':14} {'step':>4} {'life':>5}  {'pred':12} {'replay':22} real")
    for row in rows:
        played = row["played"]
        replay = f"{played['end']} {played['s'] + row['lead']:.1f} s"
        if played["end"] == "patch" and not played["planned"]:
            replay += " unplanned"
        flags = []
        if played["raid"] is not None:
            flags.append(f"replay raid pass +{played['raid'] + row['lead']:.1f} s")
        if row["real_raid"] is not None:
            flags.append(f"real raid pass +{row['real_raid']:.1f} s")
        if row["real_tank"]:
            flags.append("real tank pass")
        print(f"  {clock(row['window']['start']):>9} {trace.name(row['window']['guid'])[:14]:14}"
              f" {row['spike'][3]:4} {row['life']:5.1f}  {played['pred']:12} {replay:22}"
              f" {row['real']} {row['real_s']:.1f} s" + (f"; {', '.join(flags)}" if flags else ""))
    fresh = [row for row in rows if row["fresh"]]
    print(f"  fresh-spike kites, the spike at its base step and over {KITESIM_CLEAN_S:.0f} s to the replay's first"
          f" event: {len(fresh)} of {len(rows)}")
    if not fresh:
        return
    print(f"    patch ends under {KITESIM_SHORT_S:.0f} s: real"
          f" {sum(1 for row in fresh if short_patch_end(row['real'], row['real_s']))}, replay"
          f" {sum(1 for row in fresh if short_patch_end(row['played']['end'], row['played']['s'] + row['lead']))}")
    print(f"    tank patch passes: real {sum(1 for row in fresh if row['real_tank'])}, replay"
          f" {sum(1 for row in fresh if row['played']['end'] == 'tank')}")
    print(f"    raid passes: real {sum(1 for row in fresh if row['real_raid'] is not None)}, replay"
          f" {sum(1 for row in fresh if row['played']['raid'] is not None)}")
    ends = collections.Counter(row["played"]["end"] for row in fresh)
    print("    replay ends: " + ", ".join(f"{end} {n}" for end, n in ends.most_common())
          + f"; median {statistics.median(row['played']['s'] + row['lead'] for row in fresh):.1f} s, real"
            f" {statistics.median(row['real_s'] for row in fresh):.1f} s")


SECTIONS = (
    ("phases", "anub.phase spans, his health at each edge, deaths per phase", show_phases),
    ("pull", "locked out past the Web Door or never landed, stage 9 to the pull, the nearest player to his wait"
     " point and each bot tank at the engage", show_pull),
    ("spike", "every Pursued by Anub'arak window, kite branches, outcome, protection, kite stages, boosts,"
     " a kiter marked in a patch, the tank patch spent and one spent in the run stage, the spike's approach to"
     " each tank patch, the spike against the gather and unlatched patches at each mark's end, the speeds at"
     " each cover, the kiter standing still, a flip-flopping walk, the nearest HoP-ready paladin at 12 yd, Feign"
     " Death dropped at once, Impale on others, the crowd where he went down, each kite's anub.kiteplan"
     " terminals and whether the last was used before the mark ended, its anub.kitepred at the mark against the"
     " real end, its anub.kitepath picks with the seconds held and a reversal within 1 s, the first foreseen"
     " hazard against the first real one, each patch kite's turn with its"
     " anub.kitemove and whether the spike failed on its patch, a patch+hand kite's Hand at the turn and against"
     " the predicted catch with the patch under it, the paladin nearest an anub.bridge turn, the spike within"
     " 14.5 yd of a patch before the turn, Impale on a side tank, each submerge's first tank patch gone, the"
     " escort at the boost and the end and in Permafrost, a Hand with a boost the kiter cast later in its kite,"
     " patch kites turned within 3 s of a boost, the escort waiting by the patch at the Hand or the end, a kiter"
     " dead to anything but the spike with its own immunity unused, kite tools spent off the mark, Demonic Circle"
     " teleports, his distance from the gather, each speed boost with the spike's speed and one on a spike no faster"
     " flagged, a kite's first boost against its pick's +boost@, the standby escort at the claim, the spike's next"
     " leg past a patch after an own immunity or a patch end, the kiter closing 2+ s on a spike no faster than"
     " itself",
     show_spike),
    ("spheres", "spheres over time, patches the spike used, patch supply at each mark, duty shots, killers,"
     " landings, grips from the cast spot, the grip duty to kill and a sphere back up, each far grip's links"
     " against the side's spot, anub.relay rows, anub.spot0 and anub.spot1 changes, each side's first tank"
     " patch, spheres and tank patches spent per submerge against the allowance and when the side's patch came"
     " back, heroic's kite spheres per submerge and the kite patches down as he went under, flagged under two",
     show_spheres),
    ("burrowers", "each burrower: life, submerges, Permafrost, Shadow Strike, dead inside 30 s up, exit, side,"
     " pickup, DPS focus per side, casts off every focus, per wave and side the seconds from coming up to the"
     " first focus pick and the side tank's peak Expose Weakness, in phase 3 the anub.p3wave rows and whether"
     " the side tanks lived until the wave died, on older traces focus due per wave and dropped with"
     " the focus alive, each focus pick by side against its tank and the patches, distance from the patch"
     " centre while on a side tank, where each submerge went under, each opener and the players nearest its"
     " exit, each pulled off its side tank by a non-tank within 15 s of coming up with the puller's casts,"
     " triggered ones marked *, and how long until it was back on a tank and the taunts that did it, each tank"
     " taunt onto a burrower kept or lost by its yards at the cast, Bladestorm"
     " with a burrower within 25 yd, burrower threat cap vetoes per bot, melee bot seconds on a burrower no tank"
     " within 5 yd held by anub.dps value",
     show_burrowers),
    ("dps", "per phase and DPS role: target kind share, moving share, yards walked by move order, damage by"
     " target kind; his health lost per phase 1, flagged under 20 percent; anub.dps rows per role; each anub.dside"
     " group per wave and melee walks across the midline between the side spots", show_dps),
    ("frost", "seconds carrying Permafrost per group and bot, he up and he down, stints over 3 s with the"
     " bot's target", show_frost),
    ("threat", "per surfaced window: his target by role, time to a tank, drag legs, taunts, threat cap, main"
     " tank in a patch, submerge spot to the tank patches, the longest stretch held 10+ yd off the side spots'"
     " midpoint with no drag leg, the drag's pre-steps, anub.subspot rows, when the submerge drag left",
     show_threat),
    ("swarm", "phase 3: Leeching Swarm, lust, per 5 s his health and the raid's, the swarm and the healers'"
     " heals by who took them, bot heals onto healthy non-carriers, DPS on burrowers up at its start and newer"
     " ones, when the burst opened, burst spent from 40%%, Penetrating Cold heals, the seconds each phase 3"
     " carrier went with no HoT or shield, anub.swarmheal rows, anub.healduty and anub.coldheal per healer bot,"
     " per phase 3 Penetrating Cold wave the first ticks survived, the frost holder, Aura Mastery at the first two"
     " ticks, anub.mastery rows, the resisted share per tick, each carrier's health at the landing, risk, first"
     " tick margin and fate, its anub.coldheal healers and the heals and absorbs on it before the tick, and herds"
     " of healers on one carrier while another at risk got nothing, the externals with a caster dead within"
     " 10 s flagged and the tank dips under 35%%, defensives, deaths",
     show_swarm),
    ("healers", "per healer and phase the distance to the main and side tanks, far from a side tank on a burrower,"
     " follow acts, engine passes, anub.hstand, the walk behind the tank patches from anub.hbehind go to standing"
     " still, burrowers reaching a healer in phase 3 past no patch; side tanks and the main tank with no healer in"
     " reach; raid healer engine coverage; per phase healing done on Warcraft Logs' rules with pets and totems"
     " folded into their owner: effective healing plus absorbs, overheal share, HPS and share of the raid's; each"
     " healer's mana at phase 3's start and 30 s in, Divine Plea, Innervate and Hymn of Hope casts per phase",
     show_healers),
    ("tanks", "Expose Weakness per wave per side tank, the main tank's with the burrowers and his area casts"
     " before it, side tank area casts per surfaced phase and anub.saoe rows, each tank defensive cast with its"
     " stack, a tank form or stance lost while he was up with the act row that dropped it, each tank death with"
     " the damage and heals before it",
     show_tanks),
    ("scarabs", "scarabs alive after each emerge and their targets, the main tank's Mandibles stack then, scarab"
     " lives per phase, Mandibles peak stack and damage per player per phase", show_scarabs),
    ("redirects", "each Misdirection and Tricks of the Trade with its caster's target and a burrower's target"
     " after, totals per recipient", show_redirects),
    ("pursuit", "pure pursuit replayed against the real spike per kite, end and patch-pass errors at 3 and 6 s",
     show_pursuit),
    ("kitesim", "25H: the kite planner replayed from each bot kite's start, its first pick's prediction and where the"
     " replay ended against the real kite, and for fresh-spike kites the patch ends under 10 s, tank patch passes and"
     " raid passes; no Hands, own immunity or speed boost in the replay", show_kitesim),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
