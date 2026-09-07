# Freya P2: stop bots walking back onto a live Nature Bomb

## Context

`e8e47721d` did what it was built for. Latest kill
`env/dist/logs/botobs/603_1_elder-stonebark_1788724466.ndjson` (8:50, 12 deaths) against the previous
kill `1788613108` (7:18, 7 deaths), same 25-man roster, same wave composition (20 lashers + 6 trio +
2 conservators = 150 Attuned stacks). Both ran a binary built after their respective commits: the
worldserver mtime is 19:13 UTC = 22:13 local, two minutes after `e8e47721d` landed at 22:11:17.

| | prev kill | the wipe `1788717562` | this kill |
|---|---|---|---|
| Sunbeam victims per volley (median) | 4.5 | 12.5 | **2.5** |
| Sunbeam victims (max) | 14 | 16 | **7** |
| Nature's Fury allies inside 8 yd | 8.0 | 13-17 | **0.5** |
| Conservator's Grip pacify uptime, ranged | 13.1% | - | **2.4%** |
| biggest single-spore group | 23 | 18 | **12** (the melee group, exempt by design) |
| Freya from anchor, p90 | 24.9 | - | **18.1** |
| Attuned stacks per minute | 27.9 | 24.4 | **26.2** |

P2 is already safer per unit of exposure: 717,033 -> **552,014** damage taken per minute, deaths
2.6/min -> **1.9/min**, no bot ever took 4 bombs at once (was 4 occurrences), and bombs landing
inside another bomb's blast dropped from a median of 3.0 to 1.0. What grew is the *amount* of P2 -
1:56 -> 3:07 - because four bots died in one five-second window at 3:12-3:17 in P1. **The user has
ruled P1 out of scope**; this plan is P2 only.

Three of the six P2 deaths are a bot at full health dying to a simultaneous multi-bomb detonation.
Those are the target.

### Nature Bomb, measured

- Every **18 s** Freya drops a bomb at the feet of `urand(7, 10)` players within 70 yd (25-man,
  `boss_freya.cpp` `EVENT_FREYA_NATURE_BOMB`). Blast is `64650`, `EffectRadiusIndex 13` = **10 yd**.
- **The fuse is 6 seconds, not the 11 the constant suggests.** `boss_freya_nature_bomb::UpdateAI`
  explodes at `_explodeTimer >= 11000`, but the branch below it snaps the timer from 5000 straight to
  10000, so ~5 s of counting plus one more tick. The trace agrees exactly: a bomb is first seen 6.0 s
  before its detonation (min 5.9, max 6.1, n=42).
- `ULDUAR_FREYA_NATURE_BOMB_LATCH_MS` is **3000** - half the warning.

**Failure mode 1: the escape gives up and the bot stands in the blast.**
`freya move away nature bomb` returned FAILED **95** times; in **15** of those the bot was inside a
blast and issued no move at all for the next 3 s. All three fallbacks in
`FreyaMoveAwayNatureBombAction::Execute` demand clearance from *every* bomb within 30 yd (13 yd, then
13 yd, then 12 yd). With 7-10 bombs a volley there is often no such spot, and the action returns
false rather than settling for less.

> **Hellflame** (ranged), 8:23.605, **100% -> dead**, 3 bombs for 26,150. A bomb at **0.0 yd** and
> three more at 4.9 / 8.4 / 9.7. One `freya move away nature bomb rel=64.0 -> FAILED` at 8:18.898 and
> then nothing: **zero move records in its last 34 seconds.**

**Failure mode 2: the escape works, then combat movement walks the bot back inside the fuse.**
**51** bomb victims re-entered a live blast, a median **5.3 s** into the 6 s fuse - about 0.7 s
before it fired. Movers in the 2.5 s before re-entry: `freya move away nature bomb` 40,
`reach melee` 32, `reach spell` 20. The latch stops holding the moment the bot arrives
(`GetExactDist2d(bombSpot) > CONTACT_DISTANCE`), and at 13 yd out the 11 yd trigger goes quiet, so
the node is not even reached again.

> **Shadow** (melee), 6:16.851, **99.4% -> dead**, 3 bombs for 26,344. Escaped at 6:10.939, clear at
> 13.5 yd by 6:15.034, `reach melee` at 6:15.015, four bombs within 10 yd (6.4 / 6.4 / 6.9 / 9.6) by
> 6:16.107, dead 0.7 s later.

**A blanket hold shell is ruled out on measurement.** Widening the trigger so any bot near a bomb
holds station would freeze a third of the raid whenever bombs are live: with a bomb up, bots sit in
the 10-14 yd band **33.9%** of the time (melee 34.0%, healers 41.8%). That is the reverted
Detonating-Lasher-spread mistake again. The hold has to apply only to bots that actually fled.

### Iron Roots has never once broken

`FreyaBreakIronRootsTrigger` checks `SPELL_IRON_ROOTS_DAMAGE` (62283) and
`SPELL_IRON_ROOTS_FREYA_DAMAGE` (62861). Both are **10-man** ids. `acore_world.spelldifficulty_dbc`
maps 62283 -> **62930** and 62861 -> **62438** for 25-man, and both traces only ever show **62438**
(33 applications this pull, 27 in the reference kill). `freya break iron roots` does not appear in
either trace's action log: the node has never run.

The root is permanent (`dur=-1`) until the Iron Roots creature is killed. Most break inside a second
on incidental AoE, but **7 held 2 s or longer** this pull and 2 of those had the bot inside a hazard.
One was fatal:

> **Tree** (healer), 4:36.456. Two Unstable Energy pools at 4:32.708 and 4:33.211, **Iron Roots 62438
> at 4:33.953**, `freya dodge unstable sun beam -> FAILED` at 4:33.971, its next two move attempts
> rejected (`ok=0`), dead at 4:36.456 still holding the root.

Intended outcome: a bot at full health stops dying to a bomb volley, and a rooted bot frees itself.

## Approach

### A. The bomb escape never gives up - `FreyaMoveAwayNatureBombAction::Execute`, `UldActions_Freya.cpp:75`

After the three existing tiers, add a last resort that only has to leave the blasts the bot is
actually standing in, not clear every bomb on the field:

1. `FindNearestPositionClearOfHazards(bot, bombs, ULDUAR_FREYA_NATURE_BOMB_BLAST_RADIUS, ...)` - new
   constant, **10.0f**, the real blast, no margin.
2. Still nothing: step directly away from the centroid of the bombs covering the bot, distance
   `_BLAST_RADIUS + 1`, landed with `Map::CheckCollisionAndGetValidCoords`.

The helper's own comment already licenses this - *"a bot that has to cross a hazard to leave one is
still better off out the far side than standing still"*. Out of one beats standing in four.

### B. Hold the escape spot for the fuse, but only for bots that fled

- **Trigger** `freya near nature bomb` (`UldTriggers_Freya.cpp:20`): widen from
  `ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS` (11) to `ULDUAR_FREYA_HAZARD_SEARCH_RADIUS` (30), so the
  node is still reached after the bot has stepped clear. Widening the trigger alone strands nobody -
  the action decides.
- **Action**: remember `bombOrigin`, the position the bot fled from, alongside the existing
  `bombSpot`/`bombSpotMs`. Three branches:
  - a bomb within `_AVOID_RADIUS` of the bot -> escape, as today plus A;
  - otherwise, if `bombSpotMs` is live, **the bot is melee**, the spot is still clear of **every**
    hazard in `GetFreyaEscapeHazards` (not just bombs - the sun beam dodge sits at the same relevance
    and would otherwise fight this one for the tick), and a bomb is still within `_AVOID_RADIUS` of
    `bombOrigin` -> **return true without moving**, which claims the tick and keeps `reach melee` /
    `reach spell` from walking the bot back;
  - otherwise clear the latch and return false, so a bot that never fled drops straight through to
    its DPS ladder.
- Raise `ULDUAR_FREYA_NATURE_BOMB_LATCH_MS` from 3000 to **7000**, covering the 6 s fuse plus the
  walk, and bounding the hold if the bomb GO vanishes without a detonation record.

**Melee only, and that was not in the first draft of this plan.** `Engine::DoNextAction`
(`src/Bot/Engine/Engine.cpp:164`) breaks out of the action queue on the first action that returns
true, so a bot held this way casts nothing at all for as long as the hold runs. For melee that is
free - they are outside melee range at the escape spot either way, and the alternative is walking
back to die - but for ranged and healers, who can cast from where they stand, it would be a straight
silence. So the hold is gated on `PlayerbotAI::IsMelee`. It also fixes the right deaths: both
full-health bomb deaths that came from walking back were melee (Shadow, Totemist), and the ranged one
(Hellflame) is fixed by A.

### C. Iron Roots 25-man ids - `UldEncounter_Freya.h:70-71`, `UldTriggers_Freya.cpp:110`

Add `SPELL_IRON_ROOTS_DAMAGE_25 = 62930` and `SPELL_IRON_ROOTS_FREYA_DAMAGE_25 = 62438` and check
all four in `FreyaBreakIronRootsTrigger::IsActive`. Note in the header *why* both are listed, with
the `spelldifficulty_dbc` mapping - the same trap is one edit away for every other id in this file
(Ground Tremor, Nature's Fury, Sunbeam and Unstable Energy already carry both).

### D. The spore choice skips a pooled spore - `GetFreyaTargetSpore`, `UldEncounter_Freya.cpp:278`

Alongside the `ULDUAR_FREYA_SPORE_CROWD` cap, skip a spore with a Sun Beam pool inside its 6 yd
aura - reuse `GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS)`, the same helper
`GetFreyaEscapeHazards` already uses. Applies to the roomy pick and to the melee parked spore's
fallback; when every spore is pooled, keep today's nearest-spore answer, because pacified is worse.

## Files

- `src/Ai/Raid/Uld/Util/UldEncounter_Freya.h` - `ULDUAR_FREYA_NATURE_BOMB_BLAST_RADIUS`, the raised
  `_LATCH_MS`, the two 25-man Iron Roots ids.
- `src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp` - the pooled-spore skip in `GetFreyaTargetSpore`.
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp` - the widened bomb trigger, the four root ids.
- `src/Ai/Raid/Uld/Action/UldActions_Freya.{h,cpp}` - `bombOrigin` member; the hold branch and the
  two new fallback tiers in `FreyaMoveAwayNatureBombAction::Execute`.
- `docs/raids/ulduar/freya.md` - a Nature Bomb section carrying the 6 s fuse, the two failure modes
  and the 10-14 yd band that rules out a blanket hold; the Iron Roots difficulty-id trap.
- Plan copied to `docs/plans/freya-p2-nature-bomb/freya-p2-nature-bomb.PLAN.md`.

## Verification

Only step 1 is possible here - the module cannot be linked in this environment.

1. **Static**: `python apps/codestyle/codestyle-cpp.py`, then a per-TU `-fsyntax-only` check of the
   changed translation units against `acore/ac-wotlk-build:master` using
   `/azerothcore/build/compile_commands.json` - mount the working tree's `src` read-only at
   `/azerothcore/modules/mod-playerbots/src`, pull the entry, strip `-c`/`-o`, insert
   `-fsyntax-only`, run from the entry's `directory`; needs `MSYS_NO_PATHCONV=1` in Git Bash.
   `scratchpad/incheck.py` does this. Expect the one pre-existing `unused parameter 'botAI'` warning
   in `GetFreyaConservatorSpore`.
2. **Build** the worldserver in Docker and confirm the binary's mtime moves past the commit
   (`docker exec ac-worldserver ls -l --time-style=+%F_%R env/dist/bin/worldserver` reports UTC, host
   is UTC+3).
3. **Re-pull Freya 25 hard mode** and measure against `1788724466`:
   - Bots that re-entered a live bomb blast: **51 -> under 15**.
   - `freya move away nature bomb` FAILED with the bot inside a blast and no move for 3 s:
     **15 -> 0**.
   - P2 deaths from 90%+ health to a bomb volley: **3 -> 0**.
   - Nature Bomb damage: **166,464/min of P2 -> under 120,000**.
   - `freya break iron roots` appears in the action log at all (it never has), and roots held 2 s or
     longer: **7 -> under 3**.
   - Bots sheltering on a spore that has a Sun Beam pool inside it: **0**.
4. **Guard, and it outranks every number above.** Raid damage dealt per minute of P2 must not fall
   below this pull's rate (46-91k dps across the phase, 53.0M cumulative by 8:30), and Attuned stacks
   per minute must stay at or above **26.2**. B withholds a walk-back and A adds a move that only
   fires where the bot is currently frozen, so neither should cost output - if either metric drops,
   B is the one to cut, because it is the change that keeps bots out of melee range longest.
5. `python tools/botobs/postmortem.py <trace>`, plus `--clump 8` and `--stalls`.

## Follow-ups, not in this change

- **P1, ruled out of scope by the user but the reason P2 ran 61% longer.** Two mechanics nothing
  touches: **Tidal Wave** (`62936`, `spell_cone` 20 degrees, radius index 23 = 40 yd, fired 3 s after
  the Water Spirit's `62935` charge while it carries the 3000 ms `62655` aura) went from a median 1.0
  to 5.0 victims per cast, 135,394 -> 342,391; and **Hardened Bark** (`62663`, +9% damage and -6%
  speed per 4 s stack, up to 99) let the Snaplasher land 52,637 and 56,602 after 15 seconds of
  chasing without a swing, against a 37,347 worst case in the reference kill.
- `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` is 12.0 in code; `62865 Unstable Energy` is
  `EffectRadiusIndex 8` = 5 yd, which is what the traces show. The dodge fires on pools that cannot
  reach the bot. Two P2 deaths involved Unstable Energy.
- `freya move to healing spore action` ran 271 FAILED / 264 OK - the unlatched oscillator recorded in
  earlier plans, still open.
- Back line to nearest Detonating Lasher: median 19.4 -> 10.2 yd, share inside 5 yd 5.3% -> 13.0%,
  and lashers landed 6 melee/Flame Lash hits on ranged and healers (0 in the reference kill). Two
  pulls on identical pre-change code straddle that number (20.2 / 0.4% and 14.9 / 18.0%), so this is
  inside pull-to-pull variance for now - worth watching, not acting on.
