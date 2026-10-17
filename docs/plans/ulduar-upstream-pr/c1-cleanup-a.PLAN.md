# c1-cleanup-a

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

Behaviour-neutral cleanup on `Custom`, branch `uldpr/c1-cleanup-a`. Apply the **Cleanup lanes** list of `ulduar-upstream-pr.PLAN.md` to the files below. Measurements cut from comments go to the boss doc in `docs/raids/ulduar/`. RaidObs calls stay. Any change that alters behaviour beyond the listed fixes is out of scope: log it under Decisions for review instead.

Files: every `src/Ai/Raid/Uld/**` file whose stem ends in `_Auriaya`, `_Ignis`, `_Kologarn`, `_Razorscale`,
`_XT002`, `_Vezax`, `_IronAssembly`, `_Hodir`, plus the shared files: `Uld*Context.h`, `UldStrategy.*`,
`UldEncounterGate.h`, `Definition/UldDefinitions.h`, the umbrella `Action/UldActions.h`, `Trigger/UldTriggers.h`,
`Multiplier/UldMultipliers.h`, `*_Shared.*`, `Util/UldData.h`, `Util/UldHardMode.*`, `Util/UldBotScripts.cpp`,
`Util/UldScripts.h`.

Lane-only items (the overview's "`c1` also" list):
- An Ulduar-local entry finder (same logic as `EncounterHelpers::GetFirstAliveUnitByEntry`, `src/Util/EncounterHelpers.cpp`)
  in an Ulduar util file; switch every Ulduar call site to it, all bosses, including files owned by c2/c3
  (call-site swap only there). Leave `EncounterHelpers` itself and other raids alone.
- Move `MoveOutcome` out of `src/Bot/Obs/RaidObs.h` next to `MovementAction::TryMoveTo`
  (`src/Ai/Base/Actions/MovementActions.h`), keeping RaidObs compiling.
- `UldEncounter_Auriaya.cpp:72` room-scan cache stores GUIDs, not `Unit*`.
- `conf/playerbots.conf.dist`: the `Playerbots.UlduarYoggSaronHardMode` text describes cheat kills that no longer exist; rewrite it to what `IsYoggSaronHardModeActive` callers do.
- `TeleportTo` → `NearTeleportTo` with the master-map guard at `Action/UldActions_Auriaya.cpp` (both calls) and `Action/UldActions_Kologarn.cpp`.

## Task list

## Decisions for review

## Blocked
