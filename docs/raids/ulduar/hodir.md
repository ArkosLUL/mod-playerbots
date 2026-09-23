# Hodir

Held **mid-room** at `ULDUAR_HODIR_CENTRE` `(1998.0, -235.5, 432.687)`, or on a Toasty Fire within 25 yd.
The room is x 1965-2041, y -170 to -298, and he evades outside that y band. Every helper chases him
at a fixed `AttackStartCaster` stand-off and drops its zone at its own feet — priest 17 (`:710`),
druid 22 (`:827`, Starlight cast at `:877`), shaman 25 (`:944`), mage 30 (`:1068`, fire cast at
`:1117`) — and all eight spawn round the middle (`boss_hodir.cpp` `hhd`), inside their stand-off
from the centre: priests 16.3/15.5, druids 21.6/5.4, shamans 14.5/9.3, mages 23.2/6.5. So early on
none walks, and zones land where they spawned: Starlight a median 9.8 yd from a druid's spawn and
**west** of him (29 of 36), fires 8.2 yd from a mage's and **east** (36 of 53). **They still drift**:
on 2026-09-19 one mage's block sat 6.2 yd from the centre at three freezes, then 14.0 and 24.2.
`ChaseRange(range)` has MinRange 0, so none backs away, and why they moved is out of reach: helpers
are friendly, snapshots skip them, and their ice blocks are the only position fix. The old south-west
corner hold dragged him 30-55 yd out to every fire and back — 913 of the 1,106 yd he walked over
three pulls, at 2.88 yd/s against 0.87 on a fire. navprobe settles the centre and 6/12 yd rings round
it at 432.687.

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

**A Toasty Fire sheds Biting Cold and feeds Singed.** `spell_hodir_biting_cold_player_aura` sheds a
stack on `isMoving() || HasAura(62821)` (`boss_hodir.cpp:1259`), so a bot standing in one sheds on
every tick without walking a step, and magic damage cast from inside one procs Singed on its target.
`62821` measures true to its 11 yd (with-aura p90 11.9).

**The caster band is 15-35** (`_RANGED_MIN_BOSS_GAP` / `_CASTER_MAX_BOSS_GAP`). Spell range is
measured to the target's bounding radius and Hodir is a giant, so the book 30 understates him: ranged
dealt **7,693 dps each from 30-35 yd against 7,844 from 20-25**, casts aimed at him started out to
40.1 (p99 36.2), and 35-40 is where it falls off (4,854).

**Ranged and healers have no home spot.** Nothing in his kit asks them to stand anywhere in
particular, so `DeriveHodirAnchor` offers a **buff stand or nothing**: Starlight first, since +50%
haste beats what a fire gives, else the nearest point of a live fire, capped at `_FIRE_STAND_RADIUS` 8
of its centre — park 8 / release 10 inside the aura's 11, and a bot already inside keeps the spot it
is on. Either only when the stand sits in the caster band and inside the walk the bot will make.
The fire *he* is held on needs no special case: with him on it, nothing within 8 yd of it is 15 from
him. `hodir.stand` reads `starlight` / `fire` / `none`.

**The walk depends on what the bot already holds** (`HodirBuffWalk`): `_BUFF_WALK` **15** for a bot in
Starlight or a fire, which gives up one it has to chase another, and `_BUFF_WALK_UNBUFFED` **30** for
one with neither, which gives up only the travel. 30 is the zone sweep's own radius, `static_assert`ed,
so nothing it finds is thrown away unread. **One flat number cannot serve both**, and which one it
should have been moved when the hold did: a legal Starlight stand sat inside 15 yd on **53.7%** of
ranged samples on 2026-09-19 and on **6.6%** on the fire-hold build — same 15, same code, 57-60% of
stands still on the floor and out of reach. See [pitfalls](../../engine/pitfalls.md).

The two concentric rings round a fixed anchor are gone: a leftover of the corner hold, and also what
kept ranged out of the buffs. Over the three 2026-09-19 evening pulls a usable stand sat within 15 yd
of where the bot already stood on **73 / 55 / 65%** of samples while ranged held Starlight or a fire
**24 / 23 / 24%** of the time (healers 24 / 31 / 31%), and the ring anchor was 12-14% of every accepted
move — walking bots off zones they had reached, with 314-487 `dup` and 411-431 `wait` refusals behind
it.

**One hold point per instance, read by both tanks** (`DeriveHodirHold`, `HodirBotLatches::hold`): the
centre, or the live Toasty Fire nearest the centre that is within `_HOLD_FIRE_LEASH` **25** of it —
and the point is **the fire itself**, not a spot beside it. Measured from the centre, never from him,
so he never ends up more than 25 yd out and every tank spot stays 40+ yd inside the y band he evades
outside. Melee wrap round him rather than lining up behind — p50 **1.2 yd behind his centre**, p10 6.3
behind, p90 4.1 in front, |lateral| p50 3.0, pets p50 3.3 behind — so a fire at his feet covers **98%**
of melee and pet samples against 92 / 91% for one 5 yd behind him and 80 / 73% at 8. The fire procs
nothing by itself; the bots standing in it do, which is the whole reason he goes on one. Nothing puts
a fire out but Flash Freeze (`npc_ulduar_toasty_fire::DoAction(1)` at `:682`, called only from
`SpellHitTarget` on `SPELL_FLASH_FREEZE_VISUAL` at `:325`), so the pick holds until the fire dies —
re-picking while one burns walks him between two. `hodir.tankhold` reads the fire guid or `centre`,
`hodir.hold` the point.

The old 12 yd box, with a 2 yd backstep and a 7 yd reach, existed only to keep the ranged ring out of
the **chamfered** SW corner (the floor bevels from ~(1966, -274) to ~(1990, -298)). With the ring gone
it just threw fires away: over 0-3:00 of the three evening pulls some fire burned inside 25 yd for
**79 / 47 / 51%** of the window against **27 / 33 / 51%** actually held.

**The campfire is a solid object, and the ordinary floor check clips to it.** `ValidateFloorPoint`
raycasts the dynamic tree, so a spot on the far side of a fire comes back pulled to the fire's near
edge — four tank anchors landed **1.2-1.7 yd** from a fire centre instead of 4.5 past the point, after
which he parked 5-9 yd off it with his back to the melee. Every point derived here therefore goes
through **`ValidateStaticFloorPoint`**, which drops that pass ([pitfalls](../../engine/pitfalls.md)).
The only solid gameobjects in the room are the two doors at y -166 / -298 and the caches at
(1967, -204) / (2036, -202), all 35+ yd outside anything derived here.

- **The main tank stands `_HOLD_TANK_OFFSET` 4.5 past the point**, on the far side from where he stood
  when it was adopted. He stops a median 5.0 yd from whoever holds him (p10 3.9, p90 7.0) and 3.6-4.2
  back along the drag, so he lands on it. The old 13 parked him ~9 yd short of the fire and left
  far-side melee outside its 11 yd: melee fire coverage 22.7-34.1% with a fire held ~41% of the time.
  Within `_HOLD_BEARING_MIN_GAP` 3 of the point there is no far side, so it takes a fixed bearing into
  the open floor north-east of the centre. Out of combat it is re-taken every tick, so the tank
  pre-positions past the centre from his spawn; a wipe drops the latch.
- **The off-tank stands 4.5 off the point, square to the hold bearing**, so a Frozen Blows taunt walks
  him ~6 yd sideways and leaves him on the fire either way. The tank Biting Cold shuttle runs its ±3 yd
  legs on the same axis, for the same reason: neither end may drag him off the fire.
- **Parked off the point, he is re-aimed.** He only walks toward whoever holds him, so once shoved past
  the spot (the tank flipping to a shelter after a freeze, a knockback) he parks with it already in
  reach: 5+ yd off for 80 s of 113 in centre windows over 0-3:00 on 2026-09-19, which also put the
  western Starlight zone inside the 15 yd gap. Still and over `_HOLD_REAIM_GAP` 5 off for
  `_HOLD_REAIM_MS` 2 s, the bearing is re-taken from where he stands; he normally stops within 1-2 yd.

**Singed lands on Hodir, and no `aura` row can show it.** `NoteAura` goes through `ObsSession::Tracks`,
which drops every non-player (`RaidObsSession.cpp:349`), so a trace holds no creature aura at all — an
earlier "never landed" reading was exactly that blind spot. Each proc is a triggered `cast` of `65280`
with `tgt` = Hodir: 173 / 115 / 161 in three pulls, from ranged 221, pets 150, melee 46, tanks 25,
healers 7. `62821` effect 2 is aura 42 `PROC_TRIGGER_SPELL` → `65280`, ProcChance **33**,
ProcTypeMask **65856** = `DONE_RANGED_AUTO_ATTACK | DONE_SPELL_RANGED_DMG_CLASS |
DONE_SPELL_MAGIC_DMG_CLASS_NEG`: white swings never proc it, melee spells and pets in a fire do, and
the target is whoever was hit, at any range. It stacks to **25** for 25s with aura **87
`MOD_DAMAGE_PERCENT_TAKEN` +2 per stack**, school mask 126 (magic only) — **+50% magic damage taken**
at cap, plus 3,000 fire. **`spell_hodir_toasty_fire_aura::HandleProc` (`:1494`) casts `65280` from the
*player* carrying `62821`**, at whatever it just hit, so the fire is worth nothing standing empty and
everything with the pack in it. Modelled +1 a proc, he held a mean 19.2 / 7.1 / 18.6 stacks over 0-3:00
in three corner-hold pulls and **20.6 / 15.9 / 20.8** in the three 2026-09-19 evening ones (+41 / +32 /
+42% magic damage taken), capped at 25 from ~0:45 in two of them. A lapsing fire is what loses it: one
pull sat at 0 for 11 s of 0-3:00 **with a fire still burning that nobody stood in**, and the climb back
to 25 takes 13-20 s after the next one lands (first fires 0:20.5 / 23.0 / 21.9). Procs 100 / 140 / 142
— pets 58 / 54 / 80, melee 23 / 20 / 33, ranged 9 / 60 / 26, tanks 9 / 5 / 3 — and that pull's 60 ranged
procs all came from bots standing in a fire he was *not* held on, the only thing keeping Singed alive
there at all. A player can Singe itself too: Biting Cold's tick (`CastCustomSpell(target,
SPELL_BITING_COLD_DAMAGE)`, `:1290`) is a magic-negative cast by the player on the player.

**Helper blocks are broken mage first** (`GetHodirAssignedHelperBlock`):
`ULDUAR_HODIR_MAGE_BLOCK_BREAKERS` **5** each, then druid, shaman, priest at one each, inside the
8-breaker budget — **which drops to `_LATE_BLOCK_BREAKERS` 3 once no mage block is left**, since
nothing still on ice holds up the next fire and **170-176 bot-seconds a pull**, near a tenth of the
ranged group's whole fight, went on ice after the last mage was already free. A block (32938, 110,675 hp) is summoned by the helper inside it, so
`ToTempSummon()->GetSummonerUnit()` names it, and `hodir.dpstarget` writes that kind before the guid.
A freed mage casts its first fire 6s later (`EVENT_TRY_FREE_HELPER`, `ScheduleAbilities`,
`:1094-1124`). At one breaker a block, the next fire came **17.9-40.8s (28.5 mean) after each freeze
landed** and 0:18-0:26 into the pull — longer than Singed's 25s, so it reset every cycle. With a fire
held the raid ran **238k over 223s; without, 127k over 316s**, a gap that also carries the
block-breaking. Mage first, the next fire came 20.0 / 23.9 / 26.3 / 31.5 s after each landing on
2026-09-19, but the first mage block still took 7.5-19.8 s, not ~5: its breakers first cast on it
0.5-13 s after it spawned, held by the post-freeze shelter run, the shed and dodges. Three breakers
free it in **4.4-11.0 s**, and the block carries no mechanic, so that time scales with how many shoot
it.

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
| Toasty Fire | 62821 → 65280 | 11 yd, 60s, at the mage's feet every 10s, the first 6s after the mage is freed. Sheds Biting Cold every tick exactly as movement does. Procs **Singed** at 33% on ranged/magic damage *done*. **Flash Freeze wipes every fire** (62148) |
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
  ends stood the trigger down the tick it arrived and handed the next tick to the position anchor —
  measured walking bots 18-22 yd back out with the freeze 2s away.
- **Every other mover stands down for the whole cast, every role.** `HodirGuardMultiplier` zeroes
  gap-closers (`CastReachTargetSpellAction` — Charge, Intercept, both Feral Charges), `disengage` and
  `blink` (spell actions that throw the bot: a Disengage 6 yd from Hodir threw a hunter 12 yd out of its
  shelter 2.5 s before the landing), and every `MovementAction` bar the shelter run, the icicle dodge
  and the shed inside a landed shelter. `ReachTargetAction` is in scope;
  `AttackAction` is exempt because it only sets a target. Measured before the gate: 68 of 125
  bot-freeze pairs had their last accepted move come from something else, 36 of them the position anchor
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
- **The shelter run is gated on the cast too.** A 33174 outlives the freeze by ~7 s, and keyed on
  targets alone the run fired after the landing for 134 of 358 shelter moves on 2026-09-19 (96 of 278
  on 09-18), buying nothing and flipping the tank 8 times in 7 s between a shelter and his spot, which
  dragged Hodir 8 yd off the centre.
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
  and ranking off one shared centre gave exactly that: one guid for every bot, all seven casts. The
  three land **5-31 yd apart** and `62464` holds off whichever is nearest (88% at 6-7 yd, 70% at 7-8,
  53% at 8-9, 26% at 9-10), so the detour bought nothing. The latch is only for the sideways dodge
  that carries a bot past the midpoint between two; otherwise nearest-to-self holds itself, since
  walking at a shelter keeps it the nearest.
- **What the shared pick cost**, over 175 bot-windows: walk in p50 **17.7 → 9.8** yd, max 35.0 → 28.8,
  total 3,031 → 1,970; walk back out p50 19.5 → 9.4, total 821 → 612; over 20 yd out **68 → 11**,
  over 30 yd 9 → 0. Ranged and healers inside 15 yd of him went **29% → 12%**: the centre then rode a
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
is left. The aura reaches 40 yd (`48945`, radius index 23) and a tank never leaves the hold point, so it
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
danger and has to relocate, and a 3.3 s fuse leaves room for a 6 yd leg. **The shed breaks only a
channel**: a normal cast ends and then walks, but a warlock's 15 s Drain Soul held one from 3 stacks to
5, the freeze cast added two, and the 7-stack tick killed it.

**A walk stopped short is never issued again on its own.** `IsDuplicateMove` refuses the same point
for `MaxWaitForMove` (5 s) whether or not the bot is still moving
([pitfalls](../../engine/pitfalls.md)). On 2026-09-19 a mage's Mana Sapphire (`UseItemAction`,
relevance 90 over the dodge's 65) stopped it 6.5 yd short of its dodge, 5.2 / 5.7 yd from two drifts
that had landed within 4 yd of each other, and it stood there until both went off; the Disengaged
hunter above was refused its way back into the shelter the same way. `ReleaseStalledWalk` clears
`last movement` once the bot has stood still `ULDUAR_HODIR_STALL_MS` 500 short of the dodge's
destination, the shelter or a shed leg, which a fresh spline never does.

**Biting Cold sheds on sustained movement only, and jumping cannot fake it.** A stack comes off on the
second *consecutive* tick where the server reads `isMoving()`, and any stationary tick between resets
that progress, so it needs more than a second of unbroken travel: the shuttle walks 6 yd legs
(`_SHUTTLE_HALF_LEG` 3.0), chaining down to `_SHED_FLOOR` **1**: the last stack ticks 400, takes 2 s of
walking to drop, and 4 s standing puts it back. It arms at **4 stacks, 5 in Starlight**
(`ULDUAR_HODIR_BITING_COLD_SHED_STACKS`); the tick is `200·2^stacks` and `62039` caps at 8. Arming at 2
made the shuttle 32.4% of accepted moves (18.7% of won engine passes) while six raiders dipped under
15% health. Arming at 5 stood bots through 3-5 stacks once ranged stopped standing in fires (6.8% of
the time against 14-24%): on 2026-09-19 Biting Cold ran **1.70M over 0-3:00** against 0.84-1.41M, 44%
of it at 4+ stacks over 344 bot-seconds; a hunter bled out at 4 stacks under Frozen Blows (the raid took
53.8k/s against 37.3k/s healed), and healers spent 42-77% of that window moving, mostly shedding.
**3 is the next step** if deaths or Biting Cold damage stay high. `IsHodirBitingColdShedArmed` short-circuits on the fire aura, so this governs only bots that have
none. What it spends is real but not the bulk of the problem: **57.1%** of the 17,554/s the raid
takes is the raid-wide `64545` Frozen Blows tick that nothing avoids. Passing 2 stacks also clears
`bAchievGettingCold` (`:566` via `SetData(2, 1)` at `:1281`), which the Rare Cache does not care
about.

**A crowd or solo leg must travel at least `_SHUTTLE_HALF_LEG`** (`goesSomewhere` in
`DeriveHodirShuttleLeg`). At the room's edge the collision check pulls every probe on the wall side back
onto the bot's own spot, which is clear of every hazard, so it won the ring and `MoveTo` refused it as
`there`/`dup`: on 2026-09-23 two casters stood ~12 s at the east edge while Biting Cold went 4 → 7, and
died to their own tick. Legs come out full or collapsed, nothing between (108 of 284 crowd/solo legs
to a standing bot over five pulls sat under 0.5 yd), so half a leg is a clean floor. The tank, shelter
and Starlight branches take the farther of two fixed ends and cannot collapse.

**Inside a landed shelter the shed keeps running through the freeze cast**, shuttling
`_SHELTER_SHED_RADIUS` 3 across it, inside the 6 yd park so the run never fires on it;
`HodirGuardMultiplier` lets it through only there (`IsHodirInLandedShelter`). Standing out the 9 s cast
adds two stacks, and four of five freezes on 2026-09-19 had a bot gain two.

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
the shed.** The trigger fires on *any* stack because the action owns the shed-to-floor latch — but 87%
of the time a bot holds Biting Cold it holds exactly one, **32.8% of the fight each**, and there the
shuttle does nothing at all. `HodirRaidPositionTrigger` deferring to the trigger is what kept the
ranged out of Starlight: a usable zone was in range on **85%** of their ticks while they stood in one
for **22%**.

## Traps

- **Three icicle entries, and confusing them breaks the fight.** 33169 is the small one, dodged
  always. 33173 is the drift: dodged **only while falling** and only inside 7.5 yd, because the
  shelter run parks on it at 9. The dodge stands down once a 33174 exists within 9 yd of it. 33174 is
  the shelter, and is never dodged.
- The anchor is **not combat-gated**: `MoveInLineOfSight` is a no-op, so bots pre-position and the tank
  pulls from its spot past the centre.
- **Melee are out of the position node entirely, not just its anchor.** `IsActive` gates on the same
  `!IsRanged` the anchor uses, because every fall-through rule below it wants the bot further from him
  and a melee bot doing its job breaks the 15 yd one on every tick. They ride the boss and get a fire
  only because the tank parks *him* on one; nothing walks a melee bot to a fire or a zone. From the
  corner, zones sat a median 21.6 yd from him
  and melee stood in one 1.8% of ticks. From the centre, 6 of 12 zones on 2026-09-19 landed 5-15 yd
  from it, inside the ranged gap, and melee held Starlight 9.6% of the time (1.0-5.7% before).
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
  the casters, instead of walking half the raid across the room for six seconds of buff. No cast is
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
- Healers are excluded from the targeting node entirely, and **5** non-healers break a trapped
  raider's block, picked by a GUID window offset per block so several blocks draw disjoint sets
  instead of the same five; helper blocks go by the mage-first budget above. Freeing outranks the boss (the trapped raider dies to the next
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
5,835 move records for 2,142 accepted (2,338 `dup`, 1,277 `wait`); the position anchor 2,641 for 466
(1,774 `wait`).

Traced 2026-09-18 on the fire-drag build, three pulls wiped by command at ~3:25 with a human tank
holding him 40-95%: **0-3:00 boss dps 172.6k / 159.1k / 188.1k**, health at 3:00 20.3 / 26.5 /
13.1%. Walking 12.9-15.0k yd a pull, **44-46%** undone within 5 s; `OK`-verdict A-B-A flips
~175/min. The icicle dodge is now the mover — 34-36% of accepted moves, 13-15 per small icicle, only
18-19% issued from inside the 4 yd that kills — and Ice Shards is solved: 7-13 hits over 0-3:00,
almost all on the tank.

Traced 2026-09-19 on the centre-hold build, a **kill at 4:13 without Heroism** (Exhaustion from the
pull), Bulwark holding him throughout: **0-3:00 169.5k**, 21.7% left at 3:00; fire windows 234k (67 s),
centre 133k (113 s); Hodir walked **169 yd** over 0-3:00 against 311-425. Walking 15.7k yd, **52.9%**
undone within 5 s (46.3%): the shed doubled to 837 accepted moves (22% of all) and heads the flips,
shed ↔ dodge 103 and shed ↔ `set facing` 93, while dodge ↔ `reach melee` fell 73 → 0 and
`reach melee` moves 497 → 228. Ranged Starlight fell to **9.5%**: all ten latched the western zone but
stood in it 12-41% of its best minute (the freeze and post-freeze shelter pull took ~16 s of 58,
dodges at 8-27 each, the shed and block walks the rest), and the last zones landed north, 41 yd from
the raid and outside the 30 yd search. Three bot deaths, all battle-rezzed: the stalled dodge, the
Frozen Blows bleed at 4 Biting Cold stacks, and the channel plus freeze cast at 7, each fixed above.

Traced 2026-09-19 evening on the stall/shed build, three pulls stopped by command at ~3:29:
**0-3:00 boss dps 208.1 / 190.0 / 193.1k** against the 216.5k the cache wants, health left 4.1 / 14.5 /
10.8%; fire windows 220-282k against 133-189k on the centre. That cycle held — **no bot died before
3:00**, post-freeze shelter moves 134 → **0**, stalled walks 5 → 3/4/4, Biting Cold 1.70M → 1.42 /
1.24 / **1.37M** with peak stacks 7 → 5/4/4 and bot-seconds at 4+ stacks 344 → 121 / 118 / 118, Hodir
off his point 80 s of 113 → 23 of 131, 26 of 119, 17 of 89. What was left is the pack and the buffs:
melee and pets within 10 yd of him stood in the held fire **71 / 87 / 82%** of samples, worst window
**38%**, and windows with the fire on the tank's side of him ran 33-85% against 80-100% with it beside
or behind him — the campfire clip and the box, both fixed above.

Traced 2026-09-22 on the fire-hold build, a **wipe at 5:03 with 11 deaths** and 0-3:00 boss dps
**116.5k** against 190-208k. The hold did what it was built for: the fire was held for all 78 s one
burned inside the leash, **93%** of the melee and pets standing within 10 yd of him were in it, and
Singed reached 25 stacks **12.5 s** after the first fire against ~22, holding cap 126 s of 180. **The
pack was not there to cover.** Taking the tank-only gate off the position trigger's fall-through rules
dropped melee into the "inside 15 yd of him" one: their median gap to him went 5.7-6.0 → **14.4 yd**,
samples inside the 8 yd it takes to swing 70-77% → **10%**, the node issued **1,044** accepted moves to
melee against **0** in every earlier pull, and `hodir raid position action` ↔ `reach melee` became the
top flip at **494 at 1,330 ms** — the loop in [pitfalls](../../engine/pitfalls.md). It took the ranged
half with it: melee crowding the 15-35 band fed the clump rule, the sweep rang outward, ranged settled
at a **32.6 yd** median from him, and a usable stand was within `_BUFF_WALK` on **39%** of their
samples against 73 / 55 / 65%.

Traced 2026-09-22 evening on the melee-gate build, two pulls wiped at 3:30 with 0-3:00 boss dps
**161.9k / 191.1k**. The gate holds: `hodir raid position action` reads `r357 h186 t13` with no melee
in it, their median gap to him is **6.8 / 6.3 yd**, inside the 8 yd it takes to swing 68 / 70%, and
Singed reaches cap 12.2 s after the first fire and holds it 148 s of 180.

**One number carries the rest: 225k with a Toasty Fire burning anywhere against 142k with none**, and
a fire burned for only 108 s of 180. Coverage alone cannot close the gap — 150 s at those two rates is
210.6k — so the no-fire windows have to rise too. Two things hold them down. Ranged and healers cannot
reach the buffs: a legal Starlight stand existed on **57-60%** of their samples and sat inside 15 yd on
**6.6%**, the nearest zone a median 29.6 yd off, so they held Starlight 3.7% and a fire 33.9% and
carried Biting Cold **52.5%** of the fight. And 8 of 10 ranged leave the boss after every freeze until
the last block dies, **574-680 bot-seconds** of ice a pull.

**That is also what killed them.** Biting Cold was **26.4%** of all damage taken (38.5% and peak 7
stacks in the worse pull), and the shed is not at fault: once at 4 stacks bots are back under it in a
**2.0 s** median with 93% of them moving. The cost is sitting at 1-3 stacks out of a fire, where a fire
sheds on every tick for free. Every ranged and healer death was their own Biting Cold plus the Frozen
Blows raid tick (`64545`), 39-44 yd from him. The tank died at 2:54 to four Frozen Blows swings with
Divine Shield and Ardent Defender already spent, which is a **roster** problem: the second tank slot
was the human, so `HodirFrozenBlowsSwapTrigger` had no partner to hand him to.

**Storm Power is at its cap — not a lever, do not re-open it.** 10-12 Storm Cloud windows a pull
delivered 43 and 55 Storm Power, **5.4-5.5 per cloud against a hard 6**, because every application
spends a charge (`spell_hodir_storm_power_aura::OnApply`). Only the split is arguably wrong: melee took
31 of 55.

Traced 2026-09-23 on the buff-walk build, 24-man with one bot tank (Hodir's hp scales with the roster:
38.57M, so the cache wants 214.3k). **The 16:08 pull ran 215.4k over 0-2:39 with no deaths** until a
wipe command killed the raid at 2:39.5 with him at 10.9%; its own rates put the kill at 2:59-3:01.
Against the 22:49 pull: dps with a fire 225k → **253k**, without one 142k → 145k, the fire back
14.9-16.2 s after a freeze and alive 66% of the pull, ranged / healers in Starlight 3.7 / 2.9% →
**20.0 / 29.1%**, bots on ice over the first three waves 387 → 309 s. The 16:03 pull lost two casters
to the collapsed shed leg above, took 14-16 s on the mage blocks because `hodir raid position action`
walked the breakers 41 times in the wave (14 in 16:08), and the raid left at 2:07.

**A mage's fire lands ~9 s after its block breaks, and only the block time is a lever.**
`npc_ulduar_hodir_mage` polls its release every 1 s, then schedules Toasty Fire 6 s out behind its
Fireball and Melt Ice casts (`boss_hodir.cpp:1078-1125`). With the first mage free at 5.1 s the fire
came 13.8-15.0 s into the wave. The opener has the same shape: mage blocks down at 8.8 / 9.7 s, first
fire at 18.4 s, 73k dps until then.

**Two high-churn probes are not defects, and re-tuning them is wasted work.** `hodir.shuttle` reverses
`crowd ↔ held` 3,357 times at a 959 ms median, which is exactly the designed chain — `_SHUTTLE_HALF_LEG`
3.0 gives 6 yd legs, ~0.86 s at run speed. `hodir.stormcloud` did the same 192 times at 780 ms, which
was the 45° lap step and is gone with the lap. Both are advance-on-arrival, and the volume is a
symptom of how much walking the fight demands rather than of a latch that flaps.

**Re-measure the movement economy after each change, never after several.** Every figure above moved
under one edit at a time, and the three-latch pull is the one that cannot say which latch did what.
`tools/botobs/bosses/hodir.py <file>` prints every figure here (`--pace`, `--hold`, `--singed`,
`--buffs`, `--churn`, `--blocks`): time off the hold point, fire inside the leash against fire held,
the melee pack inside the held fire and what share of it was on him at all, the melee gap to him
against the 8 yd it takes to swing, the Singed ramp and the time at none with a fire burning, how
**boss dps with a fire burning against with none**, whether a legal buff stand existed at all against
whether it was inside each walk budget, Biting Cold by stack, time stood still at the shed's arm point
out of a fire, the bot-seconds a block wave spent on ice after the last mage was free, post-freeze
shelter moves, stalled walks, shed legs sent onto the bot's own spot (refused, so the stall count
cannot see them), and **the roles each mover walked** — the one line that names a gate that stopped gating.
Traces older than `hodir.hold` fall back to `hodir.centre`.

Each cause below is separate, and all of them are still easy to reintroduce.

- **The anchor stand-down tests the whole walk back, not just the anchor.** The dodge trigger goes
  false the moment the bot is clear, but the icicle stays lethal until it detonates at 3.7s. Checking
  only the destination lets the anchor walk the bot back under the blast, where the dodge re-arms —
  about 11 round trips per icicle, one icicle every 2 s. `HodirRaidPositionTrigger` projects each
  lethal icicle onto the bot→anchor segment for exactly this reason.
- **Do not "fix" this by widening the arrival tolerance.** A dodge always displaces further than the
  tolerance — by design, not the bug. Widening it stops the *return*, and at one icicle every 2 s the
  formation becomes an unbounded random walk out of the fire inside a minute. What works instead:
  with a stand in hand `HodirRaidPositionTrigger` asks one question — walk there or stay — and with
  none it is purely reactive, firing only on a broken constraint (inside
  `ULDUAR_HODIR_RANGED_MIN_BOSS_GAP` 15 of him, past `_CASTER_MAX_BOSS_GAP` 35, clumped under
  `_DECLUMP_RADIUS` 4.5). Those three are **ranged and healers only**, gated on the same `!IsRanged`
  the anchor uses so the two cannot drift apart, and the clump rule counts only ranged and healer
  neighbours — the action's sweep filters allies the same way, or it walks a bot out over a clump
  nothing complained about. The action then sweeps for the nearest spot clear of all three at once
  rather than inventing a home to walk to; with a stand it walks to the stand and holds no arrival
  latch, which would swallow a re-anchor. Tanks keep the spring through their anchor: Hodir follows
  whoever holds him.
- **Rank the ice-block breakers on latched positions, not live ones.** Live distances re-shuffle the
  assignment every tick and bots flick between a block and the boss; a guid rotation holds still but
  hands blocks to bots across the room, and they were spending 61.6% of their time on ice walking to
  one. `HodirBotLatches::blockRank` stores where each candidate stood when the wave went up and clears
  once no block is left, so every bot reads the same numbers for as long as the wave lasts.
- **Nothing walks a bot back into a pool the dodge just left** (`IsHodirWalkThroughLiveIcicle`). The
  position anchor tests the walk against each live icicle's clear; `reach melee`, `reach spell` and
  `set behind` (`HodirGuardMultiplier`, toward any current target, not tanks or healers) and the Storm
  Power collect test it against the dodge's own trigger radius, 4.5 / 7.5, and wait out the ≤3.3s.
  Before, the move right after a dodge ended inside a live pool 45-120 times a pull for `reach melee`,
  34-38 `set behind`, 9-57 the collect and 14-26 `reach spell`, against 0-3 for the anchor; dodge ↔
  `reach melee` was the top flip at 70-77 a pull. With the check on Hodir only, `reach spell` toward
  helper ice blocks still did it 50 times on 2026-09-19.
- **Tanks do not run the icicle dodge.** They ate a ~50 yd walk around the room and took Hodir with
  them; Bulwark ended up 70 yd from the boss while alive. Tanks eat the 14,000 instead, and the
  Biting Cold shuttle already gives them the movement they need without leaving the spot.
- **The dodge holds its destination through arrival.** `FindNearestPositionClearOfHazards` rings
  outward in 2 yd steps from wherever the bot is standing, so clearing `_dest` on arrival dropped
  through to a fresh sweep in the same tick and bought another 2 yd hop — **4,197** forced moves in
  one pull, a new destination every **410 ms**, 71% of them under half a second apart, 1,000-1,700 yd
  walked for 15-55 yd of displacement. Hold the spot and re-validate it against the live hazard list
  instead; sweep only once it stops being clear. `_DODGE_ARRIVE` is 0.8 for the same reason: at 1.5 a
  bot counted as arrived a fifth of the way into a 2 yd leg, still inside the radius that re-arms the
  trigger.
- **Both stands are latched per bot, and a reject must not drop the latch.** Zones and fires are
  ranked by walk from where the bot stands and the stand bearing is taken from there, so the answer
  moves whenever the bot or the boss does: stateless that was **739 anchor changes** across 23 bots at
  a median 3,896 ms, an 11 yd jump each, and Starlight windows lasting 1.6 s against zones that live a
  minute. Latching but erasing whenever the stored point failed re-validation made it *worse* —
  **1,201 anchor moves at a median 320 ms** over as few as 14 distinct points, **1,137 with a stand in
  force** — because the re-sweep hands the bot a different zone whenever Hodir drifts a yard past the
  caster band, which rejects next tick and back. Hold the zone or fire until it is gone, re-validate
  the stored point against the band every tick, and on a reject offer nothing that tick rather than
  dropping the latch. The walk gate is deliberately *not* re-checked: it decides which zone is worth
  starting for, and a bot already on its way is past that question.
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

**The pace is short, and sustain is the whole gap.** 2026-09-18's best 0-3:00 ran **188.1k** and fire
windows **238k**, and 2026-09-19 ran 169.5k with no Heroism, so what is left is the no-fire time after each freeze — the mage-first blocks and the
centre hold target exactly that. The best kill runs **5:04.3** at **128k** dps on
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
That drift is what the hold latch and the Starlight latch re-derive against every tick. Taking the
nearest shelter already cuts the tank's worst run from 31.6 yd to 2.6, so re-measure before adding a
tank-specific pick.

**The taunt floor may be set too low.** `ULDUAR_HODIR_TAUNT_HEALTH_FLOOR` ships at **50.0f**, but a
max-roll `63511` is 28,929 against Bulwark's 45,287 pool — **63.9%** — and two land about 2.4s apart.
A taunt accepted just above the floor can therefore still be a death inside an open Frozen Blows
window. The investigation argued for 65%; 50 shipped, and nothing records why. Re-measure before
raising it: the same trace showed the floor rejecting few enough taunts that the looser value may
have been deliberate.
