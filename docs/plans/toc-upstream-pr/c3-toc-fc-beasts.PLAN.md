# c3-toc-fc-beasts

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the
**Wave 1** section bind this lane.

Cleanup on `Custom`, branch `tocpr/c3-toc-fc-beasts`. Apply the overview's "Every cleanup lane"
list to the `c3-toc-fc-beasts` files of the Wave 1 table. Measurements cut from comments go to
`docs/raids/trial-of-the-crusader/faction-champions.md` or `northrend-beasts.md`. RaidObs calls
stay. A change that alters behaviour beyond the listed ones is out of scope: log it under Decisions
for review.

Big lane (~16k lines): split into at most 3 implementers with disjoint files, e.g. Faction
Champions stems, Northrend Beasts stems; the finisher runs the checks.

Lane facts:
- `Action/ToCActions_Jormungars.cpp:479` calls `UseNitroBoosts(bot)` inside
  `... && UseNitroBoosts(bot) && RaidObs::Active()`: restructure so the boost runs outside any
  trace condition and only the trace note stays behind `RaidObs::Active()`, same evaluation order.
- `FactionChampionsGripChain::NoteGrip` (`Util/ToCFactionChampionsGripChain.h:142`, called from
  `Util/ToCHelpers_FactionChampionsDefence.cpp:3208`) is strategy logic despite its name: rename it
  so the PR's RaidObs strip doesn't delete it.
- `PlayerbotAI* ai` locals at `ToCHelpers_FactionChampionsDefence.cpp:893, 922` hold another
  player's AI: name them for whose AI it is.
- Literals to name, e.g. `ToCHelpers_FactionChampionsDefence.cpp:2003` (40000/60000/10000); bare
  doubles at `ToCHelpers_Jormungars.cpp:115, 1027`.
- Long comment blocks, e.g. `ToCHelpers_FactionChampions.h:140` (16 lines),
  `ToCHelpers_FactionChampionsDefence.h:226` (11).

## Task list

Not split (lead, 2026-11-18): the ~18k lines held 57 comment blocks over 2 lines, no dated or "user call"
line, two `ai` locals and 22 unprefixed private members, so the rest was a few hundred mechanical lines plus
the lane items. Line numbers are as of Custom `1039c9045`.

1. [x] Comments: all 57 blocks to 2 lines or less; trace keys out of logic API comments ("Notes nb.x",
   "written to fc.x"); trace and measurement evidence out (`ToCHelpers_FactionChampions.cpp:67` "25H traces",
   `ToCHelpers_Jormungars.h:51, 86`, `ToCHelpers_Icehowl.h:126`, the 3-8 s / 3-4 s / 16-24 yd / 1-2 s
   figures in the FC helpers). Every fact cut was already in `faction-champions.md` or `northrend-beasts.md`.
2. [x] `ai` → `memberAI` (`ElectChannelKicker`), `holderAI` (`KeepOrElectKicker`).
3. [x] Private members `_lowerCamelCase`: Gormok `_spotClearsBombs`; Icehowl `_dodgeSpot`, `_dodgeSpotMs`,
   `_hasDodgeSpot`, `_dodgeParked`, `_triedMs`, `_tried`; Jormungars `_duty`, `_walkSpot`, `_approachFrom`,
   `_walkIssuedMs`, `_walking`, `_issuedSpot`, `_hasIssuedSpot`, `_exitSpot`, `_exitMs`, `_hasExit`;
   `NorthrendBeastsWalkAction` `_walkSpot`, `_walkIssuedMs`, `_walking`; the beasts DPS trigger `_stem`.
4. [x] Namespace `constexpr` → `inline constexpr` in the 8 `Util/` headers; `{};` constructor bodies → `{}`
   (21, action headers).
5. [x] Named literals: `ChampionScore`'s `SCORE_*` (`ToCHelpers_FactionChampionsDefence.cpp:2003`), the
   re-pick's `REPICK_SHIFT_ENTRIES`.
6. [x] RaidObs strip made mechanical: `ToCActions_Jormungars.cpp:479` boosts in the `if` and notes through a
   `NoteNitro` wrapper; `ResolveDpsPick` reads the snobold pick under a skull only while tracing.
7. [x] Logic named like trace calls renamed: `FactionChampionsGripChain::NoteGrip` → `TrackGrip` (and
   `tools/nativetest/fc_grip_chain_test.cpp`), `FactionChampionsNoteChainGrip` →
   `FactionChampionsTrackChainGrip`, `FactionChampionsNoteWallGrip` → `FactionChampionsTrackWallGrip`.
8. [x] Group-walk and w6 audits (Decisions).
9. [x] Checks: `pb-check` over the 32 changed files plus `src/Ai/Raid/ToC` and `tools/botobs` (syntax 38 TUs,
   pblint, codestyle, botobs, nativetest incl. `fc_grip_chain`), `pblint --spell-difficulty src/Ai/Raid/ToC`
   clean.
10. [x] Review (fresh reviewer): no behaviour difference found; `ResolveDpsPick`'s skull branch accepted under the
   overview's logic-only-shape item. Four rewritten comments were wrong or ambiguous and are fixed (`CcCandidates`,
   `UpdateIce`, `FactionChampionsChainGripTarget`, `FactionChampionsChased`); the native test's
   `GripNotedBeforeItsCooldownIsRead` follows the `TrackGrip` rename. Checks green again.

## Decisions for review

- **Lane changes no behaviour.** The two reshapes keep evaluation order and results:
  - Jormungars cure walk: `UseNitroBoosts` now runs in the `if` and only the note sits behind
    `RaidObs::Active()` (inside `NoteNitro`); the note string is built only after a boost. For `p3`: the strip
    leaves `if (IsWormCureReason(reason) && travel > WORM_CURE_NITRO_FROM) UseNitroBoosts(bot);`.
  - `ResolveDpsPick` called `GetGormokSnoboldPick` before the skull test only to keep `nb.snobold` current.
    The pick is a pure read over a per-ms snobold memo, so the skull branch now calls it only while tracing;
    the trace still sees the pick under a skull.
- **Bare doubles kept**: `PREFER_JITTER_STEP = 2.39996` and `2.0 * M_PI` in `Jittered` are doubles on
  purpose. C++ has no double suffix, and `f` would change the jitter bearing at large guid counters.
- **Renamed logic**: `TrackGrip` passes the chain's turn, `FactionChampionsTrackChainGrip` and
  `FactionChampionsTrackWallGrip` record a grip the duties read. `p3`'s strip keeps all three.
- **Literals left as they are**: trigger relevance offsets, the π/8 search step, ms↔s conversions, float
  epsilons, `CycloneSpot`'s local tier 2. Class-static and `.cpp` anonymous-namespace `constexpr` stay;
  botobs' `constexpr float NAME = …;` regex still matches the `inline` form.
- **Comments kept on purpose**: "Also the nb.x value" on the stage, charge, breath and worm enums (botobs
  decodes those numbers); comments on trace-only code (`EraseAll`, `UpdateCcAuras`, the bomb and wedge note
  loops, `GripChainState::posts`, `CloseChain`'s erase, `NoteHold`'s release loops). Comments that justify
  logic by the trace stay for `p3` to reword with the strip: "These two check live first"
  (`FactionChampionsKillTarget`), "Null unless live" (Ball and Defence refresh readers), "Written every
  refresh" (`UpdatePhysicalLatch`), "Assigned again each read" (`OpenChain`).
- **Trace-only for `p3`** beyond the overview's list: `FactionChampionsState::ccAuras`, `UpdateCcAuras`,
  `EraseAll`; `BallState::cyclone` and its release loop (`cycloneStands` keeps its own); `GripChainState::posts`;
  `FactionChampionsDefenceState::hazardNotedMs`, `repickNoteMs`, `repickNoted`, `REPICK_NOTE_*`; Gormok
  `FireBomb::noted` and its note loop; Jormungars `spewNoted` and `ReadWorm`'s wedge note; Icehowl `NoteLane`
  and `LatchLane`'s `note` parameter; `ResolveDpsPick`'s skull-branch snobold read.
- **Group-walk audit** (no fixes, per the overview). Uncached walks a trigger's `IsActive` reaches, as line at
  Custom `1039c9045`: function ← trigger:
  - `ToCHelpers_FactionChampions.cpp:1367` `CounterspellMage` ← `faction champions kill target healing`
    and the late interrupt multiplier; mages only, while the kill target casts a heal Counterspell stops.
  - `ToCHelpers_FactionChampionsBall.cpp:199` `GroupOnMap` from `FactionChampionsChased` (802) ←
    `faction champions chased`; a focused bot with Nitro Boosts ready and no run latched.
  - Same walk from `FactionChampionsKite` (1100) ← `faction champions kite`; a bot with chasers once its
    2.5 s kite step lapsed.
  - `ToCHelpers_Jormungars.cpp:966` `HunterIndex` ← `GetWormRedirectTarget` ← `northrend worms redirect
    threat`; hunters while a worm is engaged.
  Every other walk sits behind a per-instance per-ms memo (FC, Ball, Defence, Impale reset, Jormungars,
  Toxin heals, Beasts deal) or a roster interval (Icehowl breath, 1 s), or runs on a chain open only.
- **w6 audits**: every strategy-wired lane multiplier tests its action family (cached `RaidActionCache` or a
  cheap cast; the worm redirect guard the class first) before `ToCEncounterIsLive`; definition rules are
  family-filtered and `ToCGate`-gated. No `GetFirstAliveUnitByEntry`, no `prioritized targets`. AI value
  writes are the bot's own: `last movement` clears, `current target` cleared by the ball action and the pull
  takeover, `position["pull"]` erased there. Per-instance state: every FC reader answers null once the
  encounter isn't live, so nothing reaches Twins; the Beasts deal re-reads the world every ms and empties with
  no beast engaged, Jormungars state clears on disengage, pools and Toxin heals close with the gate. Lust: the
  Beasts window holds it for Icehowl's daze or 50%, Faction Champions spends it at the pull; Sated stops a
  second cast.

## Known gaps

- The four uncached group walks above stay; caching them is for `p4` or a later Custom pass to weigh against
  upstream's O(1) `IsActive` rule.
- 11 lines over 120 columns in the lane's files predate this lane (14 before); none was added.
- No GPLv2 header on the lane's files; the PR branch adds them.

## Blocked

None.
