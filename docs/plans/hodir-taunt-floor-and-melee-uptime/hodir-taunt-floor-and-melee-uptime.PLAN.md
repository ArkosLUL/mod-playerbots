# Hodir: stop the taunt war, get melee back on the boss

## Context

Trace `env/dist/logs/botobs/603_3_hodir_1787595615.ndjson` (schema v6, 25-man, captured against
`c872580bd`). Read it with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`.

Raid is 23 bots plus **two real players**: `Dragon` (protection paladin) and `Deathsong` (death
knight). Both carry `h:1` in the header and have **0 `act` and 0 `move` records** - no bot AI runs on
them. `Bulwark` is the only bot tank, so `Dragon` is the de facto off-tank and every handoff between
the two is a bot arguing with a human.

Outcome: wipe at 8:26.254 to the 8-minute Berserk with Hodir at **3.2%**. 33 deaths, almost all of
them in the last four seconds to 30-127k enrage melee.

**The Flash Freeze work from `f0cf37233` landed.** 61969 applied 4 times all fight, against 15 in
`603_3_hodir_1787590072`. Three of the four are `Dragon` and `Deathsong`, who run no bot AI - exactly
**one bot** (`Malediction`, at 7:26) was caught in eight and a half minutes. Nothing in this plan
touches it.

Raid dps is up about a third: Hodir was at 24.5% at 6:02 here against 42.9% at 6:02 last time. The
wipe is now a pure enrage race, and both problems below are dps the raid is leaving on the floor.

Damage taken, 9,257,480 total: Frozen Blows 6,103,127 (65.9%), boss melee 1,286,404 (13.9%), Biting
Cold 871,459 (9.4%), Freeze 507,224 (5.5%), Ice Shards 316,649 (3.4%).

---

# Part 1 - Bulwark rips Hodir off the other tank mid-Frozen-Blows

### It is not the encounter node

Ten Frozen Blows windows, derived from 63511/64545 damage: 1:04-1:23, 1:54-2:12, 2:43-3:01,
3:29-3:49, 4:19-4:37, 5:06-5:25, 5:54-6:13, 6:42-7:02, 7:32-7:51, 8:22-8:26.

Bulwark taunted 43 times. **19 land inside a window, and every one of the 19 comes from a generic
class node** - `hand of reckoning` 10, `righteous defense` 9, both at relevance 27.0.
`hodir frozen blows swap action` taunted 15 times and **every one was outside a window**. The
encounter logic in `HodirFrozenBlowsSwapTrigger` (`UldTriggers_Hodir.cpp:230`) is correct and is not
what is misbehaving.

17 of the 19 took Hodir off `Dragon`, who was holding him correctly.

### Where they come from

`TankPaladinStrategy::InitTriggers` (`TankPaladinStrategy.cpp:123-130`) wires
`"lose aggro"` -> `NextAction("hand of reckoning", ACTION_HIGH + 7)`, and that ActionNode's
alternative is `righteous defense` (`:52-60`). `LoseAggroTrigger::IsActive()` is
`!AI_VALUE2(bool, "has aggro", "current target")` (`GenericTriggers.cpp:109`), which is true for the
entire time the other tank holds Hodir. So the pair fires on the 8s taunt cooldown forever, and shows
up in the trace as casts 0.3s apart whenever Hand of Reckoning is still cooling down.

`HodirGuardMultiplier` (`UldMultipliers.cpp:917`) stands down the generic movers and both generic
target pickers. It says nothing about taunts.

### What it costs

Bulwark has 45,287 max health. 63511 hit him 26 times for 12,645-28,929 - a max roll is **63.9% of
his pool** - and two land about 2.4s apart. `Dragon` (41,582 hp) took 63511 five times all fight,
because Bulwark keeps taking it back.

Bulwark died four times. Three of them follow a class retaunt inside a Frozen Blows window:

| retaunt | own hp | Dragon hp | death | gap |
|---|---|---|---|---|
| 1:55.88 Hand of Reckoning | 61.0% | 65.2% | 2:06.03 | 10.2s |
| 3:31.32 Righteous Defense | 100% | 100% | 3:39.33 | 8.0s |
| 5:22.77 Hand of Reckoning | 90.5% | 85.2% | 5:25.38 | 2.6s |

The fourth (8:00.65) is the enrage.

### Decision taken, and what it does not cover

**Chosen: a health floor only** - no suppression of the class taunts themselves.

Stated plainly so it is not a surprise on the next trace: a floor catches **3 of the 19** measured
bad taunts (6.6%, 6.6%, 47.8%). Raising it to 65% - the size of a max-roll 63511 against Bulwark's
pool - catches 5. **None of the three retaunts that preceded a Bulwark death are reachable by any
floor**: they were cast at 61.0%, 100% and 90.5%. The bot was healthy when it took the boss back and
died seconds later to two Frozen Blows hits. Closing that needs the taunt suppression that was not
chosen this round; the floor is still worth having and is what gets built here.

---

# Part 2 - melee are never on the boss

Melee are inside 5.5 yd of Hodir for **6.4% of their alive ticks**, median distance **17.6 yd**,
moving **66.1%** of the time. Even restricted to ticks where they are targeting Hodir it is 7.4%.

175 melee-range episodes across 8 melee in 8.4 minutes: median **1.0s** each, p90 3.1s, **248
bot-seconds in total**. The gaps between them are median 14.5s, p90 45.7s, max 156.8s.

This is chronic, not a regression from the Flash Freeze round: `603_3_hodir_1787590072` measured 7.8%
in-melee and a 19.1 yd median. Two independent causes.

## 2a - the raid stands on helper ice

**39.7% of melee alive-ticks and 37.3% of ranged ticks** are spent targeting a Flash Freeze block.
Per melee bot: Mighty 32.0%, Totemist 35.9%, Ecoterrorist 37.1%, Shadow 37.8%, Justice 39.4%,
Angry 42.8%, Obliteration 43.0%, Assasin 46.5%.

Entry **32938** is the helper block (`NPC_HODIR_FLASH_FREEZE_BLOCK`, `UldBossHelper.h:83`) and takes
all of it. Entry **32926**, the trapped raider (`NPC_HODIR_FLASH_FREEZE_PLAYER`), is 2.6% - that one
is working as intended and is worth leaving.

At least one bot is on a helper block for **54.3%** of the fight. While that is true, restricted to
the 18 bot dps the encounter node actually steers:

| on ice at once | p50 | p90 | max |
|---|---|---|---|
| ranged (10 in the raid) | 6 | 9 | **10 of 10** |
| melee (8 in the raid) | 6 | 8 | **8 of 8** |
| total dps | 12 | 16 | **18 of 18** |

Healers land on blocks too - `HodirGuardMultiplier` deliberately leaves them the generic target
picker - which is why a whole-roster count reads higher (p50 15, max 25).

Flash Freeze freezes **exactly 8 helpers, every cycle**: 11 cohorts of new blocks in the trace, 8
blocks each, 88 total. Each held raid attention a median 15.3s (p90 29.0s) and sat a median
**21.5 yd** from Hodir, p75 30.0, p90 39.0 - so most of that 15.3s is travel, not damage, and cutting
the headcount on a block cuts travel roughly proportionally.

Raid composition for sizing: 1 tank, 4 healers, 8 melee, 10 ranged, plus 2 human players.

**Cause.** `IsHodirTrappedAllyBreaker` (`UldBossHelper.cpp`) caps breakers at
`ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS = 5` **per block**, and deliberately rotates the offset so that
blocks up together draw disjoint sets. One Flash Freeze freezes every helper, so all 8 blocks come up
at once: **40 slots over 18 eligible bots**, and every eligible bot qualifies for at least one. The
function's own comment describes exactly this failure - it just did not account for the blocks being
simultaneous.

The v5 trace shows the same sink (44.6% of melee ticks on a target with no `unit` record). v6's unit
records for snapshot targets are what made it legible.

## 2b - the dodge crawls melee out of melee range and never lets go

`hodir icicle dodge action` is the top melee mover: **1749 accepted moves (41.6%)**, all at
`MOVEMENT_FORCED`. `reach melee` is second with 1457 (34.6%) and is refused 2558 times, 286 of those
by a forced walk already in flight.

Inside one dodge episode a **new destination is accepted every 420 ms** (p25 314, p75 654) and lands
only **2.0 yd** from the previous one (p90 3.2). Every accepted `MoveTo` calls
`MotionMaster::Clear()`, so the bot restarts its path about 2.4 times a second and never finishes a
walk. 235 episodes, median 6 accepted moves each, max 34, **884 bot-seconds** across 8 melee.

**89% of those destinations are further from Hodir than the bot already is**, median +0.8 yd per hop,
p75 +1.4. Six hops is a net 5-10 yd of outward drift, and after an accepted dodge move it is a median
**14.9s** (p90 36.0s) before the bot is back inside 5.5 yd. 189 of 1749 never got back.

**Cause.** `FindNearestPositionClearOfHazards` (`RaidBossHelpers.cpp:308`) rings outward from
`bot->GetPositionX/Y()` in `distanceStep = 2.0f` steps and returns the first clear angle, scanning
from angle 0. The origin moves with the bot every tick, so the answer moves with it, and nothing
biases the pick toward the boss. `HodirIcicleDodgeAction::Execute`
(`UldActions_Hodir.cpp`) re-derives it every tick with no latch.

**The dodge is doing its job and the fix must keep that.** Ice Shards is 316,649 of 9,257,480 damage
taken, 3.4%. This is about how it walks, not whether it walks.

---

## Approach

Four edits. No new files, no new nodes, no strategy or context wiring.

### A. A taunt health floor for Hodir

`UldBossHelper.h`:

```cpp
// A Frozen Blows melee add-on lands 12645-28929 on a 45287 hp tank in this raid - a max roll is 64%
// of the pool - and two arrive about 2.4s apart. Below this a taunt into an open window is a death.
constexpr float ULDUAR_HODIR_TAUNT_HEALTH_FLOOR = 50.0f;
```

Two new helpers beside `IsHodirFlashFreezeIncoming`:

- `bool HodirFrozenBlowsActive(PlayerbotAI* botAI)` - boss has
  `sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_FROZEN_BLOWS, bot)`. Lift the expression out of
  `HodirFrozenBlowsSwapTrigger::IsActive` (`UldTriggers_Hodir.cpp:236`) and have the trigger call it,
  so trigger and multiplier cannot disagree about whether the window is open. The server's
  `spelldifficulty_dbc` maps `62478 -> 63512` for 25-man, so the lookup is sound.
- `bool HodirTauntWouldBeSuicide(PlayerbotAI* botAI, Player* bot)` - true when all three hold:
  Frozen Blows is up, `bot->GetHealthPct() < ULDUAR_HODIR_TAUNT_HEALTH_FLOOR`, and Hodir's current
  victim is a **living player other than this bot**. The last clause is the rescue valve: with nobody
  holding him the taunt is the save and has to survive, same reasoning as
  `MalygosMultiplier::GetValue` (`EoEMultipliers.cpp:83`).

### B. Apply the floor to both taunt paths

**Class taunts** - a branch in `HodirGuardMultiplier::GetValue` (`UldMultipliers.cpp:917`), placed
immediately after the `IsHodirEngaged` gate and before the targeting stand-down:

```cpp
if (IsHodirTauntAction(action->getName()) && HodirTauntWouldBeSuicide(botAI, bot))
    return 0.0f;
```

Name-matched, not `dynamic_cast`: the six taunts live in four class headers this file has no other
reason to include. Copy the list from `EoEMultipliers.cpp:32` - `taunt`, `hand of reckoning`,
`dark command`, `growl`, `challenging shout`, `challenging roar` - into a file-local helper in
`UldMultipliers.cpp`. Name first, then the encounter lookup: this runs for every action in the queue.

`righteous defense` is **not** in that list and has to be added, or 9 of the 19 measured taunts walk
straight through: it is the ActionNode alternative Hand of Reckoning falls back to on cooldown.

**The encounter swap** - in `HodirFrozenBlowsSwapTrigger::IsActive` (`UldTriggers_Hodir.cpp:240`),
the assist-tank branch becomes

```cpp
if (botAI->IsAssistTankOfIndex(bot, 0, true))
    return frozenBlows && !holding && bot->GetHealthPct() >= ULDUAR_HODIR_TAUNT_HEALTH_FLOOR;
```

The main-tank branch is left alone: it already requires `!frozenBlows`, so a floor keyed on the
window would be dead code there, and refusing to take the boss back once the window has closed just
drops him on a dps.

### C. Helper blocks: ranged only, one breaker per block, capped raid-wide

In `UldBossHelper.h`:

```cpp
// One Flash Freeze puts up a block per helper - measured at 8 every cycle, 11 cycles out of 11 - so
// the per-block cap multiplies: 8 blocks x 5 breakers over 18 eligible bots put every dps on ice.
// This is the ceiling across every block that is up, not per block, and one breaker goes to each.
constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_BREAKERS = 8;

// Never empty the ranged group for ice. Sized off who is actually there rather than a raid size:
// eight blocks against a 10-man's three ranged would otherwise leave nobody on the boss.
constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE = 2;
```

Split `IsHodirTrappedAllyBreaker` into the two cases the call sites already distinguish
(`UldActions_Hodir.cpp:177` and `:190`):

- **Trapped raider (32926)** keeps today's function unchanged - `ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS`
  per block, melee included. A trapped raider dies to the next freeze; that is worth melee leaving
  the boss for, and it is only 2.6% of dps time.
- **Helper block (32938)** gets a new `IsHodirHelperBlockBreaker(botAI, bot, block)`:
  1. Sweep the live blocks from **Hodir**, not from the bot -
     `hodir->GetCreatureListWithEntryInGrid(blocks, NPC_HODIR_FLASH_FREEZE_BLOCK,
     ULDUAR_HODIR_ROOM_SEARCH_RADIUS)`. A bot-centred sweep gives every bot a different block set and
     therefore a different assignment; a shared origin is what makes the answer agree across the
     raid. Keep the caller's existing `ULDUAR_HODIR_TRAPPED_ALLY_RANGE` check first so this only runs
     for bots that already have a block in reach.
  2. Candidates: alive, same map, has a `PlayerbotAI`, not heal, not tank, and
     `PlayerbotAI::IsRanged(member)`. **Fall back to any non-tank non-heal when no ranged candidate
     exists**, or a melee-only raid frees nobody.
  3. Sort both lists by guid, never by distance - distances change every tick and a distance rank
     re-shuffles the assignment between one tick and the next. This is the existing function's rule
     and it still holds.
  4. Budget = `min(ULDUAR_HODIR_HELPER_BLOCK_BREAKERS, blocks,` and, when there is more than one
     candidate, `candidates - ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE)`, floored at 1. With this raid's 10
     ranged and 8 blocks that is 8 assigned and 2 left on the boss; with a 10-man's 3 ranged it is 1.
  5. Walk the blocks in guid order and greedily give each one the first still-unassigned candidate
     starting at `block->GetGUID().GetCounter() % total`, stopping once the budget is spent. Each
     block's own guid seeds its rotation, so a later freeze draws different bots rather than always
     the lowest guids. Return whether `bot` drew `block`.

One breaker per block frees every helper every cycle - which matters, because Toasty Fire, Starlight
and Storm Power all come from them and they are re-frozen every 48s anyway - while taking 8 of the 10
ranged instead of the measured 12-18 dps. **All 8 melee stay on the boss**, and that is where the
gain is: a melee breaking ice makes a 21 yd round trip for it. Net dps in melee or ring range of
Hodir goes from about 6 to about 10.

### D. Latch the dodge destination and stop it ratcheting outward

**`RaidBossHelpers.{h,cpp}`** - add an optional `Position const* preferNear = nullptr` to the
`HazardCircle` overload of `FindNearestPositionClearOfHazards`. Behaviour is unchanged when it is
null. When it is set, collect every clear, collision-valid candidate **at the first ring distance
that yields any**, and return the one closest to `preferNear` instead of the first angle to pass.
That keeps the shortest-walk property the ring order is there for, and removes both the angle-0 bias
and the outward ratchet.

**`UldActions_Hodir.{h,cpp}`** - give `HodirIcicleDodgeAction` a latched destination:

```cpp
private:
    Position _dest;
```

`Execute` becomes:
1. Collect hazards. Empty -> `_dest = Position();` return false.
2. `_dest` set, bot further than ~1.5 yd from it, and `_dest` still clear of every current hazard ->
   re-offer it with `MoveTo(..., MOVEMENT_FORCED)` and return. `MoveTo` answers `Duplicate` and
   `Execute` returns false, which is correct: the walk in flight is forced, and
   `IsWaitingForLastMove` only yields to a strictly higher priority, so nothing below can take the
   slot. The trace already shows this working - 286 `reach melee` refusals held by a forced walk.
3. Otherwise re-derive, store in `_dest`, and issue it. Pass `preferNear` = Hodir's position for
   melee; for everyone else pass their ring anchor from `GetHodirAnchor` when it gives one, and
   nullptr otherwise. Ranged pulled toward the boss would be a different bug.

Keep both existing fallbacks (the tighter kill-radius sweep, then give up) and keep `MOVEMENT_FORCED`
with its comment - do not escalate.

## Files

- `src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}` - `ULDUAR_HODIR_TAUNT_HEALTH_FLOOR`,
  `ULDUAR_HODIR_HELPER_BLOCK_BREAKERS`, `HodirFrozenBlowsActive`, `HodirTauntWouldBeSuicide`,
  `IsHodirHelperBlockBreaker`
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp` - swap trigger uses `HodirFrozenBlowsActive`, floor
  on the assist-tank branch
- `src/Ai/Raid/Uld/UldMultipliers.cpp` - taunt-name list and the floor branch in
  `HodirGuardMultiplier`
- `src/Ai/Raid/Uld/Action/UldActions_Hodir.{h,cpp}` - dodge latch, `preferNear`, helper-block call
  site
- `src/Ai/Raid/RaidBossHelpers.{h,cpp}` - `preferNear` on `FindNearestPositionClearOfHazards`

No `CMakeLists.txt` - AzerothCore globs module sources. Run
`python apps/codestyle/codestyle-cpp.py` from `modules/mod-playerbots` before calling it done.

## Deliberately out of scope

- **Suppressing the class taunts.** Explicitly not chosen this round. It is the only thing that
  reaches the 16 full-health retaunts, including the two that preceded a Bulwark death.
- **Handing the bot main tank an off-tank.** With one bot tank and a human off-tank, no bot can ever
  take the Frozen Blows window - `IsAssistTankOfIndex` needs a `PlayerbotAI` the human does not have.
  The passive behaviour is already right; making a bot dps bear-tank a window is a bigger change.
- **Tank defensive cooldowns.** Still nothing ties Shield Wall / Divine Protection to a Frozen Blows
  window.
- **`docs/raids/ulduar.md:435-525`**, still written against the 8 yd Starlight premise and still
  naming the removed `GetHodirDruidHelper`. Needs its own `/compact-docs-writer` pass.

## As implemented

Three deviations from the approach above, all made while writing it.

**The helper-block predicate became a per-bot query.** `IsHodirHelperBlockBreaker(bot, block)` would
have been asked once per block, and with eight blocks up that is eight grid sweeps per bot per tick.
It is `Unit* GetHodirAssignedHelperBlock(PlayerbotAI*, Player*)` instead - one sweep, returning the
block this bot owns or nullptr. The candidate list is built first, from the group walk, and a bot
that is not in it returns before the sweep, so on a raid with ranged no melee ever pays for one.
`ResolveTarget` also skips the call entirely when it already has a trapped raider, which outranks a
helper anyway. The sticky "switch only when another is clearly closer" rule went with it: a
guid-ordered assignment does not move on its own, so there is nothing to damp.

**The block sweep is 45 yd from Hodir, not 100.** `ULDUAR_HODIR_TRAPPED_ALLY_RANGE` rather than
`ULDUAR_HODIR_ROOM_SEARCH_RADIUS`. The assignment has to be raid-consistent so it cannot be filtered
by distance to the bot, and an unbounded room sweep could hand somebody a block across the room.
Measured, every block sits inside p90 39 yd of him.

**The dodge latch gained a slip check.** Re-offering a latched destination answers `Duplicate`, which
re-arms nothing, so a lower-priority action that wins the slot once could drag the bot off the path
while the dodge sat there re-offering a command the movement layer keeps refusing.
`ULDUAR_HODIR_DODGE_SLIP` drops the latch when the bot is more than a yard further from its
destination than its own closest approach, which re-issues at FORCED from where it actually is.

Not compiled - the module cannot be built headless here. Verified statically: braces balance on all
eight files, every declaration matches its definition, no stale references to the replaced predicate,
and `python apps/codestyle/codestyle-cpp.py` reports nothing new (the one hit in `UldBossHelper.h` is
a double blank line that is byte-identical at HEAD).

## Verification

The module cannot be compiled headless here, so hand the build off and verify from a fresh trace.

1. Rebuild and restart `ac-worldserver`; confirm the effective config with
   `docker exec ac-worldserver env | grep ^AC_`, since `configurationOverrides/*.env` overrides
   `playerbots.conf`.
2. Pull Hodir with the same raid, by boss so the trace stays boss-scoped.
3. On the new trace:
   - **Taunts under the floor**: no `hand of reckoning` / `righteous defense` / `taunt` / `growl` /
     `dark command` cast at Hodir inside a Frozen Blows window with the caster under 50% health while
     another living player holds him. Was 3 here (6.6%, 6.6%, 47.8%). Expect the other 16 to remain -
     that is the chosen scope, not a failure.
   - **Helper ice**: dps time on entry 32938. Was 39.7% melee / 37.3% ranged. **Melee should be 0%** -
     no melee may appear on a 32938 target unless the raid has fewer than two ranged. Ranged on ice at
     once was p50 6 / max 10; expect 8 with the cap at 8. Attackers on a single block was p50 5 / max
     11 - target 1.
   - **Helper uptime, the thing the cap of 8 buys**: 8 blocks come up per cycle, 11 cycles. All 8
     should stop being targeted before the next freeze. If blocks are outliving their cycle, one
     breaker is not enough to kill one and the shape needs revisiting, not just the constant.
   - **Ranged boss uptime, the accepted cost**: 8 of 10 ranged are now on ice each cycle against 6
     before. Their share of alive time targeting Hodir was 56.6%; if it drops sharply while melee
     uptime does not rise, the trade is not paying and the cap should come down.
   - **Dodge churn**: time between consecutive accepted `hodir icicle dodge action` moves for one
     bot. Was 420 ms median with destinations 2.0 yd apart; a latched dodge should show one accepted
     move per episode plus a re-derive only when a new icicle covers the destination.
   - **Dodge direction**: share of dodge destinations further from Hodir than the bot already is. Was
     89%, median +0.8 yd. For melee it should be near an even split.
   - **Melee uptime**: share of melee alive-ticks inside 5.5 yd of Hodir (6.4%), median distance
     (17.6 yd), and the melee-range episode count and length (175 episodes, median 1.0s, 248
     bot-seconds total). This is the number the whole of Part 2 exists to move.
   - **Ice Shards, the safety regression to watch**: 316,649 taken, 3.4% of all damage, 26 hits of
     62457 and 3 of 65370. A meaningful rise means the latch is holding a stale destination.
   - **Bulwark deaths**: 4 here, 3 of them 2.6-10.2s after a class retaunt. Expect little movement
     until the taunts themselves are gated.
   - **Boss health at 6:02**: 24.5% here, 42.9% the run before. And whether he dies before the
     8-minute Berserk at all - he reached 3.2%.
4. Keep `603_3_hodir_1787595615.ndjson` for the before/after. Retention is 7 days by default.

On approval, copy this document to
`modules/mod-playerbots/docs/plans/hodir-taunt-floor-and-melee-uptime/hodir-taunt-floor-and-melee-uptime.PLAN.md`
before starting, and fold the durable findings into `docs/raids/ulduar.md` once it ships.
