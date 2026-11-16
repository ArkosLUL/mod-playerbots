# c-icc: ICC

Lane of [raid-perf-pass.PLAN.md](raid-perf-pass.PLAN.md) (wave 2, branched from Custom after wave 1
merged, beside `b-anub-twins-jarax`). The overview's neutrality rule, file ownership, git, Docker and
verification rules bind this lane.

Implement the overview's lane `c-icc` items: the splash cache at the `IsRaidAreaDamage` sites and
Sindragosa's, `char const*` overloads for the note wrappers with `Active()` guards on pure argument
builds, and N3 `IccGetCreaturesByEntries` in one grid visit. Counts and names there (15 sites, 13
wrappers) predate later ICC work; re-count on Custom.

- Wave 1 merged the splash cache API (lane `a-shared-fc`); read its commits on Custom first and use
  it as merged.
- Leave `IccLichKingSplashMultiplier`'s own cache and `IccDbsAreaDamage`'s order as they are.
- An `Active()` guard is allowed only where every skipped argument is pure; a skipped call that
  touches a memo, a Value or any state is a behavior change.
- N3 keeps the identical per-entry predicate, range and the result order callers see; prove it with
  a harness against the per-entry calls.
- ICC has no baseline measurement; the proofs carry the lane.

One commit per fix, each compiling (`pb-check` over its paths), harness files in
`build-toc-wt/perf/_harness/c-icc/`, never committed.

## Task list

## Candidates done

## Candidates dropped

## Decisions for review

## Known gaps

## Blocked
