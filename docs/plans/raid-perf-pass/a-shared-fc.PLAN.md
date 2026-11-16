# a-shared-fc: shared raid helpers and ToC FC/Beasts multipliers

Lane of [raid-perf-pass.PLAN.md](raid-perf-pass.PLAN.md) (wave 1, beside `t-triggers`). The overview's
neutrality rule, file ownership, git, Docker and verification rules bind this lane.

Implement the overview's "Wave 1 — lane `a-shared-fc`" items 1–6 (A0 splash cache API, R4 cares
table, N2 `RaidBurstWindow`, the `RaidObsEngine.cpp` guard, N1/R3 cache parts, T7 one opener sweep).
Line references there predate `44b6dcaa0`; re-verify against Custom.

- Multipliers run once per popped useful action, so per-call savings multiply. Baseline totals over a
  full 25H clear (2026-11-16): FC grip guard 749 ms, `hold burst until tank engaged` 638 ms, FC
  suppress aoe 489 ms, then ~0.3 µs each across the other ToC multipliers. Dumps:
  `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/perf/_pmon/run2/`.
- Shared helpers keep every old signature: the ToC upstream PR later ports or skips these changes,
  and other raids keep calling the old forms.
- `RaidOpener.cpp` carries both A0 adoption and T7; commit them separately. T7: `ReadOpener` does one
  entry-set sweep instead of one per boss entry, buckets hits per entry in config order and keeps
  visit order inside each bucket, so `bosses` comes out identical. Proof: bucket-order harness.
- R4 touches `RaidEncounter.h`/`RaidEncounterRules.h`, which fan out to 100+ TUs: on `INCOMPLETE`,
  rerun `pb-check` with `PB_MAX_FANOUT=all` in the background and commit only once green.
- Lane `t-triggers` owns the ToC `Util` helpers in parallel; a change you need there goes in your
  report, never into the file.

One commit per fix, each compiling (`pb-check` over its paths), harness files in
`build-toc-wt/perf/_harness/a-shared-fc/`, never committed.

## Task list

## Candidates done

## Candidates dropped

## Decisions for review

## Known gaps

## Blocked
