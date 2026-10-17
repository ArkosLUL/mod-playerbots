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

Not split: the lane's files already meet most Cleanup items (no comment over 2 lines, no `ai`, no `0.f`),
so the remaining work is a few hundred mechanical lines plus the lane-only items.

1. [x] Entry finder `GetUldUnitByEntry` in new `Util/UldTargets.{h,cpp}`; swap all 82 Ulduar calls (c2/c3
   files too) and the two comments that name the old helper.
2. [x] `MoveOutcome` as a global enum in `MovementActions.h`; `TryMoveTo`/`MoveToImpl` return it. RaidObs keeps a
   mirror (`RaidObs::MoveOutcome`) checked by static_asserts in `MovementActions.cpp`, like `MovePriority`.
   Rename the ToC and Ulduar call sites.
3. [x] Auriaya room scan keeps pool GUIDs; `GetAuriayaRoomPools` resolves them on each call.
4. [x] `Playerbots.UlduarYoggSaronHardMode` conf text.
5. [x] Auriaya (2) and Kologarn (1) floor-fall rescues: `NearTeleportTo(...); return true;`, master-map guard
   on Auriaya's master fallback.
6. [x] Cleanup list over the lane files: narration in comments, braces on single-statement bodies,
   `ObjectGuid const&` loops, header constants `inline constexpr`, `ADD/REMOVE_STRATEGY_CHAR`, stray
   literals (Razorscale mostly), isUseful/Execute reuse where mechanical.
7. [x] Measurements cut from comments into the boss docs; docs naming the old helper or
   `RaidObs::MoveOutcome` renamed.

## Decisions for review

- **Entry finder**: `GetUldUnitByEntry(botAI, entry)` in `Util/UldTargets.{h,cpp}`, the same body as
  `EncounterHelpers::GetFirstAliveUnitByEntry`. Swapped in c2/c3 files too (Mimiron, Thorim, Flame
  Leviathan, Freya comment); docs that named the old helper for Ulduar now name the new one
  (`engine/pitfalls.md`, `raids/ulduar/{README,auriaya,kologarn,mimiron,thorim}.md`).
- **`MoveOutcome`** is a global enum next to `MovementAction` in `MovementActions.h`. `RaidObs.h` keeps a
  mirror because Bot/Obs must not depend on Ai (the `MovePriority` precedent); `MovementActions.cpp`
  static_asserts the two and converts at `NoteMove` (`ObsOutcome`). `JumpTo` and the Follow/Chase trace
  notes still use `RaidObs::MoveOutcome` directly, since they only feed the trace. `MovementActions.h`
  still includes `RaidObs.h` on Custom so transitive users keep compiling. The rename touched four ToC
  files outside this lane (`ToCActions_{FactionChampionsDefence,Icehowl,Jaraxxus,Jormungars}`), required
  for them to compile.
- **Auriaya room scan**: `GetAuriayaRoomPools` now returns resolved units by value; a pool that despawns
  mid-tick drops out instead of dangling. The two-argument `GetAuriayaAnchor` passes `nullptr` and lets
  the main-tank branch fetch the pools.
- **Yogg conf**: `IsYoggSaronHardModeActive` has no caller; only `RaidObsConfig.cpp` reads the option for
  the trace header. The text now says bots ignore it and read the freed Keepers from the instance.
- **Floor-fall rescues** now `NearTeleportTo(...); return true;` (the listed fix: a same-map `TeleportTo`
  calls `CombatStop`). Auriaya's master fallback requires `master->GetMap() == bot->GetMap()`.
- **`ADD/REMOVE_STRATEGY_CHAR`** became `inline constexpr char const*` in `UldActions_Shared.h`;
  `UldActions_Shared.cpp` is deleted (it only held the two definitions).
- **Razorscale literals** are now `RazorscaleBossHelper` constants (airborne tank flame scale, sentinel
  radius, arena leash/return, permanent ground health, first-landing spot and stack, assist tank slots,
  Flame Breath cone/range). The duplicated first-landing test is `IsRazorscaleAtFirstLanding`; the moon
  icon reads `RtiTargetValue::moonIndex`; the harpoon loop reuses `IsRazorscaleHarpoonCrew` (same
  predicate). `_roleSwapCooldown` is `static constexpr`, its out-of-line definition dropped. Values are
  unchanged, including the 18 yd first-landing `MoveInside` radius written as `30 - 12` before.
- **Other named literals**: Hodir sweep steps, trapped-ally switch margin and the fallback icicle margin
  (`+ 0.5f` now reads the existing `ULDUAR_HODIR_DODGE_TRIGGER_MARGIN`, same value); Iron Assembly escape
  sweep steps; the Yogg Keeper loop runs `KEEPER_FREYA..KEEPER_THORIM`; `IN_MILLISECONDS` for the two
  ms conversions; an XT-002 search margin and an Ignis reserve size as locals. Left as literals: trigger
  check intervals, `SetNextMovementDelay(1000)`, Yogg phase numbers, the Iron Assembly alive bitmask,
  float epsilons.
- **Comments**: past-tense incident narration rewritten (XT-002 anchors, Vezax slot reset and presence
  gate, Hodir clear-vs-lethal twice, Hodir shuttle probe, Razorscale "freeze bug"). The XT-002 ranged
  anchor history moved to `xt002.md`; the others were already in their boss docs. The MoveOutcome
  comment lost its dashes on the move.
- **isUseful/Execute reuse**: no site in the lane is mechanical. Every `isUseful` either runs the node's
  trigger or is a class check, and Razorscale's flame dodge already caches its scan.
- **Braces** kept where an `else` sibling needs them (Razorscale pet control, XT-002 parked latch, Hodir
  role pick) and around nested `if` bodies. Two Razorscale nested ifs merged into one condition, same
  evaluation order.

## Known gaps

- 37 lane files (61 under `src/Ai/Raid/Uld/`) have no GPLv2 header; not a Cleanup item, so it is left for
  the PR branch.
- Razorscale grounded ranged: `isUseful` fires beyond 14 yd of the first-landing stack point but
  `Execute`'s `MoveInside` radius is 18, so a bot 14-18 yd out fires the node and returns false each tick.
  Behaviour, not touched.
- `RazorscaleBossHelper::FindNearestHarpoon` has no caller.

## Blocked

None.
