# Thorim: move the phase-2 camp west so the Lightning Charge cones miss the ranged

Trace: `env/dist/logs/botobs/603_1_thorim_1788727248.ndjson` (2026-09-06 23:45, 25-man, hard mode,
wipe at 4:43, 30 deaths). Read with
`python modules/mod-playerbots/tools/botobs/postmortem.py <file>`. **Timestamps are milliseconds.**
The 23:46 file beside it is the 15 s corpse cleanup, not an attempt.

Build carries `390d0cc4a`, confirmed from the trace: the `thorim.lightningorb` note key only exists in
that build.

---

## Context

### `390d0cc4a` did what it was meant to

- **Traps: zero.** No Paralytic Field on anyone, against 18 last time. All 12 gauntlet bots latched
  fresh at step 0 and walked `0→1→2→3→4→5→6`; all 8 melee got a fresh `thorim.slot`.
- **Blizzard: zero damage**, against 123,539 on eight ranged and healers before.
- **Melee stopped churning:** 347 → 254 yd/min, moving 86% → 63% of snapshots.
- **Ranged hold their anchors** — they die standing exactly on all five.

None of that needs revisiting. What is left is one thing.

### Lightning Charge is killing the raid

Phase 2 opens 3:06.8, Sif joins 3:17.8, wipe at 4:43 with Thorim on 72.1%. Over those 96 s:

| source | damage | share |
|---|---|---|
| Sif Frostbolt Volley 62604 | 634,154 | 24.6% |
| **Thorim Lightning Charge 62466** | **492,708** | **19.1%** |
| Thorim melee | 456,397 | 17.7% |
| Thorim Chain Lightning 64390 | 230,526 | 8.9% |
| Sif Frostbolt 62601 | 220,178 | 8.5% |
| Thorim Lightning Shock 62017 | 192,234 | 7.5% |
| Sif Frost Nova 62605 | 112,316 | 4.4% |

**26,790 incoming dps against 26,021 hps** — healing is behind from the first second. Sif is 38% of it;
without her it is 16,742 against 26,021, comfortable, so hard mode is why this is a wipe.

Lightning Charge is the killing blow. Two mass deaths, four inside 30 ms at 3:52.9 and six inside
80 ms at 4:07.9, and it is the top line in the last four seconds of **every** victim at 15-21k each.

### How the cone actually works, checked against this trace

Thorim faces the charging orb and casts 62466 at it (`boss_thorim.cpp:604-610`), and `spell_cone`
gives it a 75° arc. Decoding all 33 hits against the boss→orb bearing at the moment of each:

- angle off the boss→orb line: **max 43.8°, mean 17.3°** — every hit inside the modelled cone
- distance from the victim to the lit orb: **17.3 to 56.5 yd, mean 37.4**

So it is a cone anchored on Thorim, not a field around the orb. Being on the side of the room with
fewer towers does not help by itself; what decides exposure is your **bearing from Thorim**.

### The proposal, and where it lands

The idea was to move the tank so the ranged sit east, near only 3 towers instead of 4. Two parts:

- **The Blizzard half is already done.** Blizzard does zero damage now that the anchors are off its
  walk, so there is nothing left for a tank to soak, and (2110.75, −252.65) is 4.1 yd from that walk —
  it would put damage back on the tank for nothing.
- **The cone half is right, and I first tested it wrong.** I scored spots by whether *no* cone reaches
  them, which nothing in the arena satisfies, and concluded the idea failed. The metric that matters
  is **how many of the seven cones cover the parking spot**, which is exactly the 3-versus-4 framing:

  | layout | mean cones covering the ranged |
  |---|---|
  | today | **3.6 - 4.2 of 7** |
  | boss at (2110.7, −252.7), ranged due east | 2.8 |
  | boss at (2110.7, −252.7), ranged east-south-east | 2.2 |
  | **recommended below** | **1.0** |

Pulling the boss west and parking the ranged away from him is the right move. The refinement is that
he does not need to go as far as (2110.7, −252.7), and the ranged want to be **south**-east of him
rather than due east, because due east lines them up with the three eastern towers.

**This replaces the ranged cone dodge.** Placement takes ~75% of the Lightning Charge exposure for no
movement at all; the dodge cost roughly 500 yd of walking per ranged bot per phase and, with only 1.0
cone left to dodge, no longer earns that. Say the word if you still want it on top and it is additive.

---

## The change

**Files:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.h` and `.cpp` — seven `Position` constants and
the comment blocks above them. No logic changes, no new node, nothing in `UldStrategy.cpp` or the two
context headers.

Every coordinate below is navprobe-verified on map 603 (distance-to-poly ≤ 0.32, Z is
`UpdateAllowedPositionZ`).

### Tank spot, which is where Thorim ends up

`ULDUAR_THORIM_PHASE2_TANK_SPOT`: (2134.8572, −287.0291) → **(2122.0, −263.0, 419.724)**

13.0 yd from the arena centre, 17.5 yd off the Blizzard walk, comfortably inside the script's
`GetArenaPlayer` scan box. The melee ring at radius 8 around it is on the floor the whole way round
(probed at 0°, 90°, 180°, 270°), and the gauntlet squad still lands 20 yd away at JUMP_END.

`ULDUAR_THORIM_PHASE2_OFFTANK_SPOT`: (2137.9, −287.0) → **(2126.0, −267.0, 419.791)**, keeping it the
same few yards off the tank. It is only the fallback for when `RingPoint` cannot produce an off-tank
point, but leaving it 26 yd from the new tank spot would be wrong.

### The five ranged anchors, south-east of him

| slot | x | y | z | cones | Blizzard | from boss |
|---|---|---|---|---|---|---|
| RANGE1 | 2136.0 | −271.0 | 419.827 | 1 | 26.5 yd | 16.1 |
| RANGE2 | 2146.0 | −273.0 | 419.678 | 1 | 16.3 yd | 26.0 |
| RANGE3 | 2129.0 | −276.0 | 419.695 | 1 | 20.7 yd | 14.8 |
| RANGE4 | 2139.0 | −282.0 | 419.581 | 1 | 22.6 yd | 25.5 |
| RANGE5 | 2131.0 | −284.0 | 419.540 | 1 | 22.6 yd | 22.8 |

Every one is covered by exactly one of the seven cones, the south-eastern tower. Tightest pair 8.2 yd,
clear of Chain Lightning's 5 yd jump. Worst Blizzard clearance 16.3 yd, better than the 16.2 today.
All 14.8 to 26.0 yd from the boss, so casters stay in range and out of the melee ring.

Rewrite the comment block above the anchors: it currently explains the wedge as a Blizzard-driven
layout that deliberately ignores Lightning Charge. Both halves change — say that the camp sits in the
boss's south-eastern shadow because that is the one bearing the tower ring barely reaches, and that
the Blizzard walk is still the second constraint.

## Deliberately not doing

- **No ranged cone dodge.** Superseded, see above.
- **Nothing about Sif**, who is 38% of the incoming. Frostbolt Volley is DBC radius 200 and hit 11-20
  of 25 with victims 41-48 yd apart, so there is no positional answer. If hard mode still wipes after
  this, that is the next thing to look at and it will not be a placement fix.
- **Lightning Shock 62017** (7.5%, 91 hits at ~2.1k) is chip damage on everyone with no dodge.
- **No change to the melee ring, the balcony route, or the state reset.** All measured healthy.

## Verification

Static:

- `grep -n "PHASE2_TANK_SPOT\|PHASE2_OFFTANK_SPOT\|PHASE2_RANGE" src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp`
  shows the seven new values and nothing else changed.
- `python apps/codestyle/codestyle-cpp.py` passes; no line over 120 columns; files stay LF.
- Per-TU syntax check in the `acore/ac-wotlk-build` image against
  `/azerothcore/build/compile_commands.json` — mount `modules/mod-playerbots/src` read-only, take the
  entry for the changed file, drop `-c` and `-o`, add `-fsyntax-only`. Expect only the two
  pre-existing `-Wunused-parameter` warnings. It does not link.

In game, one 25-man hard-mode pull, then re-read the trace:

1. **Lightning Charge 62466 on ranged and healers falls by roughly three quarters** from the 492,708
   the raid took here. This is the headline.
2. **Phase-2 incoming drops below healing.** Was 26,790 against 26,021 hps.
3. **Thorim actually sits at (2122, −263).** Check his snapshot position through phase 2 — if the tank
   cannot drag him there the whole thing is moot.
4. **Nobody is off the floor.** No bot below y −288 or past 34 yd from (2134.99, −263.12), no Z near
   −27.7.
5. **Ranged stay spread and parked.** Tightest live pair near 8.2 yd, movement no worse than the
   20-27% of snapshots seen here.
6. **Regressions.** Paralytic Field stays at zero, Blizzard stays at zero, every gauntlet bot still
   emits a `thorim.balcony` note starting at 0, every melee still gets a fresh `thorim.slot`, and
   melee travel stays near 254 yd/min.
