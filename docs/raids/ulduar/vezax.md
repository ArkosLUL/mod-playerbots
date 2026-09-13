# Vezax

Facts below come from the script, the DBC CSVs and `acore_world`. **They override the public guides
where they disagree, and they do disagree.**

| Spell | Id | Effect |
|---|---|---|
| Aura of Despair | 62692 → 64848 | No mana regen, −20% melee attack speed. On aggro |
| Shadow Crash (cast) | 62660 | Every 10s from 13s. **Instant, missile `Speed` 10, `TARGET_DEST_TARGET_ENEMY`** — the destination freezes at cast time, so it is dodgeable. Random player **beyond 12.5 yd** (`3` plus both combat reaches), falling back to *any* target |
| Shadow Crash (impact) | 62659 | 11,310 **plus `KNOCK_BACK_DEST`**, **10 yd**. The 2.7-3.6s of missile flight is the whole reaction window |
| Shadow Crash (field) | 63277 + 65269 | **8 yd, 20s.** +100% magic and +75% shadow damage done, +100% cast speed, −70% mana cost, **−75% healing done**. `SPELL_EFFECT_PERSISTENT_AREA_AURA`, so it exists as a `DynamicObject`. 63277 pulls 65269 in via `spell_linked_spell` type 2, so a bot inside carries **both** — **63277's own DBC row is only the damage and healing halves**, and reading it alone says the field does nothing for mana |
| Searing Flames | 62661 | On the tank, **radius 100 yd** = the whole raid. 13,875-16,125 fire, −75% armour 10s, every **8s (25m) / 15s (10m)**. **2000 ms cast, `PreventionType = 1`** — genuinely interruptible |
| Surge of Darkness | 62662 | 63s from pull, repeats 63s. Self-cast: +100% physical damage, −55% move speed, 10s — the slow is **on Vezax**, not the raid. Delays the Searing Flames group 10s |
| Mark of the Faceless | 63276 → 63278 | 20s from pull, repeats 40s. **10 ticks at 1/s**; each casts 63278 at the marked bot, leeching 5,000 from everyone else in range. See below — it is the fight |
| Saronite Vapors (NPC 33488) | summon 63081 | Every 30s at a **random point 45 yd out** (`TARGET_DEST_CASTER_RADIUS`, radius index 11). `NullCreatureAI` + `MoveRandom(4.0f)`, no addon auras: **the living cloud is harmless, never chases, and cannot be pulled**. Its puddle (63323 → 63322, 8 yd, `100 · 2^stacks` every 2s for half back as mana) drops **only from `JustDied`**, never from the script's own despawn |
| Saronite Animus | NPC 33524 | Hard mode. At vapor #6 with none killed, every vapor charges the anchor and merges |
| Saronite Barrier | 63364 | −99% damage taken on Vezax until the Animus dies |
| Profound Darkness | 63420 | Animus self-cast every 2s. 749 damage plus **+10% shadow damage taken per stack, 180s**. **Radius index 28 = 50,000 yd — room-wide and unavoidable** |
| Berserk | 26662 | 10 min, and instantly if the boss leaves `x ∈ [1720,1940]`, `y ∈ [20,210]` |

Two guide instructions that are wrong on this core: keep interrupting Searing Flames during the
Animus — **you do not**, `boss_general_vezax.cpp:228` skips the cast entirely while the Barrier is up;
and kill a vapor if mana is fine — **any single vapor kill calls `DoAction(1)` and disables hard mode
permanently** for that pull.

**Nobody touches the vapors, in either mode.** Killing one is the only way a puddle ever exists, so a
raid that ignores them never makes one, and hard mode needs no guard beyond that: there is no
kill-vapor, puddle-soak or puddle-avoid node, and nothing in the module references entry 33488. That
leaves **the field as the raid's only mana relief** — its −70% cost. Healers get no external source at
all and run dry; that is a known cost of the one-camp stand, not an oversight.

**The mana cheat is gone and is not coming back**, following the Mimiron rebuild.

**Sara closes the burst gate here too**, at 167 yd — same static spawn, same fix; see Auriaya.

**The raid enters from the north** — trash at y 109-137, and the only door (194750, y 31.5) is
`DOOR_TYPE_PASSAGE`, opening when he **dies**, on the way to Yogg. `ULDUAR_VEZAX_ARC_ORIENTATION` is
**+1.5708**; the −1.5291 it replaced faced that exit and put every ranged and healer slot behind him.

## The camp sits in a 13.5 yd band, and both edges are invisible from the code

Every healer and ranged bot takes a slot; a group is two files of five, files 3 yd apart across the
camp bearing and rows 2.25 yd apart along it, at **boss-relative radii 27 / 29.25 / 31.5 / 33.75 / 36**.
The two groups' inner files sit **5 yd** apart, near enough that one set of heals covers the camp and
far enough that they are not one clump. Boss-relative because every distance the camp is built on is
his, and a ranged pull can settle him yards off his spawn.

The radii are pinned between two limits, neither of which is a round number and neither of which is
readable without the arithmetic behind it. **Both come from Vezax's `creature_model_info.CombatReach`
of 8.0**, which `WorldObject::GetDistance` and `Unit::IsWithinCombatRange` both fold in:

- **Floor 24.5 yd.** Mark of the Faceless picks at random from everyone the boss measures beyond
  15 yd, falling back to everyone inside when fewer than 9 qualify (4 in 10-man). `GetDistance`
  subtracts both combat reaches, so his "15 yd" is `15 + 8.0 + 1.5` of real distance. The rows before
  this rework started at 22, which put the front rank in the melee ball's pool: the outside count ran
  8-12 against a threshold of 9 and three of nine marks in one pull landed on the tank.
- **Ceiling 38 yd.** `ReachTargetAction` tests `IsWithinCombatRange(target, spellDistance)`, which
  adds the same two reaches to `AiPlayerbot.SpellDistance` (28.5). Past it "reach spell" fires at
  `ACTION_HIGH` against this formation at `ACTION_RAID` and the two deadlock.

Slot tolerance is 1.2, and 3.0 for the tank, whose slot is the anchor
`(1852.78, 81.3856, 342.461)` itself — a tank standing on the spawn point is what keeps Vezax there
for every other radius here. 20 slots against a 25-man's 18 healers and ranged, so it does not
overflow; a bot that finds none falls through to the melee de-clump and walks onto the boss, which is
what `vezax.block = unslotted` exists to catch.

**Nothing here is out of Shadow Crash range, melee included.** `SelectTarget` skips only what is inside
`3` plus both combat reaches — `3 + 8 + 1.5` = **12.5 yd** — and a melee bot at boss combat reach sits
right on that line: one traced pull crashed a rogue at 16.1 yd and caught three more melee with it.
Melee and the tank are still left off the dodge, because holding the boss still is worth more than the
hits.

All 20 slots, the tank spot, both side spots and the southern spots are navprobe-verified on the bot
filter (`--nav 0x09`), `distance to poly` ≤ 0.39. The WMO floor is flat at Z 342.378 out to ~24 yd
north, then settles up to a yard lower on rubble, which `UpdateAllowedPositionZ` absorbs; the camp
spans that seam and none of it is off mesh. Raw terrain reads −27.707 in this room, so terrain height
is meaningless here. Slots persist per instance and fill in an alternating order (`L row 0`,
`R row 0`, …) so neither group fills first and healers spread across both; Ulduar's other bosses
re-derive from a GUID rank every tick, which reshuffles the whole formation on a death.

## Mark of the Faceless is the fight

Every tick the boss casts 63278 at the marked bot. `spell_mark_of_the_faceless_drainhealth` removes
`GetExplTargetUnit()` from the target list, so **the marked bot is the one person its own leech
skips** — nobody has to outrun their own mark, they have to leave everyone else. The radius reads 15
in the DBC, but the area test is `IsWithinDist3d`, which adds the victim's own combat reach: **16.5 yd
in practice**. Effect 2 is `SPELL_EFFECT_HEALTH_LEECH` with `EffectMultipleValue` **20**, which
`Spell.cpp:2864` feeds into `HealBySpell` on the caster — **the boss heals 20× what it takes**.

Measured, not inferred: across one pull's nine windows the boss **gained** health at **+0.225 %/s**
against −0.267 %/s outside them, netting **+20.25 % of 27,611,102 HP**. One untreated tank mark put
**+8.10 % back in 10 s**, about 25 s of the whole raid's output, and the boss stalled at 47 %. The
spread is 17 leech ticks in a window where the marked bot got clear against **76** where it did not.

Whole-pull baseline to measure the rework against, from
`603_4_general-vezax_1789302545.ndjson` (25-man, wipe 6:03, 23 deaths, boss stalled at 47%):
**1,462,825 leech damage over 345 ticks, 64% of it on the nine melee**.

**RaidObs cannot see that healing**: `ObsSession::Tracks` requires `IsPlayer()`, so a heal landing on a
creature is never logged. Read it off the boss HP trajectory in `snap.u`, never off `heal` rows.

**Who gets marked is not a free choice.** Melee stand 5-10 yd out, so `GetDistance` reads ~0 and they
can never be in the far pool — the rule structurally prefers whoever is far, and the camp is the only
thing that is far. Forcing it the other way means pulling ~7 camp members inside 24.5 yd, which makes
the pick a random draw over the whole near pool (~61% melee, not 100%) **and** puts those 7 inside the
leech of a marked melee: ~60 ticks a window against ~30. So the camp stands far enough that a melee
mark can never reach it, and the melee ball breaks out of what lands on it.

**A marked camp member side-steps 24 yd on its own group's side**, at the camp's mid radius. **24 is a
floor, not a preference**: the nearest slot on that side sits at tangential 5.5, so anything under 22
leaves a neighbour inside the leech — the camp's own 11 yd width sets it. Crossing to the southern
spots is what the previous layout did, and it dragged the leech straight through the boss and the
melee ball: one marked healer's path took it within 14 yd of Vezax and drained eight melee on the way.
A marked melee has no camp side and still takes the nearest of three spots south at 26 yd, ≥46 yd from
every camp slot.

**Both mark spots are measured from the boss, not the anchor**, and that is load-bearing twice over:
the camp they are spaced against is boss-relative, and `MovementAction::IsDuplicateMove` suppresses
any re-issue of a destination matching the last one within 0.01 yd for `maxWaitForMove` (5s). Anchor
spots are bit-identical every tick, so one aborted move stranded a marked bot for the rest of the
debuff — six issued moves against **33 `dup` and 11 `wait`** in one pull. The mark move also runs at
`MOVEMENT_FORCED`: `IsWaitingForLastMove` only yields to a strictly higher priority, so at
`MOVEMENT_COMBAT` any dodge move still in flight swallowed it.

**Melee break out; the camp does not.** `VezaxMarkOfTheFacelessBreakTrigger` fires for anything that is
not ranged and not the main tank, is not itself marked, and has a marked ally inside
`ULDUAR_VEZAX_MARK_BREAK_DISTANCE` (18). The tank holds the boss whatever happens. Camp members are
deliberately excluded: they stand 27+ yd out, so nothing marked in the ball reaches them, and
scattering the camp every 40s would cost more cast time than the few ticks it saves. Nine melee out
for ~10s is roughly 400k of raid damage forgone against the 2.24M one window healed back.

## Shadow Crash

**Dodged as a group, one fixed vector each way.** L strafes 15 yd one way and R the other, both
holding their radius, so each group keeps its shape and the field lands on the vacated footprint for
them to walk back into. The distance is set by the inner-file bot of the group that was hit, which has
to cross the impact: 15 yd of strafe minus the group's own 3 yd width leaves **12 against a 10 yd
blast**, so widening the files widens the strafe. Flight time is no longer tight — the front rank's
27 yd is 2.7s against a 1.9s walk — because the camp moved out, not because the strafe changed.
`FindNearestPositionClearOfHazards` at 12 yd is the fallback for a bot with no slot. Position and soak
both decline while an impact covers their destination, mirroring Algalon — without that the bot
clears, walks straight back with the missile still inbound, and never casts.

**Key the dodge off the cast, never the field.** The event picks a random target **beyond 12.5 yd**
and falls back to any target. 62660 is instant with `Speed` 10 and `TARGET_DEST_TARGET_ENEMY`: the
destination freezes at cast time and the missile takes 2.7-3.6s to arrive, and that flight is the
entire reaction window. The delayed spell stays in `CURRENT_GENERIC_SPELL` for all of it, so the boss
is **never** in `UNIT_STATE_CASTING` for this one and a trigger must not gate on that. The impact
carries `KNOCK_BACK_DEST`, so standing still does not hold a formation together either. An earlier
node keyed off the *field* (63277) and so moved only once the damage had already landed.

**Field soak is bounded** to 15 yd of travel, or the far group crosses to a field on the near one.
Hunters are in: the −70% is `MOD_POWER_COST_SCHOOL_PCT` with mask 127, so it covers physical too and
only the damage half is magic-only. Healers never travel to one — at −75% healing done a field is
0.25x per cast and 0.83x per point of mana, worse on both axes — but they no longer avoid one either.
**Nobody treats a field as a hazard**: the camp is where crashes land and one 8 yd circle covers most
of it, so avoiding them would walk every healer out of the camp on a 10s cadence. With the vapors gone
the avoid list is empty and the hazard sweep returns fields only.

## Node ladder and guards

**No ties**, because a tie falls to vector insertion order. The dodge leads at `EMERGENCY + 9`, the
only hazard here with a deadline. The interrupt is `+8`: losing a kick costs the raid 13,875-16,125
fire at once. Then the two halves of Mark of the Faceless — **break at `+7`, carry at `+6`** — in that
order, because the bot holding the mark is the one person its leech skips and so is never the one
taking damage. Surge of darkness closes the EMERGENCY band at `+5`. The RAID band is the reward half
and numbers separately: animus `+5`, field soak `+2`, resistance `+1`, position last.

**Returning `false` once parked is load-bearing.** Class interrupts sit at `ACTION_INTERRUPT` (40),
below `ACTION_RAID` (60), so a positioning action that returns `true` while moving starves every
interrupt that tick — and Searing Flames is the one cast in Ulduar that genuinely rewards
interrupting. Duty is GUID-ranked among bots that are both *capable and ready*, recomputed per cast so
cooldowns rotate it naturally.

**Life Tap is suppressed above 60% mana**, by a Vezax-local multiplier rather than a change to the
warlock rotation. Aura of Despair stops mana regeneration, which makes Life Tap the only mana a
warlock can find here — and the generic rotation taps on cooldown whether or not there is anything to
spend it on. One traced pull had both locks casting it **every 1.25s, flat on the GCD, for the whole
fight** at 94-100% mana: 110 taps, ~220,000 health each, both dead by 3:10 having dealt a tenth of
what the comparable caster did. A multiplier keeps it inside the encounter and writes a `veto` row
naming the action, so a trace says outright whether it fired.

**Positioning is gated on the room, not just on presence.** Vezax is visible from outside his hall,
and a presence gate had bots prepositioning through walls before the pull while their generic movers
were already zeroed. `VezaxFormationActive` requires the bot inside a 45 yd bubble around the anchor
plus a 10 yd height band. The hall runs 70 yd north and west, so that bubble deliberately stops short
of the entrance: outside it the multiplier is inert, generic movement carries a bot in, and the gate
opens on arrival. Widening it is what would put a bot back on a path through a wall. Resistance and
state reset stay presence-gated; everything else is combat-gated.

`GetVezax` reads the instance object map (`ULD_DATA_VEZAX`) rather than sweeping for the entry:
`PossibleTargetsValue` recalculates a 100 yd `ignoreLos` search on **every** call, and the movement
multiplier asks once per action per pass. It has no liveness filter of its own, so a dead boss is
rejected explicitly.

## The trace

`vezax.slot` is the stored assignment; `vezax.formation`, `vezax.block`
(`L`/`R`/`tank`/`unslotted`), `vezax.dodge` (`strafe`/`search`/`none`), `vezax.mark`
(`side`/`south`/`break`/`none`) and `vezax.interrupter` are derived, each probed inside the helper that
derives it so two call sites cannot disagree.

**The in-flight missile is a `haz` circle** from `VezaxHazardListenerScript`, since it has no world
object until it lands and the ~3s a bot can act in would otherwise be invisible. Take the destination
from `spell->m_targets`, never `GetUniqueTargetInfo()`: 62660's effects are both `TRIGGER_MISSILE` at
`TARGET_DEST_TARGET_ENEMY`, so the spell carries a destination and **no unit target at all** and the
unit list is empty. Reading it logged zero missile hazards across 21 crashes in one pull, and took the
mid-cast interrupt down with it unnoticed.

## Hard mode — the reference implementation

Hard mode = leave Saronite Vapors alive until the **Saronite Animus (33524)** spawns; Vezax gains an
invulnerable Saronite Barrier until it dies. Everyone switches target and kills it (no RTI mark —
each bot `Attack()`s directly). Nobody moves out of its Profound Darkness (63420): radius index 28 is
**50,000 yd**, so the stacking shadow-damage debuff is room-wide and the only answer is killing the
Animus faster. There is **no** Bloodlust gate and **no** taunt or assist-tank wiring for the Animus —
this doc claimed both before 2026-09-13 and neither was ever in the code.

Nothing deliberately targets a vapor in either mode, so the guard list a wider encounter would need —
zeroed assist targeting, DoT and AoE suppression, explicit pet control — is not present and is not
needed. **A stray cleave or an off-passive pet can still kill one**, and that silently ends hard mode;
if hard mode starts failing, that is the first thing to measure.

## Known gaps

**65269 uptime trails 63277 by 20-25% on every bot.** They are linked (`63277 → 65269`,
`-63277 → -65269`) and should be identical, so a fifth of the field's mana and cast-speed half is
going missing. Measured across one pull on all 13 bots that held a field; cause not yet found.

**Melee do not dodge Shadow Crash.** `DodgesShadowCrash` requires `IsRanged`, and the table above says
why that is not the same as being safe. One melee took a crash hit in the last traced pull.
