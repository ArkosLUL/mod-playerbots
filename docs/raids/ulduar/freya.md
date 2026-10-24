# Freya

**The clock and the doses.** `Attuned to Nature` starts at 150 and comes off in fixed doses: a lasher
removes **2**, a trio member **10**, the Ancient Conservator **25**. `EVENT_FREYA_ADDS_SPAM` fires at
10s and repeats every **60s** regardless of progress, capped at 6 waves, so an uncleared wave gets a second one stacked
on it; clearing one early pulls the next in after **5s**, and after the sixth the aura drops regardless.
`SpawnWave` rolls trio / Conservator / lashers per block of three, so a pull has exactly two of each.
In 25-man the trio is 2.28M health inside 60s, a ~38k raid DPS floor. Lifebinder's Gift repeats every
45s, so Eonar's Gift always overlaps a trio kill.

Freya spawns at **(2338.46, −52.33, 425.55)**, and the room floor is solid over x 2300-2400 /
y −30..−80 at z 423-426: there is no hole to path around. **It is not the water — do not re-litigate
the stream.** `navprobe` across the worst measured detour returns **14.92 yd, `PATHFIND_NORMAL`, 3
polys**, identical under `--nav 0x09`; all 133 detour segments were churn between the spore node and a
dodge (117 from the spore node, worst 14.8 yd straight against 62.9 walked).

**Targeting is direct — Freya writes no raid icons.** `FreyaSetDpsPriorityAction` sets each DPS bot's
target itself, in the SWP `MuruSetDpsPriorityAction` shape, and the `freya disable automatic targeting`
rules stand the generic pickers down. The trio target changes several times per wave as health
converges, which a group icon cannot carry without one bot spamming `SetTargetIcon` for the rest to
read back a tick later. A human's own marks are not honoured here. Priority is
Eonar's Gift > Ancient Conservator > trio slot > Detonating Lasher > Freya; once any trio member is
below `ULDUAR_FREYA_TRIO_SYNC_WINDOW_PCT` (30%) the trio outranks the other adds, and once every member
is in the release band nothing pulls a bot away.

**Eonar's Gift is a ranged job** (12s to a 30-60% Freya heal), so melee never eat the travel both ways;
melee take it only when no ranged DPS is alive. While a lasher pack is up only the first
`ULDUAR_FREYA_GIFT_SHARE` (5) ranged by GUID take it, the rest joining after `_GIFT_SHARE_MS` (5s): ten
bots take a Gift from full to dead in 4.2-6.3s, so five clear it inside the 12s while the pack keeps
the other half. Sending every ranged emptied the pack for 4.7-6.5s at 84% and 86% pack health in the
two waves that wiped, against 10% in the one that did not. **Known-open:** `TakesEonarsGift` starts the
head start the first tick it sees both the Gift and a live pack, so a Gift summoned just before a wave
gets it from the pack's spawn, whatever the Gift's own age.

## The trio wave

Snaplasher (32916), Storm Lasher (32919) and Ancient Water Spirit (33202) each start their *own* 11s
revive timer on death and come back unless all three are down when it expires (`ReviveWithAllies`
aborts on `DATA_TRIO_DOWN >= 3`). A revived member removes no further stacks, so a raid that keeps
missing the window makes no progress at all. Their health differs (`creature_template`
`difficulty_entry_1`), which is what makes the sync hard:

| Add | 10-man | 25-man |
|---|---|---|
| Snaplasher | 312 792 | 977 475 |
| Storm Lasher | 234 594 | 781 980 |
| Ancient Water Spirit | 188 748 | 524 300 |

**The split.** DPS bots are split three ways by `GetFreyaTrioAssignment`, a greedy load balance over
*remaining* health recomputed every tick. Every bot walks the same group order over the same numbers,
so it needs no shared state, and it self-corrects: a member the raid over-kills sheds attackers next
tick. `FreyaTrioSyncSuppress` (via `FreyaTrioSyncMultiplier`) is a backstop, not the mechanism: it
blocks damage on a member below `ULDUAR_FREYA_TRIO_HARD_FLOOR_PCT` (10%) while a sibling is above
`_TRIO_FLOOR_RELEASE_PCT` (15%), and releases once all three are in the band. A suppressed member drops
out of the assignment, so its bots move to a sibling instead of idling. The band is the last tenth of
the wave, about 6s of raid damage out of the 60s budget.

**Hardened Bark (62663) does not make the Snaplasher tankier.** It stacks to 99 by proc 62664 when the
Snaplasher is struck (25-man: **64191** passive, **64190** stack) and resets after 4s without a hit. Each
stack is **+10% `MOD_DAMAGE_PERCENT_DONE` (aura 79) and −5% movement speed (`MOD_DECREASE_SPEED`, aura
33)**: the DBC row is `EffectBasePoints` 9 and −6 with `DieSides` 1, so both need the `+1`. It is a
threat to whoever tanks it, never a reason to stop damaging it: a strategy that withheld raid damage
from the Snaplasher is why the trio could never die together. It sits at the cap for most of every
trio wave, split or not: **7/25/43/61/78/99** stacks at +2/+4/+6/+8/+10/+15s (combat log). It swings
every 2.45s for **57-120k** on a non-tank and 22-50k raw on a DK tank who avoided 76% of them, so
whoever takes it off the tank dies (six deaths in two resets).

**A damage dealer near the tank's threat on the Snaplasher moves to a sibling** (`ShedSnaplasherThreat`,
not yet verified in a pull). The generic `threat` strategy is never added (`AiFactory`), so nothing
else stops a bot overtaking. At `ULDUAR_FREYA_SNAPLASHER_THREAT_SHED_PCT` (80%) of the highest living
tank's threat the pick becomes the healthiest non-suppressed sibling, returning under
`_THREAT_RETURN_PCT` (60%); aggro flips at 110% in melee. The tank gets `_SNAPLASHER_TANK_LEAD_MS` (5s)
alone on a fresh one, since a ratio cannot cover the opener. Damage moves, none is withheld: with no
sibling to hit, or no tank holding threat after the head start, the pick stands.

**The Ancient Water Spirit's Tidal Wave is the trio's one real cone.** `62936` is `EffectRadiusIndex 23`
= **40 yd**, base 9,250-10,750 with a knock-back, fired **3 s after** the spirit's charge `62935` while
it carries the 3,000 ms `62655` aura, so the charge is the telegraph. Its `spell_cone` entry is narrow
(about 20°), which makes stepping off the line cheap; re-read the DB value before building on the exact
angle. Median victims per cast went 1.0 → 5.0 once the raid tightened, 135,394 → 342,391 damage.

## The Conservator wave: Grip and spores

**Conservator's Grip (62532) pacifies the whole raid.** `APPLY_AREA_AURA_ENEMY` + `MOD_PACIFY_SILENCE`
at radius index 28 = 50000 yd, cast once 6s after engage with no repeat, so it cannot be outranged and
lasts the whole wave. Pacify blocks melee swings as well as casts: tanks lose their damage and their
taunt exactly like casters.

The only counter is Potent Pheromones (64321), a **6 yd** ally aura on a Healthy Spore. Measured raid
uptime is **30.7%** for the pheromones against **8.2%** for the Grip, which is why the spore node sits at
`ACTION_RAID + 2` rather than over the hazard dodges: the shelter is worth holding, not worth dying for.
The Conservator summons the spores itself (62566, an 8s periodic triggering directional summons
62582 / 62591 / 62592 / 62593 at radius index 9 = **20 yd**), and they despawn after 22s, so they
always sit 20 yd from the boss: **melee can never be sheltered and in melee range at once unless the
boss is brought to a spore.**

**Splitting the raid by compass quadrant was rejected on evidence, do not re-derive it.** The four
summons' `ImplicitTargetA` 82-85 are **caster-relative** (front-left, back-left and so on), not world
bearings, and the core only ever casts 62566 → 62582.

`FreyaTankAddsAction::ParkConservator` walks the Conservator onto a spore and holds it there, as
Ignis's construct tank does, with hysteresis at `ULDUAR_FREYA_SPORE_RADIUS - 1`. The spore is latched
by `ObjectGuid` for its life, because fresh ones keep appearing 20 yd from wherever the boss is and
re-deriving can flip the destination mid-walk. Melee target **that** spore: `GetFreyaConservatorSpore`
keys off the Conservator, never the calling bot, so tank and melee resolve the same spore without
communicating. Their own nearest spore would oscillate: they gain the aura, the DPS node drags them
back to the boss, and they lose it.

Ranged and healers take the nearest spore **with room**, fewer than `ULDUAR_FREYA_SPORE_CROWD` (6)
inside its 6 yd: nearest outright is the same spore for all of them, the parked one, and put 18 of 25
bots on it while four others held one bot each, with this wave's two big hits 8 yd wide. A spore with
an Unstable Sun Beam inside those 6 yd is skipped too: the pool is 5 of the 6, and a bot sent there
parks in it for its ten seconds. Order is roomy and clean, then clean, then nearest; sheltered and
burning still beats pacified. No latch is needed, since the trigger stands down once the bot holds
the aura. Tanks are excluded: the add tank arrives by dragging the boss there, and the main tank holds
Freya on her anchor. `GetFreyaTargetSpore` is that whole rule, and **the trigger and the action must
both use it**: deriving it twice let the LOS-filtered `"nearest npcs"` wake a bot for one spore while
the unfiltered grid search walked it at another.

**Never aim a bot at a creature's centre.** It is inside the model's collision, so the bot never
arrives, `IsDuplicateMove` rejects the repeat from the second tick on, and the tick falls through to
the DPS chase, which walks the bot back out of the aura: 840 of 1865 spore move requests in one pull.
Bots stop at `ULDUAR_FREYA_SPORE_STAND_RANGE` (4 yd), and the trigger also stands down inside
`SPORE_RADIUS - 1`, since the aura lands a moment after the bot is inside 6 yd.

## Spreading hazards: Nature's Fury, Sunbeam, Nature Bomb

**Standing in one ball doubles what scales with tightness.** A wipe at nearest-neighbour **0.6 yd**
(against 1.3 in a kill) took **1163k** a minute against 899k: Sunbeam 271k against 137k, Nature's Fury
157k against 56-105k, 12.5 victims a beam against 4.5-5.0, 8.0 raid members inside 8 yd against 5.0.
Ground Tremor, which nothing dodges, stayed flat at 222-243k, so the fight did not get harder. Seven
bots died inside 85 ms when a Ground Tremor landed on a raid ground down by a Sunbeam on 16 and five
Nature's Fury ticks. The kill after the fixes below: **729k** a minute, Nature's Fury **35k**,
nearest-neighbour **1.31 yd**.

Nature's Fury and Sunbeam are both `EffectRadiusIndex 14` (8 yd) and not hard-mode.

**Nature's Fury** (62589 / 63571) is a 10s mark the Conservator throws every 14s at a random player
within 100 yd. Its only effect is a 2s periodic trigger, so it fires **five** times, 8 yd around the
carrier (victims a median 2.3 yd out, none past 9.1). `freya nature fury bail` (`ACTION_RAID + 4`, over
the spore node that would walk the carrier straight back) runs it to the nearest spore with nobody else
inside the splash; shelter groups sit 15-45 yd apart, so arriving clears it and keeps the pheromones.
No free spore means open floor and the Grip: ten pacified seconds beats five volleys into the raid.
Tanks never bail: whatever they hold walks after them.

**Sunbeam** (62623 / 62872) is a **1.5s** cast on a random threat-list target every 15-20s, 8 yd at
that target's feet when the cast **ends** (`Spell::SelectSpellTargets` runs from `Spell::cast`), so the
target cannot dodge its own beam and only the bots around it can move. `freya step out of sunbeam`
(`ACTION_RAID + 4`; trigger at `ULDUAR_FREYA_SUNBEAM_AVOID_RADIUS` 11, aim at `_CLEAR_RADIUS` 13, latch
1500 ms = the cast) does that for ranged and healers only: melee and tanks cannot give up a second and
a half and lose little by staying, since 13 of one pull's 16 beams were aimed at a ranged bot or its
pet. Pets are targeted and cannot be repositioned: the worst beam landed because a hunter pet ran
15.3 yd into the raid mid-cast, and 15 of its 16 victims were clear of 8 yd when the cast began.

**Nature Bomb** starts 5s after Attuned to Nature drops and repeats every **18s** to the end, one bomb
at each of `urand(7,10)` players **within 70 yd** in 25-man, 3-4 in 10-man (`EVENT_FREYA_NATURE_BOMB`).
Damage 64587 is 5850-6150 in **10 yd** with **no difficulty entry**: 64650 is a separate, higher-damage
Nature Bomb at the same radius, and `spelldifficulty` carries no row for either. The marker is GO
**194902**, summoned in the bomb creature's `Reset()`; the creature itself is banished and never
reaches the npc lists. It goes off **6s** after it lands (`boss_freya_nature_bomb::UpdateAI` fires at
`_explodeTimer >= 11000`, but the branch under it snaps the timer from 5000 to 10000), matching the
trace: first sighting 6.0s before the blast (5.9-6.1, n=42).

**The bomb escape** rings outward to a spot clear of *every* bomb inside
`ULDUAR_FREYA_HAZARD_SEARCH_RADIUS` (30), because a volley drops one on each of the stacked melee and
`FleePosition` is no answer to it ([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)).
It keeps `ACTION_RAID + 4` (a bomb hit costs more than a few pacified seconds), and melee re-converge
afterwards on the parked spore. In the reference kill, 54 bombs in 7 volleys landed **17 hits**, with
a median escape latency of **0.65s**.

- **Latched** (`bombSpot`, ceiling `ULDUAR_FREYA_NATURE_BOMB_LATCH_MS`, 7000), since it triggers at
  AVOID (11 yd) and aims at CLEAR (13)
  ([oscillation](../../engine/raid-mechanics-lessons.md#movement-that-oscillates)): re-derived each tick it
  answered a bot on the rim with a 1.9 yd hop that `reach melee` undid, 1231 escape/closer flips in a
  109s bomb phase, melee walking 400-530 yd of path to finish 25 yd away.
- **The latch outlasts the 6s fuse.** At 3000 ms, and releasing on arrival, it let the 11 yd trigger
  go quiet at 13 yd out: 51 victims walked back into a live blast a median 5.3s in, moved by the escape
  itself (40), `reach melee` (32) and `reach spell` (20). The trigger now reaches
  `_HAZARD_SEARCH_RADIUS` with `isUseful` narrowing it to the blast plus the latch window.
- **Melee hold their spot** while a bomb still covers `bombOrigin`, and only melee: a held bot casts
  nothing (`Engine::DoNextAction` stops at the first action returning true), which is free for melee,
  out of range at the escape spot anyway, and pure loss for ranged and healers. A blanket hold shell is
  out for the same reason: with a bomb up, bots sit 10-14 yd from one **34%** of the time.
- **Fallbacks.** Where overlapping bombs leave nothing clearing the full margin it takes AVOID + 1
  (without it: false on 164 of 394 attempts, 77% with 4+ bombs in range). Below that it clears the
  blast itself (`_BLAST_RADIUS`, 10), then walks straight out of the circles covering the bot, on the
  bearing away from their centre: returning false left bots in a blast for the whole fuse, 15 times in
  one pull, one dead at 100% with a bomb at 0.0 yd.

**Both escapes route around all three hazards** (`GetFreyaEscapeHazards`: bombs at CLEAR, beams at
`_SUN_BEAM_CLEARANCE`, low lashers at `_DETONATE_RADIUS`). They sit at the same relevance, so a bot
reading only its own kind alternates forever: **30%** of bomb destinations landed inside a beam and
**61%** of beam destinations inside a bomb. Each falls back to its own hazard alone before giving up:
somebody else's hazard beats your own.

**The lasher third is per bot: only ranged and healers pay it** (`GetFreyaEscapeLasherPositions`). A
beam exit has no other reason to know the pile is there: a Sunbeam during a lasher finish sent five of
the back line to one point 9-10 yd from lashers at 5-9%, and three died. Melee and tanks get `{}` and
keep `freya lasher about to blow`; owing them the pile is the lattice again. Only lashers under
`_LASHER_BAIL_PCT` count. When nothing clears both, the step-out drops the bombs before the lasher: a
bomb is a 6s fuse on a spot the bot can still walk off, a lasher under 15% is not. Since: 0 of 47
back-line beam escapes with a low lasher aimed inside 15.5 yd of one (nearest 18.4).

**The main tank answers a bomb by moving Freya** (`freya tank nature bomb`, `ACTION_RAID + 4`), since
bombs land at players' feet and the melee stack is on her: **23 of 48** landed within 10 yd of her, her
melee ring sat in a blast **34%** of the bomb phase, melee uptime within 5 yd ran **7-18%**, and raid
damage halved, 136k/s before the first volley against 68k after. A tank that ate them took **55,569 over
10 hits**, second-worst in the raid. Assist tank 0 is excluded, since stepping out would lift the
Conservator off its spore. The spot uses `FindNearestPositionClearOfHazards`'s `preferNear`, aimed at
the far side of Freya from `GetFreyaRangedCampAnchor`: it only reorders spots the same walk away, so the
tank never takes a longer trip or drags her toward the back line. Melee re-close on the generic `reach
melee`; nothing zeroes it.

**That preference needs a leash.** A volley every 18s with Freya walking after her holder random-walks
her in one direction all pull: she once finished **100 yd** east, at Z 419.9 down the slope toward the
water, the melee ring and half the raid strung out behind her. Past `ULDUAR_FREYA_TANK_LEASH` (20 yd)
from `ULDUAR_FREYA_TANK_ANCHOR` the `preferNear` flips to the anchor, and `freya tank hold freya`
(`ACTION_RAID + 1`, under the escape, which must still be able to leave the leash) walks her home when
no bomb is pressing, only for the bot she is hitting. The anchor **(2360.0847, -43.1235, 425.333)** is
navprobe-verified on `--nav 0x09`: 0.52 yd from the nearest poly, `UpdateAllowedPositionZ` 425.333,
and a 12 yd ring around it 8/8 on mesh.

## Tanks

The main tank gets Freya; assist tank 0 works down a ladder: Snaplasher (the Hardened Bark sink) >
Ancient Conservator > highest-health non-suppressed trio member > a lasher standing next to it > Freya.
Generic `TankAssistAction` is zeroed for **every** tank all encounter: gated on "the ladder has
something", it let generic assist through on a pure lasher wave, and the off-tank walked the wave into
the raid stack.

The trio rung picks the **highest-health** member on purpose: tank damage is invisible to
`GetFreyaTrioAssignment`, which counts only DPS, so aiming it at the member furthest from the floor
makes it help convergence. It is percent-based like every sync threshold, and sticky by
`ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT` (5) so the tank's own damage does not swap it every few ticks.

The taunt fires for the **Snaplasher and Conservator only**, the two adds the encounter claims; the rest
of the ladder is borrowed for damage. Taunting Freya would fight a human main tank whose raid roles are
set differently, taunting a Storm Lasher or Water Spirit would mean owning Tidal Wave positioning, and a
lasher drops the taunt on its next 10s threat wipe regardless. Mocking Blow counts as a taunt
([crowd control](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)): Arms'
`piercing howl` once fell back to it, so the `hamstring` snare trigger made a warrior taunt a
Snaplasher at 64 stacks and die 270 ms later; it falls back to `hamstring` now.

**Threat redirect.** `freya redirect threat` feeds Misdirection / Tricks to assist tank 0 while the
Snaplasher or Conservator is up, otherwise to whoever holds Freya, falling back to the group main tank.
The hunter's three redirected Steady Shots go into what that tank holds (Snaplasher, else Conservator,
else Freya) and skip a trio member under the sync floor: shot into Freya they put the add tank on her
table and nothing on the Snaplasher's. `NPC_FREYA` is in `UldThreatRedirectMultiplier`'s block list,
so the class-generic main-tank node stands down. Nothing helps against lashers, whose table is wiped
every 10s.

## Detonating Lasher waves

**They cannot be tanked, ferried, or held by any threat redirect**
([crowd control](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)). Every 10s each
casts Flame Lash, then `DoResetThreatList()` and charges a uniformly random player within 80 yd; they
spawn the same way after a 5s submerge, and one that finds nobody inside 80 yd despawns and still
counts as cleared. They run at **8.0 yd/s** (`speed_run` 1.14286) against a player's 7.0.

A `GROUP_LASHERS` wave is **10** of them, 717k in 10-man and **2.35M in 25-man**, for **2** stacks each:
150.6k HP per stack, the **worst rate in the fight** (Water Spirit 66.9k · Storm Lasher 100.4k ·
Conservator 113.8k · Snaplasher 125.5k). The wave is a survival check, not a DPS race.

**The wave spawns on top of Freya and stands still for five seconds.** `SPELL_SUMMON_WAVE_10` (62687)
is cast ten times at `EffectRadiusIndex 18`, so the pack appears **within 15 yd of Freya**, each member
`REACT_PASSIVE` and stationary for **5 s** before it picks a first target. That window is why a frost
trap is worth placing ahead rather than reacting once the pack moves.

Detonate fires in 15 yd **on death**: **62598** in 10-man (base 4162), **62937** in 25-man (base 6824),
both `EffectRadiusIndex 18`. Over six 25-man pulls: mean **6366** a hit, max **10582** (crit), victims
out to **15.5 yd** (the searcher adds object size at both ends), and **no falloff**.

**A stacked raid eats every blast whole, and that is the accepted trade**
([gather nodes](../../engine/pitfalls.md#movement-that-silently-no-ops)): **6.7-10.4 victims a blast** at a
median 0.7 yd nearest-neighbour (179 hits in 19 blasts in the reference kill), yet Detonate was only
**4.6-16.3%** of damage taken. Lasher melee and **Flame Lash 62608** are the bulk: effect 31
`WEAPON_PERCENT_DAMAGE` at base points 149, i.e. **150% weapon damage**, 13-17k a hit, the single
largest source in four of six pulls (463k-920k).

**Spreading past the blast was tried (`45d9a3e36`) and cost far more than it saved.** A 16 yd lattice
plus a multiplier zeroing `ReachTargetAction` and `CombatFormationMoveAction` cut Detonate to **1.9%**
of damage, but put the median bot **17.5 yd** from the nearest lasher (p90 **46**) against 3.7-10.2,
melee DPS on the wave fell to **21.6k** from 40.0-77.7k, **2 of 10** first-wave lashers died, and the
raid wiped at **1:53.9** against 2:06-4:01 camped. `ReachTargetAction` is every bot's only way back
into range ([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)), and
`FreyaSetDpsPriorityAction` nulls any lasher past a bot's reach, leaving it no add target at all; Eonar's
Gift then healed the survivors from ~5% back to ~65%.

**Nothing intercepts a fresh pack, and that is still open.** In the three bad waves the melee were
19-27 yd away and took **6.0-6.4s**, by which time 6-8 of 10 lashers had reached the camp, the pack's
kill rate had halved from **39.1 to 22.3-24.1 %hp/s**, and the whole raid was inside one blast. Melee
cannot win that race: they covered 11.4 of a median 29 yd in six seconds (**1.9 yd/s**, stuttering
through `reach melee` and mid-cast 19% of frames) while the pack crosses at 8.0. **A camp's distance from
the pack is an effect, not a cause:** the one clean wave (median **18.2 yd** to the nearest lasher, 18.5%
of the wave inside Detonate, 0 deaths, against 5.5-15.5 yd, 45-85% and 5-9 deaths) was clean because the
pack spawned 3.7-5.2 yd from the melee and seven of eight were in contact in **2.5s**. A pack spreads
over a 16.8-21.4 yd radius, so a spot 20 yd from its centroid sits on whatever walked out in front. Asking the camp
to walk out to 18 yd against every living lasher is a retreat from something faster than a player:
one wave that never died had the back line walk **256 yd to net 19** while losing ground (20.0 → 17.3 →
14.3 yd), the camp destination wander **195 yd for 27 net** and fight `reach spell` all the way (**521**
camp move orders against 270), and ranged damage halve, **109.5k/s → 51.2k/s**, a ranged bot casting on
**21%** of moving ticks against **78%** standing. Eonar's Gift then lived **14.5s** rather than 4-8 and
took the five surviving lashers from 7-17% back to **60-68%**.

**Melee bail per lasher.** `freya lasher about to blow` (`ACTION_RAID + 3`) steps melee out of the blast
of the lashers that are nearly dead, the only warning this wave gives. A lasher under
`ULDUAR_FREYA_LASHER_BAIL_PCT` (**15%**) sits there a median **2.6s** (p25 1.5, p75 4.3) before it
detonates, 18 yd of walking, enough to clear 15 yd. Melee spend **26%** of the wave outside at that gate,
which is the price; it clears only the *low* lashers, because clearing all ten is the lattice again.
Tanks never flee the blast; they eat it. The gate is per lasher on purpose: the older `freya lasher pack
step out` asked for 3+ under 20% inside one 8 yd pack and fired once in six pulls
([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)).

**The back line has its own bail, per bot**, since 15 of 22 deaths in those waves were ranged or
healers. Inside `_DETONATE_RADIUS` of a low lasher, `freya ranged camp` (`ACTION_RAID`) walks a ranged
DPS or healer to its own nearest spot `ULDUAR_FREYA_LASHER_CAMP_STANDOFF` (18 yd) from every low one,
through the melee bail's walk (`StepOutOfBlasts`: latch, `MOVEMENT_FORCED`, cast interrupt, 16 yd when
nothing clears 18), never off the healthy pack. It stays on the camp node, under nova and trap, so a mage
roots what is chasing it before walking. Not yet verified in a pull. One shared computed spot, with each
bot stopping 6 yd short on its own side, sent bots between pile and spot to 12 yd: 80-100% of camp orders
aimed inside 15.5 yd, costing 48 back-line Detonate hits and three dead, where a back line 25 yd off a
ghoul-held pile took 0 of 70.

**Otherwise the back line gathers**, except while any lasher within `_HAZARD_SEARCH_RADIUS` is low: then
a bot outside the blasts stays put, since the walk to the ball crosses one. The camp is
`GetFreyaRangedCampAnchor`, the lowest-GUID living ranged DPS, so every bot picks the same one with no
shared state and a live bot is always on the mesh; anchored on each bot itself, the camp sat 23.8-35.7 yd
from Freya and moved 27.5 yd between waves. It re-picks from the living, so it jumps as bots die. `ULDUAR_FREYA_RANGED_CAMP_TOLERANCE` (10 yd) has to fit inside
one AoE; `_HEALER_CAMP_TOLERANCE` (15 yd) lets healers also cover the melee and the tanks. Tanks and
melee are excluded: they are stacked on what they hit, and a tank that left would take Freya or the
Conservator with it. Bots stop `ULDUAR_FREYA_RANGED_CAMP_SPACING` (6 yd) short of the spot on their
arrival bearing, so the back line lands as a ring (aimed at the point itself they stood a median
0.6 yd apart). That cannot beat an 8 yd splash: twelve bots would need a 15.5 yd ring, a 31 yd ball, past
`Playerbots.SpellDistance` and wide enough to spread the pack the camp gathers. It stops the pile; the
sunbeam step-out answers the beam.

**Crowd control.** Node order is melee bail (`+ 3`) → nova · trap · army (`+ 2`) → camp (`ACTION_RAID`),
since a dead bot does no crowd control. The gather is the lowest-value thing a bot does here: it sits
under the spore node and the tank ladder, and moves at `MOVEMENT_COMBAT` so the bomb and beam escapes
stay above it.

- The mage novas on **self**, not through the class `frost nova` node, which gates on the *current
  target* within 10 yd and so never fires for a ranged mage: it wants `ULDUAR_FREYA_FROST_NOVA_MIN_LASHERS`
  (2) inside `_RADIUS` (10 yd), and holds the full **8s**: no DR on the add, no damage break
  ([crowd control](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)).
- **Every** hunter traps with a lasher inside `ULDUAR_FREYA_FROST_TRAP_ARM_RANGE` (20 yd), at its own feet,
  where the lasher that picked it is heading anyway, so the patch arms first. Frost Trap 13809 lays 13810:
  10 yd, 30s, **-50%**, taking a lasher to 4.0 yd/s, the only thing that makes one slower than the raid.
  It lands because Detonating Lasher has **no `creature_immunities` row**. `FreyaLasherTrapReserveMultiplier`
  zeroes Explosive Trap for the wave: traps share a **30s category cooldown** and only one may be down.
- `freya summon army` opens Army of the Dead on the wave. Its ghouls AoE-taunt through **43263**, whose
  script filters only `isWorldBoss()` targets, so **cast on time it is the wave**: the only thing that
  takes a lasher off a bot at all. Ghouls holding 7-10 of 10 lashers by +5.5s let through **0.3k and
  15.7k** of lasher melee and Flame Lash; cast 6.1s late, 102k and a death; not cast (spent on Auriaya),
  **416k in 20s** and five dead. Deliberately **not** named `army of the dead`: `IsBurstCooldownAction`
  matches on the name, and the Ulduar burst gate holds that list until Attuned to Nature drops, the whole
  add phase. 42650's 10 min cooldown outlasts the pull, so each DK casts it on the first lasher wave.

**Ranged focus** is `GetFreyaLasherPackFocus`, the lasher with the most lashers within
`ULDUAR_FREYA_LASHER_PACK_RADIUS` (8 yd): `AoeTrigger` counts attackers within 8 yd of the ***current
target***, not the bot, so pointing at the middle of a pile is what makes class AoE fire at all, and
`_MIN_COUNT` (3) is `MediumAoeTrigger`'s threshold. Below `_FINISH_PCT` (20%) with 3 up,
`FreyaLasherFinishAoeMultiplier` shuts non-healing AoE off inside `_PACK_CLEAR` (16 yd, one past
Detonate) and `GetFreyaRangedLasherFocus` (lowest health, GUID ties, self-stabilising) picks them off one
at a time so the blasts stagger. Both read `GetFreyaFinishingPackNear`, measured from the **pack's** centre.
That conjunction held **once in six pulls**: lashers scatter and die one at a time.

Melee and tanks take only what is inside `ULDUAR_FREYA_MELEE_LASHER_RANGE` (12 yd) and drop it the moment
it runs past, which is the leash: a lasher that retargets cannot tow a bot across the room. The add tank
works its normal ladder through a lasher wave.

## Movement guards

**A pinned bot's move is a silent no-op**
([pitfalls](../../engine/pitfalls.md#a-bot-mid-cast-cannot-be-moved-at-all)). Both escapes call
`FreyaClearCastBlockingMove` before `MoveTo`, so a channel no longer pins a bot inside the blast it was
told to leave, and both latches break on `IsMovementPreventedByCasting()`. All six walk-only nodes return
false on `UNIT_STATE_NOT_MOVE` (root, stun, died, distracted) before touching the motion master, or the
latch claims the tick for a walk that never starts and throws away the cast the bot could still make:
Iron Roots landing 102 ms into a bomb escape held one bot on that latch for 3.7s until it died at full
health inside four bombs. `freya break iron roots` outranks the escapes but wins only one tick, because
`Attack` reports false once the bot already swings at the root, so falling through is what leaves the
pinned bot its rotation and its root.

`freya avoid aoe hold` zeroes the generic `avoid aoe`
([pitfalls](../../engine/pitfalls.md#things-the-bot-cannot-see)), which at `ACTION_EMERGENCY` outranks
every Freya node and swaps a 15 yd escape for a 5 yd hop of its own; it took the tick back from a beam
dodge 1.9s before that bot died. Freya's nodes read every hazard at once, so it has nothing to add.
**Movement only**: nothing here zeroes `ReachTargetAction`.

## Hard mode

Hard mode = Elders left alive at pull (Brightleaf 32915 / Stonebark 32914 / Ironbranch 32913). Per living
Elder Freya gains Iron Roots (62862), Unstable Sun Beam (62450), or Ground Tremor (**62437** 10-man /
**62859** 25-man). **The empower events are scheduled once at pull and repeat unconditionally, even after
the Elder dies**, so hard-mode reactions key off the **hazard world object**, never off an Elder alive.
Killing Elders never removes the empower or costs the achievement: the count is locked at pull.

**Ground Tremor is a raid-wide interrupt, not a knockback.** Alongside its 7599 physical it carries
`Effect_2 = 68` `SPELL_EFFECT_INTERRUPT_CAST` (`EffectMechanic_2 = 26`) at radius index 28 = **50000 yd**,
so nothing dodges it, and `DurationIndex 1` makes `ProhibitSpellSchool` a **10s school lockout** on
whoever it cuts. Over 7 volleys, 31 of 37 casts in flight were `SPELL_PREVENTION_TYPE_SILENCE` and got
cut, and **71% of those never landed a same-school cast in the next 10s**; healers pay it hardest.
Hunters are exempt: Steady Shot is `PreventionType` PACIFY, which never reaches the lockout branch.

It is fully telegraphed: a **2000 ms** cast repeating `25s..35s`, a range no timer predicts. So the
handling is Ignis's Flame Jets trio: `FreyaGroundTremorCastGateMultiplier` caches the remaining window
per millisecond and blocks any candidate whose `CalcCastTime` would not land first (**328** vetoes in the
reference kill), and `freya ground tremor hold cast` (`ACTION_EMERGENCY + 2`) stops a cast already in
flight, reading `CURRENT_GENERIC_SPELL` **and** `CURRENT_CHANNELED_SPELL`: with the gate stopping ordinary
casts, a channel already running is all that is left, and reading one slot meant it never fired in 164
pushes (since: 31 times). `m_timer` carries what is left in either slot, so one compare serves both.
Unlike Ignis, **heals are gated too**: holding a Chain Heal under 2s beats losing Nature for 10, and
blocked bots fall through to their instants. Stopping first dodges the lockout outright:
`Spell::EffectInterruptCast` applies `ProhibitSpellSchool` **only if it finds a cast to cut**. Outside hard
mode the window is 0 and the gate inert.

**Iron Roots.** Root creatures 33088 / 33168 are selectable, so a trapped bot kills its own root, and the
beam stalkers 33170 / 33050 are not, so they come from `"nearest npcs"`
([pitfalls](../../engine/pitfalls.md#things-the-bot-cannot-see)). Breaking roots sits at `ACTION_RAID + 5`,
above the Sun Beam dodge at `+ 4`, because **a rooted bot cannot move**. **Its trigger needs both
difficulty ids of both roots, or it never fires**
([difficulty ids](../../engine/pitfalls.md#detection-that-never-fires)): `spelldifficulty_dbc` maps
62283 → 62930 and 62861 → **62438**, `HasAura` takes the exact spell, and a 25-man raid only applies
the 25-man half, so a check on
62283/62861 alone never ran once over 60 applications of 62438. The root lasts until its creature dies;
most break inside a second on incidental AoE, but 7 of one pull's held 2s or longer, two with the bot
inside a hazard. In the reference kill, **27** traps in 9 volleys of 3, most released under 1s, worst
**25.3s**; in a later kill nobody died under a latch while rooted. Every other Freya id
here already carries its pair.

**Unstable Sun Beam.** The dodge rings outward to a spot clear of every beam in range.
**`ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS = 12.0f` is 2.4× too wide**: Unstable Energy **62865** is
`EffectRadiusIndex 8` = **5 yd** (effect 27, persistent area aura), which matches the traces. The DBC check
was done and the constant never brought down, so this is an open gap, not an unknown. The dodge clears by
`ULDUAR_FREYA_SUN_BEAM_CLEARANCE` (15 yd), not the radius
([dodge clearance](../../engine/pitfalls.md#movement-that-silently-no-ops)): radius plus a yard gave a
median **1.9 yd** step over 694 orders, 95% under 12 yd, so none left the beam. Where overlapping beams
leave nothing that clear it takes radius + 1. It is latched (`dodgeSpot`, ceiling
`ULDUAR_FREYA_SUN_BEAM_LATCH_MS`), returning true without calling `MoveTo` while the walk is in flight
([oscillation](../../engine/raid-mechanics-lessons.md#movement-that-oscillates)).

## Per-tick scan

The per-tick lookups go through `FreyaScan`, a per-bot `ManualSetValue<FreyaScan*>` (`"freya scan"` in
`RaidUlduarValueContext`), as for Razorscale. Behind it: `Boss()` is `"find target" freya`, a threat-list
walk with a wide-string name per unit; `PossibleTargets()` a 100 yd grid sweep; `Stalkers()` that sweep
plus a LOS ray, tested on the entry first so only Healthy Spores and the two Sun Beam stalkers pay it;
`LasherCamp()` the camp spot. It is tick-scoped on purpose: a queued action can be
`Playerbots.ExpireActionTime` old when popped. `GetFreyaBossByEntry` finds her without the target list,
because the Ground Tremor nodes run wherever the gate is open, trash included.
