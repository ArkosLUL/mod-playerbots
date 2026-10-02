# EoE strategy → upstream mod-playerbots `test-staging`: readiness analysis + PR prep plan

## Context

Goal: contribute the local Eye of Eternity (Malygos) strategy (`src/Ai/Raid/EoE/`, branch `Custom`)
to `mod-playerbots/mod-playerbots` against `test-staging`. Upstream already has an older 10-file EoE
strategy, so this is a rework (+3021/−727 inside `src/Ai/Raid/EoE/`).

Decisions taken with the user (2026-10-02):
- **Two stacked PRs**: PR 1 = raid encounter framework + shared edits; PR 2 = EoE rework on top.
- **Low-risk cleanup only** before submitting: style/comments/gating fixes that keep behaviour;
  the three "one action does several jobs" splits wait for reviewer feedback.

State checked: `upstream/test-staging` = `b0cd0ea7b`, merge base `619a06fc7` (50 upstream commits since).

## Verdict

**Merges cleanly as text, but does not build on `test-staging` as-is.**
- Upstream has not touched `src/Ai/Raid/EoE/` since the merge base; all wiring sites
  (`RaidStrategyContext.h`, `BuildShared{Action,Trigger}Contexts.cpp`, `PlayerbotAI.cpp` map 616 →
  `wotlk-eoe`) are identical upstream. No upstream API change since the merge base breaks EoE's own
  code; EoE uses no fork-only core API, and core `boss_malygos.cpp` matches core `test-staging` → no
  companion core PR.
- Build blockers are all local code EoE needs: the `RaidEncounter` framework, `RaidInstanceState`,
  `Trigger::getCheckInterval()`, `EncounterHelpers::CommandPetAttack/StopPet`, and upstream's old
  `EoE/EoEActions.{h,cpp}` / `EoE/EoETriggers.{h,cpp}` must be deleted (basename clash with the new
  `Action/EoEActions.h`, `Trigger/EoETriggers.h` on the recursive include path).
- **PR requirements:** codestyle CI (`apps/codestyle/codestyle-cpp.py`) passes; GPLv2 headers,
  120 cols, registration, relevance bands, no chat/config/DB/raw pointers all pass. cppcheck (blocking
  CI) is unverified (not installed). Reviewers/CodeRabbit would flag comments, isUseful/Execute
  duplication, missing gates, raw phase numbers, naming, `const`→`constexpr`, braces on
  single-statement bodies. Template needs a `pmon` before/after and an honest AI Assistance answer.

Behaviour drift on upstream (no code change, mention in PR 2 testing notes):
- upstream `PlayerbotAI::IsTank` (`:2299`) counts a Frost Presence DK as tank; local doesn't.
- upstream `9e8ceed92` fixed a `uint32` wrap in `CastTimeMultiplier` that hit Malygos (>4.29M HP).
- `pmon` (`9e6a8a940`) groups all EoE multipliers under one "malygos" row.
- Open upstream PR #2747 edits `MovementActions.h`; re-check EoE movement calls if it lands first.

## Phase A — low-risk cleanup on `Custom` (behaviour unchanged) — DONE 2026-10-02

Also done beyond the list: framework private members renamed to `_lowerCamelCase`; trigger
intervals left as literals (upstream passes them that way too).

All in `src/Ai/Raid/EoE/` unless noted. Rewritten comments follow upstream's rule (one line
preferred, ≤2 lines, no history) and go through `use-conversational-language`.

1. **Comments.** Cut the 14 prose blocks of 3+ lines to ≤2 lines: `EoEActions_Drakes.cpp:217`,
   `EoEDefinition_Malygos.cpp:104,123`, `EoEData.h:80,95,159,177,187`, `EoEEncounter_Drakes.cpp:139,201,444`,
   `EoEEncounter_Drakes.h:50`, `EoEEncounter_Malygos.cpp:31`, `EoEEncounter_Malygos.h:41`. Drop or
   rewrite history comments: `EoEActions_Shared.cpp:237`, `EoEDefinition_Malygos.cpp:140`,
   `EoEEncounter_Drakes.cpp:239-240`, `EoEEncounter_Malygos.cpp:62`, `EoEEncounter_Malygos.h:18`,
   `EoEData.h:34,189`. Fix stale ones: `EoEEncounter_Malygos.cpp:57-58` ("Nothing is ever evicted")
   and `EoEData.h:61-62` — `RaidInstanceState` frees state on map destroy. Same pass over the
   framework files (`src/Ai/Raid/RaidEncounter.{h,cpp}`, `RaidEncounterRules.h`,
   `RaidInstanceState.h`), e.g. the dated narration at `RaidEncounter.h:24-27`.
2. **Phase enum.** `GetMalygosPhase` (`Util/EoEEncounter_Malygos.h:28`) returns an `enum class`
   instead of `0..4`; update every `== 1..4` site (`EoEDefinition_Malygos.cpp:24-27`,
   `EoETriggers.cpp`, `EoEActions_Shared.cpp`, `EoEActions_Adds.cpp:26`, `EoEMultipliers.cpp:14`).
3. **Constants and naming.** `EoEData.h` namespace-scope `const` → `inline constexpr`; name stray
   literals (`EoEActions_Drakes.cpp:108`, `EoEEncounter_Malygos.cpp:302,515`, `EoEActions_Adds.cpp:211,252`,
   `EoEActions_Shared.cpp:93`, `EoEEncounter_Malygos.h:21`, trigger intervals in `EoETriggers.h`);
   private members → `_lowerCamelCase` (`EoEActions_Adds.h:43,68`, `EoEActions_Drakes.h:29-38`);
   ctor param `ai` → `botAI` (`EoEActions_Drakes.h:23`, `EoEMultipliers.h:17`); `0.f` → `0.0f`;
   `auto&` GUID loops → `ObjectGuid const&` (`EoEActions_Adds.cpp:35`, `EoEActions_Shared.cpp:180`);
   drop braces on single-statement bodies (upstream hard rule, ~148 sites; clang-format won't do it).
4. **No double work between `isUseful()` and `Execute()`.** `isUseful()` stores the found GUID in a
   private member; `Execute()` resolves it instead of searching again: `GetHastedLord`
   (`EoEActions_Adds.cpp:62-77`), grip/snare (`EoEActions_Sparks.cpp:37-63`), `GetPowerSparkToKill`
   (`EoEActions_Sparks.cpp:65-97`).
5. **Missing gates.** Add `isUseful()` to `MalygosPositionAction` (`EoEActions_Shared.cpp:20-105`:
   P1 only, not already in position) and move the vehicle/shelter check of the Surge action from
   `Execute()` to `isUseful()` (`EoEActions_Adds.cpp:282-299`); expose the Definition's per-tick
   `Holder` cache (anonymous namespace, `EoEDefinition_Malygos.cpp:29-61`) via `EoEEncounter_Malygos.h`
   and use it instead of the group-walking `IsMainTank` at `EoEActions_Shared.cpp:39,127`,
   `EoEActions_Sparks.cpp:68`.
6. **`PowerSparkTrigger`** (`EoETriggers.cpp:22-32`): early-exit any-match instead of building a
   vector to test `!empty`.

Left as is (explain in PR text instead): the `thread_local` per-bot/per-ms caches
(`EoEDefinition_Malygos.cpp:38-61`, `RaidEncounter.cpp:43-71`); the three multi-job actions
(`EoEActions_Drakes.cpp:127-320`, `EoEActions_Adds.cpp:183-280`, `EoEActions_Shared.cpp:107-251`).

Close the cycle per `CLAUDE.local.md`: syntax-check + pblint the touched files, fold anything
durable into `docs/raids/eye-of-eternity.md`, commit on `Custom`. Then update memory
`eoe-upstream-pr-prep.md` (framework dependency, upstream `.agents/docs` rules, stacked-PR decision).

## Phase B — PR branch contents (each git step below needs the user's explicit go at that step)

**State 2026-10-02:** both branches built and verified in a throwaway clone of upstream (not in this
repo, not pushed); PR texts drafted, awaiting wording approval, `pmon` numbers and the user's
AI Assistance wording. Rebuild from this section if the clone is gone.

**PR 1 — framework**, branched from `upstream/test-staging`:
- New `src/Ai/Raid/RaidEncounter.{h,cpp}` + `RaidEncounterRules.h` from `Custom`, trimmed to what EoE
  uses: remove `RaidObs` (`RaidEncounter.cpp:26,302-303`, `.h:99`; `src/Bot/Obs` is local-only),
  `EncounterTriggerPassId`/`CheckScope` (Uld/OS only: `.cpp:79-94,266-269,290-294`, `.h:42-47`),
  `Tick`/`OnTick` (upstream Engine never calls `Strategy::OnTick`; `.cpp:500-504,537`). Also
  `EncounterGate::live`, `BossStateGateLive` and `traceName` (only `RaidObs` read them):
  `EncounterGate` becomes `bool (*)(PlayerbotAI*, uint32)`. Drop `RaidEncounterRules.h`'s
  `tools/nativetest` comment (no `tools/` upstream).
- New `src/Ai/Raid/RaidInstanceState.{h,cpp}` unchanged.
- `src/Bot/Engine/Trigger/Trigger.h`: add `getCheckInterval()` (local `:42`).
- `src/Script/Playerbots.cpp`: `void AddSC_RaidInstanceStateScripts();` in upstream's declaration
  block (`:601-606`) and the call in `AddPlayerbotsScripts` (`:624-629`).
- Commit/PR title format `type(Scope/Subscope): Summary` (≤72, imperative), e.g. `feat(Raid): ...`.

**PR 2 — EoE rework**, branched from PR 1's branch:
- Delete upstream `EoE/EoEActions.{h,cpp}`, `EoE/EoETriggers.{h,cpp}`; copy `Custom`'s whole
  `src/Ai/Raid/EoE/` (post Phase A). In `EoEDefinition_Malygos.cpp` drop `MalygosGate` and build
  the definition as `(EOE_DATA_MALYGOS, &MalygosGateOpen, &DefineMalygos)`.
- `src/Util/EncounterHelpers.{h,cpp}`: add only `CommandPetAttack` / `StopPet` (local
  `EncounterHelpers.cpp:615-657`, comments cut to upstream's rule) + `#include "CreatureAI.h"`.
  Never copy the local file: it deletes
  `GetSelfImmunitySpell`, which upstream Hyjal uses, and carries ~12 unrelated local helpers.
- Title e.g. `refactor(Raid/EoE): ...` (precedent `cc57c9c04 fix(Naxx/Thaddius)`).
- Both PRs target `test-staging` (forks can't base on a fork branch), so PR 2's diff includes PR 1's
  commit until PR 1 merges; PR 2's body says so and links PR 1, PR 1's body links PR 2 as its user.
  After PR 1 merges, PR 2 is rebased onto `test-staging` (ask first).

**PR texts** (drafted with `handover` + `use-conversational-language`, shown to the user, published
only after explicit approval of the wording): fill every `PULL_REQUEST_TEMPLATE.md` section —
observable before/after, minimum logic + per-tick cost (300 ms per-instance creature cache, ~7
creature types, phase-gated triggers), how to test (10/25 normal), Impact Assessment with `pmon`
numbers, AI Assistance answered honestly (upstream requires it; user picks the wording), Code
Provenance "original", checklist.

## Verification

- Phase A: `~/.claude/scripts/pb-syntax-check.sh` on every touched `.cpp` and includers of touched
  `.h` (`PB_MAX_FANOUT` raised); `tools/pblint/pblint.py src/Ai/Raid/EoE src/Ai/Raid/RaidEncounter*
  src/Ai/Raid/RaidInstanceState*`; upstream `codestyle-cpp.py` run on a copy (as done in this
  analysis — passes today).
- PR branches (in a worktree, on the user's go): `python apps/codestyle/codestyle-cpp.py` from the
  module root; `PB_REPO=<worktree> pb-syntax-check.sh <changed .cpp>` (compiles against the build
  image's core = the local fork; errors in untouched upstream files are not PR errors); grep creator
  tables for every EoE action/trigger name (`ai-engine.md` checklist); cppcheck with upstream's
  exact command (`cppcheck --force --inline-suppr --suppressions-list=./.suppress.cppcheck src/`)
  in a throwaway container or a local install — empty report required. Result 2026-10-02:
  codestyle passes, all 15 touched TUs compile, cppcheck 2.13 (`ubuntu:24.04`) empty on both
  branches. `Playerbots.cpp` can't be compiled: upstream needs `ModuleDatabasePool.h`, missing from
  the build image's core.
- In-game (user): EoE 10 + 25 normal on `Custom` after Phase A to confirm unchanged behaviour;
  `playerbot pmon` before/after per the template (`BotActiveAlone = 100`,
  `botActiveAloneSmartScale = 0`, `pmon toggle` after login, `pmon stack` 5 min later). Ideally one
  run on core `test-staging` + PR 2 branch, since `IsTank` and `CastTimeMultiplier` differ there.
