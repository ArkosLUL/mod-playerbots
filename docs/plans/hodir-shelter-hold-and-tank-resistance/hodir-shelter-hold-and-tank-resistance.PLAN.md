# Hodir: hold the Flash Freeze shelter, keep frost resistance on the tanks

## Context

Trace `env/dist/logs/botobs/603_3_hodir_1787590072.ndjson` (25-man, 24 bots, wipe at 6:02, 30 deaths).
Read it with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`. It contains
`4482581f2`, the tank-target / movement-economy / paladin-aura round.

Two problems, both measured from this trace.

1. **Flash Freeze catches bots that already reached the shelter.** The node that walks them to safety
   releases them the instant they arrive and the ring anchor walks them straight back out.
2. **Both tanks die and the raid follows.** Every tank death blow landed while Frost Resistance Aura
   was off them, because the paladin carrying it is a melee DPS who ends up 43-60 yd away.

Boss went 100% -> 42.9% in six minutes and flat after 5:31 with the raid dead. Damage taken raid-wide
is **3,929,499 of ~5.5M from Frozen Blows alone (71%)**; Biting Cold, Freeze and Ice Shards together
are 929k.

---

# Part 1 - the Flash Freeze shelter

The named incident is **1:47.08**, which encased four: Mighty (melee warrior), Stormweaver, Agony,
Smartface (all ranged). It is the worst of seven freezes. Not a regression - the previous trace
`603_3_hodir_1787582677.ndjson` caught 21 across its seven against 15 here.

### The window and the geometry

Flash Freeze 61968 has a **9 s cast** (the `cast` record carries `ct 9000`; measured cast-to-land
9.03 s). The drift icicle lands and leaves its Snowpacked Icicle Target a consistent **3.8 s** into
that cast, so the shelter exists for the last **6.3 s** and then lives ~5.8 s past the freeze.

`GetHodirSharedShelter` (`UldBossHelper.cpp:589`) picks the Snowpacked Icicle nearest the fixed
`ULDUAR_HODIR_RAID_ANCHOR` (1986.56, -257.11). Centres fitted from the 125 `MoveInside` destinations
in this trace (circle fit returns r = 6.00 +/- 0.01, confirming `MoveNear` offsets by exactly the
tolerance):

| land | shelter centre | from fixed anchor | run p50/p90/max | encased |
|---|---|---|---|---|
| 0:58 | (1999.9, -248.1) | 16.1 | 13.1 / 30.0 / 36.9 | 2 |
| **1:47** | **(2011.7, -255.3)** | **25.2** | **30.2 / 36.8 / 42.5** | **4** |
| 2:36 | (1988.8, -252.9) | 4.8 | 13.6 / 31.0 / 47.9 | 2 |
| 3:24 | (1996.9, -261.7) | 11.3 | 19.4 / 28.2 / 41.1 | 2 |
| 4:13 | (1991.5, -254.6) | 5.5 | 13.3 / 28.5 / 81.2 | 2 |
| 5:02 | (1990.5, -262.2) | 6.4 | 15.5 / 41.1 / 45.7 | 2 |
| 5:50 | (2010.0, -249.4) | 24.7 | 24.4 / 45.7 / 62.0 | 1 |

At ~7 yd/s a 30 yd median run is 4.3 s against a 6.3 s budget. It fits, but only if nothing
interrupts.

Safe Area 62464 is radius index 40 -> **9 yd**, so `ULDUAR_HODIR_SAFE_AREA_RADIUS = 9.0f` is correct
and needs no change.

### 1a. The shelter releases the bot the moment it arrives

- `HodirMoveSnowpackedIcicleAction::Execute` (`UldActions_Hodir.cpp:58`) calls
  `MoveInside(..., ULDUAR_HODIR_SAFE_AREA_TOLERANCE, MOVEMENT_COMBAT)`. `MoveInside` -> `MoveNear`
  (`MovementActions.cpp:1735`, `:104`) parks the bot **exactly** 6.0 yd from the shelter centre.
- `HodirNearSnowpackedIcicleTrigger::IsActive` (`UldTriggers_Hodir.cpp:68`) returns
  `dist > ULDUAR_HODIR_SAFE_AREA_TOLERANCE`. Arrival distance == release distance. **Zero hysteresis.**
- `HodirRaidPositionTrigger::IsActive` (`:133-135`) stands down only while that shelter trigger is
  active. The tick the bot touches the boundary the suppression lifts, the anchor fires, and its
  `MoveTo(..., MOVEMENT_COMBAT)` is accepted - equal priority, and the shelter run's own
  `lastdelayTime` has expired by then - so `MotionMaster::Clear()` throws the arrival away.

Three bots, one freeze:

| bot | reached | at | anchor move | sent to | at land | out by |
|---|---|---|---|---|---|---|
| Agony | (2005.30, -256.10) | 103.47 | 103.53 | (1984.6, -247.6) | (1989.39, -249.76) | 22 yd |
| Stormweaver | (2005.72, -254.44) | 104.27 | 104.25 | (1987.3, -248.7) | (1993.79, -250.72) | 18 yd |
| Smartface | (2008.78, -257.64) | 103.47 | 103.84 | (1979.3, -247.4) | (1994.05, -253.91) | 18 yd |

Stormweaver's reached position is its destination to the centimetre. It stood in the safe area with
1.8 s to spare and spent that 1.8 s walking out.

Raid-wide over all seven freezes: for **68 of 125** bot-freeze pairs the last accepted move before the
freeze landed came from something other than the shelter action - `hodir raid position action` 36,
`reach melee` 21, `reach spell` 4, `hodir biting cold shed` 4, `hodir spread storm cloud` 2.

### 1b. The shelter action reports failure while it is working

`MoveTo` returns `Duplicate` for an unchanged destination inside `maxWaitForMove`
(`MovementActions.cpp:977`) and `Waiting` for an in-flight move of equal-or-higher priority (`:989`).
Both make `Execute` return false, so from the second tick on `hodir move snowpacked icicle` is a
FAILED action and `Engine::DoNextAction` descends past it (`Engine.cpp:268-274`). Its `ACTION_RAID + 6`
buys exactly one tick, and everything below gets a turn on every tick of the run.

Do **not** fix this by returning true - that silences the bot's casting for the whole window, seven
times a pull. The descent is correct; the movers below it must be off.

### 1c. Gap-closers drag melee back

Mighty is the fourth catch. Shelter move accepted at 100.87 to (2009.7, -249.6); he covered 12 yd;
then **`charge` executed OK at 103.31** and put him back on Hodir at (1979.26, -269.05). Every shelter
retry after that returned `wait`.

`HodirGuardMultiplier` (`UldMultipliers.cpp:938`) returns 1.0f early for melee and exempts
`AttackAction` / `ReachTargetAction` for everyone else, so `reach melee`, `reach spell` and the
gap-closers are unguarded in the one window where they are lethal.

### 1d. The shelter is chosen for a point the raid is not on

`GetHodirSharedShelter` measures from the fixed anchor, but the ranged ring now rides a Toasty Fire:
`hodir.centre` notes sit a median **9.5 yd** from that anchor, p90 17.6, max 23.2, over 14 distinct
centres. Two or three Snowpacked Icicles exist at every freeze, so the pick has real alternatives.

---

# Part 2 - the tanks

### What kills them

Frozen Blows is two spells, both confirmed in `spell.reference.csv`:

- **63511** - `Effect_1 = 2` (school damage), base points **39999**, no radius: the single-target
  melee add-on. Observed median **23,691**, p90 30,400, max 35,625 *after* resists.
- **64545** - base points **3999**, radius index 28 = **50000 yd**, i.e. raid-wide: ~2,700 per tick on
  every player, ~10 ticks per window.

Per window that is ~669,000 raid-wide, six full windows in the pull. Max health: Ecoterrorist (feral
druid) **34,135**, Bulwark (prot paladin) **44,697**. A median 63511 is ~70% of Ecoterrorist's pool and
two land 2.8 s apart.

### The aura is off the tank at every death blow

`GetHodirResistancePaladin` picked Justice (retribution) and the aura went up on one cast at 0:00.27
with 87% mean raid coverage. That half works. But Frost Resistance Aura 48945 is radius index 23 =
**40 yd**, and Justice is a melee DPS who runs the icicle dodge, the shelter run and the shuttle.

Every 63511 hit, with the aura state and the distance to Justice at that instant:

| time | victim | landed | resisted | aura | dist to paladin |
|---|---|---|---|---|---|
| 2:00.09 | Ecoterrorist | 21,624 | 5,406 | YES | 27.2 |
| **2:02.88** | **Ecoterrorist** | **21,624** | 5,406 | **NO** | **46.4** | <- death
| 2:49.63 | Ecoterrorist | 23,652 | 10,136 | **NO** | 46.6 |
| 3:36.41 | Bulwark | 21,898 | 9,384 | **NO** | 51.8 |
| 3:38.96 | Bulwark | 24,522 | 3,128 | **NO** | 44.1 |
| **3:41.30** | **Bulwark** | **20,021** | 6,256 | **NO** | **50.5** | <- death
| 4:24.30 | Bulwark | 29,928 | **0** | **NO** | 60.3 |
| **4:26.73** | **Bulwark** | **25,800** | **0** | **NO** | **59.0** | <- death

Every tank killing blow: aura **off**, paladin **44-60 yd** away. Hits taken with the aura on resisted
9,384-15,833. Aura uptime by role puts the two tanks **last of everyone in combat** - Bulwark 73.3%,
Ecoterrorist 77.2%, against 87-93% for the ranged and melee.

Justice is within 40 yd of a living tank only **83.3%** of the time. What moves him out: `hodir icicle
dodge action` 32 accepted moves, `reach melee` 30, `hodir biting cold shed` 12.

Coverage if a different paladin carried it, measured over the same snapshots:

| carrier | spec | raid coverage @40 yd | tank coverage @40 yd |
|---|---|---|---|
| Justice | retribution | 90.6% | 82.0% |
| **Bulwark** | **protection (a tank)** | **80.3%** | **100.0%** |
| Holylight | holy | 91.6% | 67.4% |

A resist point on the tank is worth ~6,000 per swing on a hit that kills; on the raid it is ~700 on a
tick four healers already cover.

### The swap works, then runs out of tank

`HodirFrozenBlowsSwapTrigger` (`UldTriggers_Hodir.cpp:227`) checks
`GetSpellIdForDifficulty(SPELL_HODIR_FROZEN_BLOWS, bot)`. The client DBC has no SpellDifficulty row
for 62478, but the **server table does** - `acore_world.spelldifficulty_dbc` maps `62478 -> 63512` for
25-man, and 63512 is the aura Hodir actually carries. The check is correct. (Same for
`65123 -> 65133` Storm Cloud and `63711 -> 65134` Storm Power.)

Hold share of Hodir per Frozen Blows window:

| window | holders |
|---|---|
| w1 1:04-1:23 | Ecoterrorist 67%, Bulwark 33% |
| w2 1:54-2:12 | Bulwark 54%, Ecoterrorist 46% |
| w3 2:43-3:01 | Bulwark 83%, Ecoterrorist 17% |
| w4 3:31-3:49 | Bulwark 91% (Ecoterrorist dead since 2:56) |
| w5 4:20-4:38 | Bulwark 38%, then loose in the raid |

The assist tank does take the early windows. It degrades because he dies, not because the logic is
wrong. Fix the aura and the swap has something to swap to; leave the swap logic alone this round.

### Cooldowns

Across the whole fight: Ecoterrorist used barkskin x2, frenzied regeneration x1, survival instincts
x1. Bulwark used holy shield x5, divine shield x1. Nothing is tied to a Frozen Blows window. Noted,
not fixed here - see out of scope.

---

## Approach

Five edits. No new files, no new nodes, no strategy or context wiring.

### A. Gate the freeze window on the cast

Add to `UldBossHelper.{h,cpp}` beside `IsHodirEngaged`:

```cpp
// True while Hodir is casting Flash Freeze. 61968 is a 9 s cast and the trace measures cast-to-land
// at 9.03 s, so this is the whole window and nothing but it - it opens 3.8 s before the drift lands
// and closes exactly when the freeze resolves.
bool IsHodirFlashFreezeIncoming(PlayerbotAI* botAI);
```

`GetHodir(botAI)` plus `FindCurrentSpellBySpellID(SPELL_FLASH_FREEZE)` on the boss.

Leave `HodirNearSnowpackedIcicleTrigger` keyed on the shelter existing. Its comment is right - the
drift detonates for 14000 in 7 yd on the way down, so the raid must not run at it early. The cast
gates *suppression*; the shelter still gates *the run*.

### B. Suppress every other mover for the whole window

Extend `HodirGuardMultiplier::GetValue` (`UldMultipliers.cpp:914`) with a window branch placed
**after** the targeting stand-down and **before** the melee early-out at `:938`:

- `!IsHodirFlashFreezeIncoming(botAI)` -> fall through to today's logic unchanged.
- Inside the window, for **every** role including melee: `0.0f` for any
  `dynamic_cast<MovementAction*>(action)` whose name is not `"hodir move snowpacked icicle"` or
  `"hodir icicle dodge action"`, and `0.0f` for any `dynamic_cast<CastReachTargetSpellAction*>(action)`.

`CastReachTargetSpellAction` (`ReachTargetActions.h:31`) is exactly Charge, Intercept and both Feral
Charges - `MimironChargeGuardMultiplier` (`UldMultipliers.cpp:1017`) already uses this cast for the
same purpose and records that there are no other subclasses. Reuse it rather than naming spells.

The `AttackAction` / `ReachTargetAction` exemption at `:945` must **not** apply inside the window:
`reach melee` and `reach spell` are 25 of the 68 thefts. Keep `AttackAction` itself exempt - it only
sets a target and is not what moves the bot.

This makes the descent in 1b harmless: the engine still walks past the shelter action every tick, but
everything it could descend into is zeroed.

Accepted cost: `hodir biting cold shed` is off for the 9 s window, so a parked bot gains roughly three
Biting Cold stacks. The run itself sheds while it happens, so it is close to neutral, and it is worth
far less than a death.

### C. Give the shelter trigger hysteresis

Belt and braces for when the window predicate is unavailable (boss out of grid). In `UldBossHelper.h`:

```cpp
// The run parks at TOLERANCE and only releases at RELEASE, so arriving does not hand the bot straight
// back to the anchor. 6 -> 8 keeps both inside the 9 yd Safe Area (62464, radius index 40).
constexpr float ULDUAR_HODIR_SAFE_AREA_RELEASE = 8.0f;
static_assert(ULDUAR_HODIR_SAFE_AREA_RELEASE < ULDUAR_HODIR_SAFE_AREA_RADIUS,
              "the release ring has to stay inside what Safe Area actually covers");
static_assert(ULDUAR_HODIR_SAFE_AREA_TOLERANCE < ULDUAR_HODIR_SAFE_AREA_RELEASE,
              "the park ring has to sit inside the release ring or arriving releases the bot");
```

`HodirNearSnowpackedIcicleTrigger::IsActive` tests `> ULDUAR_HODIR_SAFE_AREA_RELEASE`; the action keeps
parking at `ULDUAR_HODIR_SAFE_AREA_TOLERANCE`.

Leave the `NPC_SNOWPACKED_ICICLE` lookup in `HodirIcicleDodgeTrigger` (`:118`) on
`ULDUAR_HODIR_SAFE_AREA_RADIUS` - that asks "is there a shelter attached to this drift", a different
question.

### D. Pick the shelter nearest where the raid stands

In `GetHodirSharedShelter` (`UldBossHelper.cpp:589`), replace `ULDUAR_HODIR_RAID_ANCHOR` with
`GetHodirRingCentre(botAI, bot)` - already the shared per-tick derivation both the position trigger
and action use, so the "two derivations would disagree and oscillate" guarantee in the existing
comment still holds. It falls back to `ULDUAR_HODIR_RAID_ANCHOR` when no fire is adopted, so the 96 of
261 centre samples with no fire are unchanged. Update the comment to say centre, not anchor.

### E. Put frost resistance where the killing blows land

In `GetHodirResistancePaladin` (`UldBossHelper.cpp`), change the preference order to **paladin tank
first, then a non-tank non-heal paladin, then any paladin**. `PlayerbotAI::IsTank(member)` already
distinguishes them and the existing group walk needs no other change. Record the measurement in the
comment: tank coverage 100% against 82%, raid coverage 80.3% against 90.6%, and every tank death blow
in this trace landed with the aura off and the ret paladin 44-60 yd away.

`HodirPaladinAuraMultiplier` needs no change - it already holds the aura slot for whichever paladin
the helper returns, so a paladin tank keeps Frost Resistance instead of Devotion for the encounter.
Bulwark's damage split justifies it: 385,246 frost against 248,798 physical.

No positional leash on the carrier. A raid with no paladin tank falls back to ret and keeps today's
82% tank coverage, which is what it has now - a leash would have to fight the icicle dodge for the
carrier's feet, and 62457 lands a median 11,825 on a 26,932 pool.

## Files

- `src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}` - `IsHodirFlashFreezeIncoming`,
  `ULDUAR_HODIR_SAFE_AREA_RELEASE`, the `GetHodirSharedShelter` centre change, the
  `GetHodirResistancePaladin` preference change
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp` - release radius in
  `HodirNearSnowpackedIcicleTrigger`
- `src/Ai/Raid/Uld/UldMultipliers.cpp` - the freeze-window branch in `HodirGuardMultiplier`

No `CMakeLists.txt` - AzerothCore globs module sources.

## As implemented

Two deviations from the text above, both found while writing it:

- `IsHodirFlashFreezeIncoming` uses `Unit::FindCurrentSpellBySpellId` (`Unit.h:1580`), which searches
  every cast slot, rather than `GetCurrentSpell(CURRENT_GENERIC_SPELL)`. Which slot a scripted boss
  cast occupies is the script's business, and guessing wrong opens the window on nothing.
  `UldTriggers_Mimiron.cpp:31` already does it this way.
- `GetHodirSharedShelter` returns early when the grid sweep finds no Snowpacked Icicle, before
  deriving the ring centre. A shelter exists for about 6 s of every 49 s cycle, and the centre costs a
  second 100 yd grid sweep to find the Toasty Fire.

**Residual, not addressed:** the icicle dodge issues `MOVEMENT_FORCED` while the shelter run issues
`MOVEMENT_COMBAT`, and `IsWaitingForLastMove` only yields to a strictly higher priority. A dodge that
fires late in the window can therefore still hold the movement slot until the freeze lands. Raising
the shelter run to `MOVEMENT_FORCED` would trade a Flash Freeze - which locks a bot out for up to
300 s and killed Prayer at 3:24 in this trace - for an Ice Shards hit at ~41% of a health pool. Worth
doing, but it is a priority change that wants its own before/after trace rather than being folded in
here untested.

## Deliberately out of scope

- **Tank defensive cooldowns.** Nothing ties Shield Wall / Survival Instincts / Divine Protection to a
  Frozen Blows window; the tanks used four and six respectively in six minutes. Real, and a separate
  change to the class strategies rather than the encounter.
- **The DPS deficit.** 42.9% boss health after six minutes is roughly half what hard mode needs. The
  movement economy work is aimed at this and should be re-measured before anything else is tried.
- **The Frozen Blows swap logic.** It works; it ran out of a living off-tank. Revisit after the aura
  fix.
- `docs/raids/ulduar.md:435-525`, still written against the 8 yd Starlight premise and still naming the
  removed `GetHodirDruidHelper`.

## Resolved, no longer an issue

Earlier rounds flagged "Thorim's strategy running live through the Hodir fight". **This trace carries
zero Thorim records** - 0 notes, 0 actions - against 5,430 notes / 29 actions in
`603_3_hodir_1787582677.ndjson`. Nothing needs doing.

It never changed behaviour even when it appeared. Across every trace that showed it, Thorim nodes
issued **zero accepted moves and zero OK verdicts**: `thorim reset encounter state action` FAILED
22-32 times (its normal return - it does its cleanup, then returns false so the engine keeps
descending) and `thorim arena positioning action` only ever IMPOSSIBLE or USELESS. The note volume was
not behaviour either: `thorim.squad` and friends are `RaidObs::ObsGuidMap` / `ObsValue` wrappers on the
encounter-state struct (`UldEncounter_Thorim.h:57-66`), so they write on state *access*, and leftover
state being probed each tick was enough. The cost was wasted trigger evaluation and a trace that was
harder to read, nothing more.

The gate that separates the wings is `NearThorimEncounter` (`UldEncounter_Thorim.cpp:80`), which needs
`z < ULDUAR_THORIM_WING_MAX_Z` (425). Hodir's floor is 432.687, so height excludes his room exactly as
the constant's comment claims.

## Verification

The module cannot be compiled headless here, so hand the build off and verify from a fresh trace.

1. Rebuild and restart `ac-worldserver`; confirm the effective config with
   `docker exec ac-worldserver env | grep ^AC_`, since `configurationOverrides/*.env` overrides
   `playerbots.conf`.
2. Pull Hodir with the same 24-bot raid, by boss so the trace stays boss-scoped.
3. On the new trace:
   - **Catches**: 61969 applies per freeze, 15 total here (2/4/2/2/2/2/1). Target 0-1 per freeze and
     none catching more than one.
   - **Theft**: re-run the "last accepted move before the freeze landed" attribution. 68 of 125 pairs
     were stolen here, 36 by `hodir raid position action`. Inside the window the only issuers should be
     `hodir move snowpacked icicle` and `hodir icicle dodge action`.
   - **Gap-closers**: no `charge` / `intercept` / feral charge with an `OK` verdict while Hodir is
     casting 61968.
   - **Arrival holds**: any bot that reaches its destination before the land time should still be
     within 9 yd of the fitted shelter centre at the land tick. Stormweaver arrives at 104.27 here and
     is 18 yd out by 106.03.
   - **Shelter pick**: `hodir.shelter` should resolve closer to the `hodir.centre` note than to
     (1986.56, -257.11) whenever the two disagree.
   - **Tank aura uptime**: the two tanks were last of everyone at 73.3% and 77.2%. They should be at
     or near the top. Re-run the per-63511 table - no tank hit should show aura NO.
   - **Tank survival**: both tanks alive past 4:30, and no 63511 hit on a tank with `resisted = 0`.
   - **Biting Cold cost** (the accepted regression): compare 62188 total and peak 62038 stacks. A small
     rise is expected; a large one means the window is holding longer than the cast.
   - **Boss health**: 42.9% at 6:02 here. Anything meaningfully better means the movement economy is
     paying off; no change means the DPS deficit needs its own pass.
4. Keep `603_3_hodir_1787590072.ndjson` for the before/after. Retention is 7 days by default.

On approval, copy this document to
`modules/mod-playerbots/docs/plans/hodir-shelter-hold-and-tank-resistance/hodir-shelter-hold-and-tank-resistance.PLAN.md`
before starting, and fold the durable findings into `docs/raids/ulduar.md` once it ships.
