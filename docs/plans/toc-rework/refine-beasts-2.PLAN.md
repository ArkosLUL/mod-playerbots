# refine-beasts-2 — Northrend Beasts: Spray spacing, no breath walk in the daze, worm hold drift

Wave 4d, a refine lane after `refine-beasts` (merged `0bf1b5182`); read that brief's "Decisions for
review" for the worm hold and breath spread it shipped. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics and the shipped design:
[northrend-beasts.md](../../raids/trial-of-the-crusader/northrend-beasts.md). Line numbers are as of
`1846fc944`; core script paths are under
`src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/` at the server root.

Traces (25H, both wipes, human tank "Dragon", build carrying waves 3-4c), in
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/`:

- `649_1_gormok-the-impaler_1792179491.ndjson`: wipe on Icehowl at 3.5%;
- `649_1_gormok-the-impaler_1792180155.ndjson`: wipe from the worm stage.

Read them with `bosses/northrend_beasts.py <f>` (sections `--stage --charge --breath --worms`),
`postmortem.py <f> --probes move.by --during nb.stage=2` (worm stage) and `--probes nb.wormhold`.

## Findings from the 2026-10-16 25H pulls

**Second pull: a Spray stack wipes the casters.**

- 3:27.2: a Burning Spray from the stationary Dreadscale gave Bile to five melee and the tank.
- The carriers keep 10.5 yd from clean bots, which took most of the free space. The casters' spot
  search fell to its `nospread` tier (`ToCActions_Jormungars.cpp:143-184`,
  `ToCHelpers_Jormungars.cpp:1643`) and parked. A parked hold never re-checks spacing when a
  neighbour parks next to it.
- By 3:47.4 six casters and healers stood within 11 yd of each other, four within about 4 yd. The
  3:47.5 Spray (10 yd splash, every 20 s on a random player within 100 yd) hit all six. The first
  Bile pulse 2 s later (8.3-9.7k on 25H, everyone within 10 yd) killed Holylight, Power and
  Malediction within 16 ms, Tree and Hellflame by 3:58.
- Bulwark died to Acidmaw's melee plus 16 Bile pulses (85k). Dreadscale died at 4:15; enraged
  Acidmaw then killed both tanks and the ranged one by one, and the raid wiped early on Icehowl.

**First pull: the stun walk.**

- A clean charge gives Staggered Daze: 15 s stun, +100% damage taken, his events delayed 15 s
  (`boss_northrend_beasts.cpp:835-851`), so his first breath after a crash is 20-23 s away.
- The breath spread still runs in the daze: its "on" test has no daze case, and
  `northrend-beasts.md:507` says "the first breath after a charge is 5 s or more out", true only
  after a rage. The charge latch reads `Daze = 4` (`ToCHelpers_Icehowl.cpp:37`).
- The charge dodge left ranged and healers 24-48 yd out. In daze 1 (6:33-6:48) they walked 37-50 yd
  each, moving 34-51% of the stun, all `icehowl move to breath stand` (`ToCTriggers_Icehowl.cpp:68`);
  daze 2 was 10-57%. Lust went out 3.3 s into daze 1, so the walk ate its start too.
- The same bot did 30-50% more damage in the daze where it moved less (Agony 15.8k/s at 15% moving
  against 12.1k/s at 47%). Across the eight living ranged, roughly 0.8M over two dazes: about what
  Icehowl had left when heroic Berserk 26662 (+500% damage, 520 s after the intro,
  `instance_trial_of_the_crusader.cpp:624-644`) landed. The next Massive Crash killed 18 in 0.6 s.
- At 8:04 a Whirl threw the human tank about 30 yd; Icehowl followed and breathed 2 s later on a
  raid still walking across his front: 16 frozen.

**Worm hold drift, both pulls.** `reposition` ↔ `reach melee` flipped 248 times and ↔ `reach spell`
91 times in the first pull (Gormok stage: 32). `nb.wormhold` shows the hold letting go on `drift`
every 0.2-0.4 s for melee (67-76 times per melee bot in the second pull), about every 0.8 s for
casters, and a Spew-wedge hold every 0.2 s. The melee stand is anchored on the mobile worm, which
moves every tick, so its drift passes the deadband at once (`ToCHelpers_Jormungars.cpp:888,1674`,
`IsWormSpotHeld` at 1812).

## Scope

0. **Spray spacing** (ranked first).
   - No two non-carriers stay inside one Spray splash (10 yd plus a margin). A parked hold lets go
     when another non-carrier parks inside that radius; one of the two moves, chosen by guid, with
     hysteresis so the hold doesn't flicker.
   - The spread tier is the last to drop; while a stationary Dreadscale lives on heroic it never
     drops, since a shared Bile kills there at the first pulse.
   - The timer is known: first Spray 15 s after Dreadscale's and 20 s after Acidmaw's reschedule,
     then every 20 s (`boss_northrend_beasts.cpp:516,625-627`). The investigator decides whether to
     tighten spacing ahead of it.
   - Decide from both traces how melee Bile carriers leave their group without crowding out the
     casters.
1. **No breath walk in the daze.** While the latch reads daze, the breath spread is off: no stand
   moves; ranged and healers cast from where they stand, and generic `reach spell` and heal reach
   close range only when they need to. The stands come back at the daze's end, with the first breath
   still 5-8 s away; the investigator picks the exact edge (daze end or its last 2-3 s) and logs it.
   The doc's "5 s or more out" becomes two cases: after a rage, after a crash.
2. **Worm hold drift.** A hold anchored on a moving worm must not let go every tick: size the drift
   deadband to the anchor's motion, or re-deal melee rear spots only past a larger turn or move of
   the worm. Stop the Spew-wedge hold flickering. Target: worm-stage `reach` ↔ `reposition` flips
   near the Gormok stage's count.
3. **Breath after a Whirl.** When he moves 30 yd after his tank, the re-latched bearings send the
   raid across his front. Decide from the first trace whether a stand walk avoids his frontal cone,
   or holds until he settles.
4. **Reader.** The banner shows the heroic Berserk time and each beast's health at it; `--charge`
   adds each bot's daze: share of time moving, the mover, casts; `--worms` counts hold releases by
   reason.

**Wave 4e** (`w8-prepull-staging`) reads the worm caster slots, melee rear spots and breath bearings
for its pre-pull stands: keep them reachable through helpers.

## Owns

- the `Gormok`, `Jormungars`, `Icehowl` and `NorthrendBeasts` stems;
- `northrend-beasts.md`;
- `northrend_beasts.py` and its test.

`src/Util/EncounterHelpers.{h,cpp}` is open for additive helpers only, listed in the report.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
