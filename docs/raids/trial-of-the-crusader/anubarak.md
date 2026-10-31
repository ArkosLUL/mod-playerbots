# Anub'arak

Raid-wide facts and the code layout: [README.md](README.md). Mechanics: `boss_anubarak_trial.cpp`;
tactics: Warcraft Tavern `anubarak-master-strategy-guide-toc-25/`.

## The room and the pull

- **Floor**: open ground about 60 yd round (745, 135), z 142.1-142.6, navprobe-clean at 20, 35 and
  50 yd from it, and at 30-45 yd with the bot filter (`--nav 0x09`). The corridor runs west to the
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
  player at 7 yd/s gains about 30 yd in the first 14 s, then loses it in 7. The steps are auras:
  65920 and 65922 last 7 s and the next goes out as each ends, 65923 stays, so the time to the next
  step is the running aura's duration. A fail strips them all.
- **Pure pursuit**: it heads straight at its target's position each moment. Replayed from the
  kiter's real path over six 25H pulls (2026-10-26), its position 6 s on is within 0.3 yd median,
  0.9 p90, its closest pass by a patch 0.4 p90; a straight lane to where the kiter will be is 6.5 /
  19.7 and 10.4. A kiter faster than it (Nitro, about 17.5 yd/s) running an arc has it cut the chord.
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
  voids it. Forbearance (25771) bars a second one within 2 min. Feign Death (5384) drops on movement:
  cast mid run it lands and goes in the same ms.
- **Hand of Freedom** (1044, snare and root immunity) strips Permafrost's slow (mechanic 11) and
  keeps it off 6 s; in the traces every one on a carrier removed it the same ms. A paladin's Hands on
  one target replace each other (`SPELL_SPECIFIC_HAND`, per caster).
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
  holds one. Acid-Drenched Mandibles (25H 67863) stacks a nature DoT, 1,800 a stack every 3 s, 60 s,
  refreshed by each hit, up to 40, no dispel; the scarab's melee itself does nothing. Determination
  fires first at 10-50 s, then every 20-60 s: full heal, +100% speed, immune to most crowd control.
  Phase 2's are 600 s summons (66340), living a median 12-17 s, 3-4 a submerge 20-60 s past the
  emerge in the 2026-10-26 late wipes, where the main tank, their pickup, came up with 7-13 stacks
  and died to them plus his melee (58-123k Mandibles in his last 20 s) in every one.

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
| EMERGENCY+9 | `anubarak kiter needs immunity` → `anubarak kiter immunity` | a marked bot, as the [kite planner](#the-kite-planner) says | paladin `hand of protection` while it has a patch to end on, the emerge is 10 s off or the plan ends on its own hand, else (always for an `own` plan) `divine shield`, the other as fallback; mage `ice block`; rogue `vanish`; hunter stops, then `feign death` |
| EMERGENCY+8.5 | `anubarak kiter needs freedom` → `anubarak free kiter` | the paladin bot picked for a kiter carrying Permafrost | `hand of freedom` on it ([below](#how-the-bots-play-it)) |
| EMERGENCY+8 | `anubarak pursued by spike` → `anubarak kite spike to permafrost` | carries 67574 | heroic: a boost due, at any stage; else walks the plan's track, `MOVEMENT_FORCED` |
| EMERGENCY+7 | `anubarak kiter needs protection` → `anubarak protect kiter` | the paladin holding the kiter's claim | `hand of protection` on the kiter |
| EMERGENCY+6 | `anubarak spike nearby` → `anubarak avoid spike` | unmarked, within 10 yd of the spike or 7 yd of its predicted path (16 and 10 while it outruns the bot), or not yet at its held dodge spot | walks to its held dodge spot, `MOVEMENT_FORCED` |
| EMERGENCY+5 | `anubarak on permafrost` → `anubarak leave permafrost` | no tank, unmarked, no dodge held, no grip duty but `kill`, within 7.5 yd of a patch | walks out ([non-tanks](#how-the-bots-play-it)) |
| EMERGENCY+3 | `anubarak burrower casting shadow strike` → `anubarak interrupt shadow strike` | a caster's interrupter | interrupt or stun; out of range, walks in first |
| RAID+7 | `anubarak side tank dead` → `anubarak battle res side tank` | the lowest-guid bot druid with Rebirth ready, a tank only when no other can, never the pickup tank while he is up; a dead side tank's corpse with no self-res pending | caster form, walks to 26 yd, Rebirth |
| RAID+6.5 | `anubarak side tank needs defensive` → `anubarak side tank defensive` | a side tank at Expose Weakness 5+ (67721 remapped: 25-man carries 67847) or under 50%, a physical defensive ready | `NextTankDefensive` on self, physical only: burrowers melee, so no Anti-Magic Shell (Felesta cast it 5× against them, 2026-10-26) |
| RAID+6 | `anubarak leeching swarm on tank` → `anubarak tank defensive` | his victim in `Swarm`, a defensive ready | `NextTankDefensive` on self |
| RAID+5.5 | `anubarak grip frost sphere` → `anubarak grip frost sphere` | a DK with a grip duty | `walk` to its stand, stops and `cast`s Death Grip on the duty sphere, `kill`s it once landed |
| RAID+5 | `anubarak ranged should seed permafrost` → `anubarak destroy frost sphere` | a sphere duty | attacks the sphere, out of reach walks in first |
| RAID+4.5 | `anubarak hop escort` → `anubarak hop escort` | heroic, the escort paladin over 4 yd off its spot (`anub.escort` `walk`/`hold`) | walks there |
| RAID+4.2 | `anubarak cleave buff near burrower` → `anubarak drop cleave buff` | a melee DPS bot with Blade Flurry or Sweeping Strikes on and a burrower in melee reach at its area share ([burrower guard](#how-the-bots-play-it)) | cancels the buff |
| RAID+4 | `anubarak dps target` → `anubarak dps target` | DPS whose `GetAnubarakDpsTarget` pick isn't its current target or its pet's | attacks the pick, pet too |
| RAID+3.8 | `anubarak melee burrower stand` → `anubarak melee burrower stand` | a melee DPS bot on a burrower near a patch, out of reach, within 8 yd of a patch or 1.5 off its held stand | walks to its stand |
| RAID+3 | `anubarak burrower needs assist tank` → `anubarak assist tank hold burrower` | a side tank with a burrower to take, not the pickup tank while he is up | picks up, holds it from its rim |
| RAID+2.5 | `anubarak side tank area threat` → `anubarak side tank area threat` | an unmarked side tank while he is up, a spell passing the [area hit test](#how-the-bots-play-it) | casts it |
| RAID+2 | `anubarak scarab on raid` → `anubarak tank pick up scarab` | the pickup tank while he is down; an unmarked side tank, not the escort, with no burrower to take and no wave due within 8 s, while he is up (heroic also down); a scarab within 30 yd on a non-tank: heroic side tanks only one on a healer, heroic phase 2's pickup tank only one on itself | taunt (not one on itself), attack |
| RAID+1.5 | `anubarak side tank ready` → `anubarak side tank ready` | an unmarked side tank with nothing to take, not the pickup tank while he is up, not gathered, not the escort | attacks a scarab on itself, walks to its rim (else spot) |
| RAID+1.2 | `anubarak redirect threat` → `anubarak redirect threat` | a hunter or rogue bot with a [redirect duty](#how-the-bots-play-it), its redirect ready or Misdirection's charges left | Misdirection or Tricks on the tank, then Steady Shot into the dump target |
| RAID+1 | `anubarak engaged by main tank` → `anubarak main tank hold boss` | the pickup tank while he is up | steps out of Permafrost, else leads him in latched legs onto the anchor (round a patch in the way), or from 65 s onto the submerge spot |
| RAID+0.5 | `anubarak raid gathers` → `anubarak gather` | heroic phase 2, not the kiter or a side tank with a burrower to hold or take, no dodge held | walks to its gather slot, round any patch; while the way there crosses the spike's path waits, stopping a walk (`wait`) |
| RAID+0.45 | `anubarak healer stand` → `anubarak healer stand` | a healer bot while he is up, unmarked, no dodge held, off Permafrost, over 6 yd off its stand | walks there, round any patch |
| RAID+0.4 | `anubarak demonic circle due` → `anubarak demonic circle` | heroic, a still warlock bot, circle missing or under 100 s; on its staging stand before the pull, or while he is up | Demonic Circle: Summon |
| MEDIUM_HEAL+5 | `anubarak penetrating cold on raid` → `anubarak heal penetrating cold` | healer, a carrier below 90% or one it can [cover](#how-the-bots-play-it) | on the lowest, the cover, else under 90% the first direct heal `CanCastSpell` allows |

The **pickup tank** is the main tank while alive, else the side tank he is on (a human who taunted
him keeps him, no bot taunts him back), else the first bot side tank; while he is up its burrowers go
to the other side. Target guards zero `TankAssist`
and `DpsAssist` for it while he is up (`anubarak pickup tank target guard`) and for a side tank all
pull (`anubarak side tank target guard`): in the 2026-10-25 wipes a side tank was on him when a wave
landed. `DpsAssist` is zeroed for a sphere shooter and a gripping DK not just waiting on its spot
(`anubarak sphere shooter target guard`): a flying sphere is out of combat, never the dps target, so
assist would swap the shooter off it every tick. Heroic phase 2 zeroes `TankAssist` and blocks
`Taunt` for the pickup tank (`anubarak main tank scarab guard`, `... scarab taunt guard`), so it hits
only scarabs on itself. A side tank with nothing to take falls through to
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
of the spot (any distance for a [far grip](#how-the-bots-play-it)) and 20 of the other side's patch: a
gripped sphere drifts back toward its route while it dies, and in 3 of 5 2026-10-26 wipes the north
grip's patch latched south. The anchor is the midpoint of the two spots. Guide: two spheres "one on
Anub'arak's left and one on his right", the boss "perfectly in-between two frost patches". **Until
one has landed he is held where the tank picks him up**: a room-centre fallback dragged him twice
before the patches landed at 9-12 s. With one down he goes to the anchor all the same: held where
picked up, he stood 14-31 yd off the burrowers in the 2026-10-26 wipes. The pre-pull pickup stand is 10 yd west of the
anchor (`GetAnubarakPickupStand`, (735, 135), navprobe-clean), off both spots; he runs in from the
east and stops on the anchor.

**Heroic's tank pair is the east one** (user call, 2026-10-29), homes 1 (north, side 0) and 0
(south): pulled in like a refill pair (`PairSpots`, pure, nativetest) to (789.6, 141.1) / (786.4, 117.5),
anchor (788.0, 129.3), R (770.2, 131.7), from the engage. Its grips and shots take only spheres homed
there while one flies, so the four nearer the centre are left for the kites; the pre-pull side points
are those spots (`GetAnubarakPrepullSideStand`) and the pickup point 10 yd west of that anchor, (778.0,
129.3). All three lie 8-16 yd from his wait point, inside his 23 yd aggro: walking onto his, Bulwark pulled
him at 21-23 yd in Z and AA (2026-10-30), before the player. So `LayoutAnubarak` steps each toward the
centre to 23 + `STAGING_UNPARK` yd off `Locs[17]`: (760.3, 132.4), (760.4, 137.1), (760.7, 128.4),
navprobe-clean (bot filter, 16/16 at 3 yd). Normal's are already clear. Burrowers walk 64-105 yd from the west exits to
them (41-63 to the old centre spots). **Heroic refills follow the spheres left** (`anub.spot0`/`anub.spot1`), from once both sides have
held a patch: each sphere's home is its nearest spawn (AnubLocs 5-10) at its first sample, and heroic
never respawns one. After the first pair the four left hover 38-57 yd from the centre, the fixed spots
35-62 yd from them, and 2026-10-26 refills waited 25-38 s on drift. One side held: the open spot is on
the line from the held patch to the nearest home past it on the open side (north +y) and 20+ yd off,
20-30 yd out. Both open: the nearest north/south pair of homes 20+ apart, each pulled in until 30
apart (west pair 5.4 yd each). Spots are pulled in to 45 yd from the centre and floor-checked; no
candidate, the rule above. A falling sphere's home still counts and the spots rebuild after the
latches, so the spot it falls for holds until its patch latches. Likely spots, navprobe-clean: west
pair (707.1, 156.1) / (709.6, 126.2); east (789.6, 141.1) / (786.5, 117.5); north open off (745, 123)
(723.8, 144.3); south open off (745, 147) (721.0, 129.0). Normal keeps the fixed rule.

**The raid point** (R) is 18 yd off the anchor at right angles to the spot line, on the room centre's
side, west on a tie: (727, 135) for the first pair, 21.6 yd from both patches, 8 behind the pickup
stand. Rebuilt as the anchor moves while he is up and at the emerge; held from a submerge to the emerge,
except heroic steps it 12 yd straight off a tank patch spent while he is down (within 45 yd of the
centre, on the floor), since the spike restarts from there. It is the healer stand and heroic's phase 2 gather, and the
submerge spot keeps off it ([below](#how-the-bots-play-it)). R for the refill cases above, navprobe-
clean with 3 and 6 yd rings 12/12: (726.3, 142.7), (770.2, 131.7), (747.2, 146.3), (743.8, 123.6).

**Healer stand**, every difficulty, while he is up: each healer bot (by guid) has a slot on a 3 yd
ring round R, turned until 8.5 yd clear of every patch, else R; over 6 yd off it walks back
(`anub.hstand` `walk`/`hold`). With the tank spots 40+ yd apart (far grips) the healers split by guid
into two groups, each ring 17.5 yd off its side's spot toward R (not toward the anchor, which keeps
them off him), when that centre clears the patches and lies within 36 yd of the main tank: from R a
side tank 30+ yd out is past a heal once the slots spread. A living main tank over 36 yd from the
centre slides it toward him to 36, when that clears the patches and lies on the floor: in 2026-10-28 T
the human took him 45 yd off the healers at R and died 10 s in. With no split and a living side tank in
reach of the centre, as in heroic's submerge drag, only the odd healers by guid slide; the rest stay for
the side tanks. Heroic, from his 35% in the last phase 1, it is behind the tank patches ([phase
3](#how-the-bots-play-it)). `anubarak healer stand hold` blocks formation and follow; reach spell
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
side 0 bare all of one wipe and side 1 bare 114 s of another. **A far grip** takes the nearest sphere
when every one left hovers past the 44 yd search: a DK with no tank duty first, the side's own tank
only without one, walks to a floor point just inside reach (28 yd 3D less 3) on the sphere's side
nearest the spot, turned ±20°/40° off the spike's 16 yd and its lane's 12, and grips as above, or
earlier once the sphere is in reach from a point no farther from the spot than the stand; the patch
serves the side wherever it lands, and the spot and anchor follow it. In 2026-10-27 pull I the last
sphere hovered 60-70 yd off side 1's spot (789-815, 136-165, z 157-165) and the side stood bare 101 s.
From 13-17 yd up one grip gains 0-20 yd: in the evening pulls M and N it landed 46-49 yd off side 1's
spot, about 70 from side 0, and side 1 lost its tank in both (N: then side 0's, then the raid); in N
the stand, read off a sphere rising past z 165, walked Obliteration 53 yd out to 8 yd under it.

**A relay grips it again** (`PlanRelay`; `RelayPosts`, pure, nativetest): a landing over 20 yd off the
spot gets a next link, the side's own DK tank first (its burrowers walk with it, its pickup keeps
Death Grip), else the lowest-guid DK bot with no tank duty, never the pickup or the other side's tank,
Death Grip back within 3 s. It posts 25 yd from the landing toward the spot (the spot when nearer;
turned ±20°/40°; on the floor, 8.5 yd off every patch, off the spike's 16 and its lane's 12) while the
link before walks out, and grips once that one has landed and the sphere is in reach from a point no
farther from the spot than the post; low, it is about 24 yd more. Until the last link lands the DKs
before it hold and nobody hits the sphere. A link that dies, is marked, loses its grip or hasn't
gripped 3 s after the landing (the sphere rises back out of reach in about 4) drops the relay, and the
sphere is killed where it is. A side tank linked in holds where it grips from.
**A grip can pull nothing**: Death Grip 49576 is magic-class and can miss the hit roll (sphere level
80, burrower 82). Over 29 instance-39 traces a 49576 with no triggered 49560: Felesta 11 of 121,
Obliteration 1 of 46, Deathsong 1 of 8. A grip whose sphere is not 3 yd nearer the DK 0.6 s on took
no pull (one that takes jumps it 20 yd in 120 ms): a relay link's lets the landing before it stand
(`anub.relay` `miss`), so the next link grips from there or the side kills it there; a first grip's
goes back to picking a DK at once. In 2026-10-27 night Q P1c Felesta's link pulled nothing, the relay
waited, its shooters only take spheres within 20 yd of the spot, and side 1 had no patch for 54 s. Without even that the DK keeps its duty and the side's shooters take any they can; a 12 s idle
hand-off to the shooters alone left the north side bare 40 s. Refills come only while he is up: nobody leaves the gather. Until the kill,
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
C or puts the tank's spot within 8.5 yd of a patch. A tank whose walk there passes within 7.5 of one
first steps round him (`prestep`; `DragPreSteps`, pure, nativetest): the nearest floor point 9, 10 or
11 yd from him, inside his 12 yd chase start so he stays put, 8.5 off every patch, with the walk there
and on to the spot clear; dropped if he moves meanwhile or after 6 s. In 2026-10-27 N P1b the
step-out left the tank 7.6 yd from side 1's patch on its far side from the anchor, every leg crossed
it, and he was held 56.3 s, 27.4 yd off the anchor (12.3% of him that phase 1). The line between the
patches is all Permafrost, so the hold tries the anchor, then points 4 and 7.5 yd off it toward his
side, then for each patch within 11.5 yd of his line to the anchor points 12 yd either side of it,
square to the line, the nearer first, 8.5 clear of every patch (`DragDetours`, pure, nativetest);
and once he has stood 8+ yd off the anchor 5 s with no leg, an 8 yd ring round it at 45°. He
surfaces where the spike was (66170): at 4:44 of 2026-10-27 pull J it ended past side 1's patch on
the line through both, every leg crossed that patch, and he was held there 65 s, 38.9 yd off (4% of
him that phase 1, melee 154 s in Permafrost). A latched leg comes back whatever the point. A leg ends once he is within 4 yd of the point;
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
first kites started with the spike within 12 yd. **Heroic drags him farther** (user calls, 2026-10-29
and -30; `FarSubmergeSpot`): searched from 40 s left, the point every 15° on the 45, 40 and 35 yd rings
within the drag's 36 yd of him ((15 - 3 s) × 3 yd/s), farthest from R in 5 yd bands, nearest the tank
on a tie, 50+ yd off R first, the same patch clearances and line to R, on the floor; none, the rule
above. The drag leaves the anchor with under 15 s left, later once the walk there (distance / 3 yd/s
+ 3 s) takes less: expected 28-35 yd off R, about (762, 165) or (762, 105). In 2026-10-30 Z it left
at 40 s (84 yd / 2 + 3 = 45 s), the backpedal legs made 3.0-3.6 yd/s net, he went down 68-70 yd off
the gather 10-17 s early, and 3 of 4 side-tank deaths came in the drag, 2 of 4 healers 35+ yd off the
side tanks. Latched for the window, redone
once a patch lands closer than the clearance it was found with; each search writes `anub.subspot`. With no patch alive or no spot clear,
the anchor rule applies. The drag there also tries points 12 yd either side of each patch within
11.5 yd of his line to the spot, nearer the spot than he is (`anub.pickup` `subhold` when none gives
a leg): in 2026-10-28 S P1b the tank's walk crossed a patch, no leg went out for 15 s, and he went
down 41 yd off the spot, 9 yd from side 1's patch, which the first kite spent 3 s in. At 20.75 he went down 18.9-25.5 yd from the patches in the 2026-10-26 wipes,
the spike starting close enough to spend them.

**DPS bots hold off him near the tank's threat** (`anubarak threat cap`, Onyxia's hold): while he is
up and his victim is a tank, a DPS bot from 90% of that tank's threat until under 80% loses its
spells and melee on him and its area damage within 30 yd of him; heals pass. Threat is read off his
threat list, never the `threat` AI value, a uint8 share that wraps past 255%. No bot has a threat
brake ([pitfalls.md](../../engine/pitfalls.md)), and in both 2026-10-16 kills Angry pulled him and
died to one hit (5.6 s, the tank still walking, 32.6k; 13.7 s, 33.1k). White swings go on:
`Attack()` starts auto-attack itself, so the hold zeroes only actions. The same 90/80 latch runs per
DPS bot and burrower whose victim is a tank (`anubarak burrower threat cap`, every phase): its actions
on that burrower and its area damage within 10 yd of it or its target with one there. A melee bot
capped on its side's focus drops it from its pick until freed (`anub.dps` `capped` once it falls to
him), since its white swings and procs still pulled burrowers off.

**Taunt guard.** `anubarak taunt guard` blocks `Taunt`, Righteous Defense included, while the bot's
current target is him and his victim is another tank, a human included; the pickup tank's own taunt
off a non-tank is direct and unaffected. He obeys taunt DR, and in phase 1 of the 2026-10-16 kills
the bot main tank taunted him 7 and 15 times (2 and 6 Righteous Defense) through the paladin's
generic `lose aggro` taunt, trading him with the human tank.

**Burrowers are held on Permafrost in every difficulty**, since the script's submerge rule is not
heroic-gated (guide: "bring burrowers to your designated frost patch"). **Each belongs to a side**
(`anub.bside`), once per instance per ms: up to two on a living unmarked side tank stay with it, so a
side tank never taunts those off the other, and a third goes to the other side while it has fewer;
the rest go by exit, at most two a side while both side tanks
are usable, and a dead or marked side tank's to the other (a marked one only exists in phase 2). A
human side tank's still loose 5 s after it came out goes to the bot side tank, past two: three on one
patch frenzy, but none eats a healer. `anubarak no ghouls` blocks `army of the dead` all fight: its
ghouls taunted burrowers off the side tanks and the patches, where one submerged to full. A side tank attacks the burrower on it, else its side's loose one, one hitting a healer first, then the
nearest its patch, and picks a loose one up even while holding one: Righteous Defense on whoever most
of them hit, unless he is on them, they are a side tank, or it would leave the tank past two of its own
(it taunts all three attackers: in 2026-10-28 R and S it put three on the tank four times, three
fatal); the other side's on them too go back to their tank's taunt (2026-10-30 BD 6:25, held back for
them, it reached Elemena 0.3 s after she died and the wipe began). Then Challenging Roar or Shout with
two within 10 yd, none of the other side's and him past 10 yd plus his reach, the class taunt, then Feral Charge,
Intercept, Avenger's Shield or Death Grip, each gated on `CanCastSpell` and reach; else it walks in. In
the 2026-10-25 wipes burrowers were on a tank 18-41% of their life and two warlocks died to them in
every pull. It walks to its **hold** only once its pick is on it; an untaunted
one is attacked in place, since walking off leaves it on its victim and the patch walk and reach
melee trade the tank back and forth. It skips the walk (and the ready node's) only while the spike
could reach the hold within the walk plus 2 s (distances over speeds), where the dodge would walk it
straight back out; a veto on the spike's lane crossing the hold, however far the spike, kept side
tanks 7-18 yd off at three phase 2 starts of the 2026-10-26 late wipes, each losing a burrower to a
submerge. With nothing to take it stands on its hold (`anub.sidetank` `ready`), hitting any scarab
on itself, and in normal phase 2 the scarab node is the main tank's only.

**The hold is the patch's rim toward him** (`GetAnubarakSideTankHold`): 7 yd from the centre toward
the anchor once every burrower on the tank carries Permafrost, the centre while one doesn't, so it
trails in; no patch latched, the spot. Burrowers stop about 3 yd short of their tank (median 2.97,
p95 3.49 over four 25H pulls), so from every exit they sit 3.5-5.7 yd in, on frost (6.39 yd reach),
with melee room outside the 7.5 yd slow. Held at the centre they sat on the exit side, the far side
from him, and melee crossed the patch: 216 and 462 s slowed in the 2026-10-26 late wipes. The rim
is not floor-checked (read every tick; it lies between patch and anchor). Patches landed close turn
both rims toward his side of the line between them, by the smallest 15° step that keeps the
burrowers (4 yd in toward their rims) 14 yd apart, past Spider Frenzy's 12, at most straight apart
(`SideHoldDirs`, pure, nativetest); a turned rim is floor-checked, else the centre. In 2026-10-28 S
P1b a refill fell 17.7 yd from the other side's patch (the shot's 20 yd floor holds on the flying
sphere, which drifts as it falls), the rims faced each other 3.7 yd apart, the two sides' burrowers
were within 12 yd in 99 of 200 samples and the side tank died twice. **Heroic phase 2:** a side
tank with no burrower of its side to hold or take gathers with the raid and walks back at the emerge,
since one standing on its patch walks a spike that marks it into the patch.

**Side tanks spend their area threat** (`anubarak side tank area threat`, `anub.saoe`). Taunts took
burrowers back in a median 0.4-0.9 s of 24-33 pull-offs a pull (2026-10-30 BA-BE); "medium aoe" needs
3 enemies within 8 yd and a side tank holds 2, so it cast Blood Boil never and Death and Decay 4-8
times a pull. While he is up an unmarked side tank with both its own burrowers (`anub.bside`) within
15 yd casts the first known, ready spell whose hit set holds both and none of the other side's, and
him only under the cap:

- round a point: death and decay at the pair's midpoint shifted 0-6 yd away from him, within 30 yd;
- round the caster: consecration, blood boil, thunder clap, swipe (bear), holy wrath;
- round its diseased target: pestilence;
- bouncing chains, 3 targets, each jump to the closest to the last hit within the jump plus both
  reaches: hammer of the righteous (10 yd), avenger's shield (7.5; `SPELL_ATTR4_BOUNCY_CHAIN_MISSILES`);
- caster chains, 2 targets, the closest to the caster in its front 180° within 10 yd plus both
  reaches: heart strike, cleave.

The core takes a creature into a player's area within the radius plus its combat reach
(`Spell.cpp`), his 10 (`creature_model_info` 29268), so he is inside any area centred within 18-20 yd
of him: in BD side tanks stood a median 7.8-9.8 yd from him, 3 from their own burrowers and 14.6 from
the other side's. Radii come off the spell, so glyphs count; an 8-10 s ground area counts the others
3 yd farther out. Burrowers, scarabs and he are undead. **The cap** (`AnubarakSideTankBossCapped`)
runs from 70% of his victim's threat on him until under 60%. `anubarak side tank area guard` zeroes
the class rotation's spell from that list failing the same test (one own burrower is enough), and any
other area spell with one of the other side's within 10 yd plus its reach of the bot or its target.

**DPS target directly, no raid icon** (`GetAnubarakDpsTarget`, DPS by `IsToCDpsBot`, only while
he is engaged), first match:

**DPS split by side** (user call), every difficulty: the living DPS bots of each role by guid serve
side 0 and 1 in turn (`anub.dside`), split again at each wave so a death rebalances. Each side has its
own focus over the burrowers `anub.bside` gives it (`anub.focus0`/`anub.focus1`). In the 2026-10-27
wipes one raid-wide focus switched sides 24-30 yd apart 13-25 times a phase 1, and the 7 melee walked
1,466-1,497 yd each a phase 1 (`melee burrower stand` 763-1,010), moving 24-58% of it; he lost
13.7-24.8% a phase 1 across five pulls, and a kill needs about 20 by Berserk. First match:

1. a player's skull on him, a burrower or a scarab, alive, in combat, selectable and attackable;
   never a sphere, which heroic has six of for the pull;
2. heroic phase 2, a gathered melee bot: the nearest scarab within 10 yd;
3. a melee bot: a burrower within 10 yd on a non-tank, any side, whose side has no usable side tank
   to taunt it back (`loose`; with one, BK and BN melee spent 14-35 s each on burrowers no tank held,
   the whole group after one on a healer, the side tank's taunt back in 0.4-0.9 s);
4. its side's **focus** whenever one qualifies (below), he up or down: no rate test, since a wave's
   8 burrowers × 753k and 14 scarabs × 212k a cycle are fixed and he gets what is left;
5. once he is up, the nearest leftover scarab, within 10 yd for melee, 30 for ranged ([Adds](#adds):
   their stacks last a minute and every hit refreshes them);
6. a ranged bot: the other side's focus, reached from where it stands; a melee bot never walks over;
7. him while selectable;
8. while he is down, the nearest scarab within 30 yd that is selectable, attackable and in combat.
   Each puts 20000 threat on a random player, so no tank gathers them; 30 yd is the tank pickup
   reach.

A bot outside the split (a human, one joined since) takes side 0's focus, else side 1's. A side's
focus (per instance): selectable and under 10%; else **settled**: tanks within 5 yd have held it 4 s,
the clock running on across a swap between tanks and reset by a non-tank victim or none within 5 yd
(restarted per tank, BH wave 6 passed side 1 → 0 → 1 and went 14 s unfocused, Felesta dead at 9
Expose Weakness stacks), its tank within 5 yd carries 6+ Expose Weakness stacks (67721/67847, user
call; BH's 6th came 1 s before the late pick; with no distance, BK 5:49 #330 was picked 14.7 yd off
its tank, still walking in), or its side has no usable side tank; and then on Permafrost or in its **free time**, the 25 s after it first
shows or emerges, since the script's first submerge check comes at 30 s. Unsettled, it is still
walking in from its exit: every wave's first pick in the 2026-10-26 late wipes came at the spawn,
51-61 yd from any tank, and the raid ran across the patches after it. Picked the moment a tank stood
within 5 yd, it lost Mighty in 2026-10-27 L 2:28: Felesta held both south burrowers from +0.7 s,
Mighty hit one from +2.3 s and had both by +4.7 s. Off Permafrost in its free time
goes first, the one whose time ends first, then the lowest health, guid breaking a tie; the pick is
kept while it qualifies, stays on the side, and nothing in its free time outranks it. Off Permafrost
one below 80% submerges on a 3 s check and comes back full, but one under 10% dies first (one was left
at 0.1% in a 2026-10-26 wipe). Shooting only patched ones, side 0's wave 1 lived 39-45 s in a
2026-10-26 pull.

A **melee** bot takes its focus only with a **stand**: a floor point within 4.5 yd of it and 8 from
every patch centre, out from its nearest patch, turned up to 100°; one in his melee reach (less 1 yd)
behind him first, then one in his reach, nearest the bot, so the switch to him needs no walk; else the
one nearest him. Else him, or a scarab while he is down. The stand is held per bot while the burrower
stays within 1.5 yd of where it stood at the pick: the walk happens once per burrower. With a burrower
within 12.5 yd of a patch as its target, `anubarak melee burrower stand` walks it there, round any
patch it would cut, and `anubarak melee burrower stand hold` blocks `reach melee`, set behind and flank
(behind a rim burrower is its patch). Held burrowers sit a median 12.7-16.8 yd from him, so one stand
reaching both exists only part of the time.

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
burrower always, and its area damage (`IsRaidAreaDamage`, Blade Flurry and Sweeping Strikes included)
while a burrower within 10 yd of it or its target is still settling, or, for a melee DPS bot, holds
60% of its tank's threat from that bot until under 50% (`AnubarakBurrowerAreaCapped`, plus the
burrower's reach); with one of those within melee reach a running Blade Flurry or Sweeping Strikes is
cancelled. In 2026-10-30 BA-BE melee pulled burrowers off with Divine Storm (8 yd round him, plus Holy
Vengeance), Blade Flurry, Sweeping Strikes, Death and Decay ticks and procs. An interrupt or stun on a
casting one passes. Bladestorm waits
while any burrower is within 25 yd: the guard reads only the cast, and in 2026-10-27 N Mighty's at
0:14.5 outlived his pick, which moved to a burrower, the stand walked him spinning into side 1's
pack, and one turned on him (dead 0:21.8). Affliction multidots
put Corruption, Unstable Affliction and Curse of Agony on all four wave-1 burrowers at 8-15 s while
the pick was him, and Agony died to a loose one every 2026-10-26 pull. A tank holding no side holds
its area damage with any burrower within 10 yd, and `anubarak main tank shout guard` blocks its
`challenging roar` and `challenging shout`: in L 3:32 a burrower walked to 3 yd of the bear main
tank, his Swipe and Challenging Roar took four, and he died at Expose Weakness 5.

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
  the flying sphere whose stand puts the kiter ahead of the spike, else the nearest.
- `stock`, normal phase 2 only, one at a time, while non-tank patches plus falling spheres number
  under 2: the flying sphere nearest the room centre, over 16.75 yd from the spike (from him before
  it spawns) and 9 yd off its line to the kiter, kept while it qualifies since spheres drift. Kites
  start every 5.5 s and phase 1's four patches were gone at 1:43 in the 2026-10-16 kill; normal
  refills a slot within 4 s, and the ring kite circles the centre. A phase 1 stock would crowd the
  submerge spot.
- `stock`, heroic (user calls, 2026-10-29 and -30: two kite spheres a submerge, so two submerges is the
  limit and the boss must reach 30% before the third): two usable kite patches down (falling ones
  count, one within 16.75 yd of a tank patch doesn't), each tick naming as many apart as are missing;
  at most two spheres a submerge, counted off those flying as its budget opens 30 s before he goes down
  (else as he does), each kite patch down then counting as one. A flying sphere hovers past the spike's
  test, so the rest wait flying. With one down at a time, kiters spent tank patches with none down:
  2026-10-30 BC P2b's last sphere drew no cast for 28 s, and BD P2's second landed 17 s after the first
  went.
  - Over phase 1's last 30 s, the first pair latched (`PreSubmergeKiteSphere`), time for two kills and
    the fall below with 4 shooters on two: nearest the centre within 50 yd, off the latched submerge
    spot (him while none) by its search clearance, at least 16.75, so the spot isn't searched again
    mid-drag; 14.5 off the spot's line to R, 16.75 off the tank patches.
  - Once he is down, the next as soon as a kite spends one, with no chase gate: in Z a 4 s gate, 2.9-8.6 s kills and
    the 1.5 s fall landed patches 9-15 s into their chase at best, and P2b's second only 50.6 s in
    (Hunter's Mark first, the sphere past 50 yd, shooters idle 4 s). The pick is `kite`'s (with no
    kiter, nearest the centre), within 50 yd of the centre, 16.75 yd off the spike (where he went down
    before it spawns), 9 off its lane to the kiter and 16.75 off the tank patches, never a tank-pair
    one before the first pair latches.

Each need gets up to two shooters, a `stock` while he is up one (its window opens as burrower wave 2
lands; 4-5 ranged left the burrowers for it in BK-BN), latched while its sphere and their eligibility hold; a vacancy
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

#### The kite planner

**One planner picks the kite's path and how it ends** (every difficulty, user call 2026-10-31;
`ToCAnubarakKitePlanner.h`, pure, nativetest; `PlanKite`), re-planned every 250 ms per kiter, memoised
per instance. Each candidate is judged by the pursuit model (`ToCAnubarakPursuit.h`: 100 ms steps of
[pure pursuit](#pursuing-spike-34660), the speed steps off its auras) up to the spike's despawn when
that is within 30 s, else 25 s; a run still going at 25 s counts as caught there (the spike runs at
top speed by then). What it sees (`KiteWorldFor`):

- the kiter at its pace, a running boost counted for what is left of it, and a bot's ready Sprint, Dash
  or Nitro (speed and seconds) for the plan to time (user call, 2026-11-01); a kiter carrying
  Permafrost at base run speed, since it runs that once off the patch (read at 1.4 yd/s, Bulwark
  marked in his own tank patch spent the allowance 0.1 s in, F 1:22, H 6:06);
- patches: a latched tank patch; usable (`KitePatchRank`: heroic only a kite patch 16.75+ yd off every
  tank patch, since after a fail the spike restarts there and its first test reaches 11.5 plus its
  5.25 yd first walk; normal any); falling (fail reach 10.5, never an ending); the slow inside 7.5 yd
  (heroic 80%, normal 30%, none under Hand of Freedom);
- heroic phase 2's gather at 20 yd (3 yd slots, up to 6 off them, Impale's 7.5, 3.5 margin) and every
  living player off it at 10 (Impale's 7.5 plus 2.5): K 6:57 Assasin under a hand walked the spike
  over side tank Felesta, 6 Impales;
- the floor, a 45 yd disc round the centre (navprobe, bot filter `0x09`, 3 yd grid: on the mesh to 50,
  z 141.7-142.7, the low edge a slope at 290-310°);
- each ready Hand (`ReadyHands`, the held plan's paladin first so the escort doesn't swap), landing
  once its paladin can reach 30 yd of the catch point at 7 yd/s; the spare ones, heroic's first
  submerge keeping one back for the second (a 5 min cooldown against 140 s between them);
- the own immunity (Divine Shield, Ice Block, Vanish, Feign Death; bots only), never after a Hand for
  a paladin (Forbearance);
- the patch budget, normal adding its flying spheres, which its kite duty shoots.

**Candidates:** at 16 headings, straight to the 40 yd ring or the 45 yd rim and round it either way,
and arcs: 1.5 s straight, then turning −40, −24, −12, 0, 12, 24 or 40°/s, sliding along the rim the
way they turn (0 runs the wall, the guide's run). Each ends `none`, `own` 0.6 s before the catch,
`hand` 1 s before it (no catch counting under the Hand), or Hand then own. The 6 best shapes, and per
usable patch the 3 passing within 15 yd of it latest, also turn into a patch at each second: `in`,
`edge0`, `edge1` (the tangent of the 8.5 yd ring, the patch on the left or right, then 15 yd on) in
the last 6 s before the shape's catch or fail, `in` every 2 s back to 15 s; and `patch+hand`, the
Hand at the catch, then `in` at each second under it. With a boost ready, those same shapes run again
boosted from the first tick of their own run with the spike faster than the kiter's run and within
15 yd, with all their endings (`+boost@<s>` in `anub.kitepath`). The held plan's remainder, from its
point nearest the kiter, joins them, a ready boost past its tick firing at once. About 450-1,300
candidates, 1.2-3.5 ms at `-O2`, the boost adding up to 0.7.

**The spike's next leg** (heroic, gather up): after an own ending or a catch it runs on at its speed,
after a patch it restarts at base speed from where it failed once the 4 s idle is over, toward the
gather either way, as far as its life lets it. A tank patch or another usable kite patch within 11.5 yd
of that leg is a tank or kite hazard at the kite's end (2026-10-31 BM: two Vanishes handed a top speed
spike over side 1's tank patch, then its kite patch; BL: a restart from #577 spent #576 3.1 s in).

**Order**, the first difference wins, seconds in 1 s bands:

1. no tank patch failed on, else the later;
2. no kite patch spent unplanned or, heroic, before the spike has chased 12 s since its restart (user
   call: a raid pass ranks above it, the patch budget kept), else the later;
3. the spike never inside the gather's or a player's clearance (one it starts inside only has to get
   no closer than 0.5 yd past that), else the later;
4. no uncovered catch, then the smaller uncovered remainder: the life left after this kite (after a
   patch's 4 s idle the next kite starts fresh, else fast) against 15 s for the next kite (6 if fast)
   plus 19 s per patch and 10 s per spare Hand left;
5. the lower cost: own 1 with 6 s of the spike's life left or less after it, else 3 (user call,
   2026-11-01: it hands the spike on at its speed), kite patch 2 (normal 0.5; normal's tank patch 10
   while a burrower lives, since a held one off Permafrost submerges back to full), Hand 2, 3 past the
   spare ones, a boost fired by the end 0.5, so a boost goes only when it buys rows 1-4; a heroic
   kite never ends on a tank patch (2026-10-29: sides bare 81-87 s, phase 3 one side unpatched);
6. the later end;
7. an own or caught end farther from the gather, 5 yd bands, so the spike's next random target
   starts farther off;
8. the wider spike margin past the patches' reach and the gather's clearance, capped at 20 yd.

Plans count a Hand as ending 1 s early and Impale as reaching 8.5 yd, so a walk planned to the tick
still makes it; replayed without, a BJ walk ending 0.1 s after the Hand was caught. Impale doesn't hand
the spike on, so a patch it fails on within 3 s of an uncovered catch still counts: replayed without,
4 of 5 tank ends followed a predicted catch by a tank patch. A spike restarted
inside a patch's reach fails only after its test interval there (1.5, 0.8, 0.4 s by speed step:
65920/65922/65923's periods) or on coming back in, so it can be led out. A base-speed spike inside
Impale's reach of a kiter outrunning it counts no catch before its first 1.5 s test
(`BaseTestGraceS`): 2026-10-30 BA Druidica, marked 1.9 yd from it on Dash, read the walk due and spent
the only patch.

**The held plan stays** unless another beats it on rows 1-5, or matches it there and ends 1+ s later;
a turn past 90° waits until it is 1 s old. Re-picked freely, the two headings left at the rim swapped every tick (G 2:09
Mighty and H 6:16 Prayer stalled 5-6 s, Prayer to death; O 1:22 Hellflame reversed 10 times).

**A human kiter** gets no candidates: its motion since the last plan, held, judged once with a Hand,
so the claim, the escort and the dodge still read its catch.

Why (2026-10-31 BH, BI, BJ): the lane step and ring commit before it judged the spike's pass only
against tank patches, so over BF-BJ 13 of 39 kites ended 1-9 s in on a patch nobody turned to (9 kite,
4 tank), the kite patches 8-22 yd off the centre, where a base-speed spike cuts inside the 40 yd ring.
Replayed closed loop at 0.3 s (nativetest), BH, BI and BJ 1:22 end on a kite patch 14.2, 14.5 and
13.7 s in, no raid pass (BJ under a Hand from 4.6 s); they spent one at 7.9, 8.7 and 4.6. Over the 49
25H traces on disk (`--kitesim`, no Hands or own immunities), of 161 fresh-spike kites the real ones
ended on a patch under 10 s 29 times and passed a tank patch 46 and the raid 95; the replay 2, 2
(both from starts where every path passed a tank patch) and 15, its patch ends 13.2 s or later
(median 14.7), 114 ending caught for want of a Hand.

Baseline 2026-10-31 late, 25H on that planner (BK 649_2_anub-arak_1793481371 wipe P3 6:58 18.7%, BL
…481941 wipe P3 8:39 14.6% after a third submerge, BM …482612 reset P1c 6:00 46.3% with both side 1
tank patches lost, BN …483135 wipe P3 7:35 15.4%): fresh kites ended on a kite patch 13.5-17.3 s in,
`--kitesim` matching (BK median 14.5 against 14.3), no bot kite passing a tank patch, patch ends under
10 s 1, 2, 0, 0. Own immunities ended 8 kites at 13-18 s, 6 with 15+ s of life left; speed boosts went
3 times on base-speed spikes; 13 kites planned a Hand, 2 landed; main tanks died 26-51 s into phase 3.

**The move** (`anub.kite`): the plan's track 1.2 s past its point nearest the kiter, about 8 yd,
floor-checked: `ring` running, `patch` once the patch walk began, `hold` within 2 yd of its end, which
returns false so a healer or caster keeps working. Normal's `kite` duty drops a sphere while no non-tank
patch is down.

**Heroic stages** (`anub.kitestage`), per instance per ms:

- `patch` once the plan's walk began, latched while the plan still ends on a patch;
- `immune`: Hand of Protection up, still running; the generic `feign death` is zeroed for a marked
  bot;
- `run` otherwise;
- `boost:<spell>` over `run`: Sprint, Dash and Nitro at the plan's boost tick, or at the catch of a plan
  caught anyway, never on a spike no faster than the kiter (2026-10-31 BL Mighty, BL Agony, BM Angry
  spent Nitro on 2.8-3.4 yd/s spikes under the old 10 yd rule); Blink, the teleport and Feral Charge
  once the spike is within 15 yd and faster, or within 10. One
  that comes ready under a Hand: Sprint, Dash and Nitro (spike faster only) go once its time left is at most what puts
  the kiter 9 yd off as it ends (`BoostLeadS`, pure, nativetest, 0.5-2 s; Nitro at 2.5 yd about
  1.1 s), Blink and the teleport at 0.5 s (O 1:50 Stormweaver's Nitro went early): cast at any stage,
  the walk to a patch included; Sprint, Dash (heroic, a druid out of Cat Form takes it first in the
  same tick, its form dropped by aura since `RemoveShapeshift` leaves Tree of Life on, and `anubarak
  kite cat hold` blocks the class form actions while marked), Nitro Boosts (none while one of those
  runs, one speed aura type), Blink 20 yd the way it faces when that lands farther from the spike and
  the spike's lane to it clears (no trail leg), then Demonic Circle: Teleport last: the circle in
  reach and 15+ yd farther from the spike, no speed boost running, and the catch due (Impale's reach
  plus 0.6 s of closing); then, under the same gates, heroic druids' Feral Charge for the form onto
  the burrower or scarab 8-25 yd off and in sight gaining most, 15+ yd, on the spike, within 45 yd of
  the centre, off the tank patches, its lane clear. Teleported as soon as the circle lay behind, V
  Agony and 2026-10-29 X Hellflame ran Nitro across their own circle and landed 18-21 yd from the
  spike. A near teleport in combat no longer idles a bot 1-2 s (`PlayerbotAI::HandleTeleportAck`):
  both stood on the circle 1.3-2.0 s and died. Sprint's +70% outruns the spike's 11.4, so the chain
  moves on as each runs out. The kite casts them itself, so the kite veto never holds them.

`anubarak hand reserve` blocks the Spell family (off-GCD too), `anubarak heal penetrating cold` and
`anubarak interrupt shadow strike` for the planned or claimed paladin from 2.5 s before the catch: P
1:22 Power's, claimed +24.1, went out at +25.2.

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

**Non-kiters dodge the spike's path and hold the spot until it has passed.** The path is its
predicted track over 3 s, a point every 0.2 s, per instance per ms (`DodgeTrack`): chased along the
kite plan's track (a human's: its motion); nobody marked (the 4 s after a fail) or no plan yet,
straight on toward its victim or spline end (`MotionTrack`).
Inside 10 yd of the spike or 7 yd of the track, a bot takes the nearest spot within 30 yd 13 yd off
every track point (16 and 10 while the spike outruns the bot, to 19 and 12; 10 yd is 0.9 s at 11.4
yd/s, and 2026-10-26 dodges started 0.3-1.3 s before the hit), its gather slot's side on a tie;
heroic also keeps it 7.5 yd off every patch when it can,
since the 80% slow still beats the path. Impale is a 6 yd cone, and clearance has to sit past the
trigger radius ([pitfalls.md](../../engine/pitfalls.md)). The spot is held per bot, walked for up to
3 s (the trigger stays on until it first arrives) then parked within 2.5 yd, while the spike lives
and the mark stays as set (nobody between kites included), until the spot comes into danger or its
home (gather slot, else healer stand, else where it stood) and the walk back clear the track, for at
most 8 s. The Permafrost step-out takes no spot in danger. Read off the spike and its line to the
kiter now, dodges went ahead of the spike, let go 20 yd on, and the gather walked bots back into the
lane: 2026-10-28 R 1,555 and S 427 swaps between the two within 1.5 s, Hellflame (R 4:15-4:22)
flipping every 0.5 s to a scarab death. Replayed open loop on R and S, swaps 2,580/690 → 17/23, no
more seconds within 7.5 yd; 1.5-4 s horizons scored alike, 5 s worse. A walk in flight is left alone, since every `MoveTo` clears the
spline. While held, `anubarak spike dodge hold` blocks `reach melee` and `reach spell` by name, so a
battle res walk still runs, plus set behind, flank, formation, follow, flee, run away, random moves,
charges, Blink and Disengage. Letting go on arrival handed bots back to reach: 130 `reach spell` and
107 `reach melee` flips in the 2026-10-16 kill, and Trueshot died while `reach spell` walked him
back.

**Hand of Protection covers a kiter the spike closes on**, every difficulty, with no Hand, Divine
Shield, Ice Block or Forbearance on it: 1 s before the plan's catch ([above](#the-kite-planner)), the
spike within Impale's reach plus 1.5 s of its closing speed, a base-speed spike inside Impale's reach
of a boosted kiter counting only once it outlasts its next 1.5 s test (S 3:50: the submerge's last
Hand went 0.6 s into a kite with Nitro running), never while the kiter outruns the spike (2026-10-30
Hands landed on BD Ecoterrorist on Dash at 19.0 yd/s against 11.4, BA Deathsong on Nitro), and on a
heroic bot not while its speed boost is still ready and the plan fires it before the Hand lands (user
call, 2026-10-30: BD Angry's and BC Power's Nitro went 9.1 s after their Hands, BC Agony's 0.5 s after
and she died as it ran out; waiting for every boost and jump, BL 2:12 Stormweaver's claim came after
her Nitro and she was caught 0.4 s later). For a plan landing a Hand (not the kiter's own), the plan's paladin first, then the
standby escort; and once the plan has failed (no patch ending ahead, or
the spike inside 9 yd anyway, and no own immunity or own hand planned) any. The claimer is an alive unmarked paladin bot within 30 yd and in sight with the hand
ready (`ReadyInstant`; the cast waits out the GCD), in the **protector order**: neither tank nor
healer, a side tank with no burrower to hold or take, a healer, any other tank, by guid within each.
One claim per mark, kept until the mark moves; one 1.5 s old with no immunity on the kiter passes to
the next paladin, and one lapses, for the same paladin to retake, when the need goes. Two 2026-10-26
hands went out 0.2 s into a kite on a 3.5 yd/s spike. **The kiter's own**: its planned hand 1 s
before the catch, its planned immunity 0.6 s before or at once at 50% health (V Justice died 8.6 s
into an `own` kite to 11 Mandibles stacks from escorting, 17.3k a tick), a non-paladin's behind its
Hand with 0.8 s of the hand left (cast after, Impale's next tick can land first: P Nightwarrior), or
0.6 s before the catch as the failed plan's last resort with no claim. Only Hand of Protection keeps
the spike on it; the others, like a death, hand it on at its current speed, so a paladin takes Divine
Shield only with no patch ending ahead and the emerge over 10 s off, or for an `own` plan.

**Immunities wait for the kites** (`anubarak kite immunity reserve`): heroic phase 2 blocks the
class `divine shield`, `ice block`, `vanish` and `feign death` for an unmarked bot; the kite casts
its own directly. `anubarak kite sprint reserve` blocks the class `sprint` and `dash` for an unmarked
bot from the pull to phase 3: their cooldowns outlast a phase 1, and the rogues spent Sprint every phase
(V 6:26 in P2c). In 2026-10-27 I Holylight's generic Divine Shield at 6:07 left her Forbearance and
no shield for her kite at 6:26, dead in 21.9 s; Justice's at 8:28, Smartface's Ice Block and
Bulwark's Divine Shield were spent the same way. A bot marked mid-cast stops it: the AI waits out a
cast before any action, and Prayer stood 2 s at I 6:06. Holylight's
Divine Shield 7 yd from the gather in a 2026-10-26 wipe handed an 11.4 yd/s spike into it: 12
Impales, 5 dead in 0.8 s. Impale is physical, and 5 of the 6 kites after the patches ran out took
Impales in the 2026-10-16 kill.

**A paladin escorts a kite whose plan lands its Hand** (heroic, user call). In the 2026-10-26 late
wipes the paladins stood at the gather while such kiters ran the far side of the 40 yd ring, 31-43 yd
off, and died with no hand; in 2026-10-27 a ready paladin was within 30 yd at the 12 yd mark in only
7 of 14 (J) and 10 of 19 (I) kites. The **escort** is the plan's paladin while its Hand hasn't landed
(the held plan's kept first), none for the kiter's own hand, so it is the one the claim picks. Until
the plan's catch is 4 s off it **shadows** the kiter: on the line from the room centre to the kiter, 20 yd
short of it (25 lagged past 30 in 2026-10-28 U Trueshot 34, Felesta 35-38) (the centre when nearer), turned 15° and 30° either way round the centre until 15 yd off
the spike (19 while the spike outruns the paladin: 15 sits inside the dodge's 16, and Q Holylight
swapped walk and dodge every 0.2-0.6 s to 35 yd off), the dodge lane
clearance off its line to the kiter, 29 yd of the kiter, 8.5 yd off every patch and on the floor
(inside the centre's navprobe-clean 50 yd); none clear, as with the spike crossing the centre (L 8:29),
points 15, 20 and 25 yd round the kiter at 30° steps, the nearest the paladin first, on the same
tests; none of those, no escort. From 4 s it waits at a **post** (`anub.escort` `post`;
`EscortPosts`, pure, nativetest): 15, 20 or 25 yd round the plan's catch point at 30° steps,
within 45 yd of the centre, 8.5 off every patch, 15 (19) off the spike's simulated track and 9 off its
lane to the catch, on the paladin's side of the spike's line to the kiter, the nearest the paladin
first, on the floor; kept until the catch point drifts 6 yd or the spike closes on it; none, the
shadow. It walks round any patch (`GetAnubarakPatchSafeLeg`): straight, U Justice crossed side 1's,
crawled 7 s at 80% slow with his step-out and escort moves refusing each other, and Angry died 44 yd off
with two Hands ready. Read earlier, the catch point swings 30-60 yd with each heading: replayed, posts alone had
the escort in reach at the hand 83% of the time, committing at 4 s 90% (3-6 s tried). Over 4 yd off
it walks there, a
walk headed within 3 yd left running. The gather and the side tank nodes skip it, and `anubarak hop
escort hold` blocks `reach melee`, `reach spell`, `reach party member to heal`, `anubarak melee
burrower stand`, set behind, flank, formation, follow and charges. It ends with the hand, the mark, the emerge or the plan's re-pick.
**A standby shadows every other bot kite** (user call, 2026-11-01; `anub.escort` `standby`): while the
plan lands no Hand but the kiter's own, the nearest Hand-ready, alive, unmarked paladin bot other than
the kiter, not a side tank holding a burrower, kept for the kite while it qualifies, walks the shadow;
none on a kiter with Forbearance. The planner offers its Hand first and the claim takes it first; once
the plan lands a Hand, that paladin escorts as above. With the escort only for Hand plans, the Hand
flickered in and out of the plan (BN Felesta 1:22 9 flips) and escorts stood 34-71 yd off at the claim:
13 kites planned one in BK-BN, 2 landed.
**A boosted kiter is met at its patch** (user call, 2026-10-30; `anub.escort` `patch`, `PatchPost`),
since it outruns the escort (V Agony 23 → 62 yd in 2 s, U Mighty 29 → 37). While a bot kiter with a
kite patch has a speed boost ready or running, the escort waits 15, 17.5 or 20 yd round the patch at
30° steps, off the spike by the shadow's clearance and the spike's and kiter's lines to the patch by
the dodge lane's, within 45 yd of the centre, 8.5 off every patch, the nearest the paladin first, on
the floor; latched to the kite's end, boosts spent or not, while the spike stays off it. None clear,
the shadow and post above.

**Hand of Freedom frees a kiter marked in Permafrost**, every difficulty: itself when a paladin bot,
else the first paladin bot within 30 yd and in sight in the protector order, never one with a Hand of
its own on the kiter, holding the protection claim, or the plan's paladin within 3 s of its hand
(`anub.freedom` `cast`). The generic `hand of
freedom on party` already skips a target with the caster's Hand on it; `HasAnyPaladinHandFromCaster`
checked only Hand of Protection rank 1, so it now lists all three.

**Scarabs** (user call: spread the stacks). DPS take them while he is down (branches 2 and 8 of the
DPS target above), and leftovers ahead of him once he is up (branch 5). Heroic phase 2's pickup tank
takes none off others, so it surfaces clean, and holds its gather edge hitting any on itself; a free
side tank, unmarked and not the escort, peels one only off a healer, as heroic side tanks do while he
is up. Normal phase 2 keeps the pickup tank on them.

**Redirects go to the tanks that need the threat** (`GetAnubarakRedirect`). In the 2026-10-26 late
wipes every Misdirection went to the main tank (most cast with the hunter on a burrower) and 12-19 of
each pull's 18-27 Tricks to a melee DPS, nothing to a side tank, while 22-26 burrowers a pull opened
on a non-tank (the nearest at the spawn is ranged or a pet, 28-46 yd off the exits). `anubarak
redirect guard` blocks the class `misdirection on main tank`, `tricks of the trade` and `tricks of the
trade on main tank` all fight. In the pull's first 10 s the first hunter and the first rogue bot in
group order redirect to the main tank (user call), Misdirection's shots dumped into him; the others
keep theirs for wave 1. Otherwise each selectable burrower not on the usable bot side tank owning it
(`anub.bside`), by guid, takes one hunter in group order (the pull's aside) whose Misdirection goes
to that tank and Steady Shots into it; a rogue Tricks that tank only while hitting that burrower and
within 20 yd, since Tricks moves only the threat of what the rogue hits.

**Shadow Strike** gets one interrupter per casting burrower, derived once per instance per ms, since
25H's four a wave overlap their 8 s casts. Each goes to its victim if unmarked with an interrupt
ready, else to the lowest-guid alive unmarked group member with one ready within 30 yd; nobody holds
two casts. First castable of kick, pummel, shield bash, counterspell, wind shear, mind
freeze, hammer of justice, holy wrath (burrowers are undead on every difficulty), shockwave,
concussion blow, bash, war stomp, shadowfury: -207 blocks silence, not interrupts or stuns, so no
Silencing Shot or Spell Lock. The guide uses AoE stuns (Shockwave, Holy Wrath), warriors and
paladins. Out of reach, it walks to the spell's DBC range less 2 yd with line of sight, or to melee
for a melee or self-range one (War Stomp, Shockwave, Holy Wrath).

**Phase 3.** The main tank chains `NextTankDefensive`. Leeching Swarm heals him by what lands, after
resists and absorbs, so every heal on a healthy non-tank goes back to him: in 2026-10-30 AA healers put
1.98M into non-tanks and 0.96M into the three tanks, who all died. Heroic (user calls, 2026-10-30, guide):

- **DPS** kill a wave up since `Swarm` like phase 1's (user call, 2026-10-31; guide: phase 3 "is
  identical to Phase 1") unless he would die within 10 s (`LeftToSideTank`, `anub.p3wave`): his health
  over its net loss a second across the last 10 s, sampled once a second, read as a wave comes up and
  every 2 s while it lives, so a wave left alone becomes the focus once his loss stalls. Left, it is no
  focus, loose or other-side pick and stays on its side tank. In BH wave 7, left by the old rule,
  killed both side tanks 8-12 s in with 8.3M (about 44 s of damage) on him; a wave of four is 3.0M,
  about 15 s. In AA 35-48% of phase 3 damage went on burrowers.
- **Burst** (`ToCAnubarakBurstWindow`): in `Swarm` lust and the rest open once those burrowers are dead,
  15 s at most (AA's Heroism spent 21 of its 40 s on wave 6). Before it the rest wait from his 40%,
  released while he is behind: at his health loss since he came up (`surfacedHpPct`), 30% falls later
  than 5 s before the submerge.
- **At risk** (`NextColdTick`): a Penetrating Cold carrier whose health plus frost absorbs at its next
  tick, after the swarm ticks before it (30% of current health each, 250 minimum, absorbs first),
  leaves 250 or less after that tick (`GetAmount()` × its frost damage taken, 0 when immune). No tick
  left before the aura ends is no risk. Non-tanks sit at 1-4k, so a fresh carrier dies at its first
  tick (3 s after landing) unless lifted; a shield survives the swarm, a heal mostly drains.
- **Duty** (`anub.healduty`, latched as `Swarm` starts): priests and druids `cold`, paladins and
  shamans `tank` (the guide's split); with 2+ healers all on one side, the lowest guid goes over. A
  healer back from a res takes its class duty.
- **Assignment** (`anub.coldheal`, healer → carrier, once per instance per ms): each carrier at risk,
  earliest tick then least left first, to the nearest free `cold` healer in heal range and sight; a
  healer at risk to itself. Kept until the carrier is safe, the aura ends or either dies.
- **Healing** (`GetAnubarakSwarmHealPick`), every healer bot, the first row with someone in heal reach
  and sight:
  1. itself at risk (`self`);
  2. `cold`: its assigned carrier (`risk`), then a tank under 35% (50% with no `tank` healer alive),
     then with no carrier assigned a tank under 90% when row 3 has nobody under 10% and no bare
     carrier (user call, 2026-11-01: two tank healers fell behind three tanks, 7.6-13.3k/s taken
     against 6.5-11.7k/s healed, Prayer and Tree giving the tanks 52-153k a phase);
     `tank`: a tank under 50%, then the most urgent carrier at risk no assigned healer reaches (`risk`);
  3. the lowest in the first of: non-tank under 10% (the swarm alone takes about 7 s), a carrier this
     healer can cover (`bare`), tank under 90%, carrier under 50%.

  Rows 1-2 at `ACTION_CRITICAL_HEAL + 12`, over every class heal, the rest at `ACTION_MEDIUM_HEAL +
  0.75`, under the mana tools. A carrier at risk gets the **rescue** (`CastAnubarakColdRescue`), the
  first `CanCastSpell` allows: on itself divine shield, divine protection, barkskin, pain suppression
  only under Weakened Soul; power word: shield (no Weakened Soul); holy shock, riptide, swiftmend (on
  Rejuvenation or Regrowth); the first of flash of light, flash heal, lesser healing wave, nourish,
  regrowth, holy light, greater heal, healing wave whose cast time + 0.3 s lands before the tick; else
  the cover. Otherwise a priest shields a tank first; a bare carrier gets the **cover**, the first of
  power word: shield (no Weakened Soul), renew, rejuvenation, lifebloom and riptide, none from a paladin
  (Sacred Shield stays on the tank). Then `CastRaidThroughputHeal`, never Wild Growth,
  and for the non-tank no Holy Light, Greater Heal, Healing Wave, Healing Touch or Nourish. `anubarak
  swarm group heal guard` blocks Chain Heal, Prayer of Healing, Circle of Healing, Wild Growth (both
  nodes), Divine Hymn and Tranquility (only Tranquility is a `CastAoeHealSpellAction`), and
  `AnubarakSwarmHealMultiplier` zeroes, for every bot, an area heal and a heal on another player who is
  no tank, no carrier and at 10%+. In 2026-10-30 BD Tree carried Penetrating Cold 12.4 of 15 s bare,
  cast 14 of 16 Nourishes on others and died, and Prayer died with no Renew and no shield on itself:
  `CastRaidThroughputHeal` tries HoTs last, and a carrier at 30-50% ranked under every tank under 90%.
  In BG (below) the old pick killed the healers: all four herded on one carrier, tanks under 50%
  outranked carriers at 4k, and Holylight (4.3k) never healed herself before her first tick.
- **Aura Mastery** (user call, guide): `anubarak aura mastery reserve` blocks the class node all
  heroic pull (it fired on any group damage, BG +4.5 s on Concentration). `anubarak aura mastery`
  (`ACTION_CRITICAL_HEAL + 13`, off the GCD) casts it for a paladin in phase 3 holding its own Frost
  Resistance Aura, when a Penetrating Cold round landed under 1.5 s ago and 2+ of its carriers are
  under the tick (`anub.mastery`); its 6 s cover the first two ticks. Frost goes to that paladin only
  in phase 3 (`frostWantsMastery`, [README](README.md)): before, Concentration Aura matters (user call),
  and in phase 3 nothing pushes casts back (66240 is `SPELL_ATTR3_TREAT_AS_PERIODIC`, `Unit.cpp:1303`).
  A swap only for Aura Mastery would cost two 1.5 s aura GCDs.
- **Healers stand behind the tank patches** (user call; `anub.hbehind`): heroic keeps spawning
  burrowers in phase 3 (`boss_anubarak_trial.cpp:263-264`), and they go for the healers. Each healer
  bot by guid takes a stand 10-24 yd behind the tank patches' midpoint, away from the centroid of the
  four tunnel exits ((740.2, 193.4), (694.5, 185.4), (732.0, 83.4), (694.9, 102.5), off the traces), up
  to 12 yd sideways: the most exit lines through a patch's 7.5 yd slow first, where a burrower crawls
  at 20% and its side tank takes it, then the nearest the main tank; 3 yd apart, 8.5 off every patch,
  within 36 yd of the main and both side tanks, on the floor (navprobe: the floor runs about 70 yd east
  of the centre, past the east pair at x 786-794). From his 35% in the last phase 1 they walk one at a
  time, healers being unable to cast on the move: the next once every earlier one stands within 6 yd of
  its own, none while a tank is under 50% (`wait`). A stand out of reach re-assigns them all; none, the
  usual stand (`none`).
- **Mana before phase 3** (user call, 2026-11-01): Holylight came in at 46-62% and was at 6-23% 45 s
  later, Elemena 55% to 3-11%, the class Divine Plea firing twice in 7 min. At `ACTION_MEDIUM_HEAL + 6`,
  heroic, unmarked: `anubarak divine plea` for a healer paladin bot under 80% mana, not in `Swarm`, no
  tank or carrier under 50% (it halves healing 15 s); `anubarak innervate healer` (every phase) for a
  druid bot, Innervate ready, on the healer in reach and sight with the least mana but itself, under
  50% and under its own, from caster form. `anubarak self innervate guard` blocks the class
  `innervate` while another healer has less mana: it blocks the class `innervate on healer` too, which
  casts under that name.
- The guide's Fortitude and Commanding Shout cancel is skipped (user call): about 0.2% of his health,
  once.

Otherwise healers lead with Penetrating Cold carriers below 90%, or bare at any health with the cover
first, in heal range and line of sight (guide: "1-3 healers assigned solely to" it), lust waits for `Swarm`, latched per instance, and
personal burst is unrestricted.

## Known gaps

- A ready boost fires at the first tick within 15 yd of a faster spike; later can outlast more of a
  short life (nativetest: a 10 s life, a top speed spike 25 yd off, caught at 9.4 s boosted at 1.8 s,
  free boosted at 2.5).
- The next leg assumes the spike's next target is at the gather; one off it, or a restart the next
  kiter steers, takes another path.
- A mid-life immunity (3) still beats patch+Hand (2+2): from BI's 1:22 start, whose one clean patch
  ending sends the restart over the other kite patch, an immunity kiter ends at 8.9 s.
- Next-leg hazards rank like the kite's own, above a raid pass and a catch: replayed over the 53 25H
  traces, 2 fresh kites that ended on a patch end caught instead, BM 3:44 among them (its patch's
  restart crossed side 1's tank patch, which the real pull lost at 4:02).
- A standby paladin is off the gather: a healer one heals the scarab targets less, a DPS one hits less.

- Untargeted raid AoE still pops spheres, all of it in normal (Starfall made every patch in the
  2026-10-16 kill), all but Starfall in heroic.
- A melee bot capped on him, or on a burrower other than its side's focus, keeps white-swinging, so it
  can still climb past that tank.
- A side tank's area spell outside the hit-tested list holds only near the other side's burrowers,
  never for him.
- A human kiter or tank reads none of this: no protection claim waits on a human paladin, a human
  kiter gets no boost stage, and the drag waits while a human holds him.
- A human's Divine Shield, Ice Block, Feign Death or Vanish hands the spike on at its current speed,
  wherever the human stands.
- A third submerge has no kite sphere: its kites live on boosts, Hands and immunities, and the boss
  stood at 35-47% at the third submerge of the 2026-10-29 and -30 wipes.
- Scarabs on a lone kiter, and a hunter opening on a kite sphere with Hunter's Mark, are unhandled.
- With 4 healers (Z) a side tank holding a burrower goes uncovered while two follow the main tank.
- A bear tank kiting in Cat Form has cat armour against scarabs, and a Feral Charge can land among bugs
  that turn on it.
- The east tank patches sit on the 40-45 yd kite ring, so runs arc through the west and centre and
  turn back; heroic's long drag moves him through much of each phase 1's second half.
- The gather slots sit 18-21 yd off the kite ring's centre, where a base-speed spike settles in pure
  pursuit (20 yd), so it crosses the gather each lap and the raid steps aside.
- Replayed, the first tank patch still goes about 11 s into phase 2 (6 of 34 first marks at 25+ s),
  and about 12 run spends in 105 marks remain: marks starting next to a patch, and pure pursuit
  settling inside the ring (20 yd at base speed, 35 at the second step) and cutting across one.
- 10H with one side tank puts both burrowers of a wave on one patch, inside Spider Frenzy's 12 yd.
- A grip stepped out 16 yd, or a far grip, can leave the patches 40+ yd apart; past about 36 a melee
  stand between him and a rim burrower no longer reaches both.
- A human's generic Divine Shield, Ice Block, Vanish or Feign Death in phase 2 is not held back.
- The kite planner reads bystanders where they stand, not where their dodge takes them; its coverage
  counts every spare Hand at 10 s whatever its paladin's reach; normal's raid is all bystanders.
- After a fail on one tank patch, a spike re-marking the other side's tank, still holding burrowers
  on its patch, spends that one too (about 1 in 24 per spent patch).
- A spent tank patch stays bare until he emerges, then waits on a sphere in reach.
- The Demonic Circle goes down where the warlock stands, its staging stand only before the pull.
- An escort or kiter collects scarabs while it runs (V Justice, 11 Mandibles stacks).
- Phase 1 immunities, Avenging Wrath (no Divine Shield for 30 s) and Divine Protection's Forbearance
  aren't held for the next submerge; a burrower held 7+ yd off its patch in phase 2 submerges back to
  full (U #472 at 42%).
- Phase 3 tank healing: in V the healers came in at 8-59% mana and the main tank, cooldowns spent in
  P1d, went 82 → 27% in 3 s at 15.7%.
- A gathered ranged bot whose burrower is out of spell range idles: the gather blocks `reach spell`.
- The direct-heal list is a copy of `JaraxxusHealIncinerateTargetAction`'s with "lesser healing
  wave" for "greater healing wave", not a 3.3.5 spell; one in `Shared` would keep them in step.
- A heroic dodge with no spot clear of Permafrost lands in its 80% slow.
- A side tank's rim (7 yd) is inside the 7.5 yd slow, so on heroic it carries the 80% slow most of the
  time it's up (AA 329 s): Bulwark crawled as AA's wave 7 landed.
- Healer threat pulls a new burrower off its side tank 3-4 s in (AA: Elemena 9, Holylight 3; wave 7's
  at +3.6 s).
- Judgement of Light comes only from the holy paladin's rotation (AA: 8 casts).
- The 40% burst hold kept 7 of AA's 42 last-phase-1 casts: the cluster (Icy Veins, Arcane Power, Death
  Wish, Bestial Wrath, Killing Spree, Force of Nature) went at 40.0-42.6% as wave 6 landed; 45% keeps 19.
- Sphere and Shadow Strike duty are memoised for one group per instance per ms, kiter protection per
  instance per ms; two groups in one instance recompute the duties on every switch and share the
  first reader's protection claim. Side tanks are latched per instance.
- While he is down, a DPS bot with no scarab within 30 yd falls back to `dps assist`, which can take
  a burrower off Permafrost.
- A human marked by his own tank patch spends it (H 1:22 Deathsong); nothing steers a human kiter.
- The human main tank's submerge spot sets where the spike starts: BJ's put a kite patch on the first
  spike's line, spent 4.6 s in.
- Justice holds frost in phase 3, so Holylight's Aura Mastery never covered BH's second deadly
  Penetrating Cold round.
- BA's phase 3 opened 7 s after wave 7 landed: those burrowers took 41 s with him at 30%, lust at +50 s
  (the rule kept by user call).
- A human main tank off the anchor: in BA P1c 17 yd off, ranged put 54-61% on burrowers and he lost
  19.5%, so a third submerge; in BB he went down 9.6 yd from tank patch 1, in BG 17.8 yd.
- A rogue's Tricks waits until it hits a loose burrower, which its DPS pick rarely is.
- Side tanks die to two frenzied burrowers at 9 Expose Weakness stacks (+225%, 5+ stacks 17-34 s of
  each 2026-10-26 wave), and most burrower submerges followed a side tank's death; no healer is
  assigned to a side tank.
- The kite ring (40 yd round the centre) runs past the west refill pair at 36-43 yd.
- A human's Army of the Dead ghouls still taunt burrowers off the patches (one submerged at 0:39 in a
  2026-10-26 wipe); only bots are blocked.
- A healer's heal threat still pulls burrowers off a side tank as the taunt's 3 s run out (Elemena
  11-12 times a pull in the 2026-10-27 evening wipes; BG 21 pull-offs, Elemena 15, most 3-4.6 s after
  a wave landed).
- Heroic boss health runs at 1.3× through the server's override; the real log's 37k healing on him is
  unexplained.
- A carrier further than 40 yd from the frost holder gets no Frost Resistance Aura, and Aura Mastery
  counts it anyway.
- A far grip with one DK bot, or with the side's DK tank's grip spent, has no relay.
- Not handled: the phase 3 heal hold and stamina-buff cancel, Freezing Slash defensives, external
  cooldowns on side tanks, scarab threat, normal's phase 2 raid positioning.

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
| `dside` | A DPS bot's burrower side, 0 or 1, keyed on the bot, re-split at each wave |
| `grip` | 1 a grip went out this pull, 2 while a side waits on a failed one. Per instance |
| `spot0`, `spot1` | Each side's tank spot, `x,y,z`: its latched patch, else the open spot. Per instance |
| `allow` | Traces before 2026-10-29: heroic tank patches the submerge's kites could spend, 0-2. Per instance |
| `sphere` | Sphere duty: `kite` and `stock` (normal), `0` or `1` (a side), `grip0`/`grip1` (a DK's grip for a side), `none` |
| `gripstep` | The DK's grip: `walk`, `hold`, `cast`, `wait` (sphere in the air, or a relay link still to grip it), `kill`, `post` (a relay link to or on its post), `none` |
| `relay` | A relay link DK: `wait` (posted), `cast` (gripped), `miss` (its grip pulled nothing), `drop` (relay dropped), `none` (no grip duty) |
| `sidetank` | `hold`, `pickup`, `ready`, `gather` (heroic phase 2, nothing to hold) |
| `saoe` | A side tank's area threat: the spell, else `cap` (it would hit him over the cap), `other` (the other side's burrowers), `none` |
| `leave` | `walk` while a non-tank steps out of Permafrost, `none` |
| `mstand` | `walk` while a melee bot walks to its burrower stand, `none` |
| `kitestage` | Heroic kiter: `run`, `boost:<spell>` (Nitro Boosts as 55004), `immune`, `patch`, `none` |
| `kiteplan` | Bot kiter's planned ending: `patch`, `patch+hand`, `hand`, `own`, `none`; can change within a mark |
| `kitemove` | Bot kiter's patch move: `in`, `edge0` (patch on its left), `edge1`, `none` |
| `kitepath` | Bot kiter at each new pick (a held plan writes none): `ring:<deg>:<ccw\|cw>:<40\|45>`, `arc:<deg>:<deg/s>`; then `>patch:<sphere guid>@<turn s>:<move>`, `+hand`, `+own`, `+boost@<s>`; `none` once the mark moves |
| `kitepred` | Bot kiter at each new pick: the first of `tank`, `kite`, `raid` (hazards), `catch`, else the ending, `:<s from the pick>`; `none` once the mark moves |
| `p3wave` | Heroic phase 3 wave read: `kill:<ttd s>` or `leave:<ttd s>` (his time to die, -1 while he isn't losing health) |
| `hands` | Bot kiter at the mark and each change of planned ending: every paladin bot as `guid:Hand cooldown s:yd to the kiter`, `none` |
| `second` | Traces before 2026-10-29: heroic kiter `grant` (a second tank patch), `none` once the mark moves |
| `lane`, `bridge` | Older traces: the heroic lane step (`flags:degrees`, `ring:<way>`/`ring:<exit>`) and a `patch+hand` kite's Hand (`hand`, `turn`, `ask`) |
| `gather` | Heroic phase 2: `walk`, `hold`, `wait` (the way to the slot crosses the spike's path) |
| `hstand` | A healer bot's stand while he is up: `walk`, `hold`, `none` |
| `hbehind` | Heroic healer bot from his 35% in the last phase 1: `go` (to its stand behind the tank patches), `wait` (an earlier healer still walking, or a tank under 50%), `none` (no stand) |
| `kite` | The kiter's branch: `ring` (running the plan), `patch` (its patch walk), `hold` (at its end); older traces `detour`; `none` when there's no spike or the mark moved on |
| `dodge` | A new dodge spot's reason: `spike` (within 10 yd of it), `lane` (on its predicted path), `none` (no clear spot) |
| `protect` | `hop` while this paladin holds the kiter's claim, `self` while the marked bot needs its own immunity, `none` |
| `escort` | Heroic escort paladin: `walk`, `hold` (shadowing), `post` (at or to its post by the catch), `patch` (waiting by a boosted kiter's patch), `standby` (shadowing a kite with no other paladin's Hand), `none` |
| `freedom` | `cast` while this paladin owes the kiter Hand of Freedom, `none` |
| `interrupter` | `1` while the bot holds Shadow Strike duty, `0` once the cast ends |
| `pickup` | The pickup tank's step: `taunt` (off a non-tank), `attack`, `stepout` (out of Permafrost), `hold` (no leg), `drag` (a leg to the anchor), `submerge` (a leg to the submerge spot), `prestep` (the step round him before a leg), `subhold` (submerge window, no leg to the spot or round a patch) |
| `subspot` | The pickup tank at each submerge-spot search: `x,y,z rN` (N the patch clearance found with) or `none` |
| `defensive` | What a side tank or the Swarm tank cast, `covered` (one running) or `none` |
| `burst` | Heroic burst gate: `hold` (40% or under), `behind` (released), `wait` (phase 3, its burrowers up), `open` |
| `swarmheal` | Heroic phase 3 healer's pick: `self`, `risk` (a carrier at risk), `tank`, `low`, `bare` (a carrier it can cover), `cold`, `none` |
| `healduty` | Heroic phase 3 healer bot's duty: 1 `cold`, 2 `tank` |
| `coldheal` | Heroic phase 3 healer bot's carrier at risk, its own guid when itself, 0 when none |
| `mastery` | Heroic phase 3 paladin bot: `cast`, `hold N` (a fresh round, N carriers under the tick), `none` |
| `focus0`, `focus1` | Each side's focus burrower, 0 when none; kept current under a skull. Per instance. Traces before 2026-10-27 carry one raid-wide `focus` and `focusdue` |
| `dps` | A DPS bot's pick: `skull`, `loose`, `burrower`, `boss`, `capped` (him, a melee bot capped on its focus), `scarab`, `none` (a sphere shooter too) |

The marked player is the 67574 `aura` stream, so no key repeats it.

The per-bot keys are change only: a bot that takes the same branch twice writes one row.

`tools/botobs/bosses/anub_arak.py` reads the rest, keying him on entry 34564 since the spike shares
his name and boss flag, and checking map 649 since Azjol-Nerub's boss shares the `anub-arak` slug.
`--phases` (spans, his health at each edge, deaths per phase), `--pull` (who stood west of the Web
Door or above z 200 at the pull and when each got in, stage 9 to the pull, the player nearest his wait
point and each bot tank's yards at the engage, one inside 23 yd flagged), `--spike` (each 67574
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
rows per bot), `--swarm` (phase 3 length, 66240 per bot, lust against the phase 3 start; per 5 s his
health, the median tank and non-tank health, the swarm and the healers' heals onto tanks, carriers and
others; bot heals cast onto a non-tank with no Penetrating Cold at 10%+; DPS samples on him and on
burrowers up at its start or newer; those burrowers' last death against lust and `burst` `open`; bot
burst cast from his 40% to phase 3, flagged unless `behind`; Penetrating Cold windows and heals onto the
carriers, `defensive`, deaths). Also: `--burrowers` each burrower's
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
absorbs in the 12 s before, each tank's defensives from its cast rows (`anub.defensive` writes only
on a change). Then: `--spike` per kite a walk flip-flop (move targets 5+ yd apart reversing 3+
times in 2 s), the nearest Hand of Protection-ready paladin when the spike first came within 12 yd
(ready: a bot, alive, unmarked, its Hand cooldown off `anub.hands`, else 300 s since its last, no
Forbearance on the kiter; past 30 yd
flagged), Feign Death dropped within 100 ms; `--scarabs` scarabs alive at each emerge +0/10/20/30 s
with targets, the main tank's Mandibles stacks then, lives per phase, Mandibles damage and peak stacks
per player per phase; `--redirects` each Misdirection and Tricks with caster, recipient, the caster's
target and a burrower's target 1-8 s on; `--burrowers` each opener (the encounter's own creatures
skipped) and the three players nearest its exit at the spawn; `--pursuit` pure pursuit replayed per
kite at 3 and 6 s, end and patch-pass error, flagged past a 1.5 yd p90. Late baseline, the
2026-10-26 25H wipes F, G, H: Misdirection 18/17/28 all to the main tank, Tricks 22/18/27 with
19/12/19 to melee DPS; openers on a non-tank 23/24, 22/24, 26/32; main tank Mandibles stacks at the
emerges 11, 4 / 9, 13 / 0, 10, 7; raid Mandibles a phase 1 443-829k; pursuit patch-pass p90 0.3-0.4;
flip-flops G Mighty 2:09, H Prayer 6:16 (and four short ones); paladin past 30 yd at 3 of 5, 2 of 3,
1 of 5 kites. Then: `--dps` per phase and role (bots) the share by target kind, moving, yards a bot
split by the `by` of the last `move` row within 4 s (steps under 0.3 or over 30 yd left out), damage
per second by target kind; his health lost per phase 1, under 20% flagged; `dps` rows per role; each
`dside` group per wave; melee walks across the midline between `spot0` and `spot1` (5 yd either side
counts as neither), into the other group's half flagged `astray`. `--burrowers` reads `focus0`/
`focus1` with a side column (older traces `focus`, side `-`), the share on a focus over its side's
group, a cast off focus only when neither side's focus held it, and says when `focusdue` is absent.
`--spike` per kite its `kiteplan` terminals and `used` (a tank patch spent for `patch`, the hand on
the kiter for `hand`, both for `patch+hand`, its own immunity for `own`), a value carried from the
last kiter counting only with no row of its own within 1 s of the mark; tallies and a kiter dead
with its terminal unused. `--spike` per patch kite its turn (the first `bridge` row, else the first
`patch` stage, `immune` for `patch+hand`), the `kitemove` then, the patch, and whether the spike
failed on it; per `patch+hand` the Hand at the turn, turned without, or asked and never landed
(older traces: Hand of Protection casts within 1.5 s of the turn); the spike's first pass within 14.5
of a patch before the turn, a patch it spawned inside counting once it leaves; Impale on a side tank.
`--burrowers` each burrower pulled off its side tank by a non-tank within 15 s of coming up, with the
seconds it had held and the puller's casts on it in the 3 s before, triggered ones marked `*`. `--tanks` each run of the main
tank's Expose Weakness: its peak, the burrowers on him at a stack (the shared aura's row names only
the first caster), his area casts from 5 s before. `--threat` per window the longest stretch 10+ yd off the anchor with no
drag leg, cut at the first submerge leg, 10 s or more listed. `--spike` per `patch+hand` kite its
Hand against the catch the reader predicts (the kiter on its heading at its measured speed, then the
ring; spike steps from its last restart), the spike's speed and distance
then, whether the patch followed under it; a `hand` row with no `ask` before it is the Hand at the
catch, its turn the first `patch` stage after; after a `turn` the nearest ready paladin; per submerge
the seconds into phase 2 when its first tank patch went (its `patchN` latch let go). `--spheres` per
far grip each link (DK, cast, landing, distance to the spot before and after) and `relay` rows; a
Death Grip whose 49576 triggered no 49560 from its caster within 5 ms flagged `no pull`, tallied per
caster. `--spike` per kite the escort's distance at the claim, at the kiter's boost before any Hand and at
the end, its seconds in Permafrost, its walk and dodge swaps within 1 s, a kiter dead to anything but the
spike with its own unused,
claim to cast; then a kiter within 15.5 yd of a tank patch under a hand, an own after a hand
ran out, a boost in a hand's first 8 s, `second` grants, and per submerge the seconds kited and
idle, the terminals, the seconds after its first tank patch and kiter deaths; heroic, kite tools
(Sprint, Dash, Nitro, Divine Shield, Ice Block, Vanish, Feign Death) a bot spent off its mark before
phase 3 and still on its base cooldown as a submerge's spike came up, per phase; each Demonic Circle
teleport with the spike off the kiter and the circle (where it landed when the summon came before the
trace), the boost before it and its pass by the circle, and the seconds stood on landing; per submerge
his distance from the gather; Cat Form and Feral Charge among the boosts; per kite `kitepred` at the
mark against the real end, the `kitepath` picks with seconds held, a turn past 90° within 1 s of the
last flagged `reversal`, and the first predicted hazard against the first real one (the planner's
rules: a tank patch, a kite patch other than the pick's or under 12 s of spike age, the gather,
Impale or death); a pick carried from an earlier mark counts only with no row within 1 s of it. The
patch kite's turn reads `bridge` on older traces, else the first `patch` stage. `--burrowers` per wave
and side the seconds from coming up to its first `focus0`/`focus1` pick and the side tank's peak
Expose Weakness to the next wave; in phase 3 the `p3wave` rows (merged over bots), when the wave died
and each side tank's fate. `--kitesim` (25H) the planner ported (its `Rules` read from the header, a
missing one refused) replayed from each bot kite's start (spike step off its measured speed, patches
by kind off the latches, the gather as formed 10 s into the submerge, life from the submerge, 7 yd/s,
the world frozen at the start; no Hands, own immunities, speed boosts or bystanders; the next leg
ported), closed loop as the native
test's: per kite the first pick as `kitepred`, the replay's end and the real one, raid and tank
passes; for fresh-spike kites patch ends under 10 s, tank and raid passes. `--spheres`
heroic, per submerge each kite sphere downed from 30 s before he went down, its spend and the kite it
ended, under 15 s in flagged, a spike that came up with no kite patch down flagged, the kite patches on
the floor as he went down, under 2 flagged, and allowance 0 with no `allow` rows. `--threat` when the submerge drag left. `--healers` also the main tank's seconds with no healer within 40 yd.
`--burrowers` Bladestorm casts with a burrower within 25 yd, `anubarak burrower guard` vetoes on it,
`anubarak burrower threat cap` vetoes per bot. `--threat` drag pre-steps (counted as legs) and
`subspot` rows per window with how far he went down from the last spot, `no submerge leg` (no
`submerge` or `prestep` row after its first search) with its `subhold` rows. `--burrowers` each run
of 3+ burrowers on one side tank with its taunts in the second before (Righteous Defense once per
burrower) and, per phase, the samples with the two sides' burrowers within 12 yd. `--spike` per
submerge the bystanders' gather/dodge swaps within 1.5 s and dodge walks. `--spike` a Hand of
Protection with a boost the kiter cast later in its kite, patch kites turned within 3 s of a boost
going out, and each `escort` `patch` wait: where, its yards to the kite patch and the kiter at the Hand
or the kite's end. `--tanks` side tank area casts per surfaced phase by spell (triggered ones left out)
and `saoe` rows. `--swarm` per Penetrating Cold carrier in phase 3 its seconds with no HoT or shield of
those carried, a death flagged. `--healers` each healer's first `hbehind` `go` and the seconds until it
stood at its stand (`hstand` `hold`, under 1.5 yd in 1 s), and each burrower within 5 yd of a healer
in phase 3 with no step from its exit or emerge within 7.5 yd of a Permafrost centre. `--swarm` the
`swarmheal` counts per healer, `healduty` and `coldheal` spans, and per phase 3 Penetrating Cold wave
its first ticks survived, the frost holder, Aura Mastery up at ticks 1-2, `mastery` rows, the resisted
share per tick and per carrier its health at the landing, its margin at the first tick (the reader's
own risk model: health × (1 − drain)³ against the tick, no absorbs or timers; a survived tick's margin
clamped to 0, the pool read at first sight), fate, heals and absorbs before it by source and its
`coldheal` healer; a herd is 2+ healers' heals on one carrier within 1.5 s while another at risk got
none. `--healers` a healing table per phase on Warcraft Logs' rules (effective plus absorbs to the
shield's caster, overheal over gross, HPS, share; pets and totems to their owner); the trace has no
heals onto pets and no swarm heal on him. `--burrowers` each pulled-off burrower's seconds until a tank
had it back and the taunt that did it. `--spike` per kite each Sprint, Dash or Nitro with the spike's
speed (-0.5 to +1 s) against the kiter's run over the 1.5 s before, one no faster flagged; the first
against its pick's `+boost@` (on time within 0.5 s; only on traces carrying one); the `standby`
escort's yards to the kiter at the first `protect` `hop`, past 30 flagged; after an own ending or a
patch end, the next mark's leg until the spike is within 12 yd of its kiter or its spot at the mark,
any patch it passes within 11.5 yd (BM 1:22 Vanish spent side 1's tank patch +1.1 s in, BM 3:44 #1118
+4.1 s, BL 1:39 #576 +2.6 s); summed per pull. `--healers` each healer's mana at phase 3's start and
30 s in, and its Divine Plea, Innervate (with target) and Hymn of Hope casts by phase. `--burrowers`
per melee bot the seconds on a burrower no tank within 5 yd held, by `anub.dps` (BK-BN 14-36 s, about
70% `loose`). Baseline, the 2026-10-27 25H wipes I,
J (`1496982cf`): his health a phase 1 I 24.8/13.7/14.3/14.8%, J 19.6/16.1/4.0%; melee 142-255 yd a
bot a phase 1, moving 24-59%, 17-55 midline crossings; ranged 27-43 yd; held off J P1c 64.8 s.
Baseline, the 2026-10-27 afternoon 25H wipes K, L (`8145d288f`): his health a phase 1 K
22.0/19.1/17.7/9.2%, L 20.8/21.5/11.7/11.6/3.4%; melee 105-218 yd a bot a phase 1; kites K 10, L 13,
L's four deaths with a patch unused (Prayer 1:22 and 8:51, Druidica 3:44, Stormweaver 8:29); K 1:22
Power's spike 14.2 yd from a tank patch 8.7 s in, never turned; Impale ×6 on Felesta K 6:57; the main
tank at Expose Weakness 5 L 3:31.
Baseline, the 2026-10-27 evening 25H wipes M, N (`5d6963920`): his health a phase 1 M
25.1/17.3/3.9/0.4%, N 25.6/12.3/4.1%; the first tank patch of each submerge M 11.9/7.1/15.2 s, N
15.2/10.2 s into phase 2; patch walks taken M 3/3, N 2/2; kite deaths M 7 of 15 (a human's first,
then 5 in the gather), N 3 of 9; far grips 46-49 yd off side 1's spot; N P1b drag held 56.3 s.
Baseline, the 2026-10-27 night 25H wipes O 4:17 (59.8%), P 8:08 in phase 3 (28.4%), Q 6:54 (43.1%)
(`fdb3a950f`): his health a phase 1 O 23.6/16.4, P 23.9/19.1/17.8/8.9, Q 24.2/20.9/11.8%; tank
patches spent a submerge O 1/1, P 2/1/2, Q 2/1/1, 5 of the 14 in `run`; the first of each submerge O
13.7/11.9, P 37.9/15.9/18.6, Q 14.4/15.2/14.4 s into phase 2; kite deaths O 2, P 2, Q 5; flip-flops
O 2, P 2, Q 3; Death Grip with no pull Q Felesta 2 of 7, Obliteration 1 of 4, Deathsong 1 of 2.
Baseline, the 2026-10-28 morning 25H wipes R 8:13 in P1d (36.1%), S 6:01 (43.9%) (`4cad7660b`): the
first tank patch R 33.9/27.9/10.3, S 33.9/5.7 s into phase 2; kiter deaths R 0, S 4; gather/dodge
swaps within 1.5 s R 1,555, S 427; Impales on bystanders R 7, S 5; runs of 3+ burrowers on one side
tank R 8, S 7; the two sides' burrowers within 12 yd S P1b 99 of 200 samples.
Baseline, the 2026-10-28 evening 25H wipes T 0:48 in P1, U 5:23 in P1c (56.8%), V 9:03 in P3 (15.7%)
(`076b7ce55`; main tank human in T and U): tank patches spent a submerge U 1/2, V 2/2/0 (no sphere left
for V P2c, 3 kiters dead there); kiter deaths U 1, V 4; hand kites with the escort within 30 yd at
the claim U 1 of 3, V 2 of 4; main tank with no healer in 40 yd T 8.8 s.
Baseline, the 2026-10-29 25H wipes W 6:53 in P2c, X 8:27 and Y 8:33 in P3 (`32ba6602d`; main tank and
Deathsong human): boss at the third submerge W 35.5, X 42.2, Y 35.3%; tank patches spent a submerge W
2/2/1, X 2/1/1, Y 2/2/1, every sphere; a side bare with burrowers out W 81, X 87, Y 84 s; kiter deaths
W 1, X 1, Y 0, all in P2c; Hand within 1.1 s of the catch in 9 of 9 patch+hand kites.
Baseline, the 2026-10-30 25H wipe Z 6:55 in P2c (`e131b1990`; Ecoterrorist the bot main tank, Deathsong
human, 4 healers): boss at the third submerge 47.3% (phases 1 -23.2/-17.3/-12.1); side-tank deaths 4,
3 in the 40 s drag; no tank patch spent by a kite; no kite patch down as P2's and P2c's spikes came
up; flip-flops Justice 6:06.9, Tree 6:34.3; Hellflame's teleport at 4:25.8 landed 25.6 yd off the
spike.
Baseline, the 2026-10-30 25H wipe AA 7:05.6 in P3 at 15.7% (`f87f49475`; as Z): two submerges, phases 1
-26.0/-22.1/-21.9, phase 3 at 5:56.5; Bulwark pulled him at 21.2 yd; in phase 3 his net loss 0.14%/s
for 25 s, then 0.42%/s, swarm 3.61M (non-tanks 2.89M), healer heals onto non-tanks 1.98M against 0.96M
onto tanks, DPS on burrowers 35% melee / 48% ranged, 42 burst casts in P1c, the three tanks dead
6:35-6:43, 31 deaths.
Baseline, the 2026-10-30 evening 25H wipes BA 8:29 in P3 at 26.8% (three submerges, P1c -19.5%) and BD
7:15 in P3 at 7.0% (`06fd22497`; Ecoterrorist and Deathsong human): BD phases 1 -27.3/-20.4/-22.1,
phase 3 30 → 7.0% in 78.8 s, Heroism at +15.3 s; one kite patch down as he went under in each of BD's
submerges.
Baseline, the 2026-10-30 night 25H wipes BF 6:51 at 15.2% (`06fd22497`) and BG 6:55.8 at 14.3%
(refine-17 without the 10 yd step; as BA): BG no death before phase 3, six kites, no kiter death,
side tanks' area casts every phase, phases 1 -30.9/-20.4/-18.5; its first kite spent tank patch 1 (he
went down 17.8 yd off it under the human main tank). Phase 3 from the Chronicle logs (`log_file`
d242a72b BG, fd225ee8 BF): BG 66.4 s, 7.48M to him (113k/s), the swarm healing him 1.94M (29k/s); BF
54.1 s, 7.45M (138k/s), 2.20M (41k/s); his phase 3 pool is 10.6M (35.35M = DB 27.19M ×
`AC_DUNGEON_SCALE_STAT_MODIFIER_RAID_HEROIC_BOSS_HEALTH` 1.3), so about 110-125 s at that damage. Raid
healing the first 30 s 70k HPS (Holylight 22-24k, other healers 7-10k, non-healers 18-21k). BG's
healers died to Penetrating Cold from +29 s (Holylight, Prayer, Elemena), then the tanks; carriers
surviving their first tick 9 of 20 (5/2/2/0 a wave). BF's healers died to burrowers (6:30.8-6:34.8,
refine-16's healer stand). A real 25H kill's healing table (Warcraft Logs, 90.8 s): five healers
24.0/9.9/9.0/7.2/5.8k HPS at 1-16% overheal, non-healers 19.5k, 75.5k in all, so throughput matched;
the gap was who got healed in the 3 s before a first tick.
Baseline, the 2026-10-31 evening 25H pulls BH (wipe 7:05.5 in phase 3 at 11.5%), BI (reset in P2b at
4:51.8, 49.1%, every patch spent) and BJ (reset in P1c at 5:01.5, 55.7%) (`a27289245`; Ecoterrorist
and Deathsong human): first kites spent a kite patch BH 7.9, BI 8.7, BJ 4.6 s in; BI P2b both tank
patches (Angry 4.5 s, Assasin 13.7 s); BJ Obliteration 4.1 s in side 1's slow, Assasin's Vanish 19 yd
from the gather handing the spike to two Ice Blocks, Druidica dead with no Hand within 39 yd, Elemena's
spike restarted 11 yd off a tank patch and spent it in 0.9 s. BH phase 3: 11 of 15 carriers through
their first tick, no healer dead to Penetrating Cold, Aura Mastery at 6:12.959; Felesta dead 5:54.2 at
9 stacks, wave 6's focus 14 s late, 41.5k in her last second; wave 7 (6:22.2) left to the side tanks,
who died 6:30.7 and 6:34.8.
Baseline, the 2026-10-26 25H wipes C, D, E: Hand of Protection on a
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
