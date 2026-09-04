# Thorim: phase-2 positioning, the melee ring, and the state that leaks between pulls

Seven traces, 2026-09-05 00:01 – 00:37, map 603 instance 1, schema v10, 25-man, 2 humans.
Read with `python modules/mod-playerbots/tools/botobs/postmortem.py <file> [--notes|--stalls|--track]`.
**Timestamps in the NDJSON are milliseconds.**

| file | time | outcome |
|---|---|---|
| `603_1_thorim_1788555562.ndjson` | 00:01 | 104 s, aborted |
| `603_1_thorim_1788555714.ndjson` | 00:02 | 10-man roster, 51 s |
| `603_1_thorim_1788555813.ndjson` | 00:04 | 8-man roster, 69 s |
| `603_1_thorim_1788555969.ndjson` | 00:10 | 232 s, wipe, 29 deaths |
| **`603_1_thorim_1788556371.ndjson`** | **00:19** | **353 s, wipe, 28 deaths — the hard-mode attempt** |
| `603_1_thorim_1788556991.ndjson` | 00:27 | 262 s, wipe, 26 deaths |
| **`603_1_thorim_1788557437.ndjson`** | **00:37** | **391 s, KILL, 3 deaths — the last attempt** |

The build under test carries `94c4e3b0c`. Both of its fixes held: squad 2 has 12 bots in the kill
(1 tank, 2 healers, 9 dps), and every bot that actually walked the balcony chain cleared both trap
bunnies by 16.5 yd. See context item 4 for why most of them did not walk it.

Only 00:19 was hard mode — the one trace where Sif joins (first Frostbolt 3:13.959, 72 casts) and
the only one carrying Blizzard 62602.

On commit, copy this file to
`docs/plans/thorim-phase2-positioning-and-state-reset/thorim-phase2-positioning-and-state-reset.PLAN.md`.

---

## Context

Four questions, three bugs — the stranded bots and the traps turn out to be the same one.

### 1. What killed the raid in hard mode

Phase-2 incoming at 00:19 (3:13 → wipe at 5:53, 160 s): **2,858,339 total = 17,855 dps** against
**20,673 hps**. The kill at 00:37 ran 9,651 incoming against 33,580 hps. Hard mode nearly doubles the
damage and leaves no headroom; the healers died 4:22 – 5:26 and the wipe followed.

| source | damage | share |
|---|---|---|
| Sif Frostbolt Volley 62604 | 876,923 | 30.7% |
| Thorim melee | 445,814 | 15.6% |
| Thorim Chain Lightning 64390 | 386,726 | 13.5% |
| Thorim Lightning Charge 62466 | 386,238 | 13.5% |
| Sif Frostbolt 62601 | 339,493 | 11.9% |
| Sif Frost Nova 62605 | 139,865 | 4.9% |
| **Blizzard 62602** | **123,539** | **4.3%** |
| Thorim Unbalancing Strike | 99,417 | 3.5% |

**Decided: fix only what positioning can fix.** Frostbolt Volley is DBC radius 200 and hit 11–20 of
25 with victims 41–48 yd apart — there is no positional answer to it, and closing a 17.9k-vs-20.7k
gap is not this pass's job.

**Blizzard is positional and it is purely a bot-placement bug.** `NPC_SIF_BLIZZARD` (32879) is
summoned at (2108.7, −280.04) every 30 s (`boss_thorim.cpp:857`) and walked along eight hard-coded
waypoints (`InitWaypoint()`, `:961-969`) casting SPELL_BLIZZARD on itself. Blizzard 62602 has DBC
radius **13.0** (Spell.dbc field 92 → SpellRadius id 17); `ULDUAR_THORIM_SIF_BLIZZARD_RADIUS` is 12.
Closest approach of each phase-2 anchor to that walk:

| anchor | clearance |
|---|---|
| **RANGE1 (2112.88, −267.69)** | **8.1 yd** |
| **RANGE3 (2156.80, −267.57)** | **6.4 yd** |
| RANGE2 (2134.13, −257.33) | 23.4 yd |
| tank / off-tank | 27.1 / 24.6 yd |
| melee 1/2/3 | 18.7 / 27.8 / 18.3 yd |

Every yard of Blizzard damage landed on ranged (50,947) and healers (72,592). None on melee, none on
tanks. The eight victims are exactly the occupants of RANGE1 and RANGE3. They are also the four
ranged who moved most in phase 2 (Elemena 418 yd, Prayer 182, Malediction 180, Druidica 175, against
0–116 for the rest): they dodge, then `ThorimPhase2PositioningTrigger` walks them back.
`ThorimArenaPositioningTrigger` (`UldTriggers_Thorim.cpp:120-133`) already carries the hazard bail
that would stop this; the phase-2 trigger has none.

**Lightning Charge is the larger avoidable one, from the same three fixed spots.** `SpellHit`
(`boss_thorim.cpp:604-610`) faces Thorim at the charging orb and casts 62466 at it. Decoding every
62466 hit in the kill against the boss→orb bearing: **every victim was within 28.6°** of it. The
three ranged anchors sit at bearings 153.5°, 98.1° and 32.4° off the boss; the seven Thunder Orbs at
81.9, 102.8, 60.0, 124.9, 159.2, 334.3 and 201.4. Each anchor is inside the cone for two to four of
the seven. Result in the kill: **508,072 on ranged and 139,362 on healers — 35% of all phase-2
damage** — against 117,593 on melee, who rotate out. `UldTriggers_Thorim.cpp:208-209` asserts "the
ranged spots are already outside anything the rotation would buy them"; that is false.

With the cone half-width the mod already uses (75°/2 + 15° = 52.5°), the seven orbs between them
cover every bearing off the boss except a 27.9° window at 253.9°–281.8° — due south, where the tank
stands and where the floor ends. **So no static ranged layout can dodge Lightning Charge; the ranged
have to step out of it, like the melee ring does.**

### 2. The melee oscillation — they are not dodging anything

Phase 2 of the kill, 191 s:

| bot | yards run | % of snapshots moving | phase-2 dps |
|---|---|---|---|
| Ecoterrorist | 1429 | 89% | 2,606 |
| Obliteration | 1125 | 89% | 3,455 |
| Angry | 1091 | 86% | 4,192 |
| Totemist | 1037 | 92% | 3,432 |
| Mighty | 980 | 85% | 4,149 |
| Justice | 947 | 74% | 4,445 |
| ranged, for contrast | 9 – 105 | 1 – 7% | 5,263 – 7,490 |

Melee on the floor average 3,713 dps against the ranged 6,380 — **58%**, and the most-travelled melee
is the worst. (Shadow and Assasin measured 0; see question 3.)

Nothing they are chasing moves. The boss is parked at (2137.35, −279.91) from 4:00 to the kill, 73 yd
travelled in the whole phase. The main tank sits at (2135.95, −285.22). The ring anchor bearing is a
flat −104.8° and every ring destination is at exactly 8.0 yd radius.

What they get instead is six destinations, all 8 yd off the boss, cycling several times a second. At
4:19.3 – 4:19.9 Totemist, Obliteration, Ecoterrorist and Mighty all aim at the same point
(2145.1, −281.8), so the "three-slot spread" is not happening either. Two nodes issue the moves from
the same recompute — `thorim phase 2 positioning action` at ACTION_RAID (1,888 phase-2 move records)
and `thorim lightning charge action` at ACTION_RAID+4 (1,000) — frequently on the same tick to
different points. `flee` accounts for 105 records and `avoid aoe` came back USELESS.

Underneath, `TryGetThorimPhase2Spot` recomputes the melee spot from scratch on every evaluation out of
four inputs that each move on their own: `RingAnchorBearing` (live main tank), the live boss position,
`RingRotation` (re-solved per call), and `EnsureMeleeSlot`'s least-loaded pick after a prune that
erases silently. `RingRotation` depends on `ThorimChargedThunderOrb`, whose 500 ms cache is thrashing:
`thorim.chargedorb` toggles between `0` and a guid **78 times in 15 s**, i.e. a 150 yd grid sweep
several times a second.

### 3. Why Shadow and Assasin were stuck the whole fight

Both are gauntlet-squad melee. In the kill they stopped at (2147, −394, 438) and (2141, −351, 438) on
the balcony hallway and never moved again from 3:32 to the end at 6:30 — just under three minutes
each, at zero dps.

- 3:17.006 / 3:20.885 — both take Paralytic Field 62241 (15 s / 11 s). Balcony moves return `blocked`.
- 3:25.109 / 3:25.355 — `thorim movement guard` starts vetoing `thorim balcony advance action`.
  19 vetoes each, plus `follow` and `see spell`, to the end. From then their only `act` records are
  the balcony node at rel 0.0 IMPOSSIBLE and the dps node FAILED, roughly one per 5 s.
- 3:32.02 — Paralytic Field falls off. Nothing changes. They never issue another move record.

`ThorimMovementGuardMultiplier::GetValue` (`UldMultipliers_Thorim.cpp:215-235`) zeroes every
`MovementAction` outside a four-name whitelist when `ThorimMeleeRingSettled` is true, and the balcony
node is not on it. `ThorimMeleeRingSettled` (`UldEncounter_Thorim.cpp:1744-1754`) wants melee role,
live phase 2, and `ringArrived` holding the bot.

Neither bot has a single `thorim.ringarrived` note in this trace, so `ringArrived` already held them
when it opened. It came from earlier pulls: 00:27 ended `{Angry, Mighty, Shadow}` and 00:19 ended
`{Angry, Assasin, Bulwark, Obliteration}`. `balconyStep` leaked the same way — Shadow's first balcony
move at 3:11.333 goes straight to JUMP_START (2137.14, −291.19) while it stands at y −397, and
Assasin's goes to BALCONY_4 while it stands at y −416; neither is reachable from step 0. So did
`meleeSlots`: only 5 of 8 melee were freshly assigned in the kill — Angry, Assasin and Shadow carried
theirs in.

**Root cause is the reset gate.** `ThorimResetEncounterStateTrigger` needs
`ThorimBotHasEncounterState(bot) && ThorimEncounterStateIsStale(botAI)`, and
`ThorimEncounterStateIsStale` returns `state->engagedSeen` — one instance-wide flag.
`ResetThorimEncounterState(bot, false)` (`:1851-1907`) clears the calling bot's latches and then sets
`engagedSeen = false` at `:1906`. **The first bot to run it closes the gate for the other 24**, so
exactly one bot per wipe cycle gets cleared and the rest carry `ringArrived`, `meleeSlots`,
`balconyStep`, `arenaAnchorArrived`, `barrierBailing`, `orbEscapes` and `dpsTargets` into the next
pull.

The reset is doing two jobs at once: those per-bot erases, and a raid-wide wipe (`squads.clear()`,
`squadsAssigned`, `squadsNoted`, `humanSquadScanMs`, `marksCleared`, `bossGuid`, every scan cache).
The raid-wide half genuinely should run once — that is why the flag exists. Splitting the two is the
fix. There is only one caller and it always passes `clearInstance = false`, so `thorimStates.erase`
at `:1858` is dead code.

Thorim is the only Uld encounter with this shape. Vezax, Iron Assembly and Algalon all derive
staleness from the world rather than a stored flag, and nothing else has a reset at all — so the fix
stays local to Thorim.

### 4. Why the Paralytic Field traps came back — the same leak

**The route from `8c211ec2d` and `94c4e3b0c` is not at fault. Every bot that actually walked it came
through clean.** What fails is that most bots never walk it, because `balconyStep` is one of the
per-bot latches that leaks, and a bot that starts the pull at step 3, 4 or 5 gets handed a waypoint
from the middle of the chain while it is still standing at the top of the ramp.

18 Paralytic Field 62241 applications in the kill. Ten are first-pass; the other eight are re-triggers
every 50 s (15 s stun + the bunny's cooldown) on Shadow at (2140.8, −350.7) and Assasin at
(2147.3, −393.8) — the two bots the movement guard had already frozen, so they belong to the same bug
rather than to the route.

Of 13 gauntlet bots, **only three emitted a `thorim.balcony` note at all**, and one of those (Agony)
opened at 5. `ObsGuidMap::Set` emits only on change, so the other ten came into the pull holding
whatever step they finished the previous one on. What the balcony node then handed each of them on
its first move, straight from the ramp at (2165–2180, −432 to −436):

| bot | first destination | latch |
|---|---|---|
| Mighty, Justice | **B1 (2141, −408)** | fresh, step 0 |
| Assasin, Malediction | B4 (2151, −332) | carried in at 3 |
| Totemist, Elemena | B5 (2137.5, −318) | carried in at 4 |
| Shadow, Power, Agony, Druidica, Holylight | JUMP_START (2137.1, −291.2) | carried in at 5 |
| Bulwark | `JumpTo` JUMP_END (2137.9, −278.2) | carried in at 6 |

The walked tracks make the consequence exact. The chain forces the bot onto the east wall; a
mid-chain destination lets the navmesh take it up the middle, and the bunnies sit at x ≈ 2135:

| bot | x through the middle stretch | closest approach | trapped |
|---|---|---|---|
| Justice (fresh) | 2151 – 2152 | 16.5 yd | no |
| Mighty (fresh) | 2151 – 2152 | 16.5 yd | no |
| Holylight (leaked) | 2140 – 2145 | **9.8 yd** south, **10.5 yd** north | twice |
| Totemist (leaked) | 2140 – 2145 | **8.4 yd** south, **11.7 yd** north | twice |

**Mighty and Justice are the only two gauntlet bots with a step-0 latch, and they are the only two
melee gauntlet bots never caught by a bunny.** No bot with a clean latch was trapped.

The leak accumulates across a session, and so does the damage:

| attempt | gauntlet bots with a fresh latch | Paralytic Field applications |
|---|---|---|
| 00:10 | 8 of 12 | 1 |
| 00:19 | 5 of 13 | 10 |
| 00:27 | 2 of 13 | 9 |
| 00:37 | 3 of 13 | 18 |

One extra symptom worth its own line: a leaked step of **6** puts the bot past the end of the chain,
and `ThorimBalconyAdvanceAction` then fires `JumpTo(JUMP_END)` from wherever it stands. Bulwark did
exactly that at 3:25.310 from (2171.6, −443.4, 437.15) — a 180 yd arc on `EFFECT_MOTION_TYPE` that
took 21 s to cross the whole hallway. It happened to clear both bunnies by 26 yd and land on the
arena floor, but nothing about that starting position was checked.

---

## Fix 1 — five ranged anchors, off the Blizzard lane

**Files:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.h` (`:227-231` declarations, `:121` next to
`ULDUAR_THORIM_MELEE_SLOTS`), `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` (`:60-62` definitions,
`:1666-1680` the single read site), `src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp`
(`ThorimPhase2PositioningTrigger`, `:143-171`).

### 1a — three spots become five

Add `constexpr uint8 ULDUAR_THORIM_RANGED_SLOTS = 5;` beside `ULDUAR_THORIM_MELEE_SLOTS` and route
both hard-coded literals through it: the `slot = (slot + 1) % 3` at `:1675` and the
`static Position const* const rangedSpots[3]` at `:1678`. That array and that modulus are the only
things in the tree that depend on the count being three — nothing else reads the three symbols.

Coordinates, all navprobe-verified against map 603 (`docker compose --profile tools run --rm
ac-navprobe --map 603 point X Y Z`); Z is `UpdateAllowedPositionZ`, distance-to-poly ≤ 0.13 on all
five, and each has a normal path from the tank spot:

| slot | x | y | z | Blizzard clearance | dist to boss | clear of the melee ring |
|---|---|---|---|---|---|---|
| RANGE1 | 2147.0 | −261.0 | 419.736 | 17.1 yd | 21.2 | 13.2 yd |
| RANGE2 | 2137.0 | −254.0 | 419.800 | 19.5 yd | 25.9 | 17.9 yd |
| RANGE3 | 2126.0 | −260.0 | 419.798 | 21.0 yd | 22.9 | 14.9 yd |
| RANGE4 | 2123.0 | −272.0 | 419.684 | 16.2 yd | 16.4 | 8.4 yd |
| RANGE5 | 2135.0 | −266.0 | 419.846 | 28.2 yd | 14.1 | 6.1 yd |

Worst Blizzard clearance 16.2 yd, against 6.4 today. Pairwise separation 10.8 – 26.4 yd, so ten
ranged and healers stand two deep instead of three to four on one coordinate — the kill has a healer
dying to Chain Lightning with two more bodies 2.1 and 2.2 yd off it, against a 5 yd jump range.

All five sit in the arena's central column because that is the only safe ground: the bunny's lane
wraps both the east and the west edge, and nothing outside x 2122 – 2148 clears 16 yd. They are all
north of the floor hole — navprobe puts (2134, −292) 3.8 yd off the mesh with
`UpdateAllowedPositionZ` at −27.7, which confirms the `y = −288` note at `UldEncounter_Thorim.h:231-232`.

While in that loop: it walks the group with **no liveness or instance filter**, unlike
`EnsureMeleeSlot`, which goes through `MemberCounts`. Dead or out-of-instance ranged still consume
slots. With three spots and 25-man wrap-around that mostly self-corrects; with five it skews the
split visibly. Add the same filter.

### 1b — a Lightning Charge sidestep for the ranged: BUILT, MEASURED, BACKED OUT

Built as a rigid rotation of the whole ranged formation off the lit orb, the same shape as
`RingRotation`, hung on the 5 s `SPELL_THORIM_LIGHTNING_ORB_VISUAL` warning. Then costed, and it does
not pay:

- No parked bearing survives the cone. Seven orbs at 105° each cover every bearing off Thorim except
  a 27.9° window due south, which is the tank's and where the floor ends. So the formation has to
  walk out and back, every charge.
- Charges come every 10–12 s (`EVENT_THORIM_LIGHTNING_CHARGE` reschedules at 10 s). Five of the seven
  orbs put the wedge inside the cone. Mean walk for the worst-placed bot is 13.2 yd each way — about
  500 yd of walking per ranged bot over a 191 s phase 2, or most of every charge cycle spent moving.
  Narrowing the cone half-width from 52.5° to the spell's own 37.5° only takes that to 342 yd.
- What it buys is 647k of damage taken over that phase 2, about 3.4k incoming dps. Healing in the
  kill ran 33,580 hps against 9,651 incoming — a 3.5× margin, so that damage cost the raid nothing.
  Ten ranged at 6,380 dps losing even a third of their uptime is roughly 4.5M of damage not dealt.

Trading 4.5M of output for 647k of healed damage is the wrong way round, so the ranged keep holding.
The comment on `ThorimLightningChargeTrigger` now records that as a trade rather than a free call.

**Worth revisiting only for hard mode**, where the margin is gone (17.9k incoming against 20.7k hps)
and avoided damage is worth more than uptime. `IsThorimHardModeActive` already exists and the two Sif
triggers use it, so the gate is cheap — but it needs its own measurement, not this one.

### 1c — stop the anchor undoing a dodge

Give `ThorimPhase2PositioningTrigger::IsActive` the three bails `ThorimArenaPositioningTrigger`
already carries at `UldTriggers_Thorim.cpp:120-133`: charged orb, Sif Blizzard, Sif Frost Nova. Note
the ranged arm currently re-anchors on a 1.0 yd deadband, which is what makes the ping-pong so tight.

### 1d — raise the Blizzard clearance from 12 to 15

DBC radius is 13 and the searcher applies the aura through `IsWithinDistInMap`, which adds both
object sizes, so the effective grab runs a couple of yards wider — the Rune of Death precedent is DBC
13, measured 15.4. At 12 the dodge stops while still inside the field. Leave
`ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS` alone for now (DBC 12 on the damage effect, and its victims are
mostly a consequence of Sif teleporting on top of people rather than of a short clearance).

---

## Fix 2 — latch the melee ring bearing for the phase

**Files:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` (`EnsureMeleeSlot` `:167-205`,
`RingAnchorBearing` `:389-397`, `RingRotation` `:418-475`, `TryGetThorimPhase2Spot` `:1642-1717`,
`ThorimChargedThunderOrb` `:1756-1793`), `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.h` (state struct
`:241-333`), `src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp` (`:327-335`, `:374-400`).

- **Latch the bearing, not the point.** Store each melee bot's ring bearing in `ThorimEncounterState`,
  struck once when it first gets a phase-2 spot. The destination is then
  `boss + ULDUAR_THORIM_MELEE_RING_RADIUS at (latched bearing + active rotation)`, so a boss that does
  move is still tracked while nothing else can move the bot.
- **Latch the rotation per lit orb.** Solve `RingRotation` when the orb guid changes, store the
  answer, reuse it until the orb goes dark. Do not re-solve per tick.
- **Anchor on `ULDUAR_THORIM_PHASE2_TANK_SPOT`** rather than the live main tank, or latch the anchor
  bearing the same way. Measured, the live tank did not move in this fight, so this removes an input
  rather than fixing an observed failure — but it removes it.
- **Stop `EnsureMeleeSlot` re-picking.** The prune should drop only members who have actually left the
  instance; a momentarily dead bot keeps its slot. Once the bearing is latched the slot no longer
  feeds the position anyway, so a re-pick cannot move anyone.
- **One destination.** Both `thorim lightning charge action` and `thorim phase 2 positioning action`
  go through `TryGetThorimPhase2Spot`, so once the bearing and the rotation are latched they hand back
  the same `Position` and can no longer disagree on the same tick. Left as two nodes: the duplicate
  move records they produce are `dup` and cheap, and merging them means touching `UldStrategy.cpp`.
- **Fix the orb cache thrash.** `ThorimChargedThunderOrb` clears `state.chargedOrbGuid` before every
  rescan, so each rescan emits two notes even when the answer has not changed, and the rescans are
  landing well inside the 500 ms interval, which means `orbScanSpell` is being flipped between the two
  markers. Assign only when the answer changes, and hold the two markers in separate cache slots so
  phase 1's Charge Orb and phase 2's Lightning Orb cannot evict each other.
- Leave `ULDUAR_THORIM_RING_ARRIVE_TOLERANCE` (3.0) and `_REPOSITION_TOLERANCE` (5.0) alone. The
  ~300 `ringArrived` toggles per bot are a symptom of a moving target; measure again before tuning.

---

## Fix 3 — stop per-bot state leaking between pulls

This is the single root cause behind both the stranded bots and the traps.

**Files:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` (`ThorimEncounterStateIsStale` `:1809-1837`,
`ThorimBotHasEncounterState` `:1839-1849`, `ResetThorimEncounterState` `:1851-1907`,
`ThorimMeleeRingSettled` `:1744-1754`), `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.h` (state struct),
`src/Ai/Raid/Uld/Multiplier/UldMultipliers_Thorim.cpp` (`:215-235`).

- **Split the reset.** Per-bot half (`meleeSlots`, `ringArrived`, `barrierBailing`,
  `followMasterStripped`, `arenaAnchorArrived`, `orbEscapes`, `petRecalls`, `petRecallMs`,
  `balconyStep`, `dpsTargets`) runs once per bot per engagement cycle, gated on a new `resetDone`
  guid set. Raid-wide half (`squads`, `squadsAssigned`, `squadsNoted`, `humanSquadScanMs`,
  `marksCleared`, `runicSmash*`, the scan caches, `bossGuid`) keeps running once, gated on
  `engagedSeen` as today. Clear `resetDone` wherever `engagedSeen` flips back to true
  (`:1829`), so the next wipe re-arms every bot.
- **Complete `ThorimBotHasEncounterState`.** It checks `meleeSlots`, `ringArrived`, `barrierBailing`,
  `squads`, `followMasterStripped`, `arenaAnchorArrived` and `runicSmashSide`, but not `orbEscapes`,
  `petRecalls`, `petRecallMs`, `balconyStep` or `dpsTargets` — a bot holding only those never trips
  the reset at all.
- **Make the balcony step latch self-healing** (`ThorimAdvanceBalconyStep`, `:1055-1086`). The latch is
  monotonic by design so a bot shoved north is not sent back south, and that property must stay. But
  the opposite case is unambiguous: a bot that is far *south* of the waypoint it claims to have passed
  has not passed it. On entry, walk the stored step back to the first waypoint the bot has genuinely
  not reached — the same `GetExactDist2d <= ULDUAR_THORIM_BALCONY_ARRIVE_TOLERANCE || y > waypoint.y`
  test the loop already uses, applied downward. That alone would have kept every bot on the route
  today, and it also covers knockbacks and teleports, not just the leak.
- **Bound the end-of-chain jump.** At step 6 the node fires `JumpTo(JUMP_END)` from wherever the bot
  stands (`UldActions_Thorim.cpp`, the `bossDown` branch). Gate it on the bot actually being at the
  edge — the `ULDUAR_THORIM_JUMP_START_TOLERANCE` distance test is already in that condition as the
  other arm of an `||`; the `step >= WAYPOINTS` arm needs a distance sanity bound of its own so a
  stale latch cannot launch a 180 yd arc from the ramp.
- **Defence in depth for the freeze.** Either of these alone would have saved this fight; take both.
  - `ThorimMeleeRingSettled` should also require `bot->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD`,
    the same test `TryGetThorimPhase2Spot` applies at `:1650-1651`. A bot on the balcony is never a
    settled ring member.
  - Add `thorim balcony advance action` to `ThorimMovementGuardMultiplier`'s whitelist.
    `ThorimBalconyGuardMultiplier` (`UldMultipliers_Thorim.cpp:150-190`) already follows the house
    pattern of always leaving one mover live; the phase-2 guard is the only one of the four Thorim
    guards with no fallback mover, and its sole unfreeze path is `ThorimRingNeedsMove` erasing
    `ringArrived`, which is driven only by `ThorimPhase2PositioningTrigger`.

---

## Deliberately not doing

- **Nothing for hard-mode survivability.** Decided. Frostbolt Volley is 30.7% of hard-mode phase-2
  damage at DBC radius 200 and has no positional answer, and the 17.9k-incoming-vs-20.7k-healing gap
  is a healing problem, not a placement one.
- **`ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS` stays at 12.** See 1d.
- **The ~28 s master-gated corridor start.** Still open from the previous round.
- **The hard-coded `> 110.0f` bails** in `ThorimGauntletPositioningTrigger`
  (`UldTriggers_Thorim.cpp:67`) and `ThorimFallFromFloorTrigger` (`:139`) against the left lane's
  117.3 / 120.8 / 126.1 yd waypoints. Still open from the previous round.
- **No change to the balcony waypoints, the arrive tolerance or the jump point.** The route is doing
  its job: 16.5 yd of clearance for every bot that walked it, nobody on it trapped. The traps are a
  state-leak symptom and fix 3 owns them.

---

## Verification

Static, before any pull:

- `grep -n "ULDUAR_THORIM_RANGED_SLOTS" src/Ai/Raid/Uld/Util/UldEncounter_Thorim.*` shows the
  constant and no surviving `% 3` or `rangedSpots[3]` literal.
- `grep -n "engagedSeen\|resetDone" src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` shows the raid-wide
  half gated on the flag and the per-bot half on the set.
- `grep -n "balcony advance" src/Ai/Raid/Uld/Multiplier/UldMultipliers_Thorim.cpp` finds it in the
  phase-2 whitelist.
- `python apps/codestyle/codestyle-cpp.py` passes; no line over 120 columns; files stay LF.
- `python tools/botobs/postmortem.py env/dist/logs/botobs/603_1_thorim_1788557437.ndjson` still runs.
- Per-TU syntax check, which does work headless. The `acore/ac-wotlk-build:master` image carries both
  a compiler and `/azerothcore/build/compile_commands.json`, so a changed file can be checked on its
  own: mount `modules/mod-playerbots/src` read-only over the image's copy, pull the entry for the file
  out of the compile database, drop `-c` and `-o`, add `-fsyntax-only`, and run it from its
  `directory`. All four files here come back clean that way.

  It does not link, so it will not catch an ODR clash or a missing symbol. A full build still has to
  happen elsewhere.

In game, one 25-man pull that reaches hard mode, then re-read the fresh trace:

1. **No Blizzard 62602 damage on anyone.** Was 123,539 across eight ranged and healers, all of it on
   RANGE1 and RANGE3. This is the headline for question 1.
2. **Lightning Charge 62466 on ranged + healers stays about where it is** (647,434 in the kill).
   Deliberately not dodged, and 1b says why.
3. **Melee travel in phase 2 drops** from 947–1429 yd to roughly a hundred, and % of snapshots moving
   from 74–92% to under 20%.
4. **Melee phase-2 dps closes on the ranged.** The ratio was 0.58; expect well past 0.8.
5. **No gauntlet bot idles above z 429.6.** Shadow and Assasin sat there for three minutes.
6. **Every gauntlet bot emits a `thorim.balcony` note starting at 0**, and every melee a fresh
   `thorim.slot` note. Was 3 of 13 and 5 of 8. This is the direct test of fix 3, and it is worth
   running on the *second* pull of a session rather than the first, since the leak only shows after a
   wipe.
7. **No Paralytic Field 62241 / 63540 on anyone.** Was 18. Cross-check the walked tracks: every
   gauntlet bot should sit at x ≈ 2151 through the y −400 to −330 stretch, not x ≈ 2141.
8. **Nobody jumps from the ramp.** No `JumpTo` toward (2137.88, −278.19) issued from further than the
   jump tolerance; Bulwark's 180 yd arc should not recur.
9. **`thorim.chargedorb` note count collapses** from ~78 per 15 s to a handful.
10. **Regression on the previous round.** The balcony chain still steps through B4 (2151, −332) and
    B5 (2137.5, −318) before JUMP_START, the squad still lands, and squad 2 still has 12 bots.
