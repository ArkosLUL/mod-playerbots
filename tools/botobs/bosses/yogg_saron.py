#!/usr/bin/env python3
"""Score a Yogg-Saron pull from a RaidObs trace: phases, clouds, portals, the brain room, Crush.

    yogg_saron.py <file>            every section
    yogg_saron.py <file> --phases   phase timeline, the Keepers freed, what happened before the pull,
                                    and how far apart the Guardians left at the handover died
    yogg_saron.py <file> --clouds   cloud-orbit exposure per role, and who summoned each Guardian
    yogg_saron.py <file> --fervor   Sara's Fervor runs that came back into the nova, novas on holders,
                                    Guardians only runners summoned
    yogg_saron.py <file> --novas    every Shadow Nova: the warning each health gate gave, who it
                                    would have killed, and which cooldown covered them
    yogg_saron.py <file> --threat   who the Guardians were on, redirects, and taunt aim
    yogg_saron.py <file> --portals  portal waves, assignments and who got down
    yogg_saron.py <file> --tentacles  tentacle stock at each brain room door, stun removal, leftovers at P3
    yogg_saron.py <file> --brain    brain-room occupancy, the Brain's health, skull exposure,
                                    who reflect and gaze killed and the reprisal holds, and where
                                    Induce Madness caught whoever it charmed and what their way out
                                    lost, Nitro Boosts, the walk to the Brain per role, turns on the
                                    spot below, brain spot picks, exits decided outside the brain room,
                                    what bots were doing when a skull's gaze hit them, the room shares,
                                    and per wave who waited at the door, how far out the team stood
                                    when it opened and when it reached the Brain, Nitro, Dash and
                                    Brain charges, and the hold short of 30%: from when, why it let
                                    go, what leaked and how many tentacles were up
    yogg_saron.py <file> --phase3   Heroism against when the raid was in position, which ranged station a
                                    leftover Crusher left them, how fast each role and the brain team
                                    reached its station and the Nitro Boosts spent on it, Immortal
                                    Guardians, who pulled them off the tanks and with what, how much of
                                    the time they were at full power or Weakened, Drain Life channels, how
                                    fast they were cut and the kicks left unused, the bot tank's targets
                                    and taunts, the Guardians on each tank, tank swaps, taunts that went
                                    back to the other tank, Misdirection, the tanks' buttons, Lay on Hands,
                                    swings on the raid during a gaze, beacon heals and how far marked Guardians were
                                    from Yogg, the rest and each other, which other Guardians the heal
                                    reached, each tank's 20 s under it and the raid's damage on the healed
                                    ones, beacon drags and the lane picked, who held each healed Guardian,
                                    what it did to non-tanks and when a tank had it, Deafening Roar
                                    against Aura Mastery, Lunatic Gaze, the damage split by Guardian health
                                    band, area casts by how many units round the target were worth
                                    hitting, healer mana and engine, phase 2 leftovers, the pets on each
                                    and Diminish Power
    yogg_saron.py <file> --crush    Crush, who took it and what each cone was aimed at, the body's
                                    knockback and who walked into it, Death Rays and the last walk
                                    before each
    yogg_saron.py <file> --sanity   Sanity minima, each trip below, who sat a wave out, where it went,
                                    Rebirths on a corpse with no Sanity

The fight has three rooms, five mechanics that kill and nothing in the world to sweep for two of
them, so most of what matters here is in `note` rows rather than in the snapshot:

- **Sara is FACTION_FRIENDLY for all of phase 1**, and the snapshot sweep keeps only units hostile to
  the anchor, so her health bar - which *is* phase 1's progress - is never sampled. `yogg.phase` is
  the only phase timeline a trace carries.
- **The body's knockback has no world object.** 64022 is an infinite self aura on Yogg triggering
  64020 every second, a 14 yd knock back that works out to a 13.26 yd horizontal ring on the floor.
  It reaches the file only as a `haz` circle, which nothing tests a death against.
- **Crush is a cone with no dynamic object either**, and its facing tracks whoever hit the tentacle
  last. `yogg.crush` is the per-bot read and `haz` carries the geometry to check it against.
- **A disguised Influence Tentacle keeps its own health while showing the disguise's entry**, because
  Creature::UpdateEntry preserves current health. A Suit of Armor at 6% is not a damaged Suit of
  Armor, it is a full-health tentacle.
- **Portals are creatures (34072), not gameobjects**, one use each, despawning after 25 s, and only
  RAID_MODE(4, 10) of the ten spots ever spawn. They are not hostile, so the snapshot sweep - which
  keeps only units hostile to the anchor - never samples one. `yogg.wave` is the only record that a
  wave happened at all, and the spot table below is the only record of where they were.

`ObsValue` and `ObsGuidMap` emit on change only, so a key going quiet means "unchanged", never
"never happened". A key missing from the whole trace is a different thing and is reported as such:
that means the probe is not reaching the recorder, not that the mechanic never fired.
"""
from __future__ import annotations

import bisect
import collections
import math
import pathlib
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.geometry import at, first_seen, guids_of_entry  # noqa: E402
from raidobs.geometry import track as tracks  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.space import show_share, threat_share  # noqa: E402
from raidobs.stuck import IDLE_MS, idle_windows, veto_counts  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, death_records, notes, roster_guids  # noqa: E402

NPC_GUARDIAN = 33136
NPC_YOGG_SARON = 33288
NPC_BRAIN = 33890
NPC_INFLUENCE_TENTACLE = 33943
NPC_LAUGHING_SKULL = 33990
NPC_CRUSHER_TENTACLE = 33966

# The six entries Creature::UpdateEntry disguises an Influence Tentacle as, picked by where it spawned
# rather than by which illusion is running. A tentacle wears one from the moment it is summoned and
# only reverts to 33943 once something damages it, so counting 33943 alone counts revealed tentacles,
# never the room.
NPC_TENTACLE_DISGUISES = (33433, 33567, 33716, 33717, 33718, 33719, 33720)

SPELL_SANITY = 63050
SPELL_INSANE = 63120
SPELL_INDUCE_MADNESS = 64059
SPELL_LUNATIC_GAZE_SKULL = 64168
SPELL_GRIM_REPRISAL = 64039
SPELL_CRUSH_CONE = 64147
SPELL_KNOCK_BACK = 64020
SPELL_SHADOW_NOVA_SARA = 65719
SPELL_SQUEEZE = 64126
SPELL_BRAIN_LINK = 63802
SPELL_BRAIN_LINK_DAMAGE = 63803
SPELL_BRAIN_LINK_OK = 63804
# What a bot casts on itself the moment it takes a portal. The portal creature is not hostile so no
# snapshot ever samples one, and a click that failed leaves nothing behind - this is the only proof a
# portal was actually used.
SPELL_ILLUSION_ROOM = 63988
SPELL_NITRO_BOOSTS_USE = 55004
SPELL_DASH = 33357
# Warrior Charge and Intercept, the two the brain charge node casts at the Brain.
SPELL_BRAIN_CHARGES = (11578, 20252)
SPELL_DIMINISH_POWER = 64145

# A step under this between samples is standing still, and a turn past this while standing is a spin:
# the skull turn flipped bots between headings 3.7 rad apart.
WALK_STILL_YD = 0.3
FLIP_TURN = 1.2

# A leftover tentacle last sampled at or under this was killed, not just out of the sweep.
LEFTOVER_DEAD_PCT = 1.0
# A leftover that gained this much in the 20 s after a beacon heal took it. Each mark's heal is 750k over
# 20 s on everything on Yogg's side within 20 yd, and a Corruptor has 745k.
LEFTOVER_BEACON_RISE_PCT = 10.0
SPELL_HAND_OF_PROTECTION = (10278, 5599, 1022)

# A Guardian's death nova is 15 yd (62714 and 65209 both carry radius index 18) and the ranged station
# is 21.5, so a Guardian dying more than 6.5 yd from the middle catches the whole raid rather than the
# melee pile. Measured: novas at 2.2-4.0 yd hit 9-10 players, novas at 8.5-14.8 yd hit 22-24.
NOVA_RADIUS = 15.0
RANGED_STATION = 21.5
P1_LEASH = 6.5

# ULDUAR_YOGG_SARON_P1_CLOUD_FREE_RADIUS: inside this no orbit reaches, so it is the only ground in the
# room a melee bot can stand on without farming a Guardian. The tank is held here and the melee dodge
# aims here, which is why where a dodge lands is read against it.
P1_CLOUD_FREE_RADIUS = 2.8

# ULDUAR_YOGG_SARON_P1_RANGED_BAND_TOLERANCE and _P1_RANGED_STACK_RADIUS, with the spot the back line
# walks to. The band is what keeps a bot off the inner orbit and the blob is what keeps the raid's
# exposure to one bearing, so a dodge that lands in both wakes nothing the back line would not have.
RANGED_BAND_TOLERANCE = 1.0
RANGED_STACK_RADIUS = 5.0
RANGED_SPOT = (1958.78, -25.587)

# ULDUAR_YOGG_SARON_HANDOVER_MELEE_SPOT, the one place melee and the tank wait out the handover, and
# AiPlayerbot.HealDistance, which decides whether standing there is worth a heal at all. No AC_ override
# touches the heal range here, so it is the code default.
HANDOVER_MELEE_SPOT = (1973.315, -5.246)
HEAL_DISTANCE = 38.5

# How long after the ring to keep watching the gap, and how often. One instant hides it: a healer steps
# out to 31-33 yd for its own nova dodge, and that is when the front line loses it.
HANDOVER_WATCH_MS = 15000
HANDOVER_WATCH_STEP_MS = 500

# Divine Sacrifice's cooldown, for asking whether a press spent early was back when a nova killed.
RAID_COOLDOWN_MS = 120000

# How long after a yogg.p1dodge note its own move may still show up. The note is written in the trigger
# and the move in the action that follows it, so they are one bot tick apart at worst.
DODGE_MOVE_MS = 1500

# A cloud dodge this soon after a step-out is a bot dodging the cloud its own step-out woke.
DODGE_FOLLOWUP_MS = 20000

# 65719 is a flat 25,000 against Sara, so what the phase costs is her health divided by it. Never
# assume that number: mod-dungeon-scale scales raid boss health and the result moves with raid size -
# 240,000 at 25 raiders, 237,500 at 24, against the 199,999 of her spawn row. Ten kills, not eight.
# She is FACTION_FRIENDLY and no snapshot ever samples her, but the `unit` row carries her max health.
NPC_SARA = 33134
SARA_NOVA_DAMAGE = 25000

# The clouds. Six of them, one per orbit, circling the middle at a constant 3.0 yd/s; radii are the
# midpoints of a four-pull spread that never drifted. A cloud summons on any player within 8.5 yd -
# SelectNearbyTarget's 6 goes through _IsWithinDist, which adds both combat reaches - and the Guardian
# appears 10 s later, because 63031 is a 10 s aura rather than the instant summon the C++ reads as.
#
# InformCloud, Sara's own 20-18-16-14-12-10 s timer, skips any cloud within 20 yd of her, so the
# innermost orbit can only ever have been triggered by a player standing on it. That is what makes
# attribution possible at all.
NPC_OMINOUS_CLOUD = 33292
CLOUD_ORBITS = (11.6, 21.4, 31.2, 41.0, 50.9, 60.8)
CLOUD_SUMMON_REACH = 8.5
CLOUD_SUMMON_DELAY_MS = 10000
# Stand inside this and the inner orbit is out of summoning reach, so a melee foot past it is what wakes
# a Guardian from the pile.
PILE_LEASH = CLOUD_ORBITS[0] - CLOUD_SUMMON_REACH
# A battle rez lands within a couple of seconds of the death it answers; past that it is a different one.
REZ_WINDOW_MS = 5000
# How long after coming back a second death still belongs to the first. Longer than this and the bot
# lived through what killed it, and the next death is its own story.
REZ_AGAIN_MS = 60000
INFORM_CLOUD_MIN_RANGE = 20.0

# ULDUAR_YOGG_SARON_P1_ROOM_RADIUS: past the outer orbit's reach no cloud touches a bot, and the phase 1
# movers wait until a bot is inside it. A move by one of them that started further out is the gate
# failing, or a build from before it.
P1_ROOM_RADIUS = CLOUD_ORBITS[-1] + CLOUD_SUMMON_REACH
# The gate reads GetDistance2d(x, y), which takes the bot's own 1.5 yd combat reach off, and the move
# lands up to a bot tick after that read: another ~1 yd at run speed. The gated pull's worst was 71.3.
ROOM_GATE_SLACK = 2.5
P1_MOVERS = ("yogg-saron phase 1 station action", "yogg-saron phase 1 spacing action",
             "yogg-saron guardian positioning action", "yogg-saron guardian control action")

# ULDUAR_YOGG_SARON_P1_NOVA_GAP_MS. Two Guardian deaths closer than this land both novas on whoever is
# inside both before the heals catch up. On 2026-09-16 a pair 17 ms apart killed two melee from full,
# one 2.5 s apart killed five at the station, and three in 3.4 s killed two melee in the handover.
NOVA_GAP_MS = 6000

# ULDUAR_YOGG_SARON_P1_NOVA_GAP_HEALTH_PCT and ..._P1_NOVA_CHAIN_RADIUS. A nova also takes 25,000-27,501,
# ~2.7%, off every other Guardian within 15 yd, so one dying beside another that low kills both at once.
NOVA_GAP_HEALTH_PCT = 30.0
NOVA_CHAIN_RADIUS = 18.0

# Sara's Fervor doubles the nova, so its holder leaves on the widest warning anyone gets, a Guardian
# under ULDUAR_YOGG_SARON_FERVOR_NOVA_HEALTH_PCT inside 17 yd. The nova reached players at 16.2, and a
# walk back within 6 s of the aura dropping is still the run's.
SPELL_SARAS_FERVOR = 63138
SPELL_SHADOW_NOVAS = (65209, 62714)
NOVA_TRIGGER_RADIUS = 17.0
# ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS. Past this a bot is clear of the blast, so a bot that was
# out here and took the nova anyway was walked back in rather than never leaving.
NOVA_CLEAR_RADIUS = 20.0
# How far back to look for that step-out. The 20% gate gave 7.2-22.1 s of warning on every leftover
# nova, so anything the bot did earlier than this belongs to a different Guardian.
NOVA_WALK_BACK_MS = 12000
FERVOR_NOVA_HEALTH_PCT = 50.0

# How far ahead of a step-out to look for the nova it was dodging, and what the walk out of the blast
# costs: 15 yd at a bot's run speed. A warning shorter than the walk is a step-out that arrives late.
STEP_OUT_LEAD_MS = 25000
STEP_OUT_WALK_MS = 2500

# Every health gate a step-out or a button hangs off, widest first, so one nova row can be read against
# all of them at once.
NOVA_WARNING_GATES = (FERVOR_NOVA_HEALTH_PCT, 20.0, 10.0)
NOVA_REACH = 16.2
FERVOR_AFTER_MS = 6000

# The two gates the ladder fires on, and what a nova is worth, mirroring
# src/Ai/Raid/Uld/Util/UldEncounter_YoggSaron.h. The damage is the unresisted hit, not the median:
# resistance is rolled per hit and the ones that kill roll none.
NOVA_SOON_HEALTH_PCT = 20.0
NOVA_NOW_HEALTH_PCT = 10.0
NOVA_EXPECTED_DAMAGE = 28600

# A trace carries the aura, not the cast name, so the ladder is read by id. Mirrors the table in
# src/Ai/Raid/RaidTankDefensive.cpp.
NOVA_DEFENSIVES = {
    12975: "last stand", 871: "shield wall", 498: "divine protection", 22812: "barkskin",
    22842: "frenzied regeneration", 61336: "survival instincts", 48707: "anti-magic shell",
    55233: "vampiric blood", 48792: "icebound fortitude", 30823: "shamanistic rage",
    31224: "cloak of shadows",
}
NOVA_EXTERNALS = {33206: "pain suppression", 47788: "guardian spirit", 6940: "hand of sacrifice"}

# Divine Sacrifice is the cast; Divine Guardian is the raid-wide 20% its talent procs, and the one that
# says the reduction actually reached the raid.
NOVA_RAID_AURAS = {64205: "divine sacrifice", 70940: "divine guardian"}
# The cast, as opposed to the pair of auras it puts on the raid. 30% of raid damage redirected to the
# paladin for 10 s, plus 20% off everyone for 6 s where the second rank of the talent is trained.
SPELL_DIVINE_SACRIFICE = 64205
# The other raid-wide cover: +100% to the caster's own resistance aura for 6 s, so it only does
# anything on the paladin holding one.
SPELL_AURA_MASTERY = 31821
NOVA_RAID_COVERS = {SPELL_DIVINE_SACRIFICE: "divine sacrifice", SPELL_AURA_MASTERY: "aura mastery"}
# Shadow Resistance Aura ranks 1-5 and Prayer of Shadow Protection. Both are aura 143
# SPELL_AURA_MOD_RESISTANCE_EXCLUSIVE and max rank of either gives 130, and the core takes the highest
# rather than the sum, so holding both is worth exactly one of them.
SPELL_SHADOW_AURA_RANKS = (19876, 19895, 19896, 27151, 48943)
SPELL_PRAYER_OF_SHADOW_PROTECTION = 48170
# Two novas this close leave no room for a heal between them, which is what killed three melee at once.
NOVA_CHAIN_MS = 6000
FERVOR_MS = 15000
P1_SPACING = "yogg-saron phase 1 spacing action"
# Real summons measured their approach out to 8.71 centre to centre, and a pass is read off every
# snapshot within 0.7 s of the mark rather than the single nearest one.
CLOUD_MARK_REACH = 8.7
CLOUD_MARK_WINDOW_MS = 700
STATION_BAND = 1.5
# ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS: the food guard's radius.
STACK_FOOD_RADIUS = 15.0

# Threat redirects and the single-target taunts, for --threat. Righteous Defense is left out: it is
# aimed at the raid member being hit rather than at a Guardian, so it cannot be ranked against one.
REDIRECT_SPELLS = {34477: "Misdirection", 57934: "Tricks of the Trade"}
TAUNT_SPELLS = {355: "Taunt", 62124: "Hand of Reckoning", 56222: "Dark Command", 2649: "Growl"}

# The same four as bot actions, for the attempts that failed and so never reached a cast row. All of
# them share spell range index 4, which is ULDUAR_YOGG_SARON_P1_TAUNT_RANGE.
TAUNT_ACTIONS = {"taunt", "hand of reckoning", "dark command", "growl"}
TAUNT_RANGE = 30.0

# The tentacle's own melee range on a player, centre to centre: CombatReach 8 on display 28814, plus
# 1.5, plus 4/3, which is 10.83. No leeway, since it never moves. Same number as the module's
# ULDUAR_YOGG_SARON_CRUSHER_REACH_TRIGGER_RADIUS, so the reader and the trigger agree on who is inside.
CRUSHER_MELEE_RANGE = 11.0

# The tentacle faces whoever it is swinging at and turns the moment anybody else hits it, so either
# sample around a Crush can name somebody else. The one facing nearest the bot the cone landed on is
# the swing's: four of one pull's five cone kills needed the sample after, the tank's the one before.
CRUSH_AIM_LAG_MS = 500
# A walk older than this did not put anybody in a cone that tracks a moving victim.
CRUSH_WALK_BLAME_MS = 3000

# 64145 is a channel, so it only exists while the tentacle keeps channelling, and the tentacle only
# re-casts it once its victim is out of melee range. Counting cast starts misses every break that was
# followed by a swing. The aura on the raid is the real measure.
SPELL_DIMINISH_POWER = 64145
CLASS_PALADIN = 2
JUDGEMENT_SPELLS = {20271: "Judgement of Light", 53407: "Judgement of Justice", 53408: "Judgement of Wisdom"}

# What a ranged bot hits a Crusher with once it is standing inside the tentacle's melee range: a
# caster's weapon swing shows up only as its imbue proc, and a hunter inside Auto Shot's dead zone falls
# back to its melee abilities. The trace has no DmgClass, so these are matched by name.
MELEE_CLASS_NAMES = {"Flametongue Attack", "Windfury Attack", "Raptor Strike", "Mongoose Bite", "Wing Clip"}

# How soon after a Judgement the channel has to come off to count as that Judgement breaking it.
JUDGEMENT_BREAK_MS = 300

# 64167 on a Laughing Skull triggers 64168 at 30 yd, and the module's node uses the same number.
LAUGHING_SKULL_RADIUS = 30.0

# TEMPSUMMON_TIMED_DESPAWN on every portal, so the whole window a wave offers is this long.
PORTAL_DESPAWN_MS = 25000

# A creature that stops being sampled below this was almost certainly killed; one that vanishes near
# full health was despawned by the phase. Nothing records a creature death, so this is the line.
TENTACLE_SPENT_PCT = 20.0

# 64022 on Yogg triggers a 14 yd knock back every second, and his model sits 4.5 yd above the
# floor, so what lands on the floor is a 13.26 yd ring. Nothing in the world can be swept for it.
KNOCKBACK_RADIUS = 13.3

# A thrown bot's movement generator reads EFFECT_MOTION_TYPE from the first sample of the flight. Height
# missed most throws: one launched from 324.9 was only at 327.3 a sample later. A switch to it this near
# the body, on the platform, is the body's knock back.
EFFECT_MOTION_TYPE = 16
LAUNCH_RADIUS = 18.0
PLATFORM_Z = (320.0, 335.0)

# How far back a throw looks for the walk that crossed the ring. A refused re-issue of the same walk
# is not written as issued, so the one that counts can be a few throws old: 27 s at one phase 3 opening.
LAUNCH_BLAME_MS = 30000

BODY = (1980.28, -25.5868)

# Brain Link ticks 63803 and -2 Sanity on both ends past this, nothing inside it. Only the owner
# carries 63802; the partner is only visible as the second raider taking 63803 in the same window.
BRAIN_LINK_RANGE = 20.0

# The boss platform floor is z 325-330 and every illusion room is z 236-244, so this separates them.
BRAIN_LEVEL_Z = 300.0

# How far back a death below the platform is read for what killed it. The reflect deaths ran from full
# or half health to dead in one to three seconds, so five covers the run without the room before it.
REPRISAL_DEATH_WINDOW_MS = 5000

# The brain level's floor runs 236 to 244. Anything well above it is in the air, where the Brain's own
# model hangs at 265 and a gap-closer aimed at it leaves the bot with nothing to walk on.
BRAIN_FLOOR_Z = 244.0

# Induce Madness strips the Sanity and teleports in the same effect, and Insane lands a few seconds
# behind it, so an ascent this close in front of a charm is that teleport rather than a portal click.
MADNESS_CHARM_MS = 10000

# The module's ULDUAR_YOGG_SARON_*_MIDDLE: the centroid of each room's Influence Tentacle summon group,
# where the healer is meant to stand while tentacles live.
ROOM_MIDDLES = {
    "stormwind": (1927.1511, 68.507256),
    "icecrown": (1925.6553, -121.59296),
    "chamber": (2104.5667, -25.509348),
}

# The module's ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE and _RADIUS, its "in the brain room" test.
BRAIN_ROOM_MIDDLE = (1980.1971, -27.854689)
BRAIN_ROOM_RADIUS = 50.0
BRAIN_MELEE_SPOT = (1969.0, -25.4)
# Inside the brain room and short of its far wall, so a bot reads as on the Brain once it stops here.
BRAIN_ARRIVAL_RADIUS = 45.0
# Phase 3 starts when the Brain drops under this, and the exit leaves from anywhere then.
BRAIN_PHASE_3_PCT = 30.0

# Induce Madness is cast in the tick AddPortals spawns the wave's portals, so the wave note plus the
# cast time is when anyone still underground loses all 100 Sanity.
INDUCE_MADNESS_CAST_MS = 60000

# Holy Light's range, for "could the healer reach this raider from where it stood".
HEAL_RANGE = 40.0

# Everything phase 2 gives the raid to kill on the platform. With none of them alive the raid has
# nothing to do but top up Sanity.
NPC_CONSTRICTOR_TENTACLE = 33983
NPC_CORRUPTOR_TENTACLE = 33985
PLATFORM_KILLABLE = (NPC_GUARDIAN, NPC_CRUSHER_TENTACLE, NPC_CONSTRICTOR_TENTACLE, NPC_CORRUPTOR_TENTACLE)

# 64169, the Sanity Well's area aura: +20 Sanity every 2 s within 6 yd, and -50% damage done.
SPELL_SANITY_WELL = 64169

# The recorder writes a Judgement's cast row after the channel break it caused, a millisecond or so
# later in the same tick, so the channel has to be read slightly before the cast.
JUDGEMENT_LEAD_MS = 50

# boss_yoggsaron.cpp yoggPortalLoc, in table order. AddPortals spawns RAID_MODE(4, 10) of them, so a
# 10-man pull only ever gets the first four.
PORTAL_SPOTS = [
    (1964.60, -42.71), (1986.94, -46.21), (1989.50, -6.70), (1965.52, -8.09), (2000.84, -25.40),
    (1960.22, -26.14), (1976.30, -47.83), (1997.69, -37.46), (1998.07, -13.36), (1976.99, -3.96),
]

PHASE_NAMES = {0: "idle", 1: "phase 1", 2: "phase 2", 3: "phase 3"}

# Phase 3. The module's ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT, where the tank holds Guardians and the
# healers stand, and ULDUAR_YOGG_SARON_PHASE_3_MELEE_GUARDIAN_RANGE, how close to it melee take one.
NPC_IMMORTAL_GUARDIAN = 33988
PHASE_3_MELEE_SPOT = (1998.5377, -22.90317, 324.8895)
PHASE_3_MELEE_GUARDIAN_RANGE = 20.0
PHASE_3_RANGED_SPOT = (2018.7628, -18.896868, 327.07245)
PHASE_3_STATION_RADIUS = 15.0
# ULDUAR_YOGG_SARON_PHASE_3_RANGED_STANDBY_SPOTS in the module's order, the ranged spot turned -15, +15,
# -30 and +30 degrees round Yogg, and the clearance from a live Crusher that sends the ranged there.
PHASE_3_RANGED_STANDBY_SPOTS = (("-15", (2019.18, -29.08, 327.154)), ("+15", (2015.72, -9.16, 327.155)),
                                ("-30", (2016.95, -39.03, 327.678)), ("+30", (2010.26, -0.55, 327.291)))
PHASE_3_RANGED_CRUSHER_CLEARANCE = 18.0
# The module's Heroism gate in phase 3: this share of the living raid at its station, or standing still
# in reach of a live target. Reach is the bots' spell distance past both combat reaches, a player's 1.5
# and the target's, which only Yogg and the Crusher have, or melee range.
LUST_POSITION_SHARE = 0.8
SPELL_DISTANCE = 28.5
PLAYER_COMBAT_REACH = 1.5
MELEE_REACH = 5.0
COMBAT_REACH = {NPC_YOGG_SARON: 30.0, NPC_CRUSHER_TENTACLE: 8.0}
SPELL_LUSTS = {32182: "Heroism", 2825: "Bloodlust"}
# An Immortal Guardian's 5 s channel, 10 and 25. A kick that lands inside the first 1.5 s keeps most of
# its 166,650 heal off.
SPELL_GUARDIAN_DRAIN_LIFE = (64159, 64160)
DRAIN_CUT_MS = 1500
DRAIN_FULL_MS = 4500
# Each Keeper's passive on the raid, in the boss script's Keeper order. Which ones show up is the only
# record of which were freed.
KEEPER_PASSIVES = {62670: "Freya", 62650: "Hodir", 62671: "Mimiron", 62702: "Thorim"}
SPELL_LUNATIC_GAZE_YOGG = 64163
LUNATIC_GAZE_MS = 4000
SPELL_SHADOW_BEACON = 64465
# 64486, the 25-man heal a marked Guardian casts 10 s after the beacon: 37,500 a second for 20 s on
# every ally within 20 yd of its centre, Yogg included.
SPELL_EMPOWERING_SHADOWS = (64468, 64486)
EMPOWERING_SHADOWS_MS = 20000
EMPOWERING_SHADOWS_RADIUS = 20.0
# How long after the cast a Guardian the heal reached is read again. Three stacked heals took one from
# 0% to 81% in this time.
BEACON_SPLASH_LOOK_MS = 6000
# Cast by a Guardian as it arrives, and by every one again as the raid wipes.
SPELL_SIMPLE_TELEPORT = 64195
GUARDIAN_SPAWN_TELEPORT_MS = 3000
SPELL_WEAKENED = 64162
SPELL_HAND_OF_RECKONING = 62124
TAUNT_COOLDOWN_MS = 8000
# What a tank can turn a Guardian back with: the class taunts, Growl 6795 and Righteous Defense 31789,
# whose cast row on the Guardian sits beside the one on the friendly target.
GUARDIAN_TAUNTS = set(TAUNT_SPELLS) | {6795, 31789}

# The module's phase 3 split: ranged hit a Guardian only above the floor and the rest goes on Yogg, under
# it only during a Lunatic Gaze, and never once Weakened. Melee stay on Yogg but for a gaze or a
# beacon-healed Guardian over the floor on a tank.
GUARDIAN_FLOOR_PCT = 30.0
GUARDIAN_WEAKENED_PCT = 10.0
# Deafening Roar: 2.3 s cast, then 4 s of silence within 50 yd. Aura Mastery with the caster's own
# Concentration Aura up puts 64364, silence immunity, on the raiders carrying that aura.
SPELL_DEAFENING_ROAR = 64189
SPELL_AURA_MASTERY_IMMUNE = 64364
ROAR_CAST_MS = 2300
# Hunter drags, and how long after a beacon heal its Guardian's hits are counted against its victims.
SPELL_DISTRACTING_SHOT = 20736
SPELL_FEIGN_DEATH = 5384
DRAG_WATCH_MS = 20000
# When the module reads a drag taunt as taken or not, by when any taunt has landed, and how stale the
# Guardian's row may be at the check.
BEACON_TAUNT_CHECK_MS = 1500
BEACON_TAUNT_LAND_MS = 1000
TAUNT_ROW_SLACK_MS = 1000
# Where a staged dragger waits, by lane in the module's order (north, south, north-east): the staging spot,
# and the hunter spot a hunter waits at instead.
BEACON_STAGE_SPOTS = ((1990.94, -4.40), (1996.06, -42.75), (2009.52, -6.19))
BEACON_HUNTER_SPOTS = ((1986.0, 7.63), (1994.44, -55.65), (2017.05, 5.26))
BEACON_LANES = ("north", "south", "northeast")
# How long into phase 3 a Nitro Boosts use counts as the walk to the station.
STATION_NITRO_WATCH_MS = 30000

# The module's phase 3 area rule: an area spell needs this many units worth hitting within the area
# trigger's radius of the target, distance minus each unit's own reach. These are the actions it holds,
# by spell name.
PHASE_3_AREA_RADIUS = 8.0
PHASE_3_AREA_MIN_WORTH = 3
PHASE_3_AREA_SPELLS = {"Blizzard", "Flamestrike", "Blast Wave", "Dragon's Breath", "Cone of Cold", "Hurricane",
                       "Typhoon", "Volley", "Mind Sear", "Rain of Fire", "Seed of Corruption", "Fan of Knives",
                       "Swipe (Cat)", "Death and Decay", "Blood Boil", "Cleave", "Fire Nova", "Magma Totem"}
# What a cast at a Guardian in the 3 s before it left its tank is read as: a DoT, or anything hitting an
# area.
FLIP_CAUSE_MS = 3000
FLIP_DOTS = {"Corruption", "Immolate", "Unstable Affliction", "Curse of Agony", "Curse of Doom", "Haunt",
             "Moonfire", "Insect Swarm", "Shadow Word: Pain", "Vampiric Touch", "Devouring Plague",
             "Serpent Sting", "Flame Shock", "Rupture", "Living Bomb"}
FLIP_AREA = PHASE_3_AREA_SPELLS | {"Divine Storm", "Consecration", "Whirlwind", "Thunder Clap", "Blade Flurry",
                                   "Sweeping Strikes", "Multi-Shot", "Chain Lightning", "Starfall",
                                   "Howling Blast", "Pestilence", "Explosive Trap", "Bladestorm", "Holy Wrath",
                                   "Shockwave", "Hellfire", "Arcane Explosion"}

# Phase 2's platform tentacles, and both Crush cones (64147 in 10-man, 65201 in 25-man).
TENTACLE_NAMES = {NPC_CRUSHER_TENTACLE: "Crusher", NPC_CONSTRICTOR_TENTACLE: "Constrictor",
                  NPC_CORRUPTOR_TENTACLE: "Corruptor"}
SPELL_CRUSH_CONES = (64147, 65201)
# How long after a stun lifts a Crush still counts against it: melee get a 4 s lead to leave the reach.
STUN_CRUSH_WATCH_MS = 5000

# A Death Ray's damage, and how long before a hit the walks that took the bot there are read.
SPELL_DEATH_RAY_DAMAGE = (63884, 63883)
DEATH_RAY_LOOK_MS = 3000
DEATH_RAY_DODGERS = ("yogg-saron phase 2 spacing action", "avoid aoe")

# The phase 3 tank swap: how far apart the Guardians on each tank are read after a swap starts, and how
# long after a taunt its Guardian sitting on the other tank counts as bounced back.
TANK_SWAP_LOOKS_MS = (0, 4000, 8000)
TAUNT_BOUNCE_MS = 4000
SPELL_MISDIRECTION = 34477
# A tank's own buttons and the externals it gives or takes, by spell id.
TANK_BUTTONS = {31884: "Avenging Wrath", 498: "Divine Protection", 642: "Divine Shield", 6940: "Hand of Sacrifice",
                48788: "Lay on Hands", 27154: "Lay on Hands", 33206: "Pain Suppression", 47788: "Guardian Spirit"}
SPELL_LAY_ON_HANDS = (633, 2800, 10310, 27154, 48788)
# When a healer's mana is read into phase 3, and what counts as a mana cooldown, by spell name.
HEALER_MANA_LOOKS_MS = (0, 60000, 120000, 180000)
HEALER_MANA_SPELLS = {"Mana Tide Totem", "Hymn of Hope", "Divine Plea", "Innervate", "Shadowfiend",
                      "Divine Illumination", "Restore Mana"}
GAZE_NODE = "yogg-saron lunatic gaze trigger"
# Kicks against Drain Life: (name, cooldown ms, range yd), and the slack on range for both units' reach,
# since the trace has positions, not combat reach.
KICK_SPELLS = {57994: ("Wind Shear", 6000, 25.0), 1766: ("Kick", 10000, 5.0), 6552: ("Pummel", 10000, 5.0),
               47528: ("Mind Freeze", 10000, 5.0), 72: ("Shield Bash", 12000, 5.0),
               2139: ("Counterspell", 24000, 30.0), 19647: ("Spell Lock", 24000, 30.0),
               19244: ("Spell Lock", 24000, 30.0)}
KICK_RANGE_SLACK = 6.0


def sara_kills_needed(trace: Trace) -> int | None:
    """Guardian deaths inside 15 yd the phase costs, from Sara's own max health."""
    for guid in guids_of_entry(trace, NPC_SARA):
        maxhp = trace.maxhp.get(guid)
        if maxhp:
            return math.ceil(maxhp / SARA_NOVA_DAMAGE)
    return None


def phase_spans(trace: Trace) -> list[tuple[int, int, int]]:
    """(phase, start, end) over the whole file, from the change-only yogg.phase stream."""
    end = trace.records[-1].get("t", 0) if trace.records else 0
    return [(int(value or 0), start, stop)
            for value, start, stop in latch_spans(trace, "yogg.phase", end)]


def phase1_end(spans: list[tuple[int, int, int]]) -> int | None:
    """Where phase 1 stopped. Phase 2's start first: the latch is back on 1 once Sara respawns after a
    wipe, and a trace that lost its opening mark has only that tail left to call phase 1."""
    start2 = next((start for phase, start, _ in spans if phase == 2), None)
    if start2 is not None:
        return start2
    return next((stop for phase, _, stop in spans if phase == 1), None)


def back_to_back(deaths: list[tuple[int, float]],
                 window_ms: int = NOVA_GAP_MS) -> list[tuple[tuple[int, float], tuple[int, float]]]:
    """Consecutive Guardian deaths no further apart than the window, each as (t, radius)."""
    ordered = sorted(deaths)
    return [(a, b) for a, b in zip(ordered, ordered[1:]) if b[0] - a[0] <= window_ms]


def nova_gaps(times) -> list[int]:
    """Time between consecutive Guardian deaths, in ms."""
    ordered = sorted(times)
    return [b - a for a, b in zip(ordered, ordered[1:])]


def beacon_splash(casters: list[tuple[float, float]], others: dict[int, tuple[float, float]],
                  radius: float = EMPOWERING_SHADOWS_RADIUS) -> list[int]:
    """The other Guardians a beacon heal reached: within `radius` of any caster at the cast. 64486 is an
    area aura applied once, so whoever was in reach then keeps the 20 s heal wherever it walks after."""
    return sorted(guid for guid, spot in others.items()
                  if any(math.dist(spot, caster) <= radius for caster in casters))


def guardian_spawn(first_seen: int, teleports: list[int]) -> int:
    """When a phase 3 Guardian arrived: a Simple Teleport that came with its first sample, else that
    sample. The cast at the wipe is not the arrival."""
    near = [when for when in teleports if when <= first_seen + GUARDIAN_SPAWN_TELEPORT_MS]
    return min(near) if near else first_seen


def kill_kind(on_it: int, non_tanks: int) -> str:
    """`focus` when at least half the bot non-tanks were on the Guardian, `splash` when one at most was,
    so splash or AoE finished it on nobody's schedule, `split` in between."""
    if non_tanks and on_it * 2 >= non_tanks:
        return "focus"
    return "splash" if on_it <= 1 else "split"


def kill_kinds(trace: Trace, deaths: list[tuple[int, int]]) -> list[str]:
    """kill_kind for each (t, guid) death, read off the last snapshot a second before it: by the death
    tick itself bots have already moved on."""
    snaps = trace.of("snap")
    stamps = [snap["t"] for snap in snaps]
    roster = roster_guids(trace)
    kinds = []
    for when, guid in deaths:
        index = bisect.bisect_right(stamps, when - 1000)
        rows = snaps[index - 1].get("u", []) if index else []
        bots = [row for row in rows
                if row[0] in roster and row[0] not in trace.humans and trace.role(row[0]) != "tank"
                and row[5] > 0 and len(row) > 7]
        kinds.append(kill_kind(sum(1 for row in bots if row[7] == guid), len(bots)))
    return kinds


def interpolate(before: tuple, after: tuple | None, when: int) -> tuple[float, float]:
    """Ground position at `when` between two (t, x, y) samples, or `before` itself with no later one.
    A running bot is up to ~3 yd past its last snapshot."""
    if after is None or after[0] <= before[0]:
        return before[1], before[2]
    share = (when - before[0]) / (after[0] - before[0])
    return before[1] + (after[1] - before[1]) * share, before[2] + (after[2] - before[2]) * share


def lower_neighbour(dying: tuple, others: list[tuple],
                    radius: float = NOVA_CHAIN_RADIUS) -> tuple[int, float, float] | None:
    """(guid, hp, distance) of the lowest other living Guardian under the gap floor within `radius` of
    the dying one, every Guardian as (guid, x, y, hp)."""
    best = None
    for guid, x, y, hp in others:
        if guid == dying[0] or not 0 < hp < NOVA_GAP_HEALTH_PCT:
            continue
        far = math.dist((x, y), dying[1:3])
        if far <= radius and (best is None or hp < best[1]):
            best = (guid, hp, far)
    return best


def home_ground(role: str, radius: float) -> bool:
    """Whether a raider this far from the middle stands where phase 1 puts it: the 21.5 yd station for
    ranged and healers, the leash for melee and tanks."""
    if role in ("ranged", "heal"):
        return abs(radius - RANGED_STATION) <= STATION_BAND
    return radius <= P1_LEASH


def came_back(series: list[tuple], trigger: float = NOVA_TRIGGER_RADIUS,
              reach: float = NOVA_REACH) -> tuple[int | None, float]:
    """One Fervor hold as (t, distance to the nearest Guardian at or under the Fervor gate, or None):
    when the bot first came back inside the nova's reach after getting past the trigger radius, and the
    seconds it spent inside from then on. (None, 0.0) if it never got out or never came back."""
    points = [(t, far) for t, far in series if far is not None]
    out = False
    back_at = None
    inside = 0.0
    for index, (t, far) in enumerate(points):
        if not out:
            out = far >= trigger
            continue
        if far <= reach:
            back_at = t if back_at is None else back_at
            if index + 1 < len(points):
                inside += (points[index + 1][0] - t) / 1000
    return back_at, inside


def missing_probes(trace: Trace) -> list[str]:
    """`yogg.*` keys the source declares and this pull never emitted. Named outright rather than
    resolved off the trace: a pull still filed under the map resolves to no boss, and a check keyed
    on that matches no key at all and reports every one present."""
    return [key for key, _, _ in silent_keys(emitted_keys(trace), "yogg-saron")]


def show_banner(trace: Trace) -> None:
    gone = missing_probes(trace)
    if gone:
        print(f"probes absent from this trace: {', '.join(gone)}")
        print("  one pull is weak evidence: a phase it never reached stays silent too, and batch.py --probes"
              "\n  shows which of these no pull of this boss has ever written")
    else:
        print("all yogg.* probes present")


def show_phases(trace: Trace) -> None:
    print("PHASES")
    spans = phase_spans(trace)
    if not spans:
        print("  no yogg.phase rows - either a pre-probe trace, or nothing ever read the encounter")
        return

    for phase, start, stop in spans:
        print(f"  {PHASE_NAMES.get(phase, phase):8} {clock(start):>9} -> {clock(stop):>9}"
              f"  ({(stop - start) / 1000.0:6.1f} s)")

    keepers = keepers_from_auras(rec.get("sp") for rec in trace.of("aura") if rec.get("sp") in KEEPER_PASSIVES)
    print(f"\n  Keepers freed           : {', '.join(keepers) or 'none seen'}")

    # How late the raid noticed. t=0 is the `pull src=bossstate` row, which the recorder writes when
    # the boss state goes IN_PROGRESS - that is inside Sara's InitFight, so t=0 is the pull itself.
    # First damage is 20-30 s later and is not a pull marker: reading it as one is what hid a build
    # whose encounter read did not open until 24 s in.
    engaged = [rec["t"] for rec in notes(trace, "yogg.engaged") if rec.get("txt") == "engaged"]
    if engaged:
        late = min(engaged)
        note = "" if late < 2000 else "   <- nothing yogg-specific ran until here"
        print()
        print(f"  gate opens              : {clock(late)}, {late / 1000.0:.1f} s after the pull{note}")

    # Phase 1 only. Guardians come back in phase 3, but the station they have to die clear of is a
    # phase 1 thing and the ring geometry below means nothing once it is gone.
    p1_end = phase1_end(spans)

    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    born = {guid: pts[0][0] for guid, pts in tracks(trace, guardians).items()}
    if born:
        early = [t for t in born.values() if t < 0]
        in_p1 = [t for t in born.values() if 0 <= t <= (p1_end or 0)]
        print(f"  Guardians in phase 1    : {len(in_p1)}, {len(early)} of them before the pull")

    # Where a Guardian died decides who its nova hits, and it is the only phase 1 number that moves
    # the back line's damage. Creature deaths are not in the `death` stream, which is roster only, so
    # the nova it casts at Sara on the way out is the record that it died at all.
    novas = [(rec["t"], rec.get("s")) for rec in trace.of("cast")
             if rec.get("sp") == SPELL_SHADOW_NOVA_SARA and rec["t"] <= (p1_end or 0)]
    where = tracks(trace, guardians)
    deaths = []
    dead = []
    for when, guid in novas:
        pts = [p for p in where.get(guid, []) if p[0] <= when]
        if pts:
            deaths.append((when, math.dist((pts[-1][1], pts[-1][2]), BODY)))
            dead.append((when, guid))
    kinds = dict(zip(deaths, kill_kinds(trace, dead)))
    radii = [radius for _, radius in deaths]
    if radii:
        wide = sum(1 for r in radii if r > RANGED_STATION - NOVA_RADIUS)
        print(f"  Guardian deaths         : {len(radii)}, median {statistics.median(radii):.1f} yd "
              f"from the middle")
        print(f"    reaching the {RANGED_STATION:.1f} yd station (over "
              f"{RANGED_STATION - NOVA_RADIUS:.1f} yd out): {wide} of {len(radii)}")

        # The only number that says how close the phase came. A kill outside the nova's reach of Sara
        # does nothing at all for it, so 13 deaths can still be 9 kills.
        counted = sum(1 for r in radii if r <= NOVA_RADIUS)
        needed = sara_kills_needed(trace)
        if needed:
            short = needed - counted
            verdict = "enough" if short <= 0 else f"{short} short"
            print(f"    counting for Sara (inside {NOVA_RADIUS:.0f} yd): {counted} of the {needed}"
                  f" she needed - {verdict}")
        else:
            print(f"    counting for Sara (inside {NOVA_RADIUS:.0f} yd): {counted}"
                  f"  (no Sara unit row, so her health is unknown)")

        tally = collections.Counter(kinds.values())
        print(f"    by who was on them    : focus {tally['focus']}, split {tally['split']}, "
              f"splash {tally['splash']}   (bot non-tanks, a second before)")

        pairs = back_to_back(deaths)
        print(f"    under the {NOVA_GAP_MS / 1000:.0f} s gap: {len(pairs)}")
        for a, b in pairs:
            print(f"      {clock(a[0]):>9} at {a[1]:4.1f} yd {kinds[a]:6}, {clock(b[0]):>9} at {b[1]:4.1f} yd "
                  f"{kinds[b]:6}  ({(b[0] - a[0]) / 1000:.3f} s apart)")

        # A death beside a low Guardian: its nova takes ~2.7% off that one too, so the pair can go in
        # one tick. The bots hold the higher one back; humans, splash and parked Guardians can still
        # line one up.
        chained = chain_deaths(trace, dead, guardians)
        print(f"    another Guardian under {NOVA_GAP_HEALTH_PCT:.0f}% inside {NOVA_CHAIN_RADIUS:.0f} yd: "
              f"{len(chained)}")
        for when, hp, far, after in chained:
            gone = f"it died {after:.3f} s later" if after is not None else "it outlived phase 1"
            print(f"      {clock(when):>9}  {hp:4.1f}% at {far:4.1f} yd, {gone}")

    if p1_end is not None:
        show_room_gate(trace, p1_end)

    show_handover(trace)


def chain_deaths(trace: Trace, dead: list[tuple[int, int]],
                 guardians: set) -> list[tuple[int, float, float, float | None]]:
    """(t, hp, distance, seconds until that one died) for each (t, guid) death with another Guardian under
    the gap floor inside chain range, read off the last snapshot before the death."""
    snaps = trace.of("snap")
    stamps = [snap["t"] for snap in snaps]
    died = {guid: when for when, guid in dead}
    rows = []
    for when, guid in dead:
        index = bisect.bisect_left(stamps, when)
        if not index:
            continue
        here = [(row[0], row[1], row[2], row[5]) for row in snaps[index - 1].get("u", []) if row[0] in guardians]
        dying = next((row for row in here if row[0] == guid), None)
        found = lower_neighbour(dying, here) if dying else None
        if found:
            other, hp, far = found
            after = died.get(other)
            rows.append((when, hp, far, (after - when) / 1000 if after is not None else None))
    return rows


def show_room_gate(trace: Trace, p1_end: int) -> None:
    moves = [rec for rec in trace.of("move")
             if rec.get("by") in P1_MOVERS and 0 <= rec.get("t", -1) <= p1_end]
    where = tracks(trace, {rec.get("g", 0) for rec in moves})
    stamps = {guid: [p[0] for p in pts] for guid, pts in where.items()}

    outside: collections.Counter = collections.Counter()
    for rec in moves:
        guid = rec.get("g", 0)
        index = bisect.bisect_right(stamps.get(guid, []), rec["t"])
        if not index:
            continue
        pts = where[guid]
        spot = interpolate(pts[index - 1], pts[index] if index < len(pts) else None, rec["t"])
        if math.dist(spot, BODY) > P1_ROOM_RADIUS + ROOM_GATE_SLACK:
            outside[rec["by"]] += 1

    counts = ", ".join(f"{mover} {count}" for mover, count in outside.most_common()) or "none"
    print(f"  moves begun outside the room (over {P1_ROOM_RADIUS:.1f} yd, "
          f"{ROOM_GATE_SLACK:.1f} yd slack): {counts}")


def show_handover(trace: Trace) -> None:
    """Sara hits 0 and Yogg is summoned invisible in the same tick; 18 s of transformation dialogue
    later ACTION_YOGG_SARON_APPEAR lights the knockback ring and summons the Brain together. Melee and
    the tank walk to one anchor for the whole window, so the questions are who was still in the ring
    when it appeared and whether the anchor held them somewhere the healers could reach."""
    down = first_seen(trace, guids_of_entry(trace, NPC_YOGG_SARON))
    ring = first_seen(trace, guids_of_entry(trace, NPC_BRAIN))
    if down is None or ring is None or ring <= down:
        return

    print(f"  handover                : {clock(down)} -> {clock(ring)} "
          f"({(ring - down) / 1000.0:.1f} s)")

    roster = roster_guids(trace)
    standing: dict[int, float] = {}
    for snap in trace.of("snap"):
        if abs(snap["t"] - ring) > 400:
            continue
        for row in snap.get("u", []):
            if row[0] in roster and row[5] > 0:
                standing.setdefault(row[0], math.dist((row[1], row[2]), BODY))

    inside: dict[str, list[bool]] = collections.defaultdict(list)
    for guid, far in standing.items():
        inside[trace.role(guid)].append(far <= KNOCKBACK_RADIUS)
    if inside:
        parts = ", ".join(f"{role} {sum(v)} of {len(v)}"
                          for role, v in sorted(inside.items()) if v)
        print(f"    in the {KNOCKBACK_RADIUS} yd ring when it lit: {parts}")

    # Health off the same row as the position, because the dead are out of all of this: a corpse keeps
    # its last spot in the snapshot and then releases to the entrance, which read as a 90 yd front line
    # in the one pull that lost eight.
    where = tracks(trace, roster, ("t", "x", "y", "hp"))
    stamps = {guid: [row[0] for row in rows] for guid, rows in where.items()}

    def spots_at(when: int) -> dict[int, tuple[float, float]]:
        found: dict[int, tuple[float, float]] = {}
        for guid, rows in where.items():
            index = bisect.bisect_right(stamps[guid], when)
            if index and rows[index - 1][3] > 0:
                found[guid] = (rows[index - 1][1], rows[index - 1][2])

        return found

    # The spread is the measure of the scatter, and the centre's bearing says which way it went: the
    # back line is one bearing of the 360 and the anchor is the only one that keeps them in reach.
    ring_spots = spots_at(ring)
    front = {guid: spot for guid, spot in ring_spots.items()
             if trace.role(guid) in ("melee", "tank") and guid not in trace.humans}
    healers = [spot for guid, spot in ring_spots.items() if trace.role(guid) == "heal"]
    if front:
        spread = max(math.dist(a, b) for a in front.values() for b in front.values())
        cx = sum(spot[0] for spot in front.values()) / len(front)
        cy = sum(spot[1] for spot in front.values()) / len(front)
        print(f"    front line at the ring: {spread:.1f} yd across {len(front)} bots, centre "
              f"{math.dist((cx, cy), BODY):.1f} yd out at bearing "
              f"{math.degrees(math.atan2(cy - BODY[1], cx - BODY[0])) % 360:.0f} deg, "
              f"{math.dist((cx, cy), RANGED_SPOT):.1f} yd from the station spot")
        anchored = [math.dist(spot, HANDOVER_MELEE_SPOT) for spot in front.values()]
        out, total = beyond_reach(list(front.values()), healers, HEAL_DISTANCE)
        print(f"      from the anchor: median {statistics.median(anchored):.1f} yd, "
              f"max {max(anchored):.1f}   beyond every healer's {HEAL_DISTANCE:.1f} yd: "
              f"{out} of {total}")

    gaps: dict[int, list[float]] = collections.defaultdict(list)
    for when in range(ring, ring + HANDOVER_WATCH_MS, HANDOVER_WATCH_STEP_MS):
        here = spots_at(when)
        heals = [spot for guid, spot in here.items() if trace.role(guid) == "heal"]
        if not heals:
            continue
        for guid, spot in here.items():
            if trace.role(guid) in ("melee", "tank"):
                gaps[guid].append(min(math.dist(spot, heal) for heal in heals))
    if gaps:
        flat = [gap for one in gaps.values() for gap in one]
        loose = sorted((trace.name(guid) for guid, one in gaps.items()
                        if any(gap > HEAL_DISTANCE for gap in one)))
        print(f"    in the {HANDOVER_WATCH_MS // 1000} s after the ring, out of every healer's reach at "
              f"some point: {len(loose)} of {len(gaps)}")
        print(f"      gap to the nearest healer: median {statistics.median(flat):.1f} yd, "
              f"worst {max(flat):.1f}" + (f"   {', '.join(loose)}" if loose else ""))

    # Whether the walk to the anchor wakes anything. A cloud marks its Guardian 10 s before it arrives,
    # so a walk inside the window could only show up as a Guardian after it, and the anchor sits 0.1 yd
    # off the second orbit. Both came out zero over seven pulls, because phase 1 ends first and no cloud
    # is left near the anchor by then.
    clouds = guids_of_entry(trace, NPC_OMINOUS_CLOUD)
    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    frames = [(snap["t"], {row[0]: (row[1], row[2]) for row in snap.get("u", [])})
              for snap in trace.of("snap")]
    born: dict[int, int] = {}
    for when, spots in frames:
        for guid in spots:
            if guid in guardians and guid not in born:
                born[guid] = when
    woke = sum(1 for one in born.values() if down <= one - CLOUD_SUMMON_DELAY_MS <= ring)
    at_ring = min(frames, key=lambda frame: abs(frame[0] - ring))[1] if frames else {}
    near = sum(1 for guid in clouds if guid in at_ring
               and math.dist(at_ring[guid], HANDOVER_MELEE_SPOT) <= CLOUD_SUMMON_REACH)
    print(f"    Guardians from a cloud marked inside the window: {woke}, clouds within "
          f"{CLOUD_SUMMON_REACH} yd of the anchor when the ring lit: {near}")

    # The bot tank has to stand at the anchor with the melee and still wants to hold whatever is left,
    # and both walks are forced. More than one crossing is the two trading the tick instead of a bot
    # walking somewhere, and a refused move is one that never got the motion master.
    far = RANGED_STATION - RANGED_BAND_TOLERANCE
    for guid in sorted((one for one in roster
                        if trace.role(one) == "tank" and one not in trace.humans), key=trace.name):
        rows = [row for row in tracks(trace, {guid}).get(guid, []) if down <= row[0] <= ring]
        if not rows:
            continue

        refused: collections.Counter = collections.Counter()
        for rec in trace.of("move"):
            if rec.get("g") == guid and down <= rec["t"] <= ring and not rec.get("ok"):
                refused[rec.get("by")] += 1

        out = [math.dist((row[1], row[2]), BODY) for row in rows]
        turns = crossings(out, P1_CLOUD_FREE_RADIUS, far)
        says = ", ".join(f"{name} {count}" for name, count in refused.most_common(2)) or "none"
        print(f"    {trace.name(guid)}: {turns} crossings between the middle and the anchor, "
              f"{out[-1]:.1f} yd out when the ring lit;  refused: {says}")

    held = collections.Counter(rec.get("txt") for rec in notes(trace, "yogg.handover"))
    if held:
        print(f"    yogg.handover         : {dict(held)}")

    # Nothing fetches a parked Guardian here, so a park left the low one to bleed out while the raid
    # burned a fresh one: 7 of 9 pulls parked one to three in this window and killed 0-1 in it.
    focus = collections.Counter(rec.get("txt") for rec in notes(trace, "yogg.p1focus")
                                if down <= rec["t"] <= ring)
    killed = sum(1 for rec in trace.of("cast")
                 if rec.get("sp") == SPELL_SHADOW_NOVA_SARA and down <= rec["t"] < ring)
    print(f"    Guardian deaths in the window: {killed};  yogg.p1focus in it: {dict(focus) or 'none'}")

    # With no Guardian left the raid drops combat before the ring lights, and the non-combat engine
    # takes over: food and drink sit a bot down with no AI for up to 18 s.
    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    deaths = {rec.get("s"): rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_SHADOW_NOVA_SARA}
    born = {guid: pts[0][0] for guid, pts in tracks(trace, guardians).items() if pts}
    alive = [guid for guid, when in born.items() if when <= ring and deaths.get(guid, ring + 1) > ring]
    before = [when for when in deaths.values() if when <= ring]
    if alive:
        # How far out they stood is the risk: they follow the front line, and two dying together on top
        # of it is what killed five melee 10 s after one ring.
        far = sorted(math.dist(spot[:2], BODY) for spot in
                     (at(trace, guid, ring) for guid in alive) if spot)
        where = f"  at {', '.join(f'{one:.1f}' for one in far)} yd out" if far else ""
        print(f"    Guardians alive at the ring: {len(alive)}{where}")
    elif before:
        print(f"    last phase 1 Guardian died {(ring - max(before)) / 1000:.1f} s before the ring")

    # The ones left at the ring die after the barrier, where the phase 1 nova gap used to stop. Two 1-2 s
    # apart left melee no time to be healed between them: 3, 7 and 4 dead in three pulls.
    after = [when for when in deaths.values() if when > ring]
    gaps = nova_gaps(after + [max(before)] if before and after else after)
    if gaps:
        said = ", ".join(f"{gap / 1000.0:.1f}" for gap in gaps)
        print(f"    leftover deaths after the ring: gaps {said} s, under {NOVA_GAP_MS / 1000:.0f} s: "
              f"{sum(1 for gap in gaps if gap < NOVA_GAP_MS)}")
    # Where they detonate, which the leash decides in phase 1 and nothing decides after it. One pull's
    # five all went off 20-26 yd out, inside the arc the raid stands in, and their novas are 9 of its 10
    # deaths.
    roster = roster_guids(trace)
    for guid, when in sorted(deaths.items(), key=lambda one: one[1]):
        if when <= ring:
            continue

        spot = at(trace, guid, when)
        if not spot:
            continue

        caught = [other for other in roster
                  if (seen := at(trace, other, when))
                  and math.dist(seen[:2], spot[:2]) <= NOVA_TRIGGER_RADIUS]
        print(f"      {clock(when):>9}  {math.dist(spot[:2], BODY):4.1f} yd out, {len(caught)} raiders "
              f"inside {NOVA_TRIGGER_RADIUS:.0f} yd")

    left = collections.Counter(rec.get("txt") for rec in notes(trace, "yogg.leftover"))
    if left:
        print(f"    yogg.leftover         : {dict(left)}")

    # The raid may work the next Guardian while a low one waits out its gap, but has to come back once
    # it is free, or that one bleeds out on nobody's schedule. One handover had all ten ranged on a 99%
    # Guardian beside a parked 30% one, and both were low at the ring.
    died = sorted(deaths.values())
    end = max((when for when in died if when > ring), default=ring)
    ranged = {guid for guid in roster if trace.role(guid) == "ranged" and guid not in trace.humans}
    snaps = [snap for snap in trace.of("snap") if down <= snap["t"] <= end]
    past = 0.0
    for snap, following in zip(snaps, snaps[1:]):
        index = bisect.bisect_left(died, snap["t"])
        gap_running = index > 0 and snap["t"] - died[index - 1] < NOVA_GAP_MS
        rows = [(row[0], row[5]) for row in snap.get("u", []) if row[0] in guardians]
        targets = [row[7] if len(row) > 7 else 0 for row in snap.get("u", [])
                   if row[0] in ranged and row[5] > 0]
        if ranged_past_free_low(rows, targets, gap_running) >= 3:
            past += (following["t"] - snap["t"]) / 1000.0
    print(f"    3+ ranged on a higher Guardian while a low one was free to kill: {past:.1f} s")

    # Whose ground the raid is standing on once the leash is gone. In the pull this came from the raid
    # held a 90-210 deg arc at a median 24.2 yd and the leftover Guardians held the same arc at 20.6,
    # about 4 yd inside it, so every nova landed on somebody.
    spans = [(start, stop) for phase, start, stop in phase_spans(trace) if phase == 2]
    if spans:
        start, stop = spans[0]
        theirs, ours = [], []
        for snap in trace.of("snap"):
            if not start <= snap["t"] <= stop:
                continue

            for row in snap.get("u", []):
                if row[0] in roster:
                    ours.append((row[1], row[2]))
                elif row[0] in guardians:
                    theirs.append((row[1], row[2]))

        for what, spots in (("raid", ours), ("Guardians", theirs)):
            if not spots:
                continue

            out = sorted(math.dist(spot, BODY) for spot in spots)
            arc = sector_share(spots, BODY, 30)
            busiest = sorted(arc.items(), key=lambda one: -one[1])[:3]
            said = ", ".join(f"{edge}-{edge + 30} deg {share * 100:.0f}%" for edge, share in busiest)
            print(f"    phase 2 {what:10}: median {statistics.median(out):4.1f} yd out "
                  f"({out[0]:.1f}-{out[-1]:.1f}), mostly {said}")

    ate = []
    for rec in trace.of("act"):
        if rec.get("a") in ("food", "drink") and rec.get("vd") == "OK" and 0 <= rec["t"] <= ring:
            spot = at(trace, rec.get("g"), rec["t"])
            if spot and math.dist(spot[:2], BODY) <= STACK_FOOD_RADIUS:
                ate.append((rec["t"], trace.name(rec.get("g")), rec["a"], math.dist(spot[:2], BODY)))
    print(f"    ate or drank inside {STACK_FOOD_RADIUS:.0f} yd: {len(ate)}")
    for when, name, what, far in ate:
        print(f"      {clock(when):>9}  {name:13} {what:5} {far:4.1f} yd")


def fervor_holds(trace: Trace, until: int) -> dict[int, list[tuple[int, int, bool]]]:
    """guid -> (start, end, ran) for each Sara's Fervor a roster member picked up before `until`, `ran`
    being whether the phase 1 dodge moved it while it held the aura."""
    roster = roster_guids(trace)
    opened: dict[int, int] = {}
    spans: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        guid = rec.get("d")
        if rec.get("sp") != SPELL_SARAS_FERVOR or guid not in roster or rec["t"] > until:
            continue
        if not rec.get("r"):
            opened.setdefault(guid, rec["t"])
        elif guid in opened:
            spans[guid].append((opened.pop(guid), rec["t"]))
    for guid, start in opened.items():
        spans[guid].append((start, start + FERVOR_MS))

    dodges: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("by") == P1_SPACING:
            dodges[rec.get("g")].append(rec["t"])
    return {guid: [(start, end, any(start <= t <= end for t in dodges[guid])) for start, end in rows]
            for guid, rows in spans.items()}


def show_fervor(trace: Trace) -> None:
    print("SARA'S FERVOR")
    p1_end = phase1_end(phase_spans(trace))
    if p1_end is None:
        print("  no phase 1 in this trace")
        return

    holds = fervor_holds(trace, p1_end)
    runs = sorted(((start, guid, end) for guid, rows in holds.items() for start, end, ran in rows if ran))
    print(f"  holders: {sum(len(rows) for rows in holds.values())}, ran from a nova: {len(runs)}")

    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    snaps = trace.of("snap")
    stamps = [snap["t"] for snap in snaps]
    moves: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        moves[rec.get("g")].append(rec)

    # Past the trigger radius the dodge stands down, and the station, the leash or a reach walks the bot
    # straight back. A run that ends up inside the nova's reach again while holding Fervor was undone.
    back = 0
    for start, guid, end in runs:
        series = []
        for snap in snaps[bisect.bisect_left(stamps, start):bisect.bisect_right(stamps, end)]:
            rows = {row[0]: row for row in snap.get("u", [])}
            me = rows.get(guid)
            if not me or me[5] <= 0:
                continue
            lows = [math.dist(me[1:3], row[1:3]) for other, row in rows.items()
                    if other in guardians and row[5] > 0]
            series.append((snap["t"], min(lows) if lows else None))
        back_at, inside = came_back(series)
        head = f"    {trace.name(guid):13} {trace.role(guid):6} {clock(start):>9} -> {clock(end):>9}"
        if back_at is None:
            # Never getting out reads the same as never coming back, and they are opposite outcomes:
            # one pull's warlock was walked to 3.5 yd off the pile and killed there without ever
            # clearing the blast.
            out = max((far for _, far in series if far is not None), default=None)
            print(f"{head}  " + ("no reading" if out is None else
                                 "stayed out" if out >= NOVA_TRIGGER_RADIUS else
                                 f"never left, closest {min(far for _, far in series if far is not None):.1f} yd"))
            continue
        back += 1
        walkers = collections.Counter(rec.get("by") for rec in moves[guid]
                                      if back_at - 1500 <= rec["t"] <= back_at and rec.get("by") != P1_SPACING)
        print(f"{head}  back inside {NOVA_REACH} yd at {clock(back_at)}, {inside:.1f} s inside, "
              f"walked back by {dict(walkers)}")
    print(f"  back inside the nova while holding: {back} of {len(runs)}")

    died: dict[int, list[int]] = collections.defaultdict(list)
    for rec in death_records(trace):
        died[rec.get("g")].append(rec["t"])
    hits = []
    for rec in trace.of("dmg"):
        guid = rec.get("d")
        if rec.get("sp") not in SPELL_SHADOW_NOVAS or not rec.get("a") or guid not in holds:
            continue
        if any(start <= rec["t"] <= end for start, end, _ in holds[guid]):
            hits.append((rec["t"], guid, rec["a"], any(0 <= t - rec["t"] <= 1500 for t in died[guid])))
    print(f"  novas on Fervor holders: {len(hits)}, lethal {sum(1 for hit in hits if hit[3])}")
    for when, guid, amount, lethal in hits:
        print(f"    {clock(when):>9}  {trace.name(guid):13} {amount:6d}{'  died' if lethal else ''}")

    show_runner_spawns(trace, holds, p1_end, guardians)


def show_runner_spawns(trace: Trace, holds: dict, p1_end: int, guardians: set) -> None:
    """Guardians whose cloud only Fervor runners marked: everybody in reach at the mark was mid-run or
    walking back from one, and off its own ground. Orbit 2 over the stacked station is never one of these."""
    roster = roster_guids(trace)
    clouds = guids_of_entry(trace, NPC_OMINOUS_CLOUD)
    snaps = trace.of("snap")
    stamps = [snap["t"] for snap in snaps]

    def running(guid: int, when: int) -> bool:
        return any(ran and start <= when <= end + FERVOR_AFTER_MS for start, end, ran in holds.get(guid, []))

    born: dict[int, tuple[int, list]] = {}
    for snap in snaps:
        for row in snap.get("u", []):
            if row[0] in guardians and row[0] not in born:
                born[row[0]] = (snap["t"], row[1:3])

    summoned = []
    for when, where in sorted(born.values(), key=lambda value: value[0]):
        if not 0 <= when <= p1_end:
            continue
        spawn = {row[0]: row for row in snaps[min(bisect.bisect_left(stamps, when), len(snaps) - 1)].get("u", [])}
        near = sorted((math.dist(spawn[cloud][1:3], where), cloud) for cloud in clouds if cloud in spawn)
        if not near or near[0][0] > 4.0:
            continue
        cloud = near[0][1]

        mark = when - CLOUD_SUMMON_DELAY_MS
        who: dict[int, tuple[float, int, float]] = {}
        for snap in snaps[bisect.bisect_left(stamps, mark - CLOUD_MARK_WINDOW_MS):
                          bisect.bisect_right(stamps, mark + CLOUD_MARK_WINDOW_MS)]:
            rows = {row[0]: row for row in snap.get("u", [])}
            if cloud not in rows:
                continue
            for member in roster:
                row = rows.get(member)
                if not row:
                    continue
                gap = math.dist(row[1:3], rows[cloud][1:3])
                if gap <= CLOUD_MARK_REACH and (member not in who or gap < who[member][0]):
                    who[member] = (gap, snap["t"], math.dist(row[1:3], BODY))

        if who and all(running(member, t) and not home_ground(trace.role(member), far)
                       for member, (_, t, far) in who.items()):
            summoned.append((when, [(trace.name(member), trace.role(member), far) for member, (_, _, far) in who.items()]))

    print(f"  Guardians only runners summoned: {len(summoned)}")
    for when, who in summoned:
        print(f"    {clock(when):>9}  " + ", ".join(f"{name}/{role} {far:.1f} yd out" for name, role, far in who))


def summon_bands(distance: float) -> tuple[int, ...]:
    """Which cloud orbits can summon a Guardian on a bot this far from the middle, 1-indexed.

    A cloud takes anyone within 8.5 yd, so every orbit reaches that far either side of itself. Two
    numbers follow and the whole phase 1 layout rests on them: nothing reaches inside 3.1 yd, which is
    the circle the melee pile stands in, and the only band one orbit alone reaches is 20.4-22.6 yd,
    whose midpoint is the 21.5 yd station. Everywhere between 12.9 and 20.1 two orbits reach at once,
    and that is where a nova step-out lands by default.
    """
    return tuple(index for index, ring in enumerate(CLOUD_ORBITS, 1)
                 if abs(distance - ring) < CLOUD_SUMMON_REACH)


def dodge_followups(rows: list[tuple[int, str]], window_ms: int) -> tuple[int, int]:
    """Cloud dodges, and how many followed a nova or fervor one inside the window, over (t, reason).

    A bot that steps out of a blast and then has to dodge a cloud woke that cloud by stepping out,
    because the step-out lands it where an orbit reaches and the pile it left does not.
    """
    total = after = 0
    stepped: int | None = None
    for when, reason in sorted(rows):
        if reason in ("nova", "fervor"):
            stepped = when
        elif reason == "cloud":
            total += 1
            if stepped is not None and when - stepped <= window_ms:
                after += 1

    return total, after


def beyond_reach(spots: list[tuple[float, float]], healers: list[tuple[float, float]],
                 reach: float) -> tuple[int, int]:
    """How many of `spots` had no healer within `reach`, and how many spots there were. With no healer
    standing anywhere, nothing is reachable: a raid with its healers dead is not a raid in range."""
    out = sum(1 for spot in spots
              if not any(math.dist(spot, healer) <= reach for healer in healers))

    return out, len(spots)


def nearest_over_track(spots, other, clear_yd: float) -> tuple[float | None, bool]:
    """The nearest this bot got to a point over the samples given, and whether it was ever further out
    than `clear_yd`. A bot that stepped out of a blast and was walked back in answers both."""
    gaps = [math.dist(spot, other) for spot in spots if spot]
    if not gaps:
        return None, False

    return min(gaps), max(gaps) > clear_yd


def sector_share(spots, origin, width: int) -> dict[int, float]:
    """What share of the samples fell in each bearing sector around `origin`, keyed by the sector's
    lower edge in degrees. Bearings wrap, so due east is sector 0 and never 360."""
    if width <= 0:
        return {}

    counted: dict[int, int] = {}
    for spot in spots:
        if not spot:
            continue

        bearing = math.degrees(math.atan2(spot[1] - origin[1], spot[0] - origin[0])) % 360
        edge = int(bearing // width) * width
        counted[edge] = counted.get(edge, 0) + 1

    total = sum(counted.values())

    return {edge: count / total for edge, count in sorted(counted.items())} if total else {}


def ranged_past_free_low(rows, targets, gap_running: bool) -> int:
    """How many of `targets` sit on a Guardian with more health than the lowest one under the gap floor,
    while that one is free to kill: no gap running, and the lowest never has a lower one beside it.
    Rows are (guid, hp) for the Guardians in one snapshot; a target of 0 is nobody."""
    if gap_running:
        return 0

    living = {guid: hp for guid, hp in rows if hp > 0}
    low = [(hp, guid) for guid, hp in living.items() if hp < NOVA_GAP_HEALTH_PCT]
    if not low:
        return 0

    floor, lowest = min(low)

    return sum(1 for target in targets if target != lowest and living.get(target, 0) > floor)


def moving_at(spot, before, still_yd: float) -> bool:
    """Whether the bot had moved more than `still_yd` between the two samples. A missing sample counts
    as standing, because not knowing is not evidence of a walk."""
    if not spot or not before:
        return False

    return math.dist(spot, before) > still_yd


def rez_pairs(deaths: list[tuple[int, int]], rezzes: list[int],
              window_ms: int) -> list[tuple[int, int, int | None]]:
    """Each death with the first resurrection inside `window_ms` after it, or None when nobody came.
    Matched on time because a battle rez cast row never names its target."""
    ordered = sorted(rezzes)

    return [(when, guid, next((one for one in ordered if when <= one <= when + window_ms), None))
            for when, guid in sorted(deaths)]


def resisted_share(landed: int, resisted: int) -> float:
    """The share of one hit that resistance took off, 0.0 when the hit landed nothing at all."""
    total = landed + resisted
    return resisted / total if total > 0 else 0.0


def chain_pairs(stamps: list[int], window_ms: int) -> list[tuple[int, int]]:
    """Consecutive stamps no further apart than `window_ms`. Three in a row is two pairs, because the
    middle one is both the end of one chain and the start of the next."""
    ordered = sorted(stamps)
    return [(first, second) for first, second in zip(ordered, ordered[1:])
            if second - first <= window_ms]


def cooldown_gap(presses: list[int], cooldown_ms: int, when: int) -> int | None:
    """How long after `when` the earliest of `presses` comes back off cooldown, or None if one was
    already available then. Nothing pressed is also None: the button was in hand."""
    ready = [press + cooldown_ms for press in presses]
    if not ready or any(back <= when for back in ready):
        return None

    return min(ready) - when


def dodge_zone(distance: float) -> str:
    """Which of phase 1's three bands a spot this far from the middle sits in.

    "an orbit reaches" cannot answer where a dodge should have gone, because at the station band one
    always does by design. These three can: inside the cloud-free circle nothing reaches, out to the
    inner orbit's own reach the one orbit no timer can pick sweeps the spot, and past that the bot is on
    the ring the back line already wakes.
    """
    if distance <= P1_CLOUD_FREE_RADIUS:
        return "core"

    return "inner orbit" if distance <= CLOUD_ORBITS[0] + CLOUD_SUMMON_REACH else "beyond"


def crossings(distances: list[float], near: float, far: float) -> int:
    """How many times a track went from inside `near` to beyond `far`.

    Walking somewhere crosses once. More than that is two movers trading the tick, each clearing the
    other's hold, so neither destination is ever reached and the bot ends the window mid-stride.
    """
    count = 0
    inside = False
    for distance in distances:
        if distance <= near:
            inside = True
        elif distance >= far:
            count += inside
            inside = False

    return count


def show_clouds(trace: Trace) -> None:
    print("CLOUD ORBITS")
    rows = notes(trace, "yogg.cloudreach")
    if not rows:
        print("  no yogg.cloudreach rows")
        return

    # The stream is change-only, so a bot's reading holds until its next row. Weighting by the span
    # it held for is the only honest way to turn it into a share of the phase.
    held: dict[int, tuple[int, str]] = {}
    time_in: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    end = trace.records[-1].get("t", 0) if trace.records else 0
    for rec in sorted(rows, key=lambda r: r["t"]):
        guid, now, value = rec.get("g", 0), rec["t"], str(rec.get("txt", ""))
        if guid in held:
            was_t, was = held[guid]
            time_in[trace.role(guid)][was] += now - was_t
        held[guid] = (now, value)
    for guid, (was_t, was) in held.items():
        time_in[trace.role(guid)][was] += max(0, end - was_t)

    for role in sorted(time_in):
        total = sum(time_in[role].values()) or 1
        parts = sorted(time_in[role].items(), key=lambda kv: -kv[1])[:4]
        shares = "  ".join(f"{value}: {span * 100.0 / total:4.1f}%" for value, span in parts)
        print(f"  {role:7} {shares}")

    dodges = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.p1dodge"))
    if dodges:
        print(f"\n  phase 1 dodge reasons : {dict(dodges)}")
    station = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.p1station"))
    if station:
        print(f"  station re-issues     : {dict(station)}")
    taunts = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.p1taunt"))
    if taunts:
        print(f"  phase 1 taunts        : {dict(taunts)}  (counts = still inside 15 yd of Sara)")

    show_dodge_destinations(trace)
    show_stepout_leads(trace)
    show_cloud_spawns(trace)


def show_dodge_destinations(trace: Trace) -> None:
    """Where a phase 1 dodge sends a bot, and how many cloud orbits can reach it there.

    Shadow Nova is cleared at 20 yd and the Guardians stand on the pile, so the nearest safe spot sits
    just inside the inner orbit's outer edge, where two orbits reach at once: the step-out that saves a
    bot from one blast is what wakes the Guardian for the next. The back line never has the problem,
    because it starts at the station and moves outward from there.
    """
    dodges: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, "yogg.p1dodge"):
        dodges[rec.get("g", 0)].append((rec["t"], str(rec.get("txt", ""))))
    if not dodges:
        return

    issued: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("ok") and rec.get("by") == "yogg-saron phase 1 spacing action":
            issued[rec.get("g", 0)].append(rec)

    # The note names the reason, the move that follows carries the destination, so the two have to be
    # paired to say anything about where a given reason sends a bot.
    landed: dict[tuple[str, str], list[tuple[float, float]]] = collections.defaultdict(list)
    for guid, items in dodges.items():
        for when, reason in items:
            move = next((rec for rec in issued.get(guid, []) if when <= rec["t"] <= when + DODGE_MOVE_MS),
                        None)
            if not move:
                continue

            spot = (move.get("x", 0.0), move.get("y", 0.0))
            landed[(trace.role(guid), reason)].append(
                (math.dist(spot, BODY), math.dist(spot, RANGED_SPOT)))

    if landed:
        print()
        print(f"  where a dodge lands, from the middle   ({RANGED_STATION:.1f} yd is the station)")
        print("    role    why      n  median    max   an orbit reaches  two do  in the stack")
        for (role, reason), spots in sorted(landed.items()):
            out = sorted(distance for distance, _ in spots)
            count = len(spots)
            reaches = sum(1 for distance, _ in spots if summon_bands(distance))
            both = sum(1 for distance, _ in spots if len(summon_bands(distance)) > 1)
            stack = sum(1 for distance, from_spot in spots
                        if from_spot <= RANGED_STACK_RADIUS
                        and abs(distance - RANGED_STATION) <= RANGED_BAND_TOLERANCE)
            print(f"    {role:7} {reason:7} {count:3} {out[count // 2]:6.1f} {out[-1]:6.1f}"
                  f"   {reaches * 100 // count:14}% {both * 100 // count:6}% {stack * 100 // count:12}%")

    # What can summon where the melee dodge put them, which is the question the columns above cannot
    # answer: at the station band an orbit always reaches, on purpose.
    zones = collections.Counter(dodge_zone(distance)
                                for distance, _ in landed.get(("melee", "cloud"), []))
    if zones:
        parts = "  ".join(f"{zone} {zones[zone]}" for zone in ("core", "inner orbit", "beyond"))
        print(f"    of {sum(zones.values())} melee cloud dodges, by what reaches them there:  {parts}")

    loops = []
    for guid, items in dodges.items():
        if trace.role(guid) not in ("melee", "tank"):
            continue

        total, after = dodge_followups(items, DODGE_FOLLOWUP_MS)
        steps = sum(1 for _, reason in items if reason in ("nova", "fervor"))
        if total or steps:
            loops.append((after, steps, total, trace.name(guid)))

    if loops:
        print()
        print(f"  melee cloud dodges inside {DODGE_FOLLOWUP_MS // 1000} s of the bot's own step-out")
        for after, steps, total, name in sorted(loops, reverse=True):
            print(f"    {name:14} stepped out {steps:3}, dodged a cloud {total:3},"
                  f" of those after a step-out {after:3}")


def show_stepout_leads(trace: Trace) -> None:
    """How long before the nova a melee step-out leaves, by what opened it.

    Health cannot answer it. The Guardian that opened the window is often dead by the next snapshot and
    the next lowest one reads as the cause, which is how a step-out comes out "opened on a Guardian at
    100%". The nova a Guardian casts at Sara on the way out is its death, so the lead is the gap to the
    first one from a Guardian that was inside the trigger radius when the bot decided to leave.
    """
    p1_end = phase1_end(phase_spans(trace))
    if p1_end is None:
        return

    novas = sorted((rec["t"], rec.get("s")) for rec in trace.of("cast")
                   if rec.get("sp") == SPELL_SHADOW_NOVA_SARA)
    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    issued: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("ok") and rec.get("by") == "yogg-saron phase 1 spacing action":
            issued[rec.get("g", 0)].append(rec["t"])

    # Who each nova actually hit, so a step-out can be scored on whether it got the bot out rather
    # than on how early it left.
    struck: dict[int, set[int]] = collections.defaultdict(set)
    for rec in trace.of("dmg"):
        if rec.get("sp") in SPELL_SHADOW_NOVAS:
            struck[rec.get("s")].add(rec.get("d"))

    leads: dict[str, list[float]] = collections.defaultdict(list)
    dodged_nothing: collections.Counter = collections.Counter()
    caught: collections.Counter = collections.Counter()
    for rec in notes(trace, "yogg.p1dodge"):
        reason = str(rec.get("txt", ""))
        when = rec["t"]
        guid = rec.get("g", 0)
        if reason not in ("nova", "fervor") or when > p1_end or trace.role(guid) != "melee":
            continue
        if not any(when <= moved <= when + DODGE_MOVE_MS for moved in issued.get(guid, [])):
            continue

        here = at(trace, guid, when)
        if not here:
            continue

        lead = None
        for fired, caster in novas:
            if not when <= fired <= when + STEP_OUT_LEAD_MS or caster not in guardians:
                continue
            spot = at(trace, caster, when)
            if spot and math.dist(spot[:2], here[:2]) <= NOVA_TRIGGER_RADIUS:
                lead = (fired - when) / 1000.0
                caught[reason] += guid in struck.get(caster, set())
                break

        if lead is None:
            dodged_nothing[reason] += 1
        else:
            leads[reason].append(lead)

    if not leads and not dodged_nothing:
        return

    print()
    print(f"  how long before the nova a melee step-out left  (the walk out takes "
          f"{STEP_OUT_WALK_MS / 1000:.1f} s)")
    for reason in ("nova", "fervor"):
        got = sorted(leads.get(reason, []))
        missed = dodged_nothing[reason]
        if not got and not missed:
            continue
        says = (f"median {statistics.median(got):4.1f} s, max {got[-1]:4.1f}" if got
                else "no nova to measure against")
        print(f"    {reason:7} {len(got) + missed:3}  {says}"
              f"   over 5 s early {sum(1 for one in got if one > 5):2}"
              f"   dodged nothing inside {STEP_OUT_LEAD_MS // 1000} s {missed:2}"
              f"   took it anyway {caught[reason]:2} of {len(got)}")


def show_cloud_spawns(trace: Trace) -> None:
    """Which Guardians the raid summoned by standing on a cloud, and which Sara's timer sent.

    Every Guardian appears at the cloud that summoned it, so the cloud is read off the spawn position
    rather than guessed. Who marked it is read 10 s earlier, because 63031 is a 10 s aura and the
    Guardian arrives at the end of it - attributing one without rewinding blames whoever happens to be
    standing there when it lands.
    """
    frames = [(snap["t"], {row[0]: (row[1], row[2]) for row in snap.get("u", [])})
              for snap in trace.of("snap")]
    if not frames:
        print("\n  no snapshots, so nothing to attribute")
        return

    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    clouds = guids_of_entry(trace, NPC_OMINOUS_CLOUD)
    roster = roster_guids(trace)
    if not clouds:
        print("\n  no Ominous Cloud was ever sampled, so spawns cannot be attributed")
        return

    # What had moved a foot onto the cloud, which is the difference between a bot that walked into one
    # and a bot a dodge put there.
    dodges: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, "yogg.p1dodge"):
        dodges[rec.get("g", 0)].append((rec["t"], str(rec.get("txt", ""))))

    def moved_by(guid: int, when: int) -> tuple[str, float] | None:
        past = [(t, why) for t, why in dodges.get(guid, []) if when - DODGE_FOLLOWUP_MS <= t <= when]
        if not past:
            return None
        t, why = past[-1]

        return why, (when - t) / 1000.0

    def orbit_of(radius: float) -> int:
        for index, ring in enumerate(CLOUD_ORBITS, 1):
            if abs(radius - ring) < 1.5:
                return index
        return 0

    def frame_at(when: int):
        return min(frames, key=lambda frame: abs(frame[0] - when))

    cloud_orbit = {}
    for guid in clouds:
        for _, positions in frames:
            if guid in positions:
                cloud_orbit[guid] = orbit_of(math.dist(positions[guid], BODY))
                break

    born: dict[int, tuple[int, tuple[float, float]]] = {}
    for when, positions in frames:
        for guid in positions:
            if guid in guardians and guid not in born:
                born[guid] = (when, positions[guid])

    print("\nCLOUD SPAWNS")
    per_orbit = collections.Counter()
    contact = collections.Counter()
    detail = []
    for guid, (when, where) in sorted(born.items(), key=lambda kv: kv[1][0]):
        _, positions = frame_at(when)
        near = sorted((math.dist(positions[cloud], where), cloud)
                      for cloud in clouds if cloud in positions)
        if not near or near[0][0] > 4.0:
            detail.append((when, 0, []))
            continue

        gap, cloud = near[0]
        orbit = cloud_orbit.get(cloud, 0)
        per_orbit[orbit] += 1

        mark = when - CLOUD_SUMMON_DELAY_MS
        _, marked = frame_at(mark)
        _, before = frame_at(mark - 1000)
        who = []
        if cloud in marked:
            who = sorted((round(math.dist(marked[guid2], marked[cloud]), 1), trace.name(guid2),
                          trace.role(guid2), moved_by(guid2, mark),
                          (moving_at(marked.get(guid2), before.get(guid2), WALK_STILL_YD),
                           round(math.dist(marked[guid2], BODY), 1)))
                         for guid2 in roster
                         if guid2 in marked
                         and math.dist(marked[guid2], marked[cloud]) <= CLOUD_SUMMON_REACH)
        if who:
            contact[orbit] += 1
        detail.append((when, orbit, who))

    for when, orbit, who in detail:
        names = ", ".join(f"{name}/{role} {gap}{'w' if step else ''}"
                          + (f" <-{sent[0]} {sent[1]:.0f}s" if sent else "")
                          for gap, name, role, sent, (step, _) in who) or "-"
        ring = f"orbit{orbit}" if orbit else "no cloud"
        print(f"  {clock(when):>9}  {ring:<9}  {names}")

    # The innermost orbit is the load-bearing one: InformCloud skips every cloud within 20 yd of Sara,
    # so Sara's timer can never pick it and every Guardian off it was summoned by a player.
    inner = [index for index, ring in enumerate(CLOUD_ORBITS, 1) if ring < INFORM_CLOUD_MIN_RANGE]
    ours = sum(count for orbit, count in per_orbit.items() if orbit in inner)
    ours += sum(count for orbit, count in contact.items() if orbit not in inner)
    total = sum(per_orbit.values())
    print(f"\n  by orbit: {dict(sorted(per_orbit.items()))}   of {len(born)} Guardians")
    print(f"  the raid's own feet: {ours} of {total}"
          f"   (every orbit {'/'.join(str(i) for i in inner)} spawn, plus any other with a raider"
          f" inside {CLOUD_SUMMON_REACH} yd at the mark)")
    print(f"  Sara's timer: {total - ours}"
          f"   (20-18-16-14-12-10 s, and it can only pick a cloud over"
          f" {INFORM_CLOUD_MIN_RANGE:.0f} yd out)")

    # Melee separately from the back line: the back line's blob is one bearing of the 360 and it pays
    # for the station on purpose, while a melee foot on a cloud is a dodge that went the wrong way.
    feet = [who for _, _, who in detail if any(role == "melee" for _, _, role, _, _ in who)]
    sent = sum(1 for who in feet if any(role == "melee" and moved for _, _, role, moved, _ in who))
    walking = sum(1 for who in feet
                  if any(role == "melee" and step for _, _, role, _, (step, _) in who))
    out = [radial for who in feet for _, _, role, _, (_, radial) in who if role == "melee"]
    if feet:
        print(f"  melee feet: {len(feet)} of {total}, of those with a dodge behind them: {sent}, "
              f"with a melee walking at the mark: {walking}   (w marks a walker above)")
        past = sum(1 for one in out if one > PILE_LEASH)
        print(f"    those melee stood a median {statistics.median(out):.1f} yd out, {past} of {len(out)}"
              f" past the {PILE_LEASH:.1f} yd the inner orbit cannot reach")


def show_portals(trace: Trace) -> None:
    print("PORTAL WAVES")
    waves = [(rec["t"], int(rec.get("txt", 0))) for rec in notes(trace, "yogg.wave")]
    if waves:
        for when, ordinal in waves:
            print(f"  wave {ordinal:<2} opened at {clock(when)}")
    else:
        print("  no yogg.wave rows - no wave was ever seen, or this trace predates the clock")

    team = sorted({rec.get("g", 0) for rec in notes(trace, "yogg.brainteam") if rec.get("txt") == "1"})
    if team:
        roles = collections.Counter(trace.role(guid) for guid in team)
        names = ", ".join(trace.name(guid) for guid in team)
        print(f"\n  brain team ({len(team)}): {dict(roles)}")
        print(f"    {names}")

    # What the assignment cost. By group index it sent bots a median of 41-44 yd when the nearest
    # spot was 12-15, so assigned-against-nearest is the number that says whether it is fixed.
    assigned, nearest = [], []
    for rec in notes(trace, "yogg.portalslot"):
        slot = int(rec.get("txt", 0))
        where = position_at(trace, rec.get("g", 0), rec["t"])
        if not where or not 0 <= slot < len(PORTAL_SPOTS):
            continue
        assigned.append(math.dist(where, PORTAL_SPOTS[slot]))
        nearest.append(min(math.dist(where, spot) for spot in PORTAL_SPOTS))
    if assigned:
        print(f"\n  walk to the assigned spot: median {statistics.median(assigned):.1f} yd"
              f", nearest spot was {statistics.median(nearest):.1f} yd  ({len(assigned)} assignments)")

    states = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.portal"))
    if states:
        print(f"  portal intent rows   : {dict(states)}")

    # Who actually got down. The room is the only proof: the portal creature is not hostile, so no
    # snapshot samples one, and a click that failed leaves nothing else behind.
    arrived = {rec.get("g", 0) for rec in notes(trace, "yogg.room")
               if str(rec.get("txt", "")) in ("stormwind", "icecrown", "chamber", "brain")}
    if team:
        print(f"  reached the brain level: {len(arrived & set(team))} of {len(team)} on the team"
              f" ({len(arrived)} bots in all)")

    show_portal_drop(trace, waves)

    show_rooms(trace, waves)


def walked_for_a_portal(trace: Trace, guids: set[int], start: int, until: int) -> set[int]:
    """Of `guids`, the ones whose walk-to-a-portal node ran at all in the window. The node is gated on
    the bot's raid mark reading skull or diamond, and only a Flee portal click hands that mark back, so
    a bot teleported out by Induce Madness or killed below the platform keeps its illusion room's mark
    and the node never fires again. That bot never takes a step, which is a different failure from one
    that walked to a spot somebody else had already used."""
    return {rec.get("g", 0) for rec in trace.of("act")
            if rec.get("a") == "yogg-saron move to enter portal action"
            and rec.get("g") in guids and start <= rec["t"] <= until}


def show_portal_drop(trace: Trace, waves: list[tuple[int, int]]) -> None:
    """How many of the wave's portals were taken, against how many bots were told to take one.
    AddPortals spawns RAID_MODE(4, 10) of them per wave and each is one use, so the gap between
    assigned and gone is bots that walked to a spot somebody else had already used - or, when the node
    never ran at all, bots that never walked anywhere."""
    if not waves:
        return

    assignments: dict[int, dict[int, int]] = {}
    for rec in notes(trace, "yogg.portalslot"):
        assignments.setdefault(rec["t"], {})[rec.get("g", 0)] = int(rec.get("txt", 0))

    print()
    for start, ordinal in waves:
        rounds = [when for when in assignments if when <= start]
        plan = assignments[max(rounds)] if rounds else {}
        stale = (start - max(rounds)) / 1000.0 if rounds else 0.0

        took = {rec.get("s") for rec in trace.of("cast")
                if rec.get("sp") == SPELL_ILLUSION_ROOM and start <= rec["t"] <= start + PORTAL_DESPAWN_MS}
        missed = {guid for guid in plan if guid not in took}

        print(f"  wave {ordinal}: {len(took)} of {len(PORTAL_SPOTS)} portals taken,"
              f" {len(plan)} bots assigned a spot, plan was {stale:.0f} s old")
        if missed:
            walked = walked_for_a_portal(trace, missed, start, start + PORTAL_DESPAWN_MS)
            for label, who in (("walked and never got one", missed & walked),
                               ("never walked, the node never ran", missed - walked)):
                if who:
                    print(f"    assigned, {label}: {', '.join(sorted(trace.name(guid) for guid in who))}")


def show_rooms(trace: Trace, waves: list[tuple[int, int]]) -> None:
    """What each wave did with the room it landed in. A wave that never reaches `fighting` is a
    room the raid stood in rather than cleared, and the gap between arriving and the first cast at a
    tentacle is where that goes wrong: every tentacle is disguised on arrival, so a bot with none in
    sight has nothing to reveal one with and the room reads as already clear."""
    if not waves:
        return

    tentacles = set()
    for entry in (NPC_INFLUENCE_TENTACLE,) + NPC_TENTACLE_DISGUISES:
        tentacles |= guids_of_entry(trace, entry)

    ends = [start for start, _ in waves[1:]] + [trace.records[-1].get("t", 0) if trace.records else 0]
    print()
    for (start, ordinal), stop in zip(waves, ends):
        window = [rec for rec in notes(trace, "yogg.room") if start <= rec["t"] <= stop]
        rooms = collections.Counter(str(rec.get("txt", "")) for rec in window
                                    if str(rec.get("txt", "")) in ("stormwind", "icecrown", "chamber"))
        if not rooms:
            print(f"  wave {ordinal}: nobody reached an illusion room")
            continue

        room, bots = rooms.most_common(1)[0]
        entered = min(rec["t"] for rec in window if str(rec.get("txt", "")) == room)

        states = {str(rec.get("txt", "")) for rec in notes(trace, "yogg.roomstate")
                  if start <= rec["t"] <= stop}
        engaged = [rec["t"] for rec in trace.of("cast")
                   if start <= rec["t"] <= stop and rec.get("tgt") in tentacles]

        if engaged:
            delay = f"{(min(engaged) - entered) / 1000.0:.1f} s to the first cast at a tentacle"
        else:
            delay = "NEVER cast at a tentacle"
        cleared = "cleared" if states & {"tobrain", "atbrain"} else "not cleared"
        print(f"  wave {ordinal} {room:10} {bots} bots, entered {clock(entered)}, {delay}, {cleared}")
        print(f"    states: {', '.join(sorted(states)) or 'none'}")

    show_frozen(trace)
    show_vetoes(trace)


def show_frozen(trace: Trace) -> None:
    """The longest a bot went holding a target and casting nothing at all. This is what a vetoed
    walk looks like from outside: somewhere to be, no way to get there, and nothing moving the bot
    instead. One pull left five of six bots in a room doing it for fifty seconds while the tentacle
    they were pointed at sat at 87%."""
    frozen = idle_windows(trace, IDLE_MS)
    if not frozen:
        print(f"\n  nobody held a target for {IDLE_MS / 1000:.0f} s without casting")
        return

    print(f"\n  held a target and cast nothing for over {IDLE_MS / 1000:.0f} s:")
    for window in sorted(frozen, key=lambda w: (w["quiet"], w["start"], w["guid"]), reverse=True)[:8]:
        guid = window["guid"]
        print(f"    {trace.name(guid):14} {trace.role(guid):6} {window['quiet'] / 1000.0:5.1f} s"
              f" from {clock(window['start'])}")


def show_vetoes(trace: Trace) -> None:
    """Which multiplier zeroed which action, and how often. A veto is cheap to write and easy to
    get wrong: one that zeroes a walk with nothing walking in its place is a bot standing still, and
    this says so long before the room view does."""
    rows = veto_counts(trace)
    if not rows:
        return

    print("\n  multiplier vetoes:")
    for (multiplier, action), count in rows.most_common(10):
        print(f"    {count:5}  {multiplier} -> {action}")


def position_at(trace: Trace, guid: int, when: int):
    """Ground position only: everything here measures across a flat platform."""
    spot = at(trace, guid, when)
    return (spot[0], spot[1]) if spot else None


def show_brain(trace: Trace) -> None:
    print("BRAIN ROOM")

    rooms = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.room"))
    if rooms:
        print(f"  room reads       : {dict(rooms)}")

    # The deadlock: a bot that never walks in has no target, the tentacle lives, the door stays shut.
    states = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.roomstate"))
    if states:
        print(f"  room states      : {dict(states)}")
    else:
        print("  no yogg.roomstate rows - nothing was ever in an illusion room")

    brains = guids_of_entry(trace, NPC_BRAIN)
    if brains:
        track = []
        for snap in trace.of("snap"):
            for row in snap.get("u", []):
                if row[0] in brains:
                    track.append((snap["t"], row[5]))
        if track:
            print(f"  Brain health     : {track[0][1]:.1f}% at {clock(track[0][0])}"
                  f" -> {track[-1][1]:.1f}% at {clock(track[-1][0])}"
                  f", low {min(hp for _, hp in track):.1f}%")
    else:
        print("  the Brain was never sampled")

    tentacles = guids_of_entry(trace, NPC_INFLUENCE_TENTACLE)
    if tentacles:
        # Nothing in the trace says a creature died: the death stream is roster only, and so is dmg,
        # so counting deaths there always reads zero. The snapshot does carry the last
        # health each one was sampled at before it stopped appearing - a few percent for one the raid
        # killed, near full for one the phase despawned - so that is what gets reported.
        last = [rows[-1][1] for rows in tracks(trace, tentacles, ("t", "hp")).values() if rows]
        spent = sum(1 for hp in last if hp <= TENTACLE_SPENT_PCT)
        print(f"  Influence Tentacles seen: {len(tentacles)}, {spent} last seen under "
              f"{TENTACLE_SPENT_PCT:.0f}% health and gone")

    gaze = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_LUNATIC_GAZE_SKULL]
    if gaze:
        total = sum(rec.get("a", 0) for rec in gaze)
        print(f"  Lunatic Gaze     : {len(gaze)} hits for {total:,} damage")
    skulls = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.skull"))
    if skulls:
        near = []
        skull_guids = guids_of_entry(trace, NPC_LAUGHING_SKULL)
        for rec in notes(trace, "yogg.skull"):
            here = position_at(trace, rec.get("g", 0), rec["t"])
            seen = [position_at(trace, guid, rec["t"]) for guid in skull_guids]
            far = [math.dist(here, spot) for spot in seen if here and spot]
            if far:
                near.append(min(far))
        tail = ""
        if near:
            out_of_range = sum(1 for gap in near if gap > LAUGHING_SKULL_RADIUS)
            tail = (f", nearest skull median {statistics.median(near):.1f} yd,"
                    f" {out_of_range} of {len(near)} flips with none inside {LAUGHING_SKULL_RADIUS:.0f}")
        print(f"  skulls in arc    : {dict(skulls)}{tail}")

    # A walk faces its path, so only a still bot can turn off a skull.
    ticks = [(rec["t"], rec.get("d", 0)) for rec in trace.of("dmg")
             if rec.get("sp") == SPELL_LUNATIC_GAZE_SKULL and rec.get("d") in roster_guids(trace)]
    if ticks:
        states = skull_tick_states(ticks, snapshot_rows(trace, {victim for _, victim in ticks}))
        total = sum(states.values()) or 1
        print(f"  skull gaze ticks : {len(ticks)}, "
              + ", ".join(f"{name} {states[name] * 100 // total}%"
                          for name in ("walking", "still with a target", "still without")))
    turns = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.skullturn"))
    if turns:
        print(f"  yogg.skullturn   : {dict(turns)}")

    reach = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.tentacle"))
    if reach:
        print(f"  tentacle reach   : {dict(reach)}")

    reprisal = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_GRIM_REPRISAL]
    if reprisal:
        total = sum(rec.get("a", 0) for rec in reprisal)
        print(f"  Grim Reprisal    : {len(reprisal)} hits for {total:,} self-inflicted")
    show_reprisal(trace)

    madness = [rec for rec in trace.of("dmg") + trace.of("aura") if rec.get("sp") == SPELL_INDUCE_MADNESS]
    exits = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.exit"))
    if exits:
        # A safe after a leaving inside the same cast is the bot talking itself back to the Brain.
        brain_rows = snapshot_rows(trace, guids_of_entry(trace, NPC_BRAIN))
        casts = cast_windows([row for rows in brain_rows.values() for row in rows], SPELL_INDUCE_MADNESS)
        flips, left_in = 0, {}
        for rec in notes(trace, "yogg.exit"):
            guid = rec.get("g", 0)
            window = next((i for i, (start, stop) in enumerate(casts) if start <= rec["t"] <= stop), None)
            if window is None:
                continue
            if rec.get("txt") != "safe":
                left_in[guid] = window
            elif left_in.get(guid) == window:
                flips += 1
        print(f"  exit decisions   : {dict(exits)}, {flips} back to safe after leaving")

    nitro = [rec for rec in trace.of("cast") if rec.get("sp") == SPELL_NITRO_BOOSTS_USE]
    nitro_notes = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.nitro"))
    if nitro or nitro_notes:
        users = collections.Counter(trace.name(rec.get("s", 0)) for rec in nitro)
        print(f"  Nitro Boosts     : {len(nitro)} used {dict(users)}, probe {dict(nitro_notes)}")
    show_brain_walks(trace)
    if madness:
        print(f"  Induce Madness   : {len(madness)} rows")

    show_madness_caught(trace)

    spread = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.spread"))
    if spread:
        print(f"  tentacle picks   : {dict(spread)}")

    # Icecrown groups and Stormwind roles, then the wait at the door. Emit on change, so a count is how
    # often bots entered that share, not how long they held it.
    shares = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.share"))
    if shares:
        print(f"  room shares      : {dict(shares)}")
    for key, label in (("yogg.dash", "Dash"), ("yogg.charge", "Brain charges")):
        found = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, key))
        if found:
            print(f"  {label:16} : {dict(found)}")

    show_brain_waves(trace)
    show_stalled_walks(trace)


def show_brain_walks(trace: Trace) -> None:
    """How fast the walk from a cleared room to the Brain went per role, how often bots below turned on
    the spot, and which brain spot each non-melee was sent to."""
    states: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, "yogg.roomstate"):
        states[rec.get("g", 0)].append((rec["t"], str(rec.get("txt", ""))))
    rows = snapshot_rows(trace, roster_guids(trace))

    walks: dict[str, list[tuple[float, float, float]]] = collections.defaultdict(list)
    for guid, own in states.items():
        walks[trace.role(guid)] += walk_speeds(own, rows.get(guid, []))
    for role, done in sorted(walks.items()):
        if done:
            speed = statistics.median(yards / seconds for seconds, yards, _ in done)
            standing = statistics.median(still for _, _, still in done)
            print(f"  walk to the Brain: {role:6} {len(done):2} walks, median {speed:4.1f} yd/s,"
                  f" standing {standing:4.1f} s")

    turns = collections.Counter()
    for guid, own in rows.items():
        turns[trace.role(guid)] += facing_flips(own, BRAIN_LEVEL_Z)
    if sum(turns.values()):
        print(f"  turns on the spot below (over {FLIP_TURN} rad between samples): {dict(turns)}")

    spots = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.brainspot"))
    if spots:
        print(f"  brain spots      : {dict(spots)}")


def reprisal_deaths(deaths: list[dict], hits: list[dict], window_ms: int = REPRISAL_DEATH_WINDOW_MS) -> list[dict]:
    """Deaths below the platform that Grim Reprisal and the skulls' Lunatic Gaze did most of, over the
    last few seconds of damage. Both are the room's own cost of fighting there, so this is the count the
    reprisal hold exists to bring down."""
    killed = []
    for death in deaths:
        if death.get("z", BRAIN_LEVEL_Z) >= BRAIN_LEVEL_Z:
            continue
        window = [hit for hit in hits
                  if hit.get("d") == death.get("g") and death["t"] - window_ms <= hit["t"] <= death["t"]]
        total = sum(hit.get("a", 0) for hit in window)
        room = sum(hit.get("a", 0) for hit in window
                   if hit.get("sp") in (SPELL_GRIM_REPRISAL, SPELL_LUNATIC_GAZE_SKULL))
        if total and room * 2 > total:
            killed.append(death)
    return killed


def hold_summary(spans: dict[int, list[tuple[str, int, int]]], deaths: list[dict]) -> dict:
    """Reprisal holds from `yogg.reprisal` spans: how many, by how many bots, the median length in
    seconds, and how many ended with the bot dead anyway."""
    holds = [(guid, start, stop) for guid, rows in spans.items() for held, start, stop in rows if held == "1"]
    died = sum(1 for guid, start, stop in holds
               if any(death.get("g") == guid and start <= death["t"] <= stop for death in deaths))
    lengths = [(stop - start) / 1000.0 for _, start, stop in holds]
    return {"holds": len(holds), "bots": len({guid for guid, _, _ in holds}),
            "median": statistics.median(lengths) if lengths else None, "died": died}


def show_reprisal(trace: Trace) -> None:
    deaths = combat_deaths(trace)
    dead = {death.get("g") for death in deaths}
    killed = reprisal_deaths(deaths, [rec for rec in trace.of("dmg") if rec.get("d") in dead])
    if killed:
        waves = [(rec["t"], int(rec.get("txt", 0))) for rec in notes(trace, "yogg.wave")]
        named = []
        for death in killed:
            wave = max((ordinal for when, ordinal in waves if when <= death["t"]), default=0)
            named.append(f"{trace.name(death['g'])}({trace.role(death['g'])}) w{wave}")
        print(f"  reprisal deaths  : {len(killed)} below the platform, mostly reflect and gaze - {', '.join(named)}")

    end = trace.records[-1].get("t", 0) if trace.records else 0
    spans = holder_spans(trace, "yogg.reprisal", end)
    if spans:
        summary = hold_summary(spans, deaths)
        median = f"{summary['median']:.1f} s" if summary["median"] is not None else "-"
        print(f"  reprisal holds   : {summary['holds']} by {summary['bots']} bots, median {median},"
              f" {summary['died']} ended in a death")


def group_runs(stamps: list[int], gap_ms: int, min_ms: int) -> list[tuple[int, int]]:
    """Timestamps folded into (first, last) runs wherever neighbours sit within gap_ms, keeping only
    runs at least min_ms long."""
    runs: list[list[int]] = []
    for when in sorted(stamps):
        if runs and when - runs[-1][1] <= gap_ms:
            runs[-1][1] = when
        else:
            runs.append([when, when])
    return [(first, last) for first, last in runs if last - first >= min_ms]


def target_split(targets: list[int]) -> tuple[int, float]:
    """How many different units a group held in one snapshot, and the share of the group on the
    most-held one. The group must not be empty."""
    counts = collections.Counter(targets)
    return len(counts), max(counts.values()) / len(targets)


def show_madness_caught(trace: Trace) -> None:
    """Who Induce Madness charmed, and where each of them was when it landed. It leaves no damage row:
    it strips all 100 Sanity from whoever is still below the platform when its 60 s cast ends and
    teleports them up in the same effect, so the Insane aura and the spot that teleport started from
    are the whole record. The Flee portals are game objects, which no trace carries, so the distance is
    measured against the spots this pull's own clean exits went up from.

    Under each catch, what the way out lost: the time left when it first decided to go, how often it
    went back to safe, how long its exit walk was refused, and how long it spent casting."""
    charmed, clean, caught, rows = madness_split(trace)
    if not charmed:
        return
    ways_out = [(row[2], row[3]) for row in clean]

    verdicts = collections.defaultdict(list)
    for rec in notes(trace, "yogg.exit"):
        verdicts[rec.get("g", 0)].append((rec["t"], str(rec.get("txt", ""))))
    walks = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("by") == "yogg-saron move to exit portal action":
            walks[rec.get("g", 0)].append((rec["t"], str(rec.get("r", ""))))

    names = ", ".join(f"{trace.name(guid)}({trace.role(guid)})"
                      for guid in sorted(charmed, key=lambda guid: trace.name(guid)))
    print(f"  caught by it     : {len(caught)} of {len(charmed)} Insane - {names}")
    for when, guid, x, y, z in caught:
        gap = min((math.dist((x, y), spot) for spot in ways_out), default=0.0)
        height = f"{z - BRAIN_FLOOR_Z:.1f} yd in the air" if z > BRAIN_FLOOR_Z else "on the floor"
        print(f"    {clock(when):>10} {trace.name(guid):14} {trace.role(guid):6}"
              f" at ({x:7.1f},{y:6.1f},{z:6.1f}) {height:18}"
              f" {gap:5.1f} yd from the nearest way out, held still"
              f" {held_still(rows[guid], when, 1.0):5.1f} s")
        casting = [(row[0], row[11] if len(row) > 11 else 0) for row in rows[guid] if row[4] < BRAIN_LEVEL_Z]
        left, flips, refused, cast = exit_losses(verdicts[guid], walks[guid], casting, when)
        if left is None:
            print(f"{'':16}never decided to leave")
        else:
            print(f"{'':16}decided with {left:4.1f} s left, back to safe {flips}x,"
                  f" exit walk refused {refused:4.1f} s, casting {cast:4.1f} s")

    teleported = {guid for _, guid, *_ in caught}
    for guid in sorted(set(charmed) - teleported, key=lambda guid: charmed[guid]):
        spot = at(trace, guid, charmed[guid])
        where = "below" if spot and spot[2] < BRAIN_LEVEL_Z else "on the platform"
        print(f"    {clock(charmed[guid]):>10} {trace.name(guid):14} {trace.role(guid):6} drained to 0, {where}")


def show_brain_waves(trace: Trace) -> None:
    """One row per wave for the brain team: how long its room took, whether it split over the
    tentacles or walked them as one pack, where the healer stood, how the walk onto the Brain went and
    how much of the window it used.

    The room runs from the first team member below the platform to the first `tobrain`, the tick the
    room's door opened. Induce Madness ends 60 s after the wave note, so "left" is what remained when
    a bot decided to go and "spare" is what remained when it surfaced. A decision taken outside the
    brain room is counted, since the walk in is the walk out until there, except in the wave that ends
    phase 2."""
    waves = [(rec["t"], int(rec.get("txt", 0))) for rec in notes(trace, "yogg.wave")]
    team = {rec.get("g", 0) for rec in notes(trace, "yogg.brainteam") if rec.get("txt") == "1"}
    if not waves or not team:
        return

    tentacles = set()
    for entry in (NPC_INFLUENCE_TENTACLE,) + NPC_TENTACLE_DISGUISES:
        tentacles |= guids_of_entry(trace, entry)
    brain = next(iter(guids_of_entry(trace, NPC_BRAIN)), None)
    healers = {guid for guid in team if trace.role(guid) == "heal"}

    rows = snapshot_rows(trace, team | ({brain} if brain else set()))
    stamps = {guid: [row[0] for row in guid_rows] for guid, guid_rows in rows.items()}
    snaps = trace.of("snap")
    snap_stamps = [snap["t"] for snap in snaps]
    room_notes = notes(trace, "yogg.room")
    state_notes = notes(trace, "yogg.roomstate")
    exit_notes = [rec for rec in notes(trace, "yogg.exit") if rec.get("txt") in ("leaving", "late")]
    behind = [rec for rec in trace.of("move")
              if rec.get("by") == "set behind" and rec.get("ok") and rec.get("g") in team
              and rec.get("z", BRAIN_LEVEL_Z) < BRAIN_LEVEL_Z]

    def near(guid: int, when: int) -> list | None:
        index = bisect.bisect_right(stamps.get(guid, []), when)
        return rows[guid][index - 1] if index else None

    def median_or_dash(values: list[float], fmt: str) -> str:
        return format(statistics.median(values), fmt) if values else "-"

    # The brain hold: the team stops short of 30% while the platform has a tentacle on it, so the stun
    # runs on, and finishes the Brain late in the window.
    hold_notes = [(rec["t"], str(rec.get("txt", ""))) for rec in notes(trace, "yogg.brainhold")]
    phase3 = next((first for phase, first, _ in phase_spans(trace) if phase == 3), None)
    platform_rows = (snapshot_rows(trace, {guid for entry in TENTACLE_NAMES for guid in guids_of_entry(trace, entry)})
                     if hold_notes else {})

    def tentacles_up(when: int) -> int:
        count = 0
        for own in platform_rows.values():
            row = row_before(own, when)
            count += bool(row and when - row[0] <= 1500 and row[6] > 0)
        return count

    ends = [start for start, _ in waves[1:]] + [trace.records[-1].get("t", 0)]
    print("\n  per wave: room clear time, one-target snapshots and the top target's median share, melee walking,"
          f"\n  healer from the room middle and mates past {HEAL_RANGE:.0f} yd, its closest approach and"
          "\n  heals landed, set behind moves, door to the first Brain hit, healer to the Brain, exit"
          "\n  seconds left and spare, Brain lost")
    for (start, ordinal), stop in zip(waves, ends):
        im_end = start + INDUCE_MADNESS_CAST_MS
        down = [row[0] for guid in team for row in rows.get(guid, [])
                if start <= row[0] <= stop and row[4] < BRAIN_LEVEL_Z]
        if not down:
            print(f"    wave {ordinal}: nobody went down")
            continue
        arrived = min(down)

        rooms = collections.Counter(str(rec.get("txt", "")) for rec in room_notes
                                    if start <= rec["t"] <= stop and str(rec.get("txt", "")) in ROOM_MIDDLES)
        room = rooms.most_common(1)[0][0] if rooms else "?"
        middle = ROOM_MIDDLES.get(room)
        door = next((rec["t"] for rec in state_notes
                     if start <= rec["t"] <= stop and rec.get("txt") == "tobrain"), None)
        fight_end = door if door else min(im_end, stop)

        one, shares, held_snaps = 0, [], 0
        for snap in snaps[bisect.bisect_left(snap_stamps, arrived):bisect.bisect_right(snap_stamps, fight_end)]:
            held = [row[7] for row in snap.get("u", [])
                    if row[0] in team and len(row) > 7 and row[3] < BRAIN_LEVEL_Z and row[7] in tentacles]
            if len(held) < 2:
                continue
            distinct, share = target_split(held)
            held_snaps += 1
            one += distinct == 1
            shares.append(share)

        walked = total = 0
        for guid in team - healers:
            pts = [row for row in rows.get(guid, []) if arrived <= row[0] <= fight_end and row[4] < BRAIN_LEVEL_Z]
            for a, b in zip(pts, pts[1:]):
                span = b[0] - a[0]
                if span > 1000:
                    continue
                total += span
                if math.dist((a[2], a[3]), (b[2], b[3])) > 0.3:
                    walked += span

        from_middle, pairs, far = [], 0, 0
        for guid in healers:
            for row in rows.get(guid, []):
                if not (arrived <= row[0] <= fight_end and row[4] < BRAIN_LEVEL_Z and middle):
                    continue
                from_middle.append(math.dist((row[2], row[3]), middle))
                for mate in team - {guid}:
                    other = near(mate, row[0])
                    if other and row[0] - other[0] < 500 and other[4] < BRAIN_LEVEL_Z:
                        pairs += 1
                        far += math.dist(row[2:5], other[2:5]) > HEAL_RANGE

        swings = sum(1 for rec in behind if (door or stop) <= rec["t"] <= min(im_end, stop))

        # What the healer actually did with the room, which the distance alone does not say: one wave
        # held it to 17 landed heals against 137 and 172 in the other two, because its station was
        # 36-44 yd from its own room's tentacles and a heal reaches 38.5.
        landed = sum(1 for rec in trace.of("heal")
                     if rec.get("s") in healers and arrived <= rec["t"] <= min(im_end, stop))
        closest = min(from_middle) if from_middle else None

        first_hit = None
        brain_rows = [row for row in rows.get(brain, []) if start <= row[0] <= stop] if brain else []
        for a, b in zip(brain_rows, brain_rows[1:]):
            if b[6] < a[6] and door and b[0] >= door:
                first_hit = b[0]
                break
        lost = brain_rows[0][6] - brain_rows[-1][6] if brain_rows else 0.0

        decided = {}
        for rec in exit_notes:
            if start <= rec["t"] <= stop and rec.get("g") in team:
                decided.setdefault(rec["g"], rec["t"])
        left = [(im_end - when) / 1000 for when in decided.values()]
        spare = []
        for guid, when in decided.items():
            up = next((row[0] for row in rows.get(guid, []) if row[0] > when and row[4] >= BRAIN_LEVEL_Z), None)
            if up:
                spare.append((im_end - up) / 1000)

        at_brain = []
        if brain_rows and door:
            spot = (brain_rows[0][2], brain_rows[0][3])
            until = min(decided.values()) if decided else im_end
            for guid in healers:
                at_brain += [math.dist((row[2], row[3]), spot) for row in rows.get(guid, [])
                             if door + 10000 <= row[0] <= until and row[4] < BRAIN_LEVEL_Z]

        clear = f"{(door - arrived) / 1000:4.1f} s" if door else "  open"
        top = median_or_dash(shares, ".2f")
        walking = f"{walked * 100 / total:3.0f}%" if total else "  -"
        healer = (f"{median_or_dash(from_middle, '4.1f')} yd / {far * 100 / pairs:3.0f}%"
                  if pairs else f"{median_or_dash(from_middle, '4.1f')} yd /   -")
        healer += f" (closest {closest:4.1f}, {landed:3} heals)" if closest is not None else f" ({landed:3} heals)"
        hit = f"{(first_hit - door) / 1000:4.1f} s" if first_hit and door else "   -"
        outside = 0
        if brain_rows and brain_rows[-1][6] >= BRAIN_PHASE_3_PCT:
            outside = decided_outside(decided, rows, BRAIN_ROOM_MIDDLE, BRAIN_ROOM_RADIUS)
        outside_note = f", {outside} decided outside the brain room" if outside else ""
        print(f"    wave {ordinal} {room:9} {clear}  {one:3}/{held_snaps:<3} {top:>4}  {walking}  {healer}"
              f"  behind {swings:2}  hit {hit}  healer@Brain {median_or_dash(at_brain, '4.1f')} yd"
              f"  exit {median_or_dash(left, '4.1f')}/{median_or_dash(spare, '4.1f')} s  Brain -{lost:.1f}%"
              f"{outside_note}")

        if door and brain:
            staged, out, on_brain = 0, [], []
            for guid in team - healers:
                own = [row for row in rows.get(guid, []) if start <= row[0] <= stop]
                at_door = row_before(own, door)
                if not at_door or at_door[4] >= BRAIN_LEVEL_Z:
                    continue
                out.append(math.dist((at_door[2], at_door[3]), BRAIN_MELEE_SPOT))
                before = [rec["txt"] for rec in state_notes if rec.get("g") == guid and start <= rec["t"] < door]
                staged += bool(before) and before[-1] == "staged"
                when = brain_arrival(own, door, brain)
                if when is not None and when <= im_end:
                    on_brain.append((when - door) / 1000)
            casts = [rec for rec in trace.of("cast") if start <= rec["t"] <= stop and rec.get("s") in team]
            boosts = {"nitro": sum(1 for rec in casts if rec.get("sp") == SPELL_NITRO_BOOSTS_USE),
                      "dash": sum(1 for rec in casts if rec.get("sp") == SPELL_DASH),
                      "charge": sum(1 for rec in casts if rec.get("sp") in SPELL_BRAIN_CHARGES and rec.get("tgt") == brain)}
            print(f"      door open: {staged}/{len(out)} staged, {median_or_dash(out, '.0f')} yd from the melee spot,"
                  f" on the Brain {median_or_dash(on_brain, '.1f')} s / last"
                  f" {format(max(on_brain), '.1f') if on_brain else '-'} s,"
                  f" {', '.join(f'{label} {n}' for label, n in boosts.items())}")

        window = brain_hold_window([note for note in hold_notes if start <= note[0] <= min(im_end, stop)])
        if window and brain_rows:
            held, let_go, why = window
            at_hold = row_before(brain_rows, held)
            at_go = row_before(brain_rows, let_go) if let_go else None
            leak = f", {at_hold[6] - at_go[6]:.1f}% lost while held" if at_hold and at_go else ""
            go = f" at {(im_end - let_go) / 1000:.1f} s left" if let_go else ""
            if phase3 and start <= phase3 <= im_end:
                end = (f"phase 3 {(im_end - phase3) / 1000:.1f} s before the end with {tentacles_up(phase3)}"
                       f" tentacles up")
            else:
                end = f"the Brain survived at {brain_rows[-1][6]:.1f}%"
            print(f"      brain hold: from {(im_end - held) / 1000:.1f} s left at"
                  f" {at_hold[6] if at_hold else 0:.1f}% with {tentacles_up(held)} tentacles up, {why}{go}{leak}; {end}")


def brain_arrival(rows: list[list], door: int, brain: int, radius: float = BRAIN_ARRIVAL_RADIUS) -> int | None:
    """When a bot first stood still in the brain room with the Brain targeted, at or after the door
    opened. `rows` are its snapshot rows, [t, guid, x, y, z, o, hp, mana, target, moving, ...]."""
    for row in rows:
        if (row[0] >= door and len(row) > 9 and row[8] == brain and not row[9]
                and math.dist((row[2], row[3]), BRAIN_ROOM_MIDDLE) < radius):
            return row[0]
    return None


def walk_outcomes(walks: list[tuple[int, str]], steps: list[tuple[int, str]]) -> dict:
    """Per bot and node, how its forced walks went: which branches `yogg.walk` reported, and the yards
    still to go each time the stall release let go of a MoveTo booking.

    A walk that reports `walking` and never `arrived` is one the bot was still being sent on when the
    wave ended. `yogg.walkstep` is emitted on every release rather than on change, so its count is how
    many times the spline died short of where it was aimed."""
    out: dict = {}

    def row_for(guid: int, node: str) -> dict:
        return out.setdefault((guid, node), {"branches": collections.Counter(), "short": []})

    for guid, text in walks:
        node, _, branch = text.partition(" ")
        row_for(guid, node)["branches"][branch] += 1
    for guid, text in steps:
        node, _, rest = text.partition(" ")
        row_for(guid, node)["short"].append(float(rest.split()[0]))
    return out


def show_stalled_walks(trace: Trace) -> None:
    """The forced walks below the platform that never got where they were sent. Down there a forced
    spline dies after a yard or two and MoveTo refuses the same point again for MaxWaitForMove, so the
    walk advances in 5 s hops: one healer covered 24 of its 56 yd in 54 s and healed almost nothing all
    wave. The watchdog then gives up, and a bot standing still in an uncleared illusion room is a charm
    waiting to land - there is no exit from a room whose door has not opened."""
    walks = [(rec.get("g", 0), str(rec.get("txt", ""))) for rec in notes(trace, "yogg.walk")]
    steps = [(rec.get("g", 0), str(rec.get("txt", ""))) for rec in notes(trace, "yogg.walkstep")]
    # The exit walk is left out: it uses its portal at INTERACTION_DISTANCE, the same 5 yd that counts as
    # arrived, so a bot that got out cleanly often never records one. `caught by it` above is the honest
    # measure of that walk. These are the ones that leave a bot standing where it stopped.
    below = {"illusion", "healer", "brainspot", "stage"}

    rows = {key: value for key, value in walk_outcomes(walks, steps).items()
            if key[1] in below and ("arrived" not in value["branches"] or value["short"])}
    if not rows:
        return

    print("\n  walks below the platform that stalled or never arrived:")
    for (guid, node), value in sorted(rows.items(), key=lambda kv: (trace.name(kv[0][0]), kv[0][1])):
        branches = ", ".join(f"{name} x{count}" for name, count in sorted(value["branches"].items()))
        short = (f", released {len(value['short'])} times a median"
                 f" {statistics.median(value['short']):.0f} yd short" if value["short"] else "")
        print(f"    {trace.name(guid):14} {trace.role(guid):6} {node:10} {branches}{short}")


def aura_windows(trace: Trace, spell: int) -> list[tuple[int, int, int]]:
    """(guid, applied, removed) per aura window, closed at the last record when it never lifts."""
    end = trace.records[-1].get("t", 0) if trace.records else 0
    open_at: dict[int, int] = {}
    out: list[tuple[int, int, int]] = []
    for rec in sorted((r for r in trace.of("aura") if r.get("sp") == spell), key=lambda r: r["t"]):
        guid = rec.get("d", 0)
        if not rec.get("r"):
            open_at.setdefault(guid, rec["t"])
        elif guid in open_at:
            out.append((guid, open_at.pop(guid), rec["t"]))
    out.extend((guid, start, end) for guid, start in open_at.items())
    return sorted(out, key=lambda w: w[1])


def show_phase2(trace: Trace) -> None:
    """Everything phase 2 is decided by that is not the brain room: who gets held by a Constrictor and
    for how long, whether Brain Linked pairs ever close, and how often two nodes trade a bot."""
    print("PHASE 2")

    roster = roster_guids(trace)
    squeeze = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_SQUEEZE]
    grabs = aura_windows(trace, SPELL_SQUEEZE)
    rescues = [rec for rec in trace.of("cast") if rec.get("sp") in SPELL_HAND_OF_PROTECTION]

    if grabs:
        total = sum(rec.get("a", 0) for rec in squeeze)
        longest = max((stop - start) / 1000.0 for _, start, stop in grabs)
        print(f"  Constrictor grabs  : {len(grabs)}, {total:,} damage, longest {longest:.1f} s")
        for guid, start, stop in grabs:
            took = sum(rec.get("a", 0) for rec in squeeze
                       if rec.get("d") == guid and start <= rec["t"] <= stop + 500)
            freed = any(rec.get("tgt") == guid and start <= rec["t"] <= stop + 500 for rec in rescues)
            print(f"    {trace.name(guid):14} {trace.role(guid):6} {clock(start)} -> {clock(stop)}"
                  f"  ({(stop - start) / 1000.0:5.1f} s) {took:>7,}"
                  f"  {'Hand of Protection' if freed else 'rode it out'}")
        print(f"    rescues cast     : {len(rescues)}")
    else:
        print("  no Squeeze rows - nothing was ever grabbed")

    links = aura_windows(trace, SPELL_BRAIN_LINK)
    link_dmg = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_BRAIN_LINK_DAMAGE]
    if links:
        print(f"  Brain Link         : {len(links)} links, "
              f"{sum(rec.get('a', 0) for rec in link_dmg):,} damage")
        for guid, start, stop in links:
            # The aura holds the partner's GUID privately, but it casts 63803 (apart) or 63804
            # (together) on that partner every second, so the cast names the pair exactly. Damage rows
            # only name it while the two are already too far apart, which is the half that matters
            # least.
            partner = next((rec.get("tgt") for rec in trace.of("cast")
                            if rec.get("sp") in (SPELL_BRAIN_LINK_DAMAGE, SPELL_BRAIN_LINK_OK)
                            and rec.get("s") == guid and rec.get("tgt") not in (None, 0, guid)
                            and start <= rec["t"] <= stop), None)
            if partner is None:
                print(f"    {clock(start)} -> {clock(stop)}  {trace.name(guid):14}"
                      f"  never ticked, so the pair stayed inside {BRAIN_LINK_RANGE:.0f} yd")
                continue

            gaps = []
            for snap in trace.of("snap"):
                if not start <= snap["t"] <= stop:
                    continue
                seen = {row[0]: (row[1], row[2]) for row in snap.get("u", [])
                        if row[0] in (guid, partner)}
                if len(seen) == 2:
                    gaps.append(math.dist(seen[guid], seen[partner]))
            if not gaps:
                continue
            over = sum(1 for gap in gaps if gap > BRAIN_LINK_RANGE)
            print(f"    {clock(start)} -> {clock(stop)}  {trace.name(guid)}({trace.role(guid)})"
                  f" + {trace.name(partner)}({trace.role(partner)})"
                  f"  gap median {statistics.median(gaps):.1f} yd, {over} of {len(gaps)} samples over"
                  f" {BRAIN_LINK_RANGE:.0f}")
    else:
        print("  no 63802 rows - nothing was ever linked")

    show_handovers(trace)
    show_idle_platform(trace)


def show_idle_platform(trace: Trace) -> None:
    """Phase 2 time with nothing on the platform to kill, which is Sanity Well time. A window is a run
    of snapshots with no Guardian, Crusher, Constrictor or Corruptor sampled alive, counted from the
    first tentacle of the phase so the transformation before it does not read as idle."""
    killable = set()
    for entry in PLATFORM_KILLABLE:
        killable |= guids_of_entry(trace, entry)
    tentacles = killable - guids_of_entry(trace, NPC_GUARDIAN)
    phase2 = [(start, stop) for phase, start, stop in phase_spans(trace) if phase == 2]
    opened = first_seen(trace, tentacles) if tentacles else None
    if not phase2 or opened is None:
        return

    empty = []
    for snap in trace.of("snap"):
        if not any(start <= snap["t"] <= stop for start, stop in phase2) or snap["t"] < opened:
            continue
        if not any(row[0] in killable and row[5] > 0 for row in snap.get("u", [])):
            empty.append(snap["t"])
    windows = group_runs(empty, 600, 1500)

    sanity: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") == SPELL_SANITY and not rec.get("r") and rec.get("st") is not None:
            sanity[rec.get("d", 0)].append((rec["t"], rec["st"]))
    wells = [rec for rec in trace.of("aura") if rec.get("sp") == SPELL_SANITY_WELL and not rec.get("r")]
    idle_notes = [rec for rec in notes(trace, "yogg.sanity") if rec.get("txt") == "idle"]
    roster = roster_guids(trace)

    reasons = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.sanity"))
    if reasons:
        print(f"  sanity walks       : {dict(reasons)}")

    total = sum(stop - start for start, stop in windows)
    print(f"  nothing to kill on the platform: {len(windows)} windows, {total / 1000:.1f} s")
    for start, stop in windows:
        levels = []
        for guid in roster:
            spot = at(trace, guid, start)
            held = [stacks for when, stacks in sanity.get(guid, []) if when <= start]
            if spot and spot[2] >= BRAIN_LEVEL_Z and held:
                levels.append(held[-1])
        at_well = {rec.get("d") for rec in wells if start <= rec["t"] <= stop}
        trips = {rec.get("g") for rec in idle_notes if start <= rec["t"] <= stop}
        under = sum(1 for level in levels if level < 100)
        median = f"{statistics.median(levels):.0f}" if levels else "-"
        print(f"    {clock(start)} -> {clock(stop)}  {(stop - start) / 1000:5.1f} s  Sanity median {median},"
              f" {under} of {len(levels)} upstairs under 100, {len(trips)} idle walks, {len(at_well)} reached a well")


def stun_window(wave: int, door: int, phase3: int | None) -> tuple[int, int] | None:
    """Shattered Illusion's span in one wave. It starts when the brain room door opens, which the Brain
    does in the same branch, and ends when Induce Madness lands or phase 3 starts, whichever is first."""
    end = wave + INDUCE_MADNESS_CAST_MS
    if phase3 is not None:
        end = min(end, phase3)
    return (door, end) if end > door else None


def hp_removed(samples: list[tuple[int, float]], max_hp: int, start: int, end: int) -> float:
    """Health taken off one unit inside [start, end] from its (t, hp%) samples. Only drops count, and a
    drop across `start` counts from the last sample before it."""
    removed = 0.0
    previous = None
    for when, pct in samples:
        if when > end:
            break
        if when >= start and previous is not None:
            removed += max(0.0, previous - pct)
        previous = pct
    return removed / 100.0 * max_hp


def show_tentacles(trace: Trace) -> None:
    """What phase 2 leaves on the platform. Shattered Illusion is the only time the raid gains on it,
    with no spawns, no Diminish Power and no Crush, so each wave reads as the tentacle stock when the
    brain room door opened, what the stun took off it and when the platform was clear."""
    print("PLATFORM TENTACLES")

    spans = phase_spans(trace)
    phase2 = next(((start, stop) for phase, start, stop in spans if phase == 2), None)
    phase3 = next((start for phase, start, _ in spans if phase == 3), None)
    if not phase2:
        print("  no phase 2 in this trace")
        return

    tentacles: dict[int, int] = {}
    for entry in TENTACLE_NAMES:
        for guid in guids_of_entry(trace, entry):
            tentacles[guid] = entry
    samples = {guid: [(row[0], row[6]) for row in rows]
               for guid, rows in snapshot_rows(trace, set(tentacles)).items()}
    stamps = {guid: [when for when, _ in rows] for guid, rows in samples.items()}
    if not samples:
        print("  no tentacle was ever sampled")
        return

    def alive(guid: int, when: int) -> float | None:
        index = bisect.bisect_right(stamps[guid], when)
        if not index:
            return None
        sampled, pct = samples[guid][index - 1]
        # A live tentacle is in every snapshot, so a gap this long means it died or despawned.
        return pct if pct > 0 and when - sampled <= 1000 else None

    def stock(when: int) -> tuple[float, list[str]]:
        total, parts = 0.0, []
        for guid in samples:
            pct = alive(guid, when)
            if pct is not None:
                total += pct / 100.0 * trace.maxhp.get(guid, 0)
                parts.append(f"{TENTACLE_NAMES[tentacles[guid]]} {pct:.0f}%")
        return total, parts

    def removed(start: int, end: int) -> float:
        return sum(hp_removed(rows, trace.maxhp.get(guid, 0), start, end) for guid, rows in samples.items())

    waves = [(rec["t"], int(rec.get("txt", 0))) for rec in notes(trace, "yogg.wave")]
    doors = sorted(rec["t"] for rec in notes(trace, "yogg.roomstate") if rec.get("txt") in ("tobrain", "atbrain"))
    team = {rec.get("g", 0) for rec in notes(trace, "yogg.brainteam") if rec.get("txt") == "1"}
    team_heights = tracks(trace, team, ("t", "z"))
    snaps = [snap for snap in trace.of("snap") if phase2[0] <= snap["t"] <= (phase3 or phase2[1])]

    stuns: list[tuple[int, int, int]] = []
    for wave, ordinal in waves:
        door = next((when for when in doors if wave <= when < wave + INDUCE_MADNESS_CAST_MS), None)
        window = stun_window(wave, door, phase3) if door is not None else None
        if not window:
            print(f"  wave {ordinal:<2} {clock(wave)}  no door opened before Induce Madness")
            continue
        start, end = window
        stuns.append((ordinal, start, end))

        at_door, parts = stock(start)
        left, _ = stock(end - 1)
        took = removed(start, end)
        clear = next((snap["t"] for snap in snaps if start <= snap["t"] < end
                      and not any(row[0] in tentacles and row[5] > 0 for row in snap.get("u", []))), None)

        surfaced = []
        for guid, heights in team_heights.items():
            rows = [(when, z) for when, z in heights if wave <= when < wave + 80000]
            below = next((when for when, z in rows if z < BRAIN_LEVEL_Z), None)
            up = next((when for when, z in rows if below is not None and when > below and z >= BRAIN_LEVEL_Z), None)
            if up is not None:
                surfaced.append(up - wave)

        seconds = (end - start) / 1000.0
        cut = " (phase 3)" if phase3 is not None and end == phase3 else ""
        print(f"  wave {ordinal:<2} {clock(wave)}  stun +{(start - wave) / 1000:.1f} -> +{(end - wave) / 1000:.1f}{cut}"
              f"  stock {at_door / 1e6:.2f}M -> {left / 1e6:.2f}M, took {took / 1e6:.2f}M"
              f" ({took / seconds / 1000:.1f}k/s)"
              f"  clear {'+%.1f' % ((clear - wave) / 1000) if clear else 'never'}"
              f"  team up {'+%.1f' % (statistics.median(surfaced) / 1000) if surfaced else '-'}")
        if parts:
            print(f"      at the door: {', '.join(parts)}")

    stunned = [(start, end) for _, start, end in stuns]
    live, cursor = [], phase2[0]
    for start, end in stunned:
        live.append((cursor, start))
        cursor = end
    live.append((cursor, phase3 or phase2[1]))
    for label, windows in (("live", live), ("stunned", stunned)):
        seconds = sum(end - start for start, end in windows) / 1000.0
        took = sum(removed(start, end) for start, end in windows)
        if seconds > 0:
            print(f"  {label:8}: {took / 1e6:.2f}M over {seconds:.0f} s = {took / seconds / 1000:.1f}k/s")

    if phase3 is not None:
        left, _ = stock(phase3)
        standing = [guid for guid in samples if alive(guid, phase3) is not None]
        print(f"  at phase 3 {clock(phase3)}: {len(standing)} alive, {left / 1e6:.2f}M")
        for guid in sorted(standing, key=lambda g: samples[g][0][0]):
            print(f"      {TENTACLE_NAMES[tentacles[guid]]:11} spawned {clock(samples[guid][0][0])}"
                  f"  {alive(guid, phase3):.1f}%")

    # Who used a stun: bot melee and pets holding a Crusher inside one, and any Crush that followed it.
    crushers = {guid for guid, entry in tentacles.items() if entry == NPC_CRUSHER_TENTACLE}
    on_it = collections.Counter()
    for snap in snaps:
        if not any(start <= snap["t"] < end for start, end in stunned):
            continue
        for row in snap.get("u", []):
            if row[7] not in crushers or row[5] <= 0:
                continue
            if row[0] in trace.owners:
                on_it["pet"] += 1
            elif trace.role(row[0]) == "melee" and row[0] not in trace.humans:
                on_it["bot melee"] += 1
    # Each Crush writes two cast rows in the same millisecond, and only one of them names the victim.
    after = collections.Counter()
    for cast in trace.of("cast"):
        victim = cast.get("tgt")
        if cast.get("sp") not in SPELL_CRUSH_CONES or not victim or not any(
                end <= cast["t"] < end + STUN_CRUSH_WATCH_MS for _, end in stunned):
            continue
        after["pet" if victim in trace.owners else trace.role(victim)] += 1
    print(f"  on a stunned Crusher: {dict(on_it) or 'nobody'} (samples)")
    print(f"  Crush within {STUN_CRUSH_WATCH_MS // 1000} s of a stun lifting: {sum(after.values())}"
          f"{' ' + str(dict(after)) if after else ''}")
    engaged = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.stunned"))
    leaving = sum(1 for rec in notes(trace, "yogg.crush")
                  if rec.get("txt") == "reach" and trace.role(rec.get("g", 0)) == "melee")
    if engaged:
        print(f"  yogg.stunned: {dict(engaged)}, melee yogg.crush=reach rows: {leaving}")


def show_handovers(trace: Trace) -> None:
    """Two nodes trading a bot. A successful move handed to a different node inside two seconds is one
    of them undoing the other, and it is what a raid strategy with no movement guard looks like."""
    spans = [(start, stop) for phase, start, stop in phase_spans(trace) if phase == 2]
    if not spans:
        return

    roster = roster_guids(trace)
    last: dict[int, tuple[int, str]] = {}
    per_bot: dict[int, int] = collections.Counter()
    pairs: dict[tuple[str, str], int] = collections.Counter()
    issued: dict[str, int] = collections.Counter()
    for rec in trace.of("move"):
        if not any(start <= rec["t"] <= stop for start, stop in spans):
            continue
        guid, node = rec.get("g", 0), str(rec.get("by", "?"))
        if guid not in roster:
            continue
        issued[node] += 1
        if not rec.get("ok"):
            continue
        before = last.get(guid)
        last[guid] = (rec["t"], node)
        if before and before[1] != node and rec["t"] - before[0] <= 2000:
            per_bot[guid] += 1
            pairs[tuple(sorted((before[1], node)))] += 1

    if not per_bot:
        return

    by_role: dict[str, list[int]] = collections.defaultdict(list)
    for guid, count in per_bot.items():
        by_role[trace.role(guid)].append(count)
    parts = ", ".join(f"{role} {sum(v) / len(v):.0f}/bot" for role, v in sorted(by_role.items()))
    print(f"  node handovers     : {sum(per_bot.values())} in phase 2 ({parts})")
    for (one, two), count in pairs.most_common(3):
        print(f"    {one} <-> {two}: {count}")
    for node, count in issued.most_common(4):
        print(f"    issued {count:5} by {node}")


def show_crush(trace: Trace) -> None:
    print("CRUSH, KNOCKBACK AND DEATH RAYS")

    for key, label in (("yogg.crush", "Crush"), ("yogg.knockback", "body ring"),
                       ("yogg.deathray", "Death Ray")):
        rows = notes(trace, key)
        if not rows:
            print(f"  {label:10}: no {key} rows")
            continue
        per_role: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
        for rec in rows:
            per_role[trace.role(rec.get("g", 0))][str(rec.get("txt", ""))] += 1
        print(f"  {label}:")
        for role in sorted(per_role):
            total = sum(per_role[role].values()) or 1
            shares = "  ".join(f"{value}: {count * 100.0 / total:4.1f}%"
                               for value, count in sorted(per_role[role].items()))
            print(f"    {role:7} {shares}   ({total} transitions)")

    for spell, label in ((SPELL_CRUSH_CONE, "Crush 64147"), (SPELL_KNOCK_BACK, "Knock Back 64020")):
        hits = [rec for rec in trace.of("dmg") if rec.get("sp") == spell]
        if hits:
            total = sum(rec.get("a", 0) for rec in hits)
            killed = sum(1 for rec in hits if rec.get("ok", 0) > 0)
            print(f"  {label}: {len(hits)} hits, {total:,} damage, {killed} killing blows")

    deaths = combat_deaths(trace)
    resets = len(death_records(trace)) - len(deaths)
    if deaths or resets:
        by_role = collections.Counter(trace.role(rec.get("g", 0)) for rec in deaths)
        wiped = f"  (+{resets} to a wipe command)" if resets else ""
        print(f"  deaths by role: {dict(by_role)}{wiped}")

    lanes = [rec for rec in trace.of("haz") if rec.get("shape") == "wedge"]
    circles = [rec for rec in trace.of("haz") if rec.get("sp") == SPELL_KNOCK_BACK]
    print(f"  haz rows: {len(lanes)} Crush wedges, {len(circles)} body rings")

    guard = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.petguard"))
    if guard:
        print(f"  pet guard        : {dict(guard)}")

    detour = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.detour"))
    if detour:
        print(f"  body detour      : {dict(detour)}")

    judged = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.judgement"))
    if judged:
        print(f"  judgement node   : {dict(judged)}")

    show_launches(trace)
    show_crush_melee(trace)
    show_crush_hits(trace)
    show_crusher_reach(trace)
    show_diminish_power(trace)
    show_death_rays(trace)


def show_death_rays(trace: Trace) -> None:
    """One line per Death Ray burst on a raider: the last walk that went out in the 3 s before, which is
    often the dodge itself, and whether the Yogg dodge or avoid aoe tried to move it."""
    roster = set(roster_guids(trace))
    hits = sorted((rec["t"], rec.get("d")) for rec in trace.of("dmg")
                  if rec.get("sp") in SPELL_DEATH_RAY_DAMAGE and rec.get("d") in roster)
    if not hits:
        return
    moves: dict[int, list[tuple[int, str, bool]]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("g") in roster:
            moves[rec.get("g")].append((rec["t"], str(rec.get("by", "")), bool(rec.get("ok"))))
    died: dict[int, list[int]] = collections.defaultdict(list)
    for rec in death_records(trace):
        died[rec.get("g", 0)].append(rec["t"])
    print(f"\n  Death Ray hits     : {len(hits)}")
    last: dict[int, int] = {}
    for when, victim in hits:
        burst = when - last.get(victim, -DEATH_RAY_LOOK_MS - 1) <= DEATH_RAY_LOOK_MS
        last[victim] = when
        if burst:
            continue
        mover, tried = ray_hit_movers(moves[victim], when)
        dodges = ", ".join(f"{action} {'walked' if ok else 'refused'}" for action, ok in tried.items()) or "no dodge"
        dead = any(when <= then <= when + DEATH_RAY_LOOK_MS for then in died[victim])
        print(f"    {clock(when)}  {trace.name(victim):14} {trace.role(victim):6} last walk {mover}; {dodges}"
              f"{'; died' if dead else ''}")


def merge_spans(spans: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Overlapping or touching (start, end) spans folded into one, in order."""
    merged: list[list[int]] = []
    for start, end in sorted(spans):
        if merged and start <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    return [(start, end) for start, end in merged]


def snapshot_rows(trace: Trace, guids: set) -> dict[int, list[list]]:
    """Every snapshot row for these guids, as [t, *row], in time order. `at` only gives a position,
    and a Crusher's victim lives in the row's target column."""
    rows: dict[int, list[list]] = collections.defaultdict(list)
    for snap in trace.of("snap"):
        for row in snap.get("u", []):
            if row[0] in guids:
                rows[row[0]].append([snap["t"], *row])
    return rows


def surfacings(rows: list[list], level_z: float) -> list[tuple[int, float, float, float]]:
    """(t, x, y, z) of the last sample below `level_z` before each crossing up past it. A bot leaves the
    brain level either by clicking a Flee portal or by Induce Madness teleporting it out, and where it
    was standing is what tells the two apart."""
    out: list[tuple[int, float, float, float]] = []
    below: list | None = None
    for row in rows:
        if row[4] < level_z:
            below = row
        elif below is not None:
            out.append((below[0], below[2], below[3], below[4]))
            below = None
    return out


def madness_teleports(surfaced: list[tuple], charmed_at: dict[int, int], window_ms: int):
    """Ascents split into the ones that clicked a portal and the ones Induce Madness teleported, which
    are the ones a charm follows inside `window_ms`. Each tuple is (t, guid, x, y, z)."""
    clean, caught = [], []
    for row in surfaced:
        charm = charmed_at.get(row[1])
        late = charm is not None and 0 <= charm - row[0] <= window_ms
        (caught if late else clean).append(row)
    return clean, caught


def madness_split(trace: Trace) -> tuple[dict[int, int], list[tuple], list[tuple], dict[int, list[list]]]:
    """Every Insane (guid -> when), the ascents that clicked a portal, the ones Induce Madness
    teleported, and the snapshot rows behind them. An Insane with no teleport in front of it was
    drained to 0 Sanity instead."""
    charmed: dict[int, int] = {}
    for rec in trace.of("aura"):
        if rec.get("sp") == SPELL_INSANE and not rec.get("r"):
            charmed.setdefault(rec.get("d", 0), rec["t"])
    rows = snapshot_rows(trace, roster_guids(trace))
    surfaced = sorted((when, guid, x, y, z) for guid, track in rows.items()
                      for when, x, y, z in surfacings(track, BRAIN_LEVEL_Z))
    clean, caught = madness_teleports(surfaced, charmed, MADNESS_CHARM_MS)
    return charmed, clean, caught, rows


def cast_windows(rows: list[list], spell: int) -> list[tuple[int, int]]:
    """(start, end) of each run of samples casting `spell`, off [t, *row] snapshot rows in time order."""
    windows, start, last = [], None, None
    for row in sorted(rows, key=lambda row: row[0]):
        casting = len(row) > 11 and row[11] == spell
        if casting and start is None:
            start = row[0]
        elif not casting and start is not None:
            windows.append((start, last))
            start = None
        if casting:
            last = row[0]
    if start is not None:
        windows.append((start, last))
    return windows


def exit_losses(verdicts: list[tuple[int, str]], walks: list[tuple[int, str]], casting: list[tuple[int, int]],
                end: int, window_ms: int = INDUCE_MADNESS_CAST_MS) -> tuple[float | None, int, float, float]:
    """What a bot caught by Induce Madness lost on the way out, over the `window_ms` before `end`, when
    the cast ended. `verdicts` are its yogg.exit notes as (t, txt), `walks` its exit portal moves as
    (t, outcome) and `casting` its samples as (t, spell being cast or 0).

    Returns the seconds left at its first leaving or late verdict (None if it never had one), how many
    times it went back to safe after that, the seconds its exit walk was refused, and the seconds it
    spent casting after deciding."""
    start = end - window_ms
    first = next((t for t, txt in verdicts if start <= t <= end and txt in ("leaving", "late")), None)
    if first is None:
        return None, 0, 0.0, 0.0

    flips = sum(1 for t, txt in verdicts if first < t <= end and txt == "safe")

    refused, since = 0.0, None
    for t, outcome in walks:
        if not first <= t <= end:
            continue
        if outcome == "wait":
            since = t if since is None else since
        elif since is not None:
            refused += (t - since) / 1000.0
            since = None
    if since is not None:
        refused += (end - since) / 1000.0

    samples = [(t, spell) for t, spell in casting if first <= t <= end]
    cast = sum((nxt - t) / 1000.0 for (t, spell), (nxt, _) in zip(samples, samples[1:] + [(end, 0)]) if spell)

    return (end - first) / 1000.0, flips, refused, cast


def sanity_trips(heights: list[tuple[int, float]], sanity: list[tuple[int, int]],
                 level_z: float) -> list[tuple[int, int, int | None, int | None, int | None]]:
    """Each trip below `level_z` as (down, up, Sanity going down, lowest below, Sanity coming up).
    `heights` is (t, z) and `sanity` (t, stacks), both in time order. A trip still below at the end
    closes on the last sample."""
    def level(when: int) -> int | None:
        held = [stacks for t, stacks in sanity if t <= when]
        return held[-1] if held else None

    spans, start = [], None
    for t, z in heights:
        if z < level_z and start is None:
            start = t
        elif z >= level_z and start is not None:
            spans.append((start, t))
            start = None
    if start is not None:
        spans.append((start, heights[-1][0]))

    trips = []
    for down, up in spans:
        at_down = level(down)
        seen = [stacks for t, stacks in sanity if down <= t <= up] + ([at_down] if at_down is not None else [])
        trips.append((down, up, at_down, min(seen) if seen else None, level(up)))
    return trips


def walk_speeds(states: list[tuple[int, str]], rows: list[list],
                still_yd: float = WALK_STILL_YD) -> list[tuple[float, float, float]]:
    """(seconds, yards, seconds standing) of each walk from a cleared illusion room to the Brain: a
    yogg.roomstate tobrain followed straight by atbrain. `states` are (t, txt) and `rows` the bot's
    [t, guid, x, y, ...] snapshot rows, both in time order."""
    out = []
    for (start, state), (stop, nxt) in zip(states, states[1:]):
        if state != "tobrain" or nxt != "atbrain" or stop <= start:
            continue
        span = [row for row in rows if start <= row[0] <= stop]
        steps = [(b[0] - a[0], math.dist(a[2:4], b[2:4])) for a, b in zip(span, span[1:])]
        if not steps:
            continue
        standing = sum(ms for ms, step in steps if step < still_yd) / 1000.0
        out.append(((stop - start) / 1000.0, sum(step for _, step in steps), standing))
    return out


def facing_flips(rows: list[list], level_z: float, still_yd: float = WALK_STILL_YD,
                 turn: float = FLIP_TURN) -> int:
    """Samples below `level_z` where a bot standing still had turned more than `turn` radians since the
    last one, off [t, guid, x, y, z, o, ...] rows in time order."""
    flips = 0
    for a, b in zip(rows, rows[1:]):
        if b[4] >= level_z or math.dist(a[2:4], b[2:4]) >= still_yd:
            continue
        if abs((b[5] - a[5] + math.pi) % (2 * math.pi) - math.pi) > turn:
            flips += 1
    return flips


def res_into_insane(casts: list[tuple[int, int, int]], deaths: list[tuple[int, int, bool]],
                    insane: list[tuple[int, int]]) -> list[tuple[int, int, int, float | None]]:
    """Resurrections cast on a corpse that died without Sanity, as (t, caster, target, seconds until it
    went Insane again or None). `casts` are (t, caster, target), `deaths` (t, guid, kept Sanity) and
    `insane` (t, guid), all in time order. Voice re-applies Insane to anyone without Sanity, so these
    come back charmed."""
    out = []
    for t, caster, target in casts:
        died = [kept for when, guid, kept in deaths if guid == target and when <= t]
        if not died or died[-1]:
            continue
        again = next((when for when, guid in insane if guid == target and when > t), None)
        out.append((t, caster, target, (again - t) / 1000.0 if again is not None else None))
    return out


def aura_seconds(rows: list[tuple[int, int, bool]], start: int, stop: int) -> float:
    """Unit-seconds an aura was up inside [start, stop], from (t, guid, removed) rows in time order.
    One still up at `stop` counts to there."""
    opened: dict[int, int] = {}
    total = 0
    for t, guid, removed in rows:
        if not removed:
            opened.setdefault(guid, t)
        elif guid in opened:
            began = opened.pop(guid)
            total += max(0, min(t, stop) - max(began, start))
    for began in opened.values():
        total += max(0, stop - max(began, start))
    return total / 1000.0


def decided_outside(decided: dict[int, int], rows: dict[int, list[list]], middle: tuple[float, float],
                    radius: float) -> int:
    """How many exit decisions, guid -> time, came while the bot stood more than `radius` from
    `middle`, read off its last snapshot_rows row at or before the decision."""
    count = 0
    for guid, when in decided.items():
        row = row_before(rows.get(guid, []), when)
        count += bool(row) and math.dist(row[2:4], middle) > radius
    return count


def swings_in_gaze(swings: list[tuple[int, int, int]], gazes: list[tuple[int, int]],
                   taunts: list[tuple[int, int]], victims: dict[int, list[tuple[int, int]]]) -> tuple[int, int]:
    """Of Guardian swings on the raid, as (t, guardian, victim): how many landed inside a gaze window,
    and how many of those had a taunt cast on that Guardian since it turned on that victim. `taunts`
    are (t, guardian) and `victims` each Guardian's (t, victim) samples, all in time order."""
    inside = taunted = 0
    for when, guardian, victim in swings:
        if not any(first <= when <= last for first, last in gazes):
            continue
        inside += 1
        since = when
        for t, target in reversed([sample for sample in victims.get(guardian, []) if sample[0] <= when]):
            if target != victim:
                break
            since = t
        taunted += any(since <= t <= when and target == guardian for t, target in taunts)
    return inside, taunted


def keepers_from_auras(spell_ids) -> list[str]:
    """The freed Keepers, in the boss script's order, from the spell ids of auras seen on the raid."""
    seen = set(spell_ids)
    return [name for spell, name in KEEPER_PASSIVES.items() if spell in seen]


def drain_life_channels(rows: list[tuple[int, int, int, int]]) -> list[tuple[int, int, int]]:
    """Each Drain Life channel as (start, caster, ms), from its aura rows on the victim as (t, caster,
    victim, removed) in time order. A channel still open at the end is left out."""
    opened: dict[tuple[int, int], int] = {}
    channels = []
    for when, caster, victim, removed in rows:
        if not removed:
            opened[(caster, victim)] = when
        elif (caster, victim) in opened:
            began = opened.pop((caster, victim))
            channels.append((began, caster, when - began))
    return channels


def drain_life_spans(rows: list[tuple[int, int, int, int]]) -> list[int]:
    """How long each Drain Life channel ran, in ms, from the same rows as drain_life_channels."""
    return [ms for _, _, ms in drain_life_channels(rows)]


def ready_kickers(kicks: dict[int, tuple[list[int], int, float]], spots: dict[int, tuple[float, float]],
                  channel: tuple[float, float], when: int, slack: float = KICK_RANGE_SLACK) -> int:
    """How many bots had their kick off cooldown and the channelling Guardian in range at `when`. Kicks
    are per bot (sorted cast times, cooldown ms, range yd), spots where each bot stood then."""
    return sum(1 for guid, (casts, cooldown, reach) in kicks.items()
               if guid in spots and not on_cooldown(casts, when, cooldown)
               and math.dist(spots[guid], channel) <= reach + slack)


def guardians_on(victims: dict[int, list[list]], tank: int, when: int, floor: float = 0.0,
                 fresh_ms: int = 1500) -> int:
    """Guardians whose snapshot_rows row just before `when`, no older than `fresh_ms`, has `tank` as its
    target and health over `floor`."""
    count = 0
    for own in victims.values():
        row = row_before(own, when)
        if row and when - row[0] <= fresh_ms and row[8] == tank and row[6] > floor:
            count += 1
    return count


def taunt_bounced(victim_rows: list[list], caster: int, when: int, tanks: set,
                  after_ms: int = TAUNT_BOUNCE_MS) -> bool:
    """Whether a Guardian `caster` taunted at `when` sat on another tank `after_ms` later, by its
    snapshot_rows rows."""
    row = row_before(victim_rows, when + after_ms)
    return bool(row) and row[0] > when and row[8] in tanks and row[8] != caster


def ray_hit_movers(moves: list[tuple[int, str, bool]], when: int,
                   window_ms: int = DEATH_RAY_LOOK_MS) -> tuple[str, dict[str, bool]]:
    """Over one bot's (t, action, ok) move rows: the last action whose walk went out in the `window_ms`
    before a Death Ray hit, '-' for none, and per Death Ray dodger that tried, whether one of its walks
    went out."""
    inside = [(action, ok) for at_ms, action, ok in moves if when - window_ms <= at_ms <= when]
    walked = [action for action, ok in inside if ok]
    tried: dict[str, bool] = {}
    for action, ok in inside:
        if action in DEATH_RAY_DODGERS:
            tried[action] = tried.get(action, False) or ok
    return (walked[-1] if walked else "-"), tried


def guardian_power_shares(health: list[float]) -> tuple[float, float]:
    """Of Immortal Guardian health samples, the share at 90% or more, all nine Empowered stacks, and the
    share under 10%, Weakened."""
    if not health:
        return 0.0, 0.0
    return (sum(1 for hp in health if hp >= 90) / len(health),
            sum(1 for hp in health if hp < 10) / len(health))


def ranged_station(crushers: list[tuple[float, float]]) -> tuple[str, tuple[float, float, float]]:
    """The module's phase 3 ranged station against the live Crushers' (x, y): the ranged spot, else the
    first standby spot clear of every one, else whichever stands furthest from the nearest."""
    def clearance(spot: tuple[float, float, float]) -> float:
        return min((math.dist(spot[:2], crusher) for crusher in crushers), default=math.inf)

    best = ("ranged spot", PHASE_3_RANGED_SPOT)
    if clearance(best[1]) >= PHASE_3_RANGED_CRUSHER_CLEARANCE:
        return best
    for name, spot in PHASE_3_RANGED_STANDBY_SPOTS:
        if clearance(spot) >= PHASE_3_RANGED_CRUSHER_CLEARANCE:
            return name, spot
        if clearance(spot) > clearance(best[1]):
            best = (name, spot)
    return best


def in_phase3_position(role: str, where: tuple[float, float, float], moving: bool,
                       target: tuple[float, float, float, float] | None,
                       ranged_spot: tuple[float, float, float] = PHASE_3_RANGED_SPOT) -> bool:
    """The module's Heroism test for one raider: on the platform and inside its phase 3 station, or
    standing still in reach of a live target given as (x, y, z, combat reach)."""
    if where[2] < BRAIN_LEVEL_Z:
        return False
    spot = ranged_spot if role == "ranged" else PHASE_3_MELEE_SPOT
    if math.dist(where[:2], spot[:2]) <= PHASE_3_STATION_RADIUS:
        return True
    if moving or not target:
        return False
    if role in ("ranged", "heal"):
        return math.dist(where, target[:3]) - target[3] - PLAYER_COMBAT_REACH <= SPELL_DISTANCE
    return math.dist(where, target[:3]) - target[3] <= MELEE_REACH


def skull_tick_states(ticks: list[tuple[int, int]], rows: dict[int, list[list]]) -> collections.Counter:
    """What each skull gaze tick, as (t, victim), found its victim doing on its last snapshot_rows row
    at or before it: walking, still with a target, or still without one."""
    states: collections.Counter = collections.Counter()
    for when, victim in ticks:
        row = row_before(rows.get(victim, []), when)
        if not row:
            continue
        if row[9]:
            states["walking"] += 1
        elif row[8]:
            states["still with a target"] += 1
        else:
            states["still without"] += 1
    return states


def held_still(rows: list[list], when: int, radius: float) -> float:
    """Seconds the bot had been standing within `radius` of where it was at `when`."""
    spot = None
    start = when
    for row in reversed([row for row in rows if row[0] <= when]):
        if spot is None:
            spot = (row[2], row[3])
        if math.dist((row[2], row[3]), spot) > radius:
            break
        start = row[0]
    return (when - start) / 1000.0


def row_before(rows: list[list], when: int) -> list | None:
    stamps = [row[0] for row in rows]
    index = bisect.bisect_right(stamps, when)
    return rows[index - 1] if index else None


def crush_aim_row(rows: list[list], when: int, hit_xy: tuple[float, float],
                  lag_ms: int = CRUSH_AIM_LAG_MS) -> list | None:
    """The Crusher row naming who a swing at `when` was aimed at: of the last sample before the hit and
    the first within `lag_ms` after it, the one facing nearest `hit_xy`, where the cone landed."""
    stamps = [row[0] for row in rows]
    index = bisect.bisect_left(stamps, when)
    candidates = []
    if index:
        candidates.append(rows[index - 1])
    if index < len(rows) and rows[index][0] - when <= lag_ms:
        candidates.append(rows[index])
    if not candidates:
        return None

    def off_facing(row: list) -> float:
        bearing = math.atan2(hit_xy[1] - row[3], hit_xy[0] - row[2])
        return abs((bearing - row[5] + math.pi) % (2 * math.pi) - math.pi)

    return min(candidates, key=off_facing)


def crush_blame(walks: list[dict], guid: int, when: int, spot: tuple[float, float], on_victim: bool,
                window_ms: int = CRUSH_WALK_BLAME_MS) -> dict | None:
    """The walk that had the bot moving in the last `window_ms` before a Crush, or None if it stood
    still. A `wait` was refused behind a walk still in flight, so it moved nobody itself but says the
    last accepted walk was still going, however long ago it was issued. For the tentacle's own victim
    only a walk that ended in its reach counts, since standing there is what made it the victim."""
    blamed = None
    in_flight = False
    for rec in walks:
        if rec.get("g") != guid or rec["t"] > when:
            continue
        if rec.get("r") == "wait":
            in_flight = in_flight or when - rec["t"] <= window_ms
            continue
        if on_victim and math.dist((rec.get("x", 0), rec.get("y", 0)), spot) > CRUSHER_MELEE_RANGE:
            continue
        blamed = rec
    if blamed and (when - blamed["t"] <= window_ms or in_flight):
        return blamed
    return None


def crusher_alive_spans(trace: Trace, crushers: set) -> dict[int, tuple[int, int]]:
    """First and last snapshot each Crusher was sampled alive."""
    spans: dict[int, list[int]] = {}
    for guid, rows in snapshot_rows(trace, crushers).items():
        alive = [row[0] for row in rows if row[6] > 0]
        if alive:
            spans[guid] = [alive[0], alive[-1]]
    return {guid: (start, end) for guid, (start, end) in spans.items()}


def show_crush_hits(trace: Trace) -> None:
    """Every Crush split by who the tentacle was swinging at. A hit on its own victim is a bot inside
    its melee range that hit it, and for anyone but the tank walking out is what would have stopped
    it. A hit on anyone else is the cone, and the angle is the answer there."""
    crushers = guids_of_entry(trace, NPC_CRUSHER_TENTACLE)
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_CRUSH_CONE and rec.get("s") in crushers]
    if not hits:
        return

    roster = roster_guids(trace)
    rows = snapshot_rows(trace, crushers | roster)
    walks = [rec for rec in trace.of("move") if rec.get("g") in roster]
    melee_hits = [rec for rec in trace.of("cast")
                  if rec.get("tgt") in crushers and trace.spells.get(rec.get("sp"), "") in MELEE_CLASS_NAMES]

    split = collections.Counter()
    kills = []
    for hit in hits:
        victim = row_before(rows.get(hit.get("d"), []), hit["t"])
        crusher = crush_aim_row(rows.get(hit["s"], []), hit["t"], (victim[2], victim[3])) if victim else \
            row_before(rows.get(hit["s"], []), hit["t"])
        aimed_at = crusher[8] if crusher and len(crusher) > 8 else None
        on_victim = aimed_at == hit.get("d")
        role = trace.role(hit.get("d"))
        split[("victim" if on_victim else "cone", "tank" if role == "tank" else "other")] += 1
        if hit.get("ok", 0) <= 0 or not crusher or not victim:
            continue

        spot = (crusher[2], crusher[3])
        gap = math.dist(spot, (victim[2], victim[3]))
        walked = crush_blame(walks, hit["d"], hit["t"], spot, on_victim)
        swung = [trace.spells.get(rec["sp"], "") for rec in melee_hits
                 if rec.get("s") == hit["d"] and rec["tgt"] == hit["s"] and hit["t"] - 3000 <= rec["t"] <= hit["t"]]
        kills.append((hit["t"], hit["d"], role, gap, on_victim, aimed_at, walked, swung))

    parts = ", ".join(f"{kind} ({who}) {count}" for (kind, who), count in sorted(split.items()))
    print(f"  Crush hits by who the tentacle was swinging at: {parts}")

    # Crush only fires on the tentacle's melee swing, so whoever takes most of it is whoever stands in
    # the reach. Nothing in the fight wants a tank in there: an unoccupied Crusher cannot Crush at all.
    taken: collections.Counter = collections.Counter()
    damage: collections.Counter = collections.Counter()
    for hit in hits:
        taken[hit.get("d")] += 1
        damage[hit.get("d")] += hit.get("a", 0)
    total = sum(damage.values()) or 1
    on_tanks = sum(value for guid, value in damage.items() if trace.role(guid) == "tank")
    print(f"  Crush damage by victim: {sum(damage.values()):,} over {len(hits)} hits,"
          f" {on_tanks * 100.0 / total:.0f}% of it on tanks")
    for guid, count in taken.most_common(6):
        print(f"    {trace.name(guid):14} {trace.role(guid):6} {count:3d} hits {damage[guid]:9,d}")
    for when, guid, role, gap, on_victim, aimed_at, walked, swung in kills:
        how = f"walked in by {walked.get('by', '?')}" if walked else "stood still"
        hit_first = f", hit it first with {', '.join(sorted(set(swung)))}" if swung else ""
        kind = "its victim" if on_victim else f"cone at {trace.name(aimed_at)}"
        print(f"    killed {clock(when):>10} {trace.name(guid):14} {role:6} {gap:4.1f} yd, {kind}, {how}{hit_first}")

    # A cone needs something in the reach to swing at, and a pet there is somebody else's death.
    cones = [aimed_at for _, _, _, _, on_victim, aimed_at, _, _ in kills if not on_victim]
    if cones:
        on_pets = sum(1 for aimed_at in cones if aimed_at in trace.owners)
        print(f"  cone kills aimed at a pet: {on_pets} of {len(cones)}")


def show_crusher_reach(trace: Trace) -> None:
    """Ranged and healers inside a Crusher's melee range. Nothing but a melee hit makes a bot its
    victim, and from there every swing is a Crush on it, so standing in there is the whole risk."""
    crushers = guids_of_entry(trace, NPC_CRUSHER_TENTACLE)
    if not crushers:
        return

    backline = {guid for guid, role in trace.roles.items() if role in ("ranged", "heal")}
    inside: collections.Counter = collections.Counter()
    for snap in trace.of("snap"):
        units = {row[0]: row for row in snap.get("u", [])}
        live = [(row[1], row[2]) for guid, row in units.items() if guid in crushers and row[5] > 0]
        if not live:
            continue
        for guid in backline & units.keys():
            row = units[guid]
            if row[5] > 0 and min(math.dist((row[1], row[2]), spot) for spot in live) <= CRUSHER_MELEE_RANGE:
                inside[guid] += 1

    print(f"  ranged/heal samples inside {CRUSHER_MELEE_RANGE:.0f} yd of a live Crusher: {sum(inside.values())}")
    if inside:
        print("    " + ", ".join(f"{trace.name(guid)} {count}" for guid, count in inside.most_common(6)))

    rows = snapshot_rows(trace, crushers)
    walks_in: collections.Counter = collections.Counter()
    for rec in trace.of("move"):
        if rec.get("g") not in backline:
            continue
        spots = [(row[2], row[3]) for row in (row_before(rows[guid], rec["t"]) for guid in rows)
                 if row and row[6] > 0 and rec["t"] - row[0] < 1000]
        if spots and min(math.dist((rec.get("x", 0), rec.get("y", 0)), spot) for spot in spots) <= CRUSHER_MELEE_RANGE:
            walks_in[rec.get("by", "?")] += 1
    if walks_in:
        print(f"  ranged/heal walks aimed inside it: {dict(walks_in.most_common(6))}")

    swings = collections.Counter(
        (trace.name(rec["s"]), trace.spells.get(rec["sp"], "")) for rec in trace.of("cast")
        if rec.get("tgt") in crushers and rec.get("s") in backline and trace.spells.get(rec.get("sp"), "") in MELEE_CLASS_NAMES)
    if swings:
        print(f"  melee-class hits by ranged/heal on a Crusher: {sum(swings.values())}  "
              + ", ".join(f"{who} {spell} {count}" for (who, spell), count in swings.most_common(6)))


def show_diminish_power(trace: Trace) -> None:
    """How long Diminish Power was actually up, read off the aura on the raid, and whether a paladin's
    Judgement ever broke it. 64148 on the tentacle breaks the channel on any taken melee-class hit, and
    every Judgement's damage is melee class, from about 19.5 yd out."""
    crushers = guids_of_entry(trace, NPC_CRUSHER_TENTACLE)
    alive = crusher_alive_spans(trace, crushers)
    if not alive:
        return

    end = trace.records[-1].get("t", 0) if trace.records else 0
    open_at: dict[tuple[int, int], int] = {}
    per_crusher: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in sorted((r for r in trace.of("aura") if r.get("sp") == SPELL_DIMINISH_POWER), key=lambda r: r["t"]):
        key = (rec.get("s", 0), rec.get("d", 0))
        if not rec.get("r"):
            open_at.setdefault(key, rec["t"])
        elif key in open_at:
            per_crusher[key[0]].append((open_at.pop(key), rec["t"]))
    for (source, _), start in open_at.items():
        per_crusher[source].append((start, end))
    channels = {guid: merge_spans(spans) for guid, spans in per_crusher.items()}

    up = sum(stop - start for spans in channels.values() for start, stop in merge_spans(spans))
    up_any = sum(stop - start for start, stop in merge_spans([s for spans in channels.values() for s in spans]))
    lived = sum(stop - start for start, stop in alive.values())
    breaks = sum(max(0, len(spans) - 1) for spans in channels.values())
    share = up * 100.0 / lived if lived else 0.0
    print(f"  Diminish Power channel: up {up / 1000:.0f} s of {lived / 1000:.0f} s Crusher-alive ({share:.0f}%),"
          f" on the raid {up_any / 1000:.0f} s, {breaks} breaks")

    paladins = {row["g"] for row in trace.header.get("roster", []) if row.get("c") == CLASS_PALADIN}
    judgements = [rec for rec in trace.of("cast")
                  if rec.get("sp") in JUDGEMENT_SPELLS and rec.get("s") in paladins and rec.get("tgt") in crushers]
    if not judgements:
        print("  no paladin Judgement reached a Crusher")
        return

    mid, broke = 0, 0
    for rec in judgements:
        spans = channels.get(rec["tgt"], [])
        before = rec["t"] - JUDGEMENT_LEAD_MS
        if not any(start <= before < stop for start, stop in spans):
            continue
        mid += 1
        if any(before <= stop <= rec["t"] + JUDGEMENT_BREAK_MS for _, stop in spans):
            broke += 1
    who = collections.Counter(trace.name(rec["s"]) for rec in judgements)
    print(f"  Judgements at a Crusher: {len(judgements)} ({dict(who)}), {mid} while it was channelling,"
          f" {broke} followed by the channel dropping within {JUDGEMENT_BREAK_MS} ms")


def closest_approach(start: tuple, end: tuple, point: tuple) -> float:
    """How near the straight walk from start to end passes to point, in 2d."""
    leg_x, leg_y = end[0] - start[0], end[1] - start[1]
    leg_squared = leg_x * leg_x + leg_y * leg_y
    along = 0.0
    if leg_squared > 0:
        along = ((point[0] - start[0]) * leg_x + (point[1] - start[1]) * leg_y) / leg_squared
        along = max(0.0, min(1.0, along))
    return math.dist((start[0] + leg_x * along, start[1] + leg_y * along), point[:2])


def body_launches(rows: list[list]) -> list[list]:
    """The rows, snapshot_rows shaped, on which this unit's flight off the body began: effect motion
    where the sample before was not, within LAUNCH_RADIUS of the body, on the platform."""
    launched = []
    for before, after in zip(rows, rows[1:]):
        if after[10] != EFFECT_MOTION_TYPE or before[10] == EFFECT_MOTION_TYPE:
            continue
        if PLATFORM_Z[0] < before[4] < PLATFORM_Z[1] and math.dist(before[2:4], BODY) < LAUNCH_RADIUS:
            launched.append(after)
    return launched


def show_launches(trace: Trace) -> None:
    """Who the body actually threw, and what walked them in. 64020 deals no damage, so a launch only
    shows up in the movement.

    The walk is the other half. Every other read of the ring asks where the bot is standing, so the
    one thing none of them can catch is a walk that crosses the body - a straight line to a spot on
    the far side, or a spot behind a target that sits inside the ring. The blame goes to the last walk
    issued before the throw whose line passed inside the ring, not the one after it: by the time the
    pulse lands a dodge node has usually already issued the walk back out."""
    roster = roster_guids(trace)
    rows = snapshot_rows(trace, roster)
    spans = phase_spans(trace)
    walks: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("g") in roster and rec.get("ok") == 1:
            walks[rec["g"]].append(rec)

    launches = []
    for guid, own in rows.items():
        stamps = [row[0] for row in own]
        for row in body_launches(own):
            blame = "nothing walked it in"
            for rec in reversed(walks[guid]):
                if rec["t"] > row[0]:
                    continue
                if row[0] - rec["t"] > LAUNCH_BLAME_MS:
                    break
                index = bisect.bisect_right(stamps, rec["t"])
                if not index:
                    break
                origin = own[index - 1]
                if closest_approach(origin[2:4], (rec.get("x", 0), rec.get("y", 0)), BODY) < KNOCKBACK_RADIUS:
                    blame = rec.get("by", "?")
                    break
            phase = next((phase for phase, first, last in spans if first <= row[0] < last), 0)
            launches.append((row[0], guid, phase, blame))

    if not launches:
        print("  nothing was thrown by the body ring")
        return

    per_phase = collections.Counter(phase for _, _, phase, _ in launches)
    print(f"  thrown by the body ring: {len(launches)}"
          f" ({', '.join(f'{PHASE_NAMES.get(phase, phase)} {count}' for phase, count in sorted(per_phase.items()))})")
    for phase in sorted(per_phase):
        mine = [entry for entry in launches if entry[2] == phase]
        blamed = collections.Counter(entry[3] for entry in mine)
        bots = collections.Counter(trace.name(entry[1]) for entry in mine)
        label = PHASE_NAMES.get(phase, phase)
        print(f"    {label} walked in by: {', '.join(f'{by} {n}' for by, n in blamed.most_common())}")
        print(f"    {label} per bot     : {', '.join(f'{name} {n}' for name, n in bots.most_common())}")
    for when, guid, _, by in sorted(launches)[:10]:
        print(f"    {clock(when):>10} {trace.name(guid):14} {trace.role(guid):6} walked in by {by}")


def show_crush_melee(trace: Trace) -> None:
    """What stood inside a Crusher's melee range. Crush is a 100% proc on the tentacle's own
    white swing and UpdateAI will not swing at a victim out of melee range, so whatever is in there is
    what fires every cone the raid eats - and a pet counts."""
    crushers = guids_of_entry(trace, NPC_CRUSHER_TENTACLE)
    if not crushers:
        return

    roster = roster_guids(trace)
    closest: dict[int, float] = {}
    for snap in trace.of("snap"):
        spots = {row[0]: (row[1], row[2], row[5]) for row in snap.get("u", [])}
        live = [spot for guid, spot in spots.items() if guid in crushers and spot[2] > 0]
        if not live:
            continue
        for guid, spot in spots.items():
            if guid not in roster and guid not in trace.owners:
                continue
            gap = min(math.dist((spot[0], spot[1]), (other[0], other[1])) for other in live)
            closest[guid] = min(closest.get(guid, gap), gap)

    inside = sorted((gap, guid) for guid, gap in closest.items() if gap <= CRUSHER_MELEE_RANGE)
    if not inside:
        print(f"  nothing came inside {CRUSHER_MELEE_RANGE:.0f} yd of a Crusher")
        return

    # Standing there is half of it; being pointed at it is what keeps a pet coming back, since PetAI
    # re-picks from its own attacker and its owner's victim the moment it has no target of its own.
    aimed: dict[int, int] = collections.Counter()
    for snap in trace.of("snap"):
        for row in snap.get("u", []):
            if len(row) > 7 and row[7] in crushers and row[0] in trace.owners:
                aimed[row[0]] += 1

    print(f"  inside {CRUSHER_MELEE_RANGE:.0f} yd of a Crusher ({len(inside)}):")
    for gap, guid in inside[:8]:
        kind = trace.role(guid) if guid in roster else "pet"
        print(f"    {trace.name(guid):28} {kind:6} {gap:5.1f} yd")

    if aimed:
        print("  pets with a Crusher targeted:")
        for guid, count in aimed.most_common(6):
            print(f"    {trace.name(guid):28} {count:5} samples")


# What each source takes off a 25-man Sanity bar, from spell_yogg_saron_sanity_reduce. Induce Madness
# is not in here: it leaves no damage or aura row of its own, so the Insane aura is the only record
# that it landed, and what it takes is the whole bar.
SANITY_COSTS = {65301: 12, 63830: 3, 63881: 3, 63803: 2, 64168: 2, 64164: 4}
SANITY_NAMES = {65301: "Psychosis", 63830: "Malady of the Mind", 63881: "Malady trigger",
                63803: "Brain Link", 64168: "Lunatic Gaze (skull)", 64164: "Lunatic Gaze (Yogg)"}


def show_sanity_sources(trace: Trace) -> None:
    """Where the Sanity went, reconstructed from the spells that take it. Deduplicated on
    (time, spell, target) because one landed hit can leave both a dmg and an aura row."""
    roster = roster_guids(trace)
    lost: dict[int, int] = collections.Counter()
    seen: set[tuple[int, int, int]] = set()
    for rec in trace.of("dmg", "aura"):
        spell, victim = rec.get("sp"), rec.get("d")
        if spell not in SANITY_COSTS or victim not in roster:
            continue
        if rec.get("e") == "aura" and rec.get("r"):
            continue
        key = (rec["t"], spell, victim)
        if key in seen:
            continue
        seen.add(key)
        lost[spell] += SANITY_COSTS[spell]

    charmed, _, caught, _ = madness_split(trace)
    madness = len(caught) * 100

    total = sum(lost.values()) + madness
    if not total:
        return

    print("  where it went:")
    rows = [("Induce Madness", madness)] if madness else []
    rows += [(SANITY_NAMES[spell], stacks) for spell, stacks in lost.items()]
    for label, stacks in sorted(rows, key=lambda row: -row[1]):
        print(f"    {label:22} {stacks:5}  {stacks * 100.0 / total:4.1f}%")
    print(f"    {'total':22} {total:5}")
    if charmed:
        drained = len(charmed) - len(caught)
        print(f"    Insane: {len(caught)} caught below by Induce Madness (100 each), {drained} drained to 0")


def show_sanity(trace: Trace) -> None:
    print("SANITY")
    rows = [rec for rec in trace.of("aura") if rec.get("sp") == SPELL_SANITY]
    if not rows:
        print("  no 63050 rows - Sanity is seeded once at the pull and never re-applied,"
              " so an empty read here means the pull never started")
        return

    low: dict[int, int] = {}
    for rec in rows:
        # A removal row carries no stack count. Counting it as zero reads every bot as Insane.
        if rec.get("r"):
            continue
        guid, stacks = rec.get("d", 0), rec.get("st", 0)
        low[guid] = min(low.get(guid, stacks), stacks)

    for guid in sorted(low, key=lambda g: low[g]):
        print(f"  {trace.name(guid):18} {trace.role(guid):7} low {low[guid]:3}")

    insane = [guid for guid, stacks in low.items() if stacks <= 0]
    if insane:
        print(f"  went Insane: {', '.join(trace.name(g) for g in insane)}")

    show_sanity_trips(trace, rows)
    show_sanity_sources(trace)
    show_res_into_insane(trace)


def show_sanity_trips(trace: Trace, rows: list[dict]) -> None:
    """Each bot's trips below as Sanity going down > lowest, then what it got back before the next one,
    plus who sat a wave out under the brain team's floor."""
    series: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in rows:
        if not rec.get("r") and rec.get("st") is not None:
            series[rec.get("d", 0)].append((rec["t"], rec["st"]))

    heights = snapshot_rows(trace, set(series))
    lines = []
    for guid, track in heights.items():
        trips = sanity_trips([(row[0], row[4]) for row in track], series[guid], BRAIN_LEVEL_Z)
        if not trips:
            continue
        parts = []
        for index, (down, up, at_down, low, at_up) in enumerate(trips):
            part = f"{at_down}>{low}"
            nxt = trips[index + 1][0] if index + 1 < len(trips) else None
            if nxt is not None and at_up is not None:
                peak = max([stacks for t, stacks in series[guid] if up <= t <= nxt] + [at_up])
                if peak > at_up:
                    part += f" +{peak - at_up}"
            parts.append(part)
        lines.append((trips[0][0], f"    {trace.name(guid):14} {trace.role(guid):6} {', '.join(parts)}"))

    if lines:
        print("  trips below, Sanity down > lowest, then what came back before the next:")
        for _, line in sorted(lines):
            print(line)

    benched = collections.Counter(trace.name(rec.get("g", 0)) for rec in notes(trace, "yogg.benched")
                                  if rec.get("txt") == "1")
    if benched:
        print(f"  benched under the brain team floor: {dict(benched)}")


def show_res_into_insane(trace: Trace) -> None:
    """Rebirths spent on a corpse that died without Sanity, which the Voice charms again on its feet."""
    rebirth = {spell for spell, name in trace.spells.items() if name == "Rebirth"}
    casts = [(rec["t"], rec.get("s", 0), rec.get("tgt", 0)) for rec in trace.of("cast") if rec.get("sp") in rebirth]
    if not casts:
        return
    deaths = sorted((rec["t"], rec.get("g", 0), any(aura[0] == SPELL_SANITY for aura in rec.get("auras", [])))
                    for rec in death_records(trace))
    insane = sorted((rec["t"], rec.get("d", 0)) for rec in trace.of("aura")
                    if rec.get("sp") == SPELL_INSANE and not rec.get("r"))
    wasted = res_into_insane(casts, deaths, insane)
    print(f"  Rebirth: {len(casts)} cast, {len(wasted)} on a corpse with no Sanity")
    for when, caster, target, again in wasted:
        tail = f", Insane again {again:.1f} s later" if again is not None else ""
        print(f"    {clock(when)} {trace.name(caster)} -> {trace.name(target)}{tail}")


def show_threat(trace: Trace) -> None:
    """Who the Guardians were actually beating on, and what the raid spent trying to change that.

    The answer is not "nothing was tried": Misdirection goes to the tank and the tank taunts. It is
    that one tank's taunt budget is roughly one cast per Guardian death, against a room where every
    Guardian carries its own threat table from SetInCombatWithZone, so the question worth asking is
    whether the budget was spent on the Guardian whose death location decides the phase.
    """
    print("THREAT")
    spans = phase_spans(trace)
    p1_end = phase1_end(spans)
    if p1_end is None:
        print("  no phase 1 in this trace")
        return

    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    roster = roster_guids(trace)
    tanks = {guid for guid in roster if trace.role(guid) == "tank"}

    held, _ = threat_share(trace, guardians, lambda when: 0 <= when <= p1_end)
    if sum(held.values()):
        show_share(held, "Guardian time on target")
    else:
        print("  no live Guardian was ever sampled with a target")

    split = focus_split(trace, guardians, roster, p1_end)
    if split:
        print(f"  bot non-tanks on one Guardian: {split[0]:4.1f}%   on two or more: {split[1]:4.1f}%"
              f"   (of the time any of them was on one)")

    show_tank_engagement(trace, p1_end, tanks)
    show_taunt_attempts(trace, p1_end, tanks)

    redirects = [rec for rec in trace.of("cast")
                 if rec.get("sp") in REDIRECT_SPELLS and 0 <= rec["t"] <= p1_end]
    if redirects:
        print(f"\n  redirects: {len(redirects)}")
        for rec in redirects:
            target = rec.get("tgt", 0)
            role = trace.role(target) if target in roster else "?"
            print(f"    {clock(rec['t']):>9}  {REDIRECT_SPELLS[rec['sp']]:<20}"
                  f" {trace.name(rec.get('s', 0)):<14} -> {trace.name(target)} ({role})")

    # A taunt is only worth its cooldown if it lands on the Guardian the raid is killing, which is the
    # lowest-health one. Ranking it against the living Guardians is the whole measurement.
    taunts = [rec for rec in trace.of("cast")
              if rec.get("sp") in TAUNT_SPELLS and 0 <= rec["t"] <= p1_end
              and rec.get("s") in roster]
    if not taunts:
        print("\n  no single-target taunt was cast in phase 1")
        return

    frames = [snap for snap in trace.of("snap") if 0 <= snap["t"] <= p1_end]

    print(f"\n  taunts: {len(taunts)}   (rank 1 = the lowest-health Guardian, the one the raid is on)")
    on_focus = 0
    for rec in taunts:
        target = rec.get("tgt", 0)
        snap = min(frames, key=lambda s: abs(s["t"] - rec["t"])) if frames else None
        live = sorted((row[5], row[0]) for row in (snap.get("u", []) if snap else [])
                      if row[0] in guardians and row[5] > 0)
        rank = next((i + 1 for i, (_, guid) in enumerate(live) if guid == target), 0)
        if rank == 1:
            on_focus += 1
        where = position_at(trace, target, rec["t"])
        radius = f"{math.dist(where, BODY):5.1f}" if where else "    ?"
        print(f"    {clock(rec['t']):>9}  {TAUNT_SPELLS[rec['sp']]:<18}"
              f" rank {rank or '?'}/{len(live):<2}  target {radius} yd out")
    print(f"    landed on the focus Guardian: {on_focus} of {len(taunts)}")


def stalled_damage(samples: list[tuple[int, int, int]]) -> tuple[int, int]:
    """Longest and total ms over (t, dealt, moving) where `dealt` never changed while `moving` was set.
    Damage frozen on a bot that is on the move every tick is a bot being walked somewhere each time it
    comes up, so it never stands still long enough to swing."""
    longest = total = run = 0
    previous: tuple[int, int] | None = None
    for when, dealt, moving in samples:
        if previous is not None and dealt == previous[1] and moving:
            run += when - previous[0]
        else:
            longest = max(longest, run)
            total += run
            run = 0
        previous = (when, dealt)

    return max(longest, run), total + run


def move_tug(rows: list[tuple[int, str]]) -> list[tuple[str, str, int]]:
    """Issuer pairs that took turns, and how many reversals each made, over (t, issuer) in time order.
    A reversal is one issuer's move, then another's, then the first one's again - a bot being pulled two
    ways rather than walked somewhere once."""
    counts: collections.Counter = collections.Counter()
    for first, second, third in zip(rows, rows[1:], rows[2:]):
        if first[1] != second[1] and first[1] == third[1]:
            counts[tuple(sorted((first[1], second[1])))] += 1

    return [(pair[0], pair[1], count) for pair, count in counts.most_common()]


def show_tank_engagement(trace: Trace, p1_end: int, tanks: set) -> None:
    """Whether a phase 1 tank ever stood still long enough to swing. Nothing here gives a tank a target
    of its own - the dps resolver stands down for one and the generic picker keeps whatever is already
    selected - so a stale selection across the room leaves the reach walking at it every tick."""
    for guid, samples in snapshot_rows(trace, tanks).items():
        inside = [(row[0], row[12], row[9]) for row in samples if 0 <= row[0] <= p1_end]
        if len(inside) < 2:
            continue

        longest, total = stalled_damage(inside)
        span = inside[-1][0] - inside[0][0] or 1
        print(f"\n  {trace.name(guid)}: damage frozen while moving for {total / 1000:5.1f} s of"
              f" {span / 1000:5.1f} ({100.0 * total / span:3.0f}%),"
              f" longest stretch {longest / 1000:5.1f} s")

        moves = [(rec["t"], rec.get("by") or "?") for rec in trace.of("move")
                 if rec.get("g") == guid and rec.get("ok") and 0 <= rec["t"] <= p1_end]
        for first, second, count in move_tug(moves)[:3]:
            print(f"    pulled both ways {count:3d} times: {first} against {second}")


def show_taunt_attempts(trace: Trace, p1_end: int, tanks: set) -> None:
    """The taunts that never left the gun, which the cast rows cannot show. The pick has no range test
    of its own, and all four class taunts reach 30 yd, so an out-of-range aim fails - and CastSpell
    selects before it validates range, so the tank is left holding what it could not reach."""
    attempts = [rec for rec in trace.of("act")
                if rec.get("a") in TAUNT_ACTIONS and rec.get("g") in tanks and 0 <= rec["t"] <= p1_end]
    if not attempts:
        return

    rows = snapshot_rows(trace, tanks)
    failed = out_of_reach = 0
    print(f"\n  taunt attempts: {len(attempts)}   (over {TAUNT_RANGE:.0f} yd the cast cannot land)")
    for rec in attempts:
        if rec.get("vd") != "OK":
            failed += 1

        aim = taunt_aim(trace, rows.get(rec["g"], []), rec["t"])
        reach = "?" if aim is None else f"{aim[1]:5.1f} yd"
        if aim and aim[1] > TAUNT_RANGE:
            out_of_reach += 1

        print(f"    {clock(rec['t']):>9}  {rec['a']:<18} {rec.get('vd', '?'):<10}"
              f" aimed {reach} away" + (f" at {trace.name(aim[0])}" if aim else ""))
    print(f"    failed: {failed} of {len(attempts)}, of which out of reach: {out_of_reach}")


def taunt_aim(trace: Trace, rows: list[list], when: int) -> tuple[int, float] | None:
    """What a taunt attempt was aimed at and how far off it was, from the tank's first snapshot at or
    after it: the selection is already set by then, whether the cast went out or not."""
    after = next((row for row in rows if row[0] >= when), None)
    if not after or not after[8]:
        return None

    spot = position_at(trace, after[8], after[0])
    return (after[8], math.dist((after[2], after[3]), spot)) if spot else None


def focus_split(trace: Trace, guardians: set, roster: set, p1_end: int) -> tuple[float, float] | None:
    """Share of phase 1 the bot non-tanks spent on one live Guardian against two or more, weighted by
    the gap to the next snapshot. Two at once is how a pair comes down in lockstep. Humans are left out:
    nothing picks their target."""
    frames = [snap for snap in trace.of("snap") if 0 <= snap["t"] <= p1_end]
    one = many = 0
    for snap, following in zip(frames, frames[1:]):
        rows = snap.get("u", [])
        live = {row[0] for row in rows if row[0] in guardians and row[5] > 0}
        targets = {row[7] for row in rows
                   if row[0] in roster and row[0] not in trace.humans and len(row) > 7
                   and trace.role(row[0]) != "tank" and row[7] in live}
        span = following["t"] - snap["t"]
        if len(targets) == 1:
            one += span
        elif len(targets) > 1:
            many += span

    total = one + many
    return (one * 100.0 / total, many * 100.0 / total) if total else None


def separation(origin: tuple, first: tuple, second: tuple) -> float:
    """Degrees between two points as seen from origin, 0 to 180, on the ground plane."""
    one = math.atan2(first[1] - origin[1], first[0] - origin[0])
    two = math.atan2(second[1] - origin[1], second[0] - origin[0])
    gap = abs(one - two) % (2 * math.pi)
    return math.degrees(min(gap, 2 * math.pi - gap))


def facing_away(orientation: float, origin: tuple, source: tuple) -> bool:
    """Whether a unit at origin facing orientation has source outside its front 180 degrees, which is
    all Lunatic Gaze tests (HasInArc(M_PI, caster) on the victim)."""
    bearing = math.atan2(source[1] - origin[1], source[0] - origin[0])
    gap = abs(orientation - bearing) % (2 * math.pi)
    return min(gap, 2 * math.pi - gap) > math.pi / 2


def target_kind(target: int, entry: int | None) -> str:
    """What a snapshot's target column points at, in phase 3 terms."""
    if not target:
        return "none"
    if entry == NPC_IMMORTAL_GUARDIAN:
        return "guardian"
    if entry in (NPC_CRUSHER_TENTACLE, NPC_CONSTRICTOR_TENTACLE, NPC_CORRUPTOR_TENTACLE):
        return "tentacle"
    return "yogg" if entry == NPC_YOGG_SARON else "other"


def first_off_tank(rows: list[list], tanks: set) -> list | None:
    """The first snapshot_rows row on which this Guardian had a victim that was not a tank."""
    return next((row for row in rows if row[8] and row[8] not in tanks), None)


def on_cooldown(casts: list[int], when: int, cooldown_ms: int) -> bool:
    """Whether the last of these sorted cast times before `when` is still inside the cooldown."""
    index = bisect.bisect_right(casts, when)
    return bool(index) and when - casts[index - 1] < cooldown_ms


def damage_band(entry: int | None, hp: float | None, in_gaze: bool) -> str:
    """Which phase 3 bucket a hit landed in, by its target's entry and health just before. Mirrors the
    module's split: Guardians over the floor, between Weakened and the floor (inside a gaze or not),
    Weakened, Yogg and the leftover tentacles."""
    if entry == NPC_YOGG_SARON:
        return "yogg"
    if entry in TENTACLE_NAMES:
        return "tentacle"
    if entry == NPC_IMMORTAL_GUARDIAN:
        if hp is None:
            return "guardian ?"
        if hp > GUARDIAN_FLOOR_PCT:
            return "over 30"
        if hp > GUARDIAN_WEAKENED_PCT:
            return "10-30 gaze" if in_gaze else "10-30"
        return "weakened"
    return "other"


def roar_verdict(pressed: bool, silenced: int) -> str:
    """'covered' when Aura Mastery went out and nobody was silenced, 'partial' when raiders were
    silenced anyway, 'uncovered' with no press."""
    if not pressed:
        return "uncovered"
    return "covered" if silenced == 0 else "partial"


def brain_hold_window(rows: list[tuple[int, str]]) -> tuple[int, int | None, str] | None:
    """(held from, let go at, why) over one Induce Madness window's `yogg.brainhold` notes in time order,
    every bot's together, or None when nobody held. Held is the first hold or dots; let go is the first
    finish or clear after it, or None and "never"."""
    held = next((when for when, txt in rows if txt in ("hold", "dots")), None)
    if held is None:
        return None
    for when, txt in rows:
        if when >= held and txt in ("finish", "clear"):
            return held, when, txt
    return held, None, "never"


def taunt_outcome(rows: list[list], when: int, caster: int, spans: list[tuple[int, int]] | None = None,
                  check_ms: int = BEACON_TAUNT_CHECK_MS) -> str:
    """How a drag taunt at `when` read `check_ms` later, as the module reads it: `took` when the Guardian's
    victim (row[8]) was the caster, `didn't take` otherwise, `-` with no fresh row, and `channelling` inside
    one of its Drain Life `spans` (start, end) that began before the taunt landed, since a casting Guardian
    skips its victim update. A channel begun later doesn't hide the switch."""
    check = when + check_ms
    if any(first <= when + BEACON_TAUNT_LAND_MS and check <= last for first, last in spans or ()):
        return "channelling"
    index = bisect.bisect_left([row[0] for row in rows], check)
    if index >= len(rows) or rows[index][0] - check > TAUNT_ROW_SLACK_MS:
        return "-"
    return "took" if rows[index][8] == caster else "didn't take"


def mark_drains(channels: list[tuple[int, int, int]], guardian: int, first: int, last: int) -> tuple[float, int, int]:
    """One marked Guardian's Drain Life between its mark and the heal: (seconds channelled inside
    [first, last], channels, of those cut before DRAIN_FULL_MS). Channels are (start, caster, ms)."""
    seconds = 0.0
    count = cut = 0
    for began, caster, ms in channels:
        if caster != guardian or began + ms < first or began > last:
            continue
        count += 1
        cut += ms < DRAIN_FULL_MS
        seconds += (min(began + ms, last) - max(began, first)) / 1000
    return seconds, count, cut


def stage_gaps(picked: list[tuple[str, tuple[float, float] | None]]) -> list[str]:
    """Each stager, in lane order, and how far it stood from its lane's staging or hunter spot, whichever
    is nearer, at the mark. `picked` is (name, where it stood or None)."""
    out = []
    for lane, (name, spot) in enumerate(picked[:len(BEACON_LANES)]):
        if spot is None:
            out.append(f"{name} {BEACON_LANES[lane]} -")
            continue
        gap = min(math.dist(spot, BEACON_STAGE_SPOTS[lane]), math.dist(spot, BEACON_HUNTER_SPOTS[lane]))
        out.append(f"{name} {BEACON_LANES[lane]} {gap:.1f} yd")
    return out


def heal_cut_seconds(samples: list[tuple[int, str]], first: int, last: int) -> tuple[float, float, float, float]:
    """Guardian-seconds from `yogg.healcut` samples between `first` and `last`: (beacon heal under a cut,
    beacon heal in all, Drain Life under a cut, Drain Life in all). Each sample, "hot a/b drain c/d", holds
    until the next one."""
    totals = [0.0, 0.0, 0.0, 0.0]
    for index, (when, text) in enumerate(samples):
        until = samples[index + 1][0] if index + 1 < len(samples) else last
        span = (min(until, last) - max(when, first)) / 1000
        parts = text.split()
        if span <= 0 or len(parts) != 4:
            continue
        hot_cut, hot = (int(value) for value in parts[1].split("/"))
        drain_cut, drain = (int(value) for value in parts[3].split("/"))
        for slot, value in enumerate((hot_cut, hot, drain_cut, drain)):
            totals[slot] += value * span
    return totals[0], totals[1], totals[2], totals[3]


def heal_cut_line(seconds: tuple[float, float, float, float]) -> str:
    hot_cut, hot, drain_cut, drain = seconds
    parts = []
    if hot:
        parts.append(f"beacon heal {hot_cut / hot * 100:.0f}% of {hot:.0f} Guardian-s")
    if drain:
        parts.append(f"Drain Life {drain_cut / drain * 100:.0f}% of {drain:.0f} Guardian-s")
    return ", ".join(parts) or "nothing healing"


def beacon_heal_rises(series: list[tuple[int, float]], heals: list[int], look_ms: int = EMPOWERING_SHADOWS_MS,
                      rise_pct: float = LEFTOVER_BEACON_RISE_PCT) -> int:
    """How many of the beacon heals at `heals` a tentacle took: its health rose at least `rise_pct` in the
    `look_ms` after one. `series` is (t, hp) in time order."""
    count = 0
    for when in heals:
        then = next((hp for t, hp in reversed(series) if t <= when), None)
        later = max((hp for t, hp in series if when < t <= when + look_ms), default=None)
        if then is not None and later is not None and later - then >= rise_pct:
            count += 1
    return count


def drag_exposure(hits: list[tuple[int, int, int]], heal_at: int, window_ms: int) -> dict[int, tuple[int, int, int]]:
    """Per victim, (total, biggest, count) of one Guardian's hits from its beacon heal to `window_ms`
    after. Hits are (t, victim, amount)."""
    out: dict[int, tuple[int, int, int]] = {}
    for when, victim, amount in hits:
        if heal_at <= when <= heal_at + window_ms:
            total, biggest, count = out.get(victim, (0, 0, 0))
            out[victim] = (total + amount, max(biggest, amount), count + 1)
    return out


def station_arrival(rows: list[list], spots: list[tuple], start: int,
                    radius: float = PHASE_3_STATION_RADIUS) -> int | None:
    """Ms from `start` to the first live snapshot_rows row on the platform within `radius` of any of
    `spots`, which is where the module stops walking a bot to its phase 3 station, or None."""
    for row in rows:
        if (row[0] >= start and row[6] > 0 and row[4] >= BRAIN_LEVEL_Z
                and any(math.dist(row[2:4], spot[:2]) <= radius for spot in spots)):
            return row[0] - start
    return None


def stacked_heals(positions: list[tuple[float, float]], radius: float = EMPOWERING_SHADOWS_RADIUS) -> list[int]:
    """The beacon heals each marked Guardian took at the cast: its own, plus one per other marked
    Guardian within `radius`. 64486 stacks per caster."""
    return [1 + sum(1 for other, there in enumerate(positions) if other != index and math.dist(here, there) <= radius)
            for index, here in enumerate(positions)]


def flip_cause(victim_role: str, spells: set[str]) -> str:
    """Why a Guardian left its tank, from the spell names its new victim cast at it just before: `dot`
    when all were DoTs, `aoe` when any hit an area, `direct` otherwise, and with nothing cast `heal
    threat` for a healer and `none` for anyone else (ground AoE ticks, totems, pets)."""
    if not spells:
        return "heal threat" if victim_role == "heal" else "none"
    if spells <= FLIP_DOTS:
        return "dot"
    return "aoe" if spells & FLIP_AREA else "direct"


def area_worth(target: tuple[float, float], units: list[tuple[int, float, float, float, bool]],
               radius: float = PHASE_3_AREA_RADIUS) -> tuple[int, bool]:
    """(worth hitting, a loose Guardian) round a target (x, y) by the module's phase 3 area rule. Units
    are (entry, x, y, health %, a tank is its victim). Worth: Yogg, leftover tentacles and Immortal
    Guardians over the floor that a tank holds. Loose: a Guardian in the area no tank holds."""
    worth = 0
    loose = False
    for entry, x, y, hp, on_tank in units:
        if math.dist(target, (x, y)) - COMBAT_REACH.get(entry, 0.0) > radius:
            continue
        if entry == NPC_IMMORTAL_GUARDIAN:
            loose = loose or not on_tank
            if hp <= GUARDIAN_FLOOR_PCT or not on_tank:
                continue
        worth += 1
    return worth, loose


def show_phase3_arrival(trace: Trace, start: int, rows: dict[int, list[list]], roster: set) -> None:
    """Seconds from phase 3 to each raider's station, by role and for the brain team, the Nitro Boosts
    spent on the walk, and how long the walk sat released behind a target the station can't reach."""
    nitro: dict[int, int] = collections.Counter(
        rec.get("s") for rec in trace.of("cast")
        if rec.get("sp") == SPELL_NITRO_BOOSTS_USE and start <= rec["t"] <= start + STATION_NITRO_WATCH_MS)
    released: dict[int, int] = collections.Counter()
    by_bot: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, "yogg.station"):
        if rec["t"] >= start:
            by_bot[rec.get("g")].append((rec["t"], str(rec.get("txt", ""))))
    groups: dict[str, list[tuple[int, int | None]]] = collections.defaultdict(list)
    ranged_spots = [PHASE_3_RANGED_SPOT] + [spot for _, spot in PHASE_3_RANGED_STANDBY_SPOTS]
    for guid in roster:
        own = rows.get(guid, [])
        then = row_before(own, start)
        if not then or then[6] <= 0:
            continue
        role = trace.role(guid)
        arrived = station_arrival(own, ranged_spots if role == "ranged" else [PHASE_3_MELEE_SPOT], start)
        # Change-only notes, so a released stretch runs to the next note or the arrival.
        cap = start + (arrived if arrived is not None else STATION_NITRO_WATCH_MS)
        stamps = by_bot.get(guid, [])
        for (when, txt), (after, _) in zip(stamps, stamps[1:] + [(cap, "")]):
            if txt == "released":
                released[guid] += max(0, min(after, cap) - when)
        groups["brain team" if then[4] < BRAIN_LEVEL_Z else role].append((guid, arrived))
    print("  station arrival    : seconds to within the station radius; Nitro in the first 30 s; released")
    for group in ("ranged", "melee", "heal", "tank", "brain team"):
        members = groups.get(group)
        if not members:
            continue
        times = [arrived / 1000 for _, arrived in members if arrived is not None]
        never = len(members) - len(times)
        boots = sum(nitro[guid] for guid, _ in members)
        held = [released[guid] / 1000 for guid, _ in members if released[guid]]
        timing = f"median {statistics.median(times):4.1f} worst {max(times):4.1f}" if times else "none arrived"
        print(f"    {group:10} {len(members):2}: {timing}{f', {never} never' if never else ''};"
              f" Nitro {boots}; released {len(held)}"
              + (f" (median {statistics.median(held):.1f} s)" if held else ""))
    picks = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.nitro") if rec["t"] >= start)
    if picks:
        print(f"    yogg.nitro       : {dict(picks)}")


def show_phase3_area(trace: Trace, start: int, stop: int, rows: dict[int, list[list]], roster: set,
                     tanks: set, guardians: set, yogg: int | None) -> None:
    """Phase 3 area casts by the bot non-tanks, by how many units round the caster's target were worth
    hitting by the module's rule, and whether a Guardian no tank held stood in the area."""
    tentacles = {guid for entry in TENTACLE_NAMES for guid in guids_of_entry(trace, entry)}
    units = snapshot_rows(trace, tentacles)
    units.update({guid: rows[guid] for guid in guardians | ({yogg} if yogg else set()) if guid in rows})
    stamps = {guid: [row[0] for row in own] for guid, own in units.items()}

    def fresh(guid: int, when: int) -> list | None:
        index = bisect.bisect_right(stamps.get(guid, []), when)
        row = units[guid][index - 1] if index else None
        return row if row and when - row[0] <= 1500 and row[6] > 0 else None

    counts: collections.Counter = collections.Counter()
    by_spell: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    loose_casts = 0
    for rec in trace.of("cast"):
        caster = rec.get("s")
        name = trace.spells.get(rec.get("sp"), "")
        # Triggered rows are the ticks of a channel or a ground effect, under the same name.
        if (name not in PHASE_3_AREA_SPELLS or rec.get("tr") or caster not in roster or caster in tanks
                or caster in trace.humans or not start <= rec["t"] <= stop):
            continue
        aimed = rec.get("tgt") or (row_before(rows.get(caster, []), rec["t"]) or [0] * 9)[8]
        there = fresh(aimed, rec["t"]) if aimed in units else None
        if not there:
            counts["no target"] += 1
            continue
        # Mind Sear never hits its own target.
        near = [(trace.entries.get(guid), row[2], row[3], row[6], row[8] in tanks)
                for guid in units if (name != "Mind Sear" or guid != aimed) and (row := fresh(guid, rec["t"]))]
        worth, loose = area_worth((there[2], there[3]), near)
        band = f"{min(worth, PHASE_3_AREA_MIN_WORTH)}{'+' if worth >= PHASE_3_AREA_MIN_WORTH else ''}"
        counts[band] += 1
        by_spell[name]["casts"] += 1
        by_spell[name]["under"] += worth < PHASE_3_AREA_MIN_WORTH
        loose_casts += loose
    total = sum(counts.values())
    if total:
        bands = "  ".join(f"{band}: {counts[band]}" for band in ("0", "1", "2", "3+", "no target") if counts[band])
        # From each unit's centre, where the module takes off its size, so the count is a floor: an
        # Empowered Guardian's size is several yards.
        print(f"\n  area spells        : {total} casts by non-tanks, worth hitting by centre distance round the"
              f" target (a lower bound) {bands}; a loose Guardian in the area {loose_casts}")
        print("    by spell (under 3): " + ", ".join(f"{name} {spell['casts']} ({spell['under']})" for name, spell in
                                               sorted(by_spell.items(), key=lambda item: -item[1]["casts"])))
    picks = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.p3area")
                                if start <= rec["t"] <= stop)
    if picks:
        print(f"    yogg.p3area      : {dict(picks)}")


def show_beacon_tanks(trace: Trace, heal_at: int, healed: set, rows: dict[int, list[list]], roster: set,
                      tanks: set, flows: list[tuple[int, int, int]], covers: dict[int, list[tuple[int, str]]],
                      died_at: dict[int, list[int]]) -> None:
    """One beacon heal's 20 s from the tanks' side: healed Guardians on each, damage taken against
    healing landed, defensives and externals, death; then the raid's damage on the healed Guardians
    and their health at the end. Flows are (t, tank, amount), damage negative and healing positive."""
    stop = heal_at + EMPOWERING_SHADOWS_MS
    for tank in sorted(tanks, key=trace.name):
        then = row_before(rows.get(tank, []), heal_at)
        if not then or then[6] <= 0:
            continue
        window = [amount for when, guid, amount in flows if guid == tank and heal_at <= when <= stop]
        taken = -sum(amount for amount in window if amount < 0)
        landed = sum(amount for amount in window if amount > 0)
        on_it = max((sum(1 for guid in healed if (row := row_before(rows.get(guid, []), when)) and row[8] == tank)
                     for when in range(heal_at, stop + 1, 2000)), default=0)
        spent = sorted({name for when, name in covers[tank] if heal_at - BEACON_SPLASH_LOOK_MS <= when <= stop})
        died = next((when for when in died_at[tank] if heal_at <= when <= stop), None)
        print(f"      tank {trace.name(tank):14}: up to {on_it} healed on it, took {taken / 1000:.0f}k, healed"
              f" {landed / 1000:.0f}k, {('used ' + '/'.join(spent)) if spent else 'nothing used'}"
              + (f", died +{(died - heal_at) / 1000:.1f} s" if died else ""))
    dealt = 0
    for guid in roster:
        own = [row for row in rows.get(guid, []) if len(row) > 12 and heal_at <= row[0] <= stop]
        dealt += sum(max(0, row[12] - previous[12]) for previous, row in zip(own, own[1:]) if previous[8] in healed)
    ends = [row[6] for guid in healed if (row := row_before(rows.get(guid, []), stop))]
    print(f"      raid on the healed Guardians: {dealt / 1000:.0f}k in 20 s,"
          f" health at the end {', '.join(f'{hp:.0f}%' for hp in ends) or '-'}")


def show_phase3_tanks(trace: Trace, start: int, stop: int, rows: dict[int, list[list]], guardians: set,
                      tanks: set, roster: set) -> None:
    """How the Guardians split between the tanks: per 10 s, at each tank swap, after each taunt; then
    Misdirection targets, the tanks' own buttons with their health, and every Lay on Hands of the pull."""
    order = sorted(tanks, key=trace.name)
    if not order:
        return
    victims = {guid: rows.get(guid, []) for guid in guardians}
    print(f"\n  Guardians per tank : all/30%+ per 10 s, {' / '.join(trace.name(tank) for tank in order)}")
    for when in range(start, stop, 10000):
        cells = [f"{guardians_on(victims, tank, when)}/{guardians_on(victims, tank, when, GUARDIAN_FLOOR_PCT)}"
                 for tank in order]
        print(f"    {clock(when)}  {'  '.join(cells)}")
    for rec in notes(trace, "yogg.tankswap"):
        if start <= rec["t"] <= stop:
            looks = "; ".join(f"+{look // 1000} s " + "/".join(str(guardians_on(victims, tank, rec["t"] + look))
                                                                for tank in order) for look in TANK_SWAP_LOOKS_MS)
            print(f"    swap {clock(rec['t'])} by {trace.name(rec.get('g', 0))}, {rec.get('txt', '')}: {looks}")
    for key in ("yogg.tankshed", "yogg.tankloh"):
        picks = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, key) if start <= rec["t"] <= stop)
        if picks:
            print(f"    {key:16} : {dict(picks)}")

    taunts = [(rec["t"], rec.get("s"), rec.get("tgt")) for rec in trace.of("cast")
              if rec.get("s") in tanks and rec.get("sp") in GUARDIAN_TAUNTS and rec.get("tgt") in guardians
              and start <= rec["t"] <= stop]
    for tank in order:
        own = [(when, aim) for when, caster, aim in taunts if caster == tank]
        if own:
            back = sum(1 for when, aim in own if taunt_bounced(victims[aim], tank, when, tanks))
            print(f"    taunts by {trace.name(tank):14}: {len(own)}, on another tank {TAUNT_BOUNCE_MS // 1000} s"
                  f" later {back}")
    redirects = collections.Counter(trace.name(rec.get("tgt", 0)) for rec in trace.of("cast")
                                    if rec.get("sp") == SPELL_MISDIRECTION and start <= rec["t"] <= stop)
    if redirects:
        print(f"    Misdirection on  : {dict(redirects)}")
    for tank in order:
        pressed = []
        for rec in trace.of("cast"):
            name = TANK_BUTTONS.get(rec.get("sp"))
            if name and rec.get("s") == tank and start <= rec["t"] <= stop and not rec.get("tr"):
                row = row_before(rows.get(tank, []), rec["t"])
                pressed.append(f"{name} {clock(rec['t'])}" + (f" at {row[6]:.0f}%" if row else ""))
        print(f"    {trace.name(tank):14} buttons: {', '.join(pressed) or 'none'}")
    for rec in trace.of("cast"):
        if rec.get("sp") in SPELL_LAY_ON_HANDS and rec.get("s") in roster and not rec.get("tr"):
            aim = rec.get("tgt") or rec.get("s")
            print(f"    Lay on Hands {clock(rec['t'])} {trace.name(rec.get('s'))} on {trace.name(aim)}"
                  f" ({trace.role(aim)})")


def show_phase3_healers(trace: Trace, start: int, stop: int, rows: dict[int, list[list]], roster: set) -> None:
    """Per bot healer: mana into phase 3, how often it was put back on the combat engine, which engine its
    Lunatic Gaze node fired on, and the mana cooldowns it cast. The non-combat engine has none of those
    cooldowns, so a healer stuck on it runs dry."""
    healers = sorted((guid for guid in roster if trace.role(guid) == "heal" and guid not in trace.humans),
                     key=trace.name)
    if not healers:
        return
    switches = collections.Counter(rec.get("g") for rec in notes(trace, "yogg.healerengine")
                                   if start <= rec["t"] <= stop)
    gaze: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in trace.of("cov"):
        for row in rec.get("r", []):
            node = trace.covnodes.get(row[0], {})
            if node.get("node") == GAZE_NODE and len(row) > 2:
                gaze[rec.get("g")][node.get("engine", "?")] += row[2]
    spent: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in trace.of("cast"):
        name = trace.spells.get(rec.get("sp"), "")
        if rec.get("s") in healers and name in HEALER_MANA_SPELLS and start <= rec["t"] <= stop:
            spent[rec.get("s")][name] += 1
    print("\n  healers            : mana % at "
          + "/".join(f"+{look // 1000}" for look in HEALER_MANA_LOOKS_MS)
          + " s, switches to the combat engine, gaze node fires by engine, mana cooldowns")
    for guid in healers:
        mana = []
        for look in HEALER_MANA_LOOKS_MS:
            row = row_before(rows.get(guid, []), start + look) if start + look <= stop else None
            mana.append(f"{row[7]:.0f}" if row and row[6] > 0 else "-")
        fires = " ".join(f"{engine} {count}" for engine, count in sorted(gaze[guid].items())) or "-"
        cooldowns = ", ".join(f"{name} {count}" for name, count in sorted(spent[guid].items())) or "none"
        print(f"    {trace.name(guid):14} {'/'.join(mana):15} switches {switches[guid]:3}  gaze {fires:14} {cooldowns}")


def show_phase3(trace: Trace) -> None:
    """Phase 3: whether ranged took the Guardians while melee stayed on Yogg, what the Guardians did to
    the raid, whether a beacon's heal reached Yogg, and what each Lunatic Gaze cost.

    Snapshot rows from snapshot_rows are [t, guid, x, y, z, o, hp, mana, target, moving, movegen,
    casting, dealt]. A marked Guardian keeps its first entry in the trace, so marked and unmarked read
    alike here; the Empowering Shadows cast is what names the marked ones."""
    print("PHASE 3")
    span = next(((start, stop) for phase, start, stop in phase_spans(trace) if phase == 3), None)
    if not span:
        print("  never reached")
        return
    start, stop = span

    roster = roster_guids(trace)
    yogg = next(iter(guids_of_entry(trace, NPC_YOGG_SARON)), None)
    guardians = {guid for guid in guids_of_entry(trace, NPC_IMMORTAL_GUARDIAN)
                 if (first_seen(trace, [guid]) or 0) >= start - 1000}
    rows = snapshot_rows(trace, roster | guardians | ({yogg} if yogg else set()))
    snaps = [snap for snap in trace.of("snap") if start <= snap["t"] <= stop]

    # Up to the last snapshot anyone on the roster was still alive, so a wipe's tail does not water
    # down the rates.
    alive_end = max((snap["t"] for snap in snaps
                     if any(row[0] in roster and row[5] > 0 for row in snap.get("u", []))), default=stop)
    deaths = sorted(rec["t"] for rec in death_records(trace) if rec.get("g") in roster and start <= rec["t"] <= stop)
    first_death = f"{clock(deaths[0])} (+{(deaths[0] - start) / 1000:.1f} s)" if deaths else "none"
    print(f"  {clock(start)} -> {clock(stop)}, roster alive until {clock(alive_end)}, {len(deaths)} deaths,"
          f" first {first_death}")

    # Heroism against the module's own gate for it, so a late one reads as either the gate or the cast.
    placed_at = None
    first_station = None
    for snap in snaps:
        units = {row[0]: row for row in snap.get("u", [])}
        crushers = [(row[1], row[2]) for guid, row in units.items()
                    if trace.entries.get(guid) == NPC_CRUSHER_TENTACLE and row[5] > 0]
        station, ranged_spot = ranged_station(crushers)
        if first_station is None:
            near = min((math.dist(PHASE_3_RANGED_SPOT[:2], crusher) for crusher in crushers), default=None)
            first_station = station + (f" (a Crusher {near:.1f} yd from the ranged spot)" if near is not None
                                       and station != "ranged spot" else "")
        living = placed = 0
        for guid, row in units.items():
            if guid not in roster or row[5] <= 0 or len(row) <= 8:
                continue
            living += 1
            aim = units.get(row[7])
            target = ((aim[1], aim[2], aim[3], COMBAT_REACH.get(trace.entries.get(row[7]), 0.0))
                      if aim and aim[5] > 0 else None)
            placed += in_phase3_position(trace.role(guid), (row[1], row[2], row[3]), bool(row[8]), target,
                                         ranged_spot)
        if living and placed >= LUST_POSITION_SHARE * living:
            placed_at = snap["t"]
            break
    ready = (f"{LUST_POSITION_SHARE:.0%} in position at +{(placed_at - start) / 1000:.1f} s" if placed_at
             else f"never {LUST_POSITION_SHARE:.0%} in position")
    lusts = sorted((rec["t"], rec.get("s"), rec.get("sp")) for rec in trace.of("cast")
                   if rec.get("sp") in SPELL_LUSTS and rec.get("s") in roster and start <= rec["t"] <= stop)
    if lusts:
        when, caster, spell = lusts[0]
        print(f"  lust               : {SPELL_LUSTS[spell]} by {trace.name(caster)} at +{(when - start) / 1000:.1f} s,"
              f" {ready}")
    else:
        print(f"  lust               : none in phase 3, {ready}")
    lust_gate = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.lustready"))
    if lust_gate:
        print(f"    yogg.lustready   : {dict(lust_gate)}")

    # A leftover Crusher on the ranged spot killed seven ranged in 2 s as they walked onto it.
    if first_station:
        print(f"  ranged station     : {first_station} at the start")
    station_notes = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.station")
                                        if start <= rec["t"] <= stop)
    if station_notes:
        print(f"    yogg.station     : {dict(station_notes)}")
    show_phase3_arrival(trace, start, rows, roster)

    if yogg and rows.get(yogg):
        line = []
        for when in range(start, alive_end + 1, 10000):
            row = row_before(rows[yogg], when)
            if row:
                line.append(f"{clock(when)} {row[6]:.2f}")
        print(f"  Yogg hp            : {', '.join(line)}")

    print("\n  targets per 10 s, share of alive bots: guardian / tentacle / yogg / none")
    for when in range(start, alive_end, 10000):
        counts: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
        for snap in snaps:
            if not when <= snap["t"] < when + 10000:
                continue
            for row in snap.get("u", []):
                if row[0] not in roster or row[0] in trace.humans or row[5] <= 0 or len(row) <= 7:
                    continue
                role = trace.role(row[0])
                group = "melee" if role in ("melee", "tank") else role
                counts[group][target_kind(row[7], trace.entries.get(row[7]))] += 1
        cells = []
        for group in ("ranged", "melee", "heal"):
            total = sum(counts[group].values())
            if total:
                share = [counts[group][kind] / total for kind in ("guardian", "tentacle", "yogg", "none")]
                cells.append(f"{group} {' '.join(f'{value:.2f}' for value in share)}")
        print(f"    {clock(when)}  {' | '.join(cells)}")

    teleports: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("sp") == SPELL_SIMPLE_TELEPORT:
            teleports[rec.get("s")].append(rec["t"])
    melee_hits = [rec for rec in trace.of("dmg") if rec.get("s") in guardians and rec.get("sp") == 0]
    # The Guardian casts Weakened on itself, and that cast row reaches the file where the aura row may not.
    weakened = {rec.get("d") for rec in trace.of("aura") if rec.get("sp") == SPELL_WEAKENED and not rec.get("r")}
    weakened |= {rec.get("s") for rec in trace.of("cast") if rec.get("sp") == SPELL_WEAKENED}
    # Thorim is not a watched creature, so Titanic Storm never reaches the file. A Guardian that leaves
    # the snapshots while the raid is still alive, last seen at 10% or less, is the kill it makes. The
    # sweep drops the Guardians once nobody is left alive to anchor it.
    stormed = 0
    tanks = {guid for guid in roster if trace.role(guid) == "tank"}
    flips = []
    casts_at: dict[int, list[tuple[int, int, str]]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("tgt") in guardians and start <= rec["t"] <= stop:
            casts_at[rec["tgt"]].append((rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")))
    print(f"\n  Immortal Guardians : {len(guardians)}, Weakened {len(weakened & guardians)}")
    for guid in sorted(guardians, key=lambda g: rows[g][0][0] if rows.get(g) else 0):
        own = rows.get(guid)
        if not own:
            continue
        spawned = guardian_spawn(own[0][0], teleports.get(guid, []))
        victim = next((row for row in own if row[8] in roster), None)
        hit = next((rec for rec in melee_hits if rec.get("s") == guid), None)
        lowest = min(row[6] for row in own)
        at_stack = sum(1 for row in own if math.dist(row[2:4], PHASE_3_MELEE_SPOT[:2]) <= PHASE_3_MELEE_GUARDIAN_RANGE)
        gone = own[-1][0] < alive_end - 2000
        stormed += gone and own[-1][6] <= 10
        flip = first_off_tank(own, tanks)
        flip_role = (trace.role(flip[8]) if flip[8] in roster else "other") if flip else ""
        if flip:
            cast = {name for when, caster, name in casts_at[guid]
                    if caster == flip[8] and flip[0] - FLIP_CAUSE_MS <= when <= flip[0]}
            flips.append(((flip[0] - spawned) / 1000, flip_role, flip_cause(flip_role, cast)))
        print(f"    {clock(spawned)}  first victim"
              f" {trace.role(victim[8]) if victim else '-':6} +{max(0, (victim[0] if victim else spawned) - spawned) / 1000:4.1f} s"
              f"  off a tank {flip_role or '-':6} +{max(0, (flip[0] if flip else spawned) - spawned) / 1000:4.1f} s"
              f"  first hit {trace.role(hit['d']) if hit else '-':6}"
              f" +{((hit['t'] if hit else spawned) - spawned) / 1000:4.1f} s  lowest {lowest:5.1f}%"
              f"  at the stack {at_stack * 100 // len(own):3}%  {'gone ' + clock(own[-1][0]) if gone else 'alive'}"
              f"{'  Weakened' if guid in weakened else ''}")
    by_role = collections.Counter(trace.role(rec.get("d")) for rec in melee_hits)
    print(f"    gone at 10% or less (Titanic Storm): {stormed}")
    print(f"    melee hits by victim role: {dict(by_role)}")
    if flips:
        print(f"    first off a tank : {len(flips)} of {len(guardians)}, median"
              f" +{statistics.median(seconds for seconds, _, _ in flips):.1f} s,"
              f" to {dict(collections.Counter(role for _, role, _ in flips))},"
              f" cause {dict(collections.Counter(cause for _, _, cause in flips))}")
    # Without Thorim nothing kills them, so how much of the time they spent at full power is the phase.
    full, weak = guardian_power_shares([row[6] for guid in guardians for row in rows.get(guid, [])
                                        if start <= row[0] <= alive_end and row[6] > 0])
    print(f"    power            : {full:.0%} of Guardian samples at 90% health or more, {weak:.0%} Weakened")
    drains = sorted((rec["t"], rec.get("s", 0), rec.get("d", 0), rec.get("r", 0)) for rec in trace.of("aura")
                    if rec.get("sp") in SPELL_GUARDIAN_DRAIN_LIFE and start <= rec["t"] <= stop)
    channels = drain_life_channels(drains)
    if channels:
        cut = sum(1 for _, _, ms in channels if ms <= DRAIN_CUT_MS)
        full = [(began, caster) for began, caster, ms in channels if ms >= DRAIN_FULL_MS]
        # A kick that was ready and in reach while the channel ran out sat on a bot busy casting.
        kicks: dict[int, tuple[list[int], int, float]] = {}
        for rec in trace.of("cast"):
            kick = KICK_SPELLS.get(rec.get("sp"))
            if kick and rec.get("s") in roster and rec.get("s") not in trace.humans:
                kicks.setdefault(rec.get("s"), ([], kick[1], kick[2]))[0].append(rec["t"])
        missed = 0
        for began, caster in full:
            spot = at(trace, caster, began)
            spots = {guid: where[:2] for guid in kicks if (where := at(trace, guid, began))}
            missed += bool(spot) and ready_kickers(kicks, spots, spot[:2], began) > 0
        print(f"    Drain Life       : {len(channels)} channels, {cut} cut inside {DRAIN_CUT_MS / 1000:.1f} s,"
              f" {len(full)} ran the full 5 s, {missed} of those with a bot kick ready and in range")
        nudges = collections.Counter(str(rec.get("txt", "")).split(" ", 1)[-1] for rec in notes(trace, "yogg.kicknudge")
                                     if start <= rec["t"] <= stop)
        if nudges:
            print(f"    yogg.kicknudge   : {dict(nudges)}")

    # A bot tank's target, and whether its taunt was even available when a Guardian swung at the raid.
    swings = [rec for rec in melee_hits if trace.role(rec.get("d")) != "tank"]
    for guid in sorted(tanks - set(trace.humans), key=trace.name):
        kinds = collections.Counter(target_kind(row[8], trace.entries.get(row[8]))
                                    for row in rows.get(guid, []) if start <= row[0] <= alive_end and row[6] > 0)
        total = sum(kinds.values()) or 1
        taunts = sorted(rec["t"] for rec in trace.of("cast")
                        if rec.get("s") == guid and rec.get("sp") == SPELL_HAND_OF_RECKONING)
        down = sum(1 for rec in swings if on_cooldown(taunts, rec["t"], TAUNT_COOLDOWN_MS))
        print(f"  bot tank {trace.name(guid):14}: targets guardian {kinds['guardian'] / total:.2f}"
              f" tentacle {kinds['tentacle'] / total:.2f} yogg {kinds['yogg'] / total:.2f} none {kinds['none'] / total:.2f};"
              f" Hand of Reckoning on cooldown at {down} of {len(swings)} swings on non-tanks")
    held = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.tankhold"))
    if held:
        print(f"    yogg.tankhold    : {dict(held)}")
    show_phase3_tanks(trace, start, alive_end, rows, guardians, tanks, roster)

    # The gaze action used to claim the tank's tick for all 4 s, so a Guardian peeled in one went
    # untaunted until it had swung.
    gazes = [(rec["t"], rec["t"] + LUNATIC_GAZE_MS) for rec in trace.of("cast")
             if rec.get("sp") == SPELL_LUNATIC_GAZE_YOGG and start <= rec["t"] <= alive_end]
    if swings:
        taunt_casts = sorted((rec["t"], rec.get("tgt", 0)) for rec in trace.of("cast")
                             if rec.get("s") in tanks and rec.get("sp") in GUARDIAN_TAUNTS)
        victims = {guid: [(row[0], row[8]) for row in rows.get(guid, [])] for guid in guardians}
        inside, taunted = swings_in_gaze([(rec["t"], rec.get("s", 0), rec.get("d", 0)) for rec in swings],
                                         gazes, taunt_casts, victims)
        print(f"    swings on the raid during a gaze: {inside} of {len(swings)},"
              f" taunted between the turn and the swing {taunted}")

    beacons = [rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_SHADOW_BEACON and start <= rec["t"] <= stop]
    heals = [rec for rec in trace.of("cast") if rec.get("sp") in SPELL_EMPOWERING_SHADOWS and start <= rec["t"] <= stop]
    # What a healed Guardian did to whoever held it: deaths, and the buttons and externals that landed.
    died_at: dict[int, list[int]] = collections.defaultdict(list)
    for rec in death_records(trace):
        died_at[rec.get("g", 0)].append(rec["t"])
    covers: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        name = NOVA_DEFENSIVES.get(rec.get("sp")) or NOVA_EXTERNALS.get(rec.get("sp"))
        if name and not rec.get("r") and rec.get("d") in roster and start <= rec["t"] <= stop:
            covers[rec.get("d")].append((rec["t"], name))
    hunter_casts = [(rec["t"], rec.get("s"), rec.get("sp"), rec.get("tgt", 0)) for rec in trace.of("cast")
                    if rec.get("sp") in (SPELL_DISTRACTING_SHOT, SPELL_FEIGN_DEATH) and start <= rec["t"] <= stop]
    flows = [(rec["t"], rec.get("d"), -rec.get("a", 0)) for rec in trace.of("dmg")
             if rec.get("d") in tanks and start <= rec["t"] <= stop]
    flows += [(rec["t"], rec.get("d"), rec.get("a", 0) - rec.get("oh", 0)) for rec in trace.of("heal")
              if rec.get("d") in tanks and start <= rec["t"] <= stop]
    # The drags themselves: every taunt at a marked Guardian, the stagers picked before each beacon and the
    # kickers held for the marks.
    drag_taunts = [(rec["t"], rec.get("s"), rec.get("sp"), rec.get("tgt", 0)) for rec in trace.of("cast")
                   if rec.get("sp") in GUARDIAN_TAUNTS | {SPELL_DISTRACTING_SHOT} and rec.get("s") in roster
                   and start <= rec["t"] <= stop]
    by_name = {trace.name(guid): guid for guid in roster}
    stage_picks = [(rec["t"], str(rec.get("txt", ""))[len("pick "):].split()) for rec in notes(trace, "yogg.beaconstage")
                   if str(rec.get("txt", "")).startswith("pick ")]
    kick_notes = [(rec["t"], str(rec.get("txt", ""))) for rec in notes(trace, "yogg.markkick")]
    # How much of the Guardians' healing ran under Mortal Strike's -50%.
    cut_samples = [(rec["t"], str(rec.get("txt", ""))) for rec in notes(trace, "yogg.healcut")]
    print(f"\n  Shadow Beacons     : {len(beacons)}")
    yogg_spot = at(trace, yogg, start) if yogg else None
    for group in group_runs([rec["t"] for rec in heals], 1000, 0):
        casters = [rec for rec in heals if group[0] <= rec["t"] <= group[1]]
        reach = []
        for rec in casters:
            spot = at(trace, rec.get("s"), rec["t"])
            if spot and yogg_spot:
                reach.append(math.dist(spot, yogg_spot))
        before = row_before(rows.get(yogg, []), group[0]) if yogg else None
        after = row_before(rows.get(yogg, []), group[0] + EMPOWERING_SHADOWS_MS) if yogg else None
        change = f"{after[6] - before[6]:+.2f}%" if before and after else "-"
        inside = sum(1 for yards in reach if yards <= EMPOWERING_SHADOWS_RADIUS)
        # The heal lands on every Guardian within 20 yd of a marked one too, which resets the pile.
        marked = {rec.get("s") for rec in casters}
        others: dict[int, tuple[float, float]] = {}
        # Fresh rows only, not health: a Weakened Guardian sits at 1 HP, which the snapshot reads as 0%.
        for guid in guardians - marked:
            row = row_before(rows.get(guid, []), group[0])
            if row and group[0] - row[0] <= 1500:
                others[guid] = (row[2], row[3])
        nearest = []
        spots = []
        for rec in casters:
            spot = at(trace, rec.get("s"), rec["t"])
            if spot:
                spots.append(spot[:2])
                if others:
                    nearest.append(min(math.dist(spot[:2], other) for other in others.values()))
        pile = f", nearest other Guardian {', '.join(f'{yards:.1f}' for yards in nearest)} yd" if nearest else ""
        print(f"    {clock(group[0])}  {len(casters)} heals, Guardians {', '.join(f'{yards:.1f}' for yards in reach)} yd"
              f" from Yogg ({inside} inside {EMPOWERING_SHADOWS_RADIUS:.0f}){pile}, Yogg {change} over the next 20 s")
        # 64486 stacks per caster, so marked Guardians dragged to one spot heal each other.
        stacks = stacked_heals(spots)
        print(f"      marked pairs within {EMPOWERING_SHADOWS_RADIUS:.0f} yd: {(sum(stacks) - len(stacks)) // 2},"
              f" heals each took {stacks}")
        healed = beacon_splash(spots, others)
        if healed:
            changes = []
            for guid in healed:
                then = row_before(rows[guid], group[0])
                later = row_before(rows[guid], group[0] + BEACON_SPLASH_LOOK_MS)
                changes.append(f"{then[6]:.0f} -> {later[6]:.0f}%")
            print(f"      other Guardians it reached: {len(healed)} ({', '.join(changes)}"
                  f" over {BEACON_SPLASH_LOOK_MS // 1000} s)")
        show_beacon_tanks(trace, group[0], marked | set(healed), rows, roster, tanks, flows, covers, died_at)
        mark_at = max((when for when in beacons if when <= group[0]), default=group[0] - 10000)
        pick = next((names for when, names in reversed(stage_picks) if mark_at - 20000 <= when <= mark_at), None)
        if pick is not None:
            stood = []
            for name in pick:
                spot = at(trace, by_name[name], mark_at) if name in by_name else None
                stood.append((name, spot[:2] if spot else None))
            print(f"      staged at the mark: {', '.join(stage_gaps(stood)) or 'nobody'}")
        reserve = next((text for when, text in kick_notes
                        if mark_at - 1000 <= when <= mark_at + 3000 and text.startswith("reserve")), None)
        if reserve is not None:
            landed = collections.Counter(text for when, text in kick_notes
                                         if mark_at <= when <= group[0] and text.startswith("kick"))
            print(f"      kick reserve: {reserve[len('reserve '):] or 'nobody'}, kicks on marks {dict(landed)}")
        if cut_samples:
            seconds = heal_cut_seconds(cut_samples, group[0], group[0] + EMPOWERING_SHADOWS_MS)
            print(f"      healing cut over the heal: {heal_cut_line(seconds)}")
        for rec in casters:
            guardian = rec.get("s")
            own = rows.get(guardian, [])
            holder = row_before(own, group[0])
            hits = [(hit["t"], hit.get("d", 0), hit.get("a", 0)) for hit in melee_hits if hit.get("s") == guardian]
            exposure = drag_exposure(hits, group[0], DRAG_WATCH_MS)
            parts = []
            for victim, (total, biggest, count) in sorted(exposure.items(), key=lambda item: -item[1][0]):
                if victim in tanks:
                    continue
                died = next((when for when in died_at[victim] if group[0] <= when <= group[0] + DRAG_WATCH_MS), None)
                spent = sorted({name for when, name in covers[victim]
                                if group[0] - BEACON_SPLASH_LOOK_MS <= when <= group[0] + DRAG_WATCH_MS})
                parts.append(f"{trace.name(victim)} {total / 1000:.0f}k in {count} (top {biggest / 1000:.0f}k)"
                             + (f" died +{(died - group[0]) / 1000:.1f} s" if died else "")
                             + (f" used {'/'.join(spent)}" if spent else ""))
            tank_at = next((row[0] for row in own if row[0] >= group[0] and row[8] in tanks), None)
            shots = sum(1 for when, _, spell, aim in hunter_casts if spell == SPELL_DISTRACTING_SHOT and aim == guardian
                        and group[0] - 11000 <= when <= group[0])
            feigns = sum(1 for when, caster, spell, _ in hunter_casts if spell == SPELL_FEIGN_DEATH
                         and group[0] <= when <= group[0] + DRAG_WATCH_MS and holder and caster == holder[8])
            hunter = f", {shots} Distracting Shot, {feigns} Feign Death" if shots or feigns else ""
            tank = f"+{(tank_at - group[0]) / 1000:.1f} s" if tank_at else "never"
            print(f"      held by {trace.name(holder[8]) if holder and holder[8] else '-'} at the heal{hunter};"
                  f" a tank had it {tank}; non-tanks hit: {'; '.join(parts) or 'none'}")
            # A missed taunt and a mark's own Drain Life are what kept marks in the pile.
            spans = [(began, began + ms) for began, caster, ms in channels if caster == guardian]
            tried = [f"{trace.name(who)} {trace.spells.get(spell, spell)} {clock(when)}"
                     f" {taunt_outcome(own, when, who, spans)}"
                     for when, who, spell, aim in drag_taunts if aim == guardian and mark_at - 300 <= when <= group[0]]
            seconds, count, cut = mark_drains(channels, guardian, mark_at, group[0])
            print(f"        taunts: {'; '.join(tried) or 'none'}; Drain Life {seconds:.1f} s in {count}"
                  f" channel(s), {cut} cut")
    drags = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.beacondrag"))
    if drags:
        print(f"    yogg.beacondrag  : {dict(drags)}")
    sides = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.beaconspot"))
    if sides:
        print(f"    yogg.beaconspot  : {dict(sides)}")
    for key in ("yogg.dragdef", "yogg.dragext", "yogg.mortalstrike"):
        picks = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, key) if start <= rec["t"] <= stop)
        if picks:
            print(f"    {key:16} : {dict(picks)}")
    if cut_samples:
        print(f"    yogg.healcut     : {heal_cut_line(heal_cut_seconds(cut_samples, start, stop))}")

    # Silences against the Aura Mastery that should have covered them.
    roars = [rec["t"] for rec in trace.of("cast")
             if rec.get("sp") == SPELL_DEAFENING_ROAR and rec.get("s") == yogg and start <= rec["t"] <= stop]
    if roars:
        silences = [(rec["t"], rec.get("d")) for rec in trace.of("aura")
                    if rec.get("sp") == SPELL_DEAFENING_ROAR and not rec.get("r") and rec.get("d") in roster]
        immunes = [(rec["t"], rec.get("d")) for rec in trace.of("aura")
                   if rec.get("sp") == SPELL_AURA_MASTERY_IMMUNE and not rec.get("r") and rec.get("d") in roster]
        masteries = [(rec["t"], rec.get("s")) for rec in trace.of("cast")
                     if rec.get("sp") == SPELL_AURA_MASTERY and rec.get("s") in roster]
        print(f"\n  Deafening Roar     : {len(roars)}")
        for when in roars:
            land = when + ROAR_CAST_MS
            silenced = {guid for t, guid in silences if when <= t <= land + 1500}
            immune = {guid for t, guid in immunes if when - 6000 <= t <= land + 500}
            # Every press, so a second paladin spending its own on the same roar shows.
            presses = [(t, caster) for t, caster in masteries if when - 6000 <= t <= land]
            pressed = ("Aura Mastery by " + ", ".join(f"{trace.name(caster)} at {(t - when) / 1000:+.1f} s"
                                                      for t, caster in presses) if presses else "no Aura Mastery")
            print(f"    {clock(when)}  silenced {len(silenced)}, immune {len(immune)}, {pressed}:"
                  f" {roar_verdict(bool(presses), len(silenced))}")
        presses = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.roar")
                                      if start <= rec["t"] <= stop)
        if presses:
            print(f"    yogg.roar        : {dict(presses)}")

    def inside_gaze(when: int) -> bool:
        return any(first <= when <= last for first, last in gazes)

    if gazes:
        gaze_s = sum(min(last, alive_end) - first for first, last in gazes) / 1000
        rest_s = max((alive_end - start) / 1000 - gaze_s, 0.001)
        casts = [rec["t"] for rec in trace.of("cast") if rec.get("s") in roster and not rec.get("tr")]
        during = sum(1 for when in casts for first, last in gazes if first <= when <= last)
        before = sum(1 for when in casts for first, _ in gazes if first - LUNATIC_GAZE_MS <= when < first)
        heal_in = heal_out = 0
        for rec in trace.of("heal"):
            if rec.get("s") in roster and start <= rec["t"] <= alive_end:
                amount = rec.get("a", 0) - rec.get("oh", 0)
                if inside_gaze(rec["t"]):
                    heal_in += amount
                else:
                    heal_out += amount
        dealt_in = dealt_out = 0
        for guid in roster:
            own = [row for row in rows.get(guid, []) if len(row) > 12 and start - 2000 <= row[0] <= alive_end]
            for previous, row in zip(own, own[1:]):
                if row[0] < start:
                    continue
                gain = max(0, row[12] - previous[12])
                if inside_gaze(row[0]):
                    dealt_in += gain
                else:
                    dealt_out += gain
        print(f"\n  Lunatic Gaze       : {len(gazes)}, {gaze_s:.0f} s of {(alive_end - start) / 1000:.0f}")
        print(f"    cast starts      : {during / len(gazes):.1f} per gaze vs {before / len(gazes):.1f} in the 4 s before")
        print(f"    healing/s        : {heal_in / gaze_s:,.0f} inside vs {heal_out / rest_s:,.0f} outside")
        print(f"    damage/s         : {dealt_in / gaze_s:,.0f} inside vs {dealt_out / rest_s:,.0f} outside")
    picks = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.gaze"))
    if picks:
        print(f"    yogg.gaze        : {dict(picks)}")

    # Where the damage went, by each bot's target and its band just before each gain. The module wants most
    # of it on Yogg, Guardians only over the floor, and under it only inside a gaze. Area damage counts on
    # the target, so a melee bot on Yogg cleaving the pile reads as all Yogg.
    bands: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    for guid in roster:
        own = [row for row in rows.get(guid, []) if len(row) > 12 and start <= row[0] <= alive_end]
        for previous, row in zip(own, own[1:]):
            gain = row[12] - previous[12]
            if gain <= 0 or gain > 200000:
                continue
            entry = trace.entries.get(previous[8])
            health = None
            if entry == NPC_IMMORTAL_GUARDIAN:
                aimed = row_before(rows.get(previous[8], []), previous[0])
                health = aimed[6] if aimed else None
            bands[trace.role(guid)][damage_band(entry, health, inside_gaze(previous[0]))] += gain
    seconds = max((alive_end - start) / 1000, 0.001)
    order = ("yogg", "over 30", "10-30", "10-30 gaze", "weakened", "tentacle", "guardian ?", "other")

    def split(counter: collections.Counter) -> str:
        whole = sum(counter.values()) or 1
        return "  ".join(f"{band} {counter[band] * 100 / whole:.0f}%" for band in order if counter[band])

    everyone = sum(bands.values(), collections.Counter())
    if everyone:
        print(f"\n  damage split by each bot's target: {sum(everyone.values()) / seconds / 1000:.0f}k/s  {split(everyone)}")
        for role in ("ranged", "melee", "tank", "heal"):
            if sum(bands[role].values()) >= 1000 * seconds:
                print(f"    {role:6}           : {sum(bands[role].values()) / seconds / 1000:.1f}k/s  {split(bands[role])}")
    show_phase3_area(trace, start, alive_end, rows, roster, tanks, guardians, yogg)

    healers = [guid for guid in roster if trace.role(guid) == "heal" and guid not in trace.humans]
    if healers and yogg_spot:
        print(f"\n  healers: median distance to the melee spot, facing away from Yogg during gazes")
        for guid in healers:
            own = [row for row in rows.get(guid, []) if start <= row[0] <= alive_end and row[6] > 0]
            if not own:
                continue
            spot = statistics.median(math.dist(row[2:4], PHASE_3_MELEE_SPOT[:2]) for row in own)
            gazing = [row for row in own if inside_gaze(row[0])]
            away = sum(1 for row in gazing if facing_away(row[5], row[2:4], yogg_spot))
            share = f"{away * 100 // len(gazing)}% of {len(gazing)}" if gazing else "-"
            print(f"    {trace.name(guid):14} {spot:5.1f} yd  away {share}")
    show_phase3_healers(trace, start, alive_end, rows, roster)

    show_phase3_leftovers(trace, start, alive_end)


def show_phase3_leftovers(trace: Trace, start: int, stop: int) -> None:
    """The tentacles phase 2 left standing: where each one ended, and how long Diminish Power, the
    Crusher's 20% off the raid's damage, sat on the average raider while it lived."""
    tentacles = {guid: entry for entry in TENTACLE_NAMES for guid in guids_of_entry(trace, entry)}
    rows = snapshot_rows(trace, set(tentacles))
    left = []
    for guid, own in rows.items():
        before = [row for row in own if row[0] <= start]
        after = [row for row in own if row[0] >= start]
        if before and after and before[-1][6] > 0:
            left.append((guid, before[-1][6], after[-1]))

    roster = roster_guids(trace)
    if left:
        # Owners rank every Guardian above these, so pets are the ones meant to take them.
        pets = {guid for guid, owner in trace.owners.items() if owner in roster}
        leftover_guids = {guid for guid, _, _ in left}
        on: dict[int, set] = collections.defaultdict(set)
        for snap in trace.of("snap"):
            if start <= snap["t"] <= stop:
                for row in snap.get("u", []):
                    if row[0] in pets and len(row) > 7 and row[7] in leftover_guids:
                        on[row[7]].add(row[0])

        # A beacon heal reaching a leftover undoes whatever went into it, so it has to die between two.
        heal_casts = [rec["t"] for rec in trace.of("cast")
                      if rec.get("sp") in SPELL_EMPOWERING_SHADOWS and start <= rec["t"] <= stop]
        heal_times = [first for first, _ in group_runs(heal_casts, 1000, 0)]

        print(f"\n  phase 2 leftovers  : {len(left)}")
        for guid, at_start, last in sorted(left, key=lambda item: item[2][0]):
            # Sampling stops with the anchor, so a tentacle going quiet is not a kill: only one sampled
            # at the bottom of its health bar is.
            fate = f"killed at +{(last[0] - start) / 1000:.1f} s" if last[6] <= LEFTOVER_DEAD_PCT else "still up"
            names = ", ".join(sorted(trace.name(pet) for pet in on[guid])) or "none"
            healed = beacon_heal_rises([(row[0], row[6]) for row in rows[guid]], heal_times)
            print(f"    {TENTACLE_NAMES[tentacles[guid]]:11} {at_start:5.1f}% at phase 3,"
                  f" last seen {clock(last[0])} at {last[6]:.1f}%, {fate}; healed by {healed} beacon(s);"
                  f" pets on it: {names}")

    applied = [(rec["t"], rec.get("d", 0), bool(rec.get("r"))) for rec in trace.of("aura")
               if rec.get("sp") == SPELL_DIMINISH_POWER and rec.get("d") in roster]
    if applied and stop > start and roster:
        share = aura_seconds(applied, start, stop) / ((stop - start) / 1000.0) / len(roster)
        print(f"    Diminish Power on the average raider: {share * 100:.0f}% of phase 3")


def nova_warning(series: list[tuple[int, float]], gate: float, when: int) -> float | None:
    """Seconds from the first sample at or under `gate` to `when`, or None when it never got there.
    Measured over 48 novas this ran a median 4.8 s at the 20% gate and 2.4 s at 10%, which is what sets
    which button is worth starting."""
    first = next((t for t, hp in series if 0 < hp <= gate and t <= when), None)
    return None if first is None else (when - first) / 1000


def nova_cover(spans: list[tuple[int, int, str]], when: int) -> list[str]:
    """Which buffs from (start, end, name) spans were up at `when`."""
    return sorted({name for start, end, name in spans if start <= when <= end})


def nova_preresist(rows: list[dict]) -> tuple[int, int, int]:
    """(hits, hits that resisted nothing, the worst unresisted hit) over `dmg` rows. The unresisted hit
    is the figure worth planning against: over one pull the damage before resistance ran a 23,547 median
    against a 17,717 median after it, and every death from full health took a hit that resisted none."""
    clean = [rec["a"] for rec in rows if not rec.get("rs")]
    return len(rows), len(clean), max(clean, default=0)


def defensive_spans(trace: Trace, until: int, wanted: dict[int, str]) -> dict[int, list[tuple]]:
    """guid -> (start, end, name) for each of `wanted` a roster member carried before `until`. An aura
    still up at the end of the window is closed there rather than dropped."""
    roster = roster_guids(trace)
    opened: dict[tuple[int, int], int] = {}
    spans: dict[int, list[tuple]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        guid, spell = rec.get("d"), rec.get("sp")
        if spell not in wanted or guid not in roster or rec["t"] > until:
            continue
        key = (guid, spell)
        if not rec.get("r"):
            opened.setdefault(key, rec["t"])
        elif key in opened:
            spans[guid].append((opened.pop(key), rec["t"], wanted[spell]))
    for (guid, spell), start in opened.items():
        spans[guid].append((start, until, wanted[spell]))
    return spans


def show_novas(trace: Trace) -> None:
    print("SHADOW NOVA")
    p1_end = phase1_end(phase_spans(trace))
    if p1_end is None:
        print("  no phase 1 in this trace")
        return

    roster = roster_guids(trace)
    guardians = guids_of_entry(trace, NPC_GUARDIAN)

    # The whole fight, not just phase 1: nothing in the core despawns the Guardians phase 1 leaves
    # alive, so their death nova outlives the phase that spawned them.
    end = max((snap["t"] for snap in trace.of("snap")), default=p1_end)

    series: dict[int, list[tuple[int, float]]] = collections.defaultdict(list)
    for snap in trace.of("snap"):
        for row in snap.get("u", []):
            if row[0] in guardians:
                series[row[0]].append((snap["t"], row[5]))

    # One nova per Guardian: it is a death explosion, so a source cannot fire twice.
    hits: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("dmg"):
        if rec.get("sp") in SPELL_SHADOW_NOVAS and rec.get("d") in roster:
            hits[rec.get("s")].append(rec)
    if not hits:
        print("  no nova hit a raider")
        return

    own = defensive_spans(trace, end, NOVA_DEFENSIVES)
    ext = defensive_spans(trace, end, NOVA_EXTERNALS)
    raid = defensive_spans(trace, end, NOVA_RAID_AURAS)

    died: dict[int, list[int]] = collections.defaultdict(list)
    for rec in death_records(trace):
        died[rec.get("g")].append(rec["t"])

    covers: list[tuple[int, bool]] = []
    rows_all = [rec for rows in hits.values() for rec in rows]
    total, clean, worst_clean = nova_preresist(rows_all)
    print(f"  novas on raiders: {len(hits)}, {total} hits")
    print(f"  resisted nothing: {clean} of {total}, worst {worst_clean} - this is what kills, so a bot "
          f"is counted at risk when the nova leaves it under {NOVA_EXPECTED_DAMAGE} hp")

    at_risk = covered = 0
    # Two novas in one tick each sit inside the same death's window, and a bot can die more than once a
    # pull, so a death is keyed by who and when rather than by either alone.
    lethal: set[tuple[int, int]] = set()
    lethal_covered: set[tuple[int, int]] = set()
    for source in sorted(hits, key=lambda guid: hits[guid][0]["t"]):
        rows = hits[source]
        when = rows[0]["t"]
        gates = "  ".join(
            f"{int(gate)}%:" + ("-" if warn is None else f"{warn:.1f}s")
            for gate, warn in ((gate, nova_warning(series.get(source, []), gate, when))
                               for gate in NOVA_WARNING_GATES))
        worst = max(rows, key=lambda rec: rec["a"])
        pool = trace.maxhp.get(worst["d"]) or 0
        share = f" ({worst['a'] / pool * 100:.0f}% of pool)" if pool else ""
        roll = " resisted nothing" if not worst.get("rs") else f" resisted {worst['rs']}"
        # The back line stands 21.5 yd out and clears a blast on the stack by about a yard, so one of
        # them taking a nova at all means the Guardian came to them.
        back = sum(1 for rec in rows if trace.role(rec.get("d")) in ("ranged", "heal"))
        caught = f", {back} of them back line" if back else ""
        print(f"    {clock(when):>9}  warning {gates}  {len(rows)} raiders{caught}, "
              f"worst {worst['a']} on {trace.name(worst['d'])}{share},{roll}")

        doomed = []
        for rec in rows:
            guid = rec.get("d")
            mhp = trace.maxhp.get(guid) or 0
            left = mhp * (rec.get("hp") or 100) / 100.0
            if mhp and left - rec["a"] < 0:
                doomed.append(rec)
        if doomed:
            at_risk += 1
            shields = []
            for rec in doomed:
                guid = rec.get("d")
                up = nova_cover(own.get(guid, []), when) + nova_cover(ext.get(guid, []), when)
                shields.append(f"{trace.name(guid)}" + (f" [{', '.join(up)}]" if up else " [nothing]"))
            if any("[" in row and "nothing" not in row for row in shields):
                covered += 1
            print(f"      at risk: {', '.join(shields)}")

        raid_up = sorted({name for guid in roster for name in nova_cover(raid.get(guid, []), when)})
        covers.append((when, bool(raid_up)))
        if raid_up:
            print(f"      raid-wide: {', '.join(raid_up)}")

        killed = {(rec.get("d"), t) for rec in rows for t in died[rec.get("d")]
                  if 0 <= t - rec["t"] <= 1500}
        if killed:
            lethal.update(killed)
            lethal_covered.update((guid, t) for guid, t in killed
                                  if nova_cover(own.get(guid, []), when) or nova_cover(ext.get(guid, []), when))
            print(f"      died: {', '.join(trace.name(guid) for guid, _ in sorted(killed, key=lambda k: k[1]))}")

    print(f"  novas that would have killed somebody: {at_risk}, with a button up on at least one: {covered}")
    print(f"  deaths to a nova: {len(lethal)}, of which had any button up: {len(lethal_covered)}")

    # Which novas nothing raid-wide was up for, against which ones killed. Every death in the pull that
    # the per-Guardian claim came from landed on an uncovered nova, and one of those went uncovered only
    # because a bot held the window with nothing left to press.
    bare = [when for when, had in covers if not had]
    if covers:
        fell = sum(1 for _, when in lethal if any(abs(when - one) <= 2000 for one in bare))
        print(f"  novas with nothing raid-wide up: {len(bare)} of {len(covers)}, "
              f"deaths on one of those: {fell} of {len(lethal)}")

    # The one-target externals, which run 2 and 3 minutes against a phase with a nova every 8 s. Spent
    # per victim they all went inside the first 15 s, on novas that killed nobody.
    spent = [(rec["t"], trace.name(rec.get("s")), NOVA_EXTERNALS[rec["sp"]])
             for rec in sorted(trace.of("cast"), key=lambda rec: rec["t"])
             if rec.get("sp") in NOVA_EXTERNALS and rec.get("s") in roster]
    print(f"  one-target externals: {len(spent)} presses")
    for when, who, what in spent:
        says = "on a nova that killed" if any(abs(when - one) <= 4000 for _, one in lethal) else ""
        print(f"    {clock(when):>9}  {who:14} {what:18} {says}")

    # A step-out only counts if the bot stays out. Four healers in one pull reached 19.8 yd from the
    # Guardian and the body detour returned them to 4.8 yd before it went off. Phase 1 is left out
    # because melee stand in the blast there on purpose.
    walked = late = 0
    for source in hits:
        fired = min(rec["t"] for rec in hits[source])
        if fired <= p1_end:
            continue

        spot = at(trace, source, fired)
        if not spot:
            continue

        for rec in hits[source]:
            late += 1
            path = [point[1:3] for point in tracks(trace, [rec.get("d")]).get(rec.get("d"), [])
                    if fired - NOVA_WALK_BACK_MS <= point[0] <= fired]
            if nearest_over_track(path, spot[:2], NOVA_CLEAR_RADIUS)[1]:
                walked += 1
    if late:
        print(f"  hit by a nova after phase 1: {late}, of those clear of {NOVA_CLEAR_RADIUS:.0f} yd at "
              f"some point in the {NOVA_WALK_BACK_MS / 1000:.0f} s before it: {walked}")

    # The handover sends the front line to one spot 25 yd round the station ring from the back line, so
    # a Guardian that followed it there cannot reach them. One back-line bot caught is somebody who
    # walked in; the whole stack is somebody who walked a Guardian to them.
    for label, inside in (("in phase 1", lambda t: t <= p1_end), ("after it", lambda t: t > p1_end)):
        hit = {rec.get("d") for rows in hits.values() for rec in rows
               if inside(rec["t"]) and trace.role(rec.get("d")) in ("ranged", "heal")}
        verdict = ", ".join(sorted(trace.name(guid) for guid in hit)) if hit else "none"
        print(f"  back line caught by a nova {label}: {len(hit)} - {verdict}")

    # The raid-wide button against the nova that needed it. Spending it is only visible as a cast: the
    # aura lands on all 25, so every row of it looks the same whoever pressed. Divine Sacrifice is the
    # press and Divine Guardian the 20% its talent procs alongside, so counting the cast counts once.
    presses = [(rec["t"], trace.name(rec.get("s")), NOVA_RAID_COVERS[rec["sp"]])
               for rec in sorted(trace.of("cast"), key=lambda rec: rec["t"])
               if rec.get("sp") in NOVA_RAID_COVERS and rec.get("s") in roster]
    print(f"  raid-wide mitigation: {len(presses)} presses, {RAID_COOLDOWN_MS // 1000} s each")
    for when, who, what in presses:
        print(f"    {clock(when):>9}  {who:14} {what:16} back at {clock(when + RAID_COOLDOWN_MS)}")
    if lethal:
        first = min(when for _, when in lethal)
        gap = cooldown_gap([when for when, _, _ in presses], RAID_COOLDOWN_MS, first)
        says = "one was in hand" if gap is None else f"the earliest was back {gap / 1000:.1f} s too late"
        print(f"    against the first nova that killed, at {clock(first)}: {says}")

    # A battle rez puts the bot back on a bar 13-17% smaller, with none of the raid buffs it died with,
    # and the next nova kills it from full health. Counted here because the deaths above include the
    # second and third ones, which no pool in the trace describes: trace.maxhp keeps the bar from before
    # the first death, so the killing blow is the only witness to the real one.
    back = sorted(spell for spell, name in trace.spells.items()
                  if name in ("Rebirth", "Reincarnation"))
    rezzes = [rec["t"] for rec in trace.of("cast") if rec.get("sp") in back]
    pairs = rez_pairs(sorted((when, guid) for guid, when in lethal), rezzes, REZ_WINDOW_MS)
    if pairs:
        up = sum(1 for _, _, one in pairs if one is not None)
        print(f"  of those deaths, resurrected straight back into it: {up} of {len(pairs)}")
        for when, guid, one in pairs:
            again = [other for other in died[guid] if when < other <= when + REZ_AGAIN_MS]
            says = f"back {(one - when) / 1000:.1f} s later" if one else "stayed down"
            lived = f", died again {(again[0] - when) / 1000:.0f} s on" if one and again else ""
            print(f"    {clock(when):>9}  {trace.name(guid):14} {says}{lived}")

    # What the gate's own figure is worth. Resistance is rolled per hit, so the median is not a floor,
    # but it is what decides whether a single nova can kill the pool the gate is comparing against.
    shares = [resisted_share(rec["a"], rec.get("rs", 0)) for rec in rows_all
              if rec["a"] + rec.get("rs", 0) > 0]
    if shares:
        print(f"  a nova landed a median {statistics.median(rec['a'] for rec in rows_all):,.0f} against "
              f"the {NOVA_EXPECTED_DAMAGE:,} the gate assumes, resisted median "
              f"{statistics.median(shares) * 100:.1f}% (min {min(shares) * 100:.1f}%)")

    # Where the 130 comes from. Shadow Resistance Aura and Prayer of Shadow Protection both give it on
    # the same exclusive aura 143 and the core takes the highest rather than the sum, so the question is
    # not the total: it is whether the raid carries the 130 itself, and whether the paladin on the aura
    # is one that can double it to 260. Removals count as well as applications, because a buff put up
    # before the pull shows up only when it comes off.
    def named(guids: set) -> str:
        return ", ".join(sorted(f"{trace.name(one)}{' (human)' if one in trace.humans else ''}"
                                for one in guids)) or "nobody"

    holders = {rec.get("s") for rec in trace.of("aura") if rec.get("sp") in SPELL_SHADOW_AURA_RANKS}
    prayer = {rec.get("s") for rec in trace.of("aura")
              if rec.get("sp") == SPELL_PRAYER_OF_SHADOW_PROTECTION}
    mastery = {rec.get("s") for rec in trace.of("cast") if rec.get("sp") == SPELL_AURA_MASTERY}
    print(f"  the 130 shadow resistance: aura from {named(holders)}, prayer from {named(prayer)}")
    print(f"    Aura Mastery pressed by {named(mastery)}, of those holding the aura: "
          f"{named(holders & mastery)}")

    # Pairs with no room for a heal between them. This is what the melee pile dies to once stepping out
    # is off the table, and a step-out cannot answer it anyway: the second Guardian is not low when the
    # first detonates.
    landed: dict[int, dict[int, int]] = collections.defaultdict(dict)
    for source in hits:
        for rec in hits[source]:
            landed[rec["d"]][hits[source][0]["t"]] = rec["a"]
    pairs = []
    for guid, mine in landed.items():
        pool = trace.maxhp.get(guid) or 0
        if not pool:
            continue
        for first, second in chain_pairs(list(mine), NOVA_CHAIN_MS):
            pairs.append(((mine[first] + mine[second]) / pool * 100, trace.name(guid), second - first))
    if pairs:
        pairs.sort(reverse=True)
        over = sum(1 for share, _, _ in pairs if share >= 100)
        print(f"  two novas inside {NOVA_CHAIN_MS // 1000} s on one bar: {len(pairs)} pairs, median "
              f"{statistics.median(share for share, _, _ in pairs):.0f}% of it, {over} over the whole bar")
        for share, who, gap in pairs[:3]:
            print(f"    {who:14} {share:5.0f}%  {gap / 1000:.1f} s apart")

    # What each gate is worth as a warning, which is what decides where a gate can sit at all: one
    # shorter than the walk is a step-out that arrives after the blast.
    table = []
    for gate in NOVA_WARNING_GATES:
        got = sorted(warn for source in hits
                     for warn in [nova_warning(series.get(source, []), gate, hits[source][0]["t"])]
                     if warn is not None)
        if got:
            table.append((gate, got))
    if table:
        print(f"  the warning each gate gave  (a step-out walks for {STEP_OUT_WALK_MS / 1000:.1f} s)")
        for gate, got in table:
            tight = sum(1 for one in got if one < STEP_OUT_WALK_MS / 1000.0)
            print(f"    {int(gate):3}%  median {statistics.median(got):5.1f} s   shortest {got[0]:5.1f}"
                  f"   under the walk {tight:3} of {len(got)}")

    # A ladder row nothing picks leaves no verdict anywhere, so the only way to see one is to notice a
    # human of that class pressing what no bot does.
    bots = collections.Counter(rec.get("sp") for rec in trace.of("cast")
                               if rec.get("sp") in NOVA_DEFENSIVES and rec.get("s") in roster
                               and rec.get("s") not in trace.humans)
    humans = {rec.get("sp") for rec in trace.of("cast")
              if rec.get("sp") in NOVA_DEFENSIVES and rec.get("s") in trace.humans}
    pressed = ", ".join(f"{NOVA_DEFENSIVES[spell]} {count}" for spell, count in bots.most_common())
    print(f"  ladder rows a bot cast: {pressed or 'none'}")
    quiet = sorted(NOVA_DEFENSIVES[spell] for spell in humans if spell not in bots)
    if quiet:
        print(f"    a human pressed these and no bot did: {', '.join(quiet)}")


SECTIONS = (
    ("phases", "phase timeline, the Keepers freed, the pre-pull window, leftover Guardian deaths after the"
     " handover", show_phases),
    ("clouds", "cloud-orbit exposure per role", show_clouds),
    ("fervor", "Sara's Fervor runs, novas on holders, Guardians runners summoned", show_fervor),
    ("novas", "every Shadow Nova: the warning it gave, who it would have killed, what covered them", show_novas),
    ("threat", "who the Guardians were on, and taunts", show_threat),
    ("portals", "portal waves, assignments, and who never walked for one", show_portals),
    ("phase2", "Constrictor, Brain Link, node handovers", show_phase2),
    ("tentacles", "platform tentacle stock, Shattered Illusion stuns, leftovers at phase 3", show_tentacles),
    ("brain", "brain room, the Brain, skulls, reprisal deaths and holds, Induce Madness, stalled walks,"
     " walk speed to the Brain, turns on the spot, exits decided outside the brain room, skull gaze ticks,"
     " the hold short of phase 3", show_brain),
    ("phase3", "Heroism, the ranged station, station arrival and Nitro, Immortal Guardians, why they left"
     " the tanks, their power and Drain Life, the tank split and swaps, swings on the raid during a gaze,"
     " beacon heals, stacked heals, who else they reached, the tanks under them, drags and what the healed"
     " Guardian did to its holder, Deafening Roar and Aura Mastery, Lunatic Gaze, the damage split, area"
     " casts, healer mana, phase 2 leftovers and the pets on them", show_phase3),
    ("crush", "Crush and who took it, knockback, Death Rays and the last walk before each", show_crush),
    ("sanity", "Sanity minima, trips below, the bench, Rebirths into Insane", show_sanity),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
