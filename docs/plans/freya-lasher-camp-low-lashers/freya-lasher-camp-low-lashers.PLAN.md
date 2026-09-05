# Freya: stop the ranged camp fleeing lashers that are not about to blow

## Context

Freya 25 hard mode wiped on 2026-09-05 at 5:43.740 with the whole raid dead and Freya untouched at
100% (`env/dist/logs/botobs/603_1_elder-stonebark_1788635038.ndjson`). The comparison pull is the kill
earlier the same day, `603_1_elder-stonebark_1788613108.ndjson` (7:18.695). Same 25-bot roster, same
code: the worldserver was built 17:07 UTC, after `9ad15ac37`, and `freya tank nature bomb` appears once
in the wipe trace, so the phase-2 Nature Bomb work shipped and is not implicated. `418afbe49` between
the two pulls touches only Flame Leviathan.

**One Detonating Lasher wave failed and the pull died with it.** Every other wave in both traces
cleared before the next spawned:

| trace | wave | result |
|---|---|---|
| kill | DL 0:09 | cleared in 48s |
| kill | DL 4:48 | cleared in 34s |
| wipe | DL 2:05 | cleared in 28s |
| wipe | **DL 3:33** | **5 of 10 in 60s, never cleared** |
| wipe | trio 4:33 | landed on the 5 survivors; nothing else died |

Eonar's Gift was summoned at 3:32.520, one second before the pack landed, and bloomed at 3:44.547
after the raid had taken it from 94% to **10.8%** — it lost the race by about a third of a second. Two
more bloomed (heals at 4:29.568 and 5:15.652); the 4:29 one took the five surviving lashers from
**7-17% back to 60-68%** and ended the pull. The kill trace lost one Gift, this pull lost three.

The mechanism is that the ranged never stopped walking.

**1. The camp is an unwinnable retreat.** `ULDUAR_FREYA_LASHER_CAMP_STANDOFF` (18 yd) is measured
against the *nearest living* lasher, and `GetFreyaLasherCampSpot` re-derives it from the pack centroid
every tick. Detonating Lashers (entry 32918) carry `speed_run 1.14286` in `creature_template` —
**8.0 yd/s against a player's 7.0** — so the standoff cannot be held against a pack that chases. Over
the failed wave the ranged walked **256 yd** to net 19, and the median distance to the nearest lasher
still fell 20.0 → 17.3 → 14.3. The camp destination itself wandered **195 yd of path for 27 yd of net
movement**, re-aiming a median 2.2 yd at a time.

**2. That fights `reach spell`.** Neither is latched (`UldActions_Freya.cpp:674` declines an arrival
latch on purpose), so the two alternate. `freya ranged camp` finished the pull at **450 OK / 444
FAILED**.

| wave | camp orders | `reach spell` orders | ranged moving | ranged damage |
|---|---|---|---|---|
| wipe 2:05 (cleared) | 104 | 0 | 33% | 109,544/s |
| **wipe 3:33 (failed)** | **521** | **270** | **62%** | **51,243/s** |
| kill 0:09 | 229 | 111 | 37% | 91,259/s |
| kill 4:48 | 165 | 4 | 37% | 89,563/s |

A ranged bot casts on **21% of ticks while moving against 78% while standing**, so doubling movement
halved ranged damage. Targeting is not the variable — melee sit at ~55% lasher / ~40% Freya in all four
waves.

**3. The doc already contains the fix and does not apply it.** `docs/raids/ulduar/freya.md` says the
bail "clears only the *low* lashers, never the whole pack, because clearing all ten is the lattice
failure again", and that the camp deliberately sits inside the blast
(`UldEncounter_Freya.h:143`: "The camp is 10 yd inside a 15 yd blast, so the raid does eat each
detonation whole. That is the trade"). The camp nonetheless clears all ten. The 18 yd constant was
fitted to the one clean wave in the four-wave table, and the same doc explains why that number is
confounded: that wave was clean because the pack spawned 3.7-5.2 yd from the melee and seven of eight
were in contact in 2.5s. The distance was an effect of a fast intercept, not something the camp
produced by walking.

Intended outcome: the ranged stop walking during a lasher wave, ranged damage returns to the ~90-110k/s
of the three waves that cleared, Eonar's Gift dies inside its fuse again, and a wave clears before the
next spawns.

## Approach

**Apply the bail's health filter to the camp.** The camp keeps both of its jobs — gather the back line
into one AoE ball, and clear the blast of a lasher that is seconds from detonating — and drops the one
it cannot do, which is outrunning a healthy pack.

Explicitly out of scope by the user's decision: no arrival latch on the camp, and no change to the
Eonar's Gift share (`ULDUAR_FREYA_GIFT_SHARE`, `_GIFT_SHARE_MS`). Both are recorded under Follow-ups.

### A. `GetFreyaLasherCampSpot` — `src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp:522`

- Build a local set of lashers below `ULDUAR_FREYA_LASHER_BAIL_PCT` (15%) once, and use it for **both**
  the centroid and the `CountFreyaLashersNear` clearance test in the outward walk. The bearing sweep,
  the `_CAMP_STEP` walk, the `_CAMP_MAX_STANDOFF` cap and the collision handling are unchanged; they
  just run against the low set.
- The clearance test currently calls the shared `CountFreyaLashersNear`, which counts every living
  lasher. Do the count inline over the local low set rather than changing that helper's signature —
  `freya frost nova lashers`, `freya trap lashers` and `GetFreyaLasherPackFocus` all want the
  all-living semantics and must not shift.
- **When no lasher is low, return the anchor's position instead of `Position()`.** This is what keeps
  the ball formed: the camp becomes a gather point on `GetFreyaRangedCampAnchor` (a live bot, always on
  the mesh), and the existing tolerance stands the trigger down. Returning empty would leave the ranged
  with nothing gathering them for most of the wave and risks losing the AoE the pack focus depends on.

### B. `FreyaRangedCampTrigger::IsActive` — `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp:160`

Replace the unconditional clause

```cpp
if (CountFreyaLashersNear(bot->GetPosition(), state, ULDUAR_FREYA_DETONATE_RADIUS))
    return true;
```

with the low-lasher form, reusing the helper `freya lasher about to blow` already uses:

```cpp
if (!GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, ULDUAR_FREYA_DETONATE_RADIUS).empty())
    return true;
```

This clause fired on 34% of ranged ticks in the failed wave, forcing a re-aim whatever the tolerance
said. The tolerance check below it (`_RANGED_CAMP_TOLERANCE` 10 yd, `_HEALER_CAMP_TOLERANCE` 15 yd) is
unchanged and becomes the normal path.

## Files

- `src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp` — the low-lasher centroid and clearance in
  `GetFreyaLasherCampSpot`, plus the anchor fallback when nothing is low.
- `src/Ai/Raid/Uld/Util/UldEncounter_Freya.h` — reword the `GetFreyaLasherCampSpot` declaration comment
  and the `_LASHER_CAMP_STANDOFF` constant comment: the clearance is owed to the lashers about to blow,
  not to the pack.
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp` — the trigger clause.
- `docs/raids/ulduar/freya.md` — the doctrine paragraph, the confounding in the four-wave table, and
  the wipe's measurements. **Governed doc: invoke `/compact-docs-writer` before editing** (a fresh
  invocation; the one from the phase-2 cycle does not carry over).
- Plan copied to `docs/plans/freya-lasher-camp-low-lashers/freya-lasher-camp-low-lashers.PLAN.md`.

No new constants, no new nodes, no registration changes.

## Verification

The module cannot be linked here, so everything past step 1 is a hand-off.

1. **Static**: `python apps/codestyle/codestyle-cpp.py`, then a per-TU syntax check of the two changed
   translation units against `acore/ac-wotlk-build:master` using
   `/azerothcore/build/compile_commands.json` — mount the working tree's `src` read-only at
   `/azerothcore/modules/mod-playerbots/src`, pull the entry, strip `-c`/`-o`, insert `-fsyntax-only`,
   run from the entry's `directory`; needs `MSYS_NO_PATHCONV=1` in Git Bash.
2. **Build the worldserver in Docker** and confirm the binary's mtime moves past the commit
   (`docker exec ac-worldserver ls -l --time-style=+%F_%R env/dist/bin/worldserver` reports UTC, the
   host is UTC+3).
3. **Re-pull Freya 25 hard mode** and measure the Detonating Lasher waves against `1788635038`'s failed
   wave (3:33-4:33):
   - `freya ranged camp` move orders **521** and `reach spell` **270** → both should fall toward the
     104 / 0 of the wave that cleared.
   - Ranged moving share **62%** → ~33%.
   - Ranged damage **51,243/s** → 90-110k/s.
   - Camp destination path **195 yd for 27 yd net** → down.
   - `freya ranged camp` verdicts **450 OK / 444 FAILED** across the pull → the split should stop being
     a coin flip.
   - Eonar's Gift lifetime **14.5s** → the 4-8s of the waves that cleared, and **zero**
     `Lifebinder's Gift Heal` casts while a pack is up (three in this trace).
   - Every lasher wave cleared before the next spawns: 10/10, not 5/10.
4. **Guard.** The raid will now stand inside Detonate range of healthy lashers on purpose. If ranged or
   healer deaths rise *while the waves clear*, the answer is a standoff against all lashers only within
   melee contact range — **not** restoring the 18 yd blanket, which the speed arithmetic rules out. If
   the waves stop clearing again, this change is wrong and should be reverted whole.
5. `python tools/botobs/postmortem.py <trace>`, plus `--clump` and `--stalls`.

## Follow-ups, not in this change

- **No camp latch.** If the next trace still shows `freya ranged camp` near 50/50 OK/FAILED during the
  finish, when several lashers are low at once, the latch idiom used by the bomb escape and the sun
  beam dodge (`spot`/`spotMs`, return `true` while in flight) is the next lever.
- **Eonar's Gift share unchanged.** `TakesEonarsGift` starts its 5s head start when the *pack* spawns,
  not when the Gift appears, so a Gift summoned just before a wave spends nearly its whole 12s fuse
  with five ranged held off it. The bet here is that ranged standing still kill it inside 5s anyway;
  Gift lifetime in the next trace is the test.
- **Nothing intercepts a fresh pack**, still open and still documented in `docs/raids/ulduar/freya.md`.
- `freya move to healing spore action` ran 242 OK / 251 FAILED in this trace — a second unlatched
  oscillator, around the Conservator, not touched here.
- `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` is 12.0 in code but the trace records beams at 5.0; still
  needs a DBC check.
