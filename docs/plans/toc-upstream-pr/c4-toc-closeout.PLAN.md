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

## Decisions for review

## Blocked
