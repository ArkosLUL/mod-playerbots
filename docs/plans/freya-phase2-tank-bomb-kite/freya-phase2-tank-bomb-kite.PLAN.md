# Freya phase 2: move the boss out of the bombs, and stop the escapes fighting each other

## Context

Freya 25 hard mode was killed on 2026-09-05 at 7:18.695 with 7 deaths
(`env/dist/logs/botobs/603_1_elder-stonebark_1788613108.ndjson`). This trace **does** reflect current
code: `freya lasher about to blow` fires 204 times and `freya avoid aoe hold` vetoes 108 times, and
the worldserver binary was built 12:38 UTC, three minutes after `d81f7f976`.

The Detonating Lasher camp change from `d81f7f976` worked and is not touched here. Both waves came in
at the number it was sized for — ranged/healer median distance to the nearest lasher 19.6 and 19.1 yd
with 17.5% and 20.8% of the wave inside Detonate, against a target of 18+ yd and ~18.5%.

**Phase 2 is what is broken.** It runs 4:47 (first damage on Freya) to 7:18. Nature Bombs start at
5:29 and arrive in six volleys of 7-10, every ~18 s, with a 7.0 s fuse and a measured 10 yd blast
(victim distance to nearest bomb: median 5.6, p90 9.0, max 11.4). Five of the seven deaths are in this
window and every one is a ground hazard — Nature Bomb x3, Sun Beam x2 — with all five dying
`[STILL WALKING]` toward a dodge destination they never reached.

Raid damage halves the moment bombs start (alive-only, damage/s):

| | waves | phase 2 pre-bomb | bomb phase |
|---|---|---|---|
| melee | 48,680 | 53,543 | 25,574 (-52%) |
| ranged | 53,274 | 77,800 | 39,379 (-49%) |
| **total** | **104,863** | **136,061** | **68,006 (-50%)** |

Melee uptime within 5 yd of Freya across phase 2 is **7-18%** (median distance 6.5-15.9 yd). The tank
sits at 4.3 yd with 65%.

Four independent defects produce that, all measured from this trace:

1. **The bomb escape has no latch.** The trigger fires at 11 yd and the escape aims for 13 yd of
   clearance, so the median hop is **1.9 yd** — barely past the line. `reach melee` pulls the bot
   straight back inside 11 and it re-fires. **1231 direction reversals in 109 s**; Totemist issued 176
   move orders per minute, one every 108 ms. Melee walked 400-530 yd of path to finish 25 yd from
   where they started. `FreyaDodgeUnstableSunBeamAction` in the same file already solves this with
   `dodgeSpot`/`dodgeSpotMs` and returning `true` to claim the tick; the bomb action never got it.
2. **The escape fails outright 42% of the time** — 164 of 394 `act` records are `FAILED`, all in the
   bomb phase. `Execute` returns false when no spot within 30 yd clears every bomb by 13 yd; 77% of
   those ticks have 4+ bombs in range. The bot then just stands in the blast. The sun-beam dodge has a
   reduced-clearance fallback for exactly this case; the bomb escape has none.
3. **The two escapes are blind to each other.** 30% of bomb-escape destinations land within 12 yd of a
   live Sun Beam, and **61% of sun-beam destinations land within 11 yd of a live bomb**. Both nodes sit
   at relevance 64, so bots alternate between them (Smartface flipped escape type 56 times). This is
   what killed Prayer and Druidica — Druidica died standing in three Unstable Energy patches while
   running the bomb escape.
4. **Nothing ever moves Freya.** `FreyaNearNatureBombTrigger` returns false for tanks, and no Freya
   tank-position action exists at all. Across six volleys the tank's distance to Freya was 4.2, 4.2,
   4.3, 4.3, 4.3, 4.3 with up to 5 bombs within 12 yd — he never moved once. He took **55,569 Nature
   Bomb damage over 10 hits**, second-worst in the raid, and spent 30% of the bomb phase inside a
   blast. Bombs land at players' feet and the melee stack is on the boss, so **23 of 48 bombs land
   within 10 yd of Freya** and the melee ring is inside a blast 34% of the bomb phase.

The two reasons the code gives for excluding tanks do not hold. The Ancient Conservator was dead at
2:56, long before the first bomb, and parking it is assist tank 0's job via `ParkConservator` — the
main tank never parks anything. "Dragging Freya toward the raid" is a direction problem, and
`FindNearestPositionClearOfHazards` already takes a `preferNear` anchor that the bomb action never
passes.

Intended outcome: bomb-phase raid damage recovers toward the 136k the raid managed before bombs
started, melee uptime on Freya rises out of the 7-18% band, and phase 2 stops producing five
ground-hazard deaths per kill.

## Approach

Four changes. **None of them zeroes a movement action or widens spacing past a bot's reach** — that is
what broke `45d9a3e36`. Melee keep following Freya on the generic `reach melee` closer, untouched.

### A. The main tank walks Freya out of the bomb field

New node, main tank only, so the existing bot-escape node keeps its current shape.

- **Trigger** `FreyaTankNatureBombTrigger` in `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.{h,cpp}`:
  Freya alive via `AI_VALUE2(Unit*, "find target", "freya")` (same gate as the existing bomb trigger),
  `PlayerbotAI::IsMainTank(bot)`, then
  `bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS)`.
  `FreyaNearNatureBombTrigger` keeps its blanket `botAI->IsTank(bot)` exclusion, so assist tank 0 is
  still out and can hold the Conservator on its spore; its comment needs rewording to say that is now
  the assist tank's reason, not every tank's.
- **Action** `FreyaTankNatureBombAction` in `src/Ai/Raid/Uld/Action/UldActions_Freya.{h,cpp}`, modelled
  line for line on `FreyaDodgeUnstableSunBeamAction` (`UldActions_Freya.cpp:393-470`): gather hazards,
  `stillClear` lambda, latch on `kiteSpot`/`kiteSpotMs`, `FreyaClearCastBlockingMove`, `MoveTo` at
  `MovementPriority::MOVEMENT_FORCED`, return `true` while the walk is in flight.
- **Direction.** Pass a `preferNear` anchor to the `HazardCircle` overload of
  `FindNearestPositionClearOfHazards` (`src/Util/EncounterHelpers.cpp:331-390`) — it reorders only
  spots that are the same walk away, so it can never talk the tank into a longer trip. The anchor is
  the raid centre reflected through Freya: take `GetFreyaRangedCampAnchor(botAI)` (the lowest-GUID
  living ranged DPS, the codebase's existing no-shared-state way of naming where the back line is),
  and aim at `freya + (freya - anchor)` normalised out to `ULDUAR_FREYA_HAZARD_SEARCH_RADIUS`. When
  there is no living ranged DPS, pass `nullptr` and take the shortest walk.
- **Wiring** `src/Ai/Raid/Uld/UldStrategy.cpp` at `ACTION_RAID + 4` (64), alongside the other two
  escapes, plus `UldTriggerContext.h` and `UldActionContext.h` registration. It outranks
  `freya tank adds` (61), which is what has to happen. A main tank standing in both a bomb and a sun
  beam has two nodes at 64; after change D both route around both hazard sets, so the tie is harmless.
- New constant in `src/Ai/Raid/Uld/Util/UldEncounter_Freya.h`:
  `ULDUAR_FREYA_TANK_BOMB_LATCH_MS = 3000` (13 yd at 7 yd/s is under 2 s, same reasoning as
  `ULDUAR_FREYA_SUN_BEAM_LATCH_MS`).

### B. Latch the bot bomb escape

`FreyaMoveAwayNatureBombAction` (`UldActions_Freya.cpp:52-68`) gains `bombSpot`/`bombSpotMs` private
members and the same claim-the-tick latch as the sun-beam dodge, with
`ULDUAR_FREYA_NATURE_BOMB_LATCH_MS = 3000`. It must also call `FreyaClearCastBlockingMove(bot)`
(`UldActions_Freya.cpp:39-43`) before the `MoveTo`, which it currently does not — a caster whose spell
blocks movement never leaves the blast today.

### C. Fallback clearance when the volley overlaps

Same shape as `UldActions_Freya.cpp:449-451`: when nothing clears every bomb by
`ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS`, retry at `ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS + 1.0f`
before giving up. Barely outside beats standing in one. Applies to the tank action too.

### D. Make the two escapes see each other's hazards

- Extract the sun-beam scan that `FreyaDodgeUnstableSunBeamAction::Execute` inlines
  (`UldActions_Freya.cpp:395-414`) into `GetFreyaSunBeamPositions(PlayerbotAI* botAI, float
  searchRadius)` in `src/Ai/Raid/Uld/Util/UldEncounter_Freya.{h,cpp}`, next to
  `GetFreyaNatureBombPositions` (`UldEncounter_Freya.cpp:390-404`). It needs `botAI` rather than
  `Player*` because the beam stalkers are non-selectable and only reachable through
  `AI_VALUE(GuidVector, "nearest npcs")`.
- All three escapes then build one `std::vector<HazardCircle>` — bombs at
  `ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS`, beams at `ULDUAR_FREYA_SUN_BEAM_CLEARANCE` — and call the
  `HazardCircle` overload (`EncounterHelpers.h:68`).
- **Never let cross-awareness strand a bot.** If the combined set clears nothing, fall back to the
  bot's own hazard type alone, then to the reduced clearance from C, before returning false. Standing
  in someone else's hazard beats standing in your own.

## Files

- `src/Ai/Raid/Uld/Util/UldEncounter_Freya.{h,cpp}` — `GetFreyaSunBeamPositions`;
  `ULDUAR_FREYA_TANK_BOMB_LATCH_MS` and `ULDUAR_FREYA_NATURE_BOMB_LATCH_MS`.
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.{h,cpp}` — `FreyaTankNatureBombTrigger`; reword the
  exclusion comment in `FreyaNearNatureBombTrigger`.
- `src/Ai/Raid/Uld/Action/UldActions_Freya.{h,cpp}` — `FreyaTankNatureBombAction`; latch, cast-block
  clear, fallback and combined hazards in `FreyaMoveAwayNatureBombAction`; combined hazards in
  `FreyaDodgeUnstableSunBeamAction`.
- `src/Ai/Raid/Uld/UldStrategy.cpp`, `src/Ai/Raid/Uld/UldTriggerContext.h`,
  `src/Ai/Raid/Uld/UldActionContext.h` — register and wire the new node at `ACTION_RAID + 4`.
- `docs/raids/ulduar/freya.md` — the phase 2 measurements, the tank's new job, and why the tank
  exclusion was narrowed rather than dropped. Governed doc: run `/compact-docs-writer` before editing.
- Plan copied to
  `docs/plans/freya-phase2-tank-bomb-kite/freya-phase2-tank-bomb-kite.PLAN.md`.

## Out of scope

- The melee-interception gap on Detonating Lasher waves, still open and documented in
  `docs/raids/ulduar/freya.md` as *"Nothing intercepts a fresh pack, and that is still open."*
- `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` is 12.0 in code but the trace records beams at 5.0. Still
  unresolved and still needs a DBC check; this plan does not change the constant.
- The Snaplasher that killed Mighty at 3:53 for 28,715 in one melee swing, in the wave phase.

## Verification

The module cannot be linked here, so everything past step 2 is a hand-off.

1. **Static**: `python apps/codestyle/codestyle-cpp.py`, and every new symbol resolves in
   `UldTriggerContext.h` / `UldActionContext.h`. Per-TU syntax check against
   `acore/ac-wotlk-build:master` using `/azerothcore/build/compile_commands.json` (strip `-c`/`-o`,
   insert `-fsyntax-only`, run from the entry's `directory`; needs `MSYS_NO_PATHCONV=1` in Git Bash).
2. **Build the worldserver in Docker** and confirm the binary's mtime moves past the commit
   (`docker exec ac-worldserver ls -l --time-style=+%F_%R env/dist/bin/worldserver` — it reports UTC,
   the host is UTC+3).
3. **Confirm the build shipped** before reading any trace as evidence: `freya tank nature bomb` must
   appear at least once in the new trace.
4. **Re-pull Freya 25 hard mode** and measure the bomb phase against this trace:
   - Raid damage in the bomb phase: **68,006/s** today against 136,061 pre-bomb. Expect it to close
     most of that gap. If it does not move, the change failed regardless of what the hazard numbers say.
   - Melee uptime within 5 yd of Freya: **7-18%** today, expect a clear rise.
   - Tank distance to Freya must stop being constant across a volley, and tank Nature Bomb damage must
     fall from **55,569 over 10 hits**.
   - Bombs within 10 yd of Freya: **23 of 48** today, and the melee ring inside a blast **34%** of the
     bomb phase. Both should fall.
   - Move-order churn: **1231 escape/closer flips in 109 s** today, median dwell 108-420 ms. With the
     latch, dwell should sit near the 3000 ms latch ceiling.
   - `freya move away nature bomb` verdicts: **164 FAILED of 394** today, expect near zero.
   - Cross-hazard: **30%** of bomb destinations within 12 yd of a beam and **61%** of beam
     destinations within 11 yd of a bomb, expect both near zero.
   - Ground-hazard deaths in phase 2: **5 of 7** today.
5. **Guard against the `45d9a3e36` failure mode.** If melee damage in the bomb phase falls below
   today's 25,574/s, or the tank walks Freya more than ~15 yd per volley, the reposition is too
   aggressive — tighten the latch or the clearance, do not widen it.
6. `python tools/botobs/postmortem.py <trace>`, plus `--clump` and `--stalls`.
