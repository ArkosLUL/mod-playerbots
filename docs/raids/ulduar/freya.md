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
ways, falling back to melee when no ranged DPS is alive. While a lasher pack is up only the first
`ULDUAR_FREYA_GIFT_SHARE` (5) ranged by GUID take it, and after `ULDUAR_FREYA_GIFT_SHARE_MS` (5s) the
rest join: ten bots take a Gift from full to dead in 4.2-6.3s, so five clear it inside the 12s while
the pack keeps the other half. Sending every ranged emptied the pack for 4.7-6.5s at 84% and 86% pack
health in the two waves that wiped, against 10% in the one that did not. The Detonating Lasher rung
resolves per role, and for ranged in two phases — see the lasher paragraphs below.

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
the DPS node drags them back to the boss to reach it, and they lose the aura on the way.

Ranged and healers can use any spore — they need the aura, not melee range, and all of them sit
inside casting range of the boss and the melee stack — so they take the nearest one **with room**,
fewer than `ULDUAR_FREYA_SPORE_CROWD` (6) raid members inside its 6 yd. Nearest outright resolves to
the same spore for every one of them, since they start the wave in one ball and that ball is on the
parked spore with the melee: `1788717562` had **18 of 25 bots sheltering on one spore** while four
others held a single bot each, and both of this wave's big hits are 8 yd wide (below). No latch is
needed, because the trigger stands down the moment the bot holds the aura, so the choice is only
ever made on the way in. A spore with an Unstable Sun Beam inside those 6 yd is skipped as well: the
pool is 5 of the 6, so it covers most of the shelter, and a bot sent there parks in it for the
pool's ten seconds. Order is roomy and clean, then clean, then nearest — sheltered and burning still
beats pacified. Tanks are excluded from the spore node itself: the add tank arrives inside the aura
by dragging the boss there, and the main tank is holding Freya on her anchor.
`GetFreyaTargetSpore` is that whole rule in one call, and **the trigger and the action must both use
it**: deriving the spore twice let the LOS-filtered `"nearest npcs"` wake a bot for one spore while the
unfiltered grid search walked it at another.

**Never aim a bot at a creature's centre.** It sits inside the model's collision, so the bot can never
occupy it, `IsDuplicateMove` rejects the identical unreachable point from the second tick on, and the
tick falls through to the DPS chase, which walks the bot back out of the aura it just reached — 840
of 1865 spore move requests in one pull. Bots stop at `ULDUAR_FREYA_SPORE_STAND_RANGE` (4 yd) instead,
and the trigger stands down at `ULDUAR_FREYA_SPORE_RADIUS - 1` as well as on the aura, since the aura
is exact and lands a moment after the bot is already inside 6 yd.

**Two 8 yd circles, and they are why the raid cannot be one ball.** Both carry `EffectRadiusIndex 14`,
and neither is hard-mode — they run on every pull.

**Nature's Fury** (62589 10-man / 63571 25-man) is a 10s mark the Conservator throws every 14s at a
random player within 100 yd (`boss_freya.cpp:1248-1251`). Its only effect is a 2s periodic trigger, so
it fires **five** times, 8 yd around the carrier each time — victims sat a median 2.3 yd out, none past
9.1. `freya nature fury bail` (`ACTION_RAID + 4`, above the spore node that would walk the carrier
straight back) runs it to the nearest spore with nobody else inside the splash; shelter groups sit
15-45 yd apart, so arriving clears it outright and keeps the pheromones. No free spore means open floor
and the Grip — ten pacified seconds beats five volleys into the raid. Tanks never bail: whatever they
hold walks after them.

**Sunbeam** (62623 / 62872) is a **1.5s** cast on a random threat-list target every 15-20s
(`boss_freya.cpp:633-636`), 8 yd at that target's feet. It lands where the target is when the cast
**ends**, not where it was when it began — `Spell::SelectSpellTargets` runs from `Spell::cast` — so the
target cannot dodge its own beam and only the bots around it can move. `freya step out of sunbeam`
(`ACTION_RAID + 4`; trigger at `_SUNBEAM_AVOID_RADIUS` 11, aim at `_CLEAR_RADIUS` 13, latch 1500 ms =
the cast) does that for ranged and healers only: melee and tanks cannot give up a second and a half,
and lose little by staying, since 13 of one pull's 16 beams were aimed at a ranged bot or its pet. Pets
are targeted and cannot be repositioned — the worst beam of `1788717562` landed because a hunter pet
ran 15.3 yd into the raid mid-cast, and **15 of its 16 victims were clear of 8 yd when the cast
began**.

**`1788717562` is the bill for standing in one ball.** It wiped at 4:59 with Freya at 100% and 120 of
150 stacks off — on pace, every wave clearing. Seven bots died inside **85 ms** at 4:12, eight inside
nine seconds, when Ground Tremor landed on a raid already ground down by a Sunbeam on 16 and five
Nature's Fury ticks on 13-17. Per minute, against the kill `1788613108` and the other wipe
`1788716763`:

| pull | out | dmg/min | Sunbeam | Ground Tremor | Nature's Fury | victims per beam | raid inside 8 yd |
|---|---|---|---|---|---|---|---|
| `1788613108` | kill | 899k | 137k | 243k | 105k | 4.5 | 5.0 |
| `1788716763` | wipe | 948k | 137k | 224k | 56k | 5.0 | 5.0 |
| `1788717562` | wipe | **1163k** | **271k** | 222k | **157k** | **12.5** | **8.0** |

Ground Tremor is the control — 50000 yd, nothing dodges it, flat across all three. The fight did not
get harder: the raid got tighter, nearest-neighbour **0.6 yd** against 1.3, and only the two mechanics
that scale with tightness moved.

Expect this stack to be broken up regularly. `EVENT_FREYA_NATURE_BOMB` repeats every **18s** for the
whole fight, dropping one bomb per player at their own feet — 7-10 in 25-man, 3-4 in 10-man
(`boss_freya.cpp:645-660`). Damage 64587 is 5850-6150 in **10 yd** with **no difficulty entry**, the
fuse is ~6s (`:1300-1317`), and the marker is GO **194902** summoned in the bomb creature's `Reset()`;
the creature itself is banished and never reaches the npc lists.

The escape rings outward to a spot clear of *every* bomb inside `ULDUAR_FREYA_HAZARD_SEARCH_RADIUS`,
because a volley drops one on each of the stacked melee. The old `FleePosition` dodge moved 5 yd out of
a 10 yd blast, so it killed everyone it fired for
([../../engine/pitfalls.md](../../engine/pitfalls.md)). Dodging keeps its `ACTION_RAID + 4` priority — a
bomb hit costs more than a few pacified seconds — and melee re-converge afterwards on the parked spore,
which is the one group here still meant to gather.

**It is latched (`bombSpot`, ceiling `ULDUAR_FREYA_NATURE_BOMB_LATCH_MS`) for the same reason the beam
dodge is.** The trigger fires at AVOID (11 yd) and the escape aims at CLEAR (13), so re-deriving every
tick answered a bot on the rim with a **1.9 yd** median hop that `reach melee` undid before the next
one: **1231 escape/closer flips in the 109s bomb phase** of `1788613108`, one bot re-aiming every
108 ms, melee walking 400-530 yd of path to finish 25 yd away. It falls back to AVOID + 1 where
overlapping bombs leave nothing clearing the full margin — without that it returned false on **164 of
394** attempts, 77% of them with 4+ bombs in range, leaving the bot standing in the blast. Below that
it clears the blast itself with no margin (`_BLAST_RADIUS`, 10) and then walks straight out of the
circles covering the bot, on the bearing away from their centre, because returning false still left
it standing in one for the whole fuse: **15** times in `1788724466`, one of them `Hellflame` dead at
**100%** with a bomb at 0.0 yd and three more inside 10, having issued no move at all in its last
34s.

**The latch has to outlast the fuse, and at 3000 ms it did not.** A bomb goes off **6s** after it
lands — `boss_freya_nature_bomb::UpdateAI` fires at `_explodeTimer >= 11000`, but the branch under it
snaps the timer from 5000 straight to 10000 — and the trace agrees, first sighting **6.0s** before
the blast (min 5.9, max 6.1, n=42). The old latch also stopped holding the moment the bot
*arrived*, and at 13 yd out the 11 yd trigger went quiet, so nothing reached the node again: **51
victims walked back into a live blast** in `1788724466`, a median **5.3s** in, moved by the escape
itself (40), `reach melee` (32) and `reach spell` (20). The trigger now reaches
`_HAZARD_SEARCH_RADIUS` with `isUseful` narrowing it back to the blast plus the latch window, and
**melee hold their spot** while a bomb still covers `bombOrigin`. Only melee:
`Engine::DoNextAction` breaks out of the queue on the first action that returns true, so a held bot
casts nothing — free for melee, out of range at the escape spot either way, and pure loss for ranged
and healers. A blanket hold shell is out for the same reason: with a bomb up, bots sit 10-14 yd from
one **34%** of the time.

**Both escapes route around both hazards** (`GetFreyaEscapeHazards`: bombs at CLEAR, beams at
`_SUN_BEAM_CLEARANCE`). They sit at the same relevance, so a bot reading only its own kind alternates
forever — **30%** of bomb destinations landed inside a beam and **61%** of beam destinations inside a
bomb, which is what killed the healer and the druid in `1788613108`. Each falls back to its own hazard
alone before giving up: somebody else's hazard beats your own.

**The main tank answers a bomb by moving Freya** (`freya tank nature bomb`, `ACTION_RAID + 4`), because
bombs land at players' feet and the melee stack is on her. **23 of 48** landed within 10 yd of her, her
melee ring was inside a blast **34%** of the bomb phase, melee uptime within 5 yd ran **7-18%**, and
raid damage halved — 136k/s before the first volley against 68k after. Tanks used to eat it: measured,
that tank never moved across six volleys — 4.2-4.3 yd from Freya every one — and took
**55,569 over 10 hits**, second-worst in the raid. Assist tank 0 is still excluded, since stepping out
would lift the Conservator off its spore. The spot uses `FindNearestPositionClearOfHazards`'s
`preferNear`, aimed at the far side of Freya from `GetFreyaRangedCampAnchor` — it only reorders spots
the same walk away, so the tank never takes a longer trip and never drags her toward the back line. Melee
re-close on the generic `reach melee`; nothing zeroes it.

**That preference had nothing pulling the other way, and it compounds.** A volley lands every 18s and
Freya walks after whoever holds her, so the tank random-walks in one direction all pull: in
`1788716763` she finished **100 yd** east of where she was tanked, on ground that settles to Z 419.9,
six yards below the tanking spot and down the slope toward the water, with the melee ring and half the
raid strung out behind her. Past `ULDUAR_FREYA_TANK_LEASH` (20 yd) from `ULDUAR_FREYA_TANK_ANCHOR` the
same `preferNear` flips to the anchor, and `freya tank hold freya` walks her home when no bomb is
pressing — `ACTION_RAID + 1`, under the escape, which still has to be able to leave the leash. It fires
only for the bot Freya is actually hitting, since walking anyone else moves no boss. The anchor is
**(2360.0847, -43.1235, 425.333)**, navprobe-verified on the bot filter (`--nav 0x09`): 0.52 yd from
the nearest poly, `UpdateAllowedPositionZ` 425.333, and a 12 yd ring around it 8/8 on mesh.

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

**A stacked raid eats every blast whole, and that is the accepted trade.** Six 25-man pulls
(2026-09-02) lost **120 of 161** bots to lashers, standing at a median **0.7 yd** nearest-neighbour
with ~10 mates inside 15 yd: **6.7-10.4 victims a blast**, ten blasts a wave. Even so, Detonate was
only **4.6-16.3%** of all damage taken — lasher melee and Flame Lash are the bulk — and those pulls
survived 2:06-4:01.

**Spreading past the blast was tried (`45d9a3e36`) and cost far more than it saved.** A 16 yd lattice
plus a multiplier zeroing `ReachTargetAction` and `CombatFormationMoveAction` did exactly what it was
built to do and the raid died faster (trace `1788552829`):

| | camp, six pulls | 16 yd lattice |
|---|---|---|
| Detonate damage taken | 4.6-16.3% of total | **1.9%** |
| median bot to nearest lasher | 3.7-10.2 yd | **17.5 yd** (p90 **46**) |
| melee DPS on the wave | 40.0k-77.7k | **21.6k** |
| lashers killed, first wave | wave cleared | **2 of 10** |
| wipe at | 2:06-4:01 | **1:53.9** |

The mechanism is worth keeping in mind before any Freya bot is moved: **`ReachTargetAction` is the
base class of both `reach melee` and `reach spell`, and the only generic path any bot has for closing
on a hostile target** — every other generic mover retreats or is non-combat, and `CastSpellAction`
out of range passes `isUseful`/`isPossible` and fails silently in `Spell::prepare`. So a bot parked
further from the pile than its own reach can never get back to it, and `FreyaSetDpsPriorityAction`
nulls any lasher past that reach, leaving it no add target at all. The wave stopped dying, Eonar's
Gift healed the survivors from ~5% back to ~65%, and sixteen living bots finished the pull on 9.5k
raid DPS between them.

**Doctrine: the camp is the back line's bail** — the melee one is melee-only and cannot reach them,
and 15 of the 22 deaths in the table below were ranged or healers. It steps off the same thing:
lashers under `_LASHER_BAIL_PCT`, never the healthy pack (see
[Crowd control and threat on adds](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)
for why ferrying an add faster than a player cannot work). `freya ranged camp` puts ranged DPS and
healers `ULDUAR_FREYA_LASHER_CAMP_STANDOFF` (18 yd, three past the 15 yd blast) clear of the nearest
**low** lasher, on the bearing the raid is already on so nobody crosses the pile to reach it.
`GetFreyaLasherCampSpot` walks outward from their centroid in `_CAMP_STEP` (2 yd) until they all
clear, capped at `_CAMP_MAX_STANDOFF` (28 yd) because past `AiPlayerbot.SpellDistance` (28.5) the
far side of the pile is unreachable, sweeping bearings when collision blocks the first. **With none
of them low it is `GetFreyaRangedCampAnchor`** — the lowest-GUID living ranged DPS, so every bot
picks the same one with no shared state — which still gathers the back line into one ball for the
AoE without asking it to walk; that is also the collision fallback, a live bot being always on the
mesh where a computed point is not. `ULDUAR_FREYA_RANGED_CAMP_TOLERANCE` (10 yd) has to fit inside
one AoE; `_HEALER_CAMP_TOLERANCE` (15 yd) gives healers the slack to also cover the melee group and
the tanks. Neither survives a *low* lasher inside Detonate range: the trigger fires whatever the
tolerance says, since a healer's slack is the whole blast. Tanks and melee are excluded: they are
already stacked on what they are hitting, and a tank that left would take Freya or the Conservator
with it.

Bots stop `ULDUAR_FREYA_RANGED_CAMP_SPACING` (6 yd) short of the spot on the bearing they arrived on,
so the back line lands as a ring rather than on one square — aiming every bot at the point itself put
them a median **0.6 yd** apart in `1788717562` against 1.3 in the kill. That alone cannot beat an 8 yd
splash: twelve bots would need a 15.5 yd ring, a 31 yd ball, past `AiPlayerbot.SpellDistance` and wide
enough to spread the pack the camp exists to gather. It stops the pile; the sunbeam step-out is what
answers the beam.

The anchor used to be the bot itself, which is why the camp had no fixed relationship to anything: in
`1788559467` it sat 23.8-35.7 yd from Freya and 16.3-25.5 yd from the pile, and moved **27.5 yd between
waves** because that one warlock walked. It also re-picks from the *living*, so the camp jumped twice in
0.3s as bots died at the `1788558567` wipe.

**Measured off the centroid the camp was never far enough out.** A pack spreads over a 16.8-21.4 yd
radius, so a spot 20 yd from the middle sits on whatever walked out in front of it. Four waves on
2026-09-05, distance from each ranged bot or healer to the *nearest* lasher:

| wave | median | share of the wave inside Detonate | deaths |
|---|---|---|---|
| `1788608477` 0:10 | **18.2 yd** | **18.5%** | **0** |
| `1788608477` 2:38 | 7.0 | 76.9% | 8 |
| `1788609171` 1:54 | 5.5 | 85.0% | 9 |
| `1788609636` 0:10 | 15.5 | 45.0% | 5 |

**Read that column as a cause and it costs a pull.** The clean wave was clean because the pack spawned
3.7-5.2 yd from the melee: seven of eight were in contact in **2.5s** and no lasher ever got within
8 yd of the back line. 18 yd was the effect of a fast intercept, not something the camp produced by
walking — and asking it to produce one against every living lasher is a retreat from something faster
than a player. `1788635038` wiped at 5:43 with Freya untouched because one wave of ten never died:
over it the back line walked **256 yd to net 19** and still lost ground, median distance to the
nearest lasher falling **20.0 → 17.3 → 14.3**, while the camp destination itself wandered **195 yd of
path for 27 yd net**, re-aimed a median 2.2 yd at a time, and fought `reach spell` all the way —
**521** camp move orders against **270**, and 450 OK / 444 FAILED over the pull. A ranged bot casts on
**21%** of ticks moving against **78%** standing, so ranged damage halved, **109.5k/s → 51.2k/s**
against the wave that had cleared ninety seconds earlier. Eonar's Gift then lived **14.5s** rather than
4-8 and bloomed three times; the heal at 4:29 took the five surviving lashers from 7-17% back to
**60-68%**.

**Nothing intercepts a fresh pack, and that is still open.** In the three bad waves the melee were
19-27 yd away and took **6.0-6.4s**, by which time 6-8 of 10 lashers had reached the camp, pack kill
rate had halved from **39.1 to 22.3-24.1 %hp/s**, and the whole raid was inside one blast. Melee
cannot win that race: in `1788609171` they needed a median 29 yd and covered 11.4 in six seconds —
**1.9 yd/s** against a 7.0 run speed, stuttering through `reach melee` and mid-cast 19% of frames —
while the pack crosses at 8.0.

`freya lasher about to blow` steps melee out of the blast of the lashers that are nearly dead, which is
the only warning this wave gives. A lasher under `ULDUAR_FREYA_LASHER_BAIL_PCT` (**15%**) sits there a
median **2.6s** (p25 1.5, p75 4.3) before it detonates — 18 yd of walking, enough to clear 15 yd. Melee
spend **26%** of the wave outside at that gate, which is the price; it clears only the *low* lashers,
never the whole pack, because clearing all ten is the lattice failure again. Tanks are excluded for the
same reason as the camp. The gate is per-lasher on purpose: the older `freya lasher pack step out` asked
for 3+ under 20% inside one 8 yd pack and fired **once in six pulls**.

**A dodge that never moves reports success.** Both escapes call `FreyaClearCastBlockingMove` before
`MoveTo`: `PointMovementGenerator` refuses to launch a spline while the bot is casting, and `MoveTo`
returns `Issued` anyway, so a channel pins a bot inside the blast it was just told to leave. In
`1788559467` the Sun Beam dodge logged **158 accepted moves** and bots channelling Volley or Mind Sear
went nowhere on **57-67%** of them; `Nightwarrior` held one coordinate for 3.0s, 2.4 yd inside the beam,
through four move orders. Both latches also break on `IsMovementPreventedByCasting()`, or a pinned bot
sits out the full hold. See [pitfalls](../../engine/pitfalls.md) for the general rule.

`FreyaAvoidAoeHoldMultiplier` zeroes the generic `avoid aoe`, which sits at `ACTION_EMERGENCY` (90),
outranks every Freya node, and replaces a 15 yd escape with a flat `AiPlayerbot.FleeDistance` (5 yd) hop
on a bearing of its own — it took the tick back from Nightwarrior's dodge 1.9s before it died. Freya
answers all three of its hazards with nodes that read every hazard at once, so the generic one has
nothing to add. XT-002 and Razorscale suppress it the same way. **Movement only**: nothing here zeroes
`ReachTargetAction`.

Node order: bail (`ACTION_RAID + 3`) → nova · trap · army (`+ 2`) → camp (`ACTION_RAID`). A dead bot
does no crowd control, so leaving a blast outranks laying one; the camp is the lowest-value thing a bot
can be doing here, so it sits under the Healthy Spore node and under the tank ladder, and
`MOVEMENT_COMBAT` rather than `FORCED` keeps the Nature Bomb and Sun Beam escapes above it.

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
script filters only `isWorldBoss()` targets. They hold **12.5-18.9%** of all lasher attention — the
only thing that takes a lasher off a bot at all — and the bot DK does open it: 9 casts, 213 taunts in
`1788552829`. Deliberately **not** named `army of the dead`:
`IsBurstCooldownAction` matches on the action name, and the Ulduar burst gate holds that list until
Attuned to Nature drops, which is the whole add phase.

Ranged focus is `GetFreyaLasherPackFocus`, the lasher with the most lashers within
`ULDUAR_FREYA_LASHER_PACK_RADIUS` (8 yd): `AoeTrigger` counts attackers within 8 yd of the ***current
target***, not of the bot, so pointing at the middle of a pile is what makes class AoE fire at all, and
`_MIN_COUNT` (3) is `MediumAoeTrigger`'s own threshold. Below `_FINISH_PCT` (20%) with 3 up,
`FreyaLasherFinishAoeMultiplier` shuts non-healing AoE off inside `_PACK_CLEAR` (16 yd, one past
Detonate) and `GetFreyaRangedLasherFocus` — lowest health, GUID breaking ties, self-stabilising since
the focused add stays lowest — picks them off one at a time so the blasts stagger. Both gates read
`GetFreyaFinishingPackNear`, which measures from the **pack's** centre rather than the bot's. That
conjunction is near-dead in practice: it held **once in six pulls**, because lashers scatter by design
and die one at a time.

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
bot cannot move**, so it must free itself before it can step out of anything.

**Its trigger needs both difficulty ids of both roots, or it never fires.** `spelldifficulty_dbc`
maps 62283 → 62930 and 62861 → **62438**, `HasAura` takes the exact spell, and a 25-man raid only
ever applies the 25-man half — so the original check on 62283/62861 alone could never be true, and
`freya break iron roots` had never once run: `1788724466` and `1788613108` carry 33 and 27
applications of 62438 between them and no verdict from the node. The root is permanent until its
creature dies; most break inside a second on incidental AoE, but **7 held 2s or longer** in
`1788724466` and two of those had the bot inside a hazard — one being `Tree`, dead at 4:36 rooted in
two beam pools with its dodge orders rejected. Every other Freya id here already carries its pair.

The beam dodge rings outward to a spot clear of every beam in range, not away from the nearest one.
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

