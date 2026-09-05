# Vezax

Facts below come from the script, the DBC CSVs and `acore_world`. **They override the public guides
where they disagree, and they do disagree.**

| Spell | Id | Effect |
|---|---|---|
| Aura of Despair | 62692 → 64848 | No mana regen, −20% melee attack speed. On aggro |
| Shadow Crash (cast) | 62660 | Every 10s from 13s. **Instant, missile `Speed` 10, `TARGET_DEST_TARGET_ENEMY`** — the destination freezes at cast time, so it is dodgeable. Random player **beyond 12.5 yd** (`3` plus both combat reaches), falling back to *any* target |
| Shadow Crash (impact) | 62659 | 11,310 **plus `KNOCK_BACK_DEST`**, **10 yd**. The ~1.8-2.6s of missile flight is the whole reaction window |
| Shadow Crash (field) | 63277 + 65269 | **8 yd, 20s.** +100% magic and +75% shadow damage done, +100% cast speed, −70% mana cost, **−75% healing done**. `SPELL_EFFECT_PERSISTENT_AREA_AURA`, so it exists as a `DynamicObject`. 63277 pulls 65269 in via `spell_linked_spell` type 2, so a bot inside carries **both** — **63277's own DBC row is only the damage and healing halves**, and reading it alone says the field does nothing for mana |
| Searing Flames | 62661 | On the tank, **radius 100 yd** = the whole raid. 13,875-16,125 fire, −75% armour 10s, every **8s (25m) / 15s (10m)**. **2000 ms cast, `PreventionType = 1`** — genuinely interruptible |
| Surge of Darkness | 62662 | 63s from pull, repeats 63s. Self-cast: +100% physical damage, −55% move speed, 10s — the slow is **on Vezax**, not the raid. Delays the Searing Flames group 10s |
| Mark of the Faceless | 63276 → 63278 | 20s from pull, repeats 40s, lasts 10s. Drains 5,000 hp/s from allies **within 15 yd** and heals Vezax. Prefers a player **beyond 15 yd** when ≥9 (25m) / ≥4 (10m) are out there, else someone inside |
| Saronite Vapors (NPC 33488) | summon 63081 | Every 30s at a **random point 45 yd out** (`TARGET_DEST_CASTER_RADIUS`, radius index 11). `NullCreatureAI` + `MoveRandom(4.0f)`, no addon auras: **the living cloud is harmless, never chases, and cannot be pulled** — the puddle lands where it spawned ±4 yd and the raid walks to it |
| Saronite Vapors (puddle) | 63323 (30s) → 63322 | Dropped on the **corpse**. 8 yd, reapplied every **2s**. Deals `100 · 2^stacks` and returns **half as mana** — stack 5 is 3,200, stack 8 is 25,600 |
| Saronite Animus | NPC 33524 | Hard mode. At vapor #6 with none killed, every vapor charges the anchor and merges |
| Saronite Barrier | 63364 | −99% damage taken on Vezax until the Animus dies |
| Profound Darkness | 63420 | Animus self-cast every 2s. 749 damage plus **+10% shadow damage taken per stack, 180s**. **Radius index 28 = 50,000 yd — room-wide and unavoidable** |
| Berserk | 26662 | 10 min, and instantly if the boss leaves `x ∈ [1720,1940]`, `y ∈ [20,210]` |

Two guide instructions that are wrong on this core: keep interrupting Searing Flames during the
Animus — **you do not**, `boss_general_vezax.cpp:228` skips the cast entirely while the Barrier is up;
and kill a vapor if mana is fine — **any single vapor kill calls `DoAction(1)` and disables hard mode
permanently** for that pull.

**The mana cheat is gone, not kept as a fallback**, following the Mimiron rebuild. The vapor puddle is
the raid's only mana source, and the exit is HP-predictive — leave when the next tick
(`100 · 2^(stacks+1)`) would exceed `ULDUAR_VEZAX_VAPOR_SOAK_MAX_TICK_HP_PCT` (0.35) of current
health, which self-tunes across gear and raid size. If the soak underperforms, tune it; do not
reinstate the cheat.

**Sara closes the burst gate here too**, at 167 yd — same static spawn, same fix; see Auriaya.

**The raid enters from the north** — trash at y 109-137, and the only door (194750, y 31.5) is
`DOOR_TYPE_PASSAGE`, opening when he **dies**, on the way to Yogg. `ULDUAR_VEZAX_ARC_ORIENTATION` is
**+1.5708**; the −1.5291 it replaced faced that exit and put every ranged and healer slot behind him.

**The formation is one camp of two groups.** Every healer and ranged bot takes a slot; a group is two
files of five, files 3 yd apart across the camp bearing and rows 3 yd apart along it, at
**boss-relative radii 22-34**. The two groups' inner files sit **5 yd** apart, near enough that one set
of heals covers the camp and far enough that they are not one clump. Boss-relative because every
distance the camp is built on is his, and a ranged pull can settle him yards off his spawn. Tolerance
is 1.2, and 3.0 for the tank, whose slot is the anchor `(1852.78, 81.3856, 342.461)` itself — a tank
standing on the spawn point is what keeps Vezax there for every other radius here. 20 slots against a
25-man's 18 healers and ranged, so it does not overflow; a bot that finds none falls through to the
melee de-clump and walks onto the boss, which is what `vezax.block = unslotted` exists to catch.

**Nothing here is out of Shadow Crash range, melee included.** `SelectTarget` skips only what is inside
`3` plus both combat reaches — `3 + 8 + 1.5` = **12.5 yd** — and a melee bot at boss combat reach sits
right on that line: one traced pull crashed a rogue at 16.1 yd and caught three more melee with it.
Melee and the tank are still left off the dodge, because holding the boss still is worth more than the
hits.

All 20 slots, the tank spot and the mark spots are navprobe-verified on the bot filter (`--nav 0x09`),
`distance to poly` ≤ 0.36. The WMO floor is flat at Z 342.378 out to ~24 yd north, then settles half a
yard lower on rubble, which `UpdateAllowedPositionZ` absorbs; the camp spans that seam and none of it
is off mesh. Raw terrain reads −27.707 in this room, so terrain height is meaningless here. Slots
persist per instance and fill in an alternating order (`L row 0`, `R row 0`, …) so neither group fills
first and healers spread across both; Ulduar's other bosses re-derive from a GUID rank every tick,
which reshuffles the whole formation on a death.

**Shadow Crash is dodged as a group, one fixed vector each way.** L strafes 15 yd one way and R the
other, both holding their radius, so each group keeps its shape and the field lands on the vacated
footprint for them to walk back into. The distance is set by the inner-file bot of the group that was
hit, which has to cross the impact: 15 yd of strafe minus the group's own 3 yd width leaves **12
against a 10 yd blast**, so widening the files widens the strafe. Only the front rank is tight on time
— 22 yd of flight is 2.2s and that bot needs 1.9s to clear — so pulling the camp inward is what spends
that margin; the strafe length does not, because a bot leaves the blast long before it stops walking.
`FindNearestPositionClearOfHazards` at 12 yd is the fallback for a bot with no slot or a lane buried
under a puddle. Position and soak both decline while an impact covers their destination, mirroring
Algalon — without that the bot clears, walks straight back with the missile still inbound, and never
casts.

**Returning `false` once parked is load-bearing.** Class interrupts sit at `ACTION_INTERRUPT` (40),
below `ACTION_RAID` (60), so a positioning action that returns `true` while moving starves every
interrupt that tick — and Searing Flames is the one cast in Ulduar that genuinely rewards
interrupting. Duty is GUID-ranked among bots that are both *capable and ready*, recomputed per cast so
cooldowns rotate it naturally.

**Field soak is bounded** to 15 yd of travel, or the far group crosses to a field on the near one.
Hunters are in: the −70% is `MOD_POWER_COST_SCHOOL_PCT` with mask 127, so it covers physical too and
only the damage half is magic-only. Healers never travel to one — at −75% healing done a field is
0.25x per cast and 0.83x per point of mana, worse on both axes — but they no longer avoid one either.
**Nobody treats a field as a hazard**: the camp is where crashes land and one 8 yd circle covers most
of it, so avoiding them would walk every healer out of the camp on a 10s cadence. The avoid list is
vapor puddles only.

**The puddle is a hazard for everyone who did not go and make it**, because `100 · 2^stacks` is paid in
health and buys nothing for a bot that is not short of mana. Only the handlers and anyone under
`LowMana` (15) may stand in one; no mana bar never qualifies. **Vapors are farmed by whoever needs the
mana**: non-tanks under **10%**, healers ranked first, two of them, closing to 5 yd before the kill so
the puddle drops at their feet rather than wherever they were standing. Nobody low means nobody kills
and the vapor despawns — correct, because the puddle is paid for in health.

**Mark of the Faceless always takes a fixed spot**, the nearest of three south of the boss at 26 yd,
opposite the camp and ~38 yd clear of every slot and strafe lane. Stepping outward along the marked
bot's own bearing is what a spread raid can afford; one camp cannot, because 18 yd from the front row
lands 6 yd short of the back row and drains it for the rest of the debuff. It was the largest damage
source in the first traced pull — 781k, every point of it healing Vezax through `HEALTH_LEECH`.

**The node ladder has no ties**, because a tie falls to vector insertion order. The dodge leads at
`EMERGENCY + 9`, the only hazard here with a deadline. The interrupt is `+8`, above the puddle exit at
`+7`: losing a kick costs the raid 13,875-16,125 fire while one more puddle tick costs one bot a
survivable hit, and the interrupt trigger yields on its own when `VezaxShouldLeaveVaporPuddle` says
the next tick is not survivable. Mark is `+6` — it heals the boss, but over 10s rather than at once.
The RAID band is the reward half: animus `+5`, vapor soak `+4`, kill vapor `+3` (a cost reduction buys
nothing for a bot with no mana), field soak `+2`, resistance `+1`, position last.

**Hard mode needs four separate guards, and the DoT one is the easy miss.** Targeting is zeroed
(`DpsAssistAction`, `TankAssistAction`), plus `CastDebuffSpellOnAttackerAction` on a vapor target and
every AoE-threat-type cast while a live vapor is in range, plus **explicit pet control** — a hunter
pet off passive will chew a wandering vapor with nobody noticing. A DoT ticking a vapor to death is
the quietest way to lose hard mode. Bloodlust arms only once the Animus is alive and only with hard
mode on. Assist tank 0 taunts the Animus; the main tank keeps Vezax inside the berserk bounds.

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

**The trace carries the decisions.** `vezax.slot` and `vezax.displaced` are stored assignments;
`vezax.formation`, `vezax.block` (`L`/`R`/`tank`/`unslotted`/`loose`/`stuck`), `vezax.dodge`
(`strafe`/`search`/`none`), `vezax.handler` and `vezax.interrupter` are derived, each probed inside the
helper that derives it so two call sites cannot disagree.

**The in-flight missile is a `haz` circle** from `VezaxHazardListenerScript`, since it has no world
object until it lands and the ~2s a bot can act in would otherwise be invisible. Take the destination
from `spell->m_targets`, never `GetUniqueTargetInfo()`: 62660's effects are both `TRIGGER_MISSILE` at
`TARGET_DEST_TARGET_ENEMY`, so the spell carries a destination and **no unit target at all** and the
unit list is empty. Reading it logged zero missile hazards across 21 crashes in one pull, and took the
mid-cast interrupt down with it unnoticed.

**The vapor puddle cannot be swept for either.** 63322 is `APPLY_AURA` / `PERIODIC_DAMAGE`, not
`Effect 27`, so it creates no `DynamicObject` and `SweepArea` can never put it in `snap.hz`, while the
corpse carrying it is dropped from `snap.u` by `PruneWatched`. It is noted as a `haz` circle at the
corpse from the existing 63323 branch instead. Only `snap.hz` feeds the death block's containment
test, so a puddle death never produces a `STOOD IN` line — read it off the 63322 rows in
`death.auras`, which carry stacks and `removedT`.

## Hard mode — the reference implementation

Hard mode = leave Saronite Vapors alive until the **Saronite Animus (33524)** spawns; Vezax gains an
invulnerable Saronite Barrier until it dies. Everyone switches target and kills it (no RTI mark —
each bot `Attack()`s directly). Nobody moves out of its Profound Darkness (63420): radius index 28 is
**50,000 yd**, so the stacking shadow-damage debuff is room-wide and the only answer is killing the
Animus faster.

## Shadow Crash is a dodgeable missile

The event picks a random target **beyond 12.5 yd** (`3` plus both combat reaches) and falls back to
any target, so it can land on anyone the formation puts outside that ring. 62660 is instant with
`Speed` 10 and `TARGET_DEST_TARGET_ENEMY`: the destination freezes at cast time and the missile takes
~1.8-2.6s to arrive, and that flight is the entire reaction window. The delayed spell stays in
`CURRENT_GENERIC_SPELL` for all of it, so the boss is **never** in `UNIT_STATE_CASTING` for this one
and a trigger must not gate on that. The impact carries `KNOCK_BACK_DEST`, so standing still does not
hold a formation together either. Key the dodge off the **cast**: an earlier node keyed off the *field*
(63277) and so moved only once the damage had already landed.

