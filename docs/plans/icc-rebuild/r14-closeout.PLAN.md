# r14-closeout — verify on pulls, fold docs, delete the plan

Wave 5. It runs on the user's call, after the boss waves and any refine lanes. Rules:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). The Status table lists each lane's carried-over items and
gaps.

## Scope

1. **Evidence.** Take every `631_*.ndjson` under
   `G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/` whose `hdr.bin` carries the encounter's
   lane merge. Group the traces by contract slug.
2. **Verify each pulled encounter.**
   - `python tools/botobs/postmortem.py <trace> --verify --probes --coverage` passes `--verify`. No
     declared key is silent across all the encounter's pulls, and `--coverage` folds other encounters'
     nodes.
   - `python tools/botobs/bosses/icecrown_citadel.py <trace> --cheats` lists only the keys the README
     cheat table keeps.
   - The boss reader runs on its own pulls and refuses another encounter's.
   - Fix what fails, by the lane rules. An encounter with no pull gets "not yet seen in a trace" in
     its doc, and the report lists it.
3. **Audit.**
   - The README cheat table equals the `IccRaidCheat` sites in code, each with its reason.
   - No raid-icon write and no `rti` or `rti cc` use remains in `src/Ai/Raid/ICC/`. The only icon a
     bot reads is a player's skull, in the resolvers.
   - Each boss doc has its mechanics table for all four difficulties.
4. **Leftovers.**
   - Remove the ToC aliases `r1-foundation` kept for dirty files, if those files are clean now.
   - Correct `docs/plans/toc-rework/toc-rework.PLAN.md`'s "Wowhead blocks automated reads", if that
     plan still exists: the reason is Wowhead's robots opt-out for Anthropic's agents
     (`docs/systems/guide-sources.md`).
   - Plus every item the Status table carries to this lane.
5. **Fold and delete.**
   - Move durable content left in the briefs into `docs/raids/icecrown-citadel/`.
   - Move ICC from "Still bare" to the converted list in `docs/systems/observability.md`.
   - Correct `docs/engine/pitfalls.md`'s raid-cheat bullet for ICC.
   - Update `docs/raids/README.md`'s ICC row.
   - Delete `docs/plans/icc-rebuild/`.
   - Fold with `compact-docs-writer`.

## Owns

Anything the verification needs fixed, listed in the report. `docs/raids/icecrown-citadel/`,
`docs/systems/observability.md`, `docs/engine/pitfalls.md`, `docs/raids/README.md`, the ToC alias
lines, and this directory, which it deletes.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
