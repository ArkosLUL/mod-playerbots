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
| EMERGENCY+9 | `anubarak kiter needs immunity` → `anubarak kiter immunity` | a marked bot: normal, no protector, the spike within 9 yd and catching it, no patch stand it reaches first; heroic, as the [kite plan](#the-kite-plan) says | paladin `hand of protection` while it has a patch to walk to, the emerge is 10 s off or the plan ends on its own hand, else (always for an `own` plan) `divine shield`, the other as fallback; mage `ice block`; rogue `vanish`; hunter stops, then `feign death` |
| EMERGENCY+8.5 | `anubarak kiter needs freedom` → `anubarak free kiter` | the paladin bot picked for a kiter carrying Permafrost | `hand of freedom` on it ([below](#how-the-bots-play-it)) |
| EMERGENCY+8 | `anubarak pursued by spike` → `anubarak kite spike to permafrost` | carries 67574 | heroic: a boost due, at any stage; else walks to the kite stand, `MOVEMENT_FORCED` |
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
| RAID+4 | `anubarak dps target` → `anubarak dps target` | DPS whose `GetAnubarakDpsTarget` pick isn't its current target or its pet's | attacks the pick, pet too |
| RAID+3.8 | `anubarak melee burrower stand` → `anubarak melee burrower stand` | a melee DPS bot on a burrower near a patch, out of reach, within 8 yd of a patch or 1.5 off its held stand | walks to its stand |
| RAID+3 | `anubarak burrower needs assist tank` → `anubarak assist tank hold burrower` | a side tank with a burrower to take, not the pickup tank while he is up | picks up, holds it from its rim |
| RAID+2 | `anubarak scarab on raid` → `anubarak tank pick up scarab` | the pickup tank while he is down; an unmarked side tank, not the escort, with no burrower to take and no wave due within 8 s, while he is up (heroic also down); a scarab within 30 yd on a non-tank: heroic side tanks only one on a healer, heroic phase 2's pickup tank only one on itself | taunt (not one on itself), attack |
| RAID+1.5 | `anubarak side tank ready` → `anubarak side tank ready` | an unmarked side tank with nothing to take, not the pickup tank while he is up, not gathered, not the escort | attacks a scarab on itself, walks to its rim (else spot) |
| RAID+1.2 | `anubarak redirect threat` → `anubarak redirect threat` | a hunter or rogue bot with a [redirect duty](#how-the-bots-play-it), its redirect ready or Misdirection's charges left | Misdirection or Tricks on the tank, then Steady Shot into the dump target |
| RAID+1 | `anubarak engaged by main tank` → `anubarak main tank hold boss` | the pickup tank while he is up | steps out of Permafrost, else leads him in latched legs onto the anchor (round a patch in the way), or from 65 s onto the submerge spot |
| RAID+0.5 | `anubarak raid gathers` → `anubarak gather` | heroic phase 2, not the kiter or a side tank with a burrower to hold or take, no dodge held | walks to its gather slot, round any patch; while the way there crosses the spike's path waits, stopping a walk (`wait`) |
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
side tank 30+ yd out is past a heal once the slots spread. `anubarak healer stand hold` blocks formation and follow; reach spell
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
first kites started with the spike within 12 yd. Latched for the window, redone
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
on that burrower and its area damage within 10 yd of it or its target with one there.

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
of them hit, unless he, the other side's or a side tank is on them too or it would leave the tank past
two (it taunts all three attackers: in 2026-10-28 R and S it put three on the tank four times, three
fatal), Challenging Roar or Shout with two within 10 yd and none of the other side's, the class taunt, then Feral Charge,
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
3. a melee bot: a burrower within 10 yd on a non-tank, any side (`loose`);
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
focus (per instance): selectable and under 10%; else **settled**, a tank within 5 yd has held it 4 s
straight (reset on a new victim or the tank past 5 yd), or its side has no usable side tank, and then on Permafrost or in its **free time**, the 25 s after it first
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
burrower always, and its area damage (`IsToCAreaDamage`) while a burrower still settling stands
within 10 yd of it or its target; an interrupt or stun on a casting one passes. Bladestorm waits
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
disappears while he is down, whatever spent it. Six spheres, no respawn: in 2026-10-27 night P two
extra spends left phase 3 without Permafrost. **A second tank patch** is the strict last resort
(`anub.second` `grant`, `anub.allow` 2): no own immunity and no other ready Hand in reach, the kite's
end before the spike's last second, a sphere spare, and the walk `in` still making it, under a Hand
once running 1.5 s more would lose it (the 15.5 yd keep-off makes its last second too late), else
(user call, 2026-10-28) at the same last moment once a catch is predicted: in S P2b four kiters died
in a row, no Hand left, with side 0's patch up. Under the allowance the kite may end on
a tank patch, a side holding no burrower first, or on a stray patch 16.75+ yd from every tank patch
(after a fail the spike restarts there, and its first test reaches 11.5 yd plus its 5.25 yd first
walk); route clear first: spike to kiter to stand passing no other tank patch within 14.5 yd; then,
every difficulty, a walk passing the gather by 12 yd. Under its own Hand of Protection the kiter
ignores the spike-behind tests: G 1:22 Holylight's immune turn timed the 26 yd walk to the near
patch, the stand pick refused it with the spike 2.5 yd off her and sent her 44 yd through the gather,
the hand ran out 15 yd short and Felesta died to the Impales.
Branches, as `anub.kite`:

- `hold` within 2 yd; returns false, so a healer or caster keeps working.
- `detour` when the straight walk cuts deeper into Permafrost's 7.5 yd reach plus 1 than either end
  already is: via the corners of a square round the patch, sides 9 yd out, the bot's side first. A
  single point straight out to the side won't do: the leg on from it still cuts to 6 yd of the patch.
- `ring` with no patch: 40° round a 40 yd circle about the room centre, the way that opens the angle
  to the spike, floor-checked (navprobe-clean at 40, 16/16), while normal's `kite` duty drops a sphere. A
  15 yd raw flee point risks leaving the mesh.

#### The kite plan

**Heroic decides at the mark how the kite ends** (`anub.kiteplan`), judged every 250 ms and kept
until it can't be had: the patch gone, the hand's paladin dead, marked or without Hand of Protection
ready (`ReadyInstant`, not `CanCastSpell`, which fails on the GCD and flipped the escort Justice ↔
Holylight every 0.2-2 s in 2026-10-27 pull I), Forbearance, the own immunity spent; `none` is
re-picked every judge. T is the latest catch over the run headings (pursuit model, 40 s, speed steps
in), L the spike's life left, 60 s from the submerge (`PickTerminal`, pure, nativetest):

- T ≥ L − 1 s: `none`, it outlasts the spike;
- a patch it may end on, L − T ≥ 12 s and a hand ready (an escort's, or a marked paladin bot's own;
  none under Forbearance), the kiter no paladin with Divine Shield ready: `patch+hand`, the hand at
  the catch, then the patch under it, a spent patch buying a fresh run from base speed;
- Divine Shield, Ice Block, Vanish or Feign Death ready: `own`, the patch left to the next kiter; late
  with a hand ready and the kiter no such paladin, `hand`, its own going out with 0.8 s of the hand
  left (cast after, Impale's next tick can land first: P Nightwarrior);
- a hand ready: `hand`; a patch: `patch`; else `none`.

The lone early `patch` is gone: replayed, the next kiter died no more often after an own's full-speed
hand-off than after the patch walk. A hand first cost Q Justice his Divine Shield to Forbearance.

**Commit by simulating the escape, never by distance or the spike's speed now.** A walk "beats the
catch" when the spike comes within 11.5 yd of the patch 1 s before it would close to 7.5 of the kiter
(`PatchMargin`); under Hand of Protection the catch counts from the hand's end, and never with the
kiter within 4 yd of the centre, where the spike fails first. A patch kite runs on while turning
1.5 s later would still beat it (`PatchWalkAfter`, over the moves allowed at the pick), else turns,
and stays turned. In 2026-10-27 J 6:06 and I 3:44 Felesta, marked
with the allowance 1 and the spike at base speed, ran on because the old reading of Impale needed the
spike faster now, turned as it reached top speed 10.7 yd back and died; replayed, the rule turns her
within 4 s. The hand goes out 1 s before the catch on the held heading's run, once the spike is within
7.5 yd plus 1.5 s of its closing speed; read off the facing, a flip-flopping kiter faces the spike and
P 3:44 Fel's went 4.1 s early. An own immunity goes 0.6 s before. `anubarak hand reserve` blocks the
Spell family (off-GCD too), `anubarak heal penetrating cold` and `anubarak interrupt shadow strike`
for the planned or claimed paladin from 2.5 s before the catch: P 1:22 Power's, claimed +24.1, went
out at +25.2.
A boost never holds anything back: in J 1:22 Justice's running Nitro gated both the patch turn and
his own Divine Shield and Hand, and he died with all three.

**The walk onto the patch is one of three moves, picked by simulation** (`BestPatchWalk`,
`anub.kitemove`): `in`, straight to the centre through the slow (heroic 80%: 1.4 yd/s inside 7.5,
none under Hand of Freedom); `edge0`/`edge1`, to the tangent point on the 8.5 yd ring, the patch on
the kiter's left or right, then 15 yd on (from outside 8.5 only). The widest margin wins; under Hand
of Protection `in` only, since the spike sits 2-3 yd behind and only its test ends it. At top speed
`in` needs the spike about 40 yd back, an edge pass about 20 with the spike behind on the kiter's
line. The patch: rank first, then the nearest with a move whose spike route (spike, kiter, legs)
passes every other tank patch by 14.5 and whose walk passes the gather by 12, then either, then any;
an edge's legs land on the floor. Turned, the kite keeps its walk while it still makes it, else goes
`in`. The stand 8.5 yd past the patch off the spike it replaced was the far side under a hand: L 1:22
Prayer circled her patch at 12-19 yd, the spike 2.5-3.7 behind, until the hand ran out; and with the
spike between kiter and patches every stand failed the spike-behind test (L 3:44 Druidica ran the
ring and died). Normal keeps the stand (`PickKiteCandidate`: rank, then the nearest).

**A `patch+hand` kite takes the hand at the catch, then the patch** (`JudgeBridge`, `anub.bridge`).
Until the hand it runs free, the patch clearance keeping the spike off its patch; landed (`hand`) the
kite runs on under it 15.5 yd off every tank patch (the 11.5 test, the spike trailing 2-3, 1), the
pass horizon the hand's end, and walks `in` with 1 s of it left. At 8.5 P 3:55 Fel's patch went 1.5 s
into the hand. It turns without (`turn`, the plan becoming `patch`) only once
the walk still makes it now but not 1.5 s on, with no hand able to land: the kiter's own, or a ready
alive unmarked paladin bot within 30 yd and in sight, the kiter free of Forbearance. A patch walk
can't be held long on this floor: replayed per heading, a fresh spike 23-47 yd off catches on the
40 yd ring 10-16 s in, the walk stays possible 4.5-7.5 s at most, and the patch goes 6-13 s in under
any turn rule (as now, graded by margin, latest by scan). With the hand at the turn the 2026-10-27
evening pulls spent each submerge's first tank patch M 11.9/7.1/15.2 s and N 15.2/10.2 s into phase 2
and every later kite had none; at the catch the patch goes about 25 s in and the restarted spike's
next catch lands near the emerge. Turning only once the hand landed, L 8:29 Stormweaver never turned:
the claim lapsed, the paladin was 37 yd off at the catch, and she died after 22.1 s with the patch
unused.

**Heroic kites run first** (`anub.kitestage`, guide: as long as possible without touching
Permafrost), the stage per instance per ms. The 2026-10-25 bot kites lasted 1-15 s, each ending on a
patch, 4-5 a phase 2:

- `run`: no patch search. A **lane step**: 16 headings, 10 yd out, floor-checked, within 45 yd of
  the room centre (or no further out than the kiter is), the spike behind and no nearer. Each heading
  is judged by the **pursuit model** (`ToCAnubarakPursuit.h`, no game headers, nativetest): 100 ms
  steps of [pure pursuit](#pursuing-spike-34660) with the speed steps off its auras, up to the kite's
  catch (6-20 s), to a hand's end under one, of the kiter on each **path** of the heading: straight
  to the 40 yd ring or the 45 yd rim (navprobe-clean all round) and round it either way, and while the
  spike is at base speed either way round from where the straight leg stands at its next step, the
  rim at the latest. Replayed over 105 traced 25H marks the paths cut run spends 13 to 12 and moved
  the median catch 12.6 to 13.8 s; one way round, K 1:22 Power's run never showed the corner the
  spike cut. Six paths a heading cost 2.3-4.2 ms a lane step at `-O2`, against 0.4-0.7 for one. It
  clears when the spike's simulated run passes every
  obstacle by its clearance and the kiter's own run every patch by 8.5, one starting inside that only
  having to head away. A straight lane missed G 4:04 (2026-10-26): Agony on Nitro, faster than the
  spike, rounded the north rim and the spike cut inside it within 8 yd of a tank patch, spent in
  `run`; the model puts that pass at 9.5 and the south heading's at 39. Over 6 s at base speed the spike
  covers 21 yd: K 1:22 (2026-10-27) Power's south heading read clear and the spike cut over a tank
  patch 9.5 s in. Obstacles: every patch at 14.5 yd; in phase 2 the gather point at 20 (3 yd slots, up
  to 6 off them, Impale's 7.5, 3.5 margin), since Impale is cast round the spike as it moves: at 12
  the spike passed within 10 yd of six gathered bots; and every living player outside the gather but
  the kiter at 10 (Impale's 7.5 plus 2.5), ranked with the gather: K 6:57 Assasin under a hand walked
  the spike over side tank Felesta, 6 Impales. Ranked: clear of every patch; the plan's goal (a patch
  kite: turning 1.5 s on along it still beats the catch); ranked first, a hand kite's 25 yd escort
  leash walked 4 of 2026-10-27 night's 5 run spends within 5-6 yd of a tank patch; an own kite, the step's distance off the
  gather in 5 yd bands, so the spike's next random target starts farther off; clear of the gather;
  for a kiter marked inside the gather's clearance, its first 3 s, the widest pass by the gather;
  the latest catch in 2 s bands; then the **restart line**, the kiter's modelled spot 3 s on to the
  gather, passing every tank patch by 14.5, since after a death or hand-off the spike runs from there
  to the raid (C 1:57 the line crossed the south patch 0.5 yd off); then, while the spike is faster
  and a patch it may end on lives, a step within 25 yd of its stand; then the widest pass by the
  patches, then by the gather and players, margins capped at 20 yd, then the step furthest from the
  spike. None clear of the patches: the latest first pass inside a patch's clearance, then the widest
  patch margin, the gather left out: weighing the gather like
  a tank patch, the second kite of every 2026-10-26 pull walked the spike over the last tank patch.
  **The heading is held** (within 15°, its path with it; `KeepHeading`, pure, nativetest) while its
  catch stays within 2 s of the best and either its spike passes no patch inside 11.5 yd and it is
  under 1.5 s old or matches the best on the class keys, or no heading passes that clear and its first
  pass inside a patch's clearance comes at most 1 s before the best's; a turn past 90° waits until it
  is held 1 s. Dropped whenever every heading passed a patch, O 1:22 Hellflame reversed 10 times and
  spent the patch. At the rim only two opposite headings pass, and
  re-ranked they swapped every tick, Mighty (G 2:09) and Prayer (H 6:16) stalling 5-6 s, Prayer to
  death, and 2026-10-27 J still had four such kites (Nightwarrior 5.2 s, 9 reversals) under the old
  keep. None at all: the plain ring step. The ring step this replaced checked only the kiter's
  leg: all four early 2026-10-26 kites circled a tank patch at 15 yd while the spike cut straight
  across it. A warlock kept within 38 yd of its circle circled it and let a speed-2 spike cut the
  chord.
- `boost:<spell>` once the spike catches the kiter within 20 s and is within 15 yd and faster now, or
  within 10, never with over 0.5 s of a hand left (O 1:50 Stormweaver's Nitro): cast at any stage, the walk to a patch included; Sprint, Dash in cat form, Nitro Boosts
  (none while one of those runs, one speed aura type), Blink 20 yd the way it faces when that lands
  farther from the spike and the spike's lane to it clears (no trail leg), then Demonic Circle: Teleport with the circle in reach and 15+ yd farther
  from the spike. Sprint's +70% outruns the spike's 11.4, so the chain moves on as each runs out. The
  kite casts them itself, so the kite veto never holds them.
- `immune`: Hand of Protection up, still running; the generic `feign death` is zeroed for a marked
  bot.
- `patch`: a `patch` kite, or a `patch+hand` one once its hand is on, whose walk no longer beats the
  catch now or 1.5 s on ([the plan](#the-kite-plan)): the plan's move, leg by leg (2 yd arrive),
  holding on the last. **A kiter carrying Permafrost counts at
  base run speed** (7 yd/s, more if boosted) in every simulation: it runs that once off the patch.
  Read at 1.4 yd/s, Bulwark marked in his own tank patch turned `patch` 0.1 s in against a 3.5 yd/s
  spike (F 1:22, H 6:06) and spent the submerge's allowance. Turned for a patch, it stays turned until
  the mark moves.

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
predicted track over 3 s, a point every 0.2 s, per instance per ms (`DodgeTrack`): heroic, chased
along the kite plan's held run, or its walk once turned for a patch; a kiter with no held heading
(normal, a human), on its run the way it faces; nobody marked (the 4 s after a fail, when it keeps
chasing the old kiter at base speed), straight on toward its victim or spline end (`MotionTrack`).
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

**Hand of Protection covers a kiter the spike closes on.** Normal: once the spike is within 12 yd of
a kiter and catches it within 20 s (a slowed kiter counts), with no Hand of Protection, Divine
Shield, Ice Block or Forbearance. Heroic, 1 s before the catch on the held run ([above](#the-kite-plan)), a base-speed spike inside
Impale's reach of a boosted kiter counting only once it outlasts its next 1.5 s test (S 3:50: the
submerge's last Hand went 0.6 s into a kite with Nitro running): for a `hand` or
`patch+hand` plan (not the kiter's own), the plan's paladin first; and once the plan has failed (no
patch walk that beats the catch, or the spike inside 9 yd anyway, and no own immunity or own hand
planned) any. The claimer is an alive unmarked paladin bot within 30 yd and in sight with the hand
ready (`ReadyInstant`; the cast waits out the GCD), in the **protector order**: neither tank nor
healer, a side tank with no burrower to hold or take, a healer, any other tank, by guid within each.
One claim per mark, kept until the mark moves; one 1.5 s old with no immunity on the kiter passes to
the next paladin, and one lapses, for the same paladin to retake, when the need goes. Two 2026-10-26
hands went out 0.2 s into a kite on a 3.5 yd/s spike. **The kiter's own** (table above): normal, with
no claim and no patch stand it reaches first, within 9 yd (Impale's 6 + 1.5 plus a margin); heroic,
its planned hand 1 s before the catch, its planned immunity 0.6 s before, a non-paladin's behind its
`hand` with 0.8 s of the hand left, or that as the failed plan's last resort with no claim. Only Hand of Protection keeps the spike on it; the others, like a
death, hand it on at its current speed, so a paladin takes Divine Shield only with no patch to walk
to and the emerge over 10 s off, or for an `own` plan.

**Immunities wait for the kites** (`anubarak kite immunity reserve`): heroic phase 2 blocks the
class `divine shield`, `ice block`, `vanish` and `feign death` for an unmarked bot; the kite casts
its own directly. In 2026-10-27 I Holylight's generic Divine Shield at 6:07 left her Forbearance and
no shield for her kite at 6:26, dead in 21.9 s; Justice's at 8:28, Smartface's Ice Block and
Bulwark's Divine Shield were spent the same way. A bot marked mid-cast stops it: the AI waits out a
cast before any action, and Prayer stood 2 s at I 6:06. Holylight's
Divine Shield 7 yd from the gather in a 2026-10-26 wipe handed an 11.4 yd/s spike into it: 12
Impales, 5 dead in 0.8 s. Impale is physical, and 5 of the 6 kites after the patches ran out took
Impales in the 2026-10-16 kill.

**A paladin escorts a kite its hand ends or bridges** (heroic, user call). In the 2026-10-26 late
wipes the paladins stood at the gather while such kiters ran the far side of the 40 yd ring, 31-43 yd
off, and died with no hand; in 2026-10-27 a ready paladin was within 30 yd at the 12 yd mark in only
7 of 14 (J) and 10 of 19 (I) kites. The **escort** is the plan's paladin for a `hand` or `patch+hand`
kite whose hand hasn't landed, latched by the plan (the first in the protector order with it ready
when the plan was picked), none for the kiter's own hand, so it is the one the claim picks. Until the
held catch is 4 s off it **shadows** the kiter: on the line from the room centre to the kiter, 25 yd
short of it (the centre when nearer), turned 15° and 30° either way round the centre until 15 yd off
the spike (19 while the spike outruns the paladin: 15 sits inside the dodge's 16, and Q Holylight
swapped walk and dodge every 0.2-0.6 s to 35 yd off), the dodge lane
clearance off its line to the kiter, 29 yd of the kiter, 8.5 yd off every patch and on the floor
(inside the centre's navprobe-clean 50 yd); none clear, as with the spike crossing the centre (L 8:29),
points 15, 20 and 25 yd round the kiter at 30° steps, the nearest the paladin first, on the same
tests; none of those, no escort. From 4 s it waits at a **post** (`anub.escort` `post`;
`EscortPosts`, pure, nativetest): 15, 20 or 25 yd round the catch point on the held run at 30° steps,
within 45 yd of the centre, 8.5 off every patch, 15 (19) off the spike's simulated track and 9 off its
lane to the catch, on the paladin's side of the spike's line to the kiter, the nearest the paladin
first, on the floor; kept until the catch point drifts 6 yd or the spike closes on it; none, the
shadow. Read earlier, the catch point swings 30-60 yd with each heading: replayed, posts alone had
the escort in reach at the hand 83% of the time, committing at 4 s 90% (3-6 s tried). Over 4 yd off
it walks there, a
walk headed within 3 yd left running. The gather and the side tank nodes skip it, and `anubarak hop
escort hold` blocks `reach melee`, `reach spell`, `reach party member to heal`, `anubarak melee
burrower stand`, set behind, flank, formation, follow and charges. It ends with the hand, the mark, the emerge or the plan's re-pick.

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

**Phase 3.** The main tank chains `NextTankDefensive`. Healers lead with Penetrating Cold carriers
below 90% in heal range and line of sight (guide: "1-3 healers assigned solely to" it). Lust
waits for `Swarm`, latched per instance (Anub'arak row of `ToCBurstWindowMultiplier`), since
phase 3 is where the raid takes its damage (guide); personal burst is unrestricted.

## Known gaps

- Untargeted raid AoE still pops spheres, all of it in normal (Starfall made every patch in the
  2026-10-16 kill), all but Starfall in heroic.
- A melee bot under a threat cap keeps white-swinging, so it can still climb past his or a burrower's
  tank.
- A human kiter or tank reads none of this: no protection claim waits on a human paladin, a human
  kiter gets no boost stage, and the drag waits while a human holds him.
- A human's Divine Shield, Ice Block, Feign Death or Vanish hands the spike on at its current speed,
  wherever the human stands.
- The second kite of a submerge has no patch but the strict second one: it lives on boosts and Hand
  of Protection, then hands the spike on at full speed.
- The gather slots sit 18-21 yd off the kite ring's centre, where a base-speed spike settles in pure
  pursuit (20 yd), so it crosses the gather each lap and the raid steps aside.
- Replayed, the first tank patch still goes about 11 s into phase 2 (6 of 34 first marks at 25+ s),
  and about 12 run spends in 105 marks remain: marks starting next to a patch, and pure pursuit
  settling inside the ring (20 yd at base speed, 35 at the second step) and cutting across one.
- 10H with one side tank puts both burrowers of a wave on one patch, inside Spider Frenzy's 12 yd.
- A grip stepped out 16 yd, or a far grip, can leave the patches 40+ yd apart; past about 36 a melee
  stand between him and a rim burrower no longer reaches both.
- A human's generic Divine Shield, Ice Block, Vanish or Feign Death in phase 2 is not held back.
- The kite plan's catch runs the kiter on its heading then the ring; the walk 1.5 s on takes the moves
  allowed now, not floor-checked there.
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
- A human marked by his own tank patch spends it (H 1:22 Deathsong); nothing steers a human kiter.
- The pursuit model runs the kiter straight then round the 40 or 45 yd ring at its speed now; a boost
  running out or a turn inside the ring is outside it.
- A rogue's Tricks waits until it hits a loose burrower, which its DPS pick rarely is.
- Side tanks die to two frenzied burrowers at 9 Expose Weakness stacks (+225%, 5+ stacks 17-34 s of
  each 2026-10-26 wave), and most burrower submerges followed a side tank's death; no healer is
  assigned to a side tank.
- The kite ring (40 yd round the centre) runs past the west refill pair at 36-43 yd.
- A human's Army of the Dead ghouls still taunt burrowers off the patches (one submerged at 0:39 in a
  2026-10-26 wipe); only bots are blocked.
- A healer's heal threat still pulls burrowers off a side tank as the taunt's 3 s run out (Elemena
  11-12 times a pull in the 2026-10-27 evening wipes).
- A far grip with one DK bot, or with the side's DK tank's grip spent, has no relay.
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
| `dside` | A DPS bot's burrower side, 0 or 1, keyed on the bot, re-split at each wave |
| `grip` | 1 a grip went out this pull, 2 while a side waits on a failed one. Per instance |
| `spot0`, `spot1` | Each side's tank spot, `x,y,z`: its latched patch, else the open spot. Per instance |
| `allow` | Heroic tank patches the submerge's kites may spend, 0 or 1, written as he submerges, 0 once one goes while he is down, 2 with a second granted. Per instance |
| `sphere` | Sphere duty: `kite` and `stock` (normal), `0` or `1` (a side), `grip0`/`grip1` (a DK's grip for a side), `none` |
| `gripstep` | The DK's grip: `walk`, `hold`, `cast`, `wait` (sphere in the air, or a relay link still to grip it), `kill`, `post` (a relay link to or on its post), `none` |
| `relay` | A relay link DK: `wait` (posted), `cast` (gripped), `miss` (its grip pulled nothing), `drop` (relay dropped), `none` (no grip duty) |
| `sidetank` | `hold`, `pickup`, `ready`, `gather` (heroic phase 2, nothing to hold) |
| `leave` | `walk` while a non-tank steps out of Permafrost, `none` |
| `mstand` | `walk` while a melee bot walks to its burrower stand, `none` |
| `kitestage` | Heroic kiter: `run`, `boost:<spell>` (Nitro Boosts as 55004), `immune`, `patch`, `none` |
| `kiteplan` | Heroic kiter's terminal: `patch`, `patch+hand`, `hand`, `own`, `none`; can change within a mark |
| `kitemove` | Heroic kiter's patch walk: `in`, `edge0` (patch on its left), `edge1`, `none`; re-picked each judge until it turns |
| `lane` | Heroic kiter's lane step, `flags:degrees`: `g` goal met, `p` clear of the patches, `r` of the gather and players, `h` the held heading kept over the best, `x` no heading clear of the 11.5 yd test, `c` some heading clear of the patches |
| `hands` | Heroic kiter at each plan pick: every paladin bot as `guid:Hand cooldown s:yd to the kiter`, `none` |
| `second` | Heroic kiter: `grant` (a second tank patch), `none` once the mark moves |
| `bridge` | Heroic `patch+hand` kiter: `hand` (on), `turn` (turned without, no hand could land), `none`; older traces `ask` (Hand asked at the turn) |
| `gather` | Heroic phase 2: `walk`, `hold`, `wait` (the way to the slot crosses the spike's path) |
| `hstand` | A healer bot's stand while he is up: `walk`, `hold`, `none` |
| `kite` | The kiter's branch: `patch` (walking to a stand, heroic its move's leg), `detour` (normal), `ring`, `hold`; `none` when there's no spike or the mark moved on |
| `dodge` | A new dodge spot's reason: `spike` (within 10 yd of it), `lane` (on its predicted path), `none` (no clear spot) |
| `protect` | `hop` while this paladin holds the kiter's claim, `self` while the marked bot needs its own immunity, `none` |
| `escort` | Heroic escort paladin: `walk`, `hold` (shadowing), `post` (at or to its post by the catch), `none` |
| `freedom` | `cast` while this paladin owes the kiter Hand of Freedom, `none` |
| `interrupter` | `1` while the bot holds Shadow Strike duty, `0` once the cast ends |
| `pickup` | The pickup tank's step: `taunt` (off a non-tank), `attack`, `stepout` (out of Permafrost), `hold` (no leg), `drag` (a leg to the anchor), `submerge` (a leg to the submerge spot), `prestep` (the step round him before a leg), `subhold` (submerge window, no leg to the spot or round a patch) |
| `subspot` | The pickup tank at each submerge-spot search: `x,y,z rN` (N the patch clearance found with) or `none` |
| `defensive` | What a side tank or the Swarm tank cast, `covered` (one running) or `none` |
| `focus0`, `focus1` | Each side's focus burrower, 0 when none; kept current under a skull. Per instance. Traces before 2026-10-27 carry one raid-wide `focus` and `focusdue` |
| `dps` | A DPS bot's pick: `skull`, `loose`, `burrower`, `boss`, `scarab`, `none` (a sphere shooter too) |

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
seconds it had held and the puller's casts on it in the 3 s before. `--tanks` each run of the main
tank's Expose Weakness: its peak, the burrowers on him at a stack (the shared aura's row names only
the first caster), his area casts from 5 s before. `--threat` per window the longest stretch 10+ yd off the anchor with no
drag leg, cut at the first submerge leg, 10 s or more listed. `--spike` per `patch+hand` kite its
Hand against the catch the reader predicts (`ImpaleInMs` ported: the kiter on its heading at its
measured speed, then the ring; spike steps from its last restart), the spike's speed and distance
then, whether the patch followed under it; a `hand` row with no `ask` before it is the Hand at the
catch, its turn the first `patch` stage after; after a `turn` the nearest ready paladin; per submerge
the seconds into phase 2 when its first tank patch went (its `patchN` latch let go). `--spheres` per
far grip each link (DK, cast, landing, distance to the spot before and after) and `relay` rows; a
Death Grip whose 49576 triggered no 49560 from its caster within 5 ms flagged `no pull`, tallied per
caster. `--spike` per kite the escort's distance at the claim, its walk and dodge swaps within 1 s,
claim to cast, the hand against the held-heading catch; then a lane step off the patches while
another heading was clear, a kiter within 15.5 yd of a tank patch under a hand, an own after a hand
ran out, a boost in a hand's first 8 s, `second` grants, and per submerge the seconds kited and
idle, the terminals, the seconds after its first tank patch and kiter deaths.
`--burrowers` Bladestorm casts with a burrower within 25 yd, `anubarak burrower guard` vetoes on it,
`anubarak burrower threat cap` vetoes per bot. `--threat` drag pre-steps (counted as legs) and
`subspot` rows per window with how far he went down from the last spot, `no submerge leg` (no
`submerge` or `prestep` row after its first search) with its `subhold` rows. `--burrowers` each run
of 3+ burrowers on one side tank with its taunts in the second before (Righteous Defense once per
burrower) and, per phase, the samples with the two sides' burrowers within 12 yd. `--spike` per
submerge the bystanders' gather/dodge swaps within 1.5 s and dodge walks. Baseline, the 2026-10-27 25H wipes I,
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
