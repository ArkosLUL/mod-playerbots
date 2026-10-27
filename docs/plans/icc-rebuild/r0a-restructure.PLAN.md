# r0a-restructure — per-encounter layout, no behaviour change

Wave 1, parallel with `r0b-guides`, chained into wave 2. Rules, decisions and checks:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Precedent: `docs/plans/toc-rework/w0a-restructure.PLAN.md`
(the same move for ToC; read its Verified facts and Decisions for review). The target model is
Ulduar's: `src/Ai/Raid/Uld/Definition/` and the definition API in `src/Ai/Raid/RaidEncounter.h`
(`EncounterDefinition`, `EncounterBuilder`, `EncounterGatedTrigger`).

## Scope

1. **Split** `src/Ai/Raid/ICC/` into `ICC/{Action,Trigger,Multiplier,Util,Definition}/`, one stem per
   encounter. Files: `ICCActions_<Stem>`, `ICCTriggers_<Stem>`, `ICCMultipliers_<Stem>`,
   `ICCHelpers_<Stem>`, and `ICCDefinition_<Stem>.cpp`, listed in `Definition/ICCDefinitions.h`.
   - Stems in dispatch order, with today's source:
     - `Marrowgar` (LM)
     - `Deathwhisper` (LDW, including `icc dark reckoning`)
     - `FrostGiant` (the Rotting Frost Giant trigger and action, now in LDW)
     - `Gunship` (GSB)
     - `Saurfang` (DBS)
     - `Dogs` (Stinky and Precious)
     - `Festergut` (FG)
     - `Rotface` (RF)
     - `Putricide` (PP)
     - `BloodPrinces` (BPC, plus the trash)
     - `BloodQueen` (BQL)
     - `Svalna` (SS)
     - `Valithria` (VT)
     - `Sindragosa` (SG, plus the gauntlet)
     - `LichKing` (LK)
   - Cross-boss helpers go to a `Shared` stem.
   - Map id, positions and core enum mirrors go to `Util/ICCData.{h,cpp}`, each with a comment naming
     its core source header. Single-stem constants and positions move to their stem.
   - `ICCScripts.{h,cpp}` (per-instance state, spell listeners, reset scripts, RaidObs name table)
     move to `Util/` unchanged.
   - Rename a stem only to avoid a collision, and log it.
2. **Definitions with a pass-through gate.** Each encounter stem gets one `EncounterDefinition` holding
   its trigger nodes as rows and its multipliers. The gate opens and goes live always, so play is
   unchanged; `r1-foundation` swaps in the real gates.
   - Boss ids and slugs come from the ICC README's naming table. Dogs and FrostGiant have no boss id:
     give them an id the gate ignores and say so in the doc.
   - Trigger classes keep their names and `IsActive` bodies. Use the builder's string-name `Node`
     overload, or add `static constexpr Name`, whichever keeps every name string identical.
   - The root strategy, `ICCActionContext.h` and `ICCTriggerContext.h` only loop over the definitions,
     as Ulduar's do.
3. **Order is behaviour.**
   - Nodes dispatch in the stem order above, which reproduces today's `ICCStrategy.cpp` insertion
     order (it is grouped by boss).
   - Multipliers keep today's order, which differs: Deathwhisper, Saurfang, Dogs, Festergut, Rotface,
     Putricide, BloodPrinces, BloodQueen, Valithria, Sindragosa, LichKing adds, LichKing spirit bomb,
     Gunship last. Add them from the root in that order, or log why a different order can't change a
     result.
4. **Pure move.**
   - Every trigger, action and multiplier name, priority and string stays identical.
   - No logic change, no renamed spell constants, no deleted dead code.
   - Bodies, comments, names, strings and priorities are copied verbatim. Allowed changes: includes,
     header guards, the definition wiring.
   - Drop only bare section labels a one-stem file makes redundant.
5. **Tooling that reads the old files.**
   - `tools/botobs/tests/test_icc_naming.py` reads `ICCTriggerContext.h` creators: point it at the
     new source of trigger names, so the same names pass.
   - Keep pblint's name checks working. If the definition model hides names from pblint, extend
     pblint rather than drop a check.
6. **Docs.** In `docs/raids/icecrown-citadel/README.md`:
   - Rewrite "Code layout and registration" for the new layout, including the seams a boss lane owns:
     its stem files and its definition.
   - Replace the "keep tactic changes inside the existing files" rule with the Upstream decision from
     the overview.
   - This lane may write the README.

## Pipeline for this lane

The lead records the baselines first (below), then creates the skeleton itself: `Util/ICCData`, the
`Shared` stem, the definition seam, the root dispatch, the umbrella headers and the context loops. It
then splits the stems into at most 3 mover groups with disjoint files, for example Lower Spire plus
Dogs, Plagueworks plus Crimson Hall, Frostwing plus Lich King. The finisher deletes the old flat files
and runs the parity and checks.

## Verified facts

- Files today: `ICCStrategy.{h,cpp}`, `ICCTriggers.{h,cpp}`, `ICCMultipliers.{h,cpp}`,
  `ICCShared.{h,cpp}`, `ICCScripts.{h,cpp}`, `ICCTriggerContext.h`, `ICCActionContext.h`,
  `Action/ICCActions.h`, and `Action/ICCActions_{LM,LDW,GSB,DBS,Dogs,FG,RF,PP,BPC,BQL,SS,VT,SG,LK}.cpp`.
  21.4k lines. The 106 `ICC_*` positions sit in `Action/ICCActions.h`.
- Counts: 51 trigger nodes (55 node actions), 51 trigger creators, 55 action creators, 13 multipliers.
  `IccRotfaceGroupPositionMultiplier` is commented out in `ICCMultipliers.h`.
- Registration:
  - `src/Ai/Raid/RaidStrategyContext.h` (include, `creators["icc"]`, factory);
  - `src/Bot/Engine/BuildSharedActionContexts.cpp` and `BuildSharedTriggerContexts.cpp` (include,
    `Add(new RaidIcc…Context())`);
  - `src/Bot/PlayerbotAI.cpp` (`case 631`, and `icc` in the instance-strategy name list);
  - `src/Script/Playerbots.cpp` (`AddSC_IcecrownBotScripts`).
  The key `icc` must not change.
- No ICC header collides case-insensitively with any other header under `src/` today.
- CMake globs sources and include directories recursively, so new directories build after the user
  re-runs CMake. The syntax check adds `-I` for them. Pass `PB_MAX_FANOUT=60`.
- pblint baseline: before any edit, run
  `python tools/pblint/pblint.py --warnings src/Ai/Raid/ICC src/Ai/Raid/RaidStrategyContext.h src/Bot/PlayerbotAI.cpp`
  and record the finding set here. It was clean on `824468439`. Compare by `[check] message`, since
  paths and lines move.

## Parity (finisher)

Against `HEAD` of `Custom`:
- the ordered (trigger, action, priority) list in dispatch order;
- the trigger and action creator key sets, with the same classes behind them;
- the multiplier class order;
- every trigger constructor's name string and every action's name;
- the pblint finding set;
- the syntax check on every `.cpp` under `src/Ai/Raid/ICC` plus both `BuildShared*Contexts.cpp`;
- the botobs suite.

Record the results under Verified facts.

## Owns

Everything under `src/Ai/Raid/ICC/`. The ICC registration lines in the files listed above, only where an
include path changes. `tools/botobs/tests/test_icc_naming.py`, `tools/pblint/` if scope item 5 needs it,
`docs/raids/icecrown-citadel/README.md`, and this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
