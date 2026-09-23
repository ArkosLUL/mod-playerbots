# w6-closeout — whole-raid audit and wrap-up

Wave 5, last. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md).

## Scope

1. **Parallel audits**, one implementer group each; they may fix what they find.
   - Multiplier inventory: every ToC multiplier is gated to its encounter and tests the action
     family first.
   - Leftover `GetFirstAliveUnitByEntry` (deprecated upstream): replace it where a nearby-creature
     lookup or `GetGuidData` fits.
   - State leaking across bosses:
     - AI values set in one encounter and never reset (`disperse distance`);
     - raid-icon marks never cleared;
     - `prioritized targets`;
     - per-instance maps never pruned.
   - Per-instance state still in hand-rolled mutex maps: move it to `RaidInstanceState<State>` if
     that store has landed (lanes list them in their reports).
   - Lust budget across the raid: no Sated check exists anywhere, so make sure the rows don't spend
     lust twice where it matters.
   - No reliance on bot cheats.
2. **Docs.**
   - A final pass over `docs/raids/trial-of-the-crusader/`. Move generic lessons reported by earlier
     lanes into `docs/engine/`.
   - Delete `docs/plans/toc-rework/`. Its durable content must already live in the raid docs.
3. **Recipe skill.** Add RaidObs integration (probes, reader, the naming contract pattern, gating a
   raid whose script has no boss state) to `C:\Users\boss2\.claude\skills\raid-boss-strategy-recipe\SKILL.md`,
   via `compact-skill-creator`.

Out of scope: extracting one encounter gate shared with Ulduar. Ulduar stays untouched.

## Task list

_Filled by the investigator: numbered tasks in groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
