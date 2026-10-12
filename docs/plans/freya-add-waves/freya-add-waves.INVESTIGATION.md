# Freya 2026-10-11 - two resets and a kill: what went wrong, what to change

## Context

Post-mortem of the three Freya pulls of 2026-10-11. Traces in `env/dist/logs/botobs/`, all 25-man,
hard mode ON, build 2026-10-11 10:38 UTC - the first pulls on a binary that contains `1da20d43b` (the
lasher-aware escapes, the pinned-bot guard, the tremor hold).

| trace `603_44_elder-stonebark_*` | outcome | wave order | deaths before the end |
|---|---|---|---|
| `1791742873` (A1) | reset at 1:27, **18/25 alive** | trio, lashers | 8 |
| `1791743089` (A2) | reset at 2:39.9, **19/25 alive** | trio, Conservator, lashers, trio | 10 |
| `1791743444` (K) | **kill 6:33** | lashers, Conservator, trio, trio, lashers, Conservator | 1 |

Neither reset was a wipe the raid lost: both were called by the human (A1 170 ms after both humans
died, A2 with all four healers up). Humans: `Felesta` (DK **tank**) and `Deathsong` (DK melee). The only
bot tank, `Bulwark`, holds Freya, so the human is the add tank and no bot runs the Snaplasher ladder.

Full combat log for all three: `chronicle-app-1:/root/.chronicle/storage/raidlogs/logs/4a60af0e-fd53-46c6-b1da-73747c09fd98`
(epoch-ms timestamps; pull t0 = 1791742874728 / 1791743091118 / 1791743446079).

**No source has been changed.** Sections 1-4 are proposed work, each a separate go-ahead.

## `1da20d43b` verified

- **Lasher-aware escapes work.** Of 47 back-line `freya step out of sunbeam` / `freya dodge unstable
  sun beam` orders issued while a lasher was under 15%, **0** aimed inside 15.5 yd of one (nearest
  18.4 yd; medians 18.8-28.6). Before the fix five bots were sent to one spot 9-10 yd out.
- **Tremor hold runs.** `freya ground tremor hold cast` went from `LOST` (164 pushes, 0 runs) to `ok`:
  fired 94x on 7 bots, **ran 31x**.
- **Pinned-bot guard:** no death under an escape latch while rooted in any of the three.
- **No output regression:** kill 6:33 against 7:03, lasher waves cleared in 23.5s and 29.2s against
  28.4s and 26.5s, 812k damage taken a minute against the 899k reference kill.

## 1. The ranged camp parks the back line inside the blast (defect, every lasher wave)

`FreyaRangedCampAction::Execute` ([UldActions_Freya.cpp:985](src/Ai/Raid/Uld/Action/UldActions_Freya.cpp#L985))
aims `ULDUAR_FREYA_RANGED_CAMP_SPACING` (6 yd) short of the camp spot **toward the bot**. The spot is
`ULDUAR_FREYA_LASHER_CAMP_STANDOFF` (18 yd) from the low lashers on the raid's own bearing, so a bot
standing between pile and camp - the normal case - is sent to 18 - 6 = **12 yd**, inside Detonate's 15.
The trigger keeps firing there (low lasher inside 15 yd) and `MoveTo` answers `there` / `wait`.

| wave | camp orders while a lasher is low | destination to nearest low lasher | inside 15.5 yd |
|---|---|---|---|
| A2 1:56 | 189 (106 accepted) | median 13.6, min 11.8 | **80%** |
| K 3:35 | 66 (34 accepted) | median 12.5, min 12.0 | **100%** |

Cost: A2 back line 10-11 of 12-13 inside the blast for 11s, five blasts of 14-18 victims, **48
back-line Detonate hits**, `Smartface`, `Power`, `Hellflame` dead. K's first lasher wave shows the
target state by accident - the back line stood 25 yd off a ghoul-held pile and took **0 of 70** hits.

**Fix.** When a low lasher exists, the destination itself must clear every low lasher by the
standoff. Keep the ring only for the no-low-lasher anchor ball it was written for; on the low-lasher
branch send each bot out on **its own bearing from the low-lasher centroid** to the first point past
`_LASHER_CAMP_STANDOFF` (same collision + per-lasher clearance loop `ComputeFreyaLasherCampSpot`
already runs, [UldEncounter_Freya.cpp:682](src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp#L682)). Own
bearings keep the spacing without a shared point. On that branch also call
`FreyaClearCastBlockingMove` - it is a bail, and a mid-cast `MoveTo` is a silent no-op.
Melee and tanks stay excluded; nothing zeroes `ReachTargetAction`.

*Optional, after measuring the fix:* hold the back line at the standoff from the whole pile while the
pack is **held** (most lashers' victims are non-players). That is K wave 1's geometry and it is not
the `1788635038` retreat, because a ghoul-held pile does not chase.

## 2. Army of the Dead is the lasher wave, and it was missing or late

| wave | Army | lashers on ghouls at +5.5s / +10s | lasher melee + Flame Lash on raid | deaths |
|---|---|---|---|---|
| A1 1:06 | **none** | 0 / 0 | **416k** in 20s | 5 (both humans) |
| A2 1:56 | bot, cast at **+6.1s** | 0 / 8 | 102k | `Malediction` at +6.5s, 4 lashers on her |
| K 0:10 | bot, cast at +0.2s | 9 / 10 | 15.7k | 0 |
| K 3:35 | both humans, +2.1s | 7 / 10 | 0.3k | 0 |

The doc's "ghouls hold 12.5-18.9% of lasher attention" is far too low: on time, they hold 70-100%.

- **A1:** `Obliteration` (the one bot DK) spent Army on Auriaya at 18:13:35, 8m45s before the wave.
- **A2:** `freya summon army` FAILED at +1.35s and landed at +6.05s; in between the DK cast Icy Touch,
  Howling Blast and Frost Strike. Army costs one rune of each kind, so the rotation kept eating them
  as they came back. Rune state is not in the trace - treat as the working hypothesis.

**Fix.** `FreyaArmyRuneReserveMultiplier`, in the shape of `FreyaLasherTrapReserveMultiplier`
([UldMultipliers_Freya.cpp:95](src/Ai/Raid/Uld/Multiplier/UldMultipliers_Freya.cpp#L95)): for a DK with
lashers alive and Army off cooldown, zero every action whose spell has a rune cost. Bounded by the
rune cooldown, on one bot.

**Research before building:** a second taunt sink for the pull's second lasher wave, which the bot DK
cannot cover (10 min cooldown) - in K the humans did. Candidates must pass the three checks in
`CLAUDE.md` (spellbook, DBC, trigger chains) before anything keys on them. Wave order is knowable:
each block of three holds one wave of each kind, so the third is certain.

## 3. Snaplasher one-shots whoever takes it off the tank

**Hardened Bark sits at 99 stacks from +13-16s of every trio wave** (25-man ids **64191** passive /
**64190** stack; the doc lists the 10-man pair and says the split "holds it far below its cap" - false).
Stacks at +2/+4/+6/+8/+10/+15s: 7/25/43/61/78/99. Swing every 2.45s: **57-120k** on a non-tank,
22-50k raw on the DK tank, who avoided 76% (13 of 54 swings landed).

Deaths to it, both resets: `Mighty` (cast **Mocking Blow** on it at 64 stacks, dead 270 ms later),
`Angry` and `Trueshot` (out-threatened the human), `Felesta` (three landed swings in a row),
`Justice` and `Shadow` (no tank up). In K the tank held it 41.2 of 42.7s and 48.5 of 50s; nobody died.

- **3a. Mocking Blow is a taunt, wired as a snare.** `piercing howl` falls back to `mocking blow`
  ([ArmsWarriorStrategy.cpp:47](src/Ai/Class/Warrior/Strategy/ArmsWarriorStrategy.cpp#L47)). Point the
  fallback at `hamstring`; check the Fury node at `FuryWarriorStrategy.cpp:44` for the same.
- **3b. Shed to a sibling near the top of its threat table.** The generic `threat` strategy is never
  added (`AiFactory.cpp`), so nothing stops a bot overtaking. In
  `FreyaSetDpsPriorityAction::ResolveFreyaDpsTarget` ([UldActions_Freya.cpp:381](src/Ai/Raid/Uld/Action/UldActions_Freya.cpp#L381)):
  when the trio slot is the Snaplasher and the bot is not a tank, compare
  `snaplasher->GetThreatMgr().GetThreat(bot)` with the highest living tank's (the read
  `ThreatValues.cpp:48` already does); inside a named margin, take the non-suppressed sibling with
  the most health instead. No damage is withheld - it moves, as the sync band already does. Release
  the cap when no tank has threat a few seconds after the bot first sees that Snaplasher, so an
  untanked one still dies.
- **3c. Redirected shots land on the wrong table.** `FreyaRedirectThreatAction::Execute`
  ([:617](src/Ai/Raid/Uld/Action/UldActions_Freya.cpp#L617)) always dumps Steady Shot into **Freya**,
  also when the redirect tank is the add tank - so the add tank gains Freya threat and nothing on the
  Snaplasher. Dump on the claimed add (Snaplasher, else Conservator) in that case.

## 4. One bomb escape returned false for a whole fuse (the kill's only death)

87 of 88 exposed bot-volleys escaped, median **0.61s**. `Power` at 6:17.2 did not: bomb at 0.0 yd,
three more inside 10, `freya move away nature bomb` FAILED on four ticks running, **no move order
issued**, not rooted or stunned by any aura - so every sweep tier and `StepOffNatureBombs` came back
empty. Bot at (2356.27, -37.18, 425.17); bombs at (2356.3,-37.2) (2353.2,-41.5) (2352.8,-46.0)
(2352.7,-45.0) (2348.9,-49.3) (2348.2,-48.7) (2373.3,-53.0). The step-off bearing points at y -28,
past the room's solid floor. **Diagnose first:** `navprobe` the ring candidates from that spot, then
make the last tier try other bearings instead of one.

## Not proposed

- The human tank's death in A2 is variance at 99 stacks. External cooldowns could cover it through
  `RaidTankDefensive`, which another session is editing.
- Holding Misdirection for the 0:10 wave: unproven, and 3c matters more.
- Withholding damage from the Snaplasher: tried, the trio never died together (`freya.md`).

## How these were measured

- Who a creature is on: `snap.u[7]` per snapshot, joined to `unit` rows by entry (Snaplasher 32916,
  Detonating Lasher 32918, Nature Bomb 34129, Army ghouls by name).
- Camp and escape destinations: `move` rows by their `by` field, against the nearest lasher under 15%
  in the snapshot at the order's time.
- Blast victims: `dmg` rows with `sp` 62937 grouped by source; Flame Lash is `sp` 62608.
- Hardened Bark stacks and the swing table: the combat log, `SPELL_AURA_APPLIED` 64190 counted since
  the last `SPELL_AURA_REMOVED`, and `SWING_DAMAGE` / `SWING_MISSED` from the Snaplasher. botobs `aura`
  rows cover roster targets only, so they show no stacks.

## Verification

1. `~/.claude/scripts/pb-syntax-check.sh` on changed `.cpp` plus includers of changed `.h`, then
   `tools/pblint/pblint.py` on the same paths, then `python apps/codestyle/codestyle-cpp.py`.
2. One hard-mode pull, then:
   - camp: no `freya ranged camp` destination inside 15.5 yd of a lasher under 15%; back-line Detonate
     hits per wave toward K wave 1's 0.
   - Army: cast inside 1s of the pack spawning; lashers on ghouls at +5.5s.
   - Snaplasher: no non-tank death to it; `mocking blow` absent from the warriors' act rows; trio
     wave still clears inside its 60s and the three still die within the 11s window.
   - `postmortem.py <trace> --coverage freya` for the new multiplier and nodes.
3. Doctrine guard, unchanged: lasher wave clear time and raid damage a minute must not get worse.

## When this ships

Fold what corrects `docs/raids/ulduar/freya.md` (Hardened Bark at cap, the 25-man ids, Army's real
share, the `1da20d43b` results) through `/compact-docs-writer`, then delete this file.
