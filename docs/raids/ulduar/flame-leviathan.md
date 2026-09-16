# Flame Leviathan

The whole fight is from vehicles, and nothing about it behaves like a normal encounter. Conclusions
here carry their derivation, because three of them were re-derived *wrongly* in one session: the tar
snare "working", Blaze hurting the raid, and pyrite crates being cosmetic.

**Spellbooks** are `creature_template_spell`, not guesswork, and **every one of these spells is
instant** (`CastingTimeIndex 1`). `CanCastVehicleSpell` only refuses cast-time spells while the base
is moving, so on this encounter driving and casting compose freely — there is no kite-or-DPS choice.

| Vehicle | Entry | Spells (energy cost) |
|---|---|---|
| Salvaged Siege Engine | 33060 | Ram 62345 (40) · Electroshock 62522 (20, 10s) · Steam Rush 62346 (40, 15s) |
| Salvaged Siege Turret | 33067 | Fire Cannon 62358 (20) · Anti-Air Rocket 62359 (10) · Shield Generator 64677 (0, 60s) |
| Salvaged Chopper | 33062 | Sonic Horn 62974 (20) · Tar 62286 (0, 15s) · Speed Boost 62299 (50) |
| Salvaged Demolisher | 33109 | Hurl Boulder 62306 (0) · Hurl Pyrite Barrel 62490 (5) · Ram 62308 (0) |
| Demolisher Mechanic Seat | 33167 | Mortar 62634 (0) · Anti-Air Rocket 64979 (0) · Grab Crate 62479 (0) · Increased Speed 62471 (25) |

## Gunners can steer, which is why nothing gates on `IsInVehicle(true)`

Seat 0 of **both** the turret (33067) and the mechanic seat (33167) carries `CAN_CONTROL`, so
`MovementAction::MoveTo` passes its `seat->CanControl()` check for a *gunner* and calls `DoMovePoint`
on the turret creature bolted into seat 7. Movement is gated on `vehicleBase->GetEntry()` instead.
No seat a bot can occupy has `CAN_ATTACK` — that flag is on the accessory seats, which hold NPCs — so
`CastSpellAction::isUseful` already blocks class spells and no targeting-suppression multiplier is
needed. Siege engine seats 1 and 2 have **no flags at all**; the chopper rear seat and demolisher
seat 3 are `ENTER_EXIT` only.

`instance_ulduar.cpp` spawns **2 of each vehicle in 10-man, 5 in 25-man** at `x = 119.8`, west of the
arena: exactly 10 / 25 usable slots, **zero slack**.

## The demolisher runs on pyrite, not energy

`Creature::Regenerate` returns early without `UNIT_FLAG2_REGENERATE_POWER`. The siege engine, turret
and chopper have it (+20 per 2s); **the demolisher and its mechanic seat have `unit_flags2 = 0` and
never regenerate**. Max pool is 100 for all of them.

So a full demolisher is **20 barrels**, then free Hurl Boulder forever unless crates arrive. Crates:
80 `Mechanolift 304-A` (33214) spawns each carry a `33218 Pyrite Safety Container` in seat 1; shoot
one down and the container falls, `MovementInform` summons `33189 Liquid Pyrite`, and the demolisher
gunner's Grab Crate runs `62496 → 62473 Reload Ammo` = `SPELL_EFFECT_ENERGIZE` **+25 to the
demolisher**. Anti-Air Rocket reaches 1000 yd and lifts spawn far outside the arena, so targets are
restricted to the arena box — a crate that lands across the zone is a crate nobody drives to.

**Every grab credits, repeats included.** `spell_vehicle_grab_pyrite` runs on each 62482 hit and only
despawns the crate 1300 ms later. The gunner's 62496 goes to the nearest 33167 (`conditions`
13/1/62496), its own seat, which force-casts 62473: +25 to itself (`TARGET_UNIT_CASTER`) and +25 to
its vehicle (`TARGET_UNIT_VEHICLE`). Every repeat grab on 2026-09-16 drew its own 62496 (31/31, 33/33),
so gunners hold no claim: another demolisher's repeat is real energy, and the same gunner's mostly
overflows past 100. The seat pays for Increased Speed, so nothing reserves the demolisher's energy. A
demolisher under `ULDUAR_FL_CRATE_DETOUR_ENERGY` detours only to 41 yd of a crate (the gunner grabs
from 49), and only while that stop stays within his reach + `FlameLeviathanDemolisherStandDist` + the
deadband of his centre: unleashed, starved demolishers sat past the barrel's 70 yd for 22–47% of a pull.

**Hurl Pyrite Barrel** needs no ammo (no `CasterAuraSpell`), only 5 energy. It lands `62489` (54000 in
20 yd), which applies `68605 Blue Pyrite`: 10 s, 10 stacks, one per landing. Every landing also resets
the duration, at 10 stacks too (`Aura::ModStackAmount`), and a periodic damage refresh keeps its tick
timer (`AuraEffect::CalculatePeriodic`), so a refresh landing just before expiry loses no tick. Each
demolisher carries its **own** aura instance, read through the caster-scoped overload.

**Burst to 6, then refresh just before expiry.** On a ~9 s refresh cycle, a refresh at 10 stacks buys
~90 stack-seconds, while burst barrel *k* only brings stack *k* forward by the cycles it skips: #2 81,
#6 45, #7 36, #10 9. `DemolisherAction` bursts to `ULDUAR_FL_PYRITE_BURST_STACKS` (6), or to 10 while
energy covers the rest plus `ULDUAR_FL_PYRITE_REFRESH_RESERVE` (four refreshes), counting barrels in
flight. It then throws one refresh once the duration is down to the last timed flight +
`ULDUAR_FL_PYRITE_REFRESH_SLACK_MS`, each adding a stack on the way to 10. A landing is timed off our
own aura: a stack gained, or the duration jumping back up. Barrels go on him only. The rule this
replaced refreshed at ≤5 s left and ignored barrels in flight, so 2–3 went out per ~8 s cycle at 10
stacks (~15–20 a minute against ~7), behind a 30-energy reserve that idled 5 barrels. On 2026-09-16
barrels stopped at 3–8 stacks in range, the stack expired exactly 10 s after the last landing, and
throwing resumed 1.3–3.1 s after the gunner's next credit.

**Read stacks off `fl.pyrite`, never tick damage.** Ticks arrive partially reduced (a parallel series
at 0.8×), so `amount / 12120` rounds a full stack into a lower bucket and invents one-stack drops the
aura cannot produce: it refreshes whole or falls off whole. Use `amount + resisted`, or `fl.pyrite`
(`GetStackAmount`). Tick *count* needs no correction and is the number that matters: uptime, one tick
per second, measured at 20–75% per demolisher over five wipes. `fl.barrel` names each decision
(`burst`, `refresh`, `hold`, `dry`, `fail`, `not boss`), and `--pyrite` blames every stack lost on the
worst one in the 10 s before it.

`spell_vehicle_grab_pyrite`'s **chopper** branch loads a crate into the chopper's rear seat to ferry
to other vehicles. That is the retail supply line and no bot code uses it — see the gap list.

## Tar cannot slow him, and Blaze cannot hurt us

`Pool of Tar` 33090 is **faction 1965, the same faction as the boss** (our vehicles are 2105).
`SummonProperties 64` is `Category 0 / Faction 0 / Flags 0` and the pool is not a vehicle, so
`TempSummon::InitStats` never swaps in the summoner's faction. Both effects target
`TARGET_UNIT_SRC_AREA_ALLY`, so they only ever hit *his* side:

| Spell | Effect | Hits |
|---|---|---|
| `62287` Tar snare, 50 yd | `MOD_DECREASE_SPEED -75%`, `Mechanic 11` | His faction — but **blocked on the boss**: `creature_immunities` row `-362` masks SNARE. Adds are not immune. |
| `62290` Burning Tar, 15 yd | 5400 fire per second, 45s | Him and his adds. **Never our vehicles.** |

`62286 Tar` drops the pool at `TARGET_DEST_CASTER_BACK`, 9 yd **behind** the chopper, so it only
lands in his path while he is behind us. Ignition needs a fire-school spell to hit the pool, which is
`NOT_SELECTABLE` and cannot be aimed at — so it happens incidentally, from **Mortar** (school 4) and
**Blue Pyrite** (school 68 = Arcane|Fire, 20 yd enemy-area). **Fire Cannon is school 1, physical,
despite the name**, and never ignites anything.

## Boss mechanics

- **Flame Vents 62396 is a 10s channel** — `AttributesEx = 200` carries `0x40`
  (`SPELL_ATTR1_CHANNELED_2`) — ticking 63847 for 15000 in a 500 yd radius, so it is not dodgeable.
  `EVENT_VENT` repeats every 20s. `boss_flame_leviathan::SpellHit` maps Electroshock to
  `InterruptNonMeleeSpells`, which always cancels a channel, so **the interrupt is real**. He is
  `MECHANIC_INTERRUPT`-immune, but only Electroshock's effect 1 carries that mechanic; effect 2
  (8489 damage) lands, the spell hits, and the script hook fires.
- **Ram, Electroshock and Sonic Horn are `TARGET_UNIT_CONE_ENEMY_104`**, and a cone is two limits.
  `Spell::CheckRange` returns OK immediately for `RangeEntry->ID == 1`, which is what Electroshock
  and Sonic Horn carry, so for those two the **effect radius** is the reach: 25 and 35 yd. **Ram is
  not one of them** — its `RangeIndex` is 11, a real 0–15 entry, so `CheckRange` enforces **15 yd**
  and the 18 yd effect radius never applies. Reading 18 off the radius index cost a release of casts
  rejected on range; it stayed invisible while Ram only ever pointed at a boss with 15 yd of combat
  reach to hide the gap. `world.spell_cone` is the **width**: Ram 100°, Electroshock 60°, Sonic Horn
  50°, and the demolisher's Ram 62308 also 100° — check the **live** table, because
  `data/sql/base/db_world/spell_cone.sql` has no row for any of them and reads as "engine default".
  `isInFront` passes no target radius, so
  his 15 yd reach widens the arc by nothing, and `CAST_ANGLE_IN_FRONT` is 120° — wider than all
  three, so `CastVehicleSpell` never turns the vehicle for them. Gate on both limits and turn the
  vehicle yourself, or the energy buys nothing and the interrupt election picks an engine that lands
  nothing: five wipes fired 71 Electroshocks, landed **one**, interrupted **0 of 40** channels.
- **Never measure a cone with `IsWithinCombatRange`** — it adds *both* combat reaches, and his is 15,
  so asking for 25 answers yes out to 47.7. The cone check adds only the target's
  (`GetObjectSize`); `FlameLeviathanInConeRange` mirrors that.
- **Pursued 62374** picks a random vehicle every 31s and lasts 35s; he drives at it and casts
  `Battering Ram 62376` on `GetVictim()` inside `IsWithinCombatRange(victim, 15)`. Its target is
  `TARGET_DEST_TARGET_ENEMY` at radius index 20, so the blast is a **25 yd sphere centred on the
  pursued vehicle** and his facing is irrelevant. Measuring it off *him* caught only a third of real
  exposure while over half its activations were false alarms — and the lead chopper, parked 45 yd
  ahead, never tripped that test at all yet sat in the sphere 15–32% of the time. Distance to the
  pursued vehicle is the whole rule, and needs no switch prediction: it re-aims itself the moment the
  aura moves. Predicting the 31s cadence instead was tried, and was where those false alarms came
  from. The aura beats `GetVictim()`.
- **A Pursued siege engine outruns him if it starts at once.** Steam Rush (every 15 s, ~35 yd) makes
  ~9.3 yd/s against his 5.1–7.5. Escapes failed in the first seconds instead: the kite ran at
  `MOVEMENT_COMBAT`, the station walk in flight had the same priority, and `IsWaitingForLastMove`
  yields only to a strictly higher one, so the hull kept driving at him for 3–7 s. The kite now goes
  `MOVEMENT_FORCED`, and `ResetKite` drops the last-move priority when Pursued ends so the next dodge
  does not wait out a kite leg. A Pursued engine is out of the vent-interrupter election: in 4 of 12
  spans it turned to face him mid-escape.
- **Nothing else opens a RaidObs trace.** He never sets `IN_PROGRESS` (only `SPECIAL` /
  `NOT_STARTED` / `DONE`) and the unit he engages is a vehicle, not a roster player, so neither obs
  opener fires and five wipes left no trace at all. `FlameLeviathanEngaged` calls `MarkPull` to cover
  it, latched per instance and released when he leaves combat so a re-pull opens a fresh one.
- **Resolve him through the instance script** (`FlameLeviathanBoss`). The entry scan stops at
  `SightDistance` (100 yd), posts sit 98–188 yd from him, and `TickFlameLeviathan` resets the shared
  state on "no boss": `fl.pursued` flapped 26 times in one 2026-09-16 pull, each with a bot 117–147 yd out.
- **He accelerates all fight.** `Gathering Speed 62375` is `MOD_SPEED_ALWAYS +5%`, **stacks to 20**,
  600s, re-applied every 15s and cleared only on reset. Against `speed_run`: he goes 5.0 → **10.0**
  yd/s, a siege engine or demolisher is a flat 7.0, a chopper 14.0. He out-runs a siege engine after
  roughly two minutes, which is why the kite path has to be efficient and why Steam Rush, Increased
  Speed and Speed Boost carry the late fight.
- **`"attackers"` usually does not contain him.** `AttackersValue` walks the *bot player's*
  `GetThreatenedByMeList()`, and threat here belongs to the vehicle creature. He is resolved by entry
  instead; `"attackers"` is kept only for picking adds.
- **Steam Rush 62346** is `CHARGE_DEST` at `TARGET_DEST_CASTER_FRONT`: a forward dash along our own
  facing, so it only escapes when he is already behind us.
- **Shield Generator 64677** is `SCHOOL_ABSORB` **15** for 5s on a 60s cooldown. Wired because it is
  free, not because it matters.

## Vehicle spells that must be self-cast

Tar, Steam Rush, Speed Boost, Increased Speed and Shield Generator are aimed at the caster or its own
vehicle. They cannot go through `CanCastVehicleSpell` — a self-cast returns `SPELL_FAILED_BAD_TARGETS`,
which that helper does not tolerate — so they are gated on cooldown and power directly. They must
also be passed the vehicle base as their target: `CastVehicleSpell` turns the vehicle to face any
*other* target first, which would aim Steam Rush straight at him and drop the tar pool on the wrong
side of the chopper.

## One action owns all movement

`FlameLeviathanDriveAction` is the only thing that steers a vehicle, with internal precedence:
kite when Pursued → clear a hard-mode hazard → back out of Battering Ram → drive to a corner post →
detour to a crate below `ULDUAR_FL_CRATE_DETOUR_ENERGY` → hold station, the tar lead included. `fl.drive` names
the branch that owned each tick. Hazard clearance used to be a separate action at a higher priority; that is
the two-owner bounce with the priorities swapped, so it was folded in rather than re-ordered. The
action is wired to two trigger nodes, `drive urgent` at `ACTION_RAID + 3` and the routine one at
`+0.5` — the engine caches actions by name, so both nodes drive one instance and one latched
destination.

Positioning: siege engines and non-lead choppers hold his **rear arc**; demolishers hold a 50 yd band
and never close; one chopper (lowest guid, neither Pursued nor frozen) runs *ahead* of him, back
turned, so its tar pool lands in his path — clamped to stop short of the Battering Ram sphere, and
giving the slot up entirely when the chase leaves no room. Each class fans out by guid rank.

**Every stand distance is measured outward from his combat reach**, so a 50 yd band puts a demolisher
65 yd from his centre and the arrival deadband takes it past Hurl Pyrite Barrel's 70. `HoldStation`
clamps the demolisher against that range: a barrel that will not cast drops the Blue Pyrite stack the
raid does its boss damage with.

`FlameLeviathanVehicleMovementMultiplier` zeroes every other `MovementAction` while a bot is on an FL
vehicle, exempting only the drive action, boarding and `LeaveVehicleAction` — and it stays **inert
until he is engaged**, or the raid could never drive into the arena. The Void Reaver risk applies:
once it is live, a silently failing drive action means vehicles stand still all fight.

**"Engaged" is his combat, not the rider's.** Threat here belongs to the vehicle creature, so a bot
that drives far enough out drops combat mid-pull — which switched off the drive action and the
multiplier together and handed the wheel to `follow`. `FlameLeviathanEngaged` reads the boss's combat
first, falling back to the rider's so the approach still counts.

## The kite is a wall loop, not a corner run

`FlameLeviathanKiteRing()` builds eight nodes from the four `NPC_FREYA_WARD_TARGET` spawn points —
`(159.4, 64.1)`, `(382.9, 74.0)`, `(374.0, -141.0)`, `(157.7, -140.3)` — inset 15 yd off the walls
with each corner chamfered into two nodes 35 yd out. The direction is latched per instance on the
first Pursued and **never reversed**; nodes within 50 yd of him are skipped so the ring never routes
through him; and the vehicle **advances on approach at 30 yd, never on arrival**.

That last rule is the whole point. The old kite drove corner to corner and only switched once within
5 yd, so the vehicle buried itself in the corner while he cut the diagonal — and its advance was also
conditioned on him being within 50 yd, which made progress depend on already being caught.

## Hard mode

At pull, `ActivateTowers()` adds one empower aura per surviving tower and schedules that tower's
periodic ground event:

| Tower | Boss aura | Ground NPC | Hazard |
|---|---|---|---|
| Storm | 65076 | 33364 (8 spawn) | Static lightning strikes at 8 fixed marks, ~5s telegraph |
| Flame | 65075 | 33369 | Escort-path **moving** fire trail, drops fire every 2s |
| Frost | 65077 | 33108 (2 spawn) | Chases a random target, commits once it stops, strikes ~6 s later |
| Life | 64482 | 33367 (4 spawn) | Adds that live until killed — contain, not dodge |

**Mimiron's Inferno is a trail, not a circle.** 33369 walks a waypoint path and every 2 s summons
**33370**, each burning **30 s** — about fifteen 9 yd patches in a line behind a moving head, median
14 on the ground at once. Scanning only 33369 dodges the head and leaves the trail unseen: a hull
standing in one loses **51-54% of its health per 5 s** (`--inferno`), and on 2026-09-12 that was
**61%** of every hull point the fleet lost out of ~6% of hull-seconds. Both entries count now, and
`ClearHazard` fans off the radial to a point clear of *every* patch within 45 yd — straight out from
the nearest lands in the next as often as it escapes.

**It reaches further than 9 yd.** 62910 is a dynamic-object aura, and `DynObjAura::FillTargetMap`
tests `IsWithinDistInMap`, which adds both object sizes: 9 + 0.39 + the hull's, so a siege engine
(7.7) burns out to **17.1 yd** and a chopper to 10.4. The old flat 18 yd scan gave a siege engine
under a yard of warning, and on 2026-09-16 siege hulls lost 20–37%/5 s at 13–17.5 yd from a patch.

**The Life tower adds were never being shot, and "the kill-nearest-attacker loop covers it" was
wrong.** `FlameLeviathanVehicleAction` chose `boss ? boss : add`, so an add was only ever considered
with the boss dead or off-grid, and the `add` it fell back to came from the bot's own `"attackers"`
list — which is empty inside a vehicle, because threat here belongs to the vehicle creature. Every
seat fired at the boss and nothing else. On 2026-09-05, 72 adds spawned in one 287 s pull, the field
was never clear after 32 s, and the only damage they took came from bots' personal rotations leaking
past the movement multiplier: **22 s to kill one add**.

**The wards fire all pull, and the adds stay until killed.** `ActivateTowers` schedules `EVENT_FREYA`
**once** at 30 s, so four wards spawn one per arena corner (`ULDUAR_FL_ARENA_CORNERS`), each firing a
wave on its own 29 s timer from 34 s. Since core `f4763cc9e`, `npc_freya_ward_summon::IsSummonedBy`
makes each add `TEMPSUMMON_MANUAL_DESPAWN` and calls `DoZoneInCombat`: every player within 250 yd,
their pets and **their vehicle bases**, at zero threat. `ThreatManager::AddThreat` sends a rider's
threat to its vehicle (a gunner's through the turret to the hull), and the victim switches only at
110% melee / 130% ranged, so **the first hull to hit a fresh add holds it**. `CanAIAttack` skips riders
on Leviathan's seats and anything out of LOS. The 2026-09-05 add figures (a median 124 yd travelled,
20% of raid damage taken, hulls losing 2.57%/5 s with an add in melee against 1.96% without) are from
the old `npc_freya_ward`, which re-ran `SelectNearestTarget(200)` on every summon each wave.

**That script needs world DB update `2026_09_10_03.sql`**, which binds `npc_freya_ward_summon` to 33387
and 34275. Without it the adds run SmartAI and despawn at their summon duration — Ward of Life 3 s
(62907, DurationIndex 27), Lasher 10 s (62947, DurationIndex 1): on 2026-09-16, 49 of 58 vanished at
full health. `--adds` prints that diagnosis instead of scoring add handling. See the upstream-merge
pitfall in [../../engine/pitfalls.md](../../engine/pitfalls.md).

| Add | Entry | Health | AI |
|---|---|---|---|
| Writhing Lasher | 33387 | 190,260 | `npc_freya_ward_summon`: melee + `Lash 65062` on its victim every 2 s |
| Ward of Life | 34275 | 504,000 | same |

Against 230,498,304 boss health those pools are a rounding error, so **an add in a weapon's band
outranks the boss**. The guns that matter, all verified from `Spell.dbc`: **Fire Cannon 62358**
(siege *turret*, 10–70 yd, its missile 62357 lands **76k in a 20 yd sphere**) is the heaviest and
widest — it alone covers 60% of add-frames from station; **Mortar 62634** (demolisher gunner, 0–50,
11 yd splash) is free and has no minimum, covering what Fire Cannon's 10 yd floor cannot; both **Ram
62345** and **Ram 62308** are 15 yd cones that *knock back* (Effect 98). 82% of add-frames are inside
some band with nobody moving, so target selection does most of the work and repositioning little.

What each one is actually worth, which is what ranks them when several are in band:

| Weapon | Damage | Cost / cadence |
|---|---|---|
| Fire Cannon 62358 → 62357 | **76,000**, 20 yd sphere | 20 energy |
| Hurl Boulder 62306 → 62307 | **27,000**, 20 yd splash | free |
| Ram 62345 | **22,500** | 40 energy |
| Ram 62308 | **19,000** | free, 4 s cd |
| Mortar 62634 → 62635 | **11,100** + Flames 20,000 | free, 1 s cd |
| Sonic Horn 62974 | **6,300** | 20 energy |

**`FlameLeviathanBestAdd` ranks by neighbour count inside the weapon's own splash**, nearest breaking
ties — not by nearest alone. Adds clump at a median of 2, p75 3 and max 9 inside one 20 yd splash, so
the extra bodies are usually there to be had. It skips an add a posted engine is holding — victim a
siege hull inside `ULDUAR_FL_CORNER_HOLD_RADIUS` (30 yd) of a post — unless the shooter rides that
hull: a 76k Fire Cannon hit from elsewhere passes the 130% switch and drags the add out of the corner.

**Corner containment: one siege engine per corner, never all of them.** Each wave should spawn in front
of an engine parked `ULDUAR_FL_CORNER_STANDOFF` (12 yd) in from its corner and facing it, which takes
threat first, throws the adds back in (Ram's knockback is away from the caster) and cannons them there:
an add meleeing the hull is inside Fire Cannon's 10 yd minimum, one thrown back is outside it. A wave
is 694k (504k + 190k), about nine cannon shots against a 29 s interval. The posted turret shoots adds
within 30 yd of its post before anything else; a posted driver fires Ram only at what is in its cone.

- **Slots.** Siege hulls are ranked by guid once per pull, and ranks 1–4 own corners 0–3. Ranking the
  live ones renumbered everyone below a loss, so a dead hull leaves its corner unmanned instead.
- **The vent reserve never posts** (`fl.reserve`). A post is 98–188 yd from where he roams and
  Electroshock is a 25 yd cone, so posting every engine leaves Flame Vents running. The reserve is the
  usable, non-Pursued engine nearest him, kept until it freezes, dies or is Pursued: re-electing each
  scan would drag a posted engine in and back out for one Hodir's Fury. When it is not rank 0, rank 0
  takes its corner.
- **Posting starts at engage**, gated on his Tower of Life aura 64482, which `ActivateTowers` applies in
  `JustEngagedWith` 34 s before wave 1. The old gate was the first add seen, i.e. wave 1 itself, with
  engines standing 12–27 s of driving from their posts.
- **The commute rushes**: Steam Rush when the post is over 43 yd away and within the front 45°, 7.0
  yd/s otherwise. The corner drive stays `MOVEMENT_COMBAT`: a Fury dodge is `MOVEMENT_FORCED`, and an
  equal-priority move waits out the one in flight for up to `MaxWaitForMove` (5 s) of a 6.5 s fuse.
- **No reachability gate.** He came within 40 yd of any post for 0–1.7% of a pull.

**A cone weapon and a parked facing will fight each other.** Ram and Sonic Horn need the vehicle
turned, while `DriveTo`'s park block re-faces the boss every tick. `HoldStation` therefore faces
whatever the cast node is about to shoot, using the same bands — including the exclusion, since the
tar lead never shoots adds and so never turns for one.

**Hodir's Fury commits only on a stopped target.** `npc_hodirs_fury` picks a random target within
200 yd every 30 s and `MoveFollow`s it at **12 yd/s**, faster than any hull. `FollowMovementGenerator`
fires `MovementInform` only once its spline has finished **and** the target is within 0.5 yd of where
the path was issued, so it chases a moving vehicle indefinitely (69 s measured). On commit it stuns
itself; **5.0 s** later (4.8–5.3) it summons `NPC_HODIRS_FURY` 33212 overhead casting **62533**, and
`62297` lands **~1.1 s** after that (950–1164 ms). It stays stunned 5 s more, then retargets. So only a
stunned reticle is dangerous: `TickFlameLeviathan` stamps each reticle's stun and arms it for
`ULDUAR_FL_FURY_ARMED_MS` (6.5 s, `fl.fury`), re-arming one still stunned after 10 s since it
retargeted and stopped between scans. On 2026-09-16, 88–89% of the time hulls spent inside the Fury
scan was against a reticle that could not strike. Once armed it is a static mark, so dodge
**radially**. The strike carries `62297`: 10 yd, **60s stun**, `Mechanic 0` and no dispel type, so no
dispel, trinket or mechanic-clear touches it.

**Each hazard has its own reach** (`FlameLeviathanHazardReach`). Hodir's Fury (10 yd) and Thorim's
Hammer (7) are creature casts at a destination, and `WorldObjectSpellAreaTargetCheck` adds no target
size to those, so they are flat from the hull's centre; Inferno adds both sizes, above. The scan counts
a hazard within `ULDUAR_FL_TOWER_HAZARD_MARGIN` (8 yd) of its reach, and `ClearHazard` steps out to
reach + 16.

**Fire frees a frozen vehicle, and the demolisher already carries it.** `Hurl Boulder 62306` triggers
`Boulder 62307`, whose third effect triggers `Flames 65045`; the gunner's `Mortar 62634` → `62635`
triggers `Flames 65044`. `spell_linked_spell` maps both to `-62297`, comment *"Flames remove ice"*,
and a negative effect at `type 1` (`SPELL_LINK_HIT`) becomes `RemoveAurasDueToSpell` in
`Spell::DoAllEffectOnTarget`. **Aim either one at the frozen ally.** All three vehicle spells are
`TARGET_FLAG_DEST_LOCATION`, so the "target" only supplies a destination and a friendly one passes
every check, while Boulder's own damage is `TARGET_UNIT_DEST_AREA_ENEMY` and cannot hurt it — the
blast is enemy-only and the Flames are not. Both are free. Hurl Boulder has a **10 yd minimum**
(RangeIndex 164, 10–70); Mortar has no minimum but reaches only 50, so the gunner covers what the
driver's floor cannot. Freeing outranks damage: the stun is a full minute of nothing.

The trap that hid this for a session: `62297` has `Mechanic 0`, `DispelType 0` **and
`AuraInterruptFlags 0`**, so the DBC alone says nothing removes it, and grepping for what casts
`65044`/`65045` finds no `creature_template_spell` row, no `smart_scripts` action and no code
reference — because the cast is an `EffectTriggerSpell` two levels down from a spellbook entry.

**One station point per class stacked the fleet.** `FlameLeviathanRearPoint` gave a whole class one
spot, so four or more vehicles shared a single 10 yd circle for **50–90%** of a pull and six bots took
`62297` in the same millisecond. Stations now fan out by guid rank across an arc at the **same
radius**, so Ram, Sonic Horn and the pyrite band keep their geometry and one strike costs one vehicle.
A frozen vehicle keeps its slot — renumbering would swing the whole fan for 60s — but hands back the
tar-lead and vent-interrupt roles, which are elected on guid order and would otherwise go with it.

**Never ask a rider whether it can move.** Every vehicle passenger carries `UNIT_STATE_ROOT`, so
`FlameLeviathanCrewUsable`'s `UNIT_STATE_NOT_MOVE` test on the crew reported the whole fleet unusable
and switched the tar lead, the vent interrupt and the corner posting off together — from `e41a0e89a`
until it was found six pulls later, with nothing in the traces naming the gate. The rider is now
checked for `UNIT_STATE_STUNNED` only; the hull keeps the full test. `fl.vent`, `fl.reserve` and
`fl.drive` exist so the next silent election failure is visible.


## Baseline to beat — 2026-08-30, before the blast/freeze fixes

Measured with the committed tools (`postmortem.py --stalls` / `--clump`, `flame_leviathan.py`), so a
re-pull compares like for like. **Keep these two traces**: the chronicle logs for both were rotated
away on the 18:01 restart, so they cannot be regenerated.

| | trace | length | outcome |
|---|---|---|---|
| A | `603_1_flame-leviathan_1788099810.ndjson` | 379 s | `idle` — wipe |
| B | `603_1_flame-leviathan_1788100330.ndjson` | 303 s | **`kill`** |

**Ordered to move, didn't** (`--stalls`): A **194 s** total, worst window Shadow (siege) 60.0 s at
(305.0, −101.1) with 14 moves accepted; B **107 s**, worst 20.1 s. A passenger froze for the same
60.0 s in Shadow's engine — a *vehicle* freeze, not a bot one, with a Hodir's Fury reticle 1.3 yd
away for 64 consecutive frames.

**Share of the pull inside one Hodir's Fury** (`--clump 10`): ≥4 together A 62.2% / B 89.7%; ≥8
together A 18.1% / B 44.1%.

**Battering Ram exposure** (`--ram`): of the frames inside the real 25 yd blast the shipped gate
caught only **27.1%** (A) / **30.3%** (B), while **71.1%** / **58.8%** of its activations were false
alarms. Per station, 63-78% of real exposure was invisible to it. The gate is
`dist(vehicle, boss) <= 25 + size`; a chopper's CombatReach is 1.0, so it is 26 yd and **the lead
chopper never trips it** — it sat inside the blast 15.8% (A) / 30.3% (B) of the time.

**Hodir's Fury** (`--fury`): A 27 commits, 16 of 19 cleared 10 yd in 5 s, **3 caught**; B 20 commits,
34 of 36 cleared, **2 caught**. Median distance from the centre at +5 s: 18.3 / 17.6 yd.
## Baseline to beat — 2026-09-05, before the Freya-adds fixes

Three wipes with the Storm and Life towers live, boss floors 76.0% / 72.2% / **27.4%**;
`603_1_flame-leviathan_1788624223.ndjson` is the reference, being the only one where the collapse is
legible rather than immediate. The second baseline `…_1788628796.ndjson` (339 s, **36.4%**) is the
first pull carrying `418afbe49`. Reproduce any figure with `flame_leviathan.py <trace> --adds` /
`--ram` / `--fury` / `--vents`; **keep those traces**, because the chronicle logs that would
corroborate them have rotated away.

The Battering Ram fix above **worked** — the backoff went from catching 27–30% of real exposure to
**64.6%**, false alarms down from 71%/59% to **47.6%**. The add work landed too: median time to kill
one add **22 s → 13 s**, and `Lash` fell from **20.0%** of raid damage taken to **4.4%**. No `62297`
landed in any of the three, so the thaw is still unverified in the field.

**The fleet got worse anyway, and the reason is the whole fight.** Hulls melted ~25% faster, the first
bot was on foot at **80 s** against 180, and the boss floor went 27.4% → 36.4%. A seated bot is
invisible to boss AoE and a dismounted one absorbs all of it
([../../engine/pitfalls.md](../../engine/pitfalls.md)), so every bot on foot is a damage sink the
raid did not have before:

| spell | on bots out of a vehicle | on crewed bots |
|---|---|---|
| 63847 Flame Vents | 861,477 (318 hits) | 5,400 (2) |
| 62297 Hodir's Fury | 594,180 (4) | 0 |
| 62376 Battering Ram | 501,478 (21) | 0 |
| **62400 Missile Barrage** (`SPELL_FL_MISSILE_BARRAGE`) | 303,227 (106) | 8,909 (4) |

**So hull survival is the fight, not dodging.** A hull is worth roughly 1.8 M on a siege engine, 1.0 M
on a demolisher and 766-826 k on a chopper — the numbers the boss's 230,498,304 has to be spent
against.

**Nothing gated on `FlameLeviathanCrewUsable` ever ran.** `fl.corner` emitted **0** times and
`fl.station` never read `tar-lead` in any Sep-05 trace, because `Vehicle::AddPassenger` roots every
passenger, so the helper's `UNIT_STATE_NOT_MOVE` test on the rider was false for every crewed bot from
`e41a0e89a` onward.

**The corner posting was an accepted regression here**, left alone pending the vent-interrupt fix —
the boss floor got *worse* because both gunners now prefer any add in range over the boss. That fix
landed; see the 09-12 baseline below.

## Baseline to beat — 2026-09-12, four towers up

`603_4_flame-leviathan_1789222298` (211 s, floor **69.1%**) and `_1789222754` (318 s, floor
**64.0%**): the first traces of `8823616d4`, and the first with all four towers standing.

**The crew-usable fix landed.** Flame Vents channels cut short went 2 of 13 → **6 of 6** and **8 of
12**, Electroshock 6/6 and 10/7, and `tar-lead` and `fl.corner` were both elected for the first time.
Hodir's Fury: 3 of 4 then **6 of 6** cleared the fuse. Thorim's Hammer is a non-event at 0.03%.

**Mimiron's Inferno replaced all of it as the hull killer** — 61% of every hull point lost, above.
All 58 deaths were bots **on foot**; none died crewed.

**Pyrite is the boss-damage hole.** Demolishers spent 56% of crewed frames past the barrel's 70 yd,
held 2.7–4.7 of 10 stacks, and one sat 70 s at zero.

**Corner posting was elected but never arrived** — median 126–209 yd from post, one engine inside
15 yd for 2% of frames — because `follow` kept taking the wheel. Fine-grain driving is clean
(path/net 1.3–1.5 for siege and demolishers); the oscillation was that tug-of-war, ~30 s a cycle.

## Baseline to beat — 2026-09-16, four towers up

`603_2_flame-leviathan_1789584377` (383 s, floor **51.5%**) and `_1789584936` (265 s, floor **68.9%**),
25-man, built 0.2 h after `abf4f4d04`, before the Fury, Inferno, crate, reserve, containment and
boss-lookup fixes above. All 15 hulls died in each, and all 46 bot deaths came on foot, a median
10.8 s / 8.1 s after leaving the hull. The world DB lacked `2026_09_10_03`, so no add figure here
means anything. Reproduce with `flame_leviathan.py <trace>`.

| | `…4377` | `…4936` | target |
|---|---|---|---|
| hull loss: nothing positional (Missile Barrage) | 40.1% | 46.0% | attributed by spell (v13) |
| hull loss: Flame Vents / Battering Ram / Inferno | 29.6 / 21.0 / 9.1% | 24.1 / 12.8 / 16.9% | Inferno under 5% |
| Vents channels run full with nobody firing | 3 | 2 | 0 |
| Pursued spans started within 40 yd: hull health lost, median | 16 pts | 56 pts | under 15 |
| first accepted kite move in those | +0.9 to +7.3 s | +0.3 to +4.8 s | under 0.5 s |
| Fury scan time against a reticle that could not strike | 88% | 89% | 0 |
| Fury strikes that caught a hull | 0 of 21 | 0 of 11 | 0 |
| bot Grab Crate repeats inside the despawn, each credited | 31 of 68 | 32 of 63 | - |
| demolisher mean stacks / frames past 70 yd | 3.5–7.2 / 23–47% | 2.8–7.4 / 22–31% | past 70 yd under 15% |
| stacks lost with the demolisher in range when barrels stopped | 8 of 15 | 4 of 8 | `late` 0–1 per pull |
| first siege engine at a post | 0:56 | 0:47 | before wave 1 at 0:34 |

## Known gaps

- **Boarding depends on the raid leader.** `FlameLeviathanVehicleNearTrigger` returns false unless
  `master->GetVehicle()` — the Oculus `GroupFlyingTrigger` defect, where one human who has not
  mounted freezes the whole raid. On 2026-09-16 it fired 1,091 / 597 times and never ran, so crews
  whose hull died stayed on foot.
- **The tar lead runs but is unmeasured.** It is elected in both 09-12 traces and the note only
  emits when `FlameLeviathanTarLeadDistance`'s clamp opens — lead capped at
  `dist(boss, pursued) − bossReach(15) − BATTERING_RAM_RADIUS(25) − size`, needing the pursued vehicle
  more than ~40 yd out — so the role does drive. What it is worth is still unmeasured.
- **The 09-16 fixes are unverified in the field.** Parked vehicles let a reticle commit more often,
  so `--fury` must keep catching no hull. Siege engines boxed out by the Inferno trail at 25 yd may
  drop the vent interrupt. The kite ring's chamfer passes ~28 yd from each post, inside Ram's sphere
  round a Pursued vehicle driving it. An add thrown against the wall may land inside Fire Cannon's
  minimum, and `DoZoneInCombat` leaves the first victim arbitrary until someone hits the add. `--corners`
  scores the last three once the DB update is in.
- **A siege engine at station starts every Pursued span inside Ram's cast range**
  (`ULDUAR_FL_SIEGE_STAND_DIST` 8 against a 7.7 hull). If the first Ram still lands now that the kite
  starts at once, back off before the 31 s switch.
- **Vezax's `hold cast outside field` multiplier fires during this fight** and vetoed resurrections on
  2026-09-16: a Vezax gate leak.
- **No chopper pyrite ferry**, so crates only reach a demolisher that drives to them itself.
- **The pyrite refresh timing is unverified in the field.** `ULDUAR_FL_PYRITE_REFRESH_SLACK_MS` (2 s)
  assumes ~1.1 s between decisions: `--pyrite` losses blamed on `late` mean it runs short, and on `fail`
  a refused refresh, which costs the whole stack. The seat's +25 is read off the DBC and `conditions`,
  not a trace.
- **No seat-shortfall fallback.** With zero slack, a bot that loses a boarding race is left on foot.
- **`PlayerbotAI::CastVehicleSpell(uint32, float, float, float)` is declared and never defined**
  (`PlayerbotAI.h:558`), so there is no ground-targeting and tar cannot be ignited deliberately.

