# c3-toc-fc-beasts

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the
**Wave 1** section bind this lane.

Cleanup on `Custom`, branch `tocpr/c3-toc-fc-beasts`. Apply the overview's "Every cleanup lane"
list to the `c3-toc-fc-beasts` files of the Wave 1 table. Measurements cut from comments go to
`docs/raids/trial-of-the-crusader/faction-champions.md` or `northrend-beasts.md`. RaidObs calls
stay. A change that alters behaviour beyond the listed ones is out of scope: log it under Decisions
for review.

Big lane (~16k lines): split into at most 3 implementers with disjoint files, e.g. Faction
Champions stems, Northrend Beasts stems; the finisher runs the checks.

Lane facts:
- `Action/ToCActions_Jormungars.cpp:479` calls `UseNitroBoosts(bot)` inside
  `... && UseNitroBoosts(bot) && RaidObs::Active()`: restructure so the boost runs outside any
  trace condition and only the trace note stays behind `RaidObs::Active()`, same evaluation order.
- `FactionChampionsGripChain::NoteGrip` (`Util/ToCFactionChampionsGripChain.h:142`, called from
  `Util/ToCHelpers_FactionChampionsDefence.cpp:3208`) is strategy logic despite its name: rename it
  so the PR's RaidObs strip doesn't delete it.
- `PlayerbotAI* ai` locals at `ToCHelpers_FactionChampionsDefence.cpp:893, 922` hold another
  player's AI: name them for whose AI it is.
- Literals to name, e.g. `ToCHelpers_FactionChampionsDefence.cpp:2003` (40000/60000/10000); bare
  doubles at `ToCHelpers_Jormungars.cpp:115, 1027`.
- Long comment blocks, e.g. `ToCHelpers_FactionChampions.h:140` (16 lines),
  `ToCHelpers_FactionChampionsDefence.h:226` (11).

## Task list

## Decisions for review

## Known gaps

## Blocked
