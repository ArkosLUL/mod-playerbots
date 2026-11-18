# c4-toc-closeout

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority) and the
**`c4-toc-closeout`** paragraph bind this lane. Docs and skill only, no C++.

Branch `tocpr/c4-toc-closeout` from `Custom`, after `c1`–`c3` merged (check the Status table). It
finishes the ToC rework's closeout: `docs/plans/toc-rework/w6-closeout.PLAN.md` scope items 2 and 3.
Scope item 1 (code audits) was done by `c1`–`c3`; their briefs in this directory record the result.

1. Final pass over `docs/raids/trial-of-the-crusader/` per `docs/README.md`: verified facts,
   decisions with rationale, traps, known gaps; no change lists. Fix every name the wave 1 renames
   left stale.
2. Move generic lessons from the ToC rework into `docs/engine/` (`pitfalls.md`,
   `raid-mechanics-lessons.md`), each stated once.
3. Re-check RaidObs's ToC trace close (kill credit or boss-state fall) and
   `docs/systems/observability.md`'s rule about `IsEncounterInProgress` with nobody alive against
   core #28117 (`instance_trial_of_the_crusader.cpp` now keeps boss state); correct the docs.
4. Fold `docs/plans/toc-rework/w5-anubarak-rest-rest.PLAN.md`'s open items into `anubarak.md`'s
   Known gaps.
5. Delete `docs/plans/toc-rework/` once every durable fact in it lives in a permanent doc.
6. Add a RaidObs integration section (probes, reader, naming contract, gating a raid whose script
   has no boss state) to `C:/Users/boss2/.claude/skills/raid-boss-strategy-recipe/SKILL.md` by
   invoking the `compact-skill-creator` skill. That file is outside the repo: edit it in place, it is
   not committed.

Docs go through the `compact-docs-writer` skill (standing approval in `CLAUDE.local.md`).

## Task list

Not split: docs only, about 70 changed lines. The rework's merge stages had already folded nearly
all of its durable content.

1. [x] Stale names. Every backticked name in the ToC docs was parsed and checked against `src/`,
   `tools/` and the core. Fixed: `RaidPredictedHit`, `RaidCover`, `GetRaidCover`,
   `GetRaidPersonalDefensive`, `FinishRaidCover` (was `ToC*`, `Util/ToCHelpers_Defensive`) in
   `lord-jaraxxus.md`, `twin-valkyr.md`, `faction-champions.md`; `CastRaidThroughputHeal` in
   `northrend-beasts.md`; `set behind` (was `set behind target`); Spike Fail 66181 (was `SPIKE_FAIL`).
2. [x] Core #28117 re-check. `instance_trial_of_the_crusader.cpp` calls `SetBossNumber(5)` and has
   no `IsEncounterInProgress` override any more. RaidObs takes the credit/fall path only when
   `GetEncounterCount()` is 0 (`RaidObsLifecycle.cpp`), so ToC traces now close on `DONE`/`FAIL`/
   `NOT_STARTED`. Beasts goes `DONE` only once every beast is dead, Twins on either twin's death.
   `observability.md`'s credit/fall section now covers VoA only, and the Twins no-credit gap is gone
   from it and from `twin-valkyr.md`. README "Instance script" states the boss-state close and drops
   "no object data" (the script calls `LoadObjectData`). `raid-mechanics-lessons.md` was already
   correct.
3. [x] `w5-anubarak-rest-rest` folded into `anubarak.md` Known gaps: the guide's answer for each
   open item, Mandibles 65775/67861/67862/67863, the 2026-10-16 25N figures. The direct-heal gap now
   says what is true: Jaraxxus uses `CastRaidThroughputHeal`, Penetrating Cold keeps its own list.
4. [x] Generic lessons. Each carried-over engine lesson in the rework's briefs was checked by its
   key term and was already in `docs/engine/` or `docs/classes/`. Added the two found only in boss
   docs: single-target CC per caster (`SPELL_ATTR5_LIMIT_N`) and damage that pushes back no cast
   (`SPELL_ATTR3_TREAT_AS_PERIODIC`, verified on 66240) in `raid-mechanics-lessons.md`, with the boss
   docs linking to them. Added pblint's `GetSpellForDifficultyFromSpell` blind spot to `pitfalls.md`.
5. [x] Stale facts the rework recorded outside ToC: VoA's "`spelldifficulty_dbc` maps … nothing
   else" (the DBC adds rows 481 and 2132); VoA's Freezing Ground dodge never fires on 25-man (keys
   on 72090 alone); the ToC README's "harder, needing new state tracking" line.
6. [x] Deleted `docs/plans/toc-rework/` (41 files). Each brief's Decisions, Carried over and Known
   gaps were spot-checked against the permanent docs by distinctive terms. Decisions later lanes
   superseded were not carried.
7. [x] RaidObs section in `C:/Users/boss2/.claude/skills/raid-boss-strategy-recipe/SKILL.md` (naming
   contract, probes, reader, no-boss-state gate and close), via `compact-skill-creator`: 502 → 642
   words. Outside the repo, not committed.
8. [x] Review: every changed claim re-checked against the core, the DBC CSVs, pblint and RaidObs.
   Fixed `raid-mechanics-lessons.md`'s two `anubarak.md#the-kite-plan` links (the heading is "The
   kite planner"), broken on Custom already. Two findings left open (Known gaps).

Word deltas (`wc -w`): README 5304 → 5290, anubarak 31543 → 31686, faction-champions 17389 →
17358, twin-valkyr 5116 → 5099, observability 9097 → 8991, pitfalls 12624 → 12635,
raid-mechanics-lessons 10499 → 10588, vault-of-archavon 1301 → 1332; lord-jaraxxus and
northrend-beasts unchanged.

## Decisions for review

- `RaidObsLifecycle.cpp:385`'s comment still cites heroic Icehowl as the latch case, stale since ToC
  keeps boss state. It's C++, out of this docs lane: a later Custom pass. The alive-player guard there
  is still needed for VoA.
- `lord-jaraxxus.md` "Raid cover" still restates part of the shared deal in `docs/raids/README.md`,
  interleaved with Jaraxxus-only predictions. Left as is.
- `docs/plans/icc-rebuild/` (`r0a-restructure`, `r0b-guides`, `r14-closeout`) points at deleted
  `toc-rework` files. Git keeps them (`git show 2438a2d6d:docs/plans/toc-rework/<file>`). `r14`'s task
  to correct `toc-rework.PLAN.md`'s Wowhead line is now moot. ICC's plans aren't this lane's files.
- The recipe skill has no `version` field, so nothing was bumped. Flagged, not added.
- Tooling left as is (not docs): `tools/botobs/tests/test_toc_naming.py:60,61,65,201,202` sample
  removed trigger names (they pass, prefix only), and four boss readers' banners list `toc.stage`/
  `toc.aura` as silent on pulls with no staging window.

## Known gaps

Code debt from the rework's `w6` list, still on Custom, which `p3` would export unless a lane drops it:
- Dead: `AvoidCreatureClusterAction` with `FleeFromCreatureCluster` (`ToCActions_Shared`),
  `GetCreatureClusterCenter` and `GetNearestCreatureByEntry` (`ToCHelpers_Shared`),
  `ANUBARAK_PIT_CENTER` (`ToCData`), `SPELL_MASSIVE_CRASH_*` (`ToCData.h`, unread).
- Duplicated: `TryClassTaunt` (Anub'arak and Jormungars actions); segment distances
  (`DistanceToSegment2d` Jaraxxus, `SegmentDistance2d` Anub'arak, `DistanceToIcehowlLane`) beside
  `EncounterHelpers::SegmentCrossesCircle`; the spell-mover tests (`IsBlinkOrDisengage`,
  `IsSpellMover`, `CompetesWithChargeDodge`); the Jaraxxus and Anub'arak threat caps;
  `NorthrendBeastsWalkAction` serves FC and Staging from the NorthrendBeasts header.
- Outside ToC: VoA's Freezing Ground dodge is dead on 25-man (now in `vault-of-archavon.md`).

Review findings left open:
- `docs/raids/README.md:88`'s open gap says moving ToC onto rows "waits on the ToC rework's closeout",
  which this lane finished; the gap itself stays open. Not a lane file (other sessions edit it).
- The recipe skill's RaidObs naming bullet has one line at ~118 columns, the rest wrap at 100.
  Cosmetic, outside the repo.

## Blocked

None.
