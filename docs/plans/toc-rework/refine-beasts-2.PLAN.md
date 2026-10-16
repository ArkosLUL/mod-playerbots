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

Read `northrend-beasts.md` "Acidmaw & Dreadscale" and "Icehowl" first: mechanics, ids and what the bots
do today. Paths are under `src/Ai/Raid/ToC/` unless they start with `docs/` or `tools/`. No new files,
so no CMake re-run; no new rule or node, so no registration seam. Probe keys stay string literals on
the line of the call. No coordinates: the floor is GO 195527, so every spot derives from a unit and is
clipped live. Remap each new spell constant at the call (`GetSpellIdForDifficulty(SPELL_X, unit)`).
Keep the melee stand, the spread predicate and each search tier in named functions: `w8-prepull-staging`
derives its pre-pull stands from them.

### Contracts

| Key or name | Writer | Values |
|---|---|---|
| `WormMoveReason::Spray` | A | after `Pool`, before `Bile`; name `spray` |
| `WormSpotTier::Pair` | A | after `Full`, before `NoSpread`; name `pair`; parks |
| `nb.wormmove` | A | reasons gain `spray`, tiers gain `pair` |
| `nb.wormhold` | A | `set spray <tier>`, cause `release spray` |
| `nb.breath` | B | `ObsValue<uint8>` per instance: 0 off, 1 up, 2 grace, 3 moving, 4 daze |
| `nb.spread` | B | keeps the bot's step while the layout is held (3), `none` while off or in the daze |

Group C reads `WORM_SPLASH_RADIUS` and `WORM_SPREAD_TRIGGER` from the source through
`raidobs.geometry.radius`, so both stay `constexpr float` in `Util/ToCHelpers_Jormungars.h`.

### Group A — worms: Spray, spread, settle, Spew timer

Files: `Util/ToCHelpers_Jormungars.h`, `Util/ToCHelpers_Jormungars.cpp`, `Action/ToCActions_Jormungars.h`,
`Action/ToCActions_Jormungars.cpp`.

1. **Constants** in the header: `SPELL_PARALYTIC_SPRAY = 66901` (row 604), `SPELL_BURNING_SPRAY = 66902`
   (row 608); `WORM_SPRAY_TRIGGER = WORM_SPLASH_RADIUS + 2` (12), `WORM_SPRAY_CLEARANCE =
   WORM_SPLASH_RADIUS + 3` (13); `WORM_SPOT_FLOOR_RADIUS = 44` (2 yd inside Icehowl's gate-side charge
   end, `static_assert` against `ICEHOWL_CHARGE_REACH_GATE - 2` in the `.cpp`); `WORM_SETTLE_MS = 500`,
   `WORM_SETTLE_MOVE = 0.5`, `WORM_SETTLE_TURN_DEG = 5`; `WORM_SPEW_EARLIEST_MS = 15000`,
   `WORM_SPEW_LEAD_MS = 4000`. Enum values per the contracts, with their names.
2. **Spray read**, in `Refresh`: an up stationary worm whose `CURRENT_GENERIC_SPELL` is its own remapped
   Spray, preparing, gives `state.spray` = target guid (`m_targets.GetUnitTargetGUID()`), the target's
   position this read, the worm, Burning or Paralytic. Cleared otherwise and in `Disengage`.
3. **Spray rule** (urgent), in `GetWormMovePlan` for a bot past the holder return and not on a cure walk
   or run, both Sprays:
   - not the target: rule {`Spray`, target position, trigger `WORM_SPRAY_TRIGGER`, clearance
     `WORM_SPRAY_CLEARANCE`, `RULE_HARD | RULE_URGENT`};
   - the target: one such rule on each other living member within `2 × WORM_SPRAY_TRIGGER` of it, on
     their positions, so it walks off its neighbours while they walk off it.

   `UrgentAt` (so `BreakPinnedCasts` too) is true at a spot within the trigger of the target, or, for
   the target, with another member within it. A Spray hold never parks: one helper,
   `WormHoldParks(reason, tier)` = `WormTierParks(tier) && reason != Spray`, decides for the validity's
   walk-only test and for the action's AlreadyThere (no hold set).
4. **Spread peers**: the rule applies to a caster carrying neither debuff, while both worms live and the
   stationary worm is up, and not on a cure walk; peers are the other casters carrying neither, at
   their held spots (`HeldSpotOf`). Search circles stay: `WORM_SPREAD_CLEARANCE` on each peer, the melee
   stack on the mobile worm.
5. **Spread predicate** `SpreadFailsAt(spot, pairTolerant)`, replacing the peer circles' triggers (set
   them to 0; the melee-stack circle keeps its trigger):
   - near = peers whose held spot lies within `WORM_SPREAD_TRIGGER` of the spot; none: holds;
   - pair-tolerant and exactly one near peer with no other peer (self excepted) within the trigger of
     its spot: holds;
   - otherwise fails only for the bot with the highest guid among the bots of that set (itself and the
     near peers), so one bot of a too-close set moves and the rest keep their spots.

   Strict (`pairTolerant` false) for the bot's own spot with no hold and for every parked hold but
   `Pair` (tolerant) and `NoSpread`/`Partner`/`Direct` (skipped, as now). `FirstFailedRule`'s
   `skipSpread` becomes that mode; the cause stays `spread`.
6. **Pair tier**: `plan.pairCircles` = `spreadCircles` less the circle of the pair partner, the peer
   nearest the bot (by held spot) with no other peer within the trigger of its spot; empty with no such
   peer. `plan.spreadStrict` = heroic and Dreadscale is the up stationary worm.
7. **`FindWormSpot`**: `full`, then `pair` (`circles` + `pairCircles`, `fits`) when `pairCircles` is
   non-empty, then `nospread` unless `plan.spreadStrict`; the rest unchanged. A strict non-urgent move
   with no spot backs off as now.
8. **Floor**: `AcceptFailure` tests `WORM_SPOT_FLOOR_RADIUS`; the drag's `OnWormFloor` keeps
   `WORM_FLOOR_RADIUS`.
9. **Settle**: per worm slot `motionMs` and `motionPos` (with facing), stamped whenever the worm
   `isMoving()` or sits past `WORM_SETTLE_MOVE` or `WORM_SETTLE_TURN_DEG` from the stamp; reset on
   `Disengage` and a new guid in the slot. `WormRead.settled` = still `WORM_SETTLE_MS`. The melee stand
   (`rules.hasStand`) needs the up mobile worm settled; `AnchorDrifted` tests drift only once the anchor
   has settled (gone or under ground still releases).
10. **Spew timer**: per worm slot `spewScheduleMs`, set on the slot's first read of a guid (engage),
    the submerged → up edge (emerge) and the rising edge of `spewing` (cast start).
    `WormRead.spewArmed` = spewing, or `WORM_SPEW_EARLIEST_MS - WORM_SPEW_LEAD_MS` since the schedule.
    `rules.cones` and `accept->cones` take only armed up mobile worms; `UrgentAt`'s spewing test stays.

### Group B — Icehowl: daze and walk

Files: `Util/ToCHelpers_Icehowl.h`, `Util/ToCHelpers_Icehowl.cpp`; `Trigger/ToCTriggers_Icehowl.{h,cpp}`,
`Action/ToCActions_Icehowl.{h,cpp}`, `Multiplier/ToCMultipliers_Icehowl.{h,cpp}` only if needed.

11. **Constant** `ICEHOWL_SPREAD_SETTLE_MS = 500`. `BreathSpread` gains `Moving` and `Daze`.
12. **`ReadBreathSpread`**, in order: passive, another beast engaged, a latched charge: `Off`; Staggered
    Daze on him: `Daze`; no victim or not a player: `Off`; he `isMoving()` (stamp `lastMovingMs`) or
    moved under `ICEHOWL_SPREAD_SETTLE_MS` ago: `Moving` when latched, else `Off`; victim in his melee
    range: `Up`; else the grace as now.
13. **`RefreshBreath`**: `Daze` acts as `Off` (latch dropped, `nb.arc` 0, deal kept); `Moving` keeps
    the latch, deal and `active` with no re-latch, so `HasIcehowlBreathSlot` and the guard hold, but
    `GetIcehowlBreathStand` returns false and keeps the bot's `nb.spread` step. Re-latch on the first
    `Up` read as now (8 yd, 30°, room). Write `nb.breath` each refresh.

### Group C — reader and test

Files: `tools/botobs/bosses/northrend_beasts.py`, `tools/botobs/tests/test_northrend_beasts.py`.

14. **Banner**, heroic (`diff` 2, 3): Berserk 26662 at its `cast` row from a beast guid, else due at
    509 s (the trace opens at Gormok's engage, 11 s after the intro's end), `not reached` past the end;
    each beast's health at it from its last sampled row, `dead` with none.
15. **`--charge`**, per `daze` cycle over its 15 s from the outcome: per bot the share of snapshots
    moving, yards walked, the action owning most accepted `move` rows, its `cast` rows; sorted by share.
16. **`--breath`**: `nb.breath` at each breath, seconds since Icehowl's last Whirl `cast` row and his
    displacement over the 4 s before it.
17. **`--worms`**: hold releases by cause, raid-wide per role; per Spray cast (the eight ids, from a worm
    guid) the target and role, who stood within `WORM_SPLASH_RADIUS` of it at cast end (snapshot nearest
    `t + ct`), who took Bile or Toxin (applies 0.8-4 s after, first 400 ms), and `spray` `nb.wormmove`
    branches per bot in the cast; the caster spread over the stage's snapshots with both worms up:
    pairs of casters carrying neither debuff within `WORM_SPREAD_TRIGGER`, median and max, and the share
    with one caster beside two others.
18. **Test**: a heroic trace with and without a Berserk row, a normal one without the line; a daze
    window with a moving and a still bot; a Spray cast with its applies and a `spray` branch; an
    `nb.wormhold` release tally; `nb.breath` read at a breath. `EveryView` still reads every section
    empty on another boss.

### Integrate

Syntax-check every changed `.cpp` plus `Util/ToCHelpers_Jormungars.h` and `Util/ToCHelpers_Icehowl.h`
(fan-out 60); pblint with `--spell-difficulty`; the Python suite; `python apps/codestyle/codestyle-cpp.py`.
Fold `northrend-beasts.md` with `compact-docs-writer`: the worm and Icehowl "What the bots do" for every
task with these decisions' rationale (the stand bullet's "the first breath after a charge is 5 s or
more out" becomes the two cases), the Known gaps below, and the trace table (`wormmove` `spray` and
`pair`, `wormhold` `spray`, `breath`, the new reader output).

## Decisions for review

Trace figures: second pull `…1792180155` (P2), first `…1792179491` (P1), both 25H.

| Choice | Source | Alternative |
|---|---|---|
| Spray dodge at the cast: bots within 12 yd of the target walk 13 off it, the target walks 13 off members within 24, forced; holders and cure walks hold; both Sprays | `Spell::_cast` picks the splash at cast end; most casters hit stood 4-7.4 yd from the target at the cast's start, melee mostly 0-3 yd, so only both walking opens 10 yd in ~0.9 s | spacing alone |
| No tightening ahead of the Spray timer | spacing is held statically and the dodge covers the cast; the timer slips 20.1-21.1 s; re-spacing before each Spray walks every caster every 20 s | strict spacing from 4 s before the earliest Spray |
| A Spray hold releases on arrival | after the cast melee walk back to their dealt stand instead of merging on the other group's side | park it |
| A parked hold re-tests spread; of a too-close set only the highest guid moves | scope 0; P2: 14-15 caster pairs stood within 10 yd from 3:33 to 3:47, four `release spread` all stage, most holds parked on `nospread` | both move |
| `pair` tier after `full`, parked pair holds tolerate one partner with no other neighbour | guide: "okay if you are in range of one other player"; P2 3:23.0, before any Bile: the floor in reach packs 6 casters at 13 yd (8 at a 44 yd floor), 12 as pairs (16) against 14 | strict only, which that layout can't fit |
| No `nospread` on heroic while Dreadscale is the stationary worm; `pair` and urgent escapes still run | scope 0: a shared Burning Bile kills at the first pulse there; a Paralytic Spray doesn't | never on heroic |
| Spread between casters carrying neither debuff | carriers keep the Bile and cure rules | Bile casters as peers, as before |
| Spot floor 44 yd from `ARENA_CENTER`; drag and surfacing keep 35 | the floor reaches past 46 yd everywhere (Icehowl's charge end, the twin orbs at 47, players 49-60 yd out); at P2 3:23.0 it turns 432 yd² of caster floor into 626 | 35 |
| Melee Bile carriers keep their rule (12 yd off clean bots, the escape keeps them) | the dodge cuts them from a group to one; at P2 3:23.0, before any melee carried Bile, the casters already stood 8 pairs deep: the stack came from space and parked `nospread` holds, not the carriers | a carrier zone behind the mobile worm, inside the casters' 19.3-22.3 yd ring |
| Ranged reach stays the mobile worm | the doc's kill order; conservative | ranged take the stationary worm out of the mobile one's reach: 12-15 strict spots at P2 3:23.0 instead of 6-8 (carried over) |
| Melee stands and drift wait for a worm still 500 ms | 85-94% of melee drift releases came while a worm walked (P2 453/532, P1 351/372); a worm walks 10-15% of the time | a wider deadband, still one release per few yards of a walk |
| Spew cones count from 11 s after the worm's engage, emerge or last Spew cast start, or while it spews | the script reschedules Spew at 15-30 s from each; 79-89% of Spew moves (P2 139/175, P1 101/113) fell inside that dead window, mostly during the Emerge cast's facing swings | hysteresis on the cone, which a 40-80° swing beats |
| Breath stands off for the whole daze, back when Staggered Daze falls | the breath comes 5-8 s after the daze ends and the Whirl 2-5 s, which walks him off the layout anyway; a walk in the last seconds costs +100% damage time | stands back in the daze's last 2-3 s |
| The layout holds without walks or re-latch while Icehowl moves and 500 ms after | P1 8:02-8:05: his victim stayed 8-15 yd, mostly inside his 14.8 yd reach, through a 36 yd Whirl walk, so `nb.arc` moved three times and 24 stand walks issued in 4 s; no bot crossed his front (all ≥ 107° off his facing at the breath); the 17 frozen were the raid compressed behind him by the walk, which neither option saves | re-latch on his predicted end |
| `nb.breath` per instance | `nb.arc` can't tell a held layout from a live one, nor the daze from off | none |
| Berserk due 509 s into the trace without a cast row | engage is 11 s after the intro's end, Berserk 520 s after it; P1's cast at 508.986 s | no line |
| `WormMovePlan::pairable` runs the pair tier whenever there is a partner, not when `pairCircles` is non-empty | implementer: with one peer and the mobile worm under ground the partner's circle is the only spread one, so `pairCircles` is empty and the literal test skips the tier, which matters on heroic where `nospread` is off | task 7 as written |
| Pair partner: the nearest peer among the lone ones | implementer's reading of task 6; never pairs into a trio | the nearest peer, only if lone |
| `UrgentAt` takes `sprayDodge`: the plan passes `!cureWalk`, `BreakPinnedCasts` true for every member | implementer: a cure walk gets no Spray rule, so it mustn't turn urgent from one; the cast break can't tell a cure walk, so a pinned cure walker by a Spray target loses its cast, then walks its cure | skip the Spray in the cast break |
| Spray ids remapped with `bot` at the call in `Refresh` | implementer: same map so same difficulty, as the Spew ticks; pblint `--spell-difficulty` accepts only a constant inside the `GetSpellIdForDifficulty` call | remap on the worm |
| A slot clock keeps its stamps while the slot reads no worm; only another non-empty guid or `Disengage` resets it | implementer: a one-tick null read would restart the Spew timer | reset on an empty read |
| A parked `full` hold is tested strictly | implementer, per task 5; churn for review: a lower-guid bot taking a `pair` spot beside a lone `full` holder makes that holder (highest guid of the set) let go, and it usually re-holds a yard or two away as `pair` | test `full` holds pair-tolerant |
| `state.spray` stores the worm guid and Burning flag | task 2; nothing reads them yet | drop them |
| Icehowl's `isMoving()` stamp on every `ReadBreathSpread` call, before the passive, stage and charge tests | implementer: the first `Up` latch after a rage also waits 500 ms after his last move; steps 1-5 return as task 12 says | stamp only at step 6 |
| While Moving, `victimOut` clears | implementer: the 1 s grace counts only while he stands, else a chase uses it up and the layout drops as he settles with the tank at his reach's edge | let the grace run |
| While Moving, the roster rebuild and sticky deal keep running; only the re-latch is skipped | implementer: as in the grace | freeze the deal |
| A stand walk in flight when he starts moving finishes; none issues until he stood 500 ms | implementer | cancel it |
| Disengage writes `nb.breath` 0 and clears the moving stamp | implementer | leave the last value |
| Reader: daze casts leave out triggered (`tr`) rows | implementer: procs swamp them; P1 daze 1 Obliteration 157 triggered against 13 own, most bots 2-12× | count all |
| Reader: Spray "took" counts only that Spray's debuff (Toxin for Paralytic, Bile for Burning) from the casting worm; "within" includes the target | implementer | any debuff apply |
| Reader: caster spread counts roster ranged and heal members, humans included, `nb.worm` 1 or 2 as both up, a debuffed caster out with the cure section's 250 ms slack; "one beside two others" = snapshots where some clean caster has two within the trigger | implementer | bots only |
| Reader: `BILE_PULSE_RADIUS` renamed `WORM_SPLASH_RADIUS` | implementer: one source constant serves the pulse and the splash | keep the name |
| Melee spots skip the rear-arc test while the mobile worm walks, as the stand does | review: its facing mid walk is as stale as the stand; a hold taken mid walk lets go on `drift` within ~0.5 s of the stop and the stand places it; testing the arc mid walk re-releases walking holds on `melee` as the worm turns | rear arc on every melee spot, as before this lane |
| A Spray dodge walk keeps going after the cast ends, until it arrives | review: the splash's victims are fixed at the cast's end, so the rest of the walk costs at most a second or so; releasing it early needs a second release cause | release a Spray hold once no Spray is being cast |

## Carried over

- Ranged on the stationary worm when out of the mobile one's reach (decision above): a kill-order call
  for review, with its own targeting and reach changes.
- Merge stage, engine lesson candidates for `raid-mechanics-lessons.md` "Reading a boss script": a
  `TARGET_DEST_TARGET_ENEMY` area is placed when the cast ends, so its cast bar is the dodge window; a
  boss chasing a knocked-back tank keeps his victim in reach, so a reach test misses his walk and his
  own `isMoving()` reads it; a hazard the script reschedules on a phase edge can be gated off its timer.
- Outside ToC: nothing.

## Known gaps

- After a daze the breath usually lands as Icehowl finishes following the Whirl-thrown tank, with the
  raid compressed behind him; only tanking his back to a wall (no throw) avoids it.
- On heroic with Dreadscale stationary, a caster with no `full` or `pair` spot stays where it stands.
- The Spray target keeps off members within 24 yd only, so its walk can end inside 10 yd of one further
  out; a holder targeted holds still and its neighbours walk.
- A Spray on a pinned, stacked melee group can still splash it: the walk needs ~0.9 s of the cast.
- Ranged reach binds to the mobile worm, so with it at the arena edge the floor in reach stays small.
- While the mobile worm walks, melee follow by generic reach and take their stand once it settles.

## Blocked

None.
