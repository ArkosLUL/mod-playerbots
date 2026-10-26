# Anub'arak

Raid-wide facts and the code layout: [README.md](README.md). Mechanics: `boss_anubarak_trial.cpp`;
tactics: Warcraft Tavern `anubarak-master-strategy-guide-toc-25/`.

## The room and the pull

- **Floor**: open ground about 60 yd round (745, 135), z 142.1-142.6, navprobe-clean at 20, 35 and
  50 yd from it, and at 30-40 yd with the bot filter (`--nav 0x09`). The corridor runs west to the
  **Web Door** (GO 195485, 661.6, 144.7), which closes at the engage: anyone west of it is locked out.
- **Landing**: the Lich King's floor break (stage 8 → 9) knocks every player up (68193) and drops
  the raid under the arena, where the mesh is a flat 137.3 over terrain at 125-135, likely water.
  navprobe models no liquid and paths from there to the door come back `INCOMPLETE`, yet in the
  2026-10-16 25N kill every bot stood in the pit at the pull.
- **Pre-pull**: he waits at (785.9, 133.4) submerged and `UNIT_FLAG_NON_ATTACKABLE` until a player
  comes within 80 yd, then stands and turns attackable; `detection_range` 20. His engage sets
  `IsEncounterInProgress()`, and his combat marks the pull. Ten prey scarabs
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
remember is the spike's, which re-rolls its own threat. Taunts on him diminish: `flags_extra`
0x80000 ([pitfalls.md](../../engine/pitfalls.md)).

**How he follows a tank** (`ChaseMovementGenerator`): combat reach 10, run 1.3 (9.1 yd/s against a
player's 7). He starts once his victim is 3D farther than both reaches plus `CONTACT_DISTANCE`, 12.0
yd from a player, and heads for a point about 2 yd past a moving victim, so a long walk lands him on
the tank. Once the tank stops he stops at his next 400 ms recheck, wherever he is: a tank stopping
just past 12 yd moves him 0-3.6 yd. In the 2026-10-16 kill he started at 11.6-12.2 yd and stopped
0.2-0.4 s after the tank.

## Spells

| Spell | Ids 10N/25N/10H/25H | Facts |
|---|---|---|
| Freezing Slash | 66012 | instant, physical and frost, a hit plus a 3 s stun on his victim; 0.8-7.2k on the 2026-10-16 25N tanks |
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
- **An untouched sphere is no valid target.** Spheres are critters (creature type 8), and
  `AttackersValue::IsPossibleTarget` refuses a critter out of combat, so `invalid target` → `drop
  target` clears one 0.2 s after `Attack()` and drops the bot to the non-combat engine. In the
  2026-10-16 kill sphere duty landed no shot; every patch came from Starfall (53190/53195) or from
  spells on spheres Starfall had already put in combat. Once in combat a sphere is anyone's target:
  mages' generic `polymorph` (relevance 30) sheeped spheres six times.

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
- A target that dies, goes fully immune (Divine Shield 642, Ice Block 45438), feigns death or
  stealths (Vanish) is replaced from the spike's spot at the current speed (`SelectNewTarget(true)`
  keeps the speed auras). Hand of Protection keeps the mark, a still target included (core
  `102c26d97`, 2026-10-05).
- **Immunities**: Impale is physical (school 1), so Hand of Protection (1022/5599/10278, 30 yd)
  voids it. Forbearance (25771) bars a second one within 2 min.
- **Cadence**: while patches lasted, the 2026-10-16 25N kites started 5.4-5.6 s apart, so one
  phase 2 asks about ten patches.

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
the floor break and he turns attackable within 80 yd before anyone engages, so a stage-gated node
has the main tank pull on sight.

**Resolve him through guid slot 11 and the spike by a grid search.** `GetFirstAliveUnitByEntry`
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
tank, bots before humans, assistants first as in `IsAssistTankOfIndex`, so a human takes only a side
no bot can (two bots and a human off-tank, or three bots, both work). A tank is `IsTank` or tank by
spec, the main tank the explicit one, else the first living tank: `IsTank` can read false for a tick,
and a re-read index swaps the sides. A side tank that dies is remembered for the battle res.

**A human side tank's side is the tank spot he stands at**, since no bot can tell him one: standing
within 8 yd of the other side's spot and 4 yd nearer it than his own, he swaps sides with the bot side
tank, at once when a side just took a new tank (and pre-pull, on the side points), else after 3 s
there, so walking past doesn't swap. Never between two humans.

**Waves**, per pull: a burrower first seen more than 10 s after the last opens a wave
(`anub.wave`), and its exit fixes its side, the north exits (y above the room centre) side 0. The
next is due 5 s after the pull or emerge, then 45 s after each; one that would fall after the 80 s
submerge comes 5 s after the emerge instead.

No ties, since a tie falls to insertion order:

| Relevance | Trigger → action | Who, when | Does |
|---|---|---|---|
| EMERGENCY+9 | `anubarak kiter needs immunity` → `anubarak kiter immunity` | a marked bot, no protector, the spike within 9 yd and faster than it, no patch stand it reaches first (heroic: no boost left) | paladin `hand of protection` while it has a patch to walk to or the emerge is 10 s off, else `divine shield`, the other as fallback; mage `ice block`; rogue `vanish`; hunter `feign death` |
| EMERGENCY+8 | `anubarak pursued by spike` → `anubarak kite spike to permafrost` | carries 67574 | heroic `boost` stage: the boost; else walks to the kite stand, `MOVEMENT_FORCED` |
| EMERGENCY+7 | `anubarak kiter needs protection` → `anubarak protect kiter` | the paladin holding the kiter's claim | `hand of protection` on the kiter |
| EMERGENCY+6 | `anubarak spike nearby` → `anubarak avoid spike` | unmarked, within 10 yd of the spike or 7 yd of its line to the kiter (16 and 10 while it outruns the bot) | walks to its held dodge spot, `MOVEMENT_FORCED` |
| EMERGENCY+5 | `anubarak on permafrost` → `anubarak leave permafrost` | no tank, unmarked, no dodge held, no grip duty but `kill`, within 7.5 yd of a patch | walks out ([non-tanks](#how-the-bots-play-it)) |
| EMERGENCY+3 | `anubarak burrower casting shadow strike` → `anubarak interrupt shadow strike` | a caster's interrupter | interrupt or stun; out of range, walks in first |
| RAID+7 | `anubarak side tank dead` → `anubarak battle res side tank` | the lowest-guid bot druid with Rebirth ready, a tank only when no other can, never the pickup tank while he is up; a dead side tank's corpse with no self-res pending | caster form, walks to 26 yd, Rebirth |
| RAID+6.5 | `anubarak side tank needs defensive` → `anubarak side tank defensive` | a side tank at Expose Weakness 5+ (67721 remapped: 25-man carries 67847) or under 50%, a physical defensive ready | `NextTankDefensive` on self, physical only: burrowers melee, so no Anti-Magic Shell (Felesta cast it 5× against them, 2026-10-26) |
| RAID+6 | `anubarak leeching swarm on tank` → `anubarak tank defensive` | his victim in `Swarm`, a defensive ready | `NextTankDefensive` on self |
| RAID+5.5 | `anubarak grip frost sphere` → `anubarak grip frost sphere` | a DK with a grip duty | `walk` to its stand, stops and `cast`s Death Grip on the duty sphere, `kill`s it once landed |
| RAID+5 | `anubarak ranged should seed permafrost` → `anubarak destroy frost sphere` | a sphere duty | attacks the sphere, out of reach walks in first |
| RAID+4 | `anubarak dps target` → `anubarak dps target` | DPS whose `GetAnubarakDpsTarget` pick isn't its current target or its pet's | attacks the pick, pet too |
| RAID+3.8 | `anubarak melee burrower stand` → `anubarak melee burrower stand` | a melee DPS bot on a burrower near a patch, out of reach or within 8 yd of a patch | walks to its stand |
| RAID+3 | `anubarak burrower needs assist tank` → `anubarak assist tank hold burrower` | a side tank with a burrower to take, not the pickup tank while he is up | picks up, holds it from its rim |
| RAID+2 | `anubarak scarab on raid` → `anubarak tank pick up scarab` | the pickup tank while he is down; a side tank with no burrower to take while he is up and no wave due within 8 s; a scarab within 30 yd on a non-tank | taunt, attack |
| RAID+1.5 | `anubarak side tank ready` → `anubarak side tank ready` | an unmarked side tank with nothing to take, not the pickup tank while he is up, not gathered | attacks a scarab on itself, walks to its rim (else spot) |
| RAID+1 | `anubarak engaged by main tank` → `anubarak main tank hold boss` | the pickup tank while he is up | steps out of Permafrost, else leads him in latched legs onto the anchor, or from 65 s onto the submerge spot |
| RAID+0.5 | `anubarak raid gathers` → `anubarak gather` | heroic phase 2, not the kiter or a side tank with a burrower to hold or take | walks to its gather slot, round any patch |
| RAID+0.45 | `anubarak healer stand` → `anubarak healer stand` | a healer bot while he is up, unmarked, no dodge held, off Permafrost, over 6 yd off its stand | walks there, round any patch |
| RAID+0.4 | `anubarak demonic circle due` → `anubarak demonic circle` | heroic, a still warlock bot, circle missing or under 100 s; on its staging stand before the pull, or while he is up | Demonic Circle: Summon |
| MEDIUM_HEAL+5 | `anubarak penetrating cold on raid` → `anubarak heal penetrating cold` | healer, a carrier below 90% | first direct heal `CanCastSpell` allows, on the lowest |

The **pickup tank** is the main tank while alive, else the side tank he is on (a human who taunted
him keeps him, no bot taunts him back), else the first bot side tank; while he is up its burrowers go
to the other side. Target guards zero `TankAssist`
and `DpsAssist` for it while he is up (`anubarak pickup tank target guard`) and for a side tank all
pull (`anubarak side tank target guard`): in the 2026-10-25 wipes a side tank was on him when a wave
landed. `DpsAssist` is zeroed for a sphere shooter and a gripping DK not just waiting on its spot
(`anubarak sphere shooter target guard`): a flying sphere is out of combat, never the dps target, so
assist would swap the shooter off it every tick. A side tank with nothing to take falls through to
the scarab node, else the ready node. `anubarak control tank movement multiplier` zeroes formation
moves for a tank on him or a burrower.

**Every taunt** (boss, burrower, scarab) first checks `CanCastSpell` and the spell's range:
`CastSpell` selects and faces the target before failing, so a taunt on cooldown would do that every
tick.

**No landing or regroup node**: in the 2026-10-16 kill every bot landed in the pit ([the
pull](#the-room-and-the-pull)). `--pull` reports anyone locked out or never landed.

**Emerge: the pickup tank taunts only off another non-tank, no redirect.** He keeps his threat and
teleports to the spike, so a taunt on every emerge only spends the cooldown, and a taunt hands back
to top threat after 3 s anyway ([thorim.md](../ulduar/thorim.md)).

**Tank patches and the anchor.** Each side has a **spot**, 12 yd either side of the room centre,
(745, 135 ± 12), 24 yd apart (navprobe-clean, bot filter, and so are 16 yd rings round both): burrowers
held 4 yd inside each patch toward him ([below](#how-the-bots-play-it)) stay 16 yd apart, past Spider
Frenzy's 12 (66129, radius index 32); slot 3's sphere starts 26 yd 3D from the north spot, inside
Death Grip's 28 (at ± 7 it sat 33-38 yd off, and the north patch came at 0:31 and 1:14 in the
2026-10-26 late wipes); and after a fail on one the spike's restart lane to the gather clears the other
by about 22 yd (13.6 at ± 7). Once one side has a patch, the other's spot is that patch moved 24 yd
toward its own side, so a patch off its spot drags the pair along. Each side latches the patch nearest
its spot within 30 yd and nearer its spot than the other's, never the other side's, re-latching once
it is gone. A gripped sphere's patch is the side's it was gripped for wherever it lands, within 30 yd
of the spot and 20 of the other side's patch: a
gripped sphere drifts back toward its route while it dies, and in 3 of 5 2026-10-26 wipes the north
grip's patch latched south. The anchor is the midpoint of the two spots. Guide: two spheres "one on
Anub'arak's left and one on his right", the boss "perfectly in-between two frost patches". **Until
one has landed he is held where the tank picks him up**: a room-centre fallback dragged him twice
before the patches landed at 9-12 s. With one down he goes to the anchor all the same: held where
picked up, he stood 14-31 yd off the burrowers in the 2026-10-26 wipes. The pre-pull pickup stand is 10 yd west of the
anchor (`GetAnubarakPickupStand`, (735, 135), navprobe-clean), off both spots; he runs in from the
east and stops on the anchor.

**Heroic refills follow the spheres left** (`anub.spot0`/`anub.spot1`), from once both sides have
held a patch: each sphere's home is its nearest spawn (AnubLocs 5-10) at its first sample, and heroic
never respawns one. After the first pair the four left hover 38-57 yd from the centre, the fixed spots
35-62 yd from them, and 2026-10-26 refills waited 25-38 s on drift. One side held: the open spot is on
the line from the held patch to the nearest home past it on the open side (north +y) and 20+ yd off,
20-30 yd out. Both open: the nearest north/south pair of homes 20+ apart, each pulled in until 30
apart (west pair 5.4 yd each). Spots are pulled in to 45 yd from the centre and floor-checked; no
candidate, the rule above. A falling sphere's home still counts and the spots rebuild after the
latches, so the spot it falls for holds until its patch latches. Likely spots, navprobe-clean: west
pair (707.1, 156.1) / (709.6, 126.2); east (789.6, 141.1) / (786.5, 117.5); north open off (745, 123)
(723.8, 144.3); south open off (745, 147) (721.0, 129.0). Normal and the first pair keep the fixed rule.

**The raid point** (R) is 18 yd off the anchor at right angles to the spot line, on the room centre's
side, west on a tie: (727, 135) for the first pair, 21.6 yd from both patches, 8 behind the pickup
stand. Rebuilt as the anchor moves while he is up; held from a submerge to the emerge, so a patch spent
while he is down doesn't move the gather. It is the healer stand and heroic's phase 2 gather, and the
submerge spot keeps off it ([below](#how-the-bots-play-it)). R for the refill cases above, navprobe-
clean with 3 and 6 yd rings 12/12: (726.3, 142.7), (770.2, 131.7), (747.2, 146.3), (743.8, 123.6).

**Healer stand**, every difficulty, while he is up: each healer bot (by guid) has a slot on a 3 yd
ring round R, turned until 8.5 yd clear of every patch, else R; over 6 yd off it walks back
(`anub.hstand` `walk`/`hold`). `anubarak healer stand hold` blocks formation and follow; reach spell
and reach heal stay. In the 2026-10-26 wipes healers stood 13-19 yd from the main tank and trailed the
human ([healer engine](../README.md)); side tanks went 15-17 s with no healer in 40 yd.

**A sphere within 12 yd of a side's spot is shot first**, no grip spent. At the pull one always hovers
over the south spot (slot 4, home (744.4, 119.5)); the other five drift within 20 yd of homes 22-62
yd from the spots, slot 3 (759.1, 164.0) nearest the north one. Gripping first, the north side took
the south sphere.

A side still open is **gripped** while it can be (`anub.grip` 1): a bot DK with Death Grip back within
3 s, the side's own tank if it is one, else the lowest guid holding no tank duty, else, before any
burrower is up, the other side's DK tank; never the pickup tank. It Death Grips (49576, 28 yd 3D) the
nearest flying sphere in reach of the spot, never one within 12 yd of the other side's spot, in
flight to another grip or picked by the other side's DK (two DKs once pulled the same sphere). With
none, it steps up to 16 yd toward the nearest within 44 yd, never nearer the other side's spot, just
far enough to put it 3 yd inside reach (at 1 yd drift broke the cast twice), floor-checked; it casts as soon as the sphere is in reach from anywhere in
that area, stopping first. The core lets the grip through (sphere rank 0, no dungeon-boss flag), and
the sphere comes down where the DK stood at the cast, which kills it there. The cast is recorded
against the side read before it, never by re-reading the duty: a recompute after it sees Death Grip on
cooldown and drops the DK, and a 2026-10-26 grip landed with nobody to kill it. **It has landed** once
within 4 yd above that spot and within 3 yd of it, or anywhere within 20 yd of the side's spot and 20
of the other side's patch; the side's shooters join then. Both 2026-10-26 grip failures were casts
mid-walk judged against the DK's live position: the spheres had landed 4.7-5.3 yd behind her.
**No side waits on a cooldown**: with no DK ready it shoots (a 40 s wait left sides bare to
0:40-0:43). **A grip not landed 3 s after the cast fails for its side only** (`anub.grip` 2 while
one waits): the side shoots and grips again once a DK's Death Grip is back. A pull-wide fail left
side 0 bare all of one wipe and side 1 bare 114 s of another. With no sphere in reach the DK keeps
its duty and the side's shooters take any they can; a 12 s idle hand-off to the shooters alone left
the north side bare 40 s. Refills come only while he is up: nobody leaves the gather. Until the kill,
`anubarak grip spot hold` blocks `reach melee`, set behind, flank, formation, follow and charges, so
the DK stays where the patch belongs; one waiting on its spot hits him from there. A DK owed a grip
keeps Death Grip out of its burrower pickup, and `anubarak grip reserve` blocks the DK rotation's
`death grip`, which spent it on burrowers at 0:09 and 0:56 in a 2026-10-26 wipe. `anubarak side tank
grip reserve` blocks it for a side tank all pull too: the pickup casts its grip directly, and the
rotation's yanked a burrower Felesta had just Dark Commanded (2026-10-26, 0:08.7). `anubarak side
tank righteous defense reserve` likewise blocks the class chain's `righteous defense`: it taunts
everything on its friend, and off the main tank it took him twice in a 2026-10-26 wipe, the side tank
dying to him at 3:32.

**Shooting** a tank patch otherwise takes a sphere within 12 yd of the spot at once, else after 10 s
the nearest within 20 yd, never one within 20 yd of the other side's patch, its gripped spot or its
shot sphere; the pick is latched while it qualifies. In the 2026-10-25 wipes the nearest sphere
swapped every tick, the two shooters split, and both shot spheres landed 31.5 yd out, past the latch.

**The main tank never stands in Permafrost.** Within 7.5 yd of a patch it first steps to the nearest
spot 8.5 yd clear of every patch, inside his 11.5 yd reach when one is (`stepout`): slowed 80%, a
2026-10-25 submerge leg crawled at 0.9 yd/s and he went down 0.7 yd from a tank patch. `anubarak pickup tank stand
guard` blocks tank face while a tank patch is latched.

**The drag is one latched leg, planned on a still boss** (`GetAnubarakDragLeg`). He starts only past
12 yd and stops at his next recheck wherever he is ([how he follows](#timeline)), so re-planning
every tick against a 2 yd arrive overshot and turned him back: 17 legs, 163 yd and 9 reversals in
phase 1 of the 2026-10-16 kill, 11, 113 and 8 after the emerge, and 216 `reach melee` ↔ `set behind`
flips. A leg is planned once he has stood still 500 ms, past that recheck, more than 8 yd from the
point (4 yd in the submerge window). The tank spot lies on his ray to the point, C + max(D − E, 0) / k
out, all read live: C both combat reaches plus `CONTACT_DISTANCE`, k his run speed over the tank's
backward speed (backing up keeps its front to him for parries and blocks),
E his speed × 0.2 s, D his distance to the point; floor-checked; no leg if that pulls it inside
C, or puts the tank's spot within 8.5 yd of a patch or its walk within 7.5. The line between the
patches is all Permafrost, so the hold tries the anchor, then points 4 and 7.5 yd off it toward his
side; a latched leg comes back whatever the point. A leg ends once he is within 4 yd of the point;
else it stands until he has followed and stood still 500 ms, or for 6 s, and is then judged like a
new one. A moved point (the submerge window opening, a patch re-latched) is judged only then, never mid-walk. The action returns true only on the tick it
issues the walk, so threat abilities get the ticks while he follows, and `anubarak boss drag hold`
blocks `reach melee`, set behind, tank face, formation, follow and charges while the leg is in
flight: any other walk replaces the leg's, and he stops wherever the tank stops.

**From 65 s after the pull or an emerge** (he submerges at 80 s) the point is the spot nearest the
tank, within 40 yd of it and 40 of the room centre (navprobe-clean to 45, 16/16), 30 yd from every
patch, else 25, 20.75 (11.5 + 5.25 + the 4 yd done radius), 16.75; within the nearest clear ring, the
one toward a point 40 yd past the tank, away from the tank patches. Every difficulty, first 25+ yd from
R with its line to R passing every patch by 14.5, so the spike starts away from the raid and its run
there spends none, about (712, 156) or (712, 114) for the first pair; dropped when nothing passes. At
the 2026-10-26 heroic submerges 10-13 players stood within 20 yd of where he went down, and 3 of 4
first kites started with the spike within 12 yd. Latched for the window, redone
once a patch lands closer than the clearance it was found with. With no patch alive or no spot clear,
the anchor rule applies. At 20.75 he went down 18.9-25.5 yd from the patches in the 2026-10-26 wipes,
the spike starting close enough to spend them.

**DPS bots hold off him near the tank's threat** (`anubarak threat cap`, Onyxia's hold): while he is
up and his victim is a tank, a DPS bot from 90% of that tank's threat until under 80% loses its
spells and melee on him and its area damage within 30 yd of him; heals pass. Threat is read off his
threat list, never the `threat` AI value, a uint8 share that wraps past 255%. No bot has a threat
brake ([pitfalls.md](../../engine/pitfalls.md)), and in both 2026-10-16 kills Angry pulled him and
died to one hit (5.6 s, the tank still walking, 32.6k; 13.7 s, 33.1k). White swings go on:
`Attack()` starts auto-attack itself, so the hold zeroes only actions.

**Taunt guard.** `anubarak taunt guard` blocks `Taunt`, Righteous Defense included, while the bot's
current target is him and his victim is another tank, a human included; the pickup tank's own taunt
off a non-tank is direct and unaffected. He obeys taunt DR, and in phase 1 of the 2026-10-16 kills
the bot main tank taunted him 7 and 15 times (2 and 6 Righteous Defense) through the paladin's
generic `lose aggro` taunt, trading him with the human tank.

**Burrowers are held on Permafrost in every difficulty**, since the script's submerge rule is not
heroic-gated (guide: "bring burrowers to your designated frost patch"). **Each belongs to a side**
(`anub.bside`), once per instance per ms: one on a living unmarked side tank stays with it, so a side
tank never taunts one off the other; the rest go by exit, at most two a side while both side tanks
are usable, and a dead or marked side tank's to the other (a marked one only exists in phase 2). A
human side tank's still loose 5 s after it came out goes to the bot side tank, past two: three on one
patch frenzy, but none eats a healer. `anubarak no ghouls` blocks `army of the dead` all fight: its
ghouls taunted burrowers off the side tanks and the patches, where one submerged to full. A side tank attacks the burrower on it, else its side's loose one, one hitting a healer first, then the
nearest its patch, and picks a loose one up even while holding one: Righteous Defense on whoever most
of them hit unless he is on them too, Challenging Roar or Shout with two within 10 yd and none of the other side's, the class taunt, then Feral Charge,
Intercept, Avenger's Shield or Death Grip, each gated on `CanCastSpell` and reach; else it walks in. In
the 2026-10-25 wipes burrowers were on a tank 18-41% of their life and two warlocks died to them in
every pull. It walks to its **hold** only once its pick is on it; an untaunted
one is attacked in place, since walking off leaves it on its victim and the patch walk and reach
melee trade the tank back and forth. It skips the walk while the hold lies within 9 yd of the
spike's lane, where the dodge would walk it straight back out. With nothing to take it stands on its
hold (`anub.sidetank` `ready`), hitting any scarab on itself, and in phase 2 the scarab node is the
main tank's only.

**The hold is the patch's rim toward him** (`GetAnubarakSideTankHold`): 7 yd from the centre toward
the anchor once every burrower on the tank carries Permafrost, the centre while one doesn't, so it
trails in; no patch latched, the spot. Burrowers stop about 3 yd short of their tank (median 2.97,
p95 3.49 over four 25H pulls), so from every exit they sit 3.5-5.7 yd in, on frost (6.39 yd reach),
with melee room outside the 7.5 yd slow. Held at the centre they sat on the exit side, the far side
from him, and melee crossed the patch: 216 and 462 s slowed in the 2026-10-26 late wipes. The rim
is not floor-checked (read every tick; it lies between patch and anchor). **Heroic phase 2:** a side
tank with no burrower of its side to hold or take gathers with the raid and walks back at the emerge,
since one standing on its patch walks a spike that marks it into the patch.

**DPS target directly, no raid icon** (`GetAnubarakDpsTarget`, DPS by `IsToCDpsBot`, only while
he is engaged), first match:

1. a player's skull on him, a burrower or a scarab, alive, in combat, selectable and attackable;
   never a sphere, which heroic has six of for the pull;
2. heroic phase 2, a gathered melee bot: the nearest scarab within 10 yd;
3. the **focus burrower** while it goes **ahead of him** (`anub.focusdue` 1): always while he is
   down or one is off Permafrost in its free time (below), else once every burrower alive, a submerged one at full health, would take 0.7 of the raid's
   damage rate (his and their health lost over the last 10 s, 3 s of samples at least), plus 15 s, to
   kill before the next wave; heroic counts the phase 2 time before that deadline at a quarter, since
   only the ranged reach the patches from the gather. Once due it stays due until the focus it came due
   on dies. Phase 3 too. 0.7 is 128k/s on burrowers against 174k/s on him (25H); phase 2 managed
   25-40k/s. In the 2026-10-25 wipes the old burrower-first order put 55-64% of DPS on them and he lost
   14-17% a phase; the unlatched rate check flipped off at 0:14 with the focus at 38-54%, and wave 2's
   focus never came due before the submerge, so it lived through phase 2 in the 2026-10-26 wipes;
4. him while selectable;
5. the focus burrower (`anub.focus`, per instance): selectable and under 10%; else **settled**, a
   tank within 5 yd has it, or its side has no usable side tank, and then on Permafrost or in its
   **free time**, the 25 s after it first shows or emerges, since the script's first submerge
   check comes at 30 s. Unsettled, it is still walking in from its exit: every wave's first pick in
   the 2026-10-26 late wipes came at the spawn, 51-61 yd from any tank, and the raid ran across the
   patches after it. Off Permafrost in its free time goes first, the one whose time ends first,
   then the lowest health, guid breaking a tie; the pick is kept while it qualifies and nothing in its
   free time outranks it. Off Permafrost one below 80% submerges on a 3 s check and comes back full,
   but one under 10% dies first (one was left at 0.1% in a 2026-10-26 wipe). Shooting only patched
   ones, side 0's wave 1 lived 39-45 s in a 2026-10-26 pull;
6. while he is down, the nearest scarab within 30 yd that is selectable, attackable and in combat.
   Each puts 20000 threat on a random player, so no tank gathers them; 30 yd is the tank pickup
   reach.

Wave 2 comes due about 10 s after it lands, so it dies mostly in phase 1. A **melee** bot takes the
focus only with a **stand**: a floor point within 4.5 yd of it and 8 from every patch centre, out from
its nearest patch, turned up to 100° toward the bot; else him, or a scarab while he is down. With a
burrower within 12.5 yd of a patch as its target, `anubarak melee burrower stand` walks it there,
round any patch it would cut, and `anubarak melee burrower stand hold` blocks `reach melee`, set
behind and flank (behind a rim burrower is its patch).

**Non-tanks leave Permafrost** (`anubarak leave permafrost`, `anub.leave`): within 7.5 yd of a patch
(players carry the slow to 7.47 yd, p99), no tank, unmarked, no grip duty but `kill`, no dodge held
(a dodge with no clear spot stays in the slow), a bot walks to the nearest floor point 8.5 yd clear of
every patch, in reach of its current target (melee range less 0.5, else `spell` range less 2) and off
the spike's danger and lane, else just off the spike, else anywhere clear. `AvoidAoeAction` never
moves anyone off Permafrost: a 2026-10-26 grip came down on 7 ranged and a healer, who stood in it
55-57 s. The gather walk detours round a patch its leg would cut, and a shooter's sphere stand is
never inside the slow.

**Burrower guard** (`anubarak burrower guard`, spells and attacks): while he is engaged, a non-tank's
action aimed at a burrower other than its own pick is zeroed, and a taunt or Death Grip on any
burrower always; an interrupt or stun on a casting one passes. Affliction multidots put Corruption,
Unstable Affliction and Curse of Agony on all four wave-1 burrowers at 8-15 s while the pick was him,
and Agony died to a loose one every 2026-10-26 pull.

A sphere shooter and a gripping DK not
waiting on its spot get no pick, or the sphere node (+5) and this one swap the target every tick.
`anubarak dps target guard` zeroes `DpsAssist` exactly while there is a pick, so a bot without one
keeps assist.

**Sphere duty.** Shooters are the alive unmarked ranged DPS bots in the pit (within 60 yd of the
room centre, below z 200), no warlock (its pet follows it onto the sphere), none walking a dodge
(held, over 2.5 yd off its spot); one parked on its dodge spot takes only a sphere in reach from
there and never walks for one. A player never reads the assignment, and a bot locked out or never
landed can't reach a sphere: either would hold a need nobody else gets. Needs, in order:

- each side, while he is up, whose patch is missing with no sphere falling for it: the grip, else
  shooting ([tank patches](#how-the-bots-play-it)), `0` or `1`, the DK's `grip0`/`grip1`. 10H shoots
  one per bot side tank. A lost tank patch comes ahead of any kite.
- `kite`, normal: while the kiter has no usable patch and no sphere is falling within 30 yd of it,
  the flying sphere whose stand puts the kiter ahead of the spike, else the nearest. Heroic has no
  kite need ([kite](#how-the-bots-play-it)): a kite patch prepared on the raid's side each submerge
  went unused by 9 kites in a 2026-10-26 wipe, 57-75 yd off or behind the spike, while both tank
  patches went.
- `stock`, normal phase 2 only, one at a time, while non-tank patches plus falling spheres number
  under 2: the flying sphere nearest the room centre, over 16.75 yd from the spike (from him before
  it spawns) and 9 yd off its line to the kiter, kept while it qualifies since spheres drift. Kites
  start every 5.5 s and phase 1's four patches were gone at 1:43 in the 2026-10-16 kill; normal
  refills a slot within 4 s, and the ring kite circles the centre. A phase 1 stock would crowd the
  submerge spot.

Each need gets up to two shooters, latched while its sphere and their eligibility hold; a vacancy
takes the eligible unassigned shooter nearest the sphere, the lower guid on a tie. One shooter per
need by guid spent 1:46-2:09 of the 2026-10-16 kill dodging, 55-72 yd off its sphere. A shooter
fires from where it stands once the sphere is in reach, 3D within its `spell` range less 2 yd and in
sight; else it walks toward the sphere's ground point until in reach (the sphere flies about 13.5 yd
up; in reach but out of sight, half way), floor-checked, never within 10 yd of the spike or across
its lane.

**Guards on spheres.** `anubarak sphere shooter drop guard` blocks `drop target` while a shooter's
current target is its flying duty sphere: [an untouched sphere is no valid
target](#frost-spheres-and-permafrost). `anubarak sphere guard` zeroes any spell or attack aimed at
a sphere but the bot's own duty sphere, and crowd control even on that one (`polymorph`, `hex`,
`repentance`, `freezing trap`, `shackle undead`, `fear`, `banish`, `hibernate`, `entangling roots`,
`cyclone`, `sap`, `blind`, `seduction`, also as a name's leading word): guide "Do not attack Frost
Spheres unless explicitly told to", and heroic has six. On the duty sphere it also zeroes any DoT,
hit of its own or not (Flame Shock's ticks downed two spheres late), a channel and an area spell: a
sphere that dies late lands wherever it drifted, and a splash downs a second one. Heroic zeroes `starfall` for every bot all pull: its stars found a
second sphere in every 2026-10-25 wipe, so the pull made three patches. Other area spells have no
target and stay unguarded.

**The kiter stands 8.5 yd past a patch, away from the spike, not in it.** The fail test runs before
Impale on the same tick, at 11.5 yd against Impale's 7.5, which is the guide's "sit outside the
frost patch"; standing in it pays the 80% heroic slow. Each stand is floor-checked (moved at most 1.5
yd across and 3 yd in height, never into the 7.5 yd slow reach), retried ±20° and ±40° round its
patch, and kept only if the spike is farther from it than the bot; the nearest stand wins. A stand
is kept while its patch lives, the spike is still behind it and its bearing moves less than 30°;
after a `detour` only the patch is kept.

**Normal takes a tank patch last** while a burrower lives, since a held burrower off Permafrost
submerges back to full. **Heroic spends one tank patch per submerge, as the last resort** (user
call): allowance (`anub.allow`, set as he submerges) 1 while a flying sphere is spare of the missing
tank patches to replace it after the emerge, else 0; spent (written 0) once a latched tank patch
disappears while he is down, whatever spent it. Six spheres, no respawn. Under it the kite may end on
a tank patch, a side holding no burrower first, or on a stray patch 16.75+ yd from every tank patch
(after a fail the spike restarts there, and its first test reaches 11.5 yd plus its 5.25 yd first
walk); route clear first: spike to kiter to stand passing no other tank patch within 14.5 yd.
Branches, as `anub.kite`:

- `hold` within 2 yd; returns false, so a healer or caster keeps working.
- `detour` when the straight walk cuts deeper into Permafrost's 7.5 yd reach plus 1 than either end
  already is: via the corners of a square round the patch, sides 9 yd out, the bot's side first. A
  single point straight out to the side won't do: the leg on from it still cuts to 6 yd of the patch.
- `ring` with no patch: 40° round a 40 yd circle about the room centre, the way that opens the angle
  to the spike, floor-checked (navprobe-clean at 40, 16/16), while normal's `kite` duty drops a sphere. A
  15 yd raw flee point risks leaving the mesh.

**Heroic kites run first** (`anub.kitestage`, guide: as long as possible without touching
Permafrost), the stage per instance per ms. The 2026-10-25 bot kites lasted 1-15 s, each ending on a
patch, 4-5 a phase 2:

- `run`: no patch search. A **lane step**: 16 headings, 10 yd out, floor-checked, within 40 yd of
  the room centre (or no further out than the kiter is), the spike behind and no nearer. A heading
  clears when no obstacle lies inside the triangle spike, kiter, **probe** (the spike chases, so its
  path stays inside it) and both the spike's leg and the kiter's to the probe pass it by its
  clearance, a leg starting inside that only having to head away. The probe is where the heading
  takes the kiter in 3 s (10 yd at least): two 2026-10-26 kiters with Nitro left looped the rim on
  10 yd steps that each cleared while the loop swept the spike across a tank patch. Obstacles: every patch at 14.5 yd, and in phase
  2 the gather point at 20 (3 yd slots, up to 6 off them, Impale's 7.5, 3.5 margin), since Impale is
  cast round the spike as it moves: at 12 the spike passed within 10 yd of six gathered bots. Ranked:
  clear of every patch; then the **restart line**, probe to the gather, passing every tank patch by
  14.5, since after a death or hand-off the spike runs from there to the raid (C 1:57 the line crossed
  the south patch 0.5 yd off); then clear of the gather; then, while the spike is faster and a patch it
  may end on lives, a step within 25 yd of its stand; then the widest lane past the patches, then past
  the gather, margins capped at 20 yd, then the step furthest from the spike. None clear of the
  patches: the widest patch margin alone, the gather left out: weighing the gather like a tank patch,
  the second kite of every 2026-10-26 pull walked the spike over the last tank patch. The last
  heading (within 15°) stays while it ranks with the best and is within 3 yd of its margins: kiters
  dithered ±1 yd at the rim for 2-3 s. None at all: the plain ring step. The ring step this replaced checked only the kiter's
  leg: all four early 2026-10-26 kites circled a tank patch at 15 yd while the spike cut straight
  across it. A warlock kept within 38 yd of its circle circled it and let a speed-2 spike cut the
  chord.
- `boost:<spell>` once the spike is within 15 yd and faster, or within 10 at any speed: Sprint, Dash in cat form, Nitro Boosts
  (none while one of those runs, one speed aura type), Blink 20 yd the way it faces when that lands
  farther from the spike and the spike's lane to it clears (no trail leg), then Demonic Circle: Teleport with the circle in reach and 15+ yd farther
  from the spike. Sprint's +70% outruns the spike's 11.4, so the chain moves on as each runs out. The
  kite casts them itself, so the kite veto never holds them.
- `immune`: Hand of Protection up, still running. The protector's claim and the kiter's own
  immunity wait while any boost is ready or running; the generic `feign death` is zeroed for a marked
  bot.
- `patch`, to the stand of the patch it may end on, T away at the kiter's speed, only while the spike
  is faster: under the hand once both its end and Impale's reach are within T + 1.5 s (turning at the
  hand's end spent a patch with the spike 17 yd back at 6.1 yd/s, 8 s of running left); else, no boost
  and no immunity coming (Forbearance, no paladin bot in 30 yd with Hand of Protection ready), once
  the spike would close to Impale's 7.5 yd within T + 1 s, or is within 9 yd. With none, 2 s of
  immunity left or 9 yd, onto lane steps. A slower spike keeps `immune`, then `run`. Turned for a
  patch, it stays turned until the mark moves.

**Heroic phase 2 gathers** everyone but the kiter and a side tank with a burrower to hold or take at
R, each on its slot 3 yd round it (`anub.gather`), walking once 6 yd off; the main tank's slot is 6 yd
off R away from where he went down, the spike's summon 2.5 s in, and it picks up scarabs from there.
The melee hit him to the submerge (user: the damage is vital) and walk once he is down. `anubarak gather hold` blocks `reach melee`, `reach
spell`, `reach party member to heal`, set behind, flank, formation, follow and charges meanwhile, so
melee take the scarabs that come to them and the ranged the burrowers on the patches; the dodge still
outranks it.

Kite and dodge break a channel that pins the feet (a pinned move is a no-op, and Impale does
14-20k), and clear the last-move booking after 500 ms standing still: `MoveTo` refuses a repeat
point for up to 5 s, so a potion or knockback would stall the bot. For a new point both also drop a
`FORCED` booking still in flight to `COMBAT`, restored if nothing went out: `MoveTo` refuses a point
at no higher priority, so a re-aimed stand, or the first leg after the bot's own dodge, would walk
the stale leg out first.

**The kite veto keeps attack actions.** `anubarak protect spike kite multiplier` zeroes reach-spell,
Blink and Disengage (spell movers, which a scarab on the kiter would fire in a direction the kite
never chose) and every movement action for the kiter but attack actions and the kite itself, and
zeroes both tank holds, which move. A blanket `MovementAction` veto also zeroes `AttackAction` and
kills targeting ([action-selection.md](../../engine/action-selection.md)).

**Non-kiters dodge the spike and its lane, and hold the spot.** Inside 10 yd of the spike or 7 yd
of its line to the kiter, a bot takes the nearest spot 13 yd from the spike and 9 yd off the line,
within 30 yd; while the spike outruns the bot, 16 and 10 yd, to 19 and 12 (10 yd is 0.9 s at 11.4
yd/s, and 2026-10-26 dodges started 0.3-1.3 s before the hit); heroic also keeps it 7.5 yd off every patch when it can, since the 80% slow still beats
the lane. Impale is a 6 yd cone, and clearance has to sit past the trigger radius
([pitfalls.md](../../engine/pitfalls.md)). The spot is held per bot: walked for up to 3 s, then
parked within 2.5 yd, while the spike lives, the mark stays as set (nobody between kites included),
the spot keeps the trigger distances off the spike and its lane, and the spike stays within 20 yd of it (or its
distance when set), for at most 8 s. A walk in flight is left alone, since every `MoveTo` clears the
spline. While held, `anubarak spike dodge hold` blocks `reach melee` and `reach spell` by name, so a
battle res walk still runs, plus set behind, flank, formation, follow, flee, run away, random moves,
charges, Blink and Disengage. Letting go on arrival handed bots back to reach: 130 `reach spell` and
107 `reach melee` flips in the 2026-10-16 kill, and Trueshot died while `reach spell` walked him
back.

**Hand of Protection covers a kiter the spike closes on.** Once the spike is within 12 yd of a kiter
and faster than it (a slowed kiter counts), with no Hand of Protection, Divine Shield, Ice Block or
Forbearance, the first alive unmarked paladin
bot by guid within 30 yd and in sight that `CanCastSpell` allows claims it; range and sight are
asked apart, since `CanCastSpell` waves an out-of-range target through. One claim per mark, kept
until the mark moves; one 1.5 s old with no immunity on the kiter passes to the next paladin, and
one lapses, for the same paladin to retake, when the spike falls back past 12 yd or slows. Two
2026-10-26 hands went out 0.2 s into a kite on a 3.5 yd/s spike. **Last resort**:
with no claim, no boost and no patch stand it reaches before the spike closes to Impale's
reach, a marked bot within 9 yd (Impale's 6 + 1.5 plus a margin) uses its own (table above). Only
Hand of Protection keeps the spike on it; the others, like a death, hand it on at its current speed,
so a paladin takes Divine Shield only with no patch to walk to and the emerge over 10 s off. Holylight's
Divine Shield 7 yd from the gather in a 2026-10-26 wipe handed an 11.4 yd/s spike into it: 12
Impales, 5 dead in 0.8 s. Impale is physical, and 5 of the 6 kites after the patches ran out took
Impales in the 2026-10-16 kill.

**Scarabs.** DPS take them only while he is down (branch 4 of the DPS target above); the guide only
asks raiders to mind their scarab aggro. Phase 2's are 600 s summons (66340) and outlive the emerge,
but once he is selectable the boss pick holds and `dps assist` stays zeroed, so leftovers fall to
`anubarak scarab on raid` alone: tanks taunt them off non-tanks.

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

- Untargeted raid AoE still pops spheres, all of it in normal (Starfall made every patch in the
  2026-10-16 kill), all but Starfall in heroic.
- A melee bot under the threat cap keeps white-swinging, so it can still climb past his tank.
- A human kiter or tank reads none of this: no protection claim waits on a human paladin, a human
  kiter gets no boost stage, and the drag waits while a human holds him.
- A human's Divine Shield, Ice Block, Feign Death or Vanish hands the spike on at its current speed,
  wherever the human stands.
- The second kite of a submerge has no patch: it lives on boosts and Hand of Protection, then hands
  the spike on at full speed.
- 10H with one side tank puts both burrowers of a wave on one patch, inside Spider Frenzy's 12 yd.
- A grip stepped out 16 yd can leave the patches up to 40 yd apart; past about 36 a melee stand
  between him and a rim burrower no longer reaches both.
- After a fail on one tank patch, a spike re-marking the other side's tank, still holding burrowers
  on its patch, spends that one too (about 1 in 24 per spent patch).
- A spent tank patch stays bare until he emerges, then waits on a sphere in reach.
- The Demonic Circle goes down where the warlock stands, its staging stand only before the pull.
- A gathered ranged bot whose burrower is out of spell range idles: the gather blocks `reach spell`.
- The direct-heal list is a copy of `JaraxxusHealIncinerateTargetAction`'s with "lesser healing
  wave" for "greater healing wave", not a 3.3.5 spell; one in `Shared` would keep them in step.
- A heroic dodge with no spot clear of Permafrost lands in its 80% slow.
- Sphere and Shadow Strike duty are memoised for one group per instance per ms, kiter protection per
  instance per ms; two groups in one instance recompute the duties on every switch and share the
  first reader's protection claim. Side tanks are latched per instance.
- While he is down, a DPS bot with no scarab within 30 yd falls back to `dps assist`, which can take
  a burrower off Permafrost.
- Scarabs alive at the emerge are no DPS pick once he surfaces; only side tanks with nothing to take
  peel them off non-tanks.
- Side tanks die to two frenzied burrowers at 9 Expose Weakness stacks (+225%, 5+ stacks 17-34 s of
  each 2026-10-26 wave), and most burrower submerges followed a side tank's death; no healer is
  assigned to a side tank.
- The kite ring (40 yd round the centre) runs past the west refill pair at 36-43 yd.
- A human's Army of the Dead ghouls still taunt burrowers off the patches (one submerged at 0:39 in a
  2026-10-26 wipe); only bots are blocked.
- A DPS bot can still pull a freshly taunted burrower off its side tank on plain threat (two did
  within 3 s in a 2026-10-26 pull).
- Scarab Mandible stacks on the main tank in phase 2 go unanswered beyond the nature aura (8.8k to
  24.9k ticks, dead at 4:31 in a 2026-10-26 wipe).
- Not handled: the phase 3 heal hold and stamina-buff cancel, Freezing Slash defensives, ignoring
  burrowers once he is low, scarab threat, normal's phase 2 raid positioning.

## What a trace answers

`postmortem.py <file> --notes anub.` prints the rows:

| Key | Says |
|---|---|
| `phase` | 1 up, 2 submerged, 3 swarm; no row before the pull, the last value stands to the end. Per instance |
| `patches`, `flying` | Patches alive and flying spheres, as the helper's sweep saw them. Per instance |
| `patch0`, `patch1` | Each side's latched tank patch, 0 when none. Per instance |
| `side0`, `side1` | Each side's tank, human included, 0 when none. Per instance |
| `wave` | Burrower waves seen this pull. Per instance |
| `bside` | A burrower's side, keyed on the burrower |
| `focusdue` | 1 while the focus burrower goes ahead of him, held until the focus it came due on dies. Per instance |
| `grip` | 1 a grip went out this pull, 2 while a side waits on a failed one. Per instance |
| `spot0`, `spot1` | Each side's tank spot, `x,y,z`: its latched patch, else the open spot. Per instance |
| `allow` | Heroic tank patches the submerge's kites may spend, 0 or 1, written as he submerges, 0 once one goes while he is down. Per instance |
| `sphere` | Sphere duty: `kite` and `stock` (normal), `0` or `1` (a side), `grip0`/`grip1` (a DK's grip for a side), `none` |
| `gripstep` | The DK's grip: `walk`, `hold`, `cast`, `wait` (sphere in the air), `kill`, `none` |
| `sidetank` | `hold`, `pickup`, `ready`, `gather` (heroic phase 2, nothing to hold) |
| `leave` | `walk` while a non-tank steps out of Permafrost, `none` |
| `mstand` | `walk` while a melee bot walks to its burrower stand, `none` |
| `kitestage` | Heroic kiter: `run`, `boost:<spell>` (Nitro Boosts as 55004), `immune`, `patch`, `none` |
| `gather` | Heroic phase 2: `walk`, `hold` |
| `hstand` | A healer bot's stand while he is up: `walk`, `hold`, `none` |
| `kite` | The kiter's branch: `patch` (walking to a stand), `detour`, `ring`, `hold`; `none` when there's no spike or the mark moved on |
| `dodge` | A new dodge spot's reason: `spike` (within 10 yd of it), `lane` (on its line to the kiter), `none` (no clear spot) |
| `protect` | `hop` while this paladin holds the kiter's claim, `self` while the marked bot needs its own immunity, `none` |
| `interrupter` | `1` while the bot holds Shadow Strike duty, `0` once the cast ends |
| `pickup` | The pickup tank's step: `taunt` (off a non-tank), `attack`, `stepout` (out of Permafrost), `hold` (no leg), `drag` (a leg to the anchor), `submerge` (a leg to the submerge spot) |
| `defensive` | What a side tank or the Swarm tank cast, `covered` (one running) or `none` |
| `focus` | The focus burrower, 0 when none; kept current under a skull. Per instance |
| `dps` | A DPS bot's pick: `skull`, `burrower`, `boss`, `scarab`, `none` (a sphere shooter too) |

The marked player is the 67574 `aura` stream, so no key repeats it.

The per-bot keys are change only: a bot that takes the same branch twice writes one row.

`tools/botobs/bosses/anub_arak.py` reads the rest, keying him on entry 34564 since the spike shares
his name and boss flag, and checking map 649 since Azjol-Nerub's boss shares the `anub-arak` slug.
`--phases` (spans, his health at each edge, deaths per phase), `--pull` (who stood west of the Web
Door or above z 200 at the pull and when each got in, stage 9 to the pull), `--spike` (each 67574
window: kiter, `kite` branches, seconds, outcome, when the spike first came within 12 yd, Hand of
Protection on the kiter, its own Divine Shield, Ice Block, Feign Death or Vanish, `protect` rows; Impale on anyone else;
`dodge`, `protect`), `--spheres` (flying above z 150 and patches below 146 from the 34606 rows, a low
one with no Permafrost `hz` centre on it falling; patches gone within 3 s of a mark ending, heroic
spheres left, `sphere` and casts at a sphere; per mark, tank and other patches, flying and falling
spheres and the `sphere` holders in the window; per duty span, the holder's casts at any sphere,
since the note names the duty and not the sphere, and its `drop target` rows), `--burrowers` (life,
submerges, share of samples within 6.4 yd of a Permafrost centre, Shadow Strike casts and whether
66134 damage followed, `interrupter`; each burrower's time holding `focus` and the share of living
DPS bot samples on it meanwhile; `dps`), `--threat` (per surfaced window: his target by role, time
from each emerge to a tank; his legs, runs of samples flagged moving of 2 yd or more start to stop,
their yards and reversals past 120°; his median distance from the drag anchor over samples with both
tank patches latched, before the window's first `submerge` row; taunts on him, Righteous Defense
counted when its friend was his victim, split bot and human; `pickup` and `anubarak threat cap` veto
rows per bot), `--swarm` (phase 3 length, 66240 per bot, lust against the phase 3 start, Penetrating
Cold windows and heals onto the carriers, `defensive`, deaths). Also: `--burrowers` each burrower's
exit, side at the spawn then each owner after (a dead side tank moves them all, so the last says
little), seconds to the first tank and to its owning side's tank, the spawn side tank's distance from
its spot, per wave the focus start against the next wave, DPS bot casts at a burrower other than the
held `focus` (by caster and spell; triggered ones, humans and casts during the target's Shadow Strike
left out) and each `focusdue` drop with the focus still sampled; `--spheres` what downed each sphere (hit
rows on spheres, logged from the pull, else the last cast at it; pet to owner, DoT or area flagged),
where a tank patch landed against its spot, each grip, whether it landed (from the DK's spot at the
cast) and the side that latched it against the side it served, each side's first patch (grip or
shots, killer, off-spot distance), and per submerge the tank patches spent against `allow` (`over`
past it, heroic): side, when it went (its sphere's last sample), the mark that ended within 3 s of it
or ran then, seconds in, its last `kitestage`, and when the side's patch came back after the emerge;
`--frost` seconds carrying Permafrost (the 67857 `aura` stream) per group (main, side and other tanks,
melee, ranged, heal; humans left out) and bot, he up and down, every stint over 3 s with its target
at the start, `leave` counts; `--spike` seconds per
`kitestage`, the boosts and immunities cast, an own immunity with no `protect` `self` in the 2 s
before flagged `unasked`, the spike's distance from the gather (the living bots holding `gather`
since he went under, the kiter left out) at each mark's end and the kite patch alive, an end by death
or own immunity within 20 yd of it flagged, the spike's closest approach to each latched tank
patch, under 11.5 yd flagged (a spent patch's last read is about 11), a window that spent a tank patch,
and a kiter marked within 7.5 yd of a Permafrost centre; `--burrowers` per burrower when
it came up and whether it died inside 30 s, counted per wave, per `focus` pick its seconds since it
came up and distance to the nearest side tank and patch (flagged `before its tank` unless a tank within
5 yd has it or it is under 10%), per burrower held on a side tank its median distance from that
side's patch centre, `sidetank` `gather` and `mstand` rows; `--threat` the main tank's share of samples inside 7.5 yd of a patch
and his distance to each tank patch at every submerge, under 25 yd flagged. More: `--spike` the
spike's and kiter's speeds at each immunity, one on a spike no faster flagged (speeds are medians of
moving 250 ms snapshot steps, 0.2 yd/s slack), seconds stalled per window (under 1.5 yd in 1 s), a tank
patch spent in `run` or `boost` flagged, per submerge the living players within 8/12/20 yd of where he
went down by role and the first kiter's distance from the spike; `--spheres` each grip's
`gripstep` reaching `kill` or dropping to `none`, a gripped sphere back above z 150, `spot0`/`spot1`
changes (off-spot read from them once written); `--burrowers` per submerge where it went under, the
nearest patch, patches alive, its side tank alive and how far, its target (pets and ghouls as
"Name (Owner)") and why it could go under; `--healers` per healer and phase the median distance to
the main and each side tank, seconds over 35 yd from a side tank a burrower is on, `follow` rows,
each engine's pass count (no row names the engine per tick, so a total), `raid healer engine` in
the coverage, `hstand` rows, per side tank the seconds with no healer within 40 yd; `--tanks` per
wave and side tank the Expose Weakness peak and seconds at 5+, each `defensive` with its stack and
health (a magic-only one on a side tank flagged), each tank death with damage by source, heals and
absorbs in the 12 s before. Baseline, the 2026-10-26 25H wipes C, D, E: Hand of Protection on a
slower spike C 1:22, E 1:22 and 1:47; patches spent in `run` C 1:57, 4:13, D 2:03, E 1:44; 10-13
players within 20 yd at each first submerge; `raid healer engine` absent, healers 13-19 yd from the
main tank with 7-45% of passes on the non-combat engine; Anti-Magic Shell on a side tank 5 times. The banner names declared probes the pull never wrote, and each side's
tanks in order when short of 2 (25-man) or 1 (10-man) or a human holds one, and a side with a tank
but no tank patch for 15+ s while he was up with burrowers out.

- **A `patch` outcome is an inference:** the mark came off with no Impale and no death. A patch seen
  going within 3 s confirms it; an immunity reads the same without one. `emerge` is a mark that
  ended within 3 s of the phase 2 edge with no patch seen going.
- **A leg starts on his first sample flagged moving**, so it misses up to one snapshot of walk.
- **Creature auras are never recorded**, so a burrower's submerge is read as its health going back to
  full, after 5 s unsampled or as a 15-point jump, and its Permafrost as its distance from a
  Permafrost centre: 6 yd plus its own size, the core's 0.389 default since its display has none,
  not a player's 1.5.
