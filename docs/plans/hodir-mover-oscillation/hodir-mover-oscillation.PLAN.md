# Hodir: stop the three movers that walk bots away from the fight

## Context

`env/dist/logs/botobs/603_1_hodir_1788378556.ndjson` (schema v10, 25 raiders, **kill at 6:20.382**,
2 deaths) is the first Hodir kill on record here, and the third pull since `1310ba793` fixed the
icicle dodge. Nothing in `src/Ai/Raid/Uld/**_Hodir.*` has changed since that commit, so this pull
runs the same Hodir code as the 5:58 wipe before it.

The kill is not slower than what came before. Lined up by boss health:

| boss hp | `1788289957` | `1788294356` | this pull |
|---|---|---|---|
| 40% | 3:54.3 | 3:23.8 | 3:33.6 |
| 30% | 4:30.6 | 3:58.3 | 4:21.3 |
| 20% | 5:22.1 | 4:29.6 | 5:02.5 |
| end | wipe 5:56.8 at 14.93% | wipe 5:57.8 at 3.48% | **kill 6:20.4** |

The middle pull was on pace for roughly 6:10 when it died. What this pull actually lost is 33
seconds, all of it between 40% and 20%, and it is bought back by one thing: **bots spend 58.7% of
their alive time walking, and half of that walking is undone within five seconds.**

Read a trace with `python tools/botobs/postmortem.py <file>`. Schema: `docs/systems/observability.md`.
Hodir code: `src/Ai/Raid/Uld/{Action,Trigger,Multiplier,Util}/*_Hodir.*`.

**Scope decided with the user:** the Starlight latch, the Biting Cold shed, and the Storm Cloud lap,
shipped together in one build. Per-action latches only, no shared reversal guard. Not the
burst-cooldown multiplier (shared code, recorded below), not melee/tank Starlight re-siting, not the
ice-breaker priority.

**Shedding by jumping was investigated and ruled out during implementation** - see Problem 2. The
shed keeps walking; what changed is that it holds each leg and sweeps for icicles and the boss.

---

## What the trace says

### Pace

Boss pool 38,567,500. Roster `dealt` totals 43,842,171 over 380.3 s (115,296 dps) against
38,413,230 taken off Hodir's own health, **ratio 1.14** (1.09 last pull - the extra is ice blocks).
A 3:00 kill needs 214,264 dps on the boss.

Where the 33 seconds went, in 30 s windows, against the same windows of the previous pull:

| window | dps | moving | walked | wasted | prev dps | prev moving | prev walked |
|---|---|---|---|---|---|---|---|
| 3:00-3:30 | 101,606 | 60.6% | 3,072 | 54.3% | 106,482 | 55.3% | 2,321 |
| 3:30-4:00 | 110,425 | 61.1% | 3,085 | 49.2% | 128,096 | 50.3% | 2,167 |
| **4:00-4:30** | **95,034** | **67.3%** | **3,484** | 49.1% | 127,101 | 51.3% | 2,205 |
| 4:30-5:00 | 99,218 | 56.0% | 2,835 | 52.8% | 88,026 | 51.5% | 2,291 |

Same fight, 40% more walking, and the dps gap is the whole deficit.

### The oscillation

Raid-wide: **36,799 yd walked, 18,187 yd net, 50.6% undone inside 5 s** (46.1% last pull). Moving
share 58.7% (54.3%); ranged 55.3% (49.8%), which is the group whose dps sagged.

Take every accepted destination change landing within 4 s of the previous one and ask whether the
new point sits more than 90 degrees off the heading the bot was already walking. **2,421 of 2,676
are turn-arounds, 90.5%** (87.0% last pull):

| previous mover | next mover | flips | rate | median flip |
|---|---|---|---|---|
| biting cold shed | biting cold shed | 454 / 485 | 94% | 879 ms |
| biting cold shed | icicle dodge | 246 / 282 | 87% | 230 ms |
| icicle dodge | biting cold shed | 241 / 249 | 97% | 403 ms |
| icicle dodge | reach melee | 100 / 100 | **100%** | 416 ms |
| reach melee | icicle dodge | 98 / 124 | 79% | 215 ms |
| reach melee | reach melee | 81 / 82 | 99% | 633 ms |

Attributing every metre to whichever action held the movement slot:

| holds the slot | share of move time | walked | net | wasted | legs | boss gap p50 |
|---|---|---|---|---|---|---|
| biting cold shed | 22.4% | 8,769 | 5,950 | 32.2% | 682 | 18.7 |
| raid position | 22.2% | 7,345 | 6,210 | 15.5% | 505 | 20.6 |
| icicle dodge | 22.4% | 3,331 | 2,780 | 16.5% | 1,016 | 14.2 |
| move snowpacked icicle | 15.8% | 5,065 | 4,747 | 6.3% | 433 | 15.8 |
| reach melee | 8.3% | 2,982 | 2,923 | 2.0% | 440 | 9.6 |
| set behind | 4.2% | 902 | 771 | 14.6% | 177 | 4.4 |
| **spread storm cloud** | 1.7% | 774 | 446 | **42.4%** | 31 | 20.4 |

The four Hodir movers own 82.8% of all movement time. Melee land within 8 yd of Hodir 96.0% of the
time `set behind` last moved them and 64.0% under the dodge, but only 36.6% under `reach melee`,
31.7% under the shed and 26.3% under the snowpacked-icicle run - and those three own 55% of melee
movement time. Melee within 8 yd overall: 49.8%.

### Problem 1 - the Starlight anchor still never settles

Unchanged, because the fix was never built: **739 anchor changes across 23 bots, gap p50 3,896 ms**
(741 / 3,354 ms last pull). `hodir.starlight` time-weighted: stand 184, noreach 151, none 79,
fire 23. Starlight uptime ranged 17.3%, healers 18.3%, melee 0.9%.

**Cause.** `UldEncounter_Hodir.cpp:312` `FindHodirStarlightStand` is stateless. Zones are ranked by
`slot.GetExactDist2d(&zone)` and the stand bearing is taken from the slot, so both inputs move
whenever the slot does. `hodir.centre` took 3 distinct values this pull and every centre change
moves all 14 slots.

### Problem 2 - the Biting Cold shuttle is the biggest single mover, and it does not need to walk

**How shedding actually works.** `spell_hodir_biting_cold_player_aura::HandleEffectPeriodic`
(`src/server/scripts/Northrend/Ulduar/Ulduar/boss_hodir.cpp:1251`) ticks every **1,005 ms**
(measured off the 62188 damage rows) and reads:

```cpp
if (target->isMoving() || target->HasAura(SPELL_MAGE_TOASTY_FIRE_AURA))
{
    if (_prev) { ModStackAmount(-1); _prev = false; }
    else _prev = true;
}
else { _prev = false; ++_counter; if (_counter >= 4) ModStackAmount(1); }
```

so **two consecutive moving ticks drop one stack** and any stationary tick resets the progress. The
bot therefore has to stay in motion for more than a second, twice, which is what the 6 yd legs buy.

**Jumping instead was investigated and does not work.** `isMoving()` is
`m_movementInfo.HasMovementFlag(MOVEMENTFLAG_MASK_MOVING)` (`src/server/game/Entities/Unit/Unit.h:1712`)
and that mask does include `MOVEMENTFLAG_FALLING | MOVEMENTFLAG_ASCENDING | MOVEMENTFLAG_DESCENDING`
(`src/server/game/Entities/Unit/UnitDefines.h:405-408`), so a jump raises it - but not for long
enough. `MotionMaster::MoveJump` (`src/server/game/Movement/MotionMaster.cpp:675`) splines to the
point and takes its duration from `length / speedXY`, so an in-place hop of 0.01 yd is airborne for
about **1.4 ms** at run speed and can never cover a 1,005 ms tick, let alone two consecutive ones.
That is what `IntenseColdJumpAction`'s own comment reports ("takes a couple of ms, it doesn't do a
natural jump", `src/Ai/Dungeon/Nex/NexActions.cpp:157-166`). Two more gates would block it anyway:
`IsDuplicateMove` refuses a destination within 0.01 yd of the last one, and `JumpTo` books the
movement slot for 1,000 ms at the same priority. A jump long enough to span two ticks needs more than
7 yd of travel at run speed, which is the walk this already does.

The one way to make it work would be a `JumpTo` that exposes `speedXY`: a 2 yd hop at 2 yd/s lasts a
second, so chained alternating hops would shed with a 2 yd amplitude and no net drift. That is an
additive change to shared movement code and is not in this scope.

The snapshot `moving` column is literally `unit->isMoving()` (`src/Bot/Obs/RaidObsSnapshot.cpp:55`),
so the trace measures the exact predicate the boss reads.

The walk does work today: stacks reached 3 on only 39 aura rows all fight, 147 drops against 149
rises. It is the price that is wrong.

**Why the walk is wrong.** `DeriveHodirShuttleLeg` (`UldEncounter_Hodir.cpp:435`) builds its hazard
list from allies only:

```cpp
Position leg = FindNearestPositionClearOfHazards(bot, crowd, ULDUAR_HODIR_DECLUMP_RADIUS,
                                                 ULDUAR_HODIR_DODGE_LEASH,
                                                 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG);
```

No icicles, no `preferNear`, no boss-distance gate, and the `crowd.empty()` branch is a blind 6 yd
hop on a guid-derived bearing. `HodirBitingColdShedAction::Execute` (`UldActions_Hodir.cpp:171`)
re-derives it every tick with no latch: **6,686 issues against 4,122 `wait` results**, and the 454
self-reversals above are it turning a bot around roughly once a second.

It is also what killed **Totemist at 5:28.744** - 7.9 yd from Hodir, still walking under
`hodir biting cold shed`, finished by Freeze 62469 for 6,201 from 26.02%.

Ranged it last moved sit beyond 30 yd from Hodir 25.6% of the time, the worst of any mover bar
`reach spell`.

### Problem 3 - the Storm Cloud lap is a runaway (new)

`HodirSpreadStormCloudAction::Execute` (`UldActions_Hodir.cpp:334`) reads both the angle and the
radius off the bot's *current* position every tick:

```cpp
float const botAngle = std::atan2(bot->GetPositionY() - centre.GetPositionY(), ...);
float const lapRadius = std::max(bot->GetExactDist2d(&centre), ULDUAR_HODIR_RAID_RING_INNER);
```

so the target is always 45 degrees ahead of wherever the bot has drifted to, at whatever radius it
has drifted to. The bot can never arrive, and the `max()` locks in any outward drift permanently.
**75.3% of its accepted re-points are turn-arounds** and 42.4% of the distance it walks is undone,
both the worst of any mover.

The radius is also the wrong quantity. The formation ring is `ULDUAR_HODIR_RAID_RING_INNER` 4.5 to
`ULDUAR_HODIR_RAID_RING_OUTER` 9.0 yd, but carriers stand 20 to 35 yd from the ring centre, so one
45 degree step is a 15 to 27 yd walk while Storm Cloud has only 4-6 one-second ticks. The comment's
worry about "diving through the middle of the formation" is backwards: driving through the raid is
how a 3 yd buff (`ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS`) gets spread.

All 15 carries walked 47 to 213 yd and every one pushed the carrier further from Hodir. The worst
killed the raid's first casualty:

**Malediction, carry 0:19.971-0:49.955.** 213.5 yd walked in 30 s, boss gap 32 yd growing to
**142 yd**. It left the chamber, was parked at 78 yd by 1:00, was caught alone by Flash Freeze, and
died at **1:21.603** to eight ticks of Frozen Blows 64545 with no healer in range.

### Damage taken, for context

6,582,056 raid-wide, 17,309/s against 16,229/s last pull, so it scales with fight length rather than
with anything positional. **Frozen Blows 64545 is 64.3%** of it, landing evenly on everyone; Biting
Cold self-damage 13.8%, Frozen Blows 63511 8.5%, Freeze 6.4%, Ice Shards 2.7%. Effective healing
6,530,857 kept 23 of 25 alive, so healing is not the constraint.

### Everything needed to fix these already exists

`UldActions_Hodir.cpp` has `CollectHodirIcicleHazards` (line 35) and `GetHodirDodgePreference`
(line 70, melee toward Hodir, ranged and healers toward their anchor) in its anonymous namespace,
and `FindNearestPositionClearOfHazards` already takes `preferNear` and a per-hazard radius
(`src/Util/EncounterHelpers.h:68`). The shuttle uses none of it. The per-bot latch pattern is
`thread_local std::unordered_map<ObjectGuid, …>` as in
`src/Ai/Raid/EoE/Util/EoEEncounter_Drakes.cpp:49` - a bot only updates its own entry on its own map
thread, so no lock.

---

## Approach

Five edits, no new files, no schema change, no new constants.

### E1 - latch the Starlight zone (`UldEncounter_Hodir.cpp:312`)

Give `FindHodirStarlightStand` a per-bot latch holding the chosen zone centre and the stand point
derived from it. Each call, after the sweep:

1. If a latched entry exists and its zone is still in `zones` (match within 1 yd - these are static
   dynamic objects), re-validate the **stored stand** against the fire leash and the caster band. If
   it still passes, return it unchanged.
2. Otherwise pick as today (nearest zone to the slot, bearing from the slot) and latch both.
3. Drop the entry when the latched zone leaves the sweep, so an expired zone re-picks at once.

The latch must live below `GetHodirAnchor`, not in the action: the trigger calls the same helper and
the two must not disagree. This also fixes the bearing rotation, since the bearing is computed once.

### E2 - the shed sweeps for icicles and prefers the boss (`UldEncounter_Hodir.cpp:435`)

Move `CollectHodirIcicleHazards` and `GetHodirDodgePreference` out of `UldActions_Hodir.cpp`'s
anonymous namespace into `UldEncounter_Hodir.{h,cpp}`; the dodge keeps calling them unchanged.

Collapse the `crowd` and `solo` branches into one sweep over a `std::vector<HazardCircle>`:

- every ally within `ULDUAR_HODIR_DODGE_LEASH` at `ULDUAR_HODIR_DECLUMP_RADIUS` (today's behaviour)
- every live icicle at `ULDUAR_HODIR_ICE_SHARDS_CLEAR` / `ULDUAR_HODIR_BIG_SHARDS_CLEAR`, the clears
  rather than the lethal radii, because a leg that ends on the edge is a death
- for ranged and healers only, **Hodir at `ULDUAR_HODIR_RANGED_MIN_BOSS_GAP`**

with `preferNear = GetHodirDodgePreference(...)`, so a tie inside the first clear ring breaks toward
the boss for melee and toward the ring slot for ranged. Keep the dodge's fallback shape: if nothing
is clear at `ULDUAR_HODIR_DODGE_LEASH`, retry with the icicles at
`ULDUAR_HODIR_ICE_SHARDS_RADIUS + 0.5f` / `ULDUAR_HODIR_BIG_SHARDS_RADIUS + 0.5f`, because a bot that
cannot move sheds nothing. `how` still reports `crowd` vs `solo` on whether any ally was in range.
Tank and Starlight branches untouched.

### E3 - the shed holds its leg (`UldActions_Hodir.cpp:171`, `UldActions_Hodir.h:52`)

`HodirBitingColdShedAction` gets `_leg` and `_legDist` beside `_shedding`, and the latch shape
`HodirIcicleDodgeAction` already has (`_dest` / `_destDist`, `UldActions_Hodir.h:45-47`), with one
deliberate difference: **on arrival it derives the next leg instead of returning false**, because it
has to chain - one leg cannot cover two consecutive aura ticks. What the latch removes is re-deriving
*while the walk is in flight*, which is every one of the 4,122 `wait` rows.

Re-derive when the walk is done (`<= ULDUAR_HODIR_DODGE_ARRIVE`), when the held leg stops being clear
of the live icicle list, or when the bot has slipped more than `ULDUAR_HODIR_DODGE_SLIP` back from its
closest approach. Clear `_leg` alongside `_shedding`.

### E4 - pin the Storm Cloud lap (`UldActions_Hodir.cpp:334`, `UldActions_Hodir.h:105`)

`HodirSpreadStormCloudAction` gets `_lapCentre`, `_lapRadius`, `_lapAngle` and `_step` beside
`_direction` / `_lastStacks`. The `stacks > _lastStacks` branch already detects a fresh carry, so
latch there, in the same place `_direction` is picked:

- `_lapCentre = centre`
- `_lapRadius = std::clamp(bot->GetExactDist2d(&centre), ULDUAR_HODIR_RAID_RING_INNER,
  ULDUAR_HODIR_RAID_RING_OUTER)` - the ring the raid is actually standing on, which is where a 3 yd
  buff has to be carried, and which bounds the whole lap inside a 9 yd circle so a runaway is
  geometrically impossible
- `_lapAngle = botAngle`

Then every tick derives `_step` from the **latched** centre, radius and angle only, never from the
bot's current position. Advance `_lapAngle` by `_direction * M_PI / 4` when the bot is within
`ULDUAR_HODIR_DODGE_ARRIVE` of `_step`, not every tick. If the next step would land further than
`ULDUAR_HODIR_CASTER_MAX_BOSS_GAP` from Hodir, flip `_direction` and re-derive once rather than
walking out; that is the belt-and-braces against leaving the room. Clear the latch when the aura is
gone.

### E5 - probe the latches

`hodir.starlight` and `hodir.anchor` already read E1 directly: a latch that holds shows up as far
fewer note rows and an anchor that stops jumping.

For E3, extend the existing `hodir.shuttle` note, which already carries
`tank`/`starlight`/`crowd`/`solo`, with `held` for when the latch returns the existing leg rather than
sweeping. That makes the latch's hit rate readable instead of inferred from the `wait` count: notes
alternate `held` and the sweep reason once per leg, so the row count is the leg count.

For E4, add a `hodir.stormcloud` note carrying `lap` on a fresh latch, `held` while walking to
`_step`, `flip` when a step is rejected for the boss gap, so a runaway cannot come back silently.

## Files

- `src/Ai/Raid/Uld/Util/UldEncounter_Hodir.cpp` - E1 (latch), E2 (shuttle sweep), and receives the
  two helpers moved out of the actions file
- `src/Ai/Raid/Uld/Util/UldEncounter_Hodir.h` - declarations for the two moved helpers, and
  `ULDUAR_HODIR_STARLIGHT_ZONE_MATCH = 1.0f` for the latch's zone identity test
- `src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp` - drop the two moved helpers, E3 leg latch, E4 lap pin
- `src/Ai/Raid/Uld/Action/UldActions_Hodir.h` - `_leg` / `_legDist` on `HodirBitingColdShedAction`,
  `_carryApplied` / `_lapCentre` / `_lapRadius` / `_lapAngle` / `_step` on
  `HodirSpreadStormCloudAction`
- `docs/raids/ulduar/hodir.md` - the measurements above. It is reachable from `CLAUDE.md` via
  `docs/engine/pitfalls.md`, so this edit needs its own **`/compact-docs-writer`** invocation

No `CMakeLists.txt` change; AzerothCore globs module sources. Run
`python apps/codestyle/codestyle-cpp.py` from `modules/mod-playerbots` before calling it done. There
is no headless build path here, so the build and the pull are the user's.

## Verification

Rebuild, restart `ac-worldserver`, confirm the effective config with
`docker exec ac-worldserver env | grep ^AC_`, then pull Hodir with the same raid.

| check | this pull | expected |
|---|---|---|
| walking undone within 5 s, raid | 50.6% | under 30% |
| turn-arounds among re-points inside 4 s | 2,421 / 2,676 (90.5%) | under 900 |
| shed self-reversals | 454 at p50 879 ms | under 100 |
| shed vs dodge flips (both directions) | 487 at 230-403 ms | under 120 |
| bots moving, share of alive time | 58.7% | under 45% |
| `hodir.anchor` changes / gap | 739, p50 3,896 ms | under 250, p50 over 15 s |
| Starlight uptime, ranged / healers | 17.3% / 18.3% | over 40% |
| Biting Cold stacks reaching 3 | 39 aura rows | no worse than 39 |
| `hodir biting cold shed` issued / `wait` | 6,686 / 4,122 | under 2,500 / under 1,200 |
| distance walked under the shed | 8,769 yd | under 2,000 yd |
| ranged beyond 30 yd when the shed last moved them | 25.6% | under 12% |
| storm cloud walked / wasted | 774 yd, 42.4% | under 350 yd, under 15% |
| worst carry, boss gap growth | 32 -> 142 yd | never past 30 yd |
| melee within 8 yd of Hodir | 49.8% | over 65% |
| melee within 8 yd under `reach melee` | 36.6% | over 70% |
| raid dps on the boss | 115,296 | over 140,000 |
| kill time | 6:20.4 | under 5:00 |

Watch `Biting Cold stacks reaching 3` alongside the shed numbers. The latch makes the shed re-derive
far less often, and if that costs stacks the legs are being held past the point where they still
chain - the arrival threshold is what to move, not the latch.

Re-run the reconciliation, since it validates every `dealt` number: sum `snap.u[11]` across the
roster at the end against `(1 - finalHp%) * 38,567,500` from Hodir's own rows. It was **1.14** here
against 1.09 the pull before; a large move means the ice-block split changed, not that throughput
did.

Keep `603_1_hodir_1788378556.ndjson` for the before/after. Retention is 7 days.

## Out of scope

- **Shedding by hopping.** `MovementAction::JumpTo` hardcodes `speedXY` to run speed, and
  `MoveJump`'s duration is `length / speedXY`, so every jump short enough to stay in place is too
  short to cover an aura tick. Adding an optional `speedXY` to `JumpTo` would let a 2 yd hop last a
  second, and chained alternating hops would shed with a 2 yd amplitude and no net drift - replacing
  8,769 yd of walking with something that never leaves the stand. It is an additive change to shared
  movement code, so it wants its own pull.
- **A shared reversal guard.** Refusing a Hodir destination that points back across the bot within
  ~600 ms would catch the 487 cross-action flips the per-action latches cannot see. The user chose
  latches only; revisit if the flip count survives them.
- **DPS cooldowns.** They fire on cadence when a tank is holding Hodir, but
  `HoldBurstUntilTankEngagedMultiplier` (`src/Ai/Base/Strategy/BurstWindowStrategy.cpp:62`) vetoes a
  burst whenever the bot has a Flash Freeze block selected instead of Hodir: the
  `!IsBossCreature(target)` branch returns 0.0 without the main-tank fallback the lust branch above
  it already uses. It hits the ranged hardest, which is exactly who breaks ice. ~6 lines in shared
  code that affects every raid; the user deferred it.
- **Melee and tank Starlight.** Still 0.9% and 0.1%. Needs re-siting the fight around the zones.
- **The ice-breaker priority.** Ranged spend 37.6% of their target time on blocks and healers 57.7%,
  and the ratio of roster damage to boss damage rose 1.09 to 1.14 because of it. Unchanged by
  decision - the budget is going to helpers as intended.
- **`hodir move snowpacked icicle`.** 15.8% of movement time and only 6.3% wasted, so it is not
  churning, but it leaves melee within 8 yd only 26.3% of the time and 40.6% of its re-points push
  the destination further along the same heading. Worth a look after these five land.
- **`reach spell`.** Ranged it last moved sit outside the caster band 60.7% of the time and beyond
  30 yd 38.4%, but it holds the slot for 1.4% of movement time, so it is noise next to the shuttle.
