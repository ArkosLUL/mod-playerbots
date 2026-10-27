# i6-closeout — verify on real pulls, fold docs, delete the plan

Wave 3. It runs only when the user calls it, after they have rebuilt with waves 1-2c and pulled ICC
bosses. Rules and contract: [icc-raidobs.PLAN.md](icc-raidobs.PLAN.md). The status table there lists
each lane's carried-over items and gaps.

## Scope

1. **Select the evidence.** Take every `631_*.ndjson` under
   `G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/` whose `hdr.bin` is newer than the
   wave 2c merge commit (`docs/systems/observability.md`, "Is this trace evidence?"). Group them by
   contract slug.
2. **Verify each pulled encounter.**
   - The file name carries the contract slug. Watch Valithria, Svalna, Gunship, the gauntlet, the
     Blood Prince trash and both trash `MarkPull`s.
   - `python tools/botobs/postmortem.py <trace> --verify --probes --coverage` passes `--verify`.
   - No declared key of the encounter is silent across all its pulls.
   - `--coverage` folds other encounters' nodes instead of listing them NEVER.
   - `python tools/botobs/bosses/icecrown_citadel.py <trace> --cheats` shows no removed key
     (`*.nitro`, `rf.spitefulfury`, `icc.tauntfixate`, `icc.tauntreset`, `vd.speed`), and the banner
     shows `iccbuffs`.
   - The boss reader runs on its own pulls and refuses another encounter's.
   - Fix what fails, by the lane rules.
2b. **Trace Blood Prince trash** (user's call after wave 2b; the user wants trash traced). Boss state 14
   goes `NOT_STARTED` → `DONE` and never `IN_PROGRESS`, and the trash isn't boss-flagged, so no trace
   opens under `blood-prince-trash` (see the "Blood Prince trash" section of
   `docs/raids/icecrown-citadel/blood-prince-council.md`). Add a `MarkPull(631, source,
   "blood-prince-trash")` hook in `src/Ai/Raid/ICC/ICCScripts.cpp` when a bot enters combat with that
   trash, entries from the core's instance script. Only the hook: the strategy gets no trash code.
   Update that doc section and the reader test if it pins the "never opens" fact.
3. **Bosses with no pull** stay unverified. Each such boss doc says "not yet seen in a trace", and
   the report lists them.
4. **Fold and delete.**
   - Move durable content left in the briefs (decisions with rationale, gaps) into
     `docs/raids/icecrown-citadel/`.
   - Move ICC from "Still bare" to the converted list in `docs/systems/observability.md`.
   - Correct `docs/engine/pitfalls.md`'s raid-cheat bullet: Yogg-Saron no longer cheats, and in ICC
     every kept cheat now sits behind `raid` and records itself.
   - Delete `docs/plans/icc-raidobs/`.
   - Fold with `compact-docs-writer`.

## Owns

Anything the verification needs fixed, listed in the report. The trash hook in `ICCScripts.cpp`. `docs/raids/icecrown-citadel/`,
`docs/systems/observability.md`, `docs/engine/pitfalls.md`, `docs/raids/README.md`, and this
directory, which it deletes.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
