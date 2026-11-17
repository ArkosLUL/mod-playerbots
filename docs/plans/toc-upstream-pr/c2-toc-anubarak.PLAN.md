# c2-toc-anubarak

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the
**Wave 1** section bind this lane.

Cleanup on `Custom`, branch `tocpr/c2-toc-anubarak`. Apply the overview's "Every cleanup lane"
list to the `c2-toc-anubarak` files of the Wave 1 table, plus the `"tree of life"` → `"tree form"`
fix it assigns to `c2`. Measurements cut from comments go to
`docs/raids/trial-of-the-crusader/anubarak.md`. RaidObs calls stay. A change that alters behaviour
beyond the listed ones is out of scope: log it under Decisions for review.

Big lane (`Util/ToCHelpers_Anubarak.cpp` alone is ~10k lines): split into at most 3 implementers
with disjoint files; the finisher runs the checks.

Lane facts:
- Most long comment blocks live here (`ToCHelpers_Anubarak.cpp` 52, `ToCHelpers_Anubarak.h` 24),
  along with most dated and "user call" lines (e.g. `Util/ToCAnubarakKitePlanner.h:34`,
  `ToCHelpers_Anubarak.cpp:149-153`).
- `PlayerbotAI* ai` locals at `ToCHelpers_Anubarak.cpp:1202, 2668, 4343, 8749, 8859` hold another
  player's AI: name them for whose AI it is (`targetAI`, `paladinAI`, …).
- `NoteAnubarakGripCast` (`ToCHelpers_Anubarak.cpp:8386`, called from `ToCActions_Anubarak.cpp:558`)
  is strategy logic despite its name: rename it so the PR's RaidObs strip doesn't delete it.
- Inline turn tables at `ToCHelpers_Anubarak.cpp:3637, 4949, 8570` and `i < 12` at `:8585` become
  named constants.
- `Definition/ToCDefinition_Anubarak.cpp:219`: `"tree of life"` → `"tree form"` (the druid action's
  registered name).

## Task list

## Decisions for review

## Known gaps

## Blocked
