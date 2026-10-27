# r1-foundation — real gates, shared raid helpers, ICC seams

Wave 2, alone, after `r0a-restructure` and `r0b-guides` merged. Rules, decisions and conventions:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). The layout is the one `r0a` wrote (ICC README, "Code
layout"). It may split.

## Scope

1. **Gate mirror.** `Util/ICCEncounterGate.h` mirrors the core's boss ids (`DATA_LORD_MARROWGAR` …
   in `icecrown_citadel.h`, 0-14) with a comment naming that header, as
   `src/Ai/Raid/Uld/UldEncounterGate.h` does for Ulduar.
2. **Real gates.** Swap every definition's pass-through gate:
   - `BossStateGate` with its boss id, 0-13, for the twelve bosses, Svalna (9) and the Sindragosa
     gauntlet (13).
   - `Dogs` and `FrostGiant` have no boss state. Give each a gate built on a generic helper: open
     while the unit, found by entry and the instance, is alive; live while it is in combat with the
     bot's group. Put that helper in `src/Ai/Raid/RaidEncounter.{h,cpp}`, because `r12-sindragosa`
     needs the same for Rimefang and Spinestalker.
   - Every multiplier becomes gated on its definition's live state. This changes play: today all 13
     act in every fight. Log it under Decisions for review.
   - Check each definition's opening steps against when its boss state moves (ICC README, "Boss
     states and trace names"): Festergut and Rotface go `IN_PROGRESS` seconds after the pull,
     Valithria when the adds reach her, Gunship at the battle start. A node that must act before
     that may only depend on "open", not "live". Record per encounter which nodes depend on which.
3. **Side effects out of `IsActive`.**
   - `IccStripExperienced` runs inside 9 triggers. The Putricide and Sindragosa triggers apply the
     `EnableICCBuffs` buffs.
   - Move both into definition `Tick`s: a raid-wide ICC definition for the strip, and the
     Putricide and Sindragosa definitions for their buffs. Behaviour stays the same:
     - the same members get the buffs on the same conditions, recorded through `IccNoteCheat`;
     - neither bots nor humans keep Experienced 71188 or 1% Threat 70115 once the fight that applied
       them is over.
   - Sindragosa's raid cheats in her group-position trigger stay where they are for
     `r12-sindragosa`.
4. **Per-instance state.** `IccInstanceState` moves into `RaidInstanceState<IccState>`
   (`src/Ai/Raid/RaidInstanceState.h`).
   - Keep the traced containers and their keys.
   - Keep the reset semantics: erase 5 s after the last boss state leaves `IN_PROGRESS`, and on map
     destroy.
   - Use `Find` for has-state checks.
5. **Hoist the ToC-only helpers ICC needs** into raid-wide files under `src/Ai/Raid/`, named in the
   `Raid*` style. Point ToC at them in the same commit, with ToC behaviour unchanged:
   - `ToCDirectAttackAction` (`ToC/Action/ToCActions_Shared.h`);
   - `ToC/Util/ToCHelpers_Defensive.{h,cpp}`: the predicted hit, raid covers and personal
     defensives. Turn its map check into a parameter;
   - from `ToC/Util/ToCHelpers_Shared.{h,cpp}`: `IsToCDpsBot`, `GetToCHumanSkull`, `ToCPetOffPick`,
     `IsToCAreaDamage`, `CastToCThroughputHeal`, `ToCRestoreLegacyRti`;
   - the per-encounter burst window (`ToC/Multiplier/ToCMultipliers_Shared.{h,cpp}`), as a hook a
     definition fills, replacing `ToCLiveEncounter` with the caller's live test;
   - the opener hold (`ToC/Util/ToCHelpers_Opener.{h,cpp}`), with the boss list from the caller;
   - the resistance-aura election (`ToC/Util/ToCHelpers_ResistAura.{h,cpp}`), with the school set per
     encounter from the caller.
   - Leave `ToCHelpers_Staging` and `AvoidCreatureClusterAction` in ToC.
   - Another session edits ToC files in the main tree. At the start, read `git -C <main> status`.
     Where a ToC call site sits in a file dirty there, keep the ToC name as a `using` declaration in
     its ToC header, so that file needn't change. List each such alias for `r14-closeout`.
   - ToC parity: the same node, multiplier and definition order, the same names, the ToC readers'
     tests green, pblint over `src/Ai/Raid/ToC` unchanged.
6. **ICC seams** for the boss lanes, empty by default:
   - a direct-attack action on the hoisted base for each definition's resolver;
   - the burst-window hook;
   - opener-hold participation;
   - the resistance-aura school;
   - a place for opening positions.
   Write how a boss lane fills each into the ICC README's "Code layout".
7. **Blood Prince trash trace.**
   - Boss state 14 goes `NOT_STARTED` → `DONE` and never `IN_PROGRESS`, and the trash isn't
     boss-flagged. So add a `RaidObs::MarkPull(631, source, "blood-prince-trash")` hook in
     `Util/ICCScripts.cpp` when a bot enters combat with that trash, with entries from
     `instance_icecrown_citadel.cpp`.
   - The strategy gets no trash code.
   - Update the "Blood Prince trash" section of `docs/raids/icecrown-citadel/blood-prince-council.md`,
     and `tools/botobs/tests/test_blood_prince_trash.py` if it pins the "never opens" fact.
8. **Fold the old plan.** Move the durable content still only in `docs/plans/icc-raidobs/` (decisions
   with rationale, known gaps, carried-over items still true) into `docs/raids/icecrown-citadel/`, then
   delete that directory. Items for later lanes go into their briefs here instead.

## Verified facts

- `RaidEncounterRules::GateOpen` (`src/Ai/Raid/RaidEncounterRules.h`) is closed once the encounter is
  done or while another one is in progress, and open otherwise, so pre-pull steps run. It is also
  open outside an instance and for ids the script lacks.
- `BossStateGateLive` (`RaidEncounter.cpp`) is true only while the state is `IN_PROGRESS`.
- `RaidInstanceState` locks the lookup only: one map updates on one thread at a time, so a reference
  must not outlive the tick. `IccState()` follows the same model today.
- ToC coupling points: `ToCHelpers_Defensive.cpp` has one map check; `ToCHelpers_Shared.cpp` has one;
  the burst window, opener and resist aura read `ToCLiveEncounter`; the opener also reads a hard-coded
  `ToCNpcs` boss list, and the resist aura `ToCStagingBoss`.
- Another session reworks ToC's Anub'arak in the main tree (last commit `fdb3a950f`); its files go
  dirty and clean again. Check `git status` at the start and again before the commit.
- The 13 multipliers and what each vetoes are listed in the boss briefs. Gating them is this lane's
  change; their content is the boss lanes'.

## Owns

Everything under `src/Ai/Raid/ICC/`. New raid-wide files under `src/Ai/Raid/` and the generic gate
helper in `RaidEncounter.{h,cpp}`. The ToC files the hoist touches, within scope item 5.
`tools/nativetest/` tests for the hoisted code if any exist. `tools/botobs/tests/test_blood_prince_trash.py`,
`docs/raids/icecrown-citadel/`, `docs/raids/trial-of-the-crusader/README.md` (code layout lines for the
hoist), `docs/plans/icc-raidobs/` (deleted), and this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
