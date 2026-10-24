# Flame Leviathan

The whole fight is from vehicles; the engine-level vehicle rules are in
[raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md#vehicles-and-flight).

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

**Hull survival is the fight.** A crewed bot is invisible to boss AoE and a dismounted one absorbs
all of it ([pitfalls](../../engine/pitfalls.md#role-and-index-traps)), so every bot on foot is a
damage sink. A hull is worth roughly 1.8 M on a siege engine, 1.0 M on a demolisher and 766-826 k on
a chopper.

## Gunners can steer, which is why nothing gates on `IsInVehicle(true)`

Seat 0 of **both** the turret (33067) and the mechanic seat (33167) carries `CAN_CONTROL`, so
`MovementAction::MoveTo` passes its `seat->CanControl()` check for a *gunner* and calls `DoMovePoint`
on the turret creature bolted into seat 7. Movement is gated on `vehicleBase->GetEntry()` instead.
No seat a bot can occupy has `CAN_ATTACK` — that flag is on the accessory seats, which hold NPCs — so
`CastSpellAction::isUseful` already blocks class spells and no targeting-suppression multiplier is
needed. Siege engine seats 1 and 2 have **no flags at all**; the chopper rear seat and demolisher
seat 3 are `CAN_ENTER_OR_EXIT` only.

`instance_ulduar.cpp` spawns **2 of each vehicle in 10-man, 5 in 25-man** at `x = 119.8`, west of the
arena: exactly 10 / 25 usable slots, **zero slack**.

## The demolisher runs on pyrite, not energy

Blue Pyrite is the raid's boss damage: in a clean kill his loss rate ran 32%/min on 33 fleet stacks
and 12%/min on 3.

`Creature::Regenerate` returns early without `UNIT_FLAG2_REGENERATE_POWER`. The siege engine, turret
and chopper have it (+20 per 2s); **the demolisher and its mechanic seat have `unit_flags2 = 0` and
never regenerate**. Max pool is 100 for all of them.

So a full demolisher is **20 barrels**, then free Hurl Boulder forever unless crates arrive. Crates:
80 `Mechanolift 304-A` (33214) spawns each carry a `33218 Pyrite Safety Container` in seat 1; shoot
one down and the container falls, `MovementInform` summons `33189 Liquid Pyrite`, and the demolisher
gunner's Grab Crate runs `62496 → 62473 Reload Ammo` = `SPELL_EFFECT_ENERGIZE` **+25 to the
demolisher**. Anti-Air Rocket reaches 1000 yd, so the gunners choose where crates land.

**Shoot a lift over the arena, at its closest pass to the centre** (`FindMechanolift`). The container
falls straight down and demolishers ride a median 30-35 yd from the centre: a crate within 20 yd of it
has a live demolisher inside the 49 yd grab reach 86-96% of the time, one 80-100 yd out 14-37%. Lifts
fly facing their course (within 1° p90), so the centre falling behind one marks the pass; inside
`ULDUAR_FL_LIFT_DROP_RADIUS` (20) it fires at once. Holding costs no crates: lifts respawn in 30 s and
all 47 spawns on loops crossing the box are still shot once a pass. Only one loop (path `13682000`,
10 spawns) crosses the middle; the other 37 bottom out 81-88 yd from the centre, so most drops gain
little. `fl.lift` records each shot.

**Every grab credits, repeats included.** `spell_vehicle_grab_pyrite` runs on each 62482 hit and only
despawns the crate 1300 ms later. The gunner's 62496 goes to the nearest 33167 (`conditions`
13/1/62496), its own seat, which force-casts 62473: +25 to itself (`TARGET_UNIT_CASTER`) and +25 to
its vehicle (`TARGET_UNIT_VEHICLE`). Every traced repeat grab drew its own 62496 (31/31, 33/33), so
gunners hold no claim: another demolisher's repeat is real energy, and the same gunner's mostly
overflows past 100. The seat pays for Increased Speed, so nothing reserves the demolisher's energy. A
demolisher under `ULDUAR_FL_CRATE_DETOUR_ENERGY` detours only to 41 yd of a crate (the gunner grabs
from 49), and only while that stop stays within his reach + `FlameLeviathanDemolisherStandDist` + the
deadband of his centre: unleashed, starved demolishers sat past the barrel's 70 yd for 22–47% of a pull.

**Hurl Pyrite Barrel** needs no ammo (no `CasterAuraSpell`), only 5 energy, and shares Hurl Boulder's
10–70 yd range (RangeIndex 164). It lands `62489` (54000 in 20 yd), which applies `68605 Blue
Pyrite`: 10 s, 10 stacks, one per landing. Every landing also resets the duration, at 10 stacks too
(`Aura::ModStackAmount`), and a periodic damage refresh keeps its tick timer
(`AuraEffect::CalculatePeriodic`), so a refresh landing just before expiry loses no tick. Each
demolisher carries its **own** aura instance, read through the caster-scoped overload.

**Burst to 6, then refresh just before expiry.** On a ~9 s refresh cycle, a refresh at 10 stacks buys
~90 stack-seconds, while burst barrel *k* only brings stack *k* forward by the cycles it skips: #2 81,
#6 45, #7 36, #10 9. `DemolisherAction` bursts to `ULDUAR_FL_PYRITE_BURST_STACKS` (6), or to 10 while
energy covers the rest plus `ULDUAR_FL_PYRITE_REFRESH_RESERVE` (four refreshes), counting barrels in
flight. It then throws one refresh once the duration is down to the last timed flight +
`ULDUAR_FL_PYRITE_REFRESH_SLACK_MS`, each adding a stack on the way to 10. A landing is timed off our
own aura: a stack gained, or the duration jumping back up. Barrels go on him only. Refreshing at a
fixed time left, blind to barrels in flight, threw 2–3 per cycle at 10 stacks (~15–20 a minute
against ~7).

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
  (`SPELL_ATTR1_IS_SELF_CHANNELED`) — ticking 63847 for 15000 in a 500 yd radius, so it is not
  dodgeable. `EVENT_VENT` repeats every 20s. `boss_flame_leviathan::SpellHit` maps Electroshock to
  `InterruptNonMeleeSpells`, which always cancels a channel, so **the interrupt is real**. He is
  `MECHANIC_INTERRUPT`-immune, but only Electroshock's effect 1 carries that mechanic; effect 2
  (8489 damage) lands, the spell hits, and the script hook fires.
- **Ram, Electroshock and Sonic Horn are `TARGET_UNIT_CONE_ENEMY_104`**, and a cone is two limits.
  `Spell::CheckRange` returns OK immediately for `RangeEntry->ID == 1`, which is what Electroshock
  and Sonic Horn carry, so for those two the **effect radius** is the reach: 25 and 35 yd. **Ram is
  not one of them** — its `RangeIndex` is 11, a real 0–15 entry, so `CheckRange` enforces **15 yd**
  and the 18 yd effect radius never applies. `world.spell_cone` is the **width**: Ram 100°,
  Electroshock 60°, Sonic Horn 50°, and the demolisher's Ram 62308 also 100° — check the **live**
  table, because `data/sql/base/db_world/spell_cone.sql` has no row for any of them and reads as
  "engine default" (`Spell::SelectImplicitConeTargets` falls back to 60°). `isInFront` passes no
  target radius, so his 15 yd reach widens the arc by nothing, and all three are narrower than
  `CastVehicleSpell`'s turn arc ([facing](../../engine/pitfalls.md#arc-and-geometry-defaults)). Gate
  on both limits and turn the vehicle yourself: ungated, five wipes fired 71 Electroshocks, landed **one**, and
  interrupted **0 of 40** channels.
- **Measure the cone the way the core does** (`FlameLeviathanInConeRange`): it adds only the target's
  `GetObjectSize`, while `IsWithinCombatRange` adds both reaches
  ([reach](../../engine/raid-mechanics-lessons.md#range-and-reach)) and with his 15 answers 25 yd
  out to 47.7.
- **One Electroshock per channel.** `FlameLeviathanIsVentInterrupter` elects the usable, non-Pursued
  engine in cone range with the most energy, and its first shot claims the channel
  (`FlameLeviathanClaimVentChannel`, `fl.interrupter`) until he stops; a claimant that freezes hands it
  back. Ranking alone re-elected every tick, since each 20-energy cast promoted the next, and four
  engines emptied 80 energy into one channel 30 ms apart. The Ram gate earmarks Electroshock's 20
  for the current interrupter. A miss (the channel still running after the cast, `fl.vent miss`)
  retries after `ULDUAR_FL_ELECTROSHOCK_RETRY_MS` (3 s): charging the 10 s cooldown left every engine
  10 s out of step with the 20 s cycle.
- **Pursued 62374** picks a random crewed siege engine or demolisher, never a chopper
  (`FlameLeviathanPursuedTargetSelector`), every 31s and lasts 35s. `EVENT_PURSUE` waits out his
  casts, so it never lands mid-channel, and re-fires at once when he kills his victim. He drives at
  it and casts `Battering Ram 62376` on `GetVictim()` inside `IsWithinCombatRange(victim, 15)`. Its
  target is `TARGET_DEST_TARGET_ENEMY` at radius index 20, so the blast is a **25 yd sphere centred
  on the pursued vehicle** and his facing is irrelevant
  ([pitfalls](../../engine/pitfalls.md#arc-and-geometry-defaults)). Distance to the vehicle carrying the
  aura, not `GetVictim()`, is the whole rule and needs no switch prediction: it re-aims itself the moment the aura moves, and
  predicting the 31s cadence was where the false alarms came from. With nobody holding the aura (the
  Pursued hull died, up to 31 s) he rams his threat victim — 16 of one pull's 46 blasts, mostly on
  hulls holding station — so `fl.ramtarget` names that victim's hull: it kites like a Pursued one and
  the rest back off it. Effect 3 is `MOD_DAMAGE_PERCENT_TAKEN` **+50% per blast, 20 stacks**, on the
  target and everything in the 20 yd splash (radius index 9), so a chain is exponential and only the
  first blast is cheap: 8 in 22 s ran 27k up to 149k and took the hull.
- **A Pursued siege engine outruns him if it starts at once.** Steam Rush (every 15 s, ~35 yd) makes
  ~9.3 yd/s against his 5.1–7.5. The kite goes `MOVEMENT_FORCED`, or the station walk already in
  flight holds the lock and the hull drives at him for 3–7 s
  ([movement lock](../../engine/pitfalls.md#movement-that-silently-no-ops)), and `ResetKite` drops the
  last-move priority when Pursued ends so the next dodge does not wait out a kite leg. A Pursued
  engine is out of the vent-interrupter election: in 4 of 12 spans it turned to face him mid-escape.
  A Pursued demolisher runs 14 yd/s on Increased Speed, cast by its gunner and noted `fl.speed`
  because no instant leaves a `cast` row; the crew of his threat victim gets it too. Neither hull
  detours round the Inferno trail while he is in range to fire: in one pull 62% of all Ram hull damage
  landed on `kite:detour` legs, against 0.7% of fleet health lost to the trail.
- **Nothing else opens a RaidObs trace.** He never sets `IN_PROGRESS` (only `SPECIAL` /
  `NOT_STARTED` / `DONE`) and the unit he engages is a vehicle, not a roster player, so neither obs
  opener fires. The encounter's tick (`FlameLeviathanTick`) calls `MarkPull` to cover it, latched per
  instance and released when he leaves combat so a re-pull opens a fresh one.
- **Resolve him through the instance script** (`FlameLeviathanBoss`): posts sit 98–188 yd from him,
  past the sight-capped entry scan, and `TickFlameLeviathan` resets the shared state on "no boss"
  ([pitfalls](../../engine/pitfalls.md#things-the-bot-cannot-see)). That finds him anywhere in
  Ulduar, so the weapon target takes him only once he is in combat: otherwise every seat on the
  approach aims at him, non-attackable 1000 yd off, and shoots no trash.
- **He accelerates all fight.** `Gathering Speed 62375` is `MOD_SPEED_ALWAYS +5%`, **stacks to 20**,
  600s, re-applied every 15s and cleared only on reset. Against `speed_run`: he goes 5.0 → **10.0**
  yd/s, a siege engine or demolisher is a flat 7.0, a chopper 14.0. He out-runs a siege engine after
  roughly two minutes, which is why the kite path has to be efficient and why Steam Rush, Increased
  Speed and Speed Boost carry the late fight.
- **`"attackers"` usually does not contain him, and may miss the trash.** `AttackersValue` walks the
  *bot player's* `GetThreatenedByMeList()`, and `ThreatManager::AddThreat` sends a rider's threat to its
  vehicle. He is resolved through the instance instead, and short of him the weapons fall back from
  `"attackers"` to the nearest hostile already in combat in `"possible targets"`. Adds are picked by
  `FlameLeviathanBestAdd` for the same reason (below).
- **Missile Barrage 62400** (`SPELL_FL_MISSILE_BARRAGE`, every 4 s) is the hull-loss floor: nothing
  positional answers it, and it ran 18–46% of hull damage in every traced pull.
- **Steam Rush 62346** is `CHARGE_DEST` at `TARGET_DEST_CASTER_FRONT`: a forward dash along our own
  facing, so it only escapes when he is already behind us. It starts 2 s of GCD category 133, which
  Electroshock and Ram share.
- **Shield Generator 64677** is `SCHOOL_ABSORB` **15** for 5s on a 60s cooldown. Wired because it is
  free, not because it matters.

## Vehicle spells that must be self-cast

Tar, Steam Rush, Speed Boost, Increased Speed and Shield Generator are aimed at the caster or its own
vehicle, so `CanCastVehicleSpell` cannot vet them
([vehicles](../../engine/raid-mechanics-lessons.md#vehicles-and-flight)); they are gated on cooldown
and power directly. They must also be passed the vehicle base as their target: `CastVehicleSpell`
turns the vehicle to face any *other* target first, which would aim Steam Rush straight at him and
drop the tar pool on the wrong side of the chopper. `CastVehicleSelfSpell` skips a spell inside its
category GCD and stamps the cooldown only once the energy has gone (a free spell has no receipt and
stamps on the call).

## One action owns all movement

`FlameLeviathanDriveAction` is the only thing that steers a vehicle, with internal precedence:
kite when Pursued → clear a hard-mode hazard → back out of Battering Ram → drive to a corner post →
detour to a crate below `ULDUAR_FL_CRATE_DETOUR_ENERGY` → hold station, the tar lead included.
`fl.drive` names the branch that owned each tick. Hazard clearance is folded in rather than given its
own action, since two owners of one vehicle bounce. The action is wired to two trigger nodes,
`drive urgent` at `ACTION_RAID + 3` and the routine one at `+0.5` — the engine caches actions by name,
so both nodes drive one instance and one latched destination.

**The drive never waits out its own leg.** `DriveToImpl` drops the stored priority one step before
re-issuing and restores it if nothing issued
([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)); a strictly higher leg, a FORCED
dodge, still blocks. Equal legs replace each other, so anything that must commit to one latches it
itself: the kite its node, `ClearHazard` its goal (below).

Positioning: siege engines and non-lead choppers hold his **rear arc**; demolishers hold a 50 yd band
and never close, their points led along his heading by `ULDUAR_FL_STATION_LEAD_S` (1.5 s of his run
speed, capped at `_MAX` 10 yd, none while he stands), since a point built off where he is now is short
by the hull's drive time. The point sits behind him, so the lead moves it closer, the safe way to be
wrong, and the cap keeps the band ~44 yd from his centre, clear of Ram's 25; a chopper at 20 yd would
be led into the sphere, so demolishers only. One chopper (lowest guid, neither Pursued nor frozen)
runs *ahead* of him, back turned, so its tar pool lands in his path — clamped to stop short of the
Battering Ram sphere, and giving the slot up entirely when the chase leaves no room. Each class fans
out by guid rank. The demolisher fan swings from his rear toward the arena centre
(`FlameLeviathanCentreTurn`), capped so its outer slot stays in his rear half: he runs a median 55 yd
from the centre, a demolisher out-drives him by only 0.7–1.5 yd/s, and unswung they trailed on the
outside of his circle in 65% of their out-of-range station frames.

**Every stand distance is measured outward from his combat reach**, so a 50 yd band puts a demolisher
65 yd from his centre and the arrival deadband takes it past Hurl Pyrite Barrel's 70. `HoldStation`
clamps the demolisher against that range: a barrel that will not cast drops the Blue Pyrite stack the
raid does its boss damage with.

The `flame leviathan vehicle movement` rule (`Exclusive`) zeroes every other `MovementAction` while a
bot is on an FL vehicle, exempting only the drive action, boarding and `leave vehicle` — and it stays
**inert until he is engaged**, or the raid could never drive into the arena. Once it is live, a
silently failing drive action means vehicles stand still all fight.

**"Engaged" is his combat, not the rider's** (`FlameLeviathanEngaged`, boss first, rider as the
fallback for the approach; [pitfalls](../../engine/pitfalls.md#before-the-pull-and-out-of-combat)).

## The kite is a wall loop, not a corner run

`FlameLeviathanKiteRing()` builds eight nodes from the four `NPC_FREYA_WARD_TARGET` spawn points —
`(159.4, 64.1)`, `(382.9, 74.0)`, `(374.0, -141.0)`, `(157.7, -140.3)` — inset 15 yd off the walls
with each corner chamfered into two nodes 35 yd out. The direction is picked per kite, toward the
neighbour node farther from him, and **never reversed** mid-kite (a pull-wide latch sent a demolisher
134 yd out up the wall into him); nodes within 50 yd of him are skipped so the ring never routes
through him; and the vehicle **advances on approach at 30 yd, never on arrival**: switching on
arrival buried the vehicle in the corner while he cut the diagonal.

**The kite steers round the Inferno trail itself**, because it outranks the hazard dodge. The trail
loops 33–56 yd inside the ring, and every Inferno hit in one pull (1.19 M) landed on a kiting hull
driving one straight leg across it. `DetourAroundFire` checks the next 30 yd of the leg and fans a
25 yd detour that never closes on him (`fl.drive` `kite:detour`). Station legs take the same detour,
free to close on him (`station:detour`): without it the trail walled the vent reserve off for 20 s
while he drove 90 yd away, and a 10-tick channel went through.

## Hard mode

At pull, and only then (`ACTION_LEVIATHAN_REFRESH_TOWERS` is never sent), `ActivateTowers()` adds one
empower aura per surviving tower and schedules that tower's periodic ground event, so a tower downed
mid-fight changes nothing. The towers stand 360-560 yd from the arena anyway.

| Tower | Boss aura | Ground NPC | Hazard |
|---|---|---|---|
| Storm | 65076 | 33364 (8 spawn) | 8 marks at once in a 200x200 box, each firing 5-20 s later |
| Flame | 65075 | 33369 | Escort-path **moving** fire trail, drops fire every 2s |
| Frost | 65077 | 33108 (2 spawn) | Chases a random target, commits once it stops, strikes ~6 s later |
| Life | 64482 | 33367 (4 spawn) | Adds that live until killed — contain, not dodge |

Each aura is **+40% max health** (aura 133, base points 39), so four towers put him at 1.4^4 =
**3.84x** his 70.0 M template pool: `mhp` 268.9 M since `DungeonScale.DisabledHealthID` exempted him
from `mod-dungeon-scale`, 322.7 M before. **A pull carrying a one-frame health step is not a damage
baseline** ([the rescale](../../engine/pitfalls.md#configuration)).

**Mimiron's Inferno is a trail, not a circle.** 33369 walks a waypoint path and every 2 s summons
**33370**, each burning **30 s** — about fifteen 9 yd patches in a line behind a moving head, median
14 on the ground at once. Scanning only 33369 dodges the head and leaves the trail unseen: a hull
standing in one loses **51-54% of its health per 5 s** (`--inferno`), and unseen it took **61%** of
every hull point the fleet lost out of ~6% of hull-seconds. Both entries count, and `ClearHazard`
fans off the radial to a point clear of *every* patch within 45 yd — straight out from the nearest
lands in the next as often as it escapes. A dodge re-planned off the nearest patch flips sides of the
trail ([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)), so `ClearHazard` latches
its goal while it stays clear, and the leg may enter no reach the hull is not already in.

**It reaches further than 9 yd**, by the hull's size
([persistent area auras](../../engine/raid-mechanics-lessons.md#range-and-reach)): a siege engine
(7.7) burns out to **16.7 yd** and a chopper to 10.0. The hazard check adds the dynobject's 0.39 as margin. A flat 18 yd scan gave a siege engine under a
yard of warning: siege hulls lost 20–37%/5 s at 13–17.5 yd from a patch.

**The wards fire all pull, and the adds stay until killed.** `ActivateTowers` schedules `EVENT_FREYA`
**once** at 30 s, so four wards spawn one per arena corner (`ULDUAR_FL_ARENA_CORNERS`), each firing a
wave on its own 29 s timer from 34 s. `npc_freya_ward_summon` makes each add
`TEMPSUMMON_MANUAL_DESPAWN` and calls `DoZoneInCombat`: every player within 250 yd, their pets and
**their vehicle bases**, at zero threat. `ThreatManager::AddThreat` sends a rider's threat to its
vehicle (a gunner's through the turret to the hull), and the victim switches only at 110% melee /
130% ranged, so **the first hull to hit a fresh add holds it**. `CanAIAttack` skips riders on
Leviathan's seats and anything out of LOS.

**The adds live only with world DB update `2026_09_10_03.sql` and core `4d4ae4f95`.** The update binds
`npc_freya_ward_summon` to 33387 and 34275; without it they run SmartAI (49 of 58 vanished at full
health). Without `4d4ae4f95` bound adds still leave at their summon duration — Ward of Life 3 s (62907,
DurationIndex 27), Lasher 10 s (62947, DurationIndex 1); the mechanism is in
[pitfalls](../../engine/pitfalls.md#upstream-merges). `--adds` prints that diagnosis instead of
scoring add handling.

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
**The claim lapses where the holder has no shot** — inside Fire Cannon's floor and outside Ram's cone
([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)).

**Corner containment: one siege engine per corner, never all of them.** Each wave spawns in front of an
engine parked `ULDUAR_FL_CORNER_STANDOFF` (12 yd) in from its corner, which takes threat, throws the
adds back in (Ram's knockback is away from the caster) and cannons them there: an add meleeing the hull
is inside Fire Cannon's 10 yd minimum, one thrown back is outside it. A wave is 694k (504k + 190k),
about nine cannon shots against a 29 s interval. The posted turret shoots adds within 30 yd of its post
before anything else. `DriveTo` parks anywhere inside `ULDUAR_FL_ARRIVE_TOLERANCE`, so the hull really
sits 16-21 yd from the spawn: the gunner opens the wave and Ram is for what reaches melee.

- **Slots.** Siege hulls are ranked by guid once per pull, and ranks 1–4 own corners 0–3. Ranking the
  live ones renumbered everyone below a loss, so a dead hull leaves its corner unmanned instead.
- **The vent reserve never posts** (`fl.reserve`). A post is 98–188 yd from where he roams and
  Electroshock is a 25 yd cone, so posting every engine leaves Flame Vents running. The reserve is the
  usable, non-Pursued engine nearest him, kept until it freezes, dies or is Pursued: re-electing each
  scan would drag a posted engine in and back out for one Hodir's Fury. When it is not rank 0, rank 0
  takes its corner.

  Holding the siege station does not keep it in reach: he drives 38–93 yd per channel, and a reserve
  on station started 42–129 yd from his centre (reach 40) and never got inside in any of 10 full
  channels. So it Steam Rushes when over a charge from his edge, 2–5 s before the next channel
  (`EVENT_VENT` every 20 s, so the rush's GCD is over when the shock is due) or at once into a running
  one, never landing in the ram sphere, and never outside that window: the rush is its only escape
  once Pursued, and a dash to close spent it 3-6 s before two switches. Between channels it waits
  outside Battering Ram's reach, `ULDUAR_FL_BATTERING_RAM_CAST_RANGE` + its 7.7 reach + the 8 yd
  deadband (45.7 yd from his centre against Ram's 37.7 and Electroshock's 40), and closes
  `ULDUAR_FL_VENT_APPROACH_LEAD_MS` (2 s) before one. Parked at the 8 yd siege station instead, it was
  the Pursued hull in 8 spans over three pulls and lost a median ~50 pts. The interrupt stops the hull
  before turning ([facing](../../engine/pitfalls.md#arc-and-geometry-defaults)): a reserve that drove past him at 22 yd,
  33–87° off, never fired.

  **It is also the only interrupter.** With four corners manned the other engines sat 51-211 yd out
  for a whole lost channel, and `FlameLeviathanIsVentInterrupter` needs cone range, so a dodge that
  walks the reserve past 40 yd drops the duty rather than a shot. Inside the vent window it therefore
  dodges a Hammer mark on the strict circle (margin 0, not the 8 yd band), steps to a point that keeps
  him in cone range rather than the nearest clear one, and holds Steam Rush back while a dodge is due:
  a `MOVEMENT_FORCED` leg cancels the charge, which cost 40 energy 0.2 s after the rush went out.
  **Hammer marks only**, never the Inferno ([warning bands](../../engine/pitfalls.md#movement-that-silently-no-ops)),
  and the rush never charges across fire.
- **Posting starts at engage**, gated on his Tower of Life aura 64482, which `ActivateTowers` applies in
  `JustEngagedWith` 34 s before wave 1; gated on the first add seen, engines stood 12–27 s of driving
  from their posts.
- **The commute rushes**: Steam Rush when the post is over 43 yd away and within the front 45°, 7.0
  yd/s otherwise. The corner drive stays `MOVEMENT_COMBAT`, below the `MOVEMENT_FORCED` Fury dodge,
  which must never wait out a corner leg: up to `MaxWaitForMove` (5 s) of a 6.5 s fuse.
- **No reachability gate.** He came within 40 yd of any post for 0–1.7% of a pull.

**A post that will not turn cannot kill what it holds.** Taking the add is not the problem — a posted
engine out-threatened the chopper the zone-engage picked within **1.0-3.2 s**, for 8 of 8 adds — but
the add then closes inside the cannon floor and comes back at the engine from the arena side. So a
posted engine owes its facing to the corner only while it has nothing to shoot; with an add inside
Ram's 15 yd it turns onto it, and the corner drive faces the same add the cast node picked. Facing
locked to the corner, 71.7% of held frames had neither gun bearing, Wards of Life lived 24-52 s, and
Lash was 15.3% of hull damage. `--corners` scores the hold, not the first victim.

**A cone weapon and a parked facing will fight each other.** Ram and Sonic Horn need the vehicle
turned, while `DriveTo`'s park block re-faces the boss every tick. `HoldStation` therefore faces
whatever the cast node is about to shoot, using the same bands — including the exclusion, since the
tar lead never shoots adds and so never turns for one. Nor does the vent reserve: it Rams only what is
already in its cone and keeps facing him, because Electroshock is a 25 yd, 60° cone and a reserve
turned onto an add would spend a tick turning back at every channel.

**Hodir's Fury commits only on a stopped target.** `npc_hodirs_fury` picks a random target within
200 yd every 30 s and `MoveFollow`s it at **12 yd/s**, faster than any hull. `FollowMovementGenerator`
fires `MovementInform` only once its spline has finished **and** the target is within 0.5 yd of where
the path was issued, so it chases a moving vehicle indefinitely (69 s measured). On commit it stuns
itself; **5.0 s** later (4.8–5.3) it summons `NPC_HODIRS_FURY` 33212 overhead casting **62533**, and
`62297` lands **~1.1 s** after that (950–1164 ms). It stays stunned 5 s more, then retargets. So only a
stunned reticle is dangerous: `TickFlameLeviathan` stamps each reticle's stun and arms it for
`ULDUAR_FL_FURY_ARMED_MS` (6.5 s, `fl.fury`), re-arming one still stunned after 10 s since it
retargeted and stopped between scans. Unarmed, 88–89% of the time hulls spent inside the Fury scan was
against a reticle that could not strike. Once armed it is a static mark, so dodge **radially**. The
strike carries `62297`: 10 yd, **60s stun**, `Mechanic 0` and no dispel type, so no dispel, trinket or
mechanic-clear touches it.

**Each hazard has its own reach** (`FlameLeviathanHazardReach`). Hodir's Fury (10 yd) and Thorim's
Hammer (7) are creature casts at a destination, and `WorldObjectSpellAreaTargetCheck` adds no target
size to those, so they are flat from the hull's centre; Inferno adds both sizes, above. The scan counts
a hazard within `ULDUAR_FL_TOWER_HAZARD_MARGIN` (8 yd) of its reach, and `ClearHazard` steps out to
reach + 16.

**Thorim's Hammer is not the circle it looks like.** `spell_thorims_hammer` deals full damage inside
7 yd and `base / max(dist - 6, 1)` outside, so every strike hits every hull on the map: in one pull 116
of 117 hits landed outside the circle for a median 4.1k, scaling to a direct hit near 150k. The circle
is worth dodging and the tail is a floor, not a missed dodge. Its 8 marks also arrive together and
live 24 s, so at the 8 yd warning band a hull near him is inside one almost continuously.

**Fire frees a frozen vehicle, and the demolisher already carries it.** `Hurl Boulder 62306` triggers
`Boulder 62307`, whose third effect triggers `Flames 65045`; the gunner's `Mortar 62634` → `62635`
triggers `Flames 65044`. `spell_linked_spell` maps both to `-62297`, comment *"Flames remove ice"*
([why the DBC hides it](../../engine/pitfalls.md#arc-and-geometry-defaults)). **Aim either one at the
frozen ally.** All three vehicle spells are `TARGET_FLAG_DEST_LOCATION`, so the "target" only supplies
a destination and a friendly one passes every check, while Boulder's own damage is
`TARGET_UNIT_DEST_AREA_ENEMY` and cannot hurt it — the blast is enemy-only and the Flames are not.
Both are free. Hurl Boulder has a **10 yd minimum** (RangeIndex 164, 10–70); Mortar has no minimum but
reaches only 50, so the gunner covers what the driver's floor cannot. Freeing outranks damage: the
stun is a full minute of nothing.

**Stations fan out by guid rank across an arc at the same radius**, so Ram, Sonic Horn and the pyrite
band keep their geometry and one strike costs one vehicle. One point per class put four or more
vehicles in a single 10 yd circle for **50–90%** of a pull, and six bots took `62297` in the same
millisecond. A frozen vehicle keeps its slot — renumbering would swing the whole fan for 60s — but
hands back the tar-lead and vent-interrupt roles, which are elected on guid order and would otherwise
go with it.

**Never ask a rider whether it can move**: `FlameLeviathanCrewUsable` tests `UNIT_STATE_STUNNED` on the
rider and the full `UNIT_STATE_NOT_MOVE` only on the hull
([why](../../engine/pitfalls.md#role-and-index-traps)). `fl.vent`, `fl.reserve` and `fl.drive` exist
so the next silent election failure is visible.

## The trace

- **A rider is not at its hull's coordinates**: a turret gunner sits 0.20 yd off, so
  `bosses/flame_leviathan.py` matches crews by proximity (`CREW_YD`, 8 yd).
- **No vehicle spell shows in the snapshot cast column**, which carries only spells with a cast time.
- **A full Flame Vents channel is 11 `63847` ticks**, and only Electroshock ends one early, so fewer is
  an interrupt; a channel the pull ended under is not scored.
- `--hulls` attributes hull loss by position, each `snap` health delta to the first matching cause, so
  its figures are shares; `dmg` covered roster players only before v13. `--inferno` reads the patches
  (62910) from `snap.hz`. `--pyrite` reads the demolisher's v13 power column: barrels 5 energy, crate
  credits +25. `fl.barrel` `dry` is no energy, `fail` the core refusing the cast. Traces that flap
  `fl.pursued` get one hull's spans merged within `PURSUED_FLAP_MS` (1.5 s), and without the notes the
  checker falls back to the boss's target column.
- **`fl.pursued` lands up to ~4 s after the aura**, while the reserve election drops a Pursued engine at
  once, so `--hulls` tags a span `reserve` from the 5 s before it and its `kite`/`ready` columns read
  late. `fl.lift` (`<guid> <x> <y>`, each gunner's first rocket at a lift) gives `--pyrite` its drop
  distances, and `--vents` prints the reserve's frames inside Ram reach between channels.

## Known gaps

- **Systems Shutdown is never used**, and it is the biggest lever left. Four `Overload Circuit 62399`
  in 25-man (two in 10) cast `62475`: `spell_systems_shutdown_aura` stuns him 20 s, **removes Gathering
  Speed** and adds +50% damage taken. A rider reaches his seats (33114) via demolisher seat 3 and
  `Throw Passenger 62324`, kills the turret it wakes, and clicks the Overload Control Device; the seats
  are immune to Vents, Missile Barrage, Ram, Fury and Inferno. Crewless bots or chopper crews could be
  the passengers.
- **Boarding depends on the raid leader.** `FlameLeviathanVehicleNearTrigger` returns false unless
  `master->GetVehicle()` — the Oculus `GroupFlyingTrigger` defect
  ([vehicles](../../engine/raid-mechanics-lessons.md#vehicles-and-flight)). It once fired 1,091 / 597
  times in two pulls and never ran, so crews whose hull died stayed on foot.
- **No seat-shortfall fallback.** With zero slack, a bot that loses a boarding race is left on foot.
- **A crewless bot walks into the arena and dies**, still running the last drive goal while Missile
  Barrage and Flame Vents (500 yd) hit a player-sized pool, a median 17.7-22.2 s after losing its hull.
  Seats have zero slack, so there is nothing to re-board and nothing walks them out; one wipe lost 22
  of 26 bots this way.
- **The tar lead runs but is unmeasured.** Its note emits only when `FlameLeviathanTarLeadDistance`'s
  clamp opens — lead capped at `dist(boss, pursued) − bossReach(15) − BATTERING_RAM_RADIUS(25) − size`,
  needing the pursued vehicle more than ~40 yd out. What the role is worth is unknown.
- **No chopper pyrite ferry**, so crates only reach a demolisher that drives to them itself.
- **`PlayerbotAI::CastVehicleSpell(uint32, float, float, float)` is declared and never defined**, so
  there is no ground-targeting and tar cannot be ignited deliberately.
- **The Flames thaw is unverified in the field**: no traced hull took `62297` from its first build
  through 2026-09-16.
- **Crate income does not cover the barrels thrown.** ~250 energy/min across the fleet against 63
  barrels/min left demolishers `dry` for 20-42% of a kill pull, and `dry` was 5 of 9 stack losses there
  (12 of 24 in a wipe). The centre drops are the first lever; after them,
  `ULDUAR_FL_PYRITE_BURST_STACKS`, the refresh rate or `ULDUAR_FL_CRATE_DETOUR_RADIUS`.
- **The crate grab ceiling is not holding and nobody knows why.** 30-47 of 46-91 grabs a pull go out
  above `ULDUAR_FL_CRATE_GRAB_CEILING` (75), wasting part of the +25. Reading the bar 1.5 s earlier gives
  the same figures, so it is not the energize landing inside the sample, and
  `FlameLeviathanRiddenVehicle` does resolve a gunner to its demolisher. One pull's 80 casts went on
  50 crates, 30 of them repeats inside the despawn, its first three from three gunners on one crate
  200 ms apart, so the first grab may be under the ceiling and the repeats what overflow. Find the
  path before moving the constant.
- **The pyrite refresh slack is thin.** `late` lost 1–2 stacks a pull against 25 `fail` of 34, nearly
  all past 70 yd; more `late` losses mean `ULDUAR_FL_PYRITE_REFRESH_SLACK_MS` (2 s) runs short. The
  seat's +25 is read off the DBC and `conditions`, not a trace.
- **Demolishers fall past 70 yd by trailing him, not from a bad goal** (383 of 385 barrel `fail`
  frames were out there); with the station point a median 45 yd from his centre the hulls sat 51-79 yd
  from it, past range for 40-59% of a pull. The centre swing and the `ULDUAR_FL_STATION_LEAD_S` lead
  answer it. If trailing on the inside still loses range, spend Increased Speed out of range or tighten
  `ULDUAR_FL_DEMOLISHER_BAND`; a lead he turns out of is worse than a stale point, so watch `fail` and
  the refusal rate together.
- **A Pursued demolisher kites past barrel range.** On Increased Speed it opens 88-112 yd and drops its
  10 stacks, a 50-energy re-burst; in one wipe that drained the fleet before 1:30. Spending the speed
  only to open the gap would keep it under 70 yd.
- **Caught on speed late.** At his ~8 yd/s by 2:50 a Pursued siege engine kept a 20–36 yd gap and took
  a blast every 2 s; Steam Rush every 15 s buys ~5 s. A Pursued demolisher's gap closed 43.6 -> 24.5 yd
  over 25 s and the hull died to the chain. `fl.speed` says whether Increased Speed's 7 yd/s was there.
- **A siege engine at station starts every Pursued span inside Ram's cast range**
  (`ULDUAR_FL_SIEGE_STAND_DIST` 8 against a 7.7 hull). The reserve holds outside it; siege spans
  starting within 40 yd of his edge lost 58-81 pts, so watch the others too.
- **A siege engine Pursued on its corner post starts boxed in.** Every kite exit crosses his front: the
  gap once went 62 → 2 yd in 9 s with the rush locked. If a working rush does not clear it, the kite
  needs its own way out of a corner.
- **Unverified edges of the hazard and post work.** Parked vehicles let a reticle commit more often,
  so `--fury` must keep catching no hull. Siege engines boxed out by the Inferno at 25 yd may drop the
  vent interrupt. The kite ring's chamfer passes ~28 yd from each post, inside Ram's sphere round a
  Pursued vehicle driving it. An add thrown against the wall may land inside Fire Cannon's minimum.
  `DoZoneInCombat` gives a fresh add to the first roster entry's hull (a chopper, 11 of 15 adds), and
  the post takes it back in 1-3.2 s, so that costs the add's walk out and back, not the hold.
- **The post turn's edges are unchecked.** Held-add frames with no gun bearing fell from 71.7% to
  24.6-28.7%. Two adds on opposite sides could still leave a post facing neither — watch `fl.corner`
  for a facing that never settles. Adds leaving manned posts (2 of 17) mean the claim lapses too
  easily. A reserve on the strict Hammer circle can take a direct hit, which `--hulls` names at once
  (none of 301).
- **The dodge latch, the leg test and the station detour are unverified in the field.** A latched goal
  must drop as the trail slides, which is why both tests re-run every tick. A station detour may circle
  a trail end instead of waiting for it to pass. The reserve keeps the full Inferno band but prefers an
  exit it can still fire from; boxed in, it takes the compromise exit and loses the channel anyway.
  Centre-side demolishers may meet more Inferno, adds and the Pursued hull's Ram sphere mid-arena,
  where he passes a p10 31 yd from the centre: watch their `hazard:inferno` share, Lash and `--ram`.
- **Trash on the approach is unverified in the field**, and no trace covers the approach: check that
  the fleet shoots the Defenders and Colossi.
- **`FlameLeviathanEngaged` falls back to the rider's combat** while he is attackable but not yet
  fighting (after the Colossi die), so bots on leftover trash there run the boss drive and the
  movement veto. Unverified.
