# refine-beasts-3: the worms after AzerothCore #28181

Wave 4u, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[northrend-beasts.md](../../raids/trial-of-the-crusader/northrend-beasts.md). Base: the core merge `e2829262b`,
which brought #28181 (`6104cfdf0`).

## Why

2026-10-24, two 25H wipes on the worms: `env/dist/logs/botobs/649_39_gormok-the-impaler_1792873825.ndjson` and
`649_39_gormok-the-impaler_1792875225.ndjson`; the player saw no Slime Pool.

- **Pools:** summoned every 12 s (was 30) and living by difficulty, 60 s on 25H, so they grow to 20 yd; the bots
  capped them at 11. Worm phase: 30 and 32 deaths, Slime Pool 11.0 / 8.2k/s, 13 pool killing blows each, victims
  11-17 yd from 31-58 s pools. The pre-merge 25H kills (`649_1_…_1792691026`, `649_5_…_1792696592`): 0 deaths,
  0.3-0.6k/s.
- **Invisible pool:** core SmartAI casts 66882 on `JUST_CREATED`, before clients have the pool; no upstream fix or
  issue as of 2026-10-25.
- **Script changes the bots keyed on:** Spew at 11 s, at once on a mobile emerge, then 22 s; the survivor keeps
  submerging and flipping form.

## Scope (the user's calls)

1. Core visual: local only, `data/sql/custom/db_world/2026_10_25_00_slime_pool_visual.sql` (gitignored).
2. Pool radius to its despawn (summon timer), drag clearance per pool 15 s on, and on heroic the drag walks the
   worm along the edge (36-42 yd), falling back to today's search.
3. Survivor form flips; no drag on a stationary survivor; melee hit a lone stationary survivor and dodge only
   the Sweep cast. Spew arming per the new timers. Reader pool cap 20, docs.

## Targets (next 25H)

Slime Pool damage near 0.3-0.6k/s, no pool killing blows, `drag edge` notes on the mobile holder, pools
visible to players after recreating `ac-db-import`; 25N still kills.
