# refine-beasts — Northrend Beasts: worm-stage oscillation and hazards, Arctic Breath spread

Wave 4c. It runs after `w7-direct-targeting` merges and branches from that merge. Read what w7
changed in the Beasts stems first: DPS now pick their beast through a resolver, and no raid icons
remain. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). This
is a refine lane, so its findings come from a trace; line numbers below are as of `8f0f5975d`.

Trace (25N, one kill at 6:33, a human tank "Dragon", a build carrying waves 3 and 4):
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_gormok-the-impaler_1792096133.ndjson`.
Read it with:

- `postmortem.py <f> --probes move.by --during nb.stage=2` (worm stage 1:48-4:29);
- `postmortem.py <f> --death 0`;
- `bosses/northrend_beasts.py <f> --worms --cure --breath`;
- `postmortem.py <f> --probes nb.arc`.

## Findings to fix

**Worm-stage oscillation.** The action owning each move flipped A-B-A 713 times in 2.7 min, against
about 130 on Gormok and 40 on Icehowl. Pairs: `northrend worms reposition` against `reach melee`
(251), `reach spell` (244) and `follow` (155).

- *No held spot.* The reposition latches only its walk. On arrival it notes `clear`, erases the latch
  and returns false (`ToCActions_Jormungars.cpp:263-288,343-345`). `northrend worms move guard`
  needs the bot to be moving (`ToCMultipliers_Jormungars.cpp:35-58`,
  `ToCHelpers_Jormungars.cpp:1106-1120`). So generic movers win the next tick, and the next trigger
  re-plans from where the bot stands.
- *The spread rule.*
  - Casters space from all 24 members, the melee stack and tanks included, on a 9/11 yd band.
  - The ring search runs outward from the bot (`EncounterHelpers.cpp:489-559`) and ignores others'
    destinations.
  - It stays on with both worms under ground (`ToCHelpers_Jormungars.cpp:988-1002`).
- *Reach dropped.* Fallback tiers drop the role range limit (`ToCActions_Jormungars.cpp:305-320`).
  The ranged limit is a raw 28.5 yd against `reach spell`'s 37 yd trigger (`RangeTriggers.cpp:155-169`).
- *Melee flanks.* Melee land on the worm's front flanks, so `set behind` (`MovementActions.cpp:2801-2858`)
  moves them.
- *Submerge.* Bots lose their target and fall to the non-combat engine. Its `follow` fights the
  spread every 0.3-1 s.

**Hazards.**

- *No path check.* No move checks its path. `EncounterHelpers.cpp:487` says so, and
  `WormSpotStillSafe` (`ToCHelpers_Jormungars.cpp:1061-1072`) checks only the endpoint.
- *The one death.*
  - Stormweaver, a Bile carrier, walked 28 yd at 3:23.7 on a line 14 yd from the stationary worm.
  - The Sweep knockback threw it into a growing Slime Pool.
  - Its next spot ran the line through the pool's centre, and it died "holding" that walk at 3:30.
- *Bile spread.*
  - At 3:21.8 a Burning Spray (10 yd splash) gave Bile to 10 stacked melee.
  - The urgent escape drops the "keep off clean bots" circles, and 5 carriers chose one spot within
    90 ms.
  - 46 pulses (~110k) hit bots carrying neither debuff.
- *Toxin.*
  - 3 melee kept Toxin 60 s.
  - `DirectCureSpot` (`ToCActions_Jormungars.cpp:87-95`) tries one bearing, refused because it sat
    in a pool, with no fallback.
  - Stuck carriers wait for a runner, and only bots run.
  - Bile pulse 66870/67621/67622/67623 hits every ally within 10 yd of the carrier and removes
    Toxin. Toxin slows to a full stop and can't be dispelled.
- *Pools.* Generic `reach spell` walks casters into pools. A cast in progress counts as "locked" while
  the pool grows onto the bot.

**Arctic Breath.** The breath at 5:45.6 froze 8 melee, a healer and a ranged.

- Every melee bot held bearing slot −3.
- `nb.arc` flickered between 0 and N every ~100 ms beforehand. `IsBreathSpreadUp`
  (`ToCHelpers_Icehowl.cpp:499-515`) drops the spread whenever Icehowl's victim steps out of his
  melee range.
- Only 4-11 bearings survived the floor probe.

**Readers.**

- `--breath` finds no breath on 25N: 67650 reaches the trace only as aura and damage records, never
  as `cast`.
- `--cure` (`northrend_beasts.py:889`) attributes Bile pulses to their source, which is always
  Dreadscale.

## Scope

**Jormungars:**

1. *Held spot per bot* in `JormungarsState`.
   - It holds while the spot passes every rule at trigger distance and stays inside generic reach of
     the current target.
   - It releases when a rule breaks at the spot itself, the target or phase changes, or the anchor
     worm drifts past a deadband.
   - The action returns true while walking and false once parked.
   - The latch clears where the trigger turns off.
   - Template: Icehowl's dealt, clamped stand (`ToCHelpers_Icehowl.cpp:594-632`, thresholds
     `ToCHelpers_Icehowl.h:39-76`).
2. *Guard while the spot holds.*
   - Zero: reach, `set behind`, combat formation, follow, charge/blink/disengage.
   - Keep: attack, reach-to-heal, `avoid aoe`. Zeroing reach strands a bot whose spot is out of
     reach (`pitfalls.md`).
   - Template: the Yogg-Saron portal-hold `Block` rule (`UldDefinition_YoggSaron.cpp:213-217`). If
     w7 added a Beasts `EncounterDefinition`, declare it there.
3. *Path test.* `accept`, the escape search and `WormSpotStillSafe` reject a straight walk that
   crosses either hazard:
   - a pool, within its radius plus its growth over the walk time (radius 2 + 0.3 × tick);
   - an up stationary worm's Sweep, within 15 yd plus a margin.
   From inside a pool, run the escape search first, and allow only an exit moving away from the
   centre.
4. *Destinations count.* Other bots' latched destinations (`state.walks`) count as obstacles. A start
   angle by guid breaks ties.
5. *Bile carriers.* Each carrier gets a deterministic slot by guid, at least 10.5 yd from other
   carriers and clean bots. The escape keeps the clean-bot circles.
6. *Caster spread dealt per instance.*
   - Slots are sticky, re-dealt on emerge or when the anchor drifts.
   - The melee stack counts as one circle.
   - The band is trigger ≥ 10 yd, clear ~13.
   - Off while the stationary worm is under ground.
   - Role limits are in combat-reach terms. Relax spread before reach, and never drop reach for a
     non-urgent move.
   - Note the tier that picked the spot in `nb.wormmove`.
7. *Melee.*
   - Dealt spots on the mobile worm's rear arc, at least 108° off its facing.
   - Spread as widely as melee reach allows. Measure whether two groups 10 yd apart fit.
   - Clamped, and re-clamped only past a deadband.
8. *Submerge.* Zero follow for every bot while a worm is engaged (Malygos and Yogg-Saron precedent).
9. *Toxin cure.*
   - `DirectCureSpot` tries 16 bearings round the Bile carrier, or walks part of the way past 30 yd.
   - A stuck carrier with no runner keeps walking toward the Bile carrier, a human one included.
10. *Pools against generic movers.*
    - A guard zeroes `reach spell` and `reach melee` while their line crosses a pool.
    - "The pool reaches me before this cast ends" counts as urgent.

**Icehowl:**

11. `IsBreathSpreadUp` gets hysteresis: it stays up until the victim has been out of reach for a short
    window.
12. Melee are dealt across all rear bearings.
13. Find out why so few bearings survive the floor probe on 25N.

**Readers and tests:**

14. `--breath` falls back to grouping Arctic Breath aura applications by Icehowl.
15. `--cure` attributes each pulse to the carrier within 10 yd of its victim.

**Priority under the oversized-lane rule:** 1-5 first, then 11-12, then 14-15, then 6-10 and 13.
Write what doesn't fit into `refine-beasts-rest.PLAN.md` and list it in the report.

## Owns

- the `Jormungars`, `Icehowl` and `NorthrendBeasts` stems;
- `northrend-beasts.md`;
- `northrend_beasts.py` and its test.

`src/Util/EncounterHelpers.{h,cpp}` is open for additive helpers only, such as a segment-against-
circle test, listed in the report. Algalon's `DropForeignBooking` lives in Ulduar, which stays
untouched: write a ToC or shared helper instead.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
