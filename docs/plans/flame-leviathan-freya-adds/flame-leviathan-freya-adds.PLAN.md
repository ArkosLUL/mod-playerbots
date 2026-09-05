# Flame Leviathan: nothing ever shoots the Freya adds

## Context

Three Flame Leviathan pulls on 2026-09-05, all wipes:

| trace | length | boss floor |
|---|---|---|
| `603_1_flame-leviathan_1788623505.ndjson` | 310 s | 76.0 % |
| `603_1_flame-leviathan_1788623945.ndjson` | 145 s | 72.2 % |
| `603_1_flame-leviathan_1788624223.ndjson` | 317 s | **27.4 %** — best attempt, the reference trace below |

Hard mode was on with the Storm and Life towers live. **No Hodir's Fury stun (62297) landed in any
of the three**, so `fl.frozen` never emitted and last session's thaw fix
(`e41a0e89a`) is still unverified in the field — that is not evidence against it.

The user reported that bots never clear the Freya adds and proposed three fixes. All three are
addressed below; the corner posting was chosen over the recommendation not to build it, so it is in
the plan as specified.

### Root cause — one line

[UldActions_FlameLeviathan.cpp:115](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L115):

```cpp
Unit* target = boss ? boss : add;
```

An add is only ever considered when the boss is **absent**. The `add` fallback is also built from
the bot player's `"attackers"` list, which is near-empty inside a vehicle because threat belongs to
the vehicle creature — the file's own comment at :97 says so. So in practice no seat has ever fired
at an add. Every add hit-point removed today came from bots' personal class rotations leaking
through the vehicle multiplier (Starfall, Deep Wounds, Auto Shot); median time to kill an add that
way is **23 s**.

### How the mechanic actually works

From `src/server/scripts/Northrend/Ulduar/Ulduar/boss_flame_leviathan.cpp`:

- `ActivateTowers` (:496) schedules `EVENT_FREYA` **once**, 30 s after engage, when the Tower of
  Life stands. `case EVENT_FREYA:` (:441) has **no `events.Repeat`**, unlike Thorim's Hammer.
- `SummonTowerHelpers(TOWER_OF_LIFE)` (:668) spawns four reticle/ward pairs at exactly the four
  positions already in `ULDUAR_FL_ARENA_CORNERS`
  ([UldEncounter_FlameLeviathan.h:110](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.h#L110)).
- `npc_freya_ward::UpdateAI` (:921) fires a wave **every 29 s, forever, not gated on combat**.
- On every wave the ward re-runs, over **all** existing summons (:923-937):
  `summon->SetTempSummonType(TEMPSUMMON_MANUAL_DESPAWN)` — *the adds never time out* — and
  `SelectNearestTarget(200.0f)` + `AttackStart`. `SelectNearestTarget` is not `playerOnly`, and the
  raid rides vehicles, so **the adds attack vehicles**, and the whole standing population re-aims at
  the raid on every wave.
- Removal is only `Reset()` / `JustDied()` / `ACTION_DESPAWN_ADDS`.

Adds, both `SmartAI` with a single row — melee plus `Lash` 65062 on the victim every 2 s, nothing
else (`smart_scripts.sql:36513, 37233`):

| entry | name | health |
|---|---|---|
| 33387 | Writhing Lasher | 190,260 |
| 34275 | Ward of Life | 504,000 |

Flame Leviathan has **230,498,304**. Vehicles: siege ~1.8 M, demolisher ~1.0 M, chopper ~0.8 M.

### What it costs (reference trace)

- 72 adds spawned in 287 s. Population never returns to zero after 32 s and reaches **8** by 260 s.
- `Lash` did **194,050** of 969,350 raid damage taken (20 %) — the second largest damage spell in
  the fight behind Battering Ram (470,020).
- Adds in melee roughly **double vehicle attrition**: 1.32 % → 2.86 % hull hp per 5 s. The gap holds
  when restricted to windows more than 40 yd from the boss, i.e. with Battering Ram excluded.
- All 15 vehicles were destroyed between 188 s and 285 s. Adds landed the killing blow on 7 of the
  25 deaths, all after the fleet was gone.
- Seating is **not** a problem: all 25 bots were crewed the whole fight (a turret gunner sits 0.20 yd
  off its hull, which is easy to misread as dismounted).

### Weapon reach, DBC-verified

Effect and radius values from `modules/mod-spell-tweaks/data/dbc-reference/`.

| ability | seat | range | shape | damage | energy | cd |
|---|---|---|---|---|---|---|
| **Fire Cannon** 62358 → 62357 | siege **turret** | 10–70 | **20 yd splash** | **76,000** | 20 | – |
| **Hurl Boulder** 62306 → 62307 | demo driver | 10–70 | 20 yd splash | 27,000 | **0** | – |
| **Mortar** 62634 → 62635 | demo **gunner** | 0–50 | 11 yd splash | 11,100 + Flames 20,000 | **0** | 1 s |
| **Ram** 62345 | siege driver | 0–**15** cone | knockback (Effect 98) + dmg | 22,500 | 40 | – |
| **Ram** 62308 | demo driver | 0–15 cone | knockback (Effect 98) + dmg | 19,000 | **0** | 4 s |
| Sonic Horn 62974 | chopper | 35 cone | dmg | 6,300 | 20 | – |

Two corrections to assumptions in the request:

1. **Fire Cannon is the Salvaged Siege Turret's** (33067, `creature_template_spell.sql:9274`), not
   the Demolisher's. The Demolisher gunner's equivalent is **Mortar**. Both are wired below.
2. **Ram does knock back** — Effect 98 with a cone target — so the corner idea's mechanism is real.
   But 22,500 against 190 k / 504 k means Ram alone will not clear a wave.

**Mortar, Hurl Boulder and the Demolisher's Ram are all free**, so gunner add duty costs no energy.

Share of add-frames already inside a weapon band **with nobody moving** (reference trace):

| | coverage |
|---|---|
| siege turret, Fire Cannon 10–70 | 59.4 % |
| demo gunner, Mortar 0–50 | 52.1 % |
| demo driver, Ram cone 0–15 | 33.5 % |
| chopper, Sonic Horn cone 0–35 | 24.6 % |
| siege driver, Ram cone 0–15 | 17.6 % |
| **reachable by something** | **81.4 %** |

Adds clump: median 2 and p75 3 inside one 20 yd Fire Cannon splash.

### Why the corner posting needs care

The evidence against it, recorded so the next session does not re-litigate it: adds spawn at the
corners but travel a **median 124 yd** from spawn (only 4 % stay within 30 yd), because every wave
re-targets the whole population; and the corners sit a median **90–96 yd** from where the boss
actually roams. The user chose it anyway, so it is built — with one reservation that is not
optional: **`FlameLeviathanIsVentInterrupter` requires `FlameLeviathanCanElectroshock`, a 25 yd cone
test against the boss, which a corner siege fails.** If every siege engine posts, nothing interrupts
Flame Vents. One siege engine must therefore always hold station.

## Plan

### 1. Save the analysis harness first

Same rule as last time: every number above came from throwaway scripts, and they are the only way to
check whether any of this worked. Add an `--adds` mode to
[tools/botobs/flame_leviathan.py](tools/botobs/flame_leviathan.py), reusing its existing `Trace`
loader, reporting per pull: add spawn/lifetime table, population over time, wander-from-spawn
distribution, per-class weapon-band coverage, add clumping, and the hull-attrition split by adds in
melee range.

Two traps it must not repeat:

- **Occupancy by exact coordinates is wrong.** A turret gunner sits 0.20 yd off its hull, so an
  exact match reports five bots dismounted who were crewed all fight. Use a small tolerance.
- **The snapshot cast column only catches spells with a cast time.** Ram, Mortar and Fire Cannon are
  instant and will never appear there, so absence from that column is not evidence they did not
  fire. Judge ability usage from add health deltas instead.

### 2. Add-target selection

New in [UldEncounter_FlameLeviathan.h](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.h) /
[.cpp](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp):

```cpp
NPC_FL_WRITHING_LASHER = 33387,
NPC_FL_WARD_OF_LIFE    = 34275,
NPC_FL_FREYA_WARD_TARGET = 33366,   // reticle, marks a live Life tower
NPC_FL_FREYA_WARD        = 33367,

// Best add to shoot: the one with the most neighbours inside `splash`, nearest breaking ties.
Unit* FlameLeviathanBestAdd(PlayerbotAI* botAI, Unit* from, float minRange, float maxRange,
                            float splash);
```

Scan `"possible targets"`, the same value `FindMechanolift`
([Actions:71-87](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L71)) already proves works from
inside a vehicle. The adds are ordinary attackable creatures — the bots' own rotations hit them all
fight — so unlike the tower reticles they do not need the `"nearest npcs"` escape hatch.

Rank by neighbour count because the clump median is 2–3; `splash` is 20 for Fire Cannon and Hurl
Boulder, 11 for Mortar, 0 for the Ram cones.

### 3. Gunners take add duty

Priority chosen by the user: **an add in range outranks the boss.** The boss has 230 M hp against
190 k / 504 k per add, so the diverted damage is a rounding error.

- `SiegeEngineTurretAction` ([Actions:243](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L243))
  — after the Shield Generator and Mechanolift branches, before the boss fallback:
  `FlameLeviathanBestAdd(botAI, vehicleBase_, 10.0f, 70.0f, 20.0f)` → `SPELL_FL_FIRE_CANNON`.
- `DemolisherTurretAction` ([Actions:171](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L171))
  — after the Increased Speed, crate, thaw and Mechanolift branches:
  `FlameLeviathanBestAdd(botAI, vehicleBase_, 0.0f, ULDUAR_FL_MORTAR_MAX_RANGE, 11.0f)` →
  `SPELL_FL_MORTAR`. This is a target swap on a cast the gunner already makes every tick, not new
  spend.

Keep the Mechanolift branch ahead of adds in both: lifts are the pyrite supply and stop existing
once shot, whereas adds keep arriving every 29 s.

### 4. Drivers and choppers: take the cone, but pyrite comes first

The user's constraint: *"driver's main idea is the application of Pyrite stacks"*.

- `DemolisherAction` ([Actions:136](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L136)) —
  leave the thaw and the `needBarrel` branch exactly as they are. Add, **after** them:
  - an add inside the 15 yd cone → `SPELL_FL_DEMOLISHER_RAM` (62308, declared at `.h:78` and never
    used; free, 4 s cooldown, knockback);
  - otherwise, only when `!needBarrel`, an add in 10–70 yd → `SPELL_FL_HURL_BOULDER` instead of the
    boss.
- `SiegeEngineAction` ([Actions:217](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L217)) —
  Ram an add already inside the cone in preference to the boss. A **posted** siege must not turn for
  it: its facing belongs to the corner (step 5), so it takes what is already in the cone and nothing
  else. A station siege keeps the current `FlameLeviathanFaceForCone` turn-and-yield behaviour.

- `ChopperAction` ([Actions:264](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L264)) — Sonic
  Horn an add in preference to the boss, **excluding the tar lead**: gate on
  `!FlameLeviathanIsTarLead(botAI, bot)` so the lead chopper keeps laying tar in the boss's path and
  never turns off it. Leave the Tar branch above untouched — it fires on the "he is behind us" test
  and must keep winning. Sonic Horn's band is the 35 yd cone, so the pick is
  `FlameLeviathanBestAdd(botAI, vehicleBase_, 0.0f, ULDUAR_FL_SONIC_HORN_CONE_RADIUS, 0.0f)`
  filtered to what `FlameLeviathanInConeRange` accepts.

  Sonic Horn is the weakest tool in the fight (6,300 against 190 k / 504 k) on the thinnest hull
  (766–826 k), so it is worth having but should not pull a chopper off station — see the facing rule
  below, which is what keeps it from doing so.

**Facing follows the target.** Ram and Sonic Horn are cones, so a driver that engages an add has to
turn, and `DriveTo`'s park block re-faces the boss every tick
([Actions:573-584](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L573)) — the two would fight
each other once per tick and the vehicle would end up pointing at neither. Fix it at the one place
that decides: `HoldStation` faces the add the seat is engaging when its own driver weapon has one in
band, and the boss otherwise. It asks `FlameLeviathanBestAdd` the same question the cast branch
does, so the two cannot disagree. A posted siege engine is the exception — its facing belongs to its
corner (step 5). The tar lead is unaffected: it keeps `faceAway = true` toward the boss.

### 5. Corner posting

New constants and helpers:

```cpp
constexpr float ULDUAR_FL_CORNER_STANDOFF = 12.0f;  // inside Ram's 15 yd, outside Fire Cannon's 10 yd floor

int8 FlameLeviathanCornerPost(PlayerbotAI* botAI, Player* bot);  // corner index, or -1
Position FlameLeviathanCornerPostPoint(uint8 index);
```

`FlameLeviathanCornerPostPoint` returns the point `ULDUAR_FL_CORNER_STANDOFF` from
`ULDUAR_FL_ARENA_CORNERS[index]` along the line toward the ring centroid, so the engine sits on the
arena side and Ram's knockback pushes adds **into** the corner.

`FlameLeviathanCornerPost` copies the election shape of `FlameLeviathanIsTarLead`
([.cpp:388](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp#L388)) — walk the group, rank live
crew-usable non-pursued siege **hulls** by GUID — and then:

- rank 0 → `-1`: **holds station and keeps the vent interrupt.** Non-negotiable, see Context.
- ranks 1..4 → corners 0..3. On 10-man there are only 2 siege engines, so exactly one posts.
- `-1` until the Life tower is confirmed live, so this never fires on a pull with the tower down.

Latch the tower in the shared per-instance state rather than testing it per bot: add
`bool freyaAddsSeen` to `FlameLeviathanState` and set it from `TickFlameLeviathan` when any bot's
`"possible targets"` or `"nearest npcs"` holds a 33387 / 34275 / 33366 / 33367. It only latches on,
and the config-driven `FlameLeviathanActiveTowerMask` cannot answer this — it reports all towers
whenever hard mode is on. Emit it as `fl.corner` through `RaidObs::NoteDerived` so the next trace
shows who posted where.

Wiring in `FlameLeviathanDriveAction::Execute`
([Actions:324](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L324)) — insert **after** the
Pursued, hazard and Battering Ram branches and **before** `DetourToCrate`/`HoldStation`, so tower
dodging and the blast backoff still outrank a posting:

```cpp
if (int8 corner = FlameLeviathanCornerPost(botAI, bot); corner >= 0)
    return DriveTo(FlameLeviathanCornerPostPoint(corner), ULDUAR_FL_ARENA_CORNERS[corner]);
```

`DriveTo` ([Actions:559](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L559)) currently faces
a `Unit*`. Add an overload taking a `Position` to face, reusing the existing park/facing block —
the corner is a point, not a unit.

### 6. Fix the Ram cone radius

`ULDUAR_FL_RAM_CONE_RADIUS` is 18.0
([.h:151](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.h#L151)) but Ram's `RangeIndex` 11 is
**15 yd** — 18 is its *effect* radius index, not its range. `FlameLeviathanInConeRange` adds the
target's object size, which hid the error against a 15 yd-reach boss and will not hide it against a
1 yd-reach lasher: the bot would keep offering casts that `CheckCast` rejects on range. Set it to
15.0 now that Ram is aimed at small targets.

The **width** is not wrong and must not be "fixed": `spell_cone.sql` in this repo carries no row for
62345, but the live `acore_world.spell_cone` does — Ram 100°, Electroshock 60°, Sonic Horn 50°, and
the Demolisher's Ram 62308 also 100°. The module's half-angles already match. Only the reach was
wrong. Both Rams share 15 yd and 100°, so the demolisher branch reuses the same two constants.

### 7. Write it down

- [docs/raids/ulduar/flame-leviathan.md](docs/raids/ulduar/flame-leviathan.md) — the Freya's Ward
  mechanic (one summon at 30 s, four corners, a wave every 29 s forever, adds never despawn,
  re-target the whole population each wave, and attack vehicles not players); the two add entries
  with health; the corrected ability table including which seat owns Fire Cannon; and the corner
  posting with the vent-interrupt reservation.
- Copy this plan to
  `docs/plans/flame-leviathan-freya-adds/flame-leviathan-freya-adds.PLAN.md`, and record the
  measured baseline in `flame-leviathan-freya-adds.BASELINE.md`.
- No `docs/engine/pitfalls.md` change is needed; nothing here is a new engine-level trap.

## Files

- [tools/botobs/flame_leviathan.py](tools/botobs/flame_leviathan.py) — new `--adds` mode
- [src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.h](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.h)
  — add entries, `ULDUAR_FL_CORNER_STANDOFF`, `FlameLeviathanBestAdd`, `FlameLeviathanCornerPost`,
  `FlameLeviathanCornerPostPoint`, `ULDUAR_FL_RAM_CONE_RADIUS` 18 → 15
- [src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp)
  — helper bodies, `freyaAddsSeen` latch in `TickFlameLeviathan`, `fl.corner` note
- [src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp)
  — both gunner actions, both driver actions, `ChopperAction`, the corner branch in `Execute`,
  the `HoldStation` facing rule, `DriveTo` position overload
- docs as listed in step 7

## Verification

The module cannot be compiled headless here, so this needs a hand-off build and a live re-pull with
the Tower of Life standing. Today's three traces are the baseline.

1. `postmortem.py <file> --notes fl` — `fl.corner` should name one siege engine per corner, stable
   for the pull, with exactly one siege engine unposted.
2. `flame_leviathan.py <file> --adds` against the new trace. Numbers to beat, from
   `603_1_flame-leviathan_1788624223.ndjson`:

   | measure | baseline | target |
   |---|---|---|
   | adds alive at 260 s | 8 | low single digits |
   | add population ever back to 0 after 32 s | never | repeatedly |
   | median time to kill one add | 23 s | a few seconds |
   | `Lash` share of raid damage taken | 20 % | well under |
   | hull hp lost per 5 s with adds in melee | 2.86 % | at or near the 1.32 % clean figure |
   | vehicles alive at 250 s | 5 of 15 | more |
   | boss floor | 27.4 % | lower |

3. Guard the things most likely to regress, because they are what the corner posting spends:
   - **Flame Vents interrupts.** Confirm Electroshock still fires — one siege engine must be
     unposted and in range. This is the single highest-risk item in the plan.
   - **Boss damage.** Four of five siege engines stop Ramming him; check the boss floor did not get
     worse than 27.4 % despite the adds being handled.
   - **Pyrite uptime.** `fl.pyrite` should be unchanged — the demolisher driver's barrel branch was
     deliberately left first.
   - **The tar lead.** `fl.station` should still show a `tar-lead` chopper throughout, and Pool of
     Tar (33090) should keep appearing at the same rate — the lead is excluded from add duty, and a
     regression here means the exclusion did not hold.
   - **Vehicles chasing adds.** The facing rule is the guard against it. Compare each class's
     distance-to-boss distribution against the baseline; a chopper or siege engine drifting off
     station means facing and station are fighting again.

---

# Iteration 2 — one dead helper switched off three roles

The first trace carrying the work above, `603_1_flame-leviathan_1788628796.ndjson`, is in
`.BASELINE.md` under "Second baseline". The add targeting landed. Nothing gated on
`FlameLeviathanCrewUsable` ran, and none of it has since 2026-08-30.

## Root cause

`FlameLeviathanCrewUsable` tested `!member->HasUnitState(UNIT_STATE_NOT_MOVE)` on the **player**.
`Vehicle::AddPassenger` calls `unit->SetControlled(true, UNIT_STATE_ROOT)` on every passenger, driver
included (`Vehicle.cpp:437`), clearing it only on exit (`Unit.cpp:15834`), and `UNIT_STATE_NOT_MOVE`
is `ROOT | STUNNED | DIED | DISTRACTED`. So the test was false for every crewed bot, always. The hull
half of the same predicate is fine — a hull is not a passenger, and `fl.frozen` has never emitted.

Introduced by `e41a0e89a` to keep a Hodir's Fury stun from holding a role. Right intent, too wide a
predicate. It killed `FlameLeviathanIsVentInterrupter` (no Electroshock ever cast),
`FlameLeviathanIsTarLead` (no lead chopper) and `FlameLeviathanCornerPost` (no engine ever posted).

## Changes

1. **`FlameLeviathanCrewUsable`** — the rider is checked for `UNIT_STATE_STUNNED` only, the hull
   keeps `UNIT_STATE_NOT_MOVE`. Boarding sets `ROOT` and nothing else, so a stun-only test keeps the
   original intent at no cost.
2. **`fl.vent`** (`RaidObs::Note`, not `NoteDerived` — a run of hits carries the same value) on every
   Electroshock, valued `hit` or `miss` from whether the channel stopped. **`fl.lifetower`** once
   when the tower latch flips, so an empty `fl.corner` separates "tower was down" from "election
   failed". **`fl.station = vent`** for the reserved engine.
3. **Stable corner ranking.** `FlameLeviathanSiegeRank` ranks every live siege hull by guid; pursued
   and stunned engines keep their slot and simply do not drive to it. The old ranking skipped them,
   so a Pursued switch (~every 31 s) renumbered everyone and swapped all four corners.
4. **`FlameLeviathanIsVentReserve`** — the rank 0 engine, once the others are posting. `HoldStation`
   and `SiegeEngineAction` both except it from add-facing, so its facing stays on the boss and inside
   Electroshock's 25 yd / 60° cone. With the tower down it returns false and all five engines behave
   as before.
5. **`flame_leviathan.py --vents`** — channels, ticks each, how many were cut short with room to
   finish, the `fl.vent` notes, and hull attrition inside versus outside a channel.

## Verification

Not compiled. Needs a hand-off build, then a re-pull with the Life and Frost towers standing.
Baseline is `1788628796`.

| measure | now | target |
|---|---|---|
| `fl.corner` notes | 0 | 4, stable for the pull |
| `fl.station = tar-lead` | 0 | 1 throughout |
| `fl.vent` casts | n/a, new note | one per channel |
| Flame Vents channels cut short | 2 of 13 | most of them |
| hull hp lost per 5 s while venting | 2.53 % | toward the 1.99 % clean rate |
| mean hull hp at 180 s | 22 % | above the 28 % of 1788624223 |
| first bot on foot | 80 s | as late as possible |
| deaths | 31 | fewer |
| boss floor | 36.4 % | below 27.4 % |
| median time to kill one add | 13 s | hold it |
| `Lash` share of damage taken | 4.4 % | hold it |

Watch, because the corner posting has still never actually run:

- **Four engines leaving station at once** when the tower latches, and what that does to Battering
  Ram exposure and boss uptime.
- **Corner churn** — `fl.corner` should not change value for a bot mid-pull.
- **The reserve drifting** out of Electroshock's cone while it chases its own station offset.

## Left alone

- **Boss floor got worse, 27.4 % to 36.4 %.** Both gunners prefer an add whenever one is in range,
  which is what was asked for, and an add now dies in 13 s instead of 22 s. Re-measure once the
  interrupt is back before touching the priority: the fleet living longer may pay for it.
- `stations()` in `flame_leviathan.py` still keeps only the last `fl.station` per bot, so it cannot
  show a role that was held and then lost.
