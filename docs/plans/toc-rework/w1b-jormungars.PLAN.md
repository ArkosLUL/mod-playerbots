# w1b-jormungars — Acidmaw and Dreadscale

Wave 4, after `w1a-beasts` merged its stage model. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Guide: `beasts-of-northrend-master-strategy-guide-toc-25/`.

## Scope

1. **Tanks.** Mobile/stationary tank split across the submerge form swap.
   - The worms wipe threat on every submerge (`boss_northrend_beasts.cpp`, the submerge handler), so
     use `RaidRedirectThreat` and re-establish the tanks on emerge.
   - Taunt ownership is not target ownership: gate on a second living tank (`docs/raids/ulduar/xt002.md`).
2. **Mechanics.**
   - Paralytic Toxin / Burning Bile pairing, where a burning player cures a toxin-slowed one per the
     guide; verify the actual mechanic in the script.
   - Slime pools: today `FleePosition`; move to hazard-clear stands.
   - The Sweep cone: take the shape from the DBC implicit target type. `HasInArc` and
     `IsBotInFrontalCone` take the full arc.
   - Spit/spray spread: a spread only helps once spacing exceeds the AoE radius.
3. **Kill timing.** Kill both worms within 10 s of each other, else enrage; add a DPS balance rule.
4. Verify the heroic differences and every Jormungar node `w0c-foundation` made live.
5. **Observability.** `nb.` probes (form, tank split, pairing); extend `northrend_beasts.py` and its
   test.

## Owns

Stem `Jormungars`; the Jormungars section of `northrend-beasts.md`; `northrend_beasts.py` and its
test.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
