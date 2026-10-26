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
- A target that loses the mark, dies or breaks the chase is replaced at the current speed
  (`SelectNewTarget(true)` keeps the speed auras). **Standing still breaks it**: the spike's chase
  spline finishes on a still target, `UNIT_STATE_CHASE_MOVE` clears, and it moves on. So an immune
  kiter has to keep moving, and the guide's "stand still under Hand of Protection" hands a fast
  spike to someone else here; Ice Block, which roots the mage, does the same.
- **Immunities**: Impale is physical (school 1), so Hand of Protection (1022/5599/10278, 30 yd)
  voids it, as do Divine Shield (642) and Ice Block (45438). Forbearance (25771) bars a second one
  within 2 min.
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
| EMERGENCY+9 | `anubarak kiter needs immunity` → `anubarak kiter immunity` | a marked paladin bot, no protector, the spike within 9 yd (heroic: no boost left) | first of `divine shield`, `hand of protection` it can cast on itself |
| EMERGENCY+8 | `anubarak pursued by spike` → `anubarak kite spike to permafrost` | carries 67574 | heroic `boost` stage: the boost; else walks to the kite stand, `MOVEMENT_FORCED` |
| EMERGENCY+7 | `anubarak kiter needs protection` → `anubarak protect kiter` | the paladin holding the kiter's claim | `hand of protection` on the kiter |
| EMERGENCY+6 | `anubarak spike nearby` → `anubarak avoid spike` | unmarked, within 10 yd of the spike or 7 yd of its line to the kiter | walks to its held dodge spot, `MOVEMENT_FORCED` |
| EMERGENCY+3 | `anubarak burrower casting shadow strike` → `anubarak interrupt shadow strike` | a caster's interrupter | interrupt or stun; out of range, walks in first |
| RAID+7 | `anubarak side tank dead` → `anubarak battle res side tank` | the lowest-guid bot druid with Rebirth ready, a dead side tank's corpse with no self-res pending | caster form, walks to 26 yd, Rebirth |
| RAID+6.5 | `anubarak side tank needs defensive` → `anubarak tank defensive` | a side tank at Expose Weakness (67721) 5+ or under 50%, a defensive ready | `NextTankDefensive` on self |
| RAID+6 | `anubarak leeching swarm on tank` → `anubarak tank defensive` | his victim in `Swarm`, a defensive ready | `NextTankDefensive` on self |
| RAID+5.5 | `anubarak grip frost sphere` → `anubarak grip frost sphere` | a DK with a grip duty | `walk` to its stand, `cast` Death Grip on the duty sphere, `kill` it once landed |
| RAID+5 | `anubarak ranged should seed permafrost` → `anubarak destroy frost sphere` | a sphere duty | attacks the sphere, out of reach walks in first |
| RAID+4 | `anubarak dps target` → `anubarak dps target` | DPS whose `GetAnubarakDpsTarget` pick isn't its current target or its pet's | attacks the pick, pet too |
| RAID+3 | `anubarak burrower needs assist tank` → `anubarak assist tank hold burrower` | a side tank with a burrower to take, not the pickup tank while he is up | picks up, holds one on its side's patch |
| RAID+2 | `anubarak scarab on raid` → `anubarak tank pick up scarab` | the pickup tank while he is down; a side tank with no burrower to take while he is up and no wave due within 8 s; a scarab within 30 yd on a non-tank | taunt, attack |
| RAID+1.5 | `anubarak side tank ready` → `anubarak side tank ready` | an unmarked side tank with nothing to take, not the pickup tank while he is up | attacks a scarab on itself, walks to its patch (else spot) |
| RAID+1 | `anubarak engaged by main tank` → `anubarak main tank hold boss` | the pickup tank while he is up | steps out of Permafrost, else leads him in latched legs onto the anchor, or from 70 s onto the submerge spot |
| RAID+0.5 | `anubarak raid gathers` → `anubarak gather` | heroic phase 2, not the kiter, a side tank or the main tank | walks to its gather slot |
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

**Tank patches and the anchor.** Each side has a **spot**, 7 yd either side of the room centre,
(745, 135 ± 7), navprobe-clean: 14 yd apart, past Spider Frenzy's 12. Once one side has a patch, the
other's spot is that patch moved 14 yd toward its own side, so a patch off its spot drags the pair
along. Each side latches the patch nearest its spot within 30 yd and nearer its spot than the
other's, never the other side's, re-latching once it is gone. A gripped sphere's patch is the side's
it was gripped for wherever it lands, within 30 yd of the spot and 13 of the other side's patch: a
gripped sphere drifts back toward its route while it dies, and in 3 of 5 2026-10-26 wipes the north
grip's patch latched south. The anchor is the midpoint of the two spots. Guide: two spheres "one on
Anub'arak's left and one on his right", the boss "perfectly in-between two frost patches". **Until
one has landed he is held where the tank picks him up**: a room-centre fallback dragged him twice
before the patches landed at 9-12 s. With one down he goes to the anchor all the same: held where
picked up, he stood 14-31 yd off the burrowers in the 2026-10-26 wipes. The pre-pull pickup stand is 10 yd west of the
anchor (`GetAnubarakPickupStand`, (735, 135), navprobe-clean), off both spots; he runs in from the
east and stops on the anchor.

**A sphere within 12 yd of a side's spot is shot first**, no grip spent. At the pull one always hovers
over the south spot (~(745, 120), 8-9 yd off); the other five orbit 25-70 yd out, only the north one
coming in grip reach of the north spot, 15-30 s at a time. Gripping first, the north side took the
south sphere, nearest to it at 22 yd.

A side still open is **gripped** while it can be (`anub.grip` 1): a bot DK with Death Grip back within
3 s, the side's own tank if it is one, else the lowest guid holding no tank duty, else, before any
burrower is up, the other side's DK tank; never the pickup tank. It Death Grips (49576, 28 yd 3D) the
nearest flying sphere in reach of the spot, never one within 12 yd of the other side's spot, in
flight to another grip or picked by the other side's DK (two DKs once pulled the same sphere). With
none, it steps up to 8 yd toward the nearest within 36 yd, never nearer the other side's spot, only
as far as reach needs, floor-checked; it casts as soon as the sphere is in reach from anywhere in
that area. The core lets the grip through (sphere rank 0, no dungeon-boss flag), and the sphere lands
on the DK, which kills it there. The side's shooters join only once it is within 3 yd (and 4 yd up)
of the DK. **No side waits on a cooldown**: with no DK ready it shoots (a 40 s wait left sides bare to
0:40-0:43). **A grip not landed 3 s after the cast switches the pull to shooting** (`anub.grip` 2); a
ready DK on its spot with no sphere in reach for 12 s hands that need to the shooters. Until the kill,
`anubarak grip spot hold` blocks `reach melee`, set behind, flank, formation, follow and charges, so
the DK stays where the patch belongs; one waiting on its spot hits him from there. A DK owed a grip
keeps Death Grip out of its burrower pickup, and `anubarak grip reserve` blocks the DK rotation's
`death grip`, which spent it on burrowers at 0:09 and 0:56 in a 2026-10-26 wipe.

**Shooting** a tank patch otherwise takes a sphere within 12 yd of the spot at once, else after 10 s
the nearest within 20 yd, never one within 13 yd of the other side's patch, its gripped spot or its
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

**From 70 s after the pull or an emerge** (he submerges at 80 s; guide: 5-10 s before) the point is
the spot nearest the tank, within 30 yd of it and 35 of the room centre, 20.75 yd from every patch
(11.5 + 5.25 + the 4 yd done radius), else 16.75; within the nearest clear ring, the one toward a
point 30 yd past the tank, away from the tank patches. Latched for the window, redone once a patch
lands closer than the clearance it was found with. With no patch alive or no spot clear, the anchor
rule applies. At 65 s in the 2026-10-16 kill 42.6% of the disc round the tank cleared 20.75; he
submerged 16.2 yd from a patch.

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
of them hit, Challenging Roar or Shout with two within 10 yd and none of the other side's, the class taunt, then Feral Charge,
Intercept, Avenger's Shield or Death Grip, each gated on `CanCastSpell` and reach; else it walks in. In
the 2026-10-25 wipes burrowers were on a tank 18-41% of their life and two warlocks died to them in
every pull. It walks to its patch (none latched: its spot) only once its pick is on it; an untaunted
one is attacked in place, since walking off leaves it on its victim and the patch walk and reach
melee trade the tank back and forth. It skips the walk while the patch lies within 9 yd of the
spike's lane, where the dodge would walk it straight back out. With nothing to take it stands on its
patch (`anub.sidetank` `ready`), hitting any scarab on itself, and in phase 2 the scarab node is the
main tank's only.

**DPS target directly, no raid icon** (`GetAnubarakDpsTarget`, DPS by `IsToCDpsBot`, only while
he is engaged), first match:

1. a player's skull on him, a burrower or a scarab, alive, in combat, selectable and attackable;
   never a sphere, which heroic has six of for the pull;
2. heroic phase 2, a gathered melee bot: the nearest scarab within 10 yd;
3. the **focus burrower** while it goes **ahead of him** (`anub.focusdue` 1): always while he is
   down, else once every burrower alive, a submerged one at full health, would take the raid's
   damage rate (his and their health lost over the last 10 s, 3 s of samples at least), plus 15 s, to
   kill before the next wave. Phase 3 too. In the 2026-10-25 wipes the old burrower-first order put
   55-64% of DPS on them and he lost 14-17% a phase; counting only those on Permafrost, plus 5 s, he
   lost 28-34% in the 2026-10-26 wipes but wave 1 outlived wave 2's spawn in every one;
4. him while selectable;
5. the focus burrower (`anub.focus`, per instance), kept while alive, selectable and on Permafrost,
   else the lowest-health one that is, guid breaking a tie. Only Permafrost counts: off it, one below
   80% submerges and comes back full;
6. while he is down, the nearest scarab within 30 yd that is selectable, attackable and in combat.
   Each puts 20000 threat on a random player, so no tank gathers them; 30 yd is the tank pickup
   reach.

Wave 2 is due after the submerge, so it dies in phase 2. A sphere shooter and a gripping DK not
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
- `kite`, while the kiter has no usable patch and no sphere is falling within 30 yd of it: the
  flying sphere whose stand puts the kiter ahead of the spike, else the nearest. **Heroic** only once
  the kiter needs one (immune, `patch` stage, or the spike within 15 yd with no boost or immunity
  left), only a sphere 16.75+ yd from every tank patch, and within the **allowance** (`anub.allow`),
  set as he submerges: the flying spheres the missing tank patches don't need, over the submerges to
  come at his loss per surfaced phase (down to 30%), at least one. Six spheres, no respawn, and the
  guide's "use them wisely"; in the 2026-10-25 wipes one phase 2 spent all four patches, a tank patch
  first.
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

**A kite takes a tank patch only when nothing else serves** while a burrower lives, since a held
burrower off Permafrost submerges back to full, and a stand past a tank patch puts the side tank and
its melee in the spike's lane; **heroic never**. Heroic first looks for a non-tank stand whose spike
route (spike to kiter, kiter to stand) passes no tank patch within 14.5 yd, then any non-tank one,
never a patch within 16.75 yd of a tank patch: after a fail the spike restarts from the kite patch,
and its first test reaches 11.5 yd plus its 5.25 yd first walk. Branches, as `anub.kite`:

- `hold` within 2 yd; returns false, so a healer or caster keeps working.
- `detour` when the straight walk cuts deeper into Permafrost's 7.5 yd reach plus 1 than either end
  already is: via the corners of a square round the patch, sides 9 yd out, the bot's side first. A
  single point straight out to the side won't do: the leg on from it still cuts to 6 yd of the patch.
- `ring` with no patch: 40° round a 40 yd circle about the room centre, the way that opens the angle
  to the spike, floor-checked (navprobe-clean at 40, 16/16), while `kite` duty drops a sphere. A
  15 yd raw flee point risks leaving the mesh.

**Heroic kites run first** (`anub.kitestage`, guide: as long as possible without touching
Permafrost), the stage per instance per ms. The 2026-10-25 bot kites lasted 1-15 s, each ending on a
patch, 4-5 a phase 2:

- `run`: no patch search. The ring step, 40°, else 25° or 55°, either way, whose leg passes every
  patch by 14.5 yd and keeps the spike behind; a warlock with a circle takes the one within 38 yd of
  it furthest from it. None: the plain ring step. A kiter within 14.5 yd of a patch first steps
  16.5 yd off it, straight away or turned up to 90° to keep the spike behind: no leg can clear it, and
  the plain ring step once ran a kiter marked 5 yd off a tank patch past it.
- `boost:<spell>` once the spike is within 15 yd and faster: Sprint, Dash in cat form, Nitro Boosts
  (none while one of those runs, one speed aura type), Blink 20 yd the way it faces when that lands
  farther from the spike, then Demonic Circle: Teleport with the circle in reach and 15+ yd farther
  from the spike. Sprint's +70% outruns the spike's 11.4, so the chain moves on as each runs out. The
  kite casts them itself, so the kite veto never holds them.
- `immune`: Hand of Protection or Divine Shield up, still running, since standing still breaks the
  chase. The protector's claim and the paladin's own immunity wait while any boost is ready or
  running; Feign Death is zeroed for a marked bot.
- `patch` with 2 s of immunity left, or the spike within 9 yd with no boost and no immunity coming
  (Forbearance, no paladin bot in 30 yd with Hand of Protection ready): the patch stands above.

**Heroic phase 2 gathers** everyone but the kiter, the side tanks and the main tank 5 yd past where
he went down, away from the tank patches, at most 25 yd from the room centre, each on its slot 3 yd
round it (`anub.gather`), walking once 6 yd off. `anubarak gather hold` blocks `reach melee`, `reach
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
within 30 yd; heroic also keeps it 7.5 yd off every patch when it can, since the 80% slow still beats
the lane. Impale is a 6 yd cone, and clearance has to sit past the trigger radius
([pitfalls.md](../../engine/pitfalls.md)). The spot is held per bot: walked for up to 3 s, then
parked within 2.5 yd, while the spike lives, the mark stays as set (nobody between kites included),
the spot keeps 10 yd off the spike and 7 off its lane, and the spike stays within 20 yd of it (or its
distance when set), for at most 8 s. A walk in flight is left alone, since every `MoveTo` clears the
spline. While held, `anubarak spike dodge hold` blocks `reach melee` and `reach spell` by name, so a
battle res walk still runs, plus set behind, flank, formation, follow, flee, run away, random moves,
charges, Blink and Disengage. Letting go on arrival handed bots back to reach: 130 `reach spell` and
107 `reach melee` flips in the 2026-10-16 kill, and Trueshot died while `reach spell` walked him
back.

**Hand of Protection covers a kiter the spike closes on.** Once the spike is within 12 yd of a kiter
with no Hand of Protection, Divine Shield, Ice Block or Forbearance, the first alive unmarked paladin
bot by guid within 30 yd and in sight that `CanCastSpell` allows claims it; range and sight are
asked apart, since `CanCastSpell` waves an out-of-range target through. One claim per mark, kept
until the mark moves; one 1.5 s old with no immunity on the kiter passes to the next paladin, and
one lapses, for the same paladin to retake, when the spike falls back past 12 yd. With no claim, a
marked paladin bot casts Divine Shield or Hand of Protection on itself once the spike is within 9 yd
(Impale's 6 + 1.5 plus a margin). The immune kiter keeps kiting, since [standing still breaks the
chase](#pursuing-spike-34660); for the same reason no bot casts Ice Block.
Impale is physical, and 5 of the 6 kites after the patches ran out took Impales in the 2026-10-16
kill.

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
- A human's Ice Block still holds the mage still, so the spike drops it and moves on at its current
  speed.
- 10H with one side tank puts both burrowers of a wave on one patch, inside Spider Frenzy's 12 yd.
- A stepped-out grip can leave the patches 20-28 yd apart, him 10-14 yd from each, past melee reach
  of the burrowers.
- After a kite ends on the far rim, the spike re-targets across the room centre and can spend a tank
  patch on the way (2.8 s into a 2026-10-26 kite).
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
- Not handled: the phase 3 heal hold and stamina-buff cancel, Freezing Slash defensives, ignoring
  burrowers once he is low, scarab threat and Mandible stacks, normal's phase 2 raid positioning.

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
| `focusdue` | 1 while the focus burrower goes ahead of him. Per instance |
| `grip` | 1 tank patches gripped, 2 a grip failed and the pull shoots. Per instance |
| `allow` | Heroic kite allowance in spheres, written as he submerges. Per instance |
| `sphere` | Sphere duty: `kite`, `stock`, `0` or `1` (a side), `grip0`/`grip1` (a DK's grip), `none` |
| `gripstep` | The DK's grip: `walk`, `hold`, `cast`, `wait` (sphere in the air), `kill`, `none` |
| `sidetank` | `hold`, `pickup`, `ready` |
| `kitestage` | Heroic kiter: `run`, `boost:<spell>` (Nitro Boosts as 55004), `immune`, `patch`, `none` |
| `gather` | Heroic phase 2: `walk`, `hold` |
| `kite` | The kiter's branch: `patch` (walking to a stand), `detour`, `ring`, `hold`; `none` when there's no spike or the mark moved on |
| `dodge` | A new dodge spot's reason: `spike` (within 10 yd of it), `lane` (on its line to the kiter), `none` (no clear spot) |
| `protect` | `hop` while this paladin holds the kiter's claim, `self` while the marked bot needs its own immunity, `none` |
| `interrupter` | `1` while the bot holds Shadow Strike duty, `0` once the cast ends |
| `pickup` | The pickup tank's step: `taunt` (off a non-tank), `attack`, `stepout` (out of Permafrost), `hold` (no leg), `drag` (a leg to the anchor), `submerge` (a leg to the submerge spot) |
| `defensive` | What the Swarm tank cast, `covered` (one running) or `none` |
| `focus` | The focus burrower, 0 when none; kept current under a skull. Per instance |
| `dps` | A DPS bot's pick: `skull`, `burrower`, `boss`, `scarab`, `none` (a sphere shooter too) |

The marked player is the 67574 `aura` stream, so no key repeats it.

The per-bot keys are change only: a bot that takes the same branch twice writes one row.

`tools/botobs/bosses/anub_arak.py` reads the rest, keying him on entry 34564 since the spike shares
his name and boss flag, and checking map 649 since Azjol-Nerub's boss shares the `anub-arak` slug.
`--phases` (spans, his health at each edge, deaths per phase), `--pull` (who stood west of the Web
Door or above z 200 at the pull and when each got in, stage 9 to the pull), `--spike` (each 67574
window: kiter, `kite` branches, seconds, outcome, when the spike first came within 12 yd, Hand of
Protection on the kiter, its own Divine Shield or Ice Block, `protect` rows; Impale on anyone else;
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
its spot, and per wave the focus start against the next wave; `--spheres` what downed each sphere (hit
rows on spheres, logged from the pull, else the last cast at it; pet to owner, DoT or area flagged),
where a tank patch landed against its spot, each grip, whether it landed and the side that latched it
against the side it served, patches per submerge against
`allow`; `--spike` seconds per `kitestage`, the boosts and immunities cast, Ice Block and Feign Death
flagged; `--threat` the main tank's share of samples inside 7.5 yd of a patch and his distance to each
tank patch at every submerge. The banner names declared probes the pull never wrote, and each side's
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
