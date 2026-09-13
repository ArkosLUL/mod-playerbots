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
as well as all three attack timers, amount 50 — **+50% haste to casting and swinging**. The row says
8. Binned by distance the hold rate is 94/90/78% across
the first three yards and 21% in the fourth, so the edge is 3 and the medians that once read as 4 were
sampling blur — bots cover 1.75 yd between snapshots. 3 yd holds *one* bot at the 4.5 yd spacing
icicles force, so Starlight is a **per-bot opportunity, never something to build a formation on**:
`ULDUAR_HODIR_STARLIGHT_STAND_RADIUS` 1.5 plus 1.0 tolerance, `static_assert`ed to stay inside it, and
a 30 yd search radius because the step runs per bot per tick. **Take the margin out of the stand
radius, never the tolerance** — tolerance also stands the position trigger down, so trimming it just
moves churn from the dodge to the anchor.

**A Toasty Fire is the Biting Cold shuttle's replacement, not a damage buff.**
`spell_hodir_biting_cold_player_aura` sheds a stack on `isMoving() || HasAura(62821)`
(`boss_hodir.cpp:1259`), so a bot standing in one sheds on every tick without walking a step. That is
the whole reason the formation chases fires. `62821` measures true to its 11 yd (with-aura p90 11.9).

**Getting the raid into one is a gate problem, not a geometry problem.** Every helper chases Hodir at
a fixed `AttackStartCaster` stand-off and drops its zone at its own feet — priest 17 (`:710`), druid
22 (`:827`, Starlight cast at `:877`), shaman 25 (`:944`), **mage 30** (`:1068`, fire cast at `:1117`)
— so a fire sits **23-30 yd from him wherever he is tanked**, and moving the fight to the middle of
the room changes nothing but costs the corner's arc collapse. Against `_CASTER_MAX_BOSS_GAP` 30 and
the 9 yd ring, a fire had to be inside 19 and **0 of 556** sampled ones were (p50 26.4, p10 23.2).
Two constants fix it:

- **The caster band is 35.** Spell range is measured to the target's bounding radius and Hodir is a
  giant, so the book 30 understates him: ranged dealt **7,693 dps each from 30-35 yd against 7,844
  from 20-25**, casts aimed at him started out to 40.1 (p99 36.2), and 35-40 is where it falls off
  (4,854).
- **A fire centre draws the ring in**, `ULDUAR_HODIR_FIRE_RING_INNER` 2.5 / `_OUTER` 4.5, and
  `GetHodirRaidFire` gates on `_FIRE_RING_OUTER`, so a fire up to **28.5 yd** out qualifies. This
  deliberately packs the raid inside Ice Shards' 4 yd splash — the trade the fire is worth, and
  `62457` damage plus `--clump` are what say whether it paid. It also puts the whole inner ring inside
  one 3 yd Storm Power pulse.

Slots otherwise sit on two concentric rings, `ULDUAR_HODIR_RAID_RING_INNER` 4.5 and `_OUTER` 9.0 with
six inner slots — 4.5 yd minimum separation, so Ice Shards catches one bot instead of five, and a bot
shedding Biting Cold can step outward without closing on its neighbours. Outer ring plus its 2 yd
arrival tolerance is exactly 11, `static_assert`ed to stay inside the fire. The centre is gated 15 yd
off **Hodir himself**, not the tank spot he leaves — he drifts 10-25 yd, and a fixed-point gate once
let the centre land 6.8 yd from him. Centre and anchor diverge by a median **9.5 yd** (p90 17.6), so
anything ranking off the formation takes the centre and not the fixed point.

**The tank drags him onto a fire so the melee get one too** — they are the half the ring never
reaches. Nothing puts a fire out but Flash Freeze (`npc_ulduar_toasty_fire::DoAction(1)` at `:682`,
called only from Hodir's `SpellHitTarget` on `SPELL_FLASH_FREEZE_VISUAL` at `:325`), so he can be
parked on one. Both tanks stand `ULDUAR_HODIR_FIRE_TANK_OFFSET` 13 past it — his reach plus a
raider's, so he stops *on* it — the off-tank `_FIRE_OFFTANK_GAP` 5.5 further out again, within
`_FIRE_DRAG_LEASH` 40. **The bearing comes from `ULDUAR_HODIR_RAID_ANCHOR`, never from Hodir**: it is
then the same number on both tanks every tick, it cannot go degenerate once he is standing on the
fire, and running out along it leaves both tanks on the far side from the raid. Latched per fire —
the next fire lands 30 yd from wherever he ends up, so re-picking while one burns walks him round the
room forever. Clamped to the y band he evades outside (`_ROOM_MIN_Y` −297 / `_MAX_Y` −171) and
floor-validated; the corner stays the fallback. `hodir.tankhold` reads the fire guid, `corner` or
`offfloor`.

**Singed is real, and has never once landed on Hodir.** `62821` effect 2 is aura 42
`PROC_TRIGGER_SPELL` → **`65280`**, ProcChance **33**, ProcTypeMask **65856** =
`DONE_RANGED_AUTO_ATTACK | DONE_SPELL_RANGED_DMG_CLASS | DONE_SPELL_MAGIC_DMG_CLASS_NEG`. Singed
stacks to **25** for 25s and carries aura **87 `MOD_DAMAGE_PERCENT_TAKEN` +2 per stack**, school mask
126 — every magic school, no physical — so **+50% magic damage taken** at cap, plus 3,000 fire. Two
things follow from the mask: **melee cannot proc it at all**, and it procs on damage *done*, so the
target is whoever was hit and range is irrelevant — a caster in a fire would Singe him from 30 yd, and
dragging him to the fire is not what applies it. But measured over 744 bot-seconds of fire uptime:
**10 applications, every one self-cast on a player, none on Hodir.** The self path is Biting Cold's
own tick — `CastCustomSpell(target, SPELL_BITING_COLD_DAMAGE)` (`:1290`) is a magic-negative cast by
the player *on the player*, which satisfies the mask. **Confirm it lands on the boss before building
anything on it**; the fire is worth adopting for the Biting Cold shed alone.

**Toasty Fire grants no Flash-Freeze exemption.** It is 11 yd and only blocks Biting Cold. The one
exemption is `SPELL_SAFE_AREA_TRIGGERED (62464)`, off `65705` on **NPC 33174**, radius index 40 → 9 yd.

| Mechanic | Ids | Numbers that drive the code |
|---|---|---|
| Flash Freeze | 61968 | **9s cast**, every 48-49s, 200 yd. Spares only 62464 carriers and pets |
| Shelter chain | 33173 → `62460` → `65370` + `62463` | Drift spawns at T+0 where its target will stand, lands T+3.9 → Ice Shards 14,000 in **7 yd** → summons **33174**, 12s. Freeze lands **T+10.1** |
| Trapped player | 61969 / 62226 | **300s**, and the *next* Flash Freeze **instakills**. Free by killing NPC **32926** (helpers: 32938) |
| Small icicles | 62227 → 63545 → 33169 → `62457` | **Every 2s** on 1 random player, falls after 2s, **14,000 Frost in 4 yd** + knockback. Off for 12s (25m) / 24s (10m) after each Flash Freeze |
| Biting Cold | 62038 / 62039 | **1005 ms ticks**, `200 · 2^stacks`. Stacks after 4 stationary ticks, sheds on the **second consecutive** tick the server reads `isMoving()` |
| Frozen Blows | 62478 / 63512 | 20s, **15s after each Flash Freeze**; +31,061 / +39,999 per swing (the damage itself is `63511`) plus a 3,999 raid tick (`64545`). All four rows are named "Frozen Blows"; the aura ids apply, the damage ids hit |
| Freeze | 62469 | Random player in 50 yd every 17-20s, 5,549 + root in 10 yd, **dispellable (Magic)** |
| Storm Cloud → Storm Power | 65123/65133 → 63711/65134 | Carrier holds **4 (10m) / 6 (25m)** *charges*, not seconds — the cloud itself lasts 30s. Storm Power is a **3 yd** pulse at the carrier's own feet, 30s, **+135% crit damage (aura 163) and +20% cast haste (aura 61)** |
| Toasty Fire | 62821 → 65280 | 11 yd, 60s, at the mage's feet every 10s and so **30 yd from Hodir**. Sheds Biting Cold every tick exactly as movement does. Procs **Singed** at 33% on ranged/magic damage *done*. **Flash Freeze wipes every fire** (62148) |
| Berserk | 26662 | 8 min, unhandled — no enrage awareness exists anywhere in the module |

**Hard mode is gone as a config.** The "Rare Cache of Winter" kill needs no different
behaviour: the helpers *are* the raid's damage, the fire is the Biting Cold answer, and Storm Power
is the biggest buff in the fight, so all three run on every pull. `AiPlayerbot.UlduarHodirHardMode`
and `IsHodirHardModeActive` were deleted rather than left gating nothing.

**The deadline is 3:00, not the 2:00 the guides give.** `SPELL_SHATTER_CHEST_TIMER` is **65272**, an
`EffectAuraPeriod_1` of 180000 ms on a 180000 ms duration, cast on engage at `boss_hodir.cpp:263`;
its single tick fires `EVENT_HARD_MODE_MISSED` and shatters the Rare Cache. So the target is halving
this fight, not thirding it.

## The Flash Freeze window is ten seconds and the shelter exists for six

The cast is 9s but the freeze lands at **+10.1s**, where the ice blocks spawn — the one trapping in
seven casts applied `61969` at +10.03s. The drift lands at **+3.9s**, so the target covers only the
last **6.2s**. The drift itself is on the floor from +0.0s and stands exactly where its target will
spawn, 0.0 yd apart across all seven, which is why the run stages on it and uses the whole cast.

- **The run parks at 6 yd and releases at 8** (`ULDUAR_HODIR_SAFE_AREA_TOLERANCE` / `_RELEASE`, both
  `static_assert`ed inside the 9 yd Safe Area), and at **9 / 11 on a drift that has not landed**
  (`ULDUAR_HODIR_BIG_SHARDS_CLEAR` plus the same 2 yd, via `GetHodirShelterPark` / `_Release`).
  `MoveInside` → `MoveNear` lands the bot at *exactly* the tolerance, so testing one number at both
  ends stood the trigger down the tick it arrived and handed the next tick to the ring anchor —
  measured walking bots 18-22 yd back out with the freeze 2s away.
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
- **Stage on the drift; do not wait for the target.** Keying the run on 33174 existing wasted the
  first 3.9s of every cast, because `HodirGuardMultiplier` zeroes every mover from cast start: the
  raid moved **20.9%** of that half against a **51.7%** whole-fight baseline, then had 6.2s to cover
  up to 35 yd. After: **26.0%**, and that reads as arriving rather than falling short — the p50 walk
  is now 9.4 yd against a 9 yd park radius, so most bots have nothing left to walk. Parking at
  `_BIG_SHARDS_CLEAR` stands it where the target appears without entering the
  14,000 / 7 yd detonation. A bot already inside the blast needs no push: `MoveInside` answers false
  there and it descends to the dodge at `ACTION_RAID + 5`, which collects drifts at the same 9 yd
  clear. 9 also clears the dodge trigger's own 7.5
  (`_BIG_SHARDS_RADIUS + _DODGE_TRIGGER_MARGIN`), so the two never fight over a parked bot.
- **Each bot takes its own nearest, latched** (`GetHodirShelter`). Trigger and action still share one
  helper, because two derivations oscillate, but that never needed one answer for the whole raid —
  and ranking off the ring centre gave exactly that: one guid for every bot, all seven casts. The
  three land **5-31 yd apart** and `62464` holds off whichever is nearest (88% at 6-7 yd, 70% at 7-8,
  53% at 8-9, 26% at 9-10), so the detour bought nothing. The latch is only for the sideways dodge
  that carries a bot past the midpoint between two; otherwise nearest-to-self holds itself, since
  walking at a shelter keeps it the nearest.
- **What the shared pick cost**, over 175 bot-windows: walk in p50 **17.7 → 9.8** yd, max 35.0 → 28.8,
  total 3,031 → 1,970; walk back out p50 19.5 → 9.4, total 821 → 612; over 20 yd out **68 → 11**,
  over 30 yd 9 → 0. Ranged and healers inside 15 yd of him went **29% → 12%**: the centre rides a
  fire, so the shared pick was the thing walking casters into his melee. Five bots missed a freeze
  outright; in the worst window the tank stood **2.6 yd** from a shelter and was sent **31.6**, and he
  and a mage sat encased 11.3s — which also costs the five non-healers who break each block.
  Splitting strands nobody: worst bot-to-nearest-healer **21.6 yd**, none over 40, against heals
  landing at p50 12.3, p90 26.7, max 51.5. Counting healers per shelter is the wrong test and says
  otherwise.
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
It arms at **5 stacks, 6 in Starlight** (`ULDUAR_HODIR_BITING_COLD_SHED_STACKS`). The tick is
`200·2^stacks` and `62039` caps at 8, so that is 6,400/s at the arm point and 12,800 one stack later
— bought because the shuttle was the most expensive thing in the fight (18.7% of won engine passes,
32.4% of accepted moves) while healers ran **33,553/s effective with 16,025/s of overheal against
17,372/s taken**. It is a ceiling, not a starting point: six raiders already dipped under 15% health
at the old arm point of 2. `IsHodirBitingColdShedArmed` short-circuits on the fire aura, so this
governs only bots that have none and is worth less the better the fire work lands — **drop it back
toward 3 the moment deaths or Biting Cold damage climb**. What it spends is real but not the bulk of
the problem: **57.1%** of the 17,554/s the raid takes is the raid-wide `64545` Frozen Blows tick that
nothing avoids. Passing 2 stacks also clears
`bAchievGettingCold` (`:566` via `SetData(2, 1)` at `:1281`), which the Rare Cache does not care
about.

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
  always. 33173 is the drift: dodged **only while falling** and only inside 7.5 yd, because the
  shelter run parks on it at 9. The dodge stands down once a 33174 exists within 9 yd of it. 33174 is
  the shelter, and is never dodged.
- The anchor is **not combat-gated**: `MoveInLineOfSight` is a no-op, so bots pre-position in the
  corner and the tank pulls from there instead of dragging him 75 yd.
- **Melee get no anchor and no Starlight** — zones sit a median **21.6 yd** from him, so melee are
  inside one **1.8%** of ticks and within 10 yd for 7.7%, and the only fix is dragging him to the
  druid, which costs the corner. They do get a fire, but only because the tank parks *him* on one;
  nothing walks a melee bot to one.
- **The Storm Cloud carrier holds still and the raid comes to it.** `boss_hodir.cpp:1462` casts Storm
  Power `CastSpell((Unit*)nullptr, ...)` — an area pulse at the carrier, so how many it hits is
  bounded only by who stands inside 3 yd, and `_DECLUMP_RADIUS` 4.5 is wider than that. Lapping the
  ring therefore spent the charges one bot at a time: raiders sat inside the pulse on **4.4%** of
  samples during a live carry, p50 **0-1** of them, and the carrier's 3rd-nearest damage dealer was
  7.0 yd away. Coverage ran **31%**, where buckets over 50% averaged **198,857 dps against 123,105**
  under (r = +0.54) and the per-bot lift measured **+108%** — an upper bound, since a bot holding it
  is also a bot standing still. **26.2%** of delivery went to healers and the tank, who produced 2.1%
  of the damage. The carrier end wastes more and nothing here fixes it: the boss picks whoever he
  likes, and **14 of 53** carries in one pull went to a tank and 6 more to healers — every one a dead
  window, since tanks refuse to carry at all.
- So the lap is gone. One **rally** per carry, seeded from where the carrier stood when the cloud
  landed, latched on `Aura::GetApplyTime()` — stack counts repeat across carries, so a rise cannot
  tell them apart — and held **per instance**, not per bot, because a carrier and a receiver that
  disagree walk past each other. Damage dealers inside `_STORM_CLOUD_COLLECT_LEASH` 15 that lack the
  buff step in to park 2.0 / release 4.0; healers and tanks are excluded, and tanks still never carry.
  Seeding from the carrier is what keeps a melee carrier gathering melee and a ranged one gathering
  the formation, instead of walking half the raid across the room for six seconds of buff. No cast is
  broken for it — that is for the 14,000 hits, not a buff.
- The old lap is still the cautionary tale: read centre, radius and bearing off the bot each tick and
  the target sits 45° ahead of a moving bot forever while `std::max` locks in every yard of outward
  drift — one carry walked **213 yd**, ended **142 yd** from the boss outside the room, and died there
  alone. Greedy re-targeting is the Auriaya corridor dance.
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

Traced 2026-09-13 with the per-bot shelter: **5:04.3 at 128k dps on the boss, and zero bot deaths**
(the one death was a human). Walk to the shelter fell p50 17.7 → **9.4** yd and max 35.0 → 12.0,
bot-windows over 20 yd out **68 → 0 of 138**, walking 30,973 → 23,584 yd, `61969` applications 2 → 1
and that one on a human. Still **57.7%** of walking is undone within 5 s (35.8% at 2 s, 70.4% at 10 s)
and `act.won` reverses A-B-A **1,162** times, 229/min across 23 bots: shed ↔ dodge 263 at 739 ms,
shed ↔ `set facing` 180 at 936 ms, dodge ↔ `set behind` 57, dodge ↔ `reach melee` 51. The shed issued
5,835 move records for 2,142 accepted (2,338 `dup`, 1,277 `wait`); the ring anchor 2,641 for 466
(1,774 `wait`).

**Two high-churn probes are not defects, and re-tuning them is wasted work.** `hodir.shuttle` reverses
`crowd ↔ held` 3,357 times at a 959 ms median, which is exactly the designed chain — `_SHUTTLE_HALF_LEG`
3.0 gives 6 yd legs, ~0.86 s at run speed. `hodir.stormcloud` did the same 192 times at 780 ms, which
was the 45° lap step and is gone with the lap. Both are advance-on-arrival, and the volume is a
symptom of how much walking the fight demands rather than of a latch that flaps.

**Re-measure the movement economy after each change, never after several.** Every figure above moved
under one edit at a time, and the three-latch pull is the one that cannot say which latch did what.

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
four and six in six minutes, unprompted.

**The pace is short, and sustain is the whole gap.** The best kill runs **5:04.3** at **128k** dps on
the boss against the **216.5k** a 38.97M pool wants in 180 s. The 0:30-1:00 window reaches **246.7k**
— above the deadline pace — so the ceiling is not the problem. What ends it is the fire lapsing: a
controlled pair either side of it, with Heroism already expired in both, runs fire 37% / cold 36% /
moving 37.6% / casting 47.8% / **233.6k dps**, then fire 0% / cold 55% / moving 57.5% / casting 30.5%
/ **122.8k**. Over 21 buckets, casting% correlates with boss dps at **r = +0.75**, moving% at −0.63
and fire coverage at +0.78, so cast uptime is the metric to move and the fire is how.

**The opening is worth 42 seconds.** Hodir lost 4.7% in the first 30 s while the raid freed helper
blocks, and Heroism went out at 0:04.5 and expired at 0:44.5 — against the 0:30-1:00 rate the opening
forgoes 5.38M damage, 42 s of fight at the pull's own average. Unexamined.

**Berserk at 8 min is still unhandled** anywhere in the module.

**Who eats Frozen Blows is not stable between pulls.** In the 5:48 kill the bot tank took **27 of the
35 `63511` hits** for 506k against the human paladin's 8 and died three times; the pull before was 19
against 10 the other way with no tank death, and `hodir frozen blows swap action` produced **7 `OK`
records against 17**. Healing was not the constraint — he received 1.43M against 0.76M, nearest
healer p50 18.1 yd — so read the swap's gating before anyone's positioning.

**The tank drags Hodir every 48s.** He moves **1.72 yd/s** during shelter runs against **1.27** the
rest of the fight, because the tank walked 39-56 yd per window and Hodir follows whoever holds him.
That drift is what the ring anchor and the Starlight latch re-derive against every tick. Taking the
nearest shelter already cuts the tank's worst run from 31.6 yd to 2.6, so re-measure before adding a
tank-specific pick.

**The taunt floor may be set too low.** `ULDUAR_HODIR_TAUNT_HEALTH_FLOOR` ships at **50.0f**, but a
max-roll `63511` is 28,929 against Bulwark's 45,287 pool — **63.9%** — and two land about 2.4s apart.
A taunt accepted just above the floor can therefore still be a death inside an open Frozen Blows
window. The investigation argued for 65%; 50 shipped, and nothing records why. Re-measure before
raising it: the same trace showed the floor rejecting few enough taunts that the looser value may
have been deliberate.
