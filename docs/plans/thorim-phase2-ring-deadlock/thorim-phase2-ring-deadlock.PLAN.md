# Thorim phase 2: fix the ring deadlock, the stale slide, and three bad shelters

Traces read with `python tools/botobs/postmortem.py <file>` from `modules/mod-playerbots`; files live in
`env/dist/logs/botobs/`. **Timestamps are milliseconds.** Scratchpad scripts behind every number:
`lib.py`/`core.py` (loaders, need `G:/` paths not `/g/`), then `jk1.py` .. `jk21.py`.

## Context

`ab6f30c67` shipped the Lightning Charge shelter table for the ranged camp and replaced the melee
Blizzard flee with a slide around the ring. Two 25-man hard-mode pulls followed on 10 Sep 2026:

| | J `1789071070` 23:17 | K `1789071729` 23:28 |
|---|---|---|
| phase 2 | 3:09.0 -> 5:46.2 (2.62 min) | 3:00.2 -> 6:14.6 (3.24 min) |
| boss HP at wipe | **43.5%** | **28.3%** |
| phase 1 deaths | 0 | 0 |
| phase 2 deaths | 30 | 30 |

Counting only phase 2 deaths whose fatal blow was not Thorim's melee swing and that landed before the
second tank died - the rest are wipe-tail noise:

| | F | H | I | **J** | **K** |
|---|---|---|---|---|---|
| counted deaths | 24 | 19 | 24 | **11** | **13** |
| Lightning Charge fatal blows | 9 | 11 | 13 | **5** | **3** |
| biggest single cone (targets) | 11 | 8 | 11 | **5** | **4** |
| boss HP at wipe | 61.9% | 59.0% | 64.0% | **43.5%** | **28.3%** |

**The change worked.** Deaths that count roughly halved, Lightning Charge stopped being the dominant
killer, and the raid pushed the boss 16-36 more percent. What is left is five defects, four of them in
the code that shipped. Defects 1 to 4 are fixed here; 5 is left open by decision.

### What is confirmed working

- `thorim.rangedslot`: 14 notes over 14 bots, **exactly one per bot**, both pulls. The latch holds.
- Shelter walks arrive: J 18 of 24, K 31 of 39.
- `thorim sif blizzard action` issued **zero** melee move orders (was 137/240/167 in F/H/I).
- Melee Blizzard damage taken more than 12 yd from the boss: **0.0%** in both pulls, against 34%, 56%
  and 58%. The 30 yd flee and its multi-second stalls are gone.
- Melee movement 52 and 26 yd/min/head, against 128, 75 and 84.
- `thorim.slot`, `thorim.squad`, `thorim.p2role` all unchanged. Phase 1 still clean.

## Defect 1: the ring deadband deadlocks, and no melee move is ever issued

`ThorimRingNeedsMove` (`UldEncounter_Thorim.cpp`) **mutates `ringArrived` and is called twice per
tick** - once by `ThorimPhase2PositioningTrigger::IsActive` (`UldTriggers_Thorim.cpp:184`) and again by
`ThorimPhase2PositioningAction::Execute` (`UldActions_Thorim.cpp:378`).

`RingSlideBeatsTheDeadband`, added in `ab6f30c67`, lets the first branch return true while the bot is
within the 5 yd reposition tolerance. That branch erases `ringArrived`. The second call then finds the
latch gone, sees the distance is also inside the **3 yd arrive tolerance**, re-inserts the latch and
returns **false** - so `Execute` returns before it reaches `MoveTo`:

- trigger: latched, distance <= 5.0, slide beats the deadband -> erase, **return true** (note `0`)
- action: not latched, distance <= 3.0 -> insert, **return false** (note `1`), no move

Before the change the two branches could not disagree: branch 1 only returned true above 5.0 yd, where
branch 2's `> 3.0` test also passed. The new exception broke that ordering.

Signature is a same-millisecond `thorim.ringarrived` `0` -> `1` pair:

| | F | H | I | **J** | **K** |
|---|---|---|---|---|---|
| same-tick 0->1 flips | 0 | 0 | 0 | **62** | **107** |
| `ringarrived` notes / min | 34.8 | 29.8 | 33.5 | **77.9** | **77.8** |

Longest spell frozen in place, per melee bot: J has five bots at 84-92 s; K has Justice at **148 s**,
the entire phase. F/H/I peaked at one bot per pull.

### This is why Angry ate Blizzard

K, 4:19.458, killed by Blizzard after **six ticks over five seconds**.

- It stood at exactly (2110.83, -262.47) from 4:05 to death - **not one yard of movement in 14 s**.
- Its last accepted move order was at **3:08.093**, 71 s earlier.
- The killing bunny landed at (2103.9, -260.2) at 4:13.514, 7.3 yd away.
- Its last computed target was 4.27 yd away and **11.56 yd clear** of that bunny.
- **47 of 72** ring bearings were clear at that moment; the ring is never fully covered (worst case
  over both pulls 22% clear, zero fully blocked snapshots).

So the slide found the right answer, `RingSlideBeatsTheDeadband` correctly said "you are in it and the
spot is not", and the arrive-tolerance branch cancelled the move on the same tick, every tick, until it
died. The `thorim.ringarrived` trace shows the pair flipping every ~300 ms throughout.

## Defect 2: melee eat the cone - 18 hits across the two pulls

Attributed by comparing each victim's actual bearing, its last `thorim.ringspot` target and the true
cone bearing taken from the lit orb's guid in `thorim.lightningorb`:

| cause | hits |
|---|---|
| the 5 yd reposition deadband held it in the cone with a safe target 3.3-4.9 yd away | **9** |
| a stale Blizzard offset dragged it into the cone | **3** |
| still walking (gap 5.2-7.5 yd) | 6 |

**The deadband half is the same root cause as defect 1, applied to the cone.** At r=8 a 5 yd chord is a
36 degree bearing change and the cone half-arc is 37.5 degrees, so *any* slide the cone needs is
swallowed. `RingSlideBeatsTheDeadband` only ever looks at bunnies.

**The stale-offset half is `BlizzardRingOffset`'s early return.** It re-tests the held offset against
bunnies and never against the cone:

- J 3:55.252, orb 0, cone bearing 46.4: Assasin and Shadow were placed **56.6 degrees** off cone by the
  cone offset, then a held -58 degree Blizzard offset pulled them to **1.4** and **4.6** degrees off.
  Both took it.
- K 5:00.881, orb 3, cone bearing 254.6: Justice 58.6 -> **27.6** degrees off. Took it.

The targets themselves are otherwise correct - 55 to 58 degrees off cone in every other case. The cone
solver is fine; what follows it is not.

## Defect 3: the slide flip-flops between sides

`BlizzardRingOffset` scans outward alternating both ways and takes the first clear bearing, with no
hysteresis beyond "hold while still clear". A small change in the bunny set flips the answer to the
opposite side. 48 offset changes in J, 31 in K:

| swing | in | who |
|---|---|---|
| -52 -> +47 (99 deg, 12.2 yd of arc) | 0.64 s | Assasin |
| 0 -> -112 (13.3 yd) | 4.01 s | Shadow and Mighty together |
| -22 -> +27 (49 deg, 6.6 yd) | 0.30-0.62 s | Angry, Justice and Totemist together |

## Defect 4: three shelters sit inside the Blizzard damage reach

Ranged and healers took **zero** Blizzard in F, H and I. In J they took 5,148; in **K, 233,227**.

Measured against the pooled bunny track (85 distinct spawn points across all five pulls) versus the
measured 9.8 yd effective reach:

| shelter | clearance |
|---|---|
| orb 2 slot 1 (2107.5, -283.35) | **7.8 yd** |
| orb 2 slot 2 (2114.5, -274.35) | **7.6 yd** |
| orb 2 slot 3 (2140.0, -241.35) | **8.2 yd** |

Every other shelter and every home spot is 12.0-26.4 yd clear. 32,836 of K's ranged Blizzard damage was
taken by bots standing *exactly on* the orb 2 slot 1 shelter. Orb 2 never lit in F, H or I, which is why
this did not show up before. The solver treated track clearance as a soft preference; the shipped
plan's own table records 7.7, 8.0 and 8.2 for these three rows.

## Defect 5: the shelter run budget assumes the bot starts at home

The table's runs (max 25.6 yd) were measured home spot -> shelter. Bots are frequently somewhere else
when the orb lights - on a previous orb's shelter, or still walking in off the balcony. Actual distance
to the shelter when the note fired:

- J Power, orb 0 slot 5: **52.1 yd**. Died mid-walk at 3:55.252, still 26.2 yd short.
- J Fel, same shelter: 33.1 yd. Died mid-walk, 18.8 yd short.
- K Malediction, orb 0 slot 5: **60.6 yd**.
- K Prayer and Holylight, orb 2 slot 3: 35.9 yd.

These are the six "still walking" cone hits and the two ranged deaths at J 3:55.252.

## What is being changed

### 1. Make `ThorimRingNeedsMove` non-mutating

Replace it with a pure predicate that only reads state. The two tolerances stay and keep their meaning -
arrive at 3.0, hold until 5.0 - because the hysteresis is the point; mutating from inside a predicate
called twice per tick is the bug. The whole thing collapses to a tolerance choice plus the escape below:

```
bool ThorimRingWantsMove(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    if (!bot)
        return false;

    ThorimEncounterState const* state = FindState(bot);
    float const tolerance = state && state->ringArrived.count(bot->GetGUID())
                                ? ULDUAR_THORIM_RING_REPOSITION_TOLERANCE
                                : ULDUAR_THORIM_RING_ARRIVE_TOLERANCE;

    return bot->GetDistance(spot) > tolerance || RingSpotBeatsTheDeadband(botAI, bot, spot);
}
```

The latch then moves to the one place that knows what actually happened,
`ThorimPhase2PositioningAction::Execute`:

- `ThorimRingWantsMove` false -> `ThorimRingMarkArrived(bot)` and return false. Safe to insert
  unconditionally: false means the bot is inside whichever tolerance applied.
- true -> `ThorimRingClearArrived(bot)`, then `MoveTo`.

`ThorimPhase2PositioningTrigger::IsActive` (`UldTriggers_Thorim.cpp:184`) just calls the predicate, so
the two sites can no longer disagree. `ThorimMeleeRingSettled` keeps reading `ringArrived` unchanged.

Note the escape has to sit in **both** branches, which is why the tolerance is picked first and the test
is `||`-ed on afterwards rather than nested inside the latched branch. Putting it only in the latched
branch leaves the bot one move and then re-arms the latch on the next tick, so a refused move (`wait`
or `blocked`, which is 40-60% of orders) is never retried and the bot dies where it stands.

### 2. Generalise the deadband exception to cover the cone

`RingSlideBeatsTheDeadband` becomes `RingSpotBeatsTheDeadband(botAI, bot, spot)` - the handed spot is
materially safer than where the bot stands - true when **either**:

- a live `NPC_SIF_BLIZZARD` is within `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE` of the bot and not of the
  spot (today's test), **or**
- an orb is lit and the bot's **current** bearing off the boss is inside the cone while the spot's
  bearing is not. Reuse `ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL)`,
  `BearingFromBoss` and `InLightningChargeCone`, so it needs `botAI` and a `GetThorim(botAI)` lookup that
  today's signature does not have.

That covers the 9 deadband cone hits and keeps the deadband doing its real job against a drifting boss.

### 3. Re-test the held Blizzard offset against the cone

In `BlizzardRingOffset`'s early return, require the held bearing to clear the cone as well as the
bunnies, using the same `orb`/`coneBearing` the search below already resolves. Hoist that resolution
above the early return. Three cone hits.

### 4. Hysteresis on the slide

When re-solving, try the side the held offset was on first and only cross over if that side has no
clear bearing within the search span. A held offset of 0 keeps today's behaviour.

### 5. Re-solve the rows that break a hard constraint

**Found during implementation, and it landed differently from the plan above.** Two things came out of
re-running the solver against the pooled data (46 boss origins from F, H, I, J and K; 179 bunny spots
from seven pulls):

**Track clearance >= 11 yd is structurally infeasible for orb 2** (solver orb 3, the south-east one that
covers four of the six camp slots). The pocket that is both >= 42.5 degrees off that cone and >= 11 yd
off the Blizzard track is about **97 square yards**; four points 9 yd apart need roughly 70 square yards
each. Infeasible at every separation down to 6 yd, at runs out to 34 yd, and even with all six slots
free to move. Best achievable worst-case clearance for that orb is 8.0 yd, which is what the shipped
rows already have (7.6-8.2). So it stays a preference, not a rule, and those three rows do not change.

That is the right trade on the measured numbers anyway. Attributing the camp's Blizzard damage by what
the bot was doing at the time:

| | J | K |
|---|---|---|
| walking to a shelter | 5,148 (100%) | 146,757 (**63%**) |
| at a home spot | 0 | 53,634 (23%) |
| standing on a shelter | 0 | 32,836 (**14%**) |

Standing on a low-clearance shelter is 14% of it. **Walking to one is 63%**, which is defect 5 - the
uncapped run - not the coordinates. Against a cone that is 20k in one instant and would catch about nine
stacked bots per lighting, the shelter is worth it at 8 yd of track clearance.

**Four other rows do break a hard constraint** once the J and K boss positions widen the origin set, and
those are the ones that changed. Three fall under the 42.5 degree placement margin and one drifts past
the 32 yd caster range:

| row (code orb/slot) | was | now | moved | off cone | track |
|---|---|---|---|---|---|
| 0/4 | (2142.00, -254.85, 419.814) | (2142.00, -255.35, 419.771) | 0.5 | 42.0 -> 43.0 | 19.8 -> 20.1 |
| 1/5 | (2119.50, -225.85, 420.293) | (2119.50, -226.85, 420.146) | 1.0 | 52.2 -> 51.5 | 12.0 -> 11.0 |
| 3/1 | (2125.50, -269.85, 419.755) | (2126.00, -269.85, 419.761) | 0.5 | 41.3 -> 43.0 | 19.4 -> 19.9 |
| 6/5 | (2132.50, -245.85, 419.762) | (2133.50, -246.85, 419.652) | 1.4 | **38.1** -> 42.8 | 13.9 -> 15.0 |

Row 1/5 moved for the caster range (32.7 -> 31.7 yd); the other three for the cone margin. **6/5 at 38.1
degrees was the one that mattered**: that is inside the 40 degree in-cone test and only 0.6 degrees
outside the real 37.5 arc, so a bot sent there was still being sent into the cone.

All four navprobed, point and path: poly distance 0.027-0.493, and every path length matches its
straight line (4.85/4.85, 21.84/21.75, 1.64/1.63, 2.73/2.72). `verify2.py` re-parses the shipped table
out of the source and confirms it is total, has no dead rows, meets every hard constraint, keeps the
tightest occupied pair at 9.0 yd, and that no row reads in-cone at the 40 degree test - so the shelter
cannot chatter against the test that sent the bot there.

## Deliberately not doing

- **Capping the shelter run.** Defect 5 stays open by decision: a bot commits to its shelter however far
  away it is. One that makes it is safe; one that does not takes the cone while moving, which is what
  killed Power and Fel at J 3:55.252 and what the six "still walking" cone hits are. Revisit only if that
  count grows once defects 1-4 are gone - part of it is bots starting from a previous orb's shelter,
  which the hysteresis and cone re-test change anyway.

- **Moving the tank anchor.** Still the right long-term answer for the tank's and the melee ring's
  Blizzard damage, but it re-solves the whole camp, and the shelter table is now measurably working. Not
  while there are four code defects on top of it. Numbers, so the next round does not re-derive them:

  | | current | proposed |
  |---|---|---|
  | `ULDUAR_THORIM_PHASE2_TANK_SPOT` | (2110.7483, -252.65265, 419.440) | **(2135.0, -263.0, 419.846)** |
  | clearance to the Blizzard track | **5.1 yd** | **29.7 yd** |
  | r=8 melee ring clearance (needs > 9.8) | **-2.9 yd** | **+21.7 yd** |
  | distance from the arena centre | 15.7 | 11.2 |

  Track figures are against the pooled 179-point bunny track from all seven traced pulls, not one pull's
  20-36 points. A 0.5 yd grid search bounded to 22 yd off the arena centre puts the optimum at
  (2134.5, -263.5) at 29.9 yd, so the proposal is within 0.2 yd of the best available and there is no
  better spot to hold out for. Navprobe: poly distance 0.130, `Map::GetHeight` and
  `UpdateAllowedPositionZ` both 419.846; the r=8 ring around it is **24/24 on mesh** (worst poly distance
  0.16, settled z 419.821-419.831); the walk from the current anchor is a clean 26.37 yd, 8 waypoints.

  The cost is the whole camp: **11 of the 18 camp points break their own constraint** against the new
  anchor. Home slots 1-5 fall to 6.0-18.3 yd out against the 22-32 yd band, and seven shelters land
  4.5-17.3 yd from the anchor against the 22 yd floor that keeps the melee ring from bridging Chain
  Lightning into the camp. That is a full re-solve of six home spots and twelve shelters, plus navprobing
  all eighteen.
- **The camp stacking.** 14 bots share 6 slots, so the tightest occupied pair is 0.00 yd in every pull
  including F, H and I - Chain Lightning chains through the camp by construction. Pre-existing, and
  Chain Lightning per minute (J 84k, K 141k) sits inside the pre-change range (F 59k, H 150k, I 127k).
- **The balcony ramp chain.** K 3:11.998 killed Power, Assasin and Mighty in 6 ms for 190k while all
  three were still walking down under `thorim balcony advance action`, 1.7 yd apart. Real, but it is a
  phase 1 -> 2 transition problem, not a phase 2 positioning one.
- **Sif.** Frostbolt Volley is DBC radius 200 with no positional answer.

## Verification

Static, from `modules/mod-playerbots`:

- `python apps/codestyle/codestyle-cpp.py` clean for the touched files. Its three standing failures
  (`DBCStructure.h` tabs, `mmaps_generator`) are pre-existing.
- No line over 120 columns; files stay LF and ASCII. Check encoding in Python reading bytes, not with
  `grep -P` - the locale here rejects it and the pass is then meaningless.
- Per-TU syntax check in `acore/ac-wotlk-build:master` against `/azerothcore/build/compile_commands.json`
  for each changed `.cpp`: take the entry for the file, drop `-c` and `-o`, add `-fsyntax-only`, run
  from its `directory`. **Mount the live tree over the image's baked copy**
  (`-v "$(pwd -W)":/azerothcore/modules/mod-playerbots`) or it silently checks stale code - verify with
  `grep -c BlizzardRingOffset` inside the container first. Needs `MSYS_NO_PATHCONV=1`. Expect only the
  two standing `-Wunused-parameter` warnings. ~2.5 min per TU, so background it, and do not edit the
  tree while it runs.
- Re-run `verify.py` so the shipped tables are re-parsed out of the source and re-checked, with the new
  hard track-clearance constraint added to it.

In game, one 25-man hard-mode pull, then re-read the trace:

1. **Same-tick `thorim.ringarrived` 0 -> 1 flips back to zero.** 62 and 107 today, 0 before the change.
   This is the whole of defect 1.
2. **No melee bot frozen in place for more than ~30 s** while alive. J had five at 84-92 s, K had one at
   148 s.
3. **Melee Blizzard damage falls.** Flat so far - 211k and 280k against 184-236k - because they now
   stand in it instead of fleeing it. With the deadlock gone this should finally drop.
4. **No melee cone hit where the bot's own target was safe.** 12 of 18 today.
5. **Ranged and healers back to zero Blizzard**, or close to it. 5,148 and 233,227 today against zero.
   Expect this to stay well above zero while the run is uncapped: 63% of K's was taken in transit, which
   nothing here changes. The part these fixes should remove is the 14% taken standing on a shelter and
   whatever the wider origin set was costing at 38.1 degrees off cone.
6. **Record how many bots were still walking to a shelter when the cone landed.** Six today. Not a pass
   or fail - the run is deliberately uncapped - but the number decides whether that gets revisited.
7. **No regression** on what already works: `thorim.rangedslot` one note per bot; `thorim sif blizzard
   action` issuing nothing for melee; melee off-ring Blizzard share at 0%; melee movement under
   ~60 yd/min; `thorim.slot`, `thorim.squad`, `thorim.p2role`; phase 1 deaths 0-1; hard mode still won.
8. **Boss HP at wipe keeps falling**, or the raid kills it. 43.5% and 28.3% today, from 59-64%.
