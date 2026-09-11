# Thorim phase 2: restore the ring latch, dodge the whole Blizzard trail, fix the camp dodge, drop the swap cheat

Trace `env/dist/logs/botobs/603_1_thorim_1789129966.ndjson` (pull L, 11 Sep 2026 15:39), read with
`python tools/botobs/postmortem.py <file>` and scratch scripts loading `tools/botobs/obstrace.py`.
Compared against J `1789071070`, K `1789071729` and F `1789067854`. **Timestamps are ms.**

## Context

L is the first pull on `207288b27` (the ring deadlock fix). 25-man hard mode, 24 in raid, wiped 6:03.7.
Phase 2 3:03.2 onward.

"After the MT" means after **Bulwark**, the designated main tank (`thorim.p2role = maintank` from the pull),
died at **5:38.5**. Ecoterrorist is the off-tank. He died twice while holding the boss after swaps, and was
battle-rezzed in between. Cutting at the off-tank's first death (4:09.7) instead would leave no non-tank
deaths at all.

| rule: non-tank deaths in phase 2 before every tank is down | F | J | K | **L** |
|---|---|---|---|---|
| counted deaths | 22 | 7 | 12 | **6** |
| Lightning Charge hits in that window | 30 | 16 | 15 | **12** |
| boss HP at wipe | 64.0% | 43.5% | 28.3% | **25.8%** |

Still improving, but the boss pace is identical in J, K and L: 73-75% at +60 s and 34-44% at +150 s.
Thorim's Lightning Charge stacks ramp his damage. His average melee swing goes from 6.8k to 15.6k by
+140 s, Lightning Charge from 13k to 28k, and Chain Lightning peaks at 31.5k. So the tanks fall at
+155-170 s, while at this pace the kill needs about 225 s. That race is the wipe. The deaths before it
come from four defects.

## Why each bot died, 3:03.2 to 5:38.5

| time | bot | fatal blow | cause |
|---|---|---|---|
| 4:09.7 | Ecoterrorist (OT, holding) | Unbalancing Strike 29.7k at 67.6% | 2nd strike in 20 s. The 3:49.6 swap never fired (defect 4) |
| 4:33.8 | Elemena (healer) | Lightning Charge 23.0k + 3 Blizzard ticks | Camp Blizzard dodge (defect 3) |
| 4:54.0 | Ecoterrorist (OT, rezzed) | 3 swings, 41k in 2.9 s, no heal landed | Ramp |
| 5:00.3 | Druidica (ranged) | Chain Lightning 15.2k, 5.3 s after Frostbolt Volley 10.5k | Ramp, no heal between |
| 5:15.5 | Assasin (melee) | Chain Lightning 31.5k, 1.3 s after Frostbolt Volley 8.1k | Ramp |
| 5:18.7 | Totemist (melee) | Lightning Charge 21.3k | `set behind` walked him into the orb 3 cone (defect 1) |
| 5:18.7 | Angry (melee) | Lightning Charge 28.8k | `set behind`, 8.6 deg off the cone centre (defect 1) |
| 5:33.6 | Mighty (melee) | 2nd Lightning Charge 28.5k | Ping-ponged `set behind` <-> ring spot for 20 s (defect 1) |
| 5:38.5 | Bulwark (MT) | melee 11-18k per swing | Ramp: 70-91k in per 10 s vs 49-89k healed |

Frostbolt Volley 62604 is instant with InterruptFlags 0. Chain Lightning 64390 has InterruptFlags 1, so
only Thorim's own movement breaks it and it cannot be kicked. Neither has a positional answer.

The 12 cone hits break down as:

- 5: `set behind` put a melee bot in the cone
- 4: the camp Blizzard dodge put a ranged or healer bot in it
- 3: still walking to a safe target

## Defect 1 (regression in 207288b27): the ring arrival latch is never written

`ThorimRingMarkArrived` is only called from the no-move branch of `ThorimPhase2PositioningAction::Execute`
(`UldActions_Thorim.cpp:384`). `Execute` only runs after `isUseful()` -> `ThorimPhase2PositioningTrigger::IsActive`
(`UldTriggers_Thorim.cpp:184`) got `ThorimRingWantsMove == true` in the same tick, and the pure predicate
gives the same answer again. So that branch is dead, `ringArrived` stays empty, and `ThorimMeleeRingSettled`
is always false. `ThorimMovementGuardMultiplier` (`UldMultipliers_Thorim.cpp:281`) therefore never zeroes
the generic movers, and the tolerance never widens to 5 yd.

| | J | K | **L** |
|---|---|---|---|
| `thorim.ringarrived` notes in phase 2 | 204 | 252 | **0** |
| movement-guard vetoes on melee | 159 | 169 | **0** |
| accepted `set behind` moves/min, melee | 0 | 0 | **33.2** |
| accepted ring positioning moves/min | 14.2 | 8.9 | **55.3** |
| melee moving, first 150 s | 12% | 6% | **27%** |

While `set behind` walks (combat priority, ~4 s), the ring's orders back out come back `wait`. The
previous plan's in-game check ("same-tick 0 -> 1 flips back to zero") passed on this build only because
the latch never fired at all.

## Defect 2 (since ab6f30c67): Blizzard avoidance tests the emitter, not the damage

`boss_thorim.cpp:867`: Sif summons `NPC_SIF_BLIZZARD` 32879 at (2108.7, -280.04). It lives 30 s and walks
eight waypoints. Its aura 62577/62603 drops a Blizzard zone every 2 s: 62576 (10-man) / 62602 (25-man),
a 10 s persistent area aura with an 8 yd radius. So the damage is a **trail**: up to 6 zones alive at
once, a median 25.9 yd (p90 49.1) behind the bunny. Hits land at most 9.9-10.8 yd from a zone centre.

`ThorimBlizzardSpots` (`UldEncounter_Thorim.cpp:560`) collects only the bunny. The melee slide
(`BlizzardRingOffset`, `RingBearingClearOfBlizzard`) and the deadband escape (`RingSpotBeatsTheDeadband`)
never see the zones that do the damage:

| melee Blizzard hits | J | K | L |
|---|---|---|---|
| bunny within 11 yd (visible to the slide) | 8 | 6 | 4 |
| trail only (invisible) | 31 | 48 | 16 |

With the zones counted, the r=8 ring is never fully blocked: worst snapshot 21% (J), 39% (K) and
33% (L) of bearings clear, median 82-93%.

## Defect 3: the camp's Blizzard dodge is the generic 30 yd flee

`ThorimSifBlizzardTrigger` fires on `TooCloseToCreature(NPC_SIF_BLIZZARD, 15 yd)`.
`ThorimSifBlizzardAction` is a bare `MoveAwayFromCreatureAction`, which takes the farthest safe point of
eight rays out to 30 yd, with no cone test.

- **Fired needlessly.** L: 22 orders, 19 of them issued more than 9.8 yd from the bunny (K 21/27,
  J 11/11). Every home spot is 12.9-25.9 yd off the bunny track, which is identical in every pull.
- **Always the full run.** Median order length 30.0 yd.
- **The shelter exemption is too narrow.** `ThorimShelterWalkPending` only covers a bot more than 3 yd
  from its shelter. Prayer reached the orb 2 shelter at 3:31.0, a zone came within 8.7 yd, and the dodge
  ran Prayer 30 yd east into the cone. Prayer's orders back to the shelter were refused `wait` until the
  charge at 3:33.8. Hellflame and Malediction took the same cone the same way.
- **Elemena** was safe at home (2131.5, -245.0), 12.9 yd clearance. At 4:19.6 the bunny came within
  16-18 yd and the dodge fired anyway: two 30 yd runs east along the bunny's own path to the arena edge,
  three Blizzard ticks, then the orb 1 cone. Elemena was moving 55% of phase 2 (K 22%), and their healing
  fell from 131k to 15k per 30 s before they died.

## Defect 4: the Unbalancing Strike swap races the raid-cheat strip

`ThorimUnbalancingStrikeAction` (`UldActions_Thorim.cpp:28-41`, from the original Thorim strategy
`e2b5ab766` #1305 and never removed) strips the debuff from the struck tank whenever
`BotCheatMask::raid` is set. That is live here: `AiPlayerbot.BotCheats = "food,taxi,raid"`, with no `AC_`
override. The strip lands 2-230 ms after the strike. `ThorimUnbalancingStrikeSwapTrigger`
(`UldTriggers_Thorim.cpp:243`) only fires while `activeTank->HasAura(SPELL_UNBALANCING_STRIKE)`, so the
partner has to tick inside that window.

| strikes followed by a swap | F 3/6 | J 2/8 | K 3/7 | L 2/5 |
|---|---|---|---|---|

In F, the three strikes whose debuff stayed up the full 15 s all got a swap within 37-199 ms: the trigger
works when nothing strips the aura. At L 3:49.6 the strike on Ecoterrorist was stripped after 30 ms, and
Bulwark was alive and never swapped. Bulwark's generic taunts came back `IMPOSSIBLE`. The next strike,
20 s later, killed Ecoterrorist.

## Changes

### 1. Write ring arrival where the no-move answer is seen

- `ThorimPhase2PositioningTrigger::IsActive` (`UldTriggers_Thorim.cpp:184`): when
  `ThorimRingWantsMove` is false, call `ThorimRingMarkArrived(bot)` and return false. Otherwise return
  true. Inserting can only widen the tolerance, so a second read in the same tick still says no. This is
  safe where the old erase-inside-the-predicate was not.
- `ThorimPhase2PositioningAction::Execute` keeps `ThorimRingClearArrived` right before `MoveTo`: the action
  owns leaving. Keep its no-move branch, which is reachable only if the spot changed between the two calls.
  Fix its comment and the header comment on `ThorimRingMarkArrived`/`ThorimRingClearArrived`
  (`UldEncounter_Thorim.h:643-647`).

### 2. Feed every Blizzard test the zones as well as the bunny

- `UldEncounter_Thorim.h`: add `SPELL_SIF_BLIZZARD_ZONE_10 = 62576` and `SPELL_SIF_BLIZZARD_ZONE_25 = 62602`
  next to `NPC_SIF_BLIZZARD`.
- `ThorimBlizzardSpots`: after the bunny loop, append
  `GetDynamicObjectPositions(bot, ULDUAR_THORIM_BLIZZARD_SCAN_RANGE, spellId)` for both ids.
  `src/Util/EncounterHelpers.cpp` already has that helper.
- The cache is shared for 500 ms by every bot, and it is filled from whichever bot scans first. The trail
  spans x 2104-2165, y -280 to -232, so raise `ULDUAR_THORIM_BLIZZARD_SCAN_RANGE` from 50 to 70.
- Refresh the comments that say "bunny" where they now mean zone. No caller changes: the melee slide,
  its hold and the deadband escape all read `ThorimBlizzardSpots` already.

### 3. Camp dodge: step to the nearest clear, cone-safe point and never walk into a covered spot

- `src/Util/EncounterHelpers.{h,cpp}`: give the `HazardCircle` overload of
  `FindNearestPositionClearOfHazards` an optional trailing `std::function<bool(float, float)> const& accept`.
  Test it after the collision correction. Existing callers (`UldActions_Algalon.cpp`, `UldActions_Freya.cpp`)
  are unchanged.
- `ThorimSifBlizzardTrigger::IsActive`, phase 2 Ranged role only: replace the `ThorimShelterWalkPending`
  exemption and the 15 yd creature test. The new test: within `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE`
  (11 yd) of any `ThorimBlizzardSpots` point, and the bot's own bearing is **not** in the lit cone
  (`InLightningChargeConeRanged`). A bot in the cone leaves the Blizzard to its shelter walk: a charge is
  10-36k, a tick 4-5k. Other roles and phase 1 keep the existing path.
- `ThorimSifBlizzardAction`: override `Execute`. For a phase 2 Ranged bot:
  1. Resolve its spot G with `TryGetThorimPhase2Spot` (home or shelter).
  2. Call `FindNearestPositionClearOfHazards` with the zones at 11 yd, max radius 15 yd, `preferNear = &G`.
     `accept` rejects points in the lit cone or more than 32 yd from the boss (the camp table's own bound).
  3. `MoveTo` at `MOVEMENT_COMBAT`. If nothing is found, return false and stand.

  Everything else falls through to `MoveAwayFromCreatureAction::Execute`. New constants go in
  `UldEncounter_Thorim.h`: the 15 yd escape radius and the 32 yd boss range.
- `ThorimPhase2PositioningTrigger::IsActive`, Ranged branch: hold while G is within 11 yd of any
  `ThorimBlizzardSpots` point, unless the bot is in the lit cone. Without this the return walk re-enters
  the zone and the dodge ping-pongs.
- Delete `ThorimShelterWalkPending` (`UldEncounter_Thorim.cpp:2222`, header `:657`); its only caller is gone.

### 4. Remove the raid-cheat strip, making the Thorim tank swap cheat-free

Delete the strip and everything that exists only to drive it:

- `ThorimUnbalancingStrikeAction`: class in `UldActions_Thorim.h:14`, `isUseful`/`Execute` in
  `UldActions_Thorim.cpp:28-41`.
- `ThorimUnbalancingStrikeTrigger`: class in `UldTriggers_Thorim.h:11`, `IsActive` in
  `UldTriggers_Thorim.cpp:21`. Its only user is that action.
- Wiring: `UldActionContext.h:89` and `:265`, `UldTriggerContext.h:89` and `:278`, and the trigger node
  at `UldStrategy.cpp:477-479`.

`ThorimUnbalancingStrikeSwapTrigger` and its action stay unchanged. With the debuff up for its real
15 s, the swap check sees it. Its "not while this bot still carries it" guard still clears before the
next strike, since strikes land every 20 s against a 15 s debuff. Raid cheat stays on for the other
strategies that use it (Yogg-Saron, ICC, SSC, BT, BWL, RS).

### 5. Docs (invoke `/compact-docs-writer` once, up front, for both)

- `docs/engine/raid-mechanics-lessons.md:84-90`: the bullet says "let the caller that issues the move own
  the latch". That is exactly the design that broke. The action only runs when there is a move, so it
  can own leaving but never arriving. Arrival is written where the decline is seen, and only as a write
  that cannot flip a second read in the same tick. Add that a check for the bug's signature dropping to
  zero also passes when the latch stops firing, so check the latch fires too.
- `docs/raids/ulduar/thorim.md:157`: the Blizzard line says "respawned every ~15s" and nothing about the
  trail. Replace it with the trail facts from defect 2, and note that avoidance has to test the zones.

First implementation step, before any code: save this plan as
`docs/plans/thorim-phase2-latch-and-blizzard-trail/thorim-phase2-latch-and-blizzard-trail.PLAN.md`.
Code comments follow `use-conversational-language` (fresh invocation).

## Deliberately not doing

- **The damage race.** Nothing positional answers the ramp, Frostbolt Volley or Chain Lightning. Every fix
  above buys uptime, and the pace decides the kill.
- **Tank anchor move.** Still deferred. The numbers are in
  `docs/plans/thorim-phase2-ring-deadlock/thorim-phase2-ring-deadlock.PLAN.md`: the tank spot is 5.1 yd
  off the track, and the tanks took 157k Blizzard in L.
- **Orb 2 shelter rows.** (2,1) sits 3.5 yd from the bunny's spawn point, where each pass drops its first
  zone. The earlier re-solve showed 11 yd of track clearance is infeasible for orb 2. Change 3's escape
  and hold handle a covered shelter at runtime.
- **Sif's Frostbolt 62601.** It is kickable (InterruptFlags 15), but no counted death in L.
- **Heals on a freshly swapped-in tank.** Ecoterrorist took no heal in the 2.9 s he died over. That is
  the generic heal AI, not this strategy.
- **The uncapped shelter run.** 3 cone hits were still walking, down from 6, so it stays below the
  revisit threshold.

## Verification

Static, from `modules/mod-playerbots`:

- `python apps/codestyle/codestyle-cpp.py`. Its three standing failures are pre-existing.
- Lines <= 120 columns, LF, ASCII. Check with a Python byte scan.
- Per-TU `-fsyntax-only` in `acore/ac-wotlk-build:master`, from `/azerothcore/build/compile_commands.json`
  (drop `-c`/`-o`). Cover `UldEncounter_Thorim.cpp`, `UldTriggers_Thorim.cpp`, `UldActions_Thorim.cpp`,
  `UldMultipliers_Thorim.cpp`, `UldStrategy.cpp`, `EncounterHelpers.cpp`, `UldActions_Algalon.cpp`,
  `UldActions_Freya.cpp`, `BuildSharedActionContexts.cpp` and `BuildSharedTriggerContexts.cpp` (the last two
  include the Ulduar contexts).
  - Mount the live tree over the baked copy (`-v "$(pwd -W)":/azerothcore/modules/mod-playerbots`) and
    grep a new symbol inside first.
  - Use `MSYS_NO_PATHCONV=1`, and pipe the script (`docker run --rm -i ... bash -s < script`), not a file
    mount.
  - About 2.5 min per TU. Background it and don't edit the tree meanwhile.

In game, one 25-man hard mode pull, then re-read the trace. These are positive checks, so a latch that
never fires fails them:

1. `thorim.ringarrived` notes present (J 204, K 252, L 0) and movement-guard vetoes on melee present
   (J 159, K 169, L 0). No accepted `set behind` for melee in phase 2 (L 33/min).
2. No same-tick `ringarrived` 0 -> 1 flips (the deadlock stays fixed). No melee bot frozen over ~30 s.
3. Most melee Blizzard hits no longer come from the trail (L 16/20, K 48/54). Melee Blizzard hits fall
   below J/K (39/54).
4. Camp dodge orders only within 11 yd of a zone and at most 15 yd long. No camp cone hit whose last
   accepted mover is `thorim sif blizzard action` (L 4).
5. No `thorim unbalancing strike action` in the trace, and the debuff on a bot tank runs its full 15 s.
   Every strike with a live, nearby partner is followed by a swap within ~1 s (L 2/5).
6. No regression: `thorim.rangedslot` one per bot, `thorim.slot`/`squad`/`p2role` unchanged, phase 1
   deaths 0-1, hard mode still reached.
7. Counted deaths at or below 6, and boss HP at wipe below 25.8%.
