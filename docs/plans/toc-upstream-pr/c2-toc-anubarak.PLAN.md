# c2-toc-anubarak

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the
**Wave 1** section bind this lane.

Cleanup on `Custom`, branch `tocpr/c2-toc-anubarak`. Apply the overview's "Every cleanup lane"
list to the `c2-toc-anubarak` files of the Wave 1 table, plus the `"tree of life"` → `"tree form"`
fix it assigns to `c2`. Measurements cut from comments go to
`docs/raids/trial-of-the-crusader/anubarak.md`. RaidObs calls stay. A change that alters behaviour
beyond the listed ones is out of scope: log it under Decisions for review.

Big lane (`Util/ToCHelpers_Anubarak.cpp` alone is ~10k lines): split into at most 3 implementers
with disjoint files; the finisher runs the checks.

Lane facts:
- Most long comment blocks live here (`ToCHelpers_Anubarak.cpp` 52, `ToCHelpers_Anubarak.h` 24),
  along with most dated and "user call" lines (e.g. `Util/ToCAnubarakKitePlanner.h:34`,
  `ToCHelpers_Anubarak.cpp:149-153`).
- `PlayerbotAI* ai` locals at `ToCHelpers_Anubarak.cpp:1202, 2668, 4343, 8749, 8859` hold another
  player's AI: name them for whose AI it is (`targetAI`, `paladinAI`, …).
- `NoteAnubarakGripCast` (`ToCHelpers_Anubarak.cpp:8386`, called from `ToCActions_Anubarak.cpp:558`)
  is strategy logic despite its name: rename it so the PR's RaidObs strip doesn't delete it.
- Inline turn tables at `ToCHelpers_Anubarak.cpp:3637, 4949, 8570` and `i < 12` at `:8585` become
  named constants.
- `Definition/ToCDefinition_Anubarak.cpp:219`: `"tree of life"` → `"tree form"` (the druid action's
  registered name).

## Task list

Split (lead, 2026-11-18): two implementers on disjoint files, then a finisher. Line numbers are as of
Custom `1039c9045`; locate by content once edits shift them.

Rules for every comment edit (both groups):
- At most 2 lines; keep only the invariant or the non-obvious why. Name the fact, not how it was found:
  "navprobe-clean out to 50 yd" → "walkable out to 50 yd".
- Out of code comments, into `needsIntegration` verbatim with the constant or function it sat on, for the
  finisher to fold into `anubarak.md`: measurements, dates, "user call", pull/trace/replay evidence
  (`2026-10-30 Z`, "six 25H pulls", "read off the traces").
- A trace key (`anub.xxx`) in a comment on logic code or a logic API goes; report the key and its meaning
  if the comment explained it. A trace-only function (`NoteHands`, `KitePathText`, `KitePredText`,
  `NoteDpsPick`, label helpers) may keep a format note of up to 2 lines.
- No behaviour change, no rename of an `ANUBARAK_*` constant, a `Rules`/`PatchRules`/`EscortRules` field
  or a public function: `tools/botobs` reads `constexpr float NAME = <number>;` and the planner's
  `struct Rules` defaults by name, `tools/nativetest/anubarak_{planner,pursuit}_test.cpp` compile the two
  planner headers. RaidObs calls stay.
- Comments follow the no-nonsense-comments rule and the `use-conversational-language` skill.

**G1 `helpers-cpp`** owns `src/Ai/Raid/ToC/Util/ToCHelpers_Anubarak.cpp`:
1. The 52 blocks over 2 lines: 88, 137, 148, 171, 176, 198, 217, 597, 1070, 1091, 1287, 1423, 1530, 1667,
   1742, 1773, 2062, 2162, 2414, 2466, 2563, 2672, 3061, 3129, 3370, 3505, 3762, 3934, 4335, 4368, 4415,
   4536, 4668, 4760, 4907, 5400, 5721, 6218, 6836, 7669, 8317, 8550, 8707, 8770, 8822, 8944, 8996, 9052,
   9101, 9383, 9409, 9586.
2. History lines: 149-153, 171, 192, 210, 217, 258, 264, 291, 7282-7283, 9563; trace/evidence mentions
   at 414, 628, 1175, 1806, 1892, 1947, 7221, 8998, plus `navprobe` wording at 127, 211, 261, 296, 1271,
   5276, 5511.
3. `PlayerbotAI* ai` locals holding another player's AI: 1202 → `kiterAI`, 2668 → `playerAI`, 4343,
   8749, 8859 → `paladinAI`.
4. Named namespace-scope `constexpr` arrays/counts, same values and order: turn tables at 3637, 4949,
   8570; the 12-bearing, 30° rings at 8585, 8683, 8976. The `FindNearestPositionClearOfHazards` step
   pair (`1.5f`, `M_PI / 8.0f`) at 5573, 5827-5831, 7041-7042, 9679-9682 gets one pair of constants if
   every site means the same search step. Leave `SIDE_AOE_SPELLS` rows, epsilons, char buffers and the
   trace-text rounding.
5. Group-walk audit, no fixes: for each `ForEachGroupMember` / `GetFirstMember` site (~30), whether it
   runs behind a per-instance memo (`…MemoValid`/`…Ms`, `Sweep`) or only from actions. Report, as
   `site line: function ← trigger(s)`, every one a trigger's `IsActive` reaches uncached.
6. Confirm every `RaidObs::Active()` condition and `Note*` argument holds only pure reads; report any
   that doesn't instead of restructuring it.
- Do not touch `NoteAnubarakGripCast` (finisher renames it).

**G2 `headers-engine`** owns `Util/ToCHelpers_Anubarak.h`, `Util/ToCAnubarakKitePlanner.h`,
`Util/ToCAnubarakPursuit.h`, `Action/ToCActions_Anubarak.{h,cpp}`, `Trigger/ToCTriggers_Anubarak.{h,cpp}`,
`Multiplier/ToCMultipliers_Anubarak.{h,cpp}`, `Definition/ToCDefinition_Anubarak.cpp`:
1. Blocks over 2 lines. `ToCHelpers_Anubarak.h`: 120, 201, 417, 422, 445, 472, 479, 508, 513, 517, 525,
   540, 560, 573, 591, 641, 648, 653, 689, 700, 729, 743, 754, 759. `ToCAnubarakKitePlanner.h`: 194, 679,
   797, 1054, 1158, 1239. `ToCAnubarakPursuit.h`: 9, 142, 324, 591, 626, 663, 699.
   `ToCMultipliers_Anubarak.h`: 49, 70, 94. `ToCActions_Anubarak.cpp`: 120. `ToCDefinition_Anubarak.cpp`:
   198.
2. History lines: `ToCAnubarakKitePlanner.h` 34, 143, 163, 925; `ToCHelpers_Anubarak.h` 25-26 ("25-man
   pulls carry"), 101, 214 ("Replayed"), 173 ("measured on him"); `ToCAnubarakPursuit.h` 10. Trace-key
   parentheticals in `ToCHelpers_Anubarak.h` API comments (230, 250, 447, 514, 517-519, 527, 563, 582,
   593, 621, 650, 655, 693, 701 and any other). `navprobe` wording at `ToCHelpers_Anubarak.h` 30, 132,
   `ToCAnubarakKitePlanner.h` 115.
3. Namespace-scope `constexpr` in the three `Util/` headers → `inline constexpr` (the `constexpr float
   NAME = <number>;` tail stays, botobs matches it). `extern const Position` stays: `Position` has no
   constexpr constructor.
4. Named constants in the two planner headers (no game headers there; nativetest builds them):
   degrees→radians `0.0174532925f` (KitePlanner 351, 884, 987, 1008, 1186; Pursuit 688-689, 737), the
   bearing steps `6.28318531f`, `0.392699082f`, `0.785398163f` with their counts `i < 16`, `i < 8`
   (Pursuit 603, 713-715, 751-753), the turn list at Pursuit 735, KitePlanner 874's `10`. Epsilons stay.
5. Private members → `_lowerCamelCase`, uses included: `ToCActions_Anubarak.h` `physicalOnly`,
   `bubbleSpell`, `bubbleMs`; `ToCMultipliers_Anubarak.h` `held`, `splash`, `names`, `heals`. A free
   function's `splash` parameter keeps its name.
6. `ToCDefinition_Anubarak.cpp:219` keeps `"tree of life"` (Decisions for review).

**Finisher**:
1. Rename `NoteAnubarakGripCast` → `TrackAnubarakGripCast` (`ToCHelpers_Anubarak.h:672`,
   `ToCHelpers_Anubarak.cpp:8386`, `ToCActions_Anubarak.cpp:558`); it writes the grip cast the duty
   logic reads, so the PR's RaidObs strip must keep it.
2. Fold the implementers' `needsIntegration` facts into `docs/raids/trial-of-the-crusader/anubarak.md`
   with `compact-docs-writer`: skip what it already states, trace keys under "What a trace answers".
3. G1's group-walk list and anything either group reports under Decisions for review; checks, review,
   commit per the overview.

Status (2026-11-18): G1, G2 and the finisher are done. Checks green on the worktree: `pb-check` over
the 9 changed `src` files plus `src/Ai/Raid/ToC` (syntax 10 TUs, pblint, codestyle, nativetest incl.
`anubarak_planner`/`anubarak_pursuit`), `pb-check tools/botobs` (botobs suite, which parses the planner's
`Rules` and the `constexpr float` constants) and `pblint --spell-difficulty src/Ai/Raid/ToC` clean.

Review (2026-11-18): no behaviour change beyond the logged ones; the new step constants are
bit-identical to the old literals. Five rewritten comments that misstated the code were corrected
(`GetAnubarakMeleeBurrowerStand`, `GetAnubarakEscortSpot`, `KitePatchRank`, the Righteous Defense
pick, `AnubarakBurrowerGuardMultiplier`), plus Feral Charge's "bug" wording and the pickup-point line
in `anubarak.md`.

## Decisions for review

- **`"tree of life"` stays; the planned `"tree form"` fix is not applied.** Block lists match
  `action->getName()` (`RaidEncounterRules::CaresBits` via `RaidEncounter.cpp:162`), and
  `CastTreeFormAction` reports the spell name it passes `CastBuffSpellAction`, `"tree of life"`
  (`DruidShapeshiftActions.h:40`); only the creator key is `"tree form"`. The rule matches today, and
  `"tree form"` would match nothing. `docs/engine/action-selection.md` ("A rule matches `getName()`
  too") and `anubarak.md` (`anubarak kite cat hold`) already say so. The overview's premise is wrong;
  `p3`/`p4` must not apply it either. This lane changes no behaviour.
- No braces around single statements and no unsuffixed floating literals in the lane's files (checked
  with a scan); nothing to do for those two items.
- Multipliers: the six definition-wired ones are family-filtered by the cares table and gated by
  `ToCGate`; the two strategy-wired ones (`AddToCAnubarakMultipliers`) test the action family or a
  per-action cache first and `AnubarakEngaged` before returning 0. No `GetFirstAliveUnitByEntry`, no
  `prioritized targets`. The only AI value write is the bot's own `last movement` clear. Encounter state is
  `RaidInstanceState<AnubarakState>` per instance, cleared by `ClearPullLatches` on a new pull, and
  Anub'arak is the last boss, so nothing reaches a later one.
- Lust: `GetAnubarakLust` casts only on a shaman without Sated/Exhaustion once `AnubarakLustOpen`, and
  the class lust is held by `ToCAnubarakBurstWindow` behind the same gate. Not spent twice.
- RaidObs strip: all ten `RaidObs::Active()` conditions in `ToCHelpers_Anubarak.cpp` hold only pure
  reads. One `Note*` argument didn't: `CountSpentKitePatches` incremented `state.kitePatchesSpent` inside
  `RaidObs::NoteDerived(…, std::to_string(++…))`. The increment now precedes the call (same order and
  value). **`kitePatchesSpent` is logic, not trace-only**: `KiteWorldFor` reads it for heroic's
  `reservePatch`. The overview's p3 list naming it a trace-only field is wrong; p3 keeps the field and
  its increment and drops only the note.
- For `p3`: `NoteDpsPick` returns its pick, so each call becomes its second argument (all are `return
  NoteDpsPick(bot, <var|nullptr>, "…")`). Trace-only, deleted with the strip: `state.grip`, `GripMode`
  and the `gripWaits` local; `ReadPhase`'s `phase`/`patches`/`flying`/`side0`/`side1` `ObsValue`s and its
  `patchCount`/`flyingCount` locals; `burrowerSide`; `TrackKiter`'s block noting `none` on the last kiter;
  `GetAnubarakEscortSpot`'s `post`/`patchPost`/`standby` out-params (only the hop escort trigger's trace
  reads them). Set-but-unused once the notes go, so delete them too: `verdict` in `AnubarakBurstHeld`,
  `AnubarakAuraMasteryDue`, `GetAnubarakScarabKite`; `why` in `GetAnubarakSideTankAoe`; `label` in
  `GetAnubarakSwarmHealPick`; `name` in `GetAnubarakGripDuty`; `nearSpike` in `GetAnubarakSpikeDodgeSpot`.
- `NoteAnubarakGripCast` is now `TrackAnubarakGripCast` (declaration, definition, its one call).
- Group-walk audit (no fixes, per the overview). Uncached walks a trigger's `IsActive` reaches, as
  `ToCHelpers_Anubarak.cpp` line at Custom `1039c9045`: function ← triggers:
  - 5130 `GetSpikeTarget` ← `ValidDodgeHold` ← `IsAnubarakDodgeHeld`/`AnubarakDodgeWalking` ← RaidGathers,
    HealerStand, RangedStand, OnPermafrost, BurrowerLead, ScarabKite, SpikeNearby; only for a bot with a
    live dodge hold.
  - 6079, 6114 `GetAnubarakBattleResTarget` ← SideTankDead, and via `GetAnubarakBurrowerLead` /
    `GetAnubarakRangedStand` HealerStand, RangedStand, BurrowerLead; only for an unmarked druid bot with
    Rebirth ready (6114 only the picked druid).
  - 7317 `GetAnubarakPenetratingColdHealTarget` ← PenetratingColdOnRaid: every healer, every phase but
    heroic phase 3.
  - 7539 `HealsOnTheWay`, 7718 `lowestTank`, 7753 `tierPick`, all in `GetAnubarakSwarmHealPick` ←
    SwarmHealUrgent, SwarmHeal (heroic phase 3).
  - 7938 `AnubarakAuraMasteryDue` ← AuraMasteryDue; 7987 `AnubarakDivinePleaDue` ← DivinePlea; 8056
    `GetAnubarakTankRescue` ← TankNeedsRescue; 9846 `GetAnubarakRedirect` ← RedirectThreat (hunters, rogues).
  - 9316 `BehindStand` ← HealerStand (heroic, until the healer set off); 9364 `GetAnubarakHealerStand` ←
    HealerStand (every healer, every tick he is up).
  Every other walk sits behind a per-instance memo, latch or the 250 ms plan gate.
- `FindNearestPositionClearOfHazards` step pairs stay literal: `(2.0f, π/8)` for the submerge spot and the
  dodge, `(1.5f, π/8)` for the tank and Permafrost step-outs, so not one shared search step.
- New named constants, same values and evaluation order: `ANUBARAK_FAR_GRIP_TURNS_DEG`,
  `ANUBARAK_MELEE_STAND_TURNS_DEG`, `ANUBARAK_SHADOW_TURNS_DEG`, `ANUBARAK_RING_BEARINGS`/`_STEP_DEG`
  (helpers); `DEG_TO_RAD`, `TWO_PI`, `DRAG_PRE_STEP_*`, `RELAY_POST_TURNS_DEG`, `RING_POINT_*`
  (`AnubarakPursuit`, the step angles bit-identical to the old literals); `HEADING_TICKS` (planner).
  botobs now also reads `ANUBARAK_RING_BEARING_STEP_DEG`, `DEG_TO_RAD` and `TWO_PI`; nothing looks them up.
- Comments kept on trace-only code (p3 deletes them with it): `GripMode`'s "Written raw into anub.grip",
  `ReadPhase`'s note on the `LogHitsOn` re-ask, the `NoteHands`/`KitePathText`/`KitePredText` format notes.
- `anubarak.md` already carried nearly every fact cut from the comments (user calls, replay and trace
  figures, navprobe results, most trace keys). Added (+64 words): the heroic pickup point's why, the
  spike's idle drift through the gather (2026-10-28 S) in the dodge track, `dside`'s split rule and the
  `kite` key's `none` reason.

## Known gaps

- The uncached group walks above stay; caching them is a behaviour-adjacent change for `p4` or a later
  Custom pass to weigh against upstream's O(1) `IsActive` rule.
- 28 lines over 120 columns predate this lane (25 in `ToCHelpers_Anubarak.cpp`, `ToCAnubarakKitePlanner.h`
  544 and 1077, `ToCActions_Anubarak.h` 284); none was added and codestyle passes.

## Blocked

None.
