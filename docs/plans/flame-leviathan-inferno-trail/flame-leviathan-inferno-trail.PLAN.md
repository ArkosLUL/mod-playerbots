# Flame Leviathan, four towers up: the fire nobody dodges, and a combat latch that drops the wheel

## Context

Two pulls on 2026-09-12, both wipes, both with **all four towers standing** — the first traces of
`8823616d4` ("stop asking a rider whether it can move") and the first ever run with Mimiron's and
Thorim's hazards live.

| trace | length | outcome | boss floor | deaths |
|---|---|---|---|---|
| `603_4_flame-leviathan_1789222298.ndjson` | 211 s | wipe | **69.1 %** | 26 |
| `603_4_flame-leviathan_1789222754.ndjson` | 318 s | wipe | **64.0 %** | 32 |

Against the `1788628796` baseline (36.4 % floor) and the `1788624223` reference (27.4 %), boss
damage collapsed. Every one of the 58 deaths was a bot **on foot**; no bot died while crewed.

Hard mode is confirmed on: `AC_AI_PLAYERBOT_ULDUAR_FLAME_LEVIATHAN_HARD_MODE=1` from
`docker exec ac-worldserver env`, so `FlameLeviathanActiveTowerMask` returns `FL_TOWER_ALL`.

### The crew-usable fix landed — all three dead roles are alive

`fl.vent`, `fl.corner`, `fl.lifetower` and `fl.station = tar-lead` all emit for the first time.

| measure | baseline `1788628796` | `1789222298` | `1789222754` |
|---|---|---|---|
| Flame Vents channels cut short | 2 of 13 | **6 of 6** | **8 of 12** |
| Electroshock cast / stopped the channel | n/a | **6 / 6** | **10 / 7** |
| `fl.station = tar-lead` elected | 0 | 1 | 1 |
| `fl.corner` assigned | 0 | 2 | 4 |

Hodir's Fury is handled too: 3 of 4 cleared the fuse in the short pull, **6 of 6** in the long one.
Thorim's Hammer is a non-event — 0.03 % of hull-marker frames inside the blast.

## What actually killed the fleet

### 1. Mimiron's Inferno — the trail, not the head

`npc_mimirons_inferno` (entry **33369**) is an `npc_escortAI` that **walks a fixed waypoint path**
and every **2 s** summons `NPC_MIMIRONS_INFERNO` (entry **33370**), each burning for **30 s**
(`boss_flame_leviathan.cpp:1066-1076`). That is a ~15-patch, 9 yd-wide burning trail behind a
walking head — median **14 patches on the ground at once**, max 15, confirmed from `snap.hz`
(spell 62910, radius 9).

[UldHardMode.cpp:74](src/Ai/Raid/Uld/Util/UldHardMode.cpp#L74) scans `nearest npcs` for entry
**33369 only** — the head. The trail is invisible to the bot.

Hull health lost per 5 s, by distance to the nearest patch edge:

| where the hull was | `1789222298` | `1789222754` |
|---|---|---|
| **inside the fire** | **51.13 %** | **51.64 %** |
| 0-10 yd from the edge | 32.92 % | 14.23 % |
| 10-25 yd | 3.98 % | 1.62 % |
| 25-60 yd | 3.99 % | 1.44 % |
| over 60 yd | 1.77 % | 1.23 % |

It is the fire, not the boss: inside the fire with the boss within 40 yd is 51.47 %, with the boss
far away 51.76 %. **A hull standing in it dies in about ten seconds.**

Share of all hull health the fleet lost:

| | `1789222298` | `1789222754` |
|---|---|---|
| inside the fire | 9.2 % | **33.0 %** |
| within 10 yd of it | 11.7 % | **28.4 %** |
| clear of it | 79.1 % | 38.6 % |

In the long pull **61 % of every hull point lost** went to a hazard the AI cannot see, out of ~6 %
of hull-seconds. Four of five siege engines died between 1:19 and 1:32, each collapsing from
~92 % to dead as the front reached it — e.g. one went 93.6 % at 1:10 → 61.8 % at 1:15 → 11.5 % at
1:20, with no add within 15 yd and the boss 42-86 yd away.

### 2. `FlameLeviathanEngaged` drops the wheel when a vehicle drives away

[UldEncounter_FlameLeviathan.cpp](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp) —
`FlameLeviathanEngaged` requires `bot->IsInCombat()`. Threat on this fight belongs to the **vehicle
creature, not the bot** (the code says so at
[UldActions_FlameLeviathan.cpp:103](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L103)), so
a rider that drives far from the fight simply drops combat. When it does, two things fire at once:

- `FlameLeviathanVehicleMovementMultiplier::GetValue` returns **1.0** instead of 0.0, so `follow`,
  `avoid aoe` and `reach melee` are free to steer a crewed bot.
- `FlameLeviathanDriveAction::Execute` returns false at its own `FlameLeviathanEngaged` gate, so the
  one action that is supposed to own the wheel stops steering.

Measured: **every** accepted `follow` move on a crewed bot happened with the hull far from the boss
— 8 of 8 in the short pull (median 117 yd, min 105), 36 of 36 in the long one (median 121 yd, min
110). 100 % beyond 60 yd in both.

This one latch explains three separate symptoms.

**Corner posting never completes.** Justice is told to post at corner 1 at 33.4 s, closes from
155 yd to 58 yd by 45.2 s — then a `follow` at 45.5 s sends it 178 yd the other way, and it drifts
back out to 159 yd by 65 s before the drive re-takes it. Distance from the assigned post:

| engine | median | min | within 15 yd | first arrival |
|---|---|---|---|---|
| Shadow | 126 yd | 8 yd | 2 % | 1:15 |
| Justice | 209 yd | 57 yd | 0 % | never |
| Bulwark | 139 yd | 56 yd | 0 % | never |
| Totemist | 152 yd | 146 yd | 0 % | never |

**That is the oscillation.** Fine-grain driving is clean — median path/net 1.3-1.5 for siege and
demolishers, 1.5-2.6 for choppers, 6-16 % of windows thrashing. The oscillation is slow, ~30 s per
cycle, and it is this tug-of-war: drive out, drop combat, get yanked back, re-engage, drive out.

**Demolishers fall out of pyrite range.** Same mechanism, same distances.

### 3. Pyrite: the chain works, the range does not

The chain is intact — `Grab Crate` (62479→62482) on a `Liquid Pyrite` container (NPC 33189) gives
+25 energy, and `Hurl Pyrite Barrel` (62490) lays `SPELL_FL_BLUE_PYRITE_DOT` (68605) on the boss,
10 stacks, 10 s. 128 crate grabs in the long pull. This is the raid's main boss damage.

**Hurl Pyrite Barrel is a 10-70 yd spell** (RangeIndex 164, from
`mod-spell-tweaks/data/dbc-reference`). The demolishers are not in that band:

| | `1789222298` | `1789222754` |
|---|---|---|
| crewed frames inside 10-70 yd | 68 % | **42 %** |
| past 70 yd | 27 % | **56 %** |
| demolisher-to-boss distance, median | 52 yd | **75 yd** |
| mean stack held, in band | 4.3 | 4.1 |
| mean stack held, past 70 yd | 3.6 | 2.9 |

Stack held on the boss per demolisher driver:

| trace | driver | mean stack | at 10 | at 0 | longest gap at 0 |
|---|---|---|---|---|---|
| `1789222298` | Hellflame | 7.4 | 56 % | 9 % | 6 s |
| | Nightwarrior | 7.6 | 60 % | 4 % | 3 s |
| | Agony | 1.8 | 0 % | 61 % | 34 s |
| | Stormweaver | 1.6 | 0 % | 62 % | 42 s |
| `1789222754` | Trueshot | 4.7 | 22 % | 38 % | 28 s |
| | Stormweaver | 3.2 | 8 % | 54 % | 50 s |
| | Smartface | 3.0 | 15 % | 57 % | 40 s |
| | Holylight | 2.7 | 16 % | 63 % | **70 s** |

`ULDUAR_FL_DEMOLISHER_BAND = 50` is the right intent, but nothing clamps the *actual* distance, and
`ULDUAR_FL_ARRIVE_TOLERANCE = 8` plus a boss accelerating on Gathering Speed plus the two
`MOVEMENT_FORCED` backoffs push it past 70 routinely.

### 4. Battering Ram is fine; the harness is not

`FlameLeviathanShouldClearBatteringRam` and `FlameLeviathanInBatteringRamBlast` already model the
real thing — a 25 yd sphere on the pursued vehicle plus the boss's own `IsWithinCombatRange(15)`
cast test. Scored against that same test, hull frames inside the blast:

| station | `1789222298` | `1789222754` |
|---|---|---|
| chopper | 17.5 % | 14.5 % |
| demolisher | 17.6 % | 0.9 % |
| siege | 27.6 % | 19.2 % |
| **all** | **20.3 %** | **8.7 %** |

`flame_leviathan.py --ram` still scores a **distance-to-boss** gate the module replaced, so its
"caught 27.9 % / 73.9 % false alarms" describes a gate that no longer exists. Do not trust that
section until it is rewritten.

### Adds, for completeness

Corner posting never ran, so add handling is roughly where the last pass left it: median kill 14 s
(short) / 18 s (long) against 13 s baseline, `Lash` 8.8 % / 4.7 % of raid damage against 4.4 %,
weapon-band coverage 58.5 % / 65.7 % against 82.6 %.

## Plan

### 1. Dodge the Inferno trail, not just the head

[UldHardMode.cpp:74](src/Ai/Raid/Uld/Util/UldHardMode.cpp#L74) — add `NPC_MIMIRONS_INFERNO`
(**33370**) alongside the existing reticle entry under `FL_TOWER_FLAMES`, and add the constant next
to `NPC_FL_MIMIRONS_INFERNO_TARGET` in
[UldEncounter_FlameLeviathan.h:110](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.h#L110). Each
patch is 9 yd; `ULDUAR_FL_TOWER_BLAST_RADIUS = 10` already covers it.

`ClearHazard` ([UldActions_FlameLeviathan.cpp:485](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L485))
steps radially away from **one** hazard. With ~14 patches laid in a line that will often step along
the trail into the next patch. Make the step clear *every* patch within the scan radius: keep the
radial candidate but reject it if it lands inside another tracked hazard, and fan the angle until
one is clear. Keep the existing "already clear, fall through" behaviour so a cleared hazard still
hands the tick to the station.

### 2. Stop the combat latch dropping the wheel

`FlameLeviathanEngaged` ([UldEncounter_FlameLeviathan.cpp](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp))
— test the **vehicle's** combat state, not the rider's, since threat lives on the vehicle creature.
Fall back to the boss's own combat state so a vehicle that has never been hit still counts as
engaged: the pull is live if the boss is in combat and attackable, regardless of who the rider is
fighting. Keep the `TickFlameLeviathan` call and the wipe reset exactly where they are — the
housekeeping depends on it.

This single change restores the veto and the drive action together, at every distance.

### 3. Freeze corner ranks against hull death

`FlameLeviathanSiegeRank` ([UldEncounter_FlameLeviathan.cpp:~540](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp#L540))
ranks **live** siege hulls, so every siege death renumbers everyone below it — Shadow went corner
3 → 2 → 1 → 0 inside five seconds at 1:20 as its neighbours died. Rank over the hulls the instance
started with, latched in `FlameLeviathanState` on first use, so a death leaves a gap instead of
reshuffling the survivors. A corner going unmanned is much cheaper than three engines each starting
a fresh 150 yd drive.

### 4. Hold the demolisher inside the barrel band

`HoldStation` ([UldActions_FlameLeviathan.cpp:534](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L534))
— after computing `goal`, clamp the demolisher's station so the *resulting* distance to the boss
stays inside Hurl Pyrite Barrel's 10-70 yd, with margin for `ULDUAR_FL_ARRIVE_TOLERANCE`. Add a
named constant for the 70 next to `ULDUAR_FL_DEMOLISHER_BAND` rather than spelling it twice.
`DetourToCrate` and the two forced backoffs may still take it out of band; that is correct, they are
emergencies — but the station it returns to must be in band.

### 5. Harness

[tools/botobs/flame_leviathan.py](tools/botobs/flame_leviathan.py):

- Rewrite `show_ram`'s `gate` to the module's real test — `dist(vehicle, pursued) <= 25 + size`,
  gated on the boss's cast range — and drop the distance-to-boss column, which now measures
  nothing. Fix the docstring paragraph that describes it.
- Add `--inferno`: patches on the ground over time, hull and bot frames inside one, hull attrition
  by distance to the nearest patch edge, and the share of total hull loss it accounts for.
- Note in the docstring that `dmg` rows cover roster players only, so hull damage is never
  attributable by spell — hull attrition has to come from `snap` deltas.

### 6. Files

- [src/Ai/Raid/Uld/Util/UldHardMode.cpp](src/Ai/Raid/Uld/Util/UldHardMode.cpp) — inferno trail entry
- [src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.h](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.h)
  — `NPC_FL_MIMIRONS_INFERNO`, the barrel-range constant
- [src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp)
  — `FlameLeviathanEngaged`, `FlameLeviathanSiegeRank`
- [src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp)
  — `ClearHazard` multi-patch step, `HoldStation` demolisher clamp
- [tools/botobs/flame_leviathan.py](tools/botobs/flame_leviathan.py) — `--ram` rewrite, `--inferno`
- [docs/raids/ulduar/flame-leviathan.md](docs/raids/ulduar/flame-leviathan.md) — the Inferno trail,
  and that combat state belongs to the vehicle not the rider
- [docs/engine/pitfalls.md](docs/engine/pitfalls.md) — a rider's `IsInCombat` is not the encounter's
  combat state when threat lives on a vehicle
- `docs/plans/flame-leviathan-freya-adds/` — fold in as iteration 3, with the tables above

Run `/compact-docs-writer` before touching either doc.

## Verification

Not compiled. Needs a hand-off build, then a re-pull with four towers up.

Baseline `603_4_flame-leviathan_1789222754.ndjson`:

| measure | now | target |
|---|---|---|
| hull loss inside/near Mimiron's Inferno | 61.4 % of all hull loss | under 15 % |
| hull frames inside the fire | 1.9 % | near zero |
| accepted `follow` moves on a crewed bot | 36 | **0** |
| siege engines within 15 yd of their post | 2 % of frames, 1 of 4 engines | all four, most of the pull |
| `fl.corner` changes per bot | 4 for Shadow | 1 |
| demolisher crewed frames inside 10-70 yd | 42 % | above 85 % |
| mean Blue Pyrite stack per demolisher | 2.7-4.7 | above 7 |
| longest gap at 0 stacks | 70 s | under 15 s |
| boss floor | 64.0 % | below 36.4 % |
| first siege hull lost | 1:19 | later |
| deaths | 32 | fewer |
| Flame Vents channels cut short | 8 of 12 | hold it |
| Hodir's Fury cleared | 6 of 6 | hold it |
| median time to kill one add | 18 s | 13 s or better |

Commands: `flame_leviathan.py <trace> --adds` / `--ram` / `--fury` / `--vents` / `--inferno`, and
`postmortem.py <trace> --notes fl`.

Watch for:

- **Four engines arriving at once** — the first pull where the posts are actually manned. Check
  what it does to Electroshock coverage; only the rank-0 reserve is meant to stay in cone range.
- **The Inferno dodge fighting the corner drive.** The hazard step is `MOVEMENT_FORCED` and sits
  above the corner branch; a trail laid across a post could pin an engine off it indefinitely.
- **Demolishers clamped into Battering Ram.** The band clamp pulls them closer to the boss; the
  25 yd sphere is on the pursued vehicle, not the boss, so this should be safe, but verify with
  `--ram` once it scores the real gate.
