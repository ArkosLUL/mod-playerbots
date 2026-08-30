# Flame Leviathan baseline — 2026-08-30, before the blast/freeze fixes

Produced by the committed tools, so the post-fix comparison is like for like. The plan body quotes
slightly different figures for the same measures because those came from throwaway scripts that
counted the vehicle set a little differently; **these are the numbers to beat.**

Traces (keep them — the chronicle logs for both pulls were rotated away on the 18:01 restart):

| | file | length | outcome |
|---|---|---|---|
| A | `603_1_flame-leviathan_1788099810.ndjson` | 379 s | `idle` — wipe |
| B | `603_1_flame-leviathan_1788100330.ndjson` | 303 s | **`kill`** |

Commands, run from the repo root:

```
python modules/mod-playerbots/tools/botobs/postmortem.py <trace> --stalls
python modules/mod-playerbots/tools/botobs/postmortem.py <trace> --clump
python modules/mod-playerbots/tools/botobs/flame_leviathan.py <trace>
```

## `--stalls` — ordered to move, didn't

| | total | worst single window |
|---|---|---|
| A | **194 s** | Shadow (siege) 2:07.2 → 3:07.2, **60.0 s** at (305.0, −101.1), 14 moves accepted, furthest goal 122 yd |
| B | **107 s** | Fel (demolisher) 2:04.5 → 2:24.6, 20.1 s at (362.2, −16.3), goal 81 yd |

Ecoterrorist froze for the same 60.0 s at (305.2, −101.2) — a passenger in Shadow's engine, which is
what makes it a *vehicle* freeze rather than a bot one. A Hodir's Fury reticle sat 1.3 yd away for
64 consecutive frames.

## `--clump 10` — how much of the pull sat inside one Hodir's Fury

| | ≥4 together | ≥6 together | ≥8 together |
|---|---|---|---|
| A | **62.2 %** | 36.8 % | 18.1 % |
| B | **89.7 %** | 65.1 % | 44.1 % |

## `flame_leviathan.py --ram`

| | frames scored | inside the real 25 yd blast | caught by the shipped gate | gate activations that were false alarms |
|---|---|---|---|---|
| A | 7151 | 1074 | **291 (27.1 %)** | **717 (71.1 %)** |
| B | 7944 | 1233 | **373 (30.3 %)** | **533 (58.8 %)** |

Share of each station's real exposure invisible to the gate:

| | chopper | demolisher | siege | tar-lead |
|---|---|---|---|---|
| A | 71 % | 77 % | 69 % | 75 % |
| B | 68 % | 78 % | 63 % | 75 % |

Lead chopper, distance to the pursued vehicle vs to the boss:

| | to boss (median) | to victim (median) | inside the blast |
|---|---|---|---|
| A | 40.1 | 53.3 | **15.8 %** (246/1554) |
| B | 39.8 | 49.3 | **30.3 %** (253/834) |

The gate is `dist(vehicle, boss) <= 25 + size`; a chopper's CombatReach is 1.0, so it is 26 yd and the
lead chopper never trips it.

## `flame_leviathan.py --fury`

| | commits | in the circle at fuse start | cleared 10 yd in 5 s | caught (60 s stun each) |
|---|---|---|---|---|
| A | 27 | 19 | 16 (84 %) | **3** |
| B | 20 | 36 | 34 (94 %) | **2** |

Median distance from the centre at +5 s: 18.3 (A) / 17.6 (B) yd.
