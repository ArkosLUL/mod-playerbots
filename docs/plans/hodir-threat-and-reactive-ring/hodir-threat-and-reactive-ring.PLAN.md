# Hodir: threat redirect, reactive ring, and rescue capping

## Context

Trace `env/dist/logs/botobs/603_3_ulduar_1787492421.ndjson` (2026-08-23, 6:42, wipe, Hodir at 62.7%).
Read it with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`.

The previous round of fixes (commit `09cfd05e6`) landed and held: anchor/dodge reversals fell from
22.8% to 10.5% of accepted moves (290/min to 97/min), healer cast uptime rose from 0.3-8.9% to
10.1-13.8%, tanks stayed on `ULDUAR_HODIR_MAINTANK_SPOT`, and the fight went from 3:30 to 6:42.
Three different problems now dominate.

### 1. DPS out-threat the tanks

Hodir's swing damage (melee + Frozen Blows 63511) while at least one tank was alive:

| | melee | 63511 | total |
|---|---|---|---|
| tanks | 96,854 | 246,033 | 342,887 |
| ranged | 68,921 | 59,190 | **128,111** |

27% of his swings landed on ranged. Decisive evidence: Bulwark taunted at 44.1 s and 52.2 s and
Hodir stayed on Druidica (ranged) from 44.1 s to 58.8 s. A taunt sets threat *equal* to the current
top and lasts 3 s, so the boss returning means a DPS was above the tank.

Two contributing causes:

- `HodirSetDpsPriorityAction::Execute` sets `neglect threat = true` every tick for anyone targeting
  Hodir (`UldActions_Hodir.cpp:234`). That bypasses `ThreatMultiplier`
  (`src/Ai/Base/Strategy/ThreatStrategy.cpp:14`), which normally zeroes a DPS action at >=80% of tank
  threat. **Deliberately left in place** - Hodir is an enrage race and the user chose redirect over
  re-throttling.
- The Frozen Blows swap trades the boss on cooldown (12 successful taunts / 216 attempts), so neither
  tank ever builds a lead over the top DPS.

Healing was not the constraint: every raider received 120-213% of the damage they took.

### 2. The ranged ring walks bots into Hodir

`GetHodirRingCentre` (`UldBossHelper.cpp:676`) gates the centre against `ULDUAR_HODIR_MAINTANK_SPOT`,
a **constant**, never against Hodir's live position. Hodir wanders 10-25 yd off that spot, and the
centre tracks the druid helper, who walks toward Hodir.

```
134.0s  centre (1987,-257)  hodir (1981,-260)  gap  6.8 yd
145.6s  centre (1987,-257)  hodir (1996,-255)  gap  9.0 yd
153.8s  centre (1987,-257)  hodir (1982,-267)  gap 10.7 yd
```

Ring radius is 4.5-11 yd, so at a 6.8 yd gap most of the ring is inside his hitbox. Deaths 11-14 all
read `by 'hodir raid position action' [arrived]` at Hodir=5.2 / 3.4 / 6.8 / 5.4 yd.

What the ring earns, measured: Starlight aura uptime 65-87% for most of the raid (real: +50% to cast
time and all three attack timers). What it does not: median nearest-neighbour among ranged/healers is
4.5 yd, exactly `ULDUAR_HODIR_DECLUMP_RADIUS`, with 51% of samples below it.

The residual oscillation comes from the anchor being a **restoring force with a fixed target**
re-issued every tick: every dodge displaces >=6 yd, so the anchor guarantees a return trip.

### 3. The DPS roster parks on helper ice blocks

Snapshot targeting, alive bots: 17/17 on Hodir at 170.9 s, 14/14 at 211.1 s, then **0/10 on every
sample from 231 s to 392 s**. Hodir went 64.1% -> 62.7% in 142 s and stood at exactly
(1988.0, -246.3) the whole time.

The targets were batches of 5-7 guids spawning right after each Flash Freeze (69.2 / 117.4 / 166.5 /
215.0 / 263.5 / 312.0 / 360.1 s), persisting 16-46 s, held by 5-10 bots at once. Flash Freeze blocks
on the NPC helpers. Exactly one raider was frozen in that whole window (Tree at 263.5 s).

`HodirSetDpsPriorityAction::ResolveTarget` has two block branches:

| branch | entry | who's inside | breaker cap |
|---|---|---|---|
| `NPC_HODIR_FLASH_FREEZE_PLAYER` | 32926 | a raider | 5, via `IsHodirTrappedAllyBreaker` |
| `NPC_HODIR_FLASH_FREEZE_BLOCK` | 32938 | an NPC helper | **none** |

and a cross-entry sticky rule at `UldActions_Hodir.cpp:212-214`:

```cpp
if (currentTarget && priorityIndex(currentTarget) <= priorityIndex(target))
    target = currentTarget;
```

Block is priority index 1, Hodir is 2, so `1 <= 2` holds a block for as long as it lives.

All four helpers matter and none should be preferred: druid casts Starlight (62807, the ring's whole
rationale - guid #1:1465 applied it 122 times), shaman casts Storm Cloud (65123 -> Storm Power
63711/65134), mage summons Toasty Fire (62821, the only thing that clears Biting Cold), priest heals
and dispels. The bug is 20 bots on one block with no release, not that helpers get freed.

The **shelter run is working and is out of scope**: `hodir move snowpacked icicle` ->
`GetHodirSharedShelter` -> NPC 33174 gave 10 trapped raiders across 7 Flash Freezes out of ~106
raider-exposures (2/5/1/0/1/0/1, against 24 -> 10 alive).

### 4. Ice Shards clear distance is shorter than the pool

`creature_template` confirms the entries our header comment mislabels:

```
33169  Icicle                    faction 16  unit_flags 0x02008000 (NOT_SELECTABLE)
33173  Snowpacked Icicle         faction 16  unit_flags 0x02008000 (NOT_SELECTABLE)
33174  Snowpacked Icicle Target  faction 14  unit_flags 0x02000002 (NON_ATTACKABLE)
```

Ice Shards Big (65370, from 33173) has DBC radius **7**; Ice Shards Small (62457, from 33169) has
radius 4. `ULDUAR_HODIR_ICE_SHARDS_CLEAR` is 6.0 for both, so the dodge steps to a spot still inside
the big pool. Killed Power, Justice and Trueshot simultaneously at 1:13 (13,438 x2 each) and Assasin
at 2:50, all with `moving -> ... by 'hodir icicle dodge action' [arrived]`. 65370 fires ~4 s after
every Flash Freeze cast.

The comment at `UldBossHelper.h:754` already records the 7 yd and then defines the clear as 6.

## Approach

### 1. Hodir threat redirect

Lift the redirect base class out of Naxx so Uld can use it, then add a Hodir subclass.

**Move** `NaxxRedirectThreatAction` (`src/Ai/Raid/Naxx/Action/NaxxActions.h:31-59`, implementation
`src/Ai/Raid/Naxx/Action/NaxxActions_Shared.cpp:12-105`) to a new
`src/Ai/Raid/RaidRedirectThreat.{h,cpp}`, renamed `RaidRedirectThreatAction`. Mirror the existing
`src/Ai/Raid/RaidAntiFear.{h,cpp}` for file shape and CMake wiring. Nothing in `Execute`,
`isUseful`, `GetTankHolding` or `GetRedirecterIndex` is encounter-specific; only
`NaxxSpellIds::Misdirection` (35079, `src/Ai/Raid/Naxx/NaxxSpellIds.h:121`) needs to travel with it as
a constant on the new header. Repoint the four existing Naxx subclasses (Thaddius, FourHorsemen,
Anub'rekhan, Gluth) at the new base name - mechanical rename, no behaviour change.

**Add** `HodirRedirectThreatAction` in `UldActions_Hodir.{h,cpp}`:

- `GetRedirectTank()` -> `GetTankHolding(GetHodir(botAI))`, falling back to the main tank when Hodir
  is on a non-tank. That follows the Frozen Blows swap automatically and covers the recovery case.
- `GetThreatDumpTarget()` -> `GetHodir(botAI)`, so a hunter's three Misdirection shots go into the
  boss rather than whatever the rotation picks.

**Wire** in `UldStrategy.cpp` next to the other Hodir nodes (currently ACTION_RAID+6 down to
ACTION_RAID+0 at lines 345-374). Give it a new trigger `hodir redirect threat` at **ACTION_RAID + 4**,
above `hodir set dps priority` (+3) and below `hodir frozen blows swap` (+4 - place the redirect at
+4 and demote the swap to +3, or slot the redirect between them; either ordering works as long as the
redirect outranks target selection). Trigger fires when Hodir is engaged and the bot is a hunter or
rogue - `isUseful()` on the base already screens class, so the trigger only needs the engage gate.

Do **not** touch the `neglect threat` line.

### 2. Reactive ranged ring

Rewrite `HodirRaidPositionTrigger::IsActive` (`UldTriggers_Hodir.cpp:103-149`) so it fires on a broken
constraint rather than on distance from the slot. Keep every existing stand-down (trapped aura,
shelter trigger, dodge trigger, biting-cold trigger) and the segment test added in `09cfd05e6`, then
replace the final `return bot->GetExactDist2d(&anchor) > tolerance;` with:

1. **Inside Hodir's melee** - `bot->GetExactDist2d(hodir) < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP`
   (new constant, 15.0f). This is the constraint that keeps ranged out of the boss regardless of where
   the centre lands.
2. **Outside Starlight** - a druid helper exists (`GetHodirDruidHelper`) and the bot lacks aura 62807.
3. **Clumped** - another living ranged or healer within `ULDUAR_HODIR_DECLUMP_RADIUS` (4.5).
4. **Off the reservation** - `bot->GetExactDist2d(&anchor) > ULDUAR_HODIR_RETURN_LEASH` (new constant,
   suggest 20.0f), so a bot that wandered still comes home and stays in heal range.

Any one true -> return true. Otherwise false.

The trigger stays stateless (it is a stack-allocated copy per tick, per the comment at
`UldTriggers_Hodir.cpp:60-64`). Hysteresis is not needed in the trigger: firing once issues a
`MOVEMENT_COMBAT` destination and the movement layer walks it out even after the trigger goes quiet.
That is exactly what removes the per-tick re-issue driving the oscillation.

`HodirRaidPositionAction::Execute`'s `_anchorReached` latch can stay as-is - with a reactive trigger it
is only reached when a constraint fired.

**Also fix the centre gate** at `UldBossHelper.cpp:676`: test the candidate zone against Hodir's live
position, not `ULDUAR_HODIR_MAINTANK_SPOT`. Without this, every slot in the ring can sit inside melee
and constraint 1 fires for the whole raid with nowhere better to go. Keep `ULDUAR_HODIR_ZONE_ADOPT_RADIUS`
as-is; replace only the `CENTRE_MIN_TANK_GAP` half of the condition, and rename the constant to match
what it now measures.

### 3. Cap and release the rescue DPS

In `HodirSetDpsPriorityAction::ResolveTarget` (`UldActions_Hodir.cpp:150-219`):

- Gate the `NPC_HODIR_FLASH_FREEZE_BLOCK` branch with `IsHodirTrappedAllyBreaker(botAI, bot, unit)`,
  the same call the `NPC_HODIR_FLASH_FREEZE_PLAYER` branch already makes at line 172.
- Distribute across blocks in `IsHodirTrappedAllyBreaker` (`UldBossHelper.cpp:925-974`): the candidate
  list is sorted by GUID and the first `ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS` (5) are picked, so with
  several blocks up the same five bots are picked for all of them and the rest idle. Rotate the window
  by the block's own GUID counter modulo the candidate count so four helper blocks draw four disjoint
  breaker sets and all helpers come free in parallel.
- **Delete** the cross-entry sticky rule at lines 211-217. The per-entry `+ 10.0f` hysteresis inside
  the scan loop already stops two bots ping-ponging between adjacent blocks; the cross-entry rule adds
  nothing except letting a live block outrank Hodir forever.

Trapped raiders keep top priority and their existing cap - unchanged.

### 4. Ice Shards big-pool clear distance

In `UldBossHelper.h`, split the one constant into two (keep the existing names for the small icicle so
the ring `static_assert` at line 782 still holds):

```cpp
constexpr float ULDUAR_HODIR_ICE_SHARDS_RADIUS = 4.0f;      // 62457 from Icicle 33169
constexpr float ULDUAR_HODIR_ICE_SHARDS_CLEAR = 6.0f;
constexpr float ULDUAR_HODIR_BIG_SHARDS_RADIUS = 7.0f;      // 65370 from Snowpacked Icicle 33173
constexpr float ULDUAR_HODIR_BIG_SHARDS_CLEAR = 9.0f;
```

Use the big clear wherever `NPC_HODIR_ICICLE_DRIFT` is scanned: `UldTriggers_Hodir.cpp:98` and `:138`,
and `UldActions_Hodir.cpp:37`. Keep the small clear for `NPC_HODIR_ICICLE_SMALL`.

Risk to watch: `ULDUAR_HODIR_SAFE_AREA_TOLERANCE` is 6.0 and parks bots inside the 9 yd safe area, so
a 9 yd big-shards clear could fight the shelter. The dodge already stands down when the drift has a
Snowpacked Icicle Target within `ULDUAR_HODIR_SAFE_AREA_RADIUS` (`UldTriggers_Hodir.cpp:102`), which
covers it - verify that stand-down still fires before shipping.

While in the file, correct the entry comment at `UldBossHelper.h:73-79`: 33173 is "Snowpacked Icicle"
(the hostile, non-selectable mound that drops 65370) and 33174 is "Snowpacked Icicle Target" (the
non-attackable Safe Area anchor). The current text swaps their descriptions.

## Files

- `src/Ai/Raid/RaidRedirectThreat.{h,cpp}` (new)
- `src/Ai/Raid/Naxx/Action/NaxxActions.h`, `NaxxActions_Shared.cpp`, `NaxxSpellIds.h`, and the four
  `NaxxActions_<Boss>.cpp` subclasses (mechanical rename only)
- `src/Ai/Raid/Uld/Action/UldActions_Hodir.{h,cpp}`
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.{h,cpp}`
- `src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}`
- `src/Ai/Raid/Uld/UldStrategy.cpp`, `UldActionContext.h`, `UldTriggerContext.h`
- CMake source lists if the build does not glob

## Deliberately out of scope

- The `neglect threat` line - user chose redirect over re-throttling.
- The shelter run (`hodir move snowpacked icicle`) - measured working, >90% sheltered per cast.
- Thorim strategy running live through the whole Hodir fight (`thorim.squad` 4512 notes spanning
  32.1-394.0 s, `thorim.squadsassigned` 725, `thorim.arenaarrived` 130). Real, separate, documented.
- The session never resolving to a boss: `{"e":"pull","boss":"ulduar","src":"mark"}` produced
  `603_3_ulduar_*` rather than `603_3_hodir_*` and swept 30+ Winter Jormungars and the Thorim gauntlet
  into a 29 MB trace. Observability issue, separate.

## Verification

The module cannot be compiled headless in this environment. Hand the build off, then verify from a
fresh trace.

1. Rebuild and restart `ac-worldserver`. Confirm effective config with
   `docker exec ac-worldserver env | grep ^AC_` - `configurationOverrides/*.env` overrides
   `playerbots.conf`.
2. Pull Hodir with the same 24-bot raid. Pull him by boss, not by mark, so the trace is boss-scoped.
3. On the new trace in `env/dist/logs/botobs/`:
   - **Threat**: Hodir's victim from `snap` boss row field `u[7]`. Time on non-tanks while a tank is
     alive should fall well below the 21 s / 128,111 damage measured here. Confirm
     `hodir redirect threat action` emits OK verdicts on the two hunters and two rogues.
   - **Targeting**: fraction of alive bots with `u[7] == hodir` per snapshot. Must not go to 0 for
     more than one Flash Freeze recovery; the 231-392 s flatline must be gone. Boss HP should fall
     continuously rather than 1.4% per 142 s.
   - **Ring**: reversal share of accepted moves (10.5% now, target under 5%), and minimum
     ranged-to-Hodir distance - no ranged should sit inside 15 yd while at its slot.
   - **Starlight**: aura 62807 uptime per bot must not regress from 65-87%.
   - **Ice Shards**: no 65370 deaths. There were 4 here.
   - `postmortem.py <file>` summary: tanks alive past 3:20, outcome not a wipe with Hodir above 60%.
4. Keep `603_3_ulduar_1787492421.ndjson` for before/after comparison. Retention is 7 days by default.

Once the work ships, move the durable findings into
`modules/mod-playerbots/docs/raids/ulduar.md` (Hodir section) and delete the plan directory.
