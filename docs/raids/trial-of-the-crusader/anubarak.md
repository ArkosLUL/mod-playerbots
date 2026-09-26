# Anub'arak

Raid-wide facts and the code layout: [README.md](README.md). Mechanics: `boss_anubarak_trial.cpp`;
tactics: Warcraft Tavern `anubarak-master-strategy-guide-toc-25/`.

## The room and the pull

- **Floor**: open ground about 60 yd round (745, 135), z 142.1-142.6, navprobe-clean at 20, 35 and
  50 yd from it. The corridor runs west to the **Web Door** (GO 195485, 661.6, 144.7), which closes
  at the engage: anyone west of it is locked out.
- **Landing**: the Lich King's floor break (stage 8 → 9) knocks every player up (68193) and drops
  the raid under the arena, where the mesh is a flat 137.3 over terrain at 125-135, likely water.
  navprobe models no liquid and paths from there to the door come back `INCOMPLETE`, so whether bots
  walk out is unverified until a trace.
- **Pre-pull**: he waits at (785.9, 133.4) submerged and `UNIT_FLAG_NON_ATTACKABLE` until a player
  comes within 80 yd, then stands and turns attackable; `detection_range` 20. `IsEncounterInProgress()`
  is already true from the Lich King scene, so only his combat marks the pull. Ten prey scarabs
  (non-attackable) roam round (722.65, 135.41) and die 2 s after the engage.

## Timeline

Engage: Berserk at 10 min. Freezing Slash on the victim at 7-15 s, then every 15-20 s (3 s stun).
Penetrating Cold at 15-20 s, then every 18 s. Burrowers at 5-8 s, then every 45 s. **Submerge 80 s
after the engage and after every emerge**; the guide's 105 s is wrong here.

**Phase 2**, 60 s: unselectable, stunned, clears his debuffs. The spike is summoned at him 2.5 s in.
From 3 s in, every 4 s, one scarab comes out of a random one of the four burrows (66339, 4 s aura
with one 4 s tick), so the last arrives about 4 s after he emerges. At 60 s he **teleports to the
spike** (66170) and despawns it; 2 s later he is selectable again, stunned 1 s.

**Phase 3**: the first surfaced `UpdateAI` below 30% health casts Leeching Swarm once (1.5 s) and
stops submerging. Burrowers stop in normal only; heroic keeps them coming.

**He never resets threat.** Nothing in the script touches his threat list, so he surfaces wherever the
spike was and walks to his top threat, normally the main tank. The random pick on emerge people
remember is the spike's, which re-rolls its own threat.

## Spells

| Spell | Ids 10N/25N/10H/25H | Facts |
|---|---|---|
| Freezing Slash | 66012 | a hit plus a 3 s stun on his victim |
| Penetrating Cold | 66013/67700/68509/68510 | 2/5/2/5 random players within 100 yd, 18 s, 3500 (normal) / 6000 (heroic) frost every 3 s; no splash, so no spread |
| Leeching Swarm | 66118/67630/68646/68647 | enemy area aura: 10/10/20/30% of current health a second, minimum 250, heals him by what lands |
| Permafrost | 66193/67855/67856/67857 | 6 yd, 15 min, slows 30/30/80/80% |
| Impale (spike) | 65919/67858/67859/67860 | 6 yd cone off the spike, physical, 14.1-15.9k normal, 17.7-19.8k heroic |
| Shadow Strike | 66134 | heroic burrower, 8 s cast, interruptible (`SpellInfoCorrections`), 40000 shadow |
| Submerge | 65981 | no row; stun, transform, untrackable |
| Pursued by Anub'arak | 67574 | no row; on the spike's target |
| Frost Sphere | 67539 | no row; on a flying sphere |

**Leeching Swarm** is an enemy area aura (effect 129) and `UnitAura::FillTargetMap` never applies one
to its owner: the players carry it, he never does.

## Frost Spheres and Permafrost

Six slots (AnubLocs 5-10, z 155.67) summoned at the engage, each drifting within 20 yd. A flying
sphere carries 67539. Damage that would kill it sets `UNIT_FLAG_NOT_SELECTABLE` and drops it to
z 143; 1.5 s later it loses 67539 and casts Permafrost. A dynobject aura skips its caster
(`Unit::_IsValidAttackTarget` refuses self), so **no sphere ever carries Permafrost**: a patch is an
alive sphere without 67539 (`IsPermafrostPatch`), a flying one has 67539 and is selectable
(`IsFrostSphereFlying`).

- **Normal respawns, heroic never.** Every 4 s, from a random slot on, the first slot whose sphere
  is still alive as a patch gets a new flying sphere; the old patch stays. A slot whose patch the
  spike uses before that tick reaches it stays empty for the pull, so normal runs down too, only
  slower. Heroic has six for the whole pull.
- **Permafrost hits everything hostile to faction 1925**: players, burrowers, scarabs. He is immune
  (`ApplySpellImmune`); the spike is not selectable, so the AoE searcher skips it. `AvoidAoeAction`
  never flees it: `area debuff` only reads periodic and dummy auras.
- Spheres run `NullCreatureAI` and stay out of combat, so out of `attackers`, until something hits
  one: only AoE or an explicit attack pops a sphere.

## Pursuing Spike (34660)

Named "Anub'arak", rank 3 and `CREATURE_TYPE_FLAG_BOSS_MOB` (so `isWorldBoss()`),
`UNIT_FLAG_NOT_SELECTABLE`, zeroes all damage. It resets its own threat and marks a random player
(67574). A reader keys the boss on entry 34564, never on name or boss flag.

- **Speed** 3.5 yd/s, 6.1 from 7 s, 11.4 from 14 s (`speed_run` 0.5 with +75%, then +225%). A
  player at 7 yd/s gains about 30 yd in the first 14 s, then loses it in 7.
- **Each tick** (1.5 / 0.8 / 0.4 s by speed stage) it tests for an alive sphere within 8 yd, 3D plus
  both sizes (spike 1.5, patch 2.0 at scale 2, falling sphere 1.0): **11.5 yd round a patch, 10.5
  round a falling sphere**. One found: it teleports onto it, `SPIKE_FAIL` strips the sphere's auras
  and despawns it 1.5 s later, the mark comes off, and after 4 s idle it marks a new random player at
  base speed. None: it casts Impale. So a patch is single-use, a falling sphere counts too, the test
  precedes Impale on the same tick, and any patch the spike merely passes is spent.
- **Submerging next to a patch spends it.** The spike is summoned on him and first tests 1.5 s
  later, about 5 yd on, so a patch within 16.75 yd of where he submerges goes before the chase. The
  guide drags him away from the centre 15 s before the submerge.
- **Safe stand**: with the patch on the line between the spike and its target, the fail fires 11.5
  yd out, before Impale's 6 + 1.5 yd can reach the target, whatever the target's distance from the
  patch. That is the guide's "sit outside the frost patch". Walking into the patch instead costs the
  80% heroic slow.
- A target that loses the mark, dies or breaks the chase is replaced at the current speed.

## Adds

- **Nerubian Burrower** (34607): 1/2/2/4 a wave from the four burrow points. Each attacks its
  nearest player and zones in. **From 30 s after spawn or emerge, checked every 3 s, one below 80%
  health without Permafrost submerges for 10 s and comes back at full health**, in every difficulty,
  not only heroic. Spider Frenzy hastes burrowers within 12 yd of each other; Expose Weakness stacks
  +25% physical damage taken on its victim (9 stacks, 10 s). Immune to silence, fear, root, polymorph
  and the like (`CreatureImmunitiesId` -207), not to interrupt or stun. Heroic Shadow Strike: first
  30-45 s after spawn, then every 30-45 s, on a random player; on hit it teleports behind the target
  and attacks it.
- **Swarm Scarab** (34605): adds 20000 threat on a random player, so neither threat nor redirect
  holds one. Acid-Drenched Mandibles stacks a nature DoT. Determination fires first at 10-50 s, then
  every 20-60 s: full heal, +100% speed, immune to most crowd control.

## How the bots play it

**Every node waits for the pull** (`AnubarakEngaged`, the encounter live). The stage gate opens at
the floor break, `IsEncounterInProgress()` is true from the Lich King scene, and he turns attackable
within 80 yd before anyone engages, so a stage-gated node has the main tank pull on sight.

**Resolve him through guid slot 13 and the spike by a grid search.** `GetFirstAliveUnitByEntry`
reads `possible targets`, which drop `UNIT_FLAG_NOT_SELECTABLE`, and both are unselectable in phase
2: through it `AnubarakSubmerged` never read true and the sphere node was dead on every difficulty.
NPC entries are the base entry on every difficulty. Spheres, spike, burrowers and scarabs come from
one grid sweep per instance per ms, taken from him, so a bot that never landed can't blank it.

**Phase**, per instance, memoised per ms: `Submerged` while he is unselectable or carries 65981;
`Swarm`, latched, once he is up and a group member carries the remapped Leeching Swarm, he casts it,
or he is below 30%; else `Surface`; `None` while he is not engaged. Every gated node waits for the
engage, so nothing reads the phase between pulls: a read 5 s after the last starts a new pull and
resets every latch. Heroic is the map's difficulty, as the script reads it.

**Side tanks are latched per pull**, each while it lives: the first two living tanks after the main
tank, assistants first as in `IsAssistTankOfIndex`. A tank is `IsTank` or tank by spec, the main
tank the explicit one, else the first living tank: `IsTank` can read false for a tick, and a re-read
index swaps the sides.

No ties, since a tie falls to insertion order:

| Relevance | Trigger → action | Who, when | Does |
|---|---|---|---|
| EMERGENCY+8 | `anubarak pursued by spike` → `anubarak kite spike to permafrost` | carries 67574 | walks to the kite stand, `MOVEMENT_FORCED` |
| EMERGENCY+6 | `anubarak spike nearby` → `anubarak avoid spike` | unmarked, within 10 yd of the spike or 7 yd of its line to the kiter | dodge spot 13 yd off the spike and 9 yd off its lane, latched until within 2 yd or 1 s without dodging |
| EMERGENCY+3 | `anubarak burrower casting shadow strike` → `anubarak interrupt shadow strike` | a caster's interrupter | interrupt or stun; out of range, walks in first |
| RAID+6 | `anubarak leeching swarm on tank` → `anubarak tank defensive` | his victim in `Swarm`, a defensive ready | `NextTankDefensive` on self |
| RAID+5 | `anubarak ranged should seed permafrost` → `anubarak destroy frost sphere` | a sphere duty | attacks the sphere |
| RAID+4 | `anubarak burrower should be focused` → `anubarak focus burrower` | DPS, a burrower on Permafrost or `cross` set | crosses the lowest-health one on Permafrost; with none, back to `skull`, clearing a stale cross |
| RAID+3 | `anubarak burrower needs assist tank` → `anubarak assist tank hold burrower` | a side tank with a burrower to take, not the pickup tank while he is up; or a leftover `square`/`triangle` | holds one on its side's patch; with none, rti back to `skull` |
| RAID+2 | `anubarak scarab on raid` → `anubarak tank pick up scarab` | the pickup tank while he is down, a side tank with no burrower to take; a scarab within 30 yd on a non-tank | taunt, attack |
| RAID+1 | `anubarak engaged by main tank` → `anubarak main tank hold boss` | the pickup tank while he is up | skull, leads him onto the anchor, or from 65 s onto the submerge spot |
| MEDIUM_HEAL+5 | `anubarak penetrating cold on raid` → `anubarak heal penetrating cold` | healer, a carrier below 90% | first direct heal `CanCastSpell` allows, on the lowest |

The **pickup tank** is the main tank while alive, else side 0's tank. `anubarak tank target guard`
zeroes `TankAssist` and `DpsAssist` for it while he is up and for a side tank while it has a
burrower to take, and `DpsAssist` for a sphere shooter, so generic targeting cannot pull any of them
off its charge: a flying sphere is out of combat, never the dps target, so assist would swap the
shooter off it every tick. A side tank with nothing to take (its burrowers submerged or on the other
side) falls through to the scarab node and tank assist. `anubarak control tank movement multiplier`
zeroes formation moves for a tank on him or a burrower.

**Every taunt** (boss, burrower, scarab) first checks `CanCastSpell` and the spell's range:
`CastSpell` selects and faces the target before failing, so a taunt on cooldown would do that every
tick.

**No landing or regroup node.** The floor break drops the raid where navprobe models nothing (see
[the pull](#the-room-and-the-pull)), and no trace yet shows whether bots walk out. `--pull` reports
anyone locked out or never landed.

**Emerge: the pickup tank taunts only off another non-tank, no redirect.** He keeps his threat and
teleports to the spike, so a taunt on every emerge only spends the cooldown, and a taunt hands back
to top threat after 3 s anyway ([thorim.md](../ulduar/thorim.md)).

**Tank patches and the anchor.** At the pull, the flying sphere nearest each point 12 yd either side
of the room centre, (745, 135 ± 12), is shot down. Each side latches the patch nearest its point
within 30 yd, never the other side's, re-latching once it is gone, and the main tank holds him at
their midpoint, else at the room centre. Guide: two spheres "one on Anub'arak's left and one on his
right", the boss "perfectly in-between two frost patches"; 12 yd is conservative and navprobe-clean.
The fallback is the room centre, not the scarab spawn point (722.65, 135.41): navprobe centres the
floor there, a 41 yd drag from his spawn against 63.

**The drag leads him onto its point**, within 2 yd measured on him: `DragBossToAnchor` stops the
tank within 12 yd, which from north or south leaves him on a tank patch. **From 65 s after the pull
or an emerge** (he submerges at 80 s) the point is the spot nearest the tank, within 30 yd of it and
35 of the room centre, 18.75 yd from every patch (11.5 + 5.25 + the 2 yd arrive): latched for the
window, redone once a patch lands too close, the anchor while there is no patch or no spot.

**Burrowers are held on Permafrost in every difficulty**, since the script's submerge rule is not
heroic-gated (guide: "bring burrowers to your designated frost patch"). A side tank attacks the
burrower on it, else the loose one nearest its patch, and taunts a loose one even while it holds
one, since 10H brings two a wave to one side tank and 25-man four to two. Loose means its victim
holds no burrower side, the boss tank included. It walks to its patch (none latched: its side
point) only once its pick is on it; an untaunted one is attacked in place, since walking off leaves
it on its victim and the patch walk and reach melee trade the tank back and forth. It skips the
walk while the patch lies within 9 yd of the spike's lane, where the dodge would walk it straight
back out. It marks its pick under its own rti, `square` side 0 and `triangle` side 1, with no group
icon, so DPS never follow it, and hands the rti back to `skull` once it has nothing to take:
`TankTargetValue` reads the rti first, so a leftover mark has it chase that icon next fight. **DPS
hit a burrower only while it carries Permafrost** (`cross`): off it, one below 80% submerges and
comes back full. A shooter on sphere duty skips it, or the two nodes swap its target every tick.

**Sphere duty.** Shooters are the alive unmarked ranged DPS bots in the pit (within 60 yd of the
room centre, below z 200) by guid, shooter *i* taking need *i*. A player never reads the assignment,
and a bot locked out or never landed can't reach a sphere: either would hold a need nobody else
gets. Needs in order: `kite`, while the kiter has no patch and no sphere is falling within 30 yd of
it, takes the flying sphere whose stand puts the kiter ahead of the spike, else the nearest; then
each side, while he is up, whose patch is missing with no sphere falling within 30 yd of its point,
takes the sphere nearest its point within 30 yd, since one further out lands where the latch never
takes it. **Heroic refills a side only while more than 2 unassigned spheres fly; a kite always gets
one**: six spheres, no respawn, and the guide's "use them wisely".

**The kiter stands 8.5 yd past a patch, away from the spike, not in it.** The fail test runs before
Impale on the same tick, at 11.5 yd against Impale's 7.5, which is the guide's "sit outside the
frost patch"; standing in it pays the 80% heroic slow. Each stand is floor-checked (moved at most 1.5
yd across and 3 yd in height, never into the 7.5 yd slow reach), retried ±20° and ±40° round its
patch, and kept only if the spike is farther from it than the bot; the nearest stand wins. A stand
is kept while its patch lives, the spike is still behind it and its bearing moves less than 30°;
after a `detour` only the patch is kept.

**A kite takes a tank patch only when nothing else serves**, in heroic (six spheres, no respawn) and
in any difficulty while a burrower lives, since a held burrower off Permafrost submerges back to
full, and a stand past a tank patch puts the side tank and its melee in the spike's lane. Heroic
first looks for a non-tank stand whose spike route (spike to kiter, kiter to stand) passes no tank
patch within 11.5 yd, then any non-tank one. Branches, as `anub.kite`:

- `hold` within 2 yd; returns false, so a healer or caster keeps working.
- `detour` when the straight walk cuts deeper into Permafrost's 7.5 yd reach plus 1 than either end
  already is: via the corners of a square round the patch, sides 9 yd out, the bot's side first. A
  single point straight out to the side won't do: the leg on from it still cuts to 6 yd of the patch.
- `ring` with no patch: 40° round a 35 yd circle about the room centre, the way that opens the angle
  to the spike, floor-checked (navprobe-clean at 35), while a shooter drops the sphere nearest the
  kiter. A 15 yd raw flee point risks leaving the mesh.

Kite and dodge break a channel that pins the feet (a pinned move is a no-op, and Impale does
14-20k), and clear the last-move booking after 500 ms standing still: `MoveTo` refuses a repeat
point for up to 5 s, so a potion or knockback would stall the bot. For a new point the kite also
drops a `FORCED` booking still in flight to `COMBAT`, restored if nothing went out: `MoveTo` refuses
a point at no higher priority, so a re-aimed stand, or the first leg after the bot's own dodge,
would walk the stale leg out first.

**The kite veto keeps attack actions.** `anubarak protect spike kite multiplier` zeroes reach-spell,
Blink and Disengage (spell movers, which a scarab on the kiter would fire in a direction the kite
never chose) and every movement action for the kiter but attack actions and the kite itself, and
zeroes both tank holds, which move. A blanket `MovementAction` veto also zeroes `AttackAction` and
kills targeting ([action-selection.md](../../engine/action-selection.md)).

**Non-kiters dodge the spike and its lane.** Inside 10 yd of the spike or 7 yd
of its line to the kiter, a bot takes the nearest spot 13 yd from the spike and 9 yd off the line,
within 30 yd, latched until within 2 yd of it or 1 s without dodging. Impale is a 6 yd cone, and
clearance has to sit past the trigger radius ([pitfalls.md](../../engine/pitfalls.md)).

**Scarabs get generic targeting; tanks taunt them off non-tanks.** Each puts 20000 threat on a random
player, so threat cannot hold one; the guide only asks raiders to mind their scarab aggro.

**Shadow Strike** gets one interrupter per casting burrower, derived once per instance per ms, since
25H's four a wave overlap their 8 s casts. Each goes to its victim if unmarked with an interrupt
ready, else to the lowest-guid alive unmarked group member with one ready within 30 yd; nobody holds
two casts. First castable of kick, pummel, shield bash, counterspell, wind shear, mind
freeze, hammer of justice, holy wrath (burrowers are undead on every difficulty), shockwave,
concussion blow, bash, war stomp, shadowfury: -207 blocks silence, not interrupts or stuns, so no
Silencing Shot or Spell Lock. The guide uses AoE stuns (Shockwave, Holy Wrath), warriors and
paladins. Out of reach, it walks to the spell's DBC range less 2 yd with line of sight, or to melee
for a melee or self-range one (War Stomp, Shockwave, Holy Wrath).

**Phase 3.** The main tank chains `NextTankDefensive`. Healers lead with Penetrating Cold carriers
below 90% in heal range and line of sight (guide: "1-3 healers assigned solely to" it). Lust
waits for `Swarm`, latched per instance (Anub'arak row of `ToCBurstWindowMultiplier`), since
phase 3 is where the raid takes its damage (guide); personal burst is unrestricted.

## Known gaps

- Raid AoE can pop spheres and nothing guards them, which spends heroic's six.
- 10H with one side tank puts both burrowers of a wave on one patch, inside Spider Frenzy's 12 yd.
- The direct-heal list is a copy of `JaraxxusHealIncinerateTargetAction`'s with "lesser healing
  wave" for "greater healing wave", not a 3.3.5 spell; one in `Shared` would keep them in step.
- The spike dodge ignores Permafrost, so a dodge can land in heroic's 80% slow.
- Sphere and Shadow Strike duty are memoised for one group per instance per ms; two groups in one
  instance recompute them on every switch. Side tanks are latched per instance.
- A side tank's `square`/`triangle` and a DPS's `cross` outlive a kill: stage 10 closes every
  Anub'arak node before the leftover clause can hand the rti back.
- Not handled: Hand of Protection or an immunity for a kiter with no patch, the phase 3 heal hold
  and stamina-buff cancel, Freezing Slash defensives, burrower pacing and ignoring burrowers once he
  is low, landing and regroup, scarab threat and Mandible stacks, Spider Frenzy spacing on 25H, the
  guide's phase 2 raid positioning.

## What a trace answers

`postmortem.py <file> --notes anub.` prints the rows:

| Key | Says |
|---|---|
| `phase` | 1 up, 2 submerged, 3 swarm; no row before the pull, the last value stands to the end. Per instance |
| `patches`, `flying` | Patches alive and flying spheres, as the helper's sweep saw them. Per instance |
| `patch0`, `patch1` | Each side's latched tank patch, 0 when none. Per instance |
| `sphere` | Sphere duty: `kite`, `0` or `1` (a side), `none` |
| `kite` | The kiter's branch: `patch` (walking to a stand), `detour`, `ring`, `hold`; `none` when there's no spike or the mark moved on |
| `dodge` | `spike` (within 10 yd of it), `lane` (on its line to the kiter), `none` (no clear spot) |
| `interrupter` | `1` while the bot holds Shadow Strike duty, `0` once the cast ends |
| `pickup` | The pickup tank's step: `taunt` (off a non-tank), `attack`, `hold` (onto the anchor), `submerge` (onto the submerge spot) |
| `defensive` | What the Swarm tank cast, `covered` (one running) or `none` |

The marked player is the 67574 `aura` stream, so no key repeats it.

The per-bot keys are change only: a bot that takes the same branch twice writes one row.

`tools/botobs/bosses/anub_arak.py` reads the rest, keying him on entry 34564 since the spike shares
his name and boss flag, and checking map 649 since Azjol-Nerub's boss shares the `anub-arak` slug.
`--phases` (spans, his health at each edge, deaths per phase), `--pull` (who stood west of the Web
Door or above z 200 at the pull and when each got in, stage 9 to the pull), `--spike` (each 67574
window: kiter, `kite` branches, seconds, outcome; Impale on anyone else; `dodge`), `--spheres`
(flying above z 150 and patches below 146 from the 34606 rows, Permafrost `hz`, patches gone within
3 s of a mark ending, heroic spheres left, `sphere` and casts at a sphere), `--burrowers` (life,
submerges, share of samples within 6.4 yd of a Permafrost centre, Shadow Strike casts and whether
66134 damage followed, `interrupter`), `--threat` (his target by role per
surfaced window, time from each emerge to a tank, taunts, distance from the drag anchor, `pickup`),
`--swarm` (phase 3 length, 66240 per bot, lust against the phase 3 start, Penetrating Cold windows
and heals onto the carriers, `defensive`, deaths). The banner names declared probes the pull never
wrote.

- **A `patch` outcome is an inference:** the mark came off with no Impale and no death. A patch seen
  going within 3 s confirms it; an immunity reads the same without one. `emerge` is a mark that
  ended within 3 s of the phase 2 edge with no patch seen going.
- **Creature auras are never recorded**, so a burrower's submerge is read as its health going back to
  full, after 5 s unsampled or as a 15-point jump, and its Permafrost as its distance from a
  Permafrost centre: 6 yd plus its own size, the core's 0.389 default since its display has none,
  not a player's 1.5.
