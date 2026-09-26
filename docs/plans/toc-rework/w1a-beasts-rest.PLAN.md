# w1a-beasts-rest — Arctic Breath spread, Fire Bomb impact

Carried over from `w1a-beasts` under the oversized-lane rule; runs once that lane has merged. Rules,
sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics, ids and the
stage model: `docs/raids/trial-of-the-crusader/northrend-beasts.md`. Guide:
`beasts-of-northrend-master-strategy-guide-toc-25/`.

## Scope

1. **Arctic Breath spread.**
   - The cone is 60° on 10N and 24° on the other three, 100 yd, aimed at a random target within 90 yd;
     5 s freeze plus damage.
   - Guide: ranged and melee in a half circle behind Icehowl, healers far apart.
   - Spread by bearing around him, sized past the cone's width at the stand radius: a spread tighter
     than the AoE buys nothing (`docs/engine/pitfalls.md`, gather/spread).
   - He is held where each charge leaves him, so the stands are boss-relative: clamp, don't chase
     (`docs/engine/raid-mechanics-lessons.md`). Stand down while `icehowl charge guard` is latched.
2. **Fire Bomb impact.** NPC 34854 appears at its target; 66317 hits 8 yd around it after a 1 s cast
   and a 14 yd/s missile from Gormok. Dodge a bomb younger than the impact, read from
   `TempSummon::GetTimer()` against its 60 s lifetime, to a clearance past 8 yd. The 2 yd pulse stays
   with the generic `avoid aoe`.
3. **Observability.** `nb.` probes for both, and the matching sections in
   `tools/botobs/bosses/northrend_beasts.py` and its test.

## Owns

The Arctic Breath part of the `Icehowl` stem, the Fire Bomb part of the `Gormok` stem, their sections
of `northrend-beasts.md`, `northrend_beasts.py` and its test.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
