# Freya

**The trio wave is the whole encounter.** Snaplasher (32916), Storm Lasher (32919) and Ancient Water
Spirit (33202) each start their *own* 11s revive timer on death and come back unless all three are
down when it expires (`boss_freya.cpp:1155-1198`, `ReviveWithAllies` aborts on `DATA_TRIO_DOWN >= 3`).
A revived member removes no further `Attuned to Nature` stacks, so a raid that keeps missing the
window makes no progress at all.

They do not have equal health, which is what makes the sync hard. From `creature_template`
`difficulty_entry_1`:

| Add | 10-man | 25-man |
|---|---|---|
| Snaplasher | 312 792 | 977 475 |
| Storm Lasher | 234 594 | 781 980 |
| Ancient Water Spirit | 188 748 | 524 300 |

**Hardened Bark (62663) does not make the Snaplasher tankier.** It stacks to 99 at +10%
`MOD_DAMAGE_PERCENT_DONE` each, applied by proc 62664 when the Snaplasher is struck, and resets after
4s without a hit. It is a threat to whoever tanks it, never a reason to stop damaging it. An earlier
version of this strategy withheld all raid damage from the Snaplasher on the opposite assumption,
which is why the trio could never die together.

There is also a hard clock: `EVENT_FREYA_ADDS_SPAM` repeats every **60s** regardless of progress
(`boss_freya.cpp:612-623`), capped at 6 waves, so an uncleared wave gets a second one stacked on it.
In 25-man that is 2.28M trio health inside 60s, a ~38k raid DPS floor. Lifebinder's Gift repeats every
45s (`:625-629`), so Eonar's Gift always overlaps a trio kill.

**How the bots solve it.** DPS bots are split three ways by `GetFreyaTrioAssignment`: a greedy load
balance over *remaining* health, recomputed every tick. Every bot walks the same group order over the
same numbers and reaches the same split, so it needs no shared state — and it self-corrects, since a
member the raid over-kills sheds attackers on the next tick. Splitting rather than focus-firing also
keeps roughly two thirds of the raid off the Snaplasher at any moment, which holds Hardened Bark far
below its cap without anyone having to withhold damage.

`FreyaTrioSyncSuppress` is a backstop, not the mechanism: it only blocks damage on a member below
`ULDUAR_FREYA_TRIO_HARD_FLOOR_PCT` (10%) while a sibling is still above
`ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT` (15%), and releases entirely once all three are in the band.
A suppressed member drops out of the assignment candidates, so its bots move to a sibling instead of
standing idle. The band covers only the last tenth of the wave, about 6s of raid damage out of the
60s budget.

**Targeting is direct — Freya writes no raid icons.** `FreyaSetDpsPriorityAction` sets each DPS bot's
target itself, in the SWP M'uru shape (`SWPActions_Muru.cpp:178-380`), and
`FreyaDisableAutomaticTargetingMultiplier` stands the generic pickers down so they cannot reclaim it.
The trio target changes several times per wave as health converges, which a group icon cannot carry
without one bot spamming `SetTargetIcon` for everyone else to read back a tick later. A human's own
marks are not honoured here.

Priority order is Eonar's Gift > Ancient Conservator > trio slot > Detonating Lasher > Freya, with
two reorderings: once any trio member is below `ULDUAR_FREYA_TRIO_SYNC_WINDOW_PCT` (30%) the trio
outranks the other adds, and once every member is in the release band nothing pulls a bot away at all.
Eonar's Gift is a ranged DPS job (12s to a 30-60% Freya heal) so melee never eat the travel time both
ways, falling back to melee when no ranged DPS is alive. The Detonating Lasher rung resolves per role,
and for ranged in two phases — see the lasher paragraphs below.

**Conservator's Grip (62532) pacifies the whole raid.** It is `APPLY_AREA_AURA_ENEMY` +
`MOD_PACIFY_SILENCE` at radius index 28 = 50000 yd, cast once at 6s with no repeat
(`boss_freya.cpp:1205`), so it cannot be outranged and it lasts the whole wave. Pacify blocks melee
swings as well as casts, so tanks lose their damage and their taunt exactly like casters — it is not a
caster-only mechanic.

The only counter is Potent Pheromones (64321), a **6 yd** ally aura on a Healthy Spore. Spores are
summoned by the Conservator itself — 62566 is an 8s periodic triggering three directional summons
(62582 / 62591 / 62592) at radius index 9 = **20 yd** — and despawn after 22s. So they always sit 20 yd
away from the boss, and **melee can never be sheltered and in melee range at once unless the boss is
brought to a spore.**

That is what `FreyaTankAddsAction::ParkConservator` does, in the Ignis construct-tank shape: walk the
Conservator onto a spore and hold it there, hysteresis at `ULDUAR_FREYA_SPORE_RADIUS - 1` so it is not
nudged back and forth. The spore is latched in an `ObjectGuid` for its whole life, because fresh ones
keep appearing 20 yd from wherever the boss currently is and re-deriving the destination each tick can
flip it mid-walk.

Melee then target **that** spore rather than the nearest one — `GetFreyaConservatorSpore` keys off the
Conservator, never the calling bot, so the tank and the melee resolve the same spore without
communicating. Sending melee to their own nearest spore is what would oscillate: they gain the aura,
the DPS node drags them back to the boss to reach it, and they lose the aura on the way. Ranged and
healers do use their own nearest spore — they need the aura, not melee range, and any spore is inside
casting range of both the boss and the melee stack. Tanks are excluded from the spore node itself: the
add tank arrives inside the aura by dragging the boss there, and the main tank never repositions Freya.
`GetFreyaTargetSpore` is that whole rule in one call, and **the trigger and the action must both use
it**: deriving the spore twice let the LOS-filtered `"nearest npcs"` wake a bot for one spore while the
unfiltered grid search walked it at another.

**Never aim a bot at a creature's centre.** It sits inside the model's collision, so the bot can never
occupy it, `IsDuplicateMove` rejects the identical unreachable point from the second tick on, and the
tick falls through to the DPS chase, which walks the bot back out of the aura it just reached — 840
of 1865 spore move requests in one pull. Bots stop at `ULDUAR_FREYA_SPORE_STAND_RANGE` (4 yd) instead,
and the trigger stands down at `ULDUAR_FREYA_SPORE_RADIUS - 1` as well as on the aura, since the aura
is exact and lands a moment after the bot is already inside 6 yd.

Expect this stack to be broken up regularly. `EVENT_FREYA_NATURE_BOMB` repeats every **18s** for the
whole fight, dropping one bomb per player at their own feet — 7-10 in 25-man, 3-4 in 10-man
(`boss_freya.cpp:645-660`). Damage 64587 is 5850-6150 in **10 yd** with **no difficulty entry**, the
fuse is ~6s (`:1300-1317`), and the marker is GO **194902** summoned in the bomb creature's `Reset()`;
the creature itself is banished and never reaches the npc lists.

The escape rings outward to a spot clear of *every* bomb inside `ULDUAR_FREYA_HAZARD_SEARCH_RADIUS`,
because a volley drops one on each of the stacked melee. The old `FleePosition` dodge moved 5 yd out of
a 10 yd blast, so it killed everyone it fired for
([../../engine/pitfalls.md](../../engine/pitfalls.md)). Tanks are excluded: stepping out would drag Freya
toward the raid or lift the Conservator off its spore, and ~6k per volley is cheaper than either.
Dodging keeps its `ACTION_RAID + 4` priority — a bomb hit costs more than a few pacified seconds — and
the "go to the parked spore" rule is what makes the raid re-converge afterwards instead of smearing
across three spores.

**Tanks.** The main tank gets Freya, assist tank 0 works down a ladder: Snaplasher (the Hardened Bark
sink) > Ancient Conservator > highest-health non-suppressed trio member > a lasher standing next to it >
Freya. Generic `TankAssistAction` is zeroed for **every** tank for the whole encounter. Gating that on
"the ladder has something" is what let generic assist through on a pure lasher wave, where the off-tank
collected the wave and walked it into the raid stack.

The trio rung picks the **highest-health** member on purpose: tank damage is invisible to
`GetFreyaTrioAssignment`, which counts only DPS, so aiming it at the member furthest from the floor
makes that unaccounted damage help convergence instead of skewing it. It is percent-based, like every
other sync threshold, and sticky by `ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT` so the tank's own damage
closing the gap does not make it swap every few ticks. Storm Lasher and Ancient Water Spirit are still
never *owned* — the ladder only borrows them as a damage target — because owning them would mean owning
Tidal Wave positioning.

The taunt fires for the **Snaplasher and Conservator only** — the two adds the encounter claims. The rest
of the ladder is borrowed for damage: taunting Freya would fight a human main tank whose raid roles are
set differently, taunting a Storm Lasher or Water Spirit would mean owning Tidal Wave positioning, and a
lasher drops the taunt on its next 10s threat wipe regardless.

**Detonating Lashers cannot be tanked, ferried, or held by any threat redirect.** Every 10s each one
casts Flame Lash, then `DoResetThreatList()` and charges a **uniformly random** player within 80 yd
(`boss_freya.cpp:1256-1262`); they spawn the same way after a 5s submerge, and one that finds nobody
inside 80 yd despawns and still counts as cleared. They run at **8.0 yd/s** (`speed_run` 1.14286)
against a player's 7.0, so a bot can neither shake one nor lead one — it can only stand with it.

A `GROUP_LASHERS` wave is **10** of them — 717k in 10-man, **2.35M in 25-man** — for **2** stacks
each. That is 150.6k HP per stack, the **worst rate in the fight** (Water Spirit 66.9k · Storm Lasher
100.4k · Conservator 113.8k · Snaplasher 125.5k). Waves come at 10s then **every 60s, six of them**;
clearing one early pulls the next in after **5s**, and after the sixth the aura drops regardless — so
the wave is a survival check, not a DPS race.

Detonate fires in 15 yd **on death**, not on a timer. It has two difficulty ids — **62598** in 10-man
(base 4162), **62937** in 25-man (base 6824) — but both carry `EffectRadiusIndex 18`, so the radius is
identical either way. Measured over six 25-man pulls: mean **6366** a hit, max **10582** (crit), victims
out to **15.5 yd** — the searcher adds the object size at both ends — and **no falloff**, the 12-15 yd
bucket averaging as much as 0-3 yd.

**Detonate is a raid killer exactly when the raid is stacked.** Those six pulls lost **120 of 161** bots
to lashers, standing at a median **0.7 yd** nearest-neighbour with ~10 mates inside 15 yd: **6.7-10.4
victims a blast**, ten blasts a wave, ~64k on every bot in a 40k pool. The older 132s-pull reading —
melee-only, 4.6:1 under lasher melee — is what justified the gather doctrine this replaced, and it was
measuring a raid that died before it ate a full wave.

**Doctrine: spread, one bot per blast** (see
[Crowd control and threat on adds](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)
for why ferrying an add faster than a player cannot work). Spacing is the only lever, and it is a **step
function**: at 16 yd a lattice slot's four orthogonal neighbours sit outside the blast and one death
costs one bot, while anything under 15.5 brings all four back at once. Nothing in between.

1. **Slot.** `GetFreyaLasherSpreadSlot` — a rectangular lattice at `ULDUAR_FREYA_LASHER_SPREAD_SPACING`
   (16 yd), `_COLUMNS` (7), rows from the roster. Centre is Freya snapped to `_ANCHOR_GRID` (5 yd) and
   clamped by the half-footprint into the navprobe-verified `ULDUAR_FREYA_ROOM_{X,Y}_{MIN,MAX}`, with
   `_FALLBACK` (2357.83, -52.33, 425.76) for an unresolvable Freya. Snapping keeps the anchor piecewise
   constant: unsnapped, a yard of tank drift slides all 25 slots, and two bots deriving it a tick apart
   would disagree.
2. **Seat.** Roster order is healers then GUID, and the cell order hands out the **quarter points**
   first: the footprint's 101 yd diagonal is past a 40 yd heal, so healers seated together reach neither
   end, while quartering puts every bot within ~36 yd of one. The dead keep their slots — indexing by
   the living re-seats the formation on every corpse. The main tank has none; it holds Freya wherever
   the pull left her.
3. **Hold.** `FreyaLasherSpreadHoldMultiplier` zeroes `ReachTargetAction` and
   `CombatFormationMoveAction` for everyone but the main tank while a lasher lives. The load-bearing
   half: the spread node returns false once the bot is inside `_TOLERANCE` (3 yd), and the generic chase
   underneath would walk it straight back to Freya. Reaching a heal or resurrect target is exempt — the
   main tank stands off-lattice and has to stay reachable.

Node order: spread (`ACTION_RAID + 3`) → nova · trap · army (`+2`). The spread sits **above** the
Healthy Spore node, which decides the overlap the 60s wave clock creates: Grip is a pacify a bot lives
through, and a spore gathers six of them into one blast.

The mage novas on **self**, not through the class `frost nova` node, which gates on the *current target*
being within 10 yd and so never fires for a ranged mage. A sphere on the caster, so it wants
`ULDUAR_FREYA_FROST_NOVA_MIN_LASHERS` (2) inside `_RADIUS` (10 yd), and it holds the full **8s** — no
DR, no damage break ([../../engine/raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md)).

**Every** hunter traps, with a lasher inside `ULDUAR_FREYA_FROST_TRAP_ARM_RANGE` (20 yd) so the patch
arms before it arrives — at the hunter's feet, which is where the one that picked this bot is heading
anyway. Frost Trap 13809 lays 13810: 10 yd, 30s, **-50%**, taking a lasher to 4.0 yd/s against a
player's 7.0, the only thing here that makes one slower than the raid. It lands because Detonating
Lasher has **no `creature_immunities` row**. `FreyaLasherTrapReserveMultiplier` zeroes the generic
Explosive Trap for the wave: traps share a **30s category cooldown** and only one may be down, so the
damage node was spending the snare.

`freya summon army` opens Army of the Dead on the wave. Its ghouls AoE-taunt through **43263**, whose
script filters only `isWorldBoss()` targets, and in trace they held **12.5-18.9%** of all lasher
attention — the only thing that takes a lasher off a bot at all. Deliberately **not** named `army of the
dead`: `IsBurstCooldownAction` matches on the action name, and the Ulduar burst gate holds that list
until Attuned to Nature drops, which is the whole add phase.

The pack helpers survive for the piles the spread does not prevent — overlapping waves, or a raid too
small to fill the lattice. Ranged focus `GetFreyaLasherPackFocus`, the lasher with the most lashers
within `ULDUAR_FREYA_LASHER_PACK_RADIUS` (8 yd): `AoeTrigger` counts attackers within 8 yd of the
***current target***, not of the bot, so pointing at the middle of a pile is what makes class AoE fire
at all, and `_MIN_COUNT` (3) is `MediumAoeTrigger`'s own threshold. Below `_FINISH_PCT` (20%) with 3 up,
`FreyaLasherFinishAoeMultiplier` shuts non-healing AoE off inside `_PACK_CLEAR` (16 yd, one past
Detonate) and `GetFreyaRangedLasherFocus` — lowest health, GUID breaking ties, self-stabilising since
the focused add stays lowest — picks them off one at a time so the blasts stagger. Both gates read
`GetFreyaFinishingPackNear`, which measures from the **pack's** centre rather than the bot's.

Melee and tanks take only what is inside `ULDUAR_FREYA_MELEE_LASHER_RANGE` (12 yd) and drop it the
moment it runs past that, which is the leash: a lasher that retargets cannot tow a bot across the room,
and a tank can damage one on top of it but can never walk one back to the raid. Tanks never flee the
blast; they eat it, and the add tank works its normal ladder through a lasher wave.

**Threat redirect.** `freya redirect threat` feeds Misdirection / Tricks to assist tank 0 while the
Snaplasher or Conservator is up, otherwise to whoever is holding Freya, falling back to the group main
tank. `NPC_FREYA` is in `UldThreatRedirectMultiplier`'s block list so the class-generic main-tank node
stands down. This can do nothing for lashers — their threat table is wiped every 10s.

Hard mode = Elders left alive at pull (Brightleaf 32915 / Stonebark 32914 / Ironbranch 32913). Per
living Elder, Freya gains an extra ability: Iron Roots (62862), Unstable Sun Beam (62450), or Ground
Tremor (**62437** 10-man / **62859** 25-man).

**Ground Tremor is a raid-wide interrupt, not a knockback.** Alongside its 7599 physical damage it
carries `Effect_2 = 68` `SPELL_EFFECT_INTERRUPT_CAST` (`EffectMechanic_2 = 26`) at radius index 28 =
**50000 yd**, so there is nothing to dodge, and `DurationIndex 1` makes `ProhibitSpellSchool` a **10s
school lockout** on whoever it cuts. Measured over 7 volleys: 31 of 37 casts in flight were
`SPELL_PREVENTION_TYPE_SILENCE` and got cut, and **71% of those never landed a same-school cast inside
the next 10s**. Healers pay it hardest. Hunters are exempt — Steady Shot is `PreventionType` PACIFY,
which never reaches the lockout branch.

It is fully telegraphed: a **2000 ms** cast, repeating `25s..35s` (a range, so no timer predicts it).
So the handling is Ignis's Flame Jets trio, ported. `FreyaGroundTremorCastGateMultiplier` caches the
remaining window per millisecond and blocks any candidate whose `CalcCastTime` would not land first;
`freya ground tremor hold cast` (`ACTION_EMERGENCY + 2`) stops a cast already in flight. Unlike Ignis,
**heals are gated too, and that is the point** — holding a Chain Heal for under 2s beats losing Nature
for 10. Bots blocked here fall through to the instants already in their rotation. Stopping first also
dodges the lockout outright: `Spell::EffectInterruptCast` applies `ProhibitSpellSchool` **only if it
finds a cast to cut**. Outside hard mode the gate is inert — the window is 0 unless Freya is casting.

**Critical: the empower events are scheduled once at pull and repeat unconditionally — they keep
firing for the whole fight even after the Elder dies.** So hard-mode reactions must key off the
**hazard world object**, never off an Elder still being alive. Killing Elders never removes the
empower and never costs the achievement, because the Elder count is locked at pull.

The two object types differ in a way that matters:

- **Root creatures 33088 / 33168 are selectable** (unit_flags 0), so a trapped bot targets and kills
  its own root to free itself.
- **Beam stalkers 33170 / 33050 are non-selectable** (`0x2000000`), so they never appear in
  attack-target lists — find them by scanning `"nearest npcs"`.

Breaking Iron Roots sits at `ACTION_RAID + 5`, above the Sun Beam dodge at `+4`, because **a rooted
bot cannot move**, so it must free itself before it can step out of anything. The beam dodge rings
outward to a spot clear of every beam in range, not away from the nearest one.
`ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS = 12.0f` is a DBC guess.

**The dodge clears by `ULDUAR_FREYA_SUN_BEAM_CLEARANCE` (15 yd), not by the radius.** Clearing by the
radius plus a yard answered a bot on the rim with a ~2 yd step — measured median **1.9 yd** over 694
orders, 95% under 12 yd, so not one left the beam — and the next spawn put it straight back inside the
trigger. Three yards of hysteresis is what lets the trigger stand down after one move. Where
overlapping beams leave nothing that clear it falls back to radius + 1 rather than giving up: barely
outside beats standing in one.

The escape is also **latched** (`dodgeSpot`, ceiling `ULDUAR_FREYA_SUN_BEAM_LATCH_MS`). While a walk to
a still-clear spot is in flight the action returns true **without calling `MoveTo`** — claiming the
tick is the point, since that both stops the re-aim clearing the bot's own motion master and stops any
lower node re-pointing it mid-dodge. Unlatched, this node alone produced 584 of the raid's 1055
direction flips, fighting the spore node at ~3 Hz while the raid stood still.

