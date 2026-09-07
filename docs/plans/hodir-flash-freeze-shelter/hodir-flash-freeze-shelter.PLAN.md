# Hodir: let each bot shelter where it stands, and use the whole Flash Freeze cast

## Context

Every bot in the raid runs to the *same* Snowpacked Icicle Target on every Flash Freeze, and it is
not the one nearest to it. `GetHodirSharedShelter` (`src/Ai/Raid/Uld/Util/UldEncounter_Hodir.cpp:85`)
ranks candidates by `shelter->GetExactDist2d(&centre)` where `centre` is `GetHodirRingCentre`, so the
whole raid converges on one drift. Both the trigger and the action call it, which is what keeps the
two from disagreeing.

Measured in `env/dist/logs/botobs/603_1_hodir_1788722807.ndjson` (schema v10, 25 raiders, kill at
5:48.164). Seven Flash Freeze casts, three drifts each. The `hodir.shelter` notes show 23/23, 23/23,
23/23, 22/22, 22/22, 21/21 and 21/21 bots on one identical guid - never once a split.

The shelters are not spread across the room. Separations per window: 9/9/13, 9/11/16, 11/16/20,
20/30/31, 15/22/22, 9/11/18, 5/16/17 yd. Ranking by distance from the bot instead wins on every axis
that was measured:

| measured at the instant the shelters appear | shared pick | own nearest |
|---|---|---|
| walk to the shelter: p50 / p90 / max | 17.7 / 25.5 / **35.0** yd | 9.8 / 16.8 / 28.8 yd |
| walk to the shelter, total over 7 freezes | 3,031 yd | **1,970 yd** |
| walk back to the anchor after: p50 / total | 19.5 yd / 821 | **9.4 yd / 612** |
| bot-windows over 20 yd out | 68 | 11 |
| bot-windows over 30 yd out | 9 | 0 |
| ranged + healers inside 15 yd of Hodir | 29% | **12%** |
| any bot beyond 30 yd from Hodir | 0% | 1% (2 of 175) |
| bot to nearest healer: p90 / max | 0 / 0 | 19.9 / **21.6** yd |

Heal range is not the constraint: heals actually landing in this pull run p50 12.3, p90 26.7, p99
36.6, max 51.5 yd, so a 21.6 yd worst case is ordinary. Nor is boss distance - the shared pick is the
thing pulling casters into his melee, because the ring centre rides a Toasty Fire.

The cost is real. Eleven bot-windows never reached the shared shelter before the freeze (two of them
the human players, so nine are ours): Malediction w3, Smartface w4, Assasin w4 and w5, Bulwark w4 and
w6. In window 4 the drifts landed 20/30/31 yd apart, **Bulwark was standing 2.6 yd from a shelter and
was sent 31.6 yd across the room**; he and Smartface ate the freeze at 3:24.722 and sat encased for
11.3 s, which also costs the five non-healers who break each block.

The mechanic never required sharing. Safe Area 62464 hold rate against the distance to the *nearest*
target, whichever one it is: 88% at 6-7 yd, 70% at 7-8, 53% at 8-9, 26% at 9-10. All three grant it.

**Second, separate problem: the first 3.9 s of every 9 s cast is dead.** `HodirGuardMultiplier`
(`src/Ai/Raid/Uld/Multiplier/UldMultipliers_Hodir.cpp:92`) zeroes every mover from cast start, but
`HodirNearSnowpackedIcicleTrigger` needs a 33174 to exist and that only spawns at cast+3.9 s. The
raid moves 20.9% of that first half against a 51.7% whole-fight baseline, then has 5.1 s to cover up
to 35 yd. Meanwhile the drift (33173) has been standing at the exact landing spot since cast+0.0 s -
measured 0.0 yd between each drift's position at cast time and the shelter it left.

Read a trace with `python tools/botobs/postmortem.py <file>` (`--notes`, `--stalls`, `--clump`,
`--bot <name>`). Schema: `docs/systems/observability.md`.

---

## Approach

Two edits. No new constants, no schema change.

### E1 - rank the shelter from the bot, and latch it (`UldEncounter_Hodir.cpp:85`)

Rename `GetHodirSharedShelter` to `GetHodirShelter` ("Shared" stops being true) and change the
ranking from `shelter->GetExactDist2d(&centre)` to `bot->GetExactDist2d(shelter)`. Drop the
`GetHodirRingCentre` call; the function is still used elsewhere and stays.

Latch the pick per bot, copying the shape `FindHodirStarlightStand` already uses at
`UldEncounter_Hodir.cpp:330`:

```cpp
thread_local std::unordered_map<ObjectGuid, ObjectGuid> latched;
```

Hold the latched guid while it is still among the live candidates; erase it and re-sweep when it is
gone. Nearest-to-self is largely self-stabilising - walking toward a shelter keeps it nearest - but a
sideways dodge can carry a bot past the midpoint of two near-equidistant ones, and the Starlight flap
is what that looks like when it goes wrong.

The single-derivation property that the current comment is protecting survives: both the trigger and
the action still call this one function with the same bot, and now get the same latched answer.

Keep `hodir.shelter` as `DescribeAssignment(guid)` - it already carries identity rather than a rule,
so there is nothing to fix there. Rewrite the block comment: the reason is no longer "the raid
converges on one drift", it is "one derivation, latched, so the trigger and the action cannot
disagree and a dodge cannot flip it".

Call sites to update: `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp:76`,
`src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp:63`, declaration `UldEncounter_Hodir.h:283`.

### E2 - stage on the drift while it is still falling

Widen the same helper's candidate set: every live `NPC_SNOWPACKED_ICICLE` (33174), plus - only while
`IsHodirFlashFreezeIncoming(botAI)` - every `NPC_HODIR_ICICLE_DRIFT` (33173) that
`IsHodirIcicleLethal` (`UldEncounter_Hodir.cpp:499`) still calls live. Rank the combined set by
distance from the bot, latch as in E1.

The handoff needs no position matching. A drift despawns and its target spawns at the same spot, so
when the latched drift leaves the candidate set the latch drops and the next sweep picks the 33174
the bot is already standing beside.

Each entry carries its own park radius, because one is a shelter and the other is a live 14,000
damage blast:

| entry | park | release |
|---|---|---|
| 33174 target | `ULDUAR_HODIR_SAFE_AREA_TOLERANCE` 6.0 | `ULDUAR_HODIR_SAFE_AREA_RELEASE` 8.0 |
| 33173 drift, still lethal | `ULDUAR_HODIR_BIG_SHARDS_CLEAR` 9.0 | 11.0 (the same 2 yd hysteresis) |

`HodirMoveSnowpackedIcicleAction::Execute` (`UldActions_Hodir.cpp:57`) picks the radius off
`GetEntry()` and otherwise stays as it is:

```cpp
float const park = target->GetEntry() == NPC_HODIR_ICICLE_DRIFT ? ULDUAR_HODIR_BIG_SHARDS_CLEAR
                                                                : ULDUAR_HODIR_SAFE_AREA_TOLERANCE;
```

`HodirNearSnowpackedIcicleTrigger::IsActive` (`UldTriggers_Hodir.cpp:68`) tests the matching release
instead of the fixed `ULDUAR_HODIR_SAFE_AREA_RELEASE`.

**No push-out code is needed.** `MoveInside` (`src/Ai/Base/Actions/MovementActions.cpp:1775`) returns
false when the bot is already within the radius, so a bot caught inside a falling drift's blast drops
through the shelter node to `hodir icicle dodge action` at `ACTION_RAID + 5`, which already collects
drifts at `ULDUAR_HODIR_BIG_SHARDS_CLEAR` and walks the bot out. Staging at 9 yd also sits outside the
dodge trigger's own `ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN` of 7.5, so
the two do not fight over a parked bot.

`HodirGuardMultiplier` needs no change - it already allows both freeze movers for the whole 9 s cast
and zeroes everything else, which is exactly what the earlier start wants.

## Files

- `src/Ai/Raid/Uld/Util/UldEncounter_Hodir.cpp` - E1 and E2, the helper
- `src/Ai/Raid/Uld/Util/UldEncounter_Hodir.h` - the rename
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp` - the per-entry release
- `src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp` - the per-entry park radius
- `docs/raids/ulduar/hodir.md` - the measurements above. Reachable from `CLAUDE.md` via
  `docs/engine/pitfalls.md`, so this edit needs its own **`/compact-docs-writer`** invocation
- `docs/plans/hodir-flash-freeze-shelter/hodir-flash-freeze-shelter.PLAN.md` - copy of this plan,
  written first

No `CMakeLists.txt` change; AzerothCore globs module sources. Run
`python apps/codestyle/codestyle-cpp.py` from `modules/mod-playerbots` before calling it done. There
is no headless build path here, so the build and the pull are the user's.

## Verification

Rebuild, restart `ac-worldserver`, confirm the effective config with
`docker exec ac-worldserver env | grep ^AC_`, then pull Hodir with the same raid.

| check | this pull | expected |
|---|---|---|
| distinct shelter guids per freeze window | 1 | 2-3 |
| shelter changes per bot within a window | 0 | 0-1 (a latch flap reads as more) |
| walk to the shelter, total over the pull | 3,031 yd | under 2,100 |
| walk back to the anchor after the freeze, total | 821 yd | under 650 |
| bot-windows over 20 yd from their shelter | 68 | under 15 |
| bots that never arrived before the freeze | 9 | 0 |
| 61969 Flash Freeze Trapped applications | 2 | 0 |
| moving, first 3.9 s of each cast | 20.9% | over 45% |
| ranged + healers inside 15 yd of Hodir, in window | 29% | under 15% |
| Hodir's own drift during shelter runs | 1.72 yd/s (1.27 baseline) | under 1.40 |
| total raid walking | 30,973 yd | under 30,000 |
| raid dps | 124,438 | no worse than 124,000 |
| kill time | 5:48.164 | no worse than 5:50 |

The last two are the guard rails. E2 puts the raid on the move 3.9 s earlier every 49 s, which is
caster uptime spent to buy arrival margin; if dps drops, read the `hodir.shelter` note churn and the
first-half movement share before touching anything else.

Keep `603_1_hodir_1788722807.ndjson` for the before/after. Retention is 7 days.

## Out of scope

- **A tank-specific shelter rule.** Hodir drifts 1.72 yd/s during shelter runs against 1.27 the rest
  of the fight, because the tank walks 39-56 yd per window and the boss follows. Nearest-to-self
  already shortens that walk a lot on its own (Bulwark's worst window goes 31.6 yd to 2.6), so
  measure the drift again before adding a rule that picks the tank's shelter by where it leaves the
  boss.
- **Bulwark's three deaths and the Frozen Blows split.** Still the open item from the previous plan,
  deferred by the user, starting at `HodirFrozenBlowsSwapAction::Execute`, `HodirTauntWouldBeSuicide`
  (`UldEncounter_Hodir.cpp:70`) and `HodirGuardMultiplier`'s taunt veto
  (`UldMultipliers_Hodir.cpp:65`).
- **The tighter stack and the Killing Spree teleport**, both already recorded in
  `docs/raids/ulduar/hodir.md` and deferred.
