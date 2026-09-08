# Mimiron Firefighter: stop the oscillation, lap the handover, step out of Rapid Burst

## Context

Yesterday's commit `4b44edfd1` fixed the two things that were ending Firefighter pulls. Today's two
attempts confirm it worked and expose what is next.

| | 7848 (07th) | 8182 (07th) | **894936 (08th)** | **895606 (08th)** |
|---|---|---|---|---|
| wipe at | 3:45 | 3:33 | **4:16** | **4:31** |
| deaths | 29 | 32 | 32 | 28 |
| Frost Bomb | 697,314 (22.9%) | 612,008 (20.2%) | **83,180 (2.2%)** | **0** |
| Frost Bomb deaths | 13-15 in 0.15 s | 13-15 in 0.15 s | **1** | **0** |
| fire, phase 2, per living bot per second | 449 | 426 | **356** | **332** |
| fire episodes 5 s or longer | 5% | – | **1%** | **2%** |
| `avoid aoe` accepted moves | 133 | 277 | **0** (339 vetoes) | **0** (309 vetoes) |

The Frost Bomb is solved, the fire tail is solved, and phase 2 now runs 87-109 s instead of 60-72 s.
What replaced them:

- **Phase 1 got slower.** Raid DPS 112,503 / 112,345 → **96,224 / 103,609**, and phase 1 ran 108.3 s
  and 100.8 s against 92.8 s and 92.9 s. That is the oscillation, and it is measured in F1.
- **Phase 2 is a healing race, lost by about 7,000 damage per second.** Incoming 29,207/s and
  20,366/s against 21,389/s and 14,703/s effective healing; a 617-679k deficit over the phase.
  Nothing one-shots any more, the raid runs out of health from 3:20 onward.

Sources: the traces in `env/dist/logs/botobs/`, `boss_mimiron.cpp`, the DBC reference CSVs,
`acore_world`, and navprobe. Cheats-free stands: no `HasCheat`, no `TeleportTo`, no `->Kill(` in the
Mimiron files.

## Corrections to yesterday's analysis, recorded so they are not repeated

1. **The hmwedge did not cost Rapid Burst.** Raw victims-per-tick rose 1 → 3, but yesterday's raid was
   down to 7-10 alive for most of phase 2 because the Frost Bomb killed 13-15 at once. As a share of
   the *living* raid: 25% / 22% (ring) against 20% / 30% (wedge). Per living bot per second: 545 / 423
   against 590 / 598. Unchanged. The wedge stays.
2. **Rapid Burst's cone is ±30°, not ±55°.** `acore_world.spell_cone` gives `ConeDegrees = 60` for
   63387/64019/64531/64532 — 60° total, 100 yd (`spell.dbc` radius index 12). Wider angles in the
   trace are stale facing: snapshots are 250 ms and the boss re-aims between them. Against ±30°,
   93-97% of hits land inside. This is what makes the dodge cheap.
3. **Melee never have a `disperse distance` on Mimiron.** `MimironPhase1PositioningTrigger::IsActive`
   opens `if (!botAI->IsRanged(bot)) return false;`, and `DisperseDistanceValue` defaults to `-1.0f`,
   which `CombatFormationMoveAction::Execute` rejects. The unstacker runs for ranged only, in phase 1
   only. Any reasoning that has melee keeping it is wrong.

## The mechanics, as the server implements them

- **Rapid Burst.** `EVENT_SPELL_RAPID_BURST` picks a random player within 80 yd, casts **63382 on
  that player**, then `SetFacingToObject(p)`; repeats every **3.2 s**. 63382 is a 3000 ms aura with a
  500 ms periodic dummy — **6 ticks**. Each tick `caster->CastSpell(nullptr, 64531|64532)`, a **60°
  cone from VX-001 along its facing**, 100 yd, 1884 base. VX-001 never moves, so the centreline is
  exactly `VX-001 → the aura carrier`, fixed for 3 s. **Phase 2 only**:
  `_events.RescheduleEvent((_phase == 2 ? EVENT_SPELL_RAPID_BURST : EVENT_HAND_PULSE), ...)`.
  Not hard-mode gated — normal mode eats the same cone.
- **Heat Wave 64533.** Self-cast every 10 s in phase 2, radius index 28 = **50000 yd**. Raid-wide and
  unavoidable: 936,259 (24.4%) and 750,071 (22.9%), the largest single source in both pulls.
- **Napalm Shell 65026.** 5 yd splash at a targeted player, Mk II only. This is the unstacker's job.
- **Fire.** Unchanged: 3 seeds every 30 s at 5 yd from random members; chains grow 7 yd every 5.75 s
  toward the globally nearest player, so **1.22 yd/s**; 3 yd aura at ~3.1k a tick; frozen while a
  player is inside 4 yd of the newest node.
- **Room floor.** navprobe, map 603, centre `(2744.65, 2569.46, 364.31)`: **24/24 on mesh, settledZ
  364.314 at r=35, 40 and 44**. r=48 puts three east headings 3-6 yd off the nearest poly; r=52 has
  one off-mesh. **44 is the outermost clean ring.**

---

## F1 — The wedge sits exactly on the unstack threshold (the oscillation)

`ULDUAR_MIMIRON_PHASE3_SPACING = 6.0f` is the hmwedge's row spacing.
`MimironPhase1PositioningAction::Execute` sets `disperse distance` to **6.0f**.
`CombatFormationMoveAction::Execute` shoves a bot `dis` yards off any group member closer than `dis`,
once a second, via `FleePosition` — and **always returns false**, so it never claims the tick; it
moves the bot underneath whatever else runs. `mimiron arc spread action` at `ACTION_RAID` then walks
it back. Neither side has hysteresis: `IsDuplicateMove`'s epsilon is 0.01 yd and
`IsWaitingForLastMove` only holds for the last leg's travel time.

The wedge is tight by construction:
`rangedDepth = spellDistance(28.5) − SPREAD_RANGE_MARGIN(4) − PHASE3_MIN_RADIUS(18) = 6.5`, so
`MimironWedgeRows` gets `1 + 6.5/6 = 2` rows — 14 ranged in two rows of seven. Radial gap 6.0; inner
row, 7 across 120° at r=18, is 37.7/6 = **6.3 yd**.

Ranged-only nearest neighbour, phase 1 — the only bots the unstacker touches:

| | 7848 (ring) | **894936** | **895606** |
|---|---|---|---|
| median | 8.8 yd | **6.1** | **6.0** |
| inside 5 yd (Napalm's radius) | 10% | **31%** | **30%** |
| inside 6 yd (unstack threshold) | 15% | **47%** | **42%** |

And the cost, phase 1, ranged and healers:

| | 7848 | **894936** | **895606** |
|---|---|---|---|
| `combat formation move` accepted | 51 | **261** | **198** |
| `mimiron arc spread action` issued | 617 | **1028** | **941** |
| accepted moves, all sources | 345 | **605** | **494** |
| distance walked | 1.79 yd/s | **2.93** | **2.91** |
| direction reversals | 56 | **184** | **180** |
| fraction of samples moving | 26% | **42%** | **42%** |
| output per ranged bot | 5,738 dps | **5,039** | **5,004** |

A ranged bot moving 42% of the time instead of 26% cannot cast for the difference, and the 13-17%
output drop is that number.

**Fix — both halves.**

- **`disperse distance` 6.0 → 5.5.** Napalm Shell's radius is 5.0 and the wedge's minimum on-slot
  spacing is 6.0, so 5.5 lands in the gap: it still unstacks anyone genuinely inside Napalm range and
  never fires on two bots both standing on their slots. One constant, trivially revertible.
- **A multiplier zeroing `combat formation move` while the bot is within
  `ULDUAR_MIMIRON_SPREAD_TOLERANCE` (5 yd) of its slot.** Match **by name**, not by type —
  `TankFaceAction` derives from `CombatFormationMoveAction` and does real work (13-15 accepted moves
  a pull). On-slot the arc spread also declines to act, so inside the tolerance nothing moves the bot
  and outside it the arc spread owns the correction. This is what makes the fix survive a later edit
  to the wedge geometry.
- Comment the invariant at `PHASE3_SPACING`: **row spacing > disperse distance > Napalm's 5 yd.**

## F2 — Melee are walked back into the fire by an unguarded `reach melee`

`DeriveMimironSpreadSlot` returns `false` for melee mid-phase, so nothing Mimiron-specific holds a
melee bot's position. The loop:

1. `mimiron dodge flames` (`ACTION_RAID + 4`) throws the bot **12 yd** out.
2. The move lock expires after that leg's travel time, ~1.7 s.
3. The dodge trigger goes quiet and the tick falls to **`reach melee` at relevance 21**, which closes
   to `0.75 +` both combat reaches — onto ground the chains are crawling toward.
4. A chain adds a node every 5.75 s toward the nearest player. Back to 1.

`MimironChargeGuardMultiplier` exists for exactly this but gates on
`dynamic_cast<CastReachTargetSpellAction*>`, and `ReachMeleeAction : ReachTargetAction :
MovementAction` is a different branch. Charge and Intercept are suppressed during a lethal window;
plain `reach melee` is not.

Melee time within 5 yd of the Mk II across phase 1: **33% / 23% → 11% / 11%**. Melee median distance
to the Mk II 6.7 / 8.4 → **10.2 / 9.3**.

**Fix — three parts.**

- Extend `MimironChargeGuardMultiplier` to also match `ReachTargetAction`. `MimironLethalWindowActive`
  is already the right predicate and already includes `MimironDodgeFlamesTrigger`.
- **Shortest safe hop.** The dodge asks the fan for one fixed distance,
  `FLAMES_RADIUS + spread + FLAMES_STEP` ≥ 12 yd, and the notes say **`flames fallback` 2121 against
  `flames ok` 468** — 82% find no clean bearing at that distance and take the unscreened fallback.
  Wrap the bearing sweep in a distance ladder (cluster edge + 3, +5, +7, +10) and take the first pair
  clearing every node the bot knows about. Lands clean more often *and* closer. Record the chosen
  distance in the `mimiron.flee` note.
- **Health gate, all roles.** Only dodge below `ULDUAR_MIMIRON_FLAMES_DODGE_HEALTH_PCT = 60.0f`, with
  one override: dodge regardless when **two or more nodes** are inside the radius, which is the case
  that kills in ~4 s rather than ~8. Known knock-on, accepted: `MimironLethalWindowActive` calls the
  same trigger, so above 60% the charge and `reach melee` guards also stand down — a healthy melee bot
  will Charge across a burning floor. That is self-consistent with having decided the fire does not
  matter above 60%. Risk to watch: phase 2 takes **1,600 damage per bot per second** from all sources
  and phase-2 overheal is only 22-27%, against phase 1's 68-72% — there is no healing slack in phase 2.
  Both numbers are single named constants.

## F3 — Step out of the Rapid Burst cone

Centreline knowable, window 3 s, escape short because the cone is only ±30°:

| escape arc needed | 894936 | 895606 |
|---|---|---|
| p25 | 3.3 yd | 3.7 yd |
| median | **6.7 yd** | **6.3 yd** |
| under 6 yd | 47% | 46% |
| under 9 yd | 63% | 66% |
| p90 | 16.9 yd | 17.0 yd |

A 9 yd threshold reaches about two thirds of victim-ticks, and a ~1.3 s step saves 4 of the 6 ticks
on each. Against 10,756/s and 8,449/s of Rapid Burst that is roughly 4,000/s of a 29,200/s incoming —
the largest avoidable slice left in phase 2 after fire.

**Fix.** New `MimironRapidBurstTrigger` / `MimironRapidBurstAction`, **in both normal and hard mode**
since the mechanic is not hard-mode gated.

- Trigger: VX-001 alive, some group member carries **63382**, the bot is inside ±30° of the bearing
  from VX-001 to that member, and the arc to clear it — `(30° − own offset) × bot radius` — is at or
  under `ULDUAR_MIMIRON_RAPID_BURST_MAX_STEP = 9.0f`. Above that the bot stands still, which is the
  honest answer for the p90 case. Read the centreline from the carrier's position, not VX-001's
  orientation.
- Action: step tangentially to the nearer cone edge plus a small margin, through the existing
  `MoveAwayClearOfMines` fan so it inherits mine, fire and bomb screening, at `MOVEMENT_FORCED`, with
  a `mimiron.flee` row keyed `rapidburst`.
- **The main tank steps in phase 2 and holds in phase 4.** Phase 2 has no `p4tank` spot to protect and
  VX-001 does not move; phase 4 does and it does.
- `MimironArcSpreadTrigger::IsActive` yields while this window is live and the bot's slot is inside the
  cone, so the formation does not pull it straight back. The file already does exactly this for
  `MimironP3Wx2LaserBarrageTrigger`.

**Ladder — top, and it is a free insert.** Rapid Burst is phase 2 only, so it can never contend with
the Laser Barrage node at all, and the two lethal nodes it now outranks in phase 2 — Frost Bomb (10 s
fuse) and Rocket Strike — both carry multi-second warnings. So nothing else moves:

| node | now | after |
|---|---|---|
| **`mimiron rapid burst`** | – | **+8** |
| `mimiron p3wx2 laser barrage` | +7 | +7 |
| `mimiron frost bomb` | +6 | +6 |
| `mimiron rocket strike` | +5 | +5 |
| `mimiron dodge flames` | +4 | +4 |
| `mimiron shock blast` | +3 | +3 |

## F4 — The handover: stack at the wall and lap it

Today's handovers were quiet by accident. Bots leave combat when the mechs go unselectable, and the
**non-combat engine's `follow` dragged the raid to wherever the human master stood** — `follow` 1686
issued / **368 accepted** against `mimiron arc spread action` 894 / 163. Measured: the raid tracked
the two humans out to r≈48-53 west and back.

The strategy is on **both** engines (`PlayerbotAI::ApplyInstanceStrategies`), so `ACTION_RAID`
outranks `follow` at 1.0 even out of combat. `follow` won because `MimironArcSpreadAction::Execute`
**returns false** when `IsMimironSpotSafe` rejects the slot, and with 26-35 nodes live and a 30 yd
Frost Bomb clearance it rejected constantly. `stagemelee` and `stagering` appear in `mimiron.slot` for
about one second each, at t=139 and t=131 — the 30/36 yd radii from yesterday are effectively inert.

**Fix.** Under Firefighter, replace both staging branches with one lap the whole raid walks together —
tank, melee, healers and ranged on the same moving point. **All three handovers** (1→2, 2→3, 3→4); the
branch is the same code and they all converge on the room centre. Branch name `hmlap`.

- Radius `ULDUAR_MIMIRON_HM_HANDOVER_LAP_RADIUS = 44.0f` — navprobe 24/24, settledZ 364.314.
- **Step-and-pause**, not a continuous walk: about 10 yd of travel, then a hold of about 2.5 s.
  Average ~3 yd/s, still 2.5x the chains' 1.22 yd/s so the fire trails behind, and the holds are what
  let healers cast. A continuous 4.5 yd/s walk would trade the fire problem for a healing one: the
  handover currently runs 3,666/s and 1,939/s incoming against 4,024/s and 1,920/s effective healing,
  which is break-even, and a permanently-moving raid would enter phase 2 low.
- Bearing advances from the server clock so every bot computes the same point with no shared state:
  `bearing = base + step × floor((getMSTime() % period) / stepPeriod)`, base on the room-centre-to-
  `ULDUAR_MIMIRON_PHASE3_STAGE` bearing (the east gap the wedge already uses).
- Pack width `ULDUAR_MIMIRON_HM_HANDOVER_LAP_ARC` ≈ 20°, indexed the way the ring is. Nothing but fire
  and leftover mines hits the raid during a handover — measured composition was Flames 52% and mine
  Explosion 48% — so stacking tight is free here.
- **Never return false.** If a waypoint fails `IsMimironSpotSafe`, advance the bearing over a bounded
  sweep; if the whole sweep fails, stand still and still return true. Handing the tick back during a
  handover is what summons `follow`.
- No park: walk until the phase goes live and run in from wherever the raid is, at most 44 yd ≈ 6 s.
  Handover length varies 24-48 s with nothing observable saying "nearly done", and phase 2 opens with
  a 1 s Frost Bomb on a 10 s fuse, not an instant hit.

Normal mode keeps the 8 yd melee ring and 22 yd stagering untouched.
`ULDUAR_MIMIRON_HM_STAGING_MELEE_RADIUS` and `..._RANGED_RADIUS` become unused and go.

**What the lap buys, stated so the next trace is not misread:** a clean *start* to phase 2, not a
clean phase. Once the raid runs in, every chain re-aims at it and crawls inward at 1.22 yd/s; across
an 87-109 s phase 2 that is 106-133 yd of growth, far more than the 44 yd back to the middle. The
middle will be burning again before the phase ends, and that is expected.

## Not fixed, and why

- **Heat Wave** — 24.4% and 22.9% of everything, raid-wide at 50000 yd. No bot-side answer; it is why
  phase 2 is a healing race. Worth stating in the doc so it is not re-investigated.
- **`IsMimironSpotSafe` screens the destination, not the path.** Power died at 2:26 standing on three
  Proximity Mines while walking to a staging slot the check had passed. Real, separate change.
- **`AvoidAoeAction`'s own two bugs** — the ignored `FleePosition` result and the `lastMoveTimer` only
  assigned under `tellWhenAvoidAoe`. Confirmed silent today (339 and 309 vetoes, zero accepted moves).
  Fixing the action itself still belongs in its own change.
- **Emergency Fire Bot contradiction** — still unresolved, still no trace that reaches phase 3.

## Files

- `src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.h` / `.cpp` — lap constants and the `hmlap` branch, dodge
  health constant, Rapid Burst cone constants and the offset/escape helper, the `PHASE3_SPACING`
  invariant comment, drop the two staging radii
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.h` / `.cpp` — `MimironRapidBurstTrigger`, the health
  gate and two-node override on `MimironDodgeFlamesTrigger`, the arc-spread yield
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.h` / `.cpp` — `MimironRapidBurstAction`, the distance
  ladder in the flames dodge and the widened `mimiron.flee` note, `disperse distance` 6.0 → 5.5
- `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Mimiron.h` / `.cpp` — `ReachTargetAction` in the charge
  guard, new name-matched on-slot guard for `combat formation move`
- `src/Ai/Raid/Uld/UldStrategy.cpp` — the `+8` node and the new multiplier
- `src/Ai/Raid/Uld/UldActionContext.h`, `UldTriggerContext.h` — the two registrations
- `docs/raids/ulduar/mimiron.md` — the findings, per the raid-findings convention. Run
  `/compact-docs-writer` up front, not as cleanup.
- `docs/plans/mimiron-oscillation-and-handover-lap/…PLAN.md` — mirror of this plan

Another session is editing Thorim files in this working tree. Stage only Mimiron work.

## Verification

The module cannot be compiled from this checkout; the worldserver Docker build is the only compile
path, so a rebuild comes first. Before that: the per-TU syntax check against the build image's
`compile_commands.json` on every changed translation unit, and the cheat grep.

Then one Firefighter pull, read with `tools/botobs/postmortem.py`:

1. **F1.** `combat formation move` accepted for ranged back near the 51 baseline, not 198-261. Ranged
   fraction-of-samples-moving back toward 26% from 42%, walking back toward 1.8 yd/s from 2.9,
   reversals back toward 56 from 180-184. Phase 1 raid DPS back over 110,000 and phase 1 under 95 s.
   Ranged-only nearest neighbour inside 5 yd should **not** rise above today's 30-31% — that is the
   Napalm Shell check on the 5.5 yd threshold.
2. **F2.** Melee time within 5 yd of the Mk II back over 23%, from 11%. `mimiron.flee` shows
   `flames ok` overtaking `flames fallback` (468:2121 today). Median accepted hop for
   `mimiron dodge flames action` below today's 11.9 yd while fire episodes of 5 s or more stay at
   1-2%. If phase-2 fire per living bot per second rises above today's 356/332, raise the 60%.
3. **F3.** `mimiron rapid burst action` appears with `rapidburst ok` rows. Rapid Burst per living bot
   per second falls from 590/598. Victim-ticks whose escape arc was under 9 yd largely disappear; the
   p90 cases remain by design. Confirm the phase-4 tank does **not** step.
4. **F4.** `mimiron.slot` shows `hmlap` across the whole handover, not one second of `stagering`.
   Accepted `follow` moves during the handover near zero, against 368 today. Nodes born during the
   handover at median r near 40 and the fraction inside 25 yd down from 67-69%. Handover effective
   healing still covering incoming — the step-and-pause check. Phase-2 fire in the **first 30 s** down;
   no claim about the back half.
5. **Phase 3 and 4 at all.** No Firefighter pull has reached them. The first trace that does says how
   bad phase 3 fire is.
6. **Normal mode.** F3 is deliberately not behind the hard-mode check, so a normal Mimiron clear needs
   its own pass: `mimiron rapid burst action` firing, no new deaths, and a `--track` of two or three
   bots showing no other behaviour change. F1's `disperse distance` edit also lands in normal mode.
7. Cheat grep still clean on the Mimiron files.
