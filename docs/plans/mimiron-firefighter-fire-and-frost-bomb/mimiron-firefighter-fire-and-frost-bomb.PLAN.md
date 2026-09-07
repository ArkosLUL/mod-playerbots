# Mimiron Firefighter: the fire and the Frost Bomb

## Context

Three Firefighter attempts on 2026-09-07, all wipes, all in phase 2:

| trace | ends | deaths | flame damage | Frost Bomb damage |
|---|---|---|---|---|
| `603_1_mimiron_1788806758.ndjson` (21:49) | 3:51 | 30 | **1,921,188 (45.4%)** | 523,276 (12.4%) |
| `603_1_mimiron_1788807848.ndjson` (22:07) | 3:45 | 29 | **790,651 (26.0%)** | 697,314 (22.9%) |
| `603_1_mimiron_1788808182.ndjson` (22:13) | 3:33 | 32 | **1,009,073 (33.4%)** | 612,008 (20.2%) |

The 21:49 pull ran with `AiPlayerbot.UlduarMimironHardMode` still `0` — it has zero
`mimiron dodge flames action` and zero `mimiron frost bomb action` records. Turning the flag on
halved the fire damage (45.4% → 26-33% of everything incoming) and changed nothing else. The two
pulls the user asked about are the flag-on ones, and they died the same way.

**Two independent killers, and the fire is the larger of the two.**

**Fire** is the single biggest damage source in all three pulls — bigger than the boss. Against
~2.0M of effective healing, the ~1.0M it does is roughly half of everything the healers produce.
It lands hardest on whoever the chains happen to reach: in 7848 that was healers and ranged
(Stormweaver 81% of its total damage, Elemena 77%), in 8182 the melee stack (Totemist 71%, Justice
67%, Ecoterrorist 100%).

**The Frost Bomb** is what actually ends each pull. Phase 2 starts at t≈154 s, VX-001's first bomb
spawns at t≈159.8, and at **t=169.7-169.9 it kills 13-15 of 25 inside 0.15 s**. It hits for 40-53k
against 22-24k health pools, so health is irrelevant and healing cannot answer it — a pure "be
outside 30 yd" check that the strategy currently fails. Everything dying after it is attrition on a
raid with no healers left.

Everything below came out of the three traces, the encounter script, the DBC and the world DB. No
rerun was needed. The cheats-free constraint stands: no `HasCheat`, no `TeleportTo`, no `->Kill(` in
the Mimiron files.

---

## The mechanics, as the server actually implements them

Facts the current code contradicts. Sources: `boss_mimiron.cpp`,
`modules/mod-spell-tweaks/data/dbc-reference/spell*.csv`, `acore_world.smart_scripts` / `.conditions`.

**Fire.** Mimiron drops **3 seeds every 30 s**, each 5 yd from a *random* raid member
(`EVENT_SPAWN_FLAMES_INITIAL`), for the whole encounter — including every phase handover. Each chain
grows one node every **5.75 s**, **7 yd** toward the **globally nearest player to that chain's newest
node**, and **stops growing entirely while any player is within 4 yd of that node**. A node is a
non-selectable trigger creature (34121) carrying aura 64561, which ticks 64566 every second: **3 yd,
~3,100 a tick as measured** — 13% of a bot's health bar per second, per node. Nothing puts fire out
in phase 2 except the Frost Bomb and VX-001's own Flame Suppressant (65192, **10 yd around itself**,
every 10 s, which also lands a 51% cast-speed slow, so it is not a place to stand). Phase 1 gets one
full-room clear 60 s in (64570) — the node count drops to 0 at t=90 in every trace.

**So where the raid stands is where the fire goes.** That is the only lever the raid has over it.

**Frost Bomb.** VX-001 casts 64623 at `SPELLVALUE_MAX_TARGETS 1`, repeating every 45 s from 1 s into
phases 2 and 4. `acore_world.conditions` restricts 64623 to creature entry **34121 carrying aura
64561** — so the bomb *always* lands on a live flame node, never on a player. That node summons
34149, whose SmartAI (`event_type 60`, params `10000,10000`) casts the explosion **exactly 10 s
later**: 65333 in 25-man, **30 yd radius, 47124 base**, plus a knockback, plus a dummy effect that
despawns every flame it catches. The bomb is the raid's fire extinguisher as much as its hazard —
and it lands wherever the raid has been burning.

**The bomb is escapable by a wide margin.** At spawn, 19 of 25 were inside 30 yd in both hard-mode
pulls, and the worst-placed bot needed **23 yd of travel — 3.3 s at 7 yd/s against a 10 s fuse**.
navprobe on map 603 around `ULDUAR_MIMIRON_ROOM_CENTER` is 16/16 on mesh and flat at Z 364.314 at
radius 30, 35, 40 and 45, and 15/16 at 55. There is always floor to run to.

**VX-001 never moves.** Its position is `(2744.65, 2569.46)` — `ULDUAR_MIMIRON_ROOM_CENTER` to two
decimals — for all 235 phase-2 samples.

---

## F1 — Two dodges fight over the fire, and both are too short

This is where the ~1M of fire damage comes from. Total time actually spent standing in fire is only
**204-258 bot-seconds** per pull, and most exposures are brief (median 0-1 s). The damage is all in
the tail: **5-14% of episodes run 5 s or longer**, costing 20-50k each — Elemena 11.4 s for 50,095,
Holylight 10.2 s for 39,743, Totemist 6.1 s for 38,319. Bots are not walking into fire and dying;
they are getting stuck in it.

Three measured causes, all in how the escape is issued:

**The hops are shorter than the gaps between nodes.** Chains grow in 7 yd steps and 50-60 nodes are
live by the wipe. Measured accepted-move distances:

| mover | accepted moves | median hop | p90 |
|---|---|---|---|
| `avoid aoe` | 281-338 | **4.0 yd** | 4.3 yd |
| `mimiron dodge flames action` | 181-294 | **5.0 yd** | 5.3 yd |

A 4-5 yard hop in a field with 7 yard spacing moves a bot from one node to the next.
`MimironDodgeFlamesAction` ([UldActions_Mimiron.cpp:570](src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp#L570))
means to flee "the centre of the whole in-range fire field… out past its edge", but it only collects
nodes within `ULDUAR_MIMIRON_FLAMES_RADIUS` (5 yd), so `spread` is near zero and the destination
lands ~5-6 yd out — still inside the field.

**`avoid aoe` takes the movement lock and then reports failure.**
`AvoidAoeAction::AvoidUnitWithDamageAura`
([MovementActions.cpp:2133](src/Ai/Base/Actions/MovementActions.cpp#L2133)) is built for exactly this
hazard: it walks `possible triggers` (non-selectable trigger units within 15 yd) for a
periodic-trigger aura whose trigger spell does school damage — 64561 → 64566 precisely. It calls
`FleePosition(unit->GetPosition(), radius)` with **radius = 3**, the DBC radius of 64566, and then
**ignores the result and returns false**. Across both hard-mode pulls it is evaluated 642 times with
**zero OK verdicts** and **619 accepted moves**. It runs at relevance **90**, above every Mimiron node
(60-65), so it goes first every tick a flame node is within 15 yd. Its own rate limiter is dead code:
`lastMoveTimer` is only assigned inside the `sPlayerbotAIConfig.tellWhenAvoidAoe` branch.

**The two then collide on a shared cooldown.** Both route through
`MovementAction::FleePosition`, which refuses outright while the shared `"recently flee info"` list
has an entry newer than `minInterval` (default 1000 ms). **71% of the 201-263
`mimiron dodge flames action` FAILEDs in each pull have an accepted flee by that same bot inside the
previous 1000 ms** — its own last hop, or `avoid aoe`'s. So the Mimiron dodge, the one that knows what
a fire field is, runs about half the times it is asked.

Mighty in 8182 at 1:10-1:15 is the whole thing in one track: `mimiron dodge flames action` FAILED at
1:10.693, `avoid aoe` moving it 3-5 yd at 1:11.129, 1:12.318 and 1:15.165, and 100% → 45.56% health
over four seconds of flame ticks.

**Fix.**

- Take `MimironDodgeFlamesAction` off `FleePosition` and put it on the Mimiron flee fan
  (`MoveAwayClearOfMines`), like every other Mimiron dodge. That drops the shared cooldown, uses
  `MoveTo` directly, and inherits the fan's screening. `MoveAwayClearOfMines` takes a `Unit*`; add a
  `Position` overload for the field centroid and have the existing signature delegate to it.
- Size the escape against the field rather than a 5 yd window: collect every flame node in
  `nearest npcs` and ask the fan for the nearest bearing whose destination clears **all** of them
  (F3 does the screening), rather than a fixed hop off a local centroid.
- Veto `avoid aoe` while Mimiron hard mode is live, so the Mimiron dodge owns the job. Do it as a
  Mimiron multiplier returning `0.0f` — the file already has `MimironChargeGuardMultiplier` doing
  exactly this for gap-closers
  ([UldMultipliers_Mimiron.cpp:75](src/Ai/Raid/Uld/Multiplier/UldMultipliers_Mimiron.cpp#L75)).
  Do **not** change `AvoidAoeAction` itself: the ignored return value and the dead rate limiter are
  real bugs, but that action runs in every encounter in the game and fixing it belongs in its own
  change with its own testing.

## F2 — The Frost Bomb clearance is 12 yd against a 30 yd blast (13-15 deaths, the wipe)

`ULDUAR_MIMIRON_FROST_BOMB_RADIUS = 12.0f` in
[UldEncounter_Mimiron.h:313](src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.h#L313). Its own comment says
so: *"Exact radii are DBC, so these are conservative defaults to confirm in-game."* They were never
confirmed. The DBC says 30. (The flames constant, 5.0 against a 3 yd aura, is fine.)

The one number does two jobs and both are wrong at 12:

- `MimironFrostBombTrigger` is `FindNearestCreature(NPC_FROST_BOMB, 12.0f)`, so **nobody between 12
  and 30 yd ever reacts** — that is most of the raid.
- `MimironFrostBombAction` is a bare `MoveAwayFromCreatureAction` on the same 12, so a bot that does
  react stops at 12 yd, still 18 yd inside the kill radius, and the trigger then goes quiet.

**Fix.** Split the number and rebuild the action on the Mimiron flee fan:

```cpp
// Frost Bomb Explosion 65333: 30 yd, 47124 base, and a knockback - about twice a bot's health, so
// this is a positional check and healing cannot answer it. The bomb summons on a burning flame node
// (64623's conditions require entry 34121 with aura 64561) and its SmartAI fires 10 s after the
// spawn, which is the whole warning.
constexpr float ULDUAR_MIMIRON_FROST_BOMB_RADIUS = 30.0f;     // what the trigger and spot tests use
constexpr float ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE = 34.0f;  // where to stand: blast plus slop
```

`MimironFrostBombAction` becomes a `MimironFleeAction` calling
`MoveAwayClearOfMines(bomb, ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE - bot->GetExactDist2d(bomb),
MovementPriority::MOVEMENT_FORCED, /*fallbackUnfiltered*/ true, /*interrupt*/ true, "frostbomb")` —
mine and barrage screening for free, `MOVEMENT_FORCED` so a formation leg cannot swallow it, an
unfiltered fallback so the bot always moves, and a `mimiron.flee` row keyed `frostbomb`.

Do **not** keep `MoveAwayFromCreatureAction`: its fan reaches only 30 yd of travel and it returns
false outright when no candidate clears `range`, so raising the constant alone would make it refuse
to move more often than it does now.

## F3 — Nothing screens a destination for fire or for the bomb

Every dodge in the encounter can put a bot into the two Firefighter hazards on purpose:

- `IsMimironSpotSafe`
  ([UldEncounter_Mimiron.cpp:141](src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.cpp#L141)) covers rocket
  strikes and, behind the hard-mode check, flames — but **not the Frost Bomb**. It is what the
  formation consults, so for the whole 10 s fuse the formation is free to walk bots back toward a slot
  inside the blast.
- `MoveAwayClearOfMines`
  ([UldActions_Mimiron.cpp:38](src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp#L38)), the shared Shock
  Blast / Rocket Strike fan, filters back-tracking, mines and the barrage cone — but **not flames and
  not the bomb**. Hellflame in 7848 died at 3:11 sitting on four flame nodes, having arrived there on
  `mimiron p3wx2 laser barrage action [arrived]`.
- `FleePosition`, which `avoid aoe` and the flames dodge both use today, screens **nothing**.

**Fix.** Add a Frost Bomb clearance to `IsMimironSpotSafe`, and add flames plus the bomb to the fan's
filters — which is also what makes F1's rewritten flames dodge land somewhere worth landing. Extend
the `mimiron.flee` note's refusal counters from `back/mine/cone` to `back/mine/cone/fire/bomb`, so the
next trace says which filter emptied a fan instead of leaving "refused everything" and "never asked"
looking identical. Keep `fallbackUnfiltered`: during a bomb window a large part of the room really is
unsafe and a bot that finds no clean bearing still has to move.

## F4 — The Frost Bomb node never gets the tick

Three Mimiron nodes are pushed at `ACTION_RAID + 4`:
[UldStrategy.cpp:600](src/Ai/Raid/Uld/UldStrategy.cpp#L600) rocket strike,
[:640](src/Ai/Raid/Uld/UldStrategy.cpp#L640) dodge flames,
[:646](src/Ai/Raid/Uld/UldStrategy.cpp#L646) frost bomb — against the ladder's own comment that *"no
two nodes share a number"*.

`Queue::findHighestRelevanceBasket` picks with strict `>`, so an exact tie goes to the first basket
pushed, which is `ProcessTriggers` order, which is the `triggers.push_back` order above. And
`Engine::DoNextAction` does `if (actionExecuted) { … break; }`. So while the flames hop returns true
the tick **ends there** and the Frost Bomb node is never reached.

| hard-mode pull | dodge flames OK | frost bomb OK |
|---|---|---|
| 7848 | 221 | 6 |
| 8182 | 143 | 13 |

Holylight in 8182 is the failure in one track. At **2:39.769** it issues the correct escape,
`-> (2750.92, 2616.01)`, 38 yd clear of the bomb. From 2:39.98 every tick is `held by combat`, with
`avoid aoe` and the flames dodge each issuing a fresh destination 1-2 yd from the last. At
**2:40.717** the frost-bomb destination stops being issued at all. It dies at 2:49.743 standing 23 yd
from the bomb, `[STILL WALKING]` on a 1.6-second-old flames hop.

**Fix** (per the answered scope question — minimal insert, every existing relative order preserved):

| node | now | after |
|---|---|---|
| `mimiron p3wx2 laser barrage` | +5 | **+7** |
| `mimiron frost bomb` | +4 | **+6** |
| `mimiron rocket strike` | +4 | **+5** |
| `mimiron dodge flames` | +4 | +4 |

Nothing else moves and no normal-mode ordering changes.

Noted, not fixed: `mimiron shock blast` sits at `+3`, *below* the flames hop, so a 99999-damage 15 yd
blast loses the tick to a fire step. Nothing died to Shock Blast in these three pulls, so this stays an
observation until a trace shows it costing something.

## F5 — The phase 1→2 handover pre-burns the arena the raid is about to fight in

The user's observation, and the trace is emphatic. The handover runs **t≈107 → t≈154, about 47
seconds**, and for all of it nothing is attackable. `mimiron.slot` shows the two staging branches
running: `stagemelee` — an **8 yd** ring on the room centre
([UldEncounter_Mimiron.cpp:641](src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.cpp#L641)) — and
`stagering`, the 22 yd `ULDUAR_MIMIRON_SPREAD_RADIUS` ring, also anchored on the room centre because
`staging` swaps the anchor away from the focus.

Mimiron does not stop seeding fire for the handover. Measured in 7848, whose raid held station
properly (median radius 5.3-6.4 yd at t=112, then 16-23 yd for the rest of the window):

- **37 flame nodes born during the handover, 86% of them inside 25 yd of the room centre**, median
  r=18.6.
- **203,272 damage taken over those 47 seconds — 100% of it fire.** Nothing else is hitting the raid.
  That is a quarter of the pull's entire fire total, paid for standing still with no boss up.

Then phase 2 starts and **VX-001 spawns on the room centre**, so the melee stack and the 22 yd ranged
ring both take station on ground the raid spent 47 seconds setting alight. 8182 is the control: its
raid happened to drift out to median r=52 during the handover, only 31% of its handover nodes landed
inside 25 yd — and its handover fire bill was 110,959 instead of 203,272.

**Fix.** Under hard mode, stage wide instead of on the centre. There is no casting range to hold, no
cone to dodge and nothing to hit for the whole window, so the only thing the radius costs is run-in
when the phase goes live:

```cpp
// Firefighter staging. Nothing is attackable for the 47 s between phases, so there is no range to
// hold - but Mimiron keeps seeding fire 5 yd from three random members every 30 s and each chain
// crawls toward whoever is nearest, so a raid parked on the room centre burns the ground VX-001 is
// about to spawn on. One pull took 203272 damage across that window, all of it fire, with 86% of the
// nodes born in it landing inside 25 yd of the centre. Melee sit inside ranged here purely because
// they have the longer trip back in. navprobe: 24/24 on mesh at both radii, flat at Z 364.314.
constexpr float ULDUAR_MIMIRON_HM_STAGING_MELEE_RADIUS = 30.0f;
constexpr float ULDUAR_MIMIRON_HM_STAGING_RANGED_RADIUS = 36.0f;
```

`GetMimironStagingMeleeSlot` and the `stagering` branch each take the hard-mode radius when
`IsMimironHardModeActive`, and keep 8 and 22 otherwise. Both stay on `ULDUAR_MIMIRON_ROOM_CENTER`, so
the shape is unchanged and only the radius moves.

Run-in cost when the phase goes live: melee 30 → ~8 yd is 22 yd ≈ 3.1 s; ranged 36 → 28 yd is 8 yd
≈ 1.1 s. Against a 47 second window that pays 111-203k of fire, that is cheap.

Second-order benefit: the Frost Bomb lands on a flame node, so pushing the handover's fire to the
perimeter also pushes the first bomb there. A bomb at r≈33 on the far side of the room from a raid at
r=18-28 is 50-60 yd away and harmless — where today's centre-heavy fire put 19 of 25 inside the blast.

## F6 — The phase 2 ring fans the fire back out

Same lever, the next window. In phase 2 every ranged bot and healer is on `branch = "ring"` (29
`mimiron.slot` notes, no other branch) — a full 360° circle of radius 22 anchored on VX-001 at the
room centre. Measured raid max-pairwise spread through phase 2: **62-86 yd**.

- Seeds land 5 yd from three *random* members every 30 s. Spread over the whole circle, they land all
  round the room.
- Each chain then crawls toward whichever player is nearest *its own* head, so 25 bots on 25 bearings
  pull 25 chains along 25 different radii.
- The 4 yd freeze rule — the one lever the raid has — needs a player close to a head, and a 22 yd ring
  leaves about 6 yd of arc between neighbours at best.

Real raids do the opposite: stay together so the fires overlap and stay in one part of the room, then
let the Frost Bomb burn that part clean.
([Warcraft Tavern](https://www.warcrafttavern.com/wotlk/guides/mimiron-strategy-guide-ulduar-25/),
[Wowpedia](https://wowpedia.fandom.com/wiki/Mimiron_(tactics)))

**Fix** (per the answered scope question — gather into an arc). While hard mode is on, the ranged
`ring` branch of `DeriveMimironSpreadSlot`
([UldEncounter_Mimiron.cpp:822](src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.cpp#L822)) becomes a wedge
instead of a circle, reusing the phase-3 machinery unchanged — `MimironWedgeRows` and
`MimironWedgeSlot` at `ULDUAR_MIMIRON_PHASE3_MIN_RADIUS` (18) and `ULDUAR_MIMIRON_PHASE3_SPACING` (6),
centreline on the bearing from the room centre to `ULDUAR_MIMIRON_PHASE3_STAGE`, the same east gap
phase 3 already uses. Anchor stays what the ring uses today. Emit a distinct branch name (`hmwedge`)
so the trace can tell it from `ring` and `p3wedge`.

Cost, stated plainly: Rapid Burst (64531/64532) is a 104° cone at ~1,600 a hit, and the existing ring
comment says the full circle is there so one cone cannot hold everyone. A 120° wedge is only just
wider than the cone, so most of the raid eats each one — against fire currently doing five to eight
times Rapid Burst's total. Heat Wave (64533, 50000 yd) is raid-wide either way.
`ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE` is the knob if the fire still fans out.

No new coordinate: the wedge sits inside the ring sweep already probed above.

---

## Not fixed, and why

- **`AvoidAoeAction`'s own two bugs** — the ignored `FleePosition` result and the `lastMoveTimer` only
  assigned under `tellWhenAvoidAoe`. Both real, both in an action every encounter runs. F1 vetoes it
  for Mimiron hard mode instead; fixing it properly deserves its own change.
- **Emergency Fire Bots.** [UldActions_Mimiron.cpp:690](src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp#L690)
  puts NPC 34147 on the DPS priority list under hard mode, while `conf/playerbots.conf.dist` promises
  "bots leave them alone either way" and `docs/raids/ulduar/mimiron.md:14` calls them "not kill
  targets". `creature_template` gives 34147 faction 16, so the code path is live. None spawned in these
  three pulls — they come from the phase-3 ACU summon trigger and the raid never got there — so this
  stays open, needing a decision on which of the three is right.
- **Proximity Mines (3 deaths in 8182, 0 in 7848).** `nearest npcs` has no cap, so the fire does not
  blind the mine check; the mine node sits at `ACTION_RAID - 1` by design.

## Files touched

- `src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.h` / `.cpp` — F2 constants, F3 `IsMimironSpotSafe`,
  F5 staging radii, F6 hard-mode wedge branch
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.h` / `.cpp` — F1 flames dodge and the `Position`
  overload, F2 frost bomb action, F3 fan filters and flee-note counters
- `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Mimiron.h` / `.cpp` — F1 `avoid aoe` veto
- `src/Ai/Raid/Uld/UldStrategy.cpp` — F4 relevance renumber, and the multiplier wiring
- `docs/raids/ulduar/mimiron.md` — the findings, per the raid-findings convention. Run
  `/compact-docs-writer` before editing it, up front rather than as cleanup.
- `docs/plans/mimiron-firefighter-fire-and-frost-bomb/…PLAN.md` — mirror of this plan, written once
  plan mode exits.

No new node, trigger or strategy wiring beyond the one multiplier. No schema bump.

## Verification

The module cannot be compiled from this checkout — the worldserver Docker build is the only compile
path, so a rebuild comes first and nothing below runs without it.

Static, before handing over: brace and paren balance against HEAD with comments and string literals
stripped; every new helper declared and defined; no stale caller of the old
`ULDUAR_MIMIRON_FROST_BOMB_RADIUS` meaning or the old `MoveAwayClearOfMines` signature; the cheat grep
clean.

Then one Firefighter pull, read with `tools/botobs/postmortem.py`:

1. **F1 is the headline.** Total 64566 damage down hard from the 790k-1,009k baseline, and its share
   of all incoming down from 26-33%. The tail is the real test: episodes of 5 s or longer should fall
   from 5-14% of exposures to near zero, and no single episode should cost 20-50k. Median accepted hop
   for `mimiron dodge flames action` well past 5 yd. `avoid aoe` shows zero accepted moves while hard
   mode is live.
2. **F2.** Zero Frost Bomb deaths, or a bot that started its run on time and still lost — a
   fan-geometry finding, not this one. `--notes mimiron.flee` shows `frostbomb ok` rows within a tick
   of the bomb spawning, ~10 s before the 65333 damage records.
3. **F3.** No death whose `moving ->` names a Mimiron dodge and whose `ON TOP` lists flame nodes. The
   new refusal counters say which filter is doing the work.
4. **F4.** `mimiron frost bomb action` OK counts no longer swamped by `mimiron dodge flames action` —
   the ratios here were 6:221 and 13:143.
5. **F5.** `mimiron.slot` shows `stagemelee`/`stagering` at the new radii through the t≈107-154 window.
   Damage taken across the handover down from 203,272 / 110,959, and the share of nodes born in that
   window landing inside 25 yd of the centre down from 86%. Check the raid is in position within a few
   seconds of phase 2 going live, not still walking.
6. **F6.** `mimiron.slot` reads `hmwedge` for ranged in phases 1, 2 and 4. Burnt fraction of the room
   at a fixed time falls against the 13-17% baseline, and raid max-pairwise spread drops from 62-86 yd
   to roughly the wedge's own width. Rapid Burst damage per bot is expected to *rise* — the question is
   whether it rises by less than the fire falls.
7. **Phase 3 and 4 at all.** No Firefighter pull has reached them; getting there is itself the result,
   and the first trace that does is what says how bad phase 3/4 fire is.
8. **No regression on a normal-mode clear.** F4 moves two node numbers that also exist in normal mode;
   F5 and F6 are behind the hard-mode check. Compare a `--track` of two or three bots against a normal
   pull.
9. Cheat grep still clean.
