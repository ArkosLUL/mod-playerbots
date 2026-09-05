# Baseline — Flame Leviathan, 2026-09-05

Frozen measurements taken **before** the Freya-adds work, so the re-pull can be compared against
them. Every number here is reproducible with the committed harness:

```
tools/botobs/flame_leviathan.py <trace> --adds
tools/botobs/flame_leviathan.py <trace> --ram
tools/botobs/flame_leviathan.py <trace> --vents
tools/botobs/postmortem.py      <trace> --notes fl
```

Traces are in `env/dist/logs/botobs/`. Keep all three — the chronicle logs that would corroborate
them rotate away.

| trace | length | outcome | boss floor |
|---|---|---|---|
| `603_1_flame-leviathan_1788623505.ndjson` | 310 s | wipe | 76.0 % |
| `603_1_flame-leviathan_1788623945.ndjson` | 145 s | wipe | 72.2 % |
| `603_1_flame-leviathan_1788624223.ndjson` | 317 s | wipe | **27.4 %** |

`1788624223` is the reference: the raid got furthest, the fleet survived longest, and it is the only
one where the collapse is legible rather than immediate. Hard mode on, Storm and Life towers live.
**No 62297 landed in any of the three**, so the Hodir's Fury thaw shipped in `e41a0e89a` is still
unverified — absence of `fl.frozen` here is not evidence against it.

## Freya's Ward adds — `--adds`

| measure | 1788623505 | 1788623945 | **1788624223** |
|---|---|---|---|
| adds seen | 41 | 29 | **72** |
| travelled from spawn, median | 104 yd | 122 yd | **124 yd** |
| never left 30 yd of spawn | 24 % | 14 % | **4 %** |
| most alive at once, late fight | 11 | 16 | **15** |
| 20 s windows with the field clear after 40 s | 4 | 1 | **1** |
| median time to kill one add | 20 s | 38 s | **22 s** |
| add-frames reachable by some weapon, nobody moving | 34.7 % | 94.7 % | **82.6 %** |
| `Lash` share of raid damage taken | 1.8 % | 42.3 % | **20.0 %** |
| hull hp lost per 5 s, no add within 8 yd | 2.45 % | 1.19 % | **1.96 %** |
| hull hp lost per 5 s, one or more | 4.70 % | 6.13 % | **2.57 %** |

Weapon-band coverage on the reference trace, with nobody moving:

| band | coverage |
|---|---|
| siege turret, Fire Cannon 10–70 | 60.3 % |
| demo gunner, Mortar 0–50 | 52.9 % |
| demo driver, Ram cone 0–15 | 34.0 % |
| demo driver, Hurl Boulder 10–70 | 31.9 % |
| chopper, Sonic Horn cone 0–35 | 25.0 % |
| siege driver, Ram cone 0–15 | 17.8 % |
| **reachable by something** | **82.6 %** |

Adds inside one 20 yd splash: median 2, p75 3, max 9.

All 15 vehicles were destroyed between 188 s and 285 s. Adds landed the killing blow on 7 of the 25
deaths, all after the fleet was gone. All 25 bots were crewed for the whole fight.

## Battering Ram — `--ram`, reference trace

Recorded because the corner posting moves siege engines and could disturb it. These numbers are
**after** the fix in `e41a0e89a`, and are much better than the 2026-08-30 traces that motivated it:

| | 08-30 A | 08-30 B | 09-05 reference |
|---|---|---|---|
| real exposure caught by the backoff | 36.4 % | 34.0 % | **64.6 %** |
| activations that were false alarms | 60.9 % | 54.4 % | **47.6 %** |

Exposure invisible to the backoff, by station: chopper 65 %, demolisher 36 %, siege 15 %.

## Hodir's Fury — `--fury`, reference trace

11 commits, 10 vehicles caught in a circle at fuse start, 8 cleared (80 %), 2 stunned.

## Two defects found while measuring, both outside this plan's scope

1. **The tar lead is never elected.** No `fl.station` note reads `tar-lead` in any of the three
   traces — every chopper reports plain `chopper`. Tar still lands, but only from `ChopperAction`'s
   incidental "he is behind us" branch (188 sampled cast frames).

   **Cause found on 2026-09-05, see the second baseline below: `FlameLeviathanCrewUsable`.** The
   first guess here was `FlameLeviathanTarLeadDistance`'s clamp — lead capped at
   `dist(boss, pursued) - bossReach(15) - BATTERING_RAM_RADIUS(25) - size`, needing the pursued
   vehicle more than ~40 yd out. That clamp is real but was never reached: `FlameLeviathanIsTarLead`
   returns false one gate earlier. It stays untested.
2. **`stations()` in `flame_leviathan.py` keeps only the last `fl.station` per bot**, so a chopper
   that led early and stationed later reads as `chopper` for the whole pull. Harmless for the
   per-station splits above, but it means that table cannot be used to prove a role was *never*
   held — check the raw notes for that.

## Second baseline — `603_1_flame-leviathan_1788628796.ndjson`, 2026-09-05 20:26

339 s, wipe, boss floor **36.4 %**. The first pull carrying `418afbe49`, and the reference for the
crew-usable work. Reproduce with `flame_leviathan.py <trace> --adds` / `--ram` / `--fury` /
`--vents`.

The add work landed:

| measure | 1788624223 | **1788628796** |
|---|---|---|
| adds seen | 72 | 79 |
| median time to kill one add | 22 s | **13 s** |
| `Lash` share of raid damage taken | 20.0 % | **4.4 %** |
| `Lash` hits landed | 67 | 37 |

Nothing gated on `FlameLeviathanCrewUsable` ran at all. `fl.corner` emitted **0** times and
`fl.station` never read `tar-lead`, in this or any Sep-05 trace. `Vehicle::AddPassenger` roots every
passenger (`Vehicle.cpp:437`), so the helper's `UNIT_STATE_NOT_MOVE` test on the rider was false for
every crewed bot from `e41a0e89a` onward.

Flame Vents channels, counting a channel as interrupted when it emits fewer than 11 ticks of 63847
and started with 10 s of pull left (`--vents`):

| trace | date | `tar-lead` elected | channels cut short |
|---|---|---|---|
| 1788099810 | Aug 30, pre-`e41a0e89a` | 3 | **11 of 15** |
| 1788100330 | Aug 30, pre-`e41a0e89a` | 2 | **6 of 11** |
| 1788623505 | Sep 5 | 0 | 1 of 12 |
| 1788624223 | Sep 5 | 0 | 1 of 11 |
| 1788628796 | Sep 5 | 0 | 2 of 13 |

The two Aug-30 pulls only had the Frost tower's helpers up, so they were a lighter fight and the
`out: kill` on 1788100330 is not attributable to the interrupt alone.

Hull attrition while the channel runs, `--vents`: 2.53 %/5 s against 1.99 % otherwise here, 2.43 %
against 1.74 % on 1788624223. `1788623505` inverts it (1.59 % against 3.53 %) — its hull curve
recovers mid-pull, so treat that trace as noise for this measure.

**Why the bots died.** A seated bot carries `UNIT_FLAG_NOT_SELECTABLE` (`Vehicle.cpp:395`, from the
seat flag) and every AoE searcher skips it (`GridNotifiers.h:1154`), so the boss's kit lands almost
entirely on dismounted bots:

| spell | on bots out of a vehicle | on crewed bots |
|---|---|---|
| 63847 Flame Vents | 861,477 (318 hits) | 5,400 (2) |
| 62297 Hodir's Fury | 594,180 (4) | 0 |
| 62376 Battering Ram | 501,478 (21) | 0 |
| 62400 Missile Barrage | 303,227 (106) | 8,909 (4) |

Hulls melted about 25 % faster than the previous pull and the deaths followed:

| | 1788624223 | **1788628796** |
|---|---|---|
| mean hull hp at 60 / 120 / 180 s | 79 % / 54 % / 28 % | 71 % / 45 % / 22 % |
| first bot on foot | 180 s | **80 s** |
| first Flame Vents damage | 228 s | **140 s** |
| first Battering Ram damage | 190 s | **100 s** |
| first death | 189 s | **99 s** |
| deaths | 25 | 31 |
| boss floor | 27.4 % | **36.4 %** |

**Hodir's Fury** fired 11 times before and 20 here. `npc_hodirs_fury` walks a follow path and only
detonates on arrival (`MovementInform(FOLLOW_MOTION_TYPE)`), and two reticles serve the whole fight,
so it arrived twice as often. The fleet was not slower — siege hulls moved 87 % of frames against
80 % — which points at the stationary dismounted bots available from 80 s. Damage went 0 to 594,180,
four hits of 131 k–155 k, none of them on a vehicle in either pull.
