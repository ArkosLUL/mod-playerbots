# Hodir: fix the anchor/dodge thrash that starves healers

Status: implemented, not yet compiled or re-traced. See "Verification" for what is still owed.

## Context

Two Hodir attempts were recorded on 2026-08-23 (Ulduar map 603, instance 3, normal, 24 members,
23 bots + 1 human). Both wiped identically: Hodir reset at 3:30 / 3:37 with him still at 67% HP.

Traces:

- `env/dist/logs/botobs/603_3_hodir_1787435701.ndjson` (latest, 23 deaths)
- `env/dist/logs/botobs/603_3_hodir_1787435362.ndjson` (prior, 30 deaths)

Read them with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`. Retention is 7
days, so keep copies if the before/after comparison still matters.

### What killed the raid

Both tanks died early (Ecoterrorist 1:19, Bulwark 2:01). With no tank, Hodir free-targeted the raid
and melee'd cloth for 23k-48k a swing; 19 more bots died between 2:06 and 3:25.

The tanks died because they were not healed:

| tank | damage taken, last 15s | healing received | ratio |
|---|---|---|---|
| Ecoterrorist (druid) | 74,445 | 9,382 | 13% |
| Bulwark (paladin) | 104,609 | 59,564 | 57% |

Ecoterrorist's only healer was Tree, a resto druid whose HoTs tick while moving. The three
cast-based healers landed nothing on him.

### Why the healers could not cast

Snapshot sampling of alive bots (`isMoving` / `castingSpellId` columns, latest trace):

| bot | role | % moving | % casting |
|---|---|---|---|
| Holylight | heal | 67% | **0%** |
| Prayer | heal | 74% | 6% |
| Tree | heal | 78% | 7% |
| Elemena | heal | 71% | 10% |

A moving bot cannot start a cast. Total raid healing was 3.1M over 3.5 minutes for 24 players taking
raid-wide Frozen Blows plus Biting Cold — roughly 15k HPS across four healers. Raid damage collapsed
the same way: Hodir went 100% to 67% in 3.5 minutes.

### Root cause: the anchor and the dodge fought each other

The move stream shows a reversal every ~320 ms:

```
0:06.154  icicle dodge   -> (1991.57, -249.28)  [ok]
0:06.580  raid position  -> (1997.54, -257.73)  [ok]
0:06.794  icicle dodge   -> (1992.44, -248.50)  [ok]
0:07.213  raid position  -> (1997.54, -257.73)  [ok]
0:07.424  icicle dodge   -> (1992.38, -247.95)  [ok]
```

Measured across all bots:

| | latest trace | prior trace |
|---|---|---|
| anchor/dodge reversals | 934 | 789 |
| share of all accepted moves | 21% | 19% |
| median gap between reversals | 321 ms | 324 ms |
| median reversal distance | 16.0 yd | — |
| total distance walked in reversals | 15,240 yd | 11,751 yd |

At ~7 yd/s that is ~2,180 bot-seconds of walking against ~5,040 bot-seconds of raid time — about
**43% of the raid's total fight time spent oscillating between two destinations**. Healers topped the
offender list (Tree 145, Prayer 114, Elemena 63).

The mechanism:

1. `HodirIcicleDodgeAction` moves the bot until it is `ULDUAR_HODIR_ICE_SHARDS_CLEAR` (6 yd) clear of
   every lethal icicle, with `ULDUAR_HODIR_DODGE_LEASH` (12 yd) as the cap.
2. `HodirIcicleDodgeTrigger` then goes false the instant the bot is 6 yd clear — but the icicle stays
   lethal for the rest of its window (`ULDUAR_HODIR_ICICLE_SPENT_MS = 3300` of a 7000 ms life, so
   ~3.7 s).
3. `HodirRaidPositionTrigger` stands down while the dodge trigger is active, and separately rejects a
   lethal icicle within 6 yd **of the anchor**. Neither guard covers an icicle sitting next to the
   bot or on the walk back.
4. So the anchor fires, walks the bot back toward the icicle, the dodge re-arms at 6 yd, and the
   cycle repeats roughly 11 times per icicle. One icicle lands every 2 s.

### Second instability: ring slots were indexed by living members

`GetHodirRingSlot` built `ringMembers` from **living** ranged members only, then used the index into
that vector to pick the angle. Every ranged death shifted every subsequent bot's index, re-seating
the whole formation. The latest trace has 11 ranged deaths, nine inside a 30-second window
(129s-157s), each one re-anchoring every surviving ranged bot. That is why the cascade accelerated
instead of stabilising.

### Third: tanks free-dodged, so Hodir wandered

Hodir's tracked position over the pull: (2016,-234) → (1977,-256) → (1976,-272) → (1984,-245) →
(1994,-253) → (2007,-225) → (2024,-246). He was dragged ~50 yd around the room because tanks ran
`hodir icicle dodge action` like everyone else instead of holding
`ULDUAR_HODIR_MAINTANK_SPOT` / `ULDUAR_HODIR_OFFTANK_SPOT`. A moving boss invalidates melee
positioning and pulls him toward the ranged formation. Bulwark reached 70 yd from Hodir while alive.

## What was changed

### 1. The anchor no longer walks a bot back under a live icicle

`src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp`

`HodirRaidPositionTrigger::IsActive` now tests every lethal icicle against the **segment** from the
bot to its anchor, rather than against the anchor alone. A new file-local `DistToSegment2d` does the
projection, clamped to the endpoints so one test covers the bot, the anchor and everything between.

This closes the loop by construction: if no lethal icicle is within 6 yd of any point on the walk,
then walking it cannot bring the bot within 6 yd of one, so the dodge cannot re-arm mid-return.

**Deviation from the original plan.** The plan also proposed raising the `_anchorReached` latch
release in `HodirRaidPositionAction::Execute` from `tolerance * 2.0f` (4 yd) to
`ULDUAR_HODIR_DODGE_LEASH` (12 yd). That change was **not** made, because with the trigger fixed it
is actively harmful: the anchor would stop pulling bots back after any dodge under 12 yd, and with an
icicle every 2 s the formation becomes an unbounded random walk with 6-12 yd steps and no restoring
force. Bots would scatter out of Starlight and off the ring within a minute. The latch release firing
after a dodge is now *correct* behaviour — the trigger already guarantees the return path is clear
before the action ever runs, so the release is what restores the formation.

### 2. Ring slots are stable across deaths

`GetHodirRingSlot` in `src/Ai/Raid/Uld/Util/UldBossHelper.cpp`

Dropped the `!member->IsAlive()` filter. Slot indices now depend only on roster composition, so a
death leaves its slot vacant instead of re-seating everyone. The `GetMapId()` filter stays — someone
who zoned out is not coming back to their slot. `total` also feeds the inner/outer split, so this
holds the ring geometry constant for the whole pull, which is the point.

### 3. Tanks do not dodge

`HodirIcicleDodgeTrigger::IsActive` in `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp`

Returns false for `botAI->IsTank(bot)`. Tanks eat the 14,000 icicle — survivable — rather than
dragging Hodir off his corner. The two-point tank shuttle in `GetHodirShuttleLeg` already gives them
the movement Biting Cold needs without leaving the spot, and it exists precisely because Hodir
follows.

### 4. Tank swap: investigated, no bug

`hodir frozen blows swap action` logged 6 `OK` against 114 `FAILED`, which looked broken. It is not.
The successes land at t = 10.7s, 21.9s, 64.2s, 72.2s, 84.8s, 92.9s — one per taunt cooldown inside
each Frozen Blows window, which is the expected rate. `HodirFrozenBlowsSwapTrigger` is stateless and
re-fires every ~110 ms, so `UldCastClassTaunt` returns false on cooldown for every attempt in
between. Those `FAILED` records are retries, not failures to swap. No change made.

### 5. Trace legibility — superseded, nothing owed here

`hodir set dps priority action` alone wrote 12,188 `FAILED` records against 287 `OK`, swamping the
trace. The change-only dedup in `NoteAction` was a single `lastAction`/`lastVerdict` pair, which the
engine defeats: it walks several nodes per tick, so the last-seen action never matches twice running
and every repeat gets written.

**Solved elsewhere.** The RaidObs v4 rework (`docs/plans/raidobs-gap-fixes/`) landed while this plan
was executing and fixes it properly: `NoteAction` now buffers into a per-pass `tick` vector that
`BeginTick` flushes, so the *ordered set* of verdicts a pass produced is the unit of change rather
than each verdict on its own, run-length-encoded across ticks. `postmortem.py` reads both the v3 and
v4 `death.acts` row shapes, so the two traces that motivated this plan stay comparable against a new
one. Nothing from this item should be re-applied on top of that.

The action itself was deliberately left alone, and that conclusion still holds. Returning `true` from
the no-op path would stop the engine descending past relevance `ACTION_RAID + 3`, stripping
`spread storm cloud` (+2), `biting cold shed` (+1) and `raid position` (+0) — a real regression to
hide a logging artefact.

One thing to watch in the first v4 trace: `HodirFrozenBlowsSwapAction` now emits an unlatched
`hodir.tankswap` note per attempt. Item 4 established those attempts are ~110 ms apart for the whole
20 s Frozen Blows window and almost all of them are taunt-on-cooldown, so expect roughly 20 `notaunt`
notes for every `taunt`. That is a lot of records for one bit of information; if it obscures the
timeline, latch it on the transition rather than dropping it.

Also checked: the `thorim.squadsassigned` note appearing at 2:53 inside a Hodir trace is benign.
`ThorimResetEncounterStateTrigger` fires when a bot still carries stale Thorim state from an earlier
pull, and `ObsValue` writes emit a note by design. That is cleanup working, not leakage.

## Files touched

- `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp`
- `src/Ai/Raid/Uld/Util/UldBossHelper.cpp`

`python apps/codestyle/codestyle-cpp.py` reports no findings in `modules/mod-playerbots` (the
failures it prints are pre-existing files under `src/server/`).

## Verification

The module cannot be compiled headless in this environment, so the build has to be handed off and the
result verified from a fresh trace.

The RaidObs v4 rework has landed alongside this, so the new trace will be schema v4 and will carry
`NoteDerived` probes the old ones do not: `hodir.anchor`, `hodir.slot` (as `index/total`),
`hodir.centre`, `hodir.shuttle`, `hodir.shelter`, `hodir.breaker`, `hodir.dpstarget`. Read them with
`postmortem.py <file> --notes hodir.` — several of the checks below are now direct reads rather than
reconstructions.

1. Rebuild and restart `ac-worldserver`.
2. Confirm the effective config first — `docker exec ac-worldserver env | grep ^AC_` — since
   gitignored `configurationOverrides/*.env` overrides `playerbots.conf`.
3. Run a Hodir pull with the same 24-bot raid.
4. On the new trace in `env/dist/logs/botobs/`:
   - **Reversals**: rerun the anchor/dodge reversal count. Target is under **5%** of accepted moves,
     down from 21%.
   - **Healer cast uptime**: `%casting` for the four healers should rise well above the current
     0-10%, and `%moving` should fall below ~40%.
   - **Tanks**: should survive past 2:00, and the `outcome` line should not be `reset` at ~3:30 with
     Hodir above 60%.
   - **Slot stability** (fix 2, now a direct read): the `total` half of `hodir.slot` must stay
     constant for the whole pull, and a given bot's `index` must never change. Any movement in either
     means the roster filter is still shifting under a death.
   - **Tanks held their corner** (fix 3): `hodir.anchor` for the two tanks should stay
     `ULDUAR_HODIR_MAINTANK_SPOT` / `ULDUAR_HODIR_OFFTANK_SPOT`, and their position track should stay
     within a shuttle leg of it.
   - `postmortem.py <file> --track <healer>` should show a healer holding a slot rather than stepping
     every sample.
5. Compare Hodir's tracked position spread — he should stay near `ULDUAR_HODIR_MAINTANK_SPOT` instead
   of drifting 50 yd.

If reversals drop but healer cast uptime does not, the next suspect is `HodirBitingColdShedAction`:
it latches `_shedding` until the aura is gone, and Biting Cold is near-permanent in this fight.
