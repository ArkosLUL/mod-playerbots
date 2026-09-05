# Baseline — Flame Leviathan, 2026-09-05

Frozen measurements taken **before** the Freya-adds work, so the re-pull can be compared against
them. Every number here is reproducible with the committed harness:

```
tools/botobs/flame_leviathan.py <trace> --adds
tools/botobs/flame_leviathan.py <trace> --ram
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
   traces — every chopper reports plain `chopper`. `FlameLeviathanTarLeadDistance` clamps the lead
   to `dist(boss, pursued) - bossReach(15) - BATTERING_RAM_RADIUS(25) - size`, so the pursued
   vehicle has to be more than ~40 yd from the boss for a lead position to exist at all, and while
   he is chasing it rarely is. The clamp shipped in `e41a0e89a` and appears to have closed the role
   it was meant to protect. Tar still lands, but only from `ChopperAction`'s incidental "he is
   behind us" branch (188 sampled cast frames).
2. **`stations()` in `flame_leviathan.py` keeps only the last `fl.station` per bot**, so a chopper
   that led early and stationed later reads as `chopper` for the whole pull. Harmless for the
   per-station splits above, but it means that table cannot be used to prove a role was *never*
   held — check the raw notes for that.
