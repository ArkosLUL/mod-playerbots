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
  - Recorded on `58b331cf6` before any edit: 0 errors, 22 warnings, all `[strategy-never-added]` at
    `RaidStrategyContext.h:39-60`, none under `src/Ai/Raid/ICC`. The same set with `--spell-difficulty`.
- `src/Ai/Raid/RS/RSMultipliers.cpp` includes `ICCActions.h` and uses nothing from it, only its
  transitive includes. It is the only include of an old ICC header outside `ICC/`.
- `EncounterGatedTrigger` forwards every `Trigger` virtual and takes name and check interval from the
  inner trigger; a gate's `live` only decides `RaidObs::NamePull` in its `Check`.
  `EncounterGatedMultiplier` keeps the inner name and, under `Family::AnyAction` and an open gate,
  hands every non-null action to the inner multiplier.
- The image's compile database is a clang build (`-Xclang` flags), so a CI-flags pass reuses it with
  clang++ only.

**Results** (finisher), after the review fixes:

- Parity against `Custom`: 55 rows equal in dispatch order (51 nodes); 51 trigger and 55 action
  keys with the same classes; 13 multipliers in order; 106 class name strings unchanged, now `Name`
  constants; 13 multiplier names unchanged. A line multiset over every old and new ICC source
  differs only by the dropped labels, the old registration and the new definition wiring.
- pblint after-set equals the baseline by `[check] message`, both modes. It reads all 55 ICC rows
  and 106 `Name` classes, so its name checks still cover ICC without an extension.
- Syntax check: all 69 ICC TUs, both `BuildShared*Contexts.cpp` and `RS/RSMultipliers.cpp` pass.
  clang++ with CI's `CXXFLAGS` (`-Werror`) passes the 33 TUs whose code is not verbatim: the
  definitions, helpers, root, scripts, the four action files that lost code, the Gunship trigger and
  the Dogs multiplier. `BuildShared*Contexts.cpp` fail that pass only on the existing unused
  parameter in `Naxx/NaxxBossHelper.h:63`.
- botobs suite: 1140 tests pass. Codestyle passes. No header under `src/` collides with a new ICC
  header ignoring case.

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

All done, by one lead with no split (see Decisions). The pre-lane sources are on `Custom`
(`git show Custom:src/Ai/Raid/ICC/<file>`).

1. Baselines: pblint both modes (Verified facts).
2. Generator over `Custom`'s files, every anchor and replacement asserted: trigger, multiplier and
   action headers and sources split per stem; stem helpers, positions and constants to
   `Util/ICCHelpers_<Stem>`; `Util/ICCData.h`, `Util/ICCHelpers_Shared`, `Util/ICCScripts`; umbrellas;
   one `Definition/ICCDefinition_<Stem>.cpp` per stem with `Node<T, A>` rows and `Multiplier<M>`
   lines; a `static constexpr Name` on every row class.
3. By hand: `Util/ICCEncounterGate.h` (boss ids, pass-through gate), `Definition/ICCDefinitions.h`,
   `ICCStrategy.{h,cpp}`, both contexts. Old flat files deleted.
4. `tools/botobs/tests/test_icc_naming.py` reads trigger names from the definition rows.
5. README "Code layout and registration" rewritten, the upstream rule replaced, file pointers fixed.
6. Parity and checks (Verified facts, Results).

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| No split: the lead moved all 21.4k lines with one asserted generator and checked parity itself | The move is mechanical once anchored; three movers would each re-read the same files and leave the finisher the same parity work | The Pipeline's three mover groups |
| Pass-through gate open and **never live** | `live` only drives `RaidObs::NamePull`; always-live lets any firing trigger rename a map-named trace, cross-boss ones too, so never-live keeps traces as they were | Scope 2's "goes live always"; or `BossStateGateLive` |
| Boss ids and the gate in `Util/ICCEncounterGate.h`, not `ICCData.h` | `r1-foundation` scope 1 names that file; Ulduar's `UldEncounterGate.h` | Ids in `ICCData.h` |
| `ICC_BOSS_NONE = 255` for Dogs and FrostGiant | `RaidEncounterRules::GateOpen` leaves an id past the encounter count open, `BossStateGateLive` returns false | 15 (`MAX_ENCOUNTERS`), which a core encounter added later would make real |
| Row classes carry `static constexpr Name`, rows are `Node<T, A>` | Convention 1; scope 2 allows it; key and class can no longer disagree | The explicit string `Node` overload with classes untouched |
| Root adds every definition's multipliers, the three empty ones (Marrowgar, FrostGiant, Svalna) beside their neighbours; `OnTick` loops the definitions | From wave 3 the root belongs to the merge stage, so a boss lane adding a multiplier, rule or tick never edits it | List only the definitions that have multipliers today |
| Stem helpers to `Util/ICCHelpers_<Stem>.{h,cpp}`: the phase readers from the trigger files, Gunship's two from its action file, the kinetic bomb assignment (with `BPC_FLOOR_Z`, verbatim) | ToC's `ToCHelpers_<Stem>`; a declaration and its definition in one stem file pair | Helpers left in the trigger and action files that defined them |
| Single-boss functions leave `ICCShared`: Blistering Cold to Sindragosa, the Remorseless Winter trio to LichKing, `IccValithriaShouldHealRaid` to Valithria; `IccAnyBloodPrincePresent` stays (council and Blood-Queen) | Scope 1 "cross-boss helpers go to `Shared`"; boss lanes own their stem | `ICCShared` moved whole |
| `ICCData.h` keeps `CreatureIdsICC` and `SpellIdsICC` whole, plus Defile and Malleable Goo (the listeners read them). Single-stem: the Lich King difficulty rows, `addEntriesLady`, `spellEntriesFlood`, `availableTargetsGS`, and the abomination ids from `ICCScripts.h` | ToC w0a's enum decision; scope 1 | Split both enums per stem |
| No `ICCData.cpp` | Positions stay `inline const` in their stem's helper header, so it would be empty | `extern` positions defined there |
| Umbrellas keep the old names `ICCActions.h`, `ICCTriggers.h`, `ICCMultipliers.h`; the contexts include them as before | `RS/RSMultipliers.cpp` (outside Owns) includes `ICCActions.h`; no ICC header collides ignoring case | New names plus an RS edit |
| Stem action headers keep the old `ICCActions.h` include list (ICC swaps only), so a renamed action source changes only its ICC includes; split trigger and multiplier sources keep the includes their body uses | Pure move; ToC w0a's include-what-you-use for split files | Trim the action headers too |
| Dropped only bare labels (`//Lord Marrogwar`, `//DBS`, `// Rotface`, …). The Rune of Blood comment moved into the Saurfang definition; the commented-out `IccRotfaceGroupPositionMultiplier`, the giant's TODO block and "Rotting Giant WiP" stay verbatim | Scope 4 | Clean them up now |
| No guide, navprobe, spell-id or trace work; coordinates moved verbatim | Pure move; each boss lane navprobes its own | n/a |
| Boss docs' `Code:` lines left as they are | Outside Owns | Fix them here |

## Carried over

- Boss docs' `Code:` lines and several boss briefs' facts name pre-split files (`ICCActions_LM.cpp`,
  `ICCShared.cpp`, …). The README's "Code layout" maps them to stems; each boss lane fixes its own.
- `Action/ICCActions_FrostGiant.cpp` keeps "Rotting Giant WiP (waiting for core fix)" and a
  commented-out `TODO` block, the only user of `ICC_ROTTING_FROST_GIANT_TANK_POSITION`: future-work
  and dead code under the no-nonsense-comments rule. For `r3-deathwhisper`.
- `spellEntriesFlood` (`Util/ICCHelpers_Rotface.h`) is never used. For `r7-rotface`.
- `Multiplier/ICCMultipliers_Rotface.h` keeps the commented-out `IccRotfaceGroupPositionMultiplier`.
  For `r7-rotface`.
- Definitions carry their trace slugs, unused while the gate is never live; `r1-foundation`'s real
  gates make a live trigger name its pull.

## Known gaps

- Pure move: every behaviour issue in the overview's "Why" is still live.
- Never built or linked: the syntax check and the CI-flags pass are the only proof. The new
  directories compile only after the user re-runs CMake.

## Blocked
