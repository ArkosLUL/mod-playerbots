# Hodir

Anchored in the **south-west corner**, `(1974.50, -275.50, 432.687)`. The room is x 1965-2041, y -170
to -298, and he evades outside that y band. Cornering him collapses the helper NPCs' 17-30 yd
stand-off arc into one place, which is the only way the buff zones land somewhere predictable. That
corner is **chamfered** — the floor bevels from ~(1966, -274) to ~(1990, -298) — so the tank spot is
the deepest point with 6 yd of floor all round, not the visual corner, which has three yards of
nothing behind it. Off-tank `(1980.00, -277.00)` sits deeper in rather than toward the raid, so a
taunt never walks him at the stack. All spots are navprobe-verified.

**Starlight measures 3 yd, whatever its DBC row says.** `62807` is aura **193
`SPELL_AURA_MELEE_SLOW`**, whose handler `HandleModCombatSpeedPct` applies `ApplyCastTimePercentMod`
as well as all three attack timers, amount 50 — **+50% haste to casting and swinging**, the biggest
throughput lever in the fight. The row says 8. Binned by distance the hold rate is 94/90/78% across
the first three yards and 21% in the fourth, so the edge is 3 and the medians that once read as 4 were
sampling blur — bots cover 1.75 yd between snapshots. 3 yd holds *one* bot at the 4.5 yd spacing
icicles force, so Starlight is a **per-bot opportunity, never something to build a formation on**:
`ULDUAR_HODIR_STARLIGHT_STAND_RADIUS` 1.5 plus 1.0 tolerance, `static_assert`ed to stay inside it, and
a 30 yd search radius because the step runs per bot per tick. **Take the margin out of the stand
radius, never the tolerance** — tolerance also stands the position trigger down, so trimming it just
moves churn from the dodge to the anchor.

**The formation is built to ride a Toasty Fire.** `62821` measures true to its 11 yd (with-aura p90
11.9) and stops Biting Cold, which is otherwise a tenth of the raid's time spent walking. Slots sit on
two concentric rings, `ULDUAR_HODIR_RAID_RING_INNER` 4.5 and `_OUTER` 9.0 with six inner slots — 4.5 yd
minimum separation, so Ice Shards' 4 yd splash catches one bot instead of five, and a bot shedding
Biting Cold can step outward without closing on its neighbours. Outer ring plus its 2 yd arrival
tolerance is exactly 11, `static_assert`ed to stay inside the fire. The centre is gated 15 yd off
**Hodir himself**, not the tank spot he leaves — he drifts 10-25 yd, and a fixed-point gate once let
the centre land 6.8 yd from him.

**In practice it never does.** A fire must also leave the whole outer ring inside
`_CASTER_MAX_BOSS_GAP` 30, so it has to sit 15-19 yd from him, and **0 of 556** sampled fires did
(p50 26.4, p10 23.2). `hodir.fire` reads `none` for every bot all fight, the centre stays
`ULDUAR_HODIR_RAID_ANCHOR`, and the raid pays the shuttle instead. That is geometry, not a bad
constant: a 12-bot ring cannot sit on a fire 26 yd out and keep its far side within 30 of the
boss, so widening the band just walks the far half out of casting range.

**Toasty Fire grants no Flash-Freeze exemption.** It is 11 yd and only blocks Biting Cold. The one
exemption is `SPELL_SAFE_AREA_TRIGGERED (62464)`, off `65705` on **NPC 33174**, radius index 40 → 9 yd.

| Mechanic | Ids | Numbers that drive the code |
|---|---|---|
| Flash Freeze | 61968 | **9s cast**, every 48-49s, 200 yd. Spares only 62464 carriers and pets |
| Shelter chain | 33173 → `62460` → `65370` + `62463` | Drift lands at T+3.8 → Ice Shards 14,000 in **7 yd** → summons **33174**, 12s. Freeze lands T+9: **6.3s of shelter** to cross the room |
| Trapped player | 61969 / 62226 | **300s**, and the *next* Flash Freeze **instakills**. Free by killing NPC **32926** (helpers: 32938) |
| Small icicles | 62227 → 63545 → 33169 → `62457` | **Every 2s** on 1 random player, falls after 2s, **14,000 Frost in 4 yd** + knockback. Off for 12s (25m) / 24s (10m) after each Flash Freeze |
| Biting Cold | 62038 / 62039 | **1005 ms ticks**, `200 · 2^stacks`. Stacks after 4 stationary ticks, sheds on the **second consecutive** tick the server reads `isMoving()` |
| Frozen Blows | 62478 / 63512 | 20s, **15s after each Flash Freeze**; +31,061 / +39,999 per swing (the damage itself is `63511`) plus a 3,999 raid tick (`64545`). All four rows are named "Frozen Blows"; the aura ids apply, the damage ids hit |
| Freeze | 62469 | Random player in 50 yd every 17-20s, 5,549 + root in 10 yd, **dispellable (Magic)** |
| Storm Cloud → Storm Power | 65123/65133 → 63711/65134 | Carrier holds **4 (10m) / 6 (25m)** stacks, one per second — **4-6 seconds of use**. Storm Power is **3 yd**, +134% crit damage |
| Toasty Fire | 62821 | 11 yd, 60s, at the mage's feet every 10s. **Flash Freeze wipes every fire** (62148) |
| Berserk | 26662 | 8 min, unhandled — no enrage awareness exists anywhere in the module |

**Hard mode is gone as a config.** The "Rare Cache of Winter" kill needs no different
behaviour: the helpers *are* the raid's damage, the fire is the Biting Cold answer, and Storm Power
is the biggest buff in the fight, so all three run on every pull. `AiPlayerbot.UlduarHodirHardMode`
and `IsHodirHardModeActive` were deleted rather than left gating nothing.

**The deadline is 3:00, not the 2:00 the guides give.** `SPELL_SHATTER_CHEST_TIMER` is **65272**, an
`EffectAuraPeriod_1` of 180000 ms on a 180000 ms duration, cast on engage at `boss_hodir.cpp:263`;
its single tick fires `EVENT_HARD_MODE_MISSED` and shatters the Rare Cache. So the target is halving
this fight, not thirding it.

## The Flash Freeze window is nine seconds and the shelter exists for six

The drift lands ~3.8s into the 9s cast, so the shelter covers only the last **6.3s** — the whole
budget for crossing the room, against a measured 30 yd median run at ~7 yd/s costing 4.3s of it.

- **The run parks at 6 yd and releases at 8** (`ULDUAR_HODIR_SAFE_AREA_TOLERANCE` / `_RELEASE`, both
  `static_assert`ed inside the 9 yd Safe Area). `MoveInside` → `MoveNear` lands the bot at *exactly*
  the tolerance, so testing one number at both ends stood the trigger down the tick it arrived and
  handed the next tick to the ring anchor — measured walking bots 18-22 yd back out with the freeze
  2s away.
- **Every other mover stands down for the whole cast, every role.** `HodirGuardMultiplier` zeroes
  gap-closers (`CastReachTargetSpellAction` — Charge, Intercept, both Feral Charges) and every
  `MovementAction` bar the shelter run and the icicle dodge. `ReachTargetAction` is in scope;
  `AttackAction` is exempt because it only sets a target. Measured before the gate: 68 of 125
  bot-freeze pairs had their last accepted move come from something else, 36 of them the ring anchor
  and 25 `reach melee` / `reach spell`.
- **Do not instead return `true` from the shelter action.** `MoveTo` answers Duplicate for a
  destination it already issued, so from the second tick of a run `Execute` returns false and the
  engine descends past `ACTION_RAID + 6` — that descent is correct, and holding the tick would
  silence the bot's casting for six seconds, seven times a pull. The movers below are what has to be
  off.
- **The window is gated on the cast, not a timer.** `IsHodirFlashFreezeIncoming` tests
  `UNIT_STATE_CASTING` plus `FindCurrentSpellBySpellId` over **every** cast slot rather than
  `CURRENT_GENERIC_SPELL`: which slot a scripted boss cast lands in is the script's business, and
  guessing wrong opens the window on nothing. `UldTriggers_Mimiron.cpp:31` is the same shape.
- **The shelter run keys off 33174 existing**, not off the boss casting. Starting when the drift
  spawns puts the raid under a 14,000 / 7 yd detonation.
- Everyone converges on the drift nearest the **ring centre**, not `ULDUAR_HODIR_RAID_ANCHOR`. The
  two are the same point while no fire is adopted, but when one was the fixed point sat a median
  9.5 yd off the ring (p90 17.6) and picked drifts 25 yd away with three closer candidates on the
  floor. Trigger and action share one helper; two derivations would oscillate. It answers "none"
  before deriving the centre, since a shelter exists for ~6s of every 49s cycle and the centre costs
  a second grid sweep.
- **The anchor is abandoned every 48s and that is correct** — tanks included. He is encased otherwise.
- **Residual, still open:** the dodge issues `MOVEMENT_FORCED` and the shelter run `MOVEMENT_COMBAT`,
  and `IsWaitingForLastMove` only yields to a strictly higher priority, so a dodge firing late in the
  window can hold the slot until the freeze lands. Raising the run trades a ≤300s lockout for one Ice
  Shards hit at ~41% of a health pool — worth doing, but it needs its own before/after trace.

## Frost Resistance Aura belongs on a tank

**Frozen Blows is 71% of everything the raid takes** — 3.93M of ~5.5M in one 25-man trace, against
929k for Biting Cold, Freeze and Ice Shards combined. `63511` is 39,999 base and lands a median
23,691 after resists into a 34-45k tank pool, so the aura is the margin between a survivable swing
and a killing blow. Every tank killing blow in that trace landed with it **off**, the retribution
paladin carrying it 44-60 yd away, two of the tanks resisting nothing at all.

`GetHodirResistancePaladin` therefore prefers a **paladin tank**, then any non-healer, then whoever
is left. The aura reaches 40 yd (`48945`, radius index 23) and a tank never leaves the corner, so it
covers the two bots that need it 100% of the time against 82% for a DPS paladin running the dodge and
the shelter. The raid loses about ten points of coverage, which is the right trade: a resist point is
~6,000 off a swing that kills a tank and ~700 off a tick the healers already cover.

## Two icicle pools, and a dodge that leaves on one radius and lands on another

Icicle **33169** leaves Ice Shards `62457` in **4 yd**; the drift **33173** leaves `65370` in **7 yd**.
Both hit for 13-14,000. The dodge leaves on the radius that actually kills and lands on a clear
carrying 2 yd of margin over it (`_ICE_SHARDS_CLEAR` 6, `_BIG_SHARDS_CLEAR` 9) — clearing everything
to 6 stepped bots onto the edge of the big pool and killed four in one pull. `_DODGE_TRIGGER_MARGIN`
is 0.5 for the same reason in reverse: testing the *clear* at both ends had bots stepping out of pools
they were never in, since the small one would trigger over 2.25× the area it kills in. Candidates are
ranked smallest displacement first and leashed to `_DODGE_LEASH` 12 — maximising distance from the
hazard is what walked Auriaya's bots into the corridor.

An icicle summon lives 7,000 ms (`62234`/`62462`, DurationIndex 165) but **detonates at 3,700 ms**:
its AI casts the fall effect at 2,000 ms and that aura's single 1,700 ms tick triggers the blast. The
last `ULDUAR_HODIR_ICICLE_SPENT_MS` = 3,300 ms are inert, so at one icicle every 2s roughly half of
those on the floor have already blown.

**A bot mid-cast cannot dodge, and every layer above it reports success.**
`Unit::IsMovementPreventedByCasting` is true for *any* cast in flight unless a channel carries
`IsActionAllowedChannel`, and `PointMovementGenerator::DoInitialize` then returns without launching a
spline while `DoUpdate` calls `StopMoving()` every tick. `MoveTo` still succeeds and the move record
reads `ok`; `LastMovement` books the slot for a second and `IsDuplicateMove` refuses every re-issue,
with the bot standing where it was. Blizzard pinned one caster **5.7 s** and Evocation, an 8 s
channel, pinned the same one for the **3.5 s** it had left, each ending in `62457` for 13,987 with the
dodge's accepted destination 4-6 yd away. It costs **101 bot-seconds a pull, 2.9% of ranged alive
time** (Hurricane 29 s, Mind Flay 12 s, Evocation 11 s). The dodge and the shelter run therefore call
`PlayerbotAI::RequestSpellInterrupt` first thing — both triggers already gate on a bot that is in
danger and has to relocate, and a 3.3 s fuse leaves room for a 6 yd leg. **The shed does not**: it
holds the movement slot for 22% of movement time, and Biting Cold at the arm threshold is 800 a tick
against Ice Shards' 14,000.

**Biting Cold sheds on sustained movement only, and jumping cannot fake it.** A stack comes off on the
second *consecutive* tick where the server reads `isMoving()`, and any stationary tick between resets
that progress, so it needs more than a second of unbroken travel: the shuttle walks 6 yd legs
(`_SHUTTLE_HALF_LEG` 3.0) on bearing −π/4, parallel to the SW bevel, chaining until the aura is gone.
It arms at 2 stacks, 3 in Starlight: ~33% movement duty for ~600/s, where arming at 1 would cost half
the raid's cast uptime to save 200/s.

`isMoving()` is the raw `MOVEMENTFLAG_MASK_MOVING` and does include the flags a jump raises, but
`MotionMaster::MoveJump` splines to its point and takes the duration from `length / speedXY` — an
in-place 0.01 yd hop is airborne **~1.4 ms** and never covers a tick, which is what
`IntenseColdJumpAction` (the Nexus answer to the same mechanic) reports in its own comment.
`IsDuplicateMove` also refuses a repeat within 0.01 yd, and `JumpTo` books the slot for 1,000 ms. A
hop long enough to span two ticks is a 7 yd walk at run speed, which is the leg already. Only an
optional `speedXY` on `JumpTo` would change that: 2 yd at 2 yd/s lasts a second.

**The shed holds each leg**, latched like the dodge — re-derived on arrival, on the leg stopping being
clear of icicles, or on slipping past `_DODGE_SLIP`, and on arrival it derives the *next* leg rather
than stopping, because the chain is what covers consecutive ticks. Stateless it issued **6,686 moves
against 4,122 refusals**, the largest single source of churn in the fight. Its sweep takes allies at
`_DECLUMP_RADIUS`, live icicles at their clears, and —
for ranged and healers only — **Hodir at `_RANGED_MIN_BOSS_GAP`**. Allies alone put three raiders on
an icicle inside fourteen seconds and left a warlock dead at 7.7 yd from the boss.

**Ask `IsHodirBitingColdShedArmed`, never `HodirBitingColdTrigger`, before standing a node down for
the shed.** The trigger fires on *any* stack because the action owns the shed-to-zero latch — but 87%
of the time a bot holds Biting Cold it holds exactly one, **32.8% of the fight each**, and there the
shuttle does nothing at all. `HodirRaidPositionTrigger` deferring to the trigger is what kept the
ranged out of Starlight: a usable zone was in range on **85%** of their ticks while they stood in one
for **22%**.

## Traps

- **Three icicle entries, and confusing them breaks the fight.** 33169 is the small one, dodged
  always. 33173 is the drift, dodged **only while falling** — the dodge stands down once a 33174
  exists within 9 yd of it, because 33174 is the shelter everyone is running to. 33174 is never
  dodged.
- The anchor is **not combat-gated**: `MoveInLineOfSight` is a no-op, so bots pre-position in the
  corner and the tank pulls from there instead of dragging him 75 yd.
- **Melee get no anchor, no fire and no Starlight.** Re-examined once Starlight turned out to be +50%
  melee haste too, and confirmed: zones sit a median **21.6 yd** from him, so melee are inside one
  **1.8%** of ticks and within 10 yd for 7.7%. The only fix is dragging him to the druid, which costs
  the corner.
- The Storm Cloud carrier **laps the formation ring**, with centre, radius, bearing and direction all
  latched for one carry and keyed on the aura's **apply time** — stack counts repeat across carries,
  so a rise cannot tell them apart. Read them off the bot each tick instead and the target sits 45°
  ahead of a moving bot forever while `std::max` locks in every yard of outward drift: one carry
  walked **213 yd** and ended **142 yd** from the boss, outside the room, and died there alone. The
  radius is clamped to the 4.5-9 yd ring rather than the carrier's own distance, because Storm Power
  is 3 yd and has to be carried *through* the raid — a carrier 30 yd out toured a circle nobody stood
  on, 23 yd a step against 4-6 one-second ticks. Tanks never lap — the trigger refuses, since leaving
  the corner mid-Frozen-Blows costs more than the buff — but the boss still picks them: **14 of 53**
  carries in one pull went to a tank and 6 more to healers, every one a dead window. Collecting those
  would mean walking the receivers to the carrier. Greedy re-targeting is the Auriaya corridor dance.
- **A tighter formation buys dps and pays in icicles.** The three latches took 12-or-more bots inside
  one 10 yd circle from **39.7% of the pull to 56.1%**, and time inside a lethal icicle radius rose
  **11.2% to 15.3%** of alive time, `62457` damage 177k to 222k. Icicles target players, so a tighter
  raid shares pools and leaves itself less clear floor to dodge into. Widening `_DECLUMP_RADIUS` or
  the clears buys that back at the throughput the stacking earned — re-measure after the cast pin
  above, since two of the three icicle deaths were the pin, not the spacing.
- **Killing Spree walks a rogue into pools no dodge can predict.** `51690` blinks the bot to a random
  target's back with `moving` and `moveGen` both zero, so nothing sweeps for it and the dodge only
  ever evaluates from where the bot currently stands. One 4.5 yd relocation landed **1.7 yd** from a
  live `62457` and killed the rogue 0.4 s later, mid-shed-leg.
- Healers are excluded from the targeting node entirely, and **5** non-healers break each ice block —
  raider and helper alike, picked by a GUID window offset per block so several blocks draw disjoint
  sets instead of the same five. Freeing outranks the boss (the trapped raider dies to the next
  freeze) but the block has little health, so only bots within 45 yd leave what they were doing.

## Every mover here thrashes unless it latches

Traced on 2026-08-23: two wipes at 67% HP, both tanks dead inside two minutes, healers at 0-10% cast
uptime and ~70% moving. 934 anchor/dodge reversals — 21% of every accepted move, median gap 321 ms,
15,240 yd walked — roughly **43% of the raid's fight time spent walking between two destinations**.

Traced 2026-09-02, with the dodge latched and the shed, Starlight and Storm Cloud not yet: bots move
**58.7%** of their alive time and **50.6%** of the distance walked is undone within five seconds
(36,799 yd walked, 18,187 net). Of destination changes landing within four seconds of the last,
**90.5% are turn-arounds** — 2,421 of 2,676: shed on shed 454 of 485 at a median 879 ms, shed against
the icicle dodge 487 flips at 230-403 ms, and **every one** of the 100 times the dodge followed
`reach melee`. The four Hodir movers own **82.8%** of movement time, `reach melee` gets 8.3%, and
melee are within 8 yd of the boss only 49.8% of the fight.

Traced 2026-09-06 with all three latched: **5:48.2 at 124k dps, against 6:20.4 at 115k**. Walking fell
38,917 to 30,973 yd, moving 58.7% to 53.9%, stalls — 6 s or more stationary while still issuing
accepted moves — 318 to 208 s, melee within 8 yd 49.8% to 58.6%, ranged beyond 30 yd 18.3% to 8.8%,
worst Storm Cloud carry 142 to 37 yd from the boss. Deaths went 2 to 6 and **not one was a mover
overshooting**: two to the cast pin above, one to Killing Spree, three to the tank.

Each cause below is separate, and all of them are still easy to reintroduce.

- **The anchor stand-down tests the whole walk back, not just the anchor.** The dodge trigger goes
  false the moment the bot is clear, but the icicle stays lethal until it detonates at 3.7s. Checking
  only the destination lets the anchor walk the bot back under the blast, where the dodge re-arms —
  about 11 round trips per icicle, one icicle every 2 s. `HodirRaidPositionTrigger` projects each
  lethal icicle onto the bot→anchor segment for exactly this reason.
- **Do not "fix" this by widening the arrival tolerance.** A dodge always displaces further than the
  tolerance — by design, not the bug. Widening it stops the *return*, and at one icicle every 2 s the
  formation becomes an unbounded random walk out of the fire inside a minute. What works instead:
  `HodirRaidPositionTrigger` is reactive for ranged, firing only on a broken constraint (inside
  `ULDUAR_HODIR_RANGED_MIN_BOSS_GAP` 15 with a clear slot to reach, no fire, clumped under
  `ULDUAR_HODIR_DECLUMP_RADIUS` 4.5, past `ULDUAR_HODIR_RETURN_LEASH` 20), so it issues one
  destination and goes quiet. `HodirRaidPositionAction` holds no arrival latch on purpose — one would
  swallow those re-anchors, which fire well inside twice the tolerance. Tanks keep the spring: Hodir
  follows whoever holds him.
- **Ring slots are indexed over the whole ranged roster, dead included.** Indexing over the living
  shifts every bot after a corpse, so one death re-seats the entire formation and `total` moves the
  inner/outer split with it. Eleven ranged deaths, nine of them in a 30 s window, re-anchored every
  survivor each time — which is what turned a bad pull into a cascade.
- **Tanks do not run the icicle dodge.** They ate a ~50 yd walk around the room and took Hodir with
  them; Bulwark ended up 70 yd from the boss while alive. Tanks eat the 14,000 instead, and the
  Biting Cold shuttle already gives them the movement they need without leaving the corner.
- **The dodge holds its destination through arrival.** `FindNearestPositionClearOfHazards` rings
  outward in 2 yd steps from wherever the bot is standing, so clearing `_dest` on arrival dropped
  through to a fresh sweep in the same tick and bought another 2 yd hop — **4,197** forced moves in
  one pull, a new destination every **410 ms**, 71% of them under half a second apart, 1,000-1,700 yd
  walked for 15-55 yd of displacement. Hold the spot and re-validate it against the live hazard list
  instead; sweep only once it stops being clear. `_DODGE_ARRIVE` is 0.8 for the same reason: at 1.5 a
  bot counted as arrived a fifth of the way into a 2 yd leg, still inside the radius that re-arms the
  trigger.
- **The Starlight stand is latched per bot too, and a reject must not drop the latch.** Zones are
  ranked against the ring slot and the stand bearing is taken from it, so any centre change rewrites
  all 14 slots and with them the answer: stateless that was **739 anchor changes** across 23 bots at a
  median 3,896 ms, an 11 yd jump each, and Starlight windows lasting 1.6 s against zones that live a
  minute. Latching but erasing whenever the stored point failed re-validation made it *worse* —
  **1,201 anchor moves at a median 320 ms** over as few as 14 distinct points, **1,137 with a stand in
  force** — because the re-sweep ranks by walk from the slot, so Hodir drifting a yard past the 15/30
  yd caster band handed the bot a different zone that rejected next tick and back. Hold the zone until
  it leaves the sweep, re-validate the stored point against the fire leash and the caster band, and on
  a reject fall back to the ring slot for that tick without dropping the latch.
- **Put the zone in the note, not just the rule.** `hodir.starlight` carried only
  `stand`/`noreach`/`fire`/`none` and `NoteDerived` emits on change, so a re-latch onto a *different*
  zone still read `stand` and wrote nothing: the flap above hid behind 131 notes that looked exactly
  like a latch holding. The value is now `stand <zone>`, and a held stand standing down reads
  `held noreach`, so a bot waiting for the boss to drift back is distinguishable from one with no zone
  at all.

Two things that look broken in a Hodir trace and are not: `hodir frozen blows swap action` logging
~95% `FAILED` is the stateless trigger retrying every ~110 ms while the taunt is on cooldown — count
the `OK` records instead, one per cooldown per Frozen Blows window is correct. And a
`thorim.squadsassigned` note inside a Hodir pull is `ThorimResetEncounterStateTrigger` clearing stale
state from an earlier attempt, which is cleanup working — Thorim nodes in a Hodir trace issue zero
accepted moves and zero `OK` verdicts, and `NearThorimEncounter` excludes his floor by height
(`z < ULDUAR_THORIM_WING_MAX_Z` 425 against 432.687).

**Still open here:** no tank defensive cooldown is tied to a Frozen Blows window — the tanks spent
four and six in six minutes, unprompted. And the pace is short of the deadline: the best kill runs
**5:48.2** at **124k** dps on the boss against the **214k** a 38.57M pool wants in 180 s. The
0:30-1:00 window does reach **222k**, so the ceiling is there and sustain is what is missing — the
earlier "starts short" reading came off a slower pull. Positioning is no longer where the slack is:
**57.1%** of the 17,554/s the raid takes is the raid-wide `64545` tick that nothing avoids. The
movement economy still wants re-measuring after each latch rather than all at once.

**Who eats Frozen Blows is not stable between pulls.** In the 5:48 kill the bot tank took **27 of the
35 `63511` hits** for 506k against the human paladin's 8 and died three times; the pull before was 19
against 10 the other way with no tank death, and `hodir frozen blows swap action` produced **7 `OK`
records against 17**. Healing was not the constraint — he received 1.43M against 0.76M, nearest
healer p50 18.1 yd — so read the swap's gating before anyone's positioning.

**The taunt floor may be set too low.** `ULDUAR_HODIR_TAUNT_HEALTH_FLOOR` ships at **50.0f**, but a
max-roll `63511` is 28,929 against Bulwark's 45,287 pool — **63.9%** — and two land about 2.4s apart.
A taunt accepted just above the floor can therefore still be a death inside an open Frozen Blows
window. The investigation argued for 65%; 50 shipped, and nothing records why. Re-measure before
raising it: the same trace showed the floor rejecting few enough taunts that the looser value may
have been deliberate.
