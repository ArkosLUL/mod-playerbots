# Hodir

Held **mid-room** at `ULDUAR_HODIR_CENTRE` `(1998.0, -235.5, 432.687)`, or on a Toasty Fire within 25 yd.
The room is x 1965-2041, y -170 to -298, and he evades outside that y band. Every helper chases him
at a fixed `AttackStartCaster` stand-off and drops its zone at its own feet — priest 17, druid 22
(Starlight on `EVENT_DRUID_STARLIGHT`), shaman 25, mage 30 (fire on `EVENT_MAGE_TOASTY_FIRE`) — and
all eight spawn round the middle (`boss_hodir.cpp` `hhd`), inside their stand-off from the centre:
priests 16.3/15.5, druids 21.6/5.4, shamans 14.5/9.3, mages 23.2/6.5. So early on none walks, and zones
land where they spawned: Starlight a median 9.8 yd from a druid's spawn and **west** of him (29 of 36),
fires 8.2 yd from a mage's and **east** (36 of 53). **They still drift**: one mage's block sat 6.2 yd
from the centre at three freezes, then 14.0 and 24.2. `ChaseRange(range)` has MinRange 0, so none backs
away, and why they moved is out of reach: helpers are friendly, snapshots skip them, and their ice
blocks are the only position fix. A south-west corner hold dragged him 30-55 yd out to every fire and
back (913 of the 1,106 yd he walked over three pulls). navprobe settles the centre and 6/12 yd rings
round it at 432.687.

**Starlight measures 3 yd, whatever its DBC row says.** `62807` is aura **193
`SPELL_AURA_MELEE_SLOW`**, whose handler `HandleModCombatSpeedPct` applies `ApplyCastTimePercentMod`
as well as all three attack timers, amount 50 — **+50% haste to casting and swinging**. The row says
8. Binned by distance the hold rate is 94/90/78% across the first three yards and 21% in the fourth,
so the edge is 3 — bots cover 1.75 yd between snapshots, which blurs medians toward 4. 3 yd holds *one*
bot at the 4.5 yd spacing icicles force, so Starlight is a **per-bot opportunity, never something to
build a formation on**: `ULDUAR_HODIR_STARLIGHT_STAND_RADIUS` 1.5 plus 1.0 tolerance, `static_assert`ed
to stay inside it (2 + 1, exactly the edge, held Starlight one tick in five for a bot reporting
arrived), and a 30 yd search radius because the step runs per bot per tick. Nothing caps how many bots
try one zone: each takes its own bearing off the centre (`FindHodirStarlightStand`), and an icicle
catching two costs two heals (one Ice Shards hit is 41% of a pool, p90 54%) against +50% haste for
both. **Take the margin out of the stand radius, never the tolerance** — tolerance also stands the
position trigger down, so trimming it just moves churn from the dodge to the anchor.

**A Toasty Fire sheds Biting Cold and feeds Singed.** `spell_hodir_biting_cold_player_aura` sheds a
stack on `isMoving() || HasAura(62821)`, so a bot standing in one sheds on every tick without walking a
step, and magic damage cast from inside one procs Singed on its target. `62821` measures true to its
11 yd (with-aura p90 11.9). Its own aura is +4 Spirit and nothing else (aura 29, misc 4), so
everything a fire is worth comes from the shed and Singed.

**The caster band is 15-35** (`_RANGED_MIN_BOSS_GAP` / `_CASTER_MAX_BOSS_GAP`). Spell range is
measured to the target's bounding radius and Hodir is a giant, so the book 30 understates him: ranged
dealt **7,693 dps each from 30-35 yd against 7,844 from 20-25**, casts aimed at him started out to
40.1 (p99 36.2), and 35-40 is where it falls off (4,854).

**15 keeps casters off the melee, not off his swings**, which land only on his victim; nothing else
in his kit cares how close a raider stands. Melee stand within ~6.5 yd of him, so an icicle (4 yd) or
Freeze (10 yd) on one catches the other, and inside his melee range, **7.8 yd** (combat reach 5 +
1.5 + 4/3), a raider pulls aggro at 110% of the victim's threat rather than 130%. So **a Starlight
stand may come to 10** (`_STARLIGHT_MIN_BOSS_GAP`): the druids cast next to him, 46% of zone samples
sat inside 15 yd of him, and a stand within 15 yd walk rose from 32.6% of caster samples at 15 to
48.3% at 10. Fire stands, the position rules and the crowd shed keep 15.

**Ranged and healers have no home spot.** Nothing in his kit asks them to stand anywhere in
particular, so `DeriveHodirAnchor` offers a **buff stand or nothing**: Starlight first, since +50%
haste beats what a fire gives, else the nearest point of a live fire, capped at `_FIRE_STAND_RADIUS` 8
of its centre — park 8 / release 10 inside the aura's 11, and a bot already inside keeps the spot it
is on. Either only when the stand sits in its band (Starlight's starts at 10) and inside the walk the
bot will make. The fire *he* is held on needs no special case: with him on it, nothing within 8 yd of
it is 15 from him. `hodir.stand` reads `starlight` / `fire` / `none`. Two concentric rings round a
fixed anchor were deleted: a usable stand sat within 15 yd on 55-73% of samples while ranged held a
buff 23-24% of the time, and the ring anchor, 12-14% of every accepted move, walked bots off zones they
had reached.

**The walk depends on what the bot already holds** (`HodirBuffWalk`): `_BUFF_WALK` **15** for a bot in
Starlight or a fire, which gives up one it has to chase another, and `_BUFF_WALK_UNBUFFED` **30** for
one with neither, which gives up only the travel. 30 is the zone sweep's own radius, `static_assert`ed,
so nothing it finds is thrown away unread. **One flat number cannot serve both**, and the right one
moved with the hold ([pitfalls](../../engine/pitfalls.md#a-solved-table-outlives-the-traces-it-was-solved-from)).
30 is ~4 s of walking against a zone that lives 51 s: with him on a fire, 15 yd reaches 6.7% of the
legal stands up and 30 reaches 30.5%.

**One hold point per instance, read by both tanks** (`DeriveHodirHold`, `HodirBotLatches::hold`): the
centre, or the live Toasty Fire nearest the centre that is within `_HOLD_FIRE_LEASH` **25** of it —
and the point is **the fire itself**, not a spot beside it. Measured from the centre, never from him,
so he never ends up more than 25 yd out and every tank spot stays 40+ yd inside the y band he evades
outside. Melee wrap round him rather than lining up behind — p50 **1.2 yd behind his centre**, p10 6.3
behind, p90 4.1 in front, |lateral| p50 3.0, pets p50 3.3 behind — so a fire at his feet covers **98%**
of melee and pet samples against 92 / 91% for one 5 yd behind him and 80 / 73% at 8. The fire procs
nothing by itself; the bots standing in it do, which is the whole reason he goes on one. Nothing puts
a fire out but Flash Freeze (`npc_ulduar_toasty_fire::DoAction(1)`, called only from
`boss_hodir::SpellHitTarget` on `SPELL_FLASH_FREEZE_VISUAL`), so the pick holds until the fire dies —
re-picking while one burns walks him between two. `hodir.tankhold` reads the fire guid or `centre`,
`hodir.hold` the point.

A 12 yd box with a 2 yd backstep and a 7 yd reach existed only to keep the ranged ring out of the
**chamfered** SW corner (the floor bevels from ~(1966, -274) to ~(1990, -298)). With the ring gone it
just threw fires away — a fire burned inside 25 yd for 47-79% of 0-3:00 against 27-51% held — so it
was deleted.

**The campfire is a solid object**, so every point derived here goes through
**`ValidateStaticFloorPoint`** ([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)):
the ordinary floor check pulled tank anchors to the fire's near edge, and he parked 5-9 yd off it with
his back to the melee. The only solid gameobjects in the room are the two doors at y -166 / -298 and
the caches at (1967, -204) / (2036, -202), all 35+ yd outside anything derived here.

- **The main tank stands `_HOLD_TANK_OFFSET` 4.5 past the point**, on the far side from where he stood
  when it was adopted. He stops a median 5.0 yd from whoever holds him (p10 3.9, p90 7.0) and 3.6-4.2
  back along the drag, so he lands on it. 13 parked him ~9 yd short of the fire and left far-side
  melee outside its 11 yd. Within `_HOLD_BEARING_MIN_GAP` 3 of the point there is no far side, so it
  takes a fixed bearing into the open floor north-east of the centre. Out of combat it is re-taken
  every tick and latches on engage; a wipe drops the latch.
- **The off-tank stands 4.5 off the point, square to the hold bearing**, so a Frozen Blows taunt walks
  him ~6 yd sideways and leaves him on the fire either way. The tank Biting Cold shuttle runs its ±3 yd
  legs on the same axis, for the same reason: neither end may drag him off the fire.
- **Parked off the point, he is re-aimed.** He only walks toward whoever holds him, so once shoved past
  the spot (the tank flipping to a shelter after a freeze, a knockback) he parks with it already in
  reach — 5+ yd off for 80 s of 113 in centre windows, which also put the western Starlight zone
  inside the 15 yd gap. Still and over `_HOLD_REAIM_GAP` 5 off for `_HOLD_REAIM_MS` 2 s, the bearing is
  re-taken from where he stands; he normally stops within 1-2 yd.

**Singed lands on Hodir, and no `aura` row can show it**
([observability](../../systems/observability.md#what-a-trace-cannot-tell-you)). Each proc is a
triggered `cast` of `65280` with `tgt` = Hodir, 115-173 a pull. `62821` effect 2 is aura 42
`PROC_TRIGGER_SPELL` → `65280`, ProcChance **33**, ProcTypeMask **65856** = `DONE_RANGED_AUTO_ATTACK |
DONE_SPELL_RANGED_DMG_CLASS | DONE_SPELL_MAGIC_DMG_CLASS_NEG`: white swings never proc it, melee spells
and pets in a fire do, and the target is whoever was hit, at any range. It stacks to **25** for 25s
with aura **87 `MOD_DAMAGE_PERCENT_TAKEN` +2 per stack**, school mask 126 (magic only) — **+50% magic
damage taken** at cap, plus 3,000 fire. **`spell_hodir_toasty_fire_aura::HandleProc` casts `65280` from
the *player* carrying `62821`**, at whatever it just hit, so the fire is worth nothing standing empty and
everything with the pack in it. Modelled +1 a proc, he holds a mean **16-21 stacks** over 0-3:00
(+32-42% magic damage taken), capped from ~0:45 in two of three pulls. A lapsing fire is what loses it: one pull sat at 0 for
11 s **with a fire still burning that nobody stood in**, and the climb back to 25 takes 13-20 s after
the next one lands. Ranged in a fire he is *not* held on can carry Singed alone (60 of one pull's 140
procs). A player can Singe itself too: Biting Cold's tick (`CastCustomSpell(target,
SPELL_BITING_COLD_DAMAGE)` in `spell_hodir_biting_cold_player_aura`) is a magic-negative cast by the
player on the player.

**Helper blocks are broken mage first** (`GetHodirAssignedHelperBlock`):
`ULDUAR_HODIR_MAGE_BLOCK_BREAKERS` **5** each, then druid, shaman, priest at one each, inside the
8-breaker budget — **which drops to `_LATE_BLOCK_BREAKERS` 3 once no mage block is left**, since
nothing still on ice holds up the next fire and **170-176 bot-seconds a pull**, near a tenth of the
ranged group's whole fight, went on ice after the last mage was already free. **The budget is one
ceiling over every block up, never a per-block cap**: one freeze ices all eight helpers, so a cap per
block multiplied, 8 × 5 slots over 18 eligible bots, and put every dps on ice, up to eleven on one block
a median 21 yd from the boss. Breakers are ranged (melee only when the raid has none): a melee bot
leaving him makes a ~21 yd round trip. A block (32938, 110,675 hp) is summoned by the helper inside it, so
`ToTempSummon()->GetSummonerUnit()` names it, and `hodir.dpstarget` writes that kind before the guid.

**A mage's fire lands ~9 s after its block breaks, and only the block time is a lever.**
`npc_ulduar_hodir_mage` polls its release every 1 s (`EVENT_TRY_FREE_HELPER`), then `ScheduleAbilities`
puts Toasty Fire 6 s out behind its Fireball and Melt Ice casts. At one breaker a block the next fire
came 17.9-40.8 s (28.5 mean) after each freeze landed — longer than Singed's 25 s, so it reset every
cycle; mage first, 14-32 s. The first mage block still takes 7.5-19.8 s, its breakers held off it by
the post-freeze shelter run, the shed and dodges; three breakers free it in **4.4-11.0 s**, and the
block carries no mechanic, so that time scales with how many shoot it. **No helper block gets breakers
once Hodir is under 12%** (`_LAST_WAVE_HEALTH_PCT`; trapped raiders still do): a freed mage's fire
comes ~15 s after the landing at best and the last fire's Singed runs 25 s past it. The last wave goes
up at 7.6-9.9% (the one before at 31.7-36.7%) and soaked 0.44-0.78 M; its fire either burned 2.8 s
before the kill or never came.

**Breakers keep to their own block.** The blocks are attackers, so the `X on attacker` DoTs (relevance
19-19.5, over the single-target ones at 5-18) spread over all eight blocks and Hodir, and AoE fires on
the count: breakers spent 42-54% of their cast time off their block, and one wave's fire came 19.7 s
after the freeze against 14.3-15.6. So Hodir's `OwnTargeting` blocks `Family::DebuffOnAttacker` too, as
Kologarn and Vezax do (single-target DoTs stay), and `HodirBreakerAoeHoldMultiplier` zeroes
`ActionThreatType::Aoe` spells, heals aside, while the target is an ice block.

**Toasty Fire grants no Flash-Freeze exemption.** It is 11 yd and only blocks Biting Cold. The one
exemption is `SPELL_SAFE_AREA_TRIGGERED (62464)`, off `65705` on **NPC 33174**, radius index 40 → 9 yd.

| Mechanic | Ids | Numbers that drive the code |
|---|---|---|
| Flash Freeze | 61968 | **9s cast**, every 48-49s, 200 yd. Spares only 62464 carriers and pets |
| Shelter chain | 33173 → `62460` → `65370` + `62463` | Drift spawns at T+0 where its target will stand, lands T+3.9 → Ice Shards 14,000 in **7 yd** → summons **33174**, 12s. Cast ends **T+9.0** (targets drawn, fires wiped), blocks spawn **T+10.0** |
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
is the biggest buff in the fight, so all three run on every pull. `Playerbots.UlduarHodirHardMode`
and `IsHodirHardModeActive` were deleted rather than left gating nothing.

**The deadline is 3:00, not the 2:00 the guides give.** `SPELL_SHATTER_CHEST_TIMER` is **65272**, an
`EffectAuraPeriod_1` of 180000 ms on a 180000 ms duration, cast in `boss_hodir::JustEngagedWith`;
its single tick fires `EVENT_HARD_MODE_MISSED` and shatters the Rare Cache. So the target is halving
this fight, not thirding it.

## The Flash Freeze window is the nine-second cast and the shelter exists for five

**The cast's end, +9.0s, is the deadline, and "the landing" everywhere below.** `Spell::_cast` draws
the target list there, `spell_hodir_flash_freeze` drops Safe Area carriers and pets from it, the
2 s aura stuns everyone left, and the linked `62148` wipes the fires. The ice blocks spawn on that
aura's first tick at **+10.0s** — the one trapping in seven casts applied `61969` at +10.03s — with
no second Safe Area check, so nothing a bot does in that last second changes the outcome. The drift
lands at **+3.9s**, so the target covers only the last **5.1s**. The drift itself is on the floor from
+0.0s and stands exactly where its target will spawn, 0.0 yd apart across all seven, which is why the
run stages on it and uses the whole cast.

- **The run parks at 6 yd and releases at 8** (`ULDUAR_HODIR_SAFE_AREA_TOLERANCE` / `_RELEASE`, both
  `static_assert`ed inside the 9 yd Safe Area), and at **9 / 11 on a drift that has not landed**
  (`ULDUAR_HODIR_BIG_SHARDS_CLEAR` plus the same 2 yd, via `GetHodirShelterPark` / `_Release`).
  `MoveInside` → `MoveNear` lands the bot at *exactly* the tolerance, so testing one number at both
  ends stood the trigger down the tick it arrived and handed the next tick to the position anchor —
  measured walking bots 18-22 yd back out with the freeze 2s away.
- **Every other mover stands down for the whole cast, every role.** The hodir guard rules zero
  gap-closers (`CastReachTargetSpellAction` — Charge, Intercept, both Feral Charges), `disengage` and
  `blink` (spell actions that throw the bot: a Disengage 6 yd from Hodir threw a hunter 12 yd out of its
  shelter 2.5 s before the landing), and every `MovementAction` bar the shelter run, the icicle dodge
  and the shed inside a landed shelter. `ReachTargetAction` is in scope;
  `AttackAction` is exempt because it only sets a target. Before the gate, 68 of 125 bot-freeze pairs
  had their last accepted move come from something else, 36 of them the position anchor and 25
  `reach melee` / `reach spell`.
- **Do not instead return `true` from the shelter action.** `MoveTo` answers Duplicate for a
  destination it already issued, so from the second tick of a run `Execute` returns false and the
  engine descends past `ACTION_RAID + 6` — that descent is correct, and holding the tick would
  silence the bot's casting for six seconds, seven times a pull. The movers below are what has to be
  off.
- **The window is gated on the cast, not a timer.** `IsHodirFlashFreezeIncoming` tests
  `UNIT_STATE_CASTING` plus `FindCurrentSpellBySpellId`
  ([every slot](../../engine/pitfalls.md#detection-that-never-fires)).
  `MimironShockBlastTrigger::IsActive` is the same shape.
- **The shelter run is gated on the cast too.** A 33174 outlives the freeze by ~7 s, and keyed on
  targets alone the run fired after the landing for 96-134 of 278-358 shelter moves, buying nothing and
  flipping the tank 8 times in 7 s between a shelter and his spot, which dragged Hodir 8 yd off the
  centre.
- **Stage on the drift; do not wait for the target.** Keying the run on 33174 existing wasted the
  first 3.9s of every cast, because the hodir guard rules zero every mover from cast start: the
  raid moved **20.9%** of that half against a **51.7%** whole-fight baseline, then had 6.2s to cover
  up to 35 yd. Staged, it moves **26.0%**, which reads as arriving rather than falling short — the p50
  walk is 9.4 yd against a 9 yd park radius. Parking at `_BIG_SHARDS_CLEAR` stands it where the target
  appears without entering the 14,000 / 7 yd detonation. A bot already inside the blast needs no push:
  `MoveInside` answers false there and it descends to the dodge at `ACTION_RAID + 5`, which collects
  drifts at the same 9 yd clear. 9 also clears the dodge trigger's own 7.5
  (`_BIG_SHARDS_RADIUS + _DODGE_TRIGGER_MARGIN`), so the two never fight over a parked bot.
- **Each bot takes its own nearest, latched** (`GetHodirShelter`). Trigger and action still share one
  helper, because two derivations oscillate, but that never needed one answer for the whole raid —
  and ranking off one shared centre gave exactly that: one guid for every bot, all seven casts. The
  three land **5-31 yd apart** and `62464` holds off whichever is nearest (88% at 6-7 yd, 70% at 7-8,
  53% at 8-9, 26% at 9-10), so the detour bought nothing. The latch is only for the sideways dodge
  that carries a bot past the midpoint between two; otherwise nearest-to-self holds itself, since
  walking at a shelter keeps it the nearest. Against the shared pick it cut the walk in from p50
  **17.7 to 9.8** yd and bot-windows over 20 yd out from **68 to 11** of 175, and ranged and healers
  inside 15 yd of him from **29% to 12%**: the shared centre rode a fire, walking casters into his
  melee. It strands nobody: worst bot-to-nearest-healer **21.6 yd**, against heals landing at p50 12.3,
  p90 26.7, max 51.5. Counting healers per shelter is the wrong test and says otherwise.
- **The anchor is abandoned every 48s and that is correct** — tanks included. He is encased otherwise.
- **Residual, still open:** the dodge issues `MOVEMENT_FORCED` and the shelter run `MOVEMENT_COMBAT`,
  so a dodge firing late in the window can hold the movement lock until the freeze lands
  ([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)). Raising the run trades a ≤300s
  lockout for one Ice Shards hit at ~41% of a health pool — worth doing, but it needs its own
  before/after trace.

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

**A bot mid-cast cannot dodge, and every layer above it reports success**
([pitfalls](../../engine/pitfalls.md#a-bot-mid-cast-cannot-be-moved-at-all)). Blizzard pinned one
caster **5.7 s** and Evocation the same one for the **3.5 s** it had left, each ending in `62457` for
13,987 with the dodge's accepted destination 4-6 yd away; it costs **101 bot-seconds a pull, 2.9% of
ranged alive time** (Hurricane 29 s, Mind Flay 12 s, Evocation 11 s). The dodge and the shelter run
therefore call `BreakCastPinningTheFeet` first thing, which asks `PlayerbotAI::RequestSpellInterrupt`
— both triggers already gate on a bot that is in danger and has to relocate, and a 3.3 s fuse leaves
room for a 6 yd leg. That request is a deferred flag, so the leg issued that same tick is still
pinned. **The shed breaks only a channel**: a normal cast ends and then walks, but a warlock's 15 s
Drain Soul held one from 3 stacks to 5, the freeze cast added two, and the 7-stack tick killed it.

**A walk stopped short is never issued again on its own**
([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)): a mage's Mana Sapphire stopped
it 6.5 yd short of its dodge between two landed drifts, and a Disengaged hunter was refused its way
back into the shelter. `ReleaseStalledWalk` clears `last movement` once the bot has stood still
`ULDUAR_HODIR_STALL_MS` 500 short of the dodge's destination, the shelter or a shed leg.

**Biting Cold sheds on sustained movement only, and jumping cannot fake it.** A stack comes off on the
second *consecutive* tick where the server reads `isMoving()`, and any stationary tick between resets
that progress, so it needs more than a second of unbroken travel: the shuttle walks 6 yd legs
(`_SHUTTLE_HALF_LEG` 3.0), chaining down to `_SHED_FLOOR` **1**: the last stack ticks 400, takes 2 s of
walking to drop, and 4 s standing puts it back. It arms at **4 stacks, 5 in Starlight**
(`ULDUAR_HODIR_BITING_COLD_SHED_STACKS`); the tick is `200·2^stacks` and `62039` caps at 8. Arming at 2
made the shuttle 32.4% of accepted moves (18.7% of won engine passes) while six raiders dipped under
15% health. Arming at 5 stood bots through 3-5 stacks once ranged stopped standing in fires: Biting
Cold ran **1.70M over 0-3:00** against 0.84-1.41M, 44% of it at 4+ stacks, a hunter bled out at 4
stacks under Frozen Blows, and healers spent 42-77% of that window moving, mostly shedding. **3 is the
next step** if deaths or Biting Cold damage stay high. `IsHodirBitingColdShedArmed` short-circuits on
the fire aura, so this governs only bots that have none. Out of a fire, sitting at 1-3 stacks is the
real cost — once at 4 stacks bots are back under it in a **2.0 s** median — and Biting Cold plus the
Frozen Blows raid tick killed every ranged and healer who died in one wipe, 39-44 yd from him. Still,
it is not the bulk of the problem: **57.1%** of the 17,554/s the raid takes is the raid-wide `64545`
tick that nothing avoids. Passing 2 stacks also clears `bAchievGettingCold` (`boss_hodir::SetData(2,
1)`, called from `spell_hodir_biting_cold_player_aura`), which the Rare Cache does not care about.

**A crowd or solo leg must travel at least `_SHUTTLE_HALF_LEG`** (`goesSomewhere` in
`DeriveHodirShuttleLeg`), the travel floor of
[pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops): at the room's edge two casters stood
~12 s while Biting Cold went 4 → 7 and died to their own tick. Legs come out full or collapsed, nothing
between (108 of 284 crowd/solo legs to a standing bot over five pulls sat under 0.5 yd), so half a leg
is a clean floor. The tank, shelter and Starlight branches take the farther of two fixed ends and
cannot collapse.

**Inside a landed shelter the shed keeps running through the freeze cast**, shuttling
`_SHELTER_SHED_RADIUS` 3 across it, inside the 6 yd park so the run never fires on it; the hodir guard
rules let it through only there (`IsHodirInLandedShelter`). Standing out the 9 s cast adds two stacks,
and four of five freezes in one pull had a bot gain two.

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
- **Positioning and targeting wait for the pull** (`IsHodirEngaged`, Hodir `IsInCombat`):
  `MoveInLineOfSight` is a no-op, so an anchor firing on sight walked the raid to its spots before
  anyone pulled, and the targeting action's own `Attack()` pulled him. Whoever pulls flips both for the
  whole raid at once.
- **Melee are out of the position node entirely, not just its anchor.** `IsActive` gates on the same
  `!IsRanged` the anchor uses, because every fall-through rule below it wants the bot further from him
  and a melee bot doing its job breaks the 15 yd one on every tick
  ([pitfalls](../../engine/pitfalls.md#role-and-index-traps)). They ride the boss and get a fire only
  because the tank parks *him* on one; nothing walks a melee bot to a fire or a zone. From the centre,
  6 of 12 zones landed 5-15 yd from him, inside the ranged gap, and melee held Starlight 9.6% of the
  time (1.8% from the corner).
- **The Storm Cloud carrier holds still and the raid comes to it.** `spell_hodir_storm_cloud_aura`
  casts Storm Power `CastSpell((Unit*)nullptr, ...)` — an area pulse at the carrier, so how many it
  hits is bounded only by who stands inside 3 yd, and `_DECLUMP_RADIUS` 4.5 is wider than that.
  Lapping the ring therefore spent the charges one bot at a time: raiders sat inside the pulse on
  **4.4%** of samples during a live carry, p50 **0-1** of them, and the carrier's 3rd-nearest damage
  dealer was 7.0 yd away. Coverage ran **31%**, where buckets over 50% averaged **198,857 dps against
  123,105** under (r = +0.54) and the per-bot lift measured **+108%** — an upper bound, since a bot
  holding it is also a bot standing still. **26.2%** of delivery went to healers and the tank, who
  produced 2.1% of the damage. The carrier end wastes more and nothing here fixes it: the boss picks
  whoever he likes, and **14 of 53** carries in one pull went to a tank and 6 more to healers — every
  one a dead window, since tanks refuse to carry at all.
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
- **Storm Power is at its cap — not a lever, do not re-open it.** 10-12 Storm Cloud windows a pull
  delivered 43-55 Storm Power, **5.4-5.5 per cloud against a hard 6**, because every application
  spends a charge (`spell_hodir_storm_power_aura::OnApply`). Only the split is arguably wrong: melee
  took 31 of 55.
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
  instead of the same five; helper blocks go by the mage-first budget above. Freeing outranks the boss
  (the trapped raider dies to the next freeze) but the block has little health, so only bots within
  45 yd leave what they were doing. Targeting keeps no stickiness across priority entries: a block
  outranks Hodir while it lives, assigned or not, so holding the current one parked the raid on helper
  ice for the rest of the fight.
- **Node relevance is a survival ranking**, since the engine stops at the first action that succeeds:
  shelter run `ACTION_RAID + 6`, icicle dodge +5, Frozen Blows swap and threat redirect +4, targeting
  +3, Storm Cloud carry and collect +2, Biting Cold shed +1, Frost Resistance and raid position +0.
  Sheltering is the only failure that is death outright; targeting sits over the shed, which moves for
  seconds and would starve it, and costs nothing since it returns false once the target is right;
  position is last. The redirect never contends with the swap (hunters and rogues against tanks) and
  outranks targeting so the charges go out before a bot switches to a block.

## Every mover here thrashes unless it latches

The four Hodir movers own **82.8%** of movement time. Before any latch the raid spent roughly **43% of
its fight time walking between two destinations** (934 anchor/dodge reversals in two wipes, median gap
321 ms), with healers at 0-10% cast uptime. Latching the shed, the Starlight stand and the Storm Cloud
rally on top of the dodge took a pull from **6:20 at 115k to 5:48 at 124k**, and none of its six deaths
was a mover overshooting. Each cause below is separate, and all of them are still easy to reintroduce.

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
  `set behind` (hodir guard rule, toward any current target, not tanks or healers) and the Storm
  Power collect test it against the dodge's own trigger radius, 4.5 / 7.5, and wait out the ≤3.3s.
  Before, the move right after a dodge ended inside a live pool 45-120 times a pull for `reach melee`,
  34-38 `set behind`, 9-57 the collect and 14-26 `reach spell`, against 0-3 for the anchor. With the
  check on Hodir only, `reach spell` toward helper ice blocks still did it 50 times a pull.
- **Tanks do not run the icicle dodge.** They ate a ~50 yd walk around the room and took Hodir with
  them; one ended up 70 yd from the boss while alive. Tanks eat the 14,000 instead — Ice Shards is
  7-13 hits over 0-3:00, almost all on the tank — and the Biting Cold shuttle already gives them the
  movement they need without leaving the spot.
- **The dodge holds its destination through arrival.** `FindNearestPositionClearOfHazards` rings
  outward in 2 yd steps from wherever the bot is standing, so clearing `_dest` on arrival dropped
  through to a fresh sweep in the same tick and bought another 2 yd hop — **4,197** forced moves in
  one pull, a new destination every **410 ms**, 71% of them under half a second apart, 1,000-1,700 yd
  walked for 15-55 yd of displacement. Hold the spot and re-validate it against the live hazard list
  instead; sweep only once it stops being clear. `_DODGE_ARRIVE` is 0.8 for the same reason: at 1.5 a
  bot counted as arrived a fifth of the way into a 2 yd leg, still inside the radius that re-arms the
  trigger.
- **Both stands are latched per bot, and a reject must not drop the latch at once.** Zones and fires are
  ranked by walk from where the bot stands and the stand bearing is taken from there, so the answer
  moves whenever the bot or the boss does: stateless that was **739 anchor changes** across 23 bots at
  a median 3,896 ms, an 11 yd jump each, and Starlight windows lasting 1.6 s against zones that live a
  minute. Latching but erasing whenever the stored point failed re-validation made it *worse* —
  **1,201 anchor moves at a median 320 ms** over as few as 14 distinct points — because the re-sweep
  hands the bot a different zone whenever Hodir drifts a yard past the caster band, which rejects next
  tick and back. Hold the zone or fire until it is gone, re-validate the stored point against the band
  every tick, and on a reject offer nothing that tick rather than dropping the latch. The walk gate is
  deliberately *not* re-checked: it decides which zone is worth starting for, and a bot already on its
  way is past that question. **Never dropping fails too**: casters sat `held noreach` on a zone left
  too close to him for **33%** of their time, in runs of 20-43 s, with another usable zone within 30 yd
  on half of it. A Starlight latch drops after **3 s** of continuous reject
  (`_STARLIGHT_REJECT_DROP_MS`): 84-96% of held time over three pulls came in runs of 10 s+ and 0-1%
  under 3 s, so a band-edge wobble still holds. The fire latch still holds until the fire dies.
- **Put the zone in the note, not just the rule.** `NoteDerived` emits on change, so a re-latch onto a
  *different* zone under a bare `stand` wrote nothing and the flap above hid behind notes that looked
  exactly like a latch holding. `hodir.starlight` reads `stand <zone>`, and a held stand standing down
  reads `held noreach`, so a bot waiting for the boss to drift back is distinguishable from one with no
  zone at all.

**Two high-churn probes are not defects, and re-tuning them is wasted work.** `hodir.shuttle` reverses
`crowd ↔ held` 3,357 times at a 959 ms median, which is exactly the designed chain — `_SHUTTLE_HALF_LEG`
3.0 gives 6 yd legs, ~0.86 s at run speed. `hodir.stormcloud` did the same 192 times at 780 ms, which
was the 45° lap step and is gone with the lap. Both are advance-on-arrival, and the volume is a
symptom of how much walking the fight demands rather than of a latch that flaps.

Two things that look broken in a Hodir trace and are not: `hodir frozen blows swap action` logging
~95% `FAILED` is the stateless trigger retrying every ~110 ms while the taunt is on cooldown — count
the `OK` records instead, one per cooldown per Frozen Blows window is correct. And a
`thorim.squadsassigned` note inside a Hodir pull is `ThorimResetEncounterStateTrigger` clearing stale
state from an earlier attempt, which is cleanup working — Thorim nodes in a Hodir trace issue zero
accepted moves and zero `OK` verdicts, and `NearThorimEncounter` excludes his floor by height
(`z < ULDUAR_THORIM_WING_MAX_Z` 425 against 432.687).

**Re-measure the movement economy after each change, never after several**: a pull carrying three
latches at once cannot say which latch did what. `tools/botobs/bosses/hodir.py` scores every figure
here; its `--churn` line of **the roles each mover walked** is what names a gate that stopped gating.
Traces older than `hodir.hold` fall back to `hodir.centre` less `LEGACY_RING_ANCHOR`'s offset (the
probe once carried the ranged ring's centre), and before `87be6d955` "off the hold point" means
nothing. Fires and Starlight zones reach a trace only as `snap.hz` rows, clustered into zones by spot;
boss dps comes off his health column, since `dealt` counts the ice blocks too; and a helper's kind
comes only from `hodir.dpstarget`, so it reads `?` before the centre-hold build. Shed legs sent onto
the bot's own spot are refused, so the stall count cannot see them; the reader counts them apart.

## Pace

**The cache wants 216.5k over 0-3:00** against a 38.97 M pool (38.57 M at 24 raiders: his health scales
with the roster), and kills at **2:57 and 2:54 with no deaths** have made it (219.0k, 223.3k). Boss
dps runs ~250k with a Toasty Fire burning against 145-174k with none, and over 21 buckets casting%
correlates with boss dps at **r = +0.75**, moving% at −0.63 and fire coverage at +0.78: cast uptime is
the metric to move, and the fire is how.

**Lust waits for the first Toasty Fire** (`hodir lust hold`, `IsHodirLustHeld`), or 30 s into the pull
(`_LUST_FALLBACK_MS`; first fires come 17-26 s in, ~9 s after the first mage block breaks). Cast at
the pull it spent 12.5 s on a 94k opener with no fire and no Singed and ran out 12 s before the freeze
landed; from the first fire it covers that fire's window to the landing. The pull clock is the age of
65272 (`SPELL_SHATTER_CHEST_TIMER`) on him, and without it nothing is held.

## Still open

- **A breaker's Starlight stand walks it back out of range of its block.** `reach spell` walks the
  breaker in and `hodir raid position action` walks it back, every ~0.5 s: 80 flips with a mage block
  targeted in one pull, 77 of them holding a Starlight stand. Both pass `OwnMovement`, since
  `Family::Reach` is kept. Fix shape: `HodirRaidPositionTrigger::IsActive` stands down while the
  current target is an ice block.
- **No tank defensive cooldown is tied to a Frozen Blows window** — the tanks spent four and six in
  six minutes, unprompted.
- **Who eats Frozen Blows is not stable between pulls.** In one kill the bot tank took **27 of the
  35 `63511` hits** for 506k against the human paladin's 8 and died three times; the pull before was 19
  against 10 the other way with no tank death, and `hodir frozen blows swap action` produced **7 `OK`
  records against 17**. Healing was not the constraint — he received 1.43M against 0.76M, nearest
  healer p50 18.1 yd — so read the swap's gating before anyone's positioning. With the human in the
  second tank slot, `HodirFrozenBlowsSwapTrigger` has no partner to hand him to: a tank died to four
  swings with Divine Shield and Ardent Defender already spent.
- **The tank drags Hodir every 48s.** He moves **1.72 yd/s** during shelter runs against **1.27** the
  rest of the fight, because the tank walked 39-56 yd per window and Hodir follows whoever holds him;
  the hold and Starlight latches re-derive against that drift every tick. Taking the nearest shelter
  already cuts the tank's worst run from 31.6 yd to 2.6, so re-measure before adding a tank-specific
  pick.
- **The taunt floor may be set too low.** `ULDUAR_HODIR_TAUNT_HEALTH_FLOOR` ships at **50.0f**, but
  `63511` lands 12,645-28,929, a max roll is **63.9%** of a 45,287 tank pool, and two land about 2.4s
  apart. A taunt accepted just above the floor can therefore still be a death inside an open Frozen
  Blows window. The investigation argued for 65%; 50 shipped, and nothing records why. Re-measure
  before raising it: the same trace showed the floor rejecting few enough taunts that the looser value
  may have been deliberate.
