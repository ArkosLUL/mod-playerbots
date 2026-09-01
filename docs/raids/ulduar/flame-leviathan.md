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

The mechanic seat's own 100 is **never** refilled (`62473` targets the demolisher, not the seat), so
its Increased Speed is capped at four casts for the fight. Ample: a given demolisher takes Pursued
once or twice.

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
  `Spell::CheckRange` returns OK immediately for `RangeEntry->ID == 1`, so the **effect radius** is
  the reach — Ram 18 yd, Electroshock 25, Sonic Horn 35 — and `world.spell_cone` is the **width**:
  Ram 100°, Electroshock 60°, Sonic Horn 50° (no row = 60°). `isInFront` passes no target radius, so
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
- **Nothing else opens a RaidObs trace.** He never sets `IN_PROGRESS` (only `SPECIAL` /
  `NOT_STARTED` / `DONE`) and the unit he engages is a vehicle, not a roster player, so neither obs
  opener fires and five wipes left no trace at all. `FlameLeviathanEngaged` calls `MarkPull` to cover
  it, latched per instance and released when he leaves combat so a re-pull opens a fresh one.
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
- **Hurl Pyrite Barrel needs no ammo** — no `CasterAuraSpell`, 5 energy is the only gate. It triggers
  `62489` (54000 AoE) which applies stacking `68605 Blue Pyrite` (10s, 10 stacks). Each demolisher
  carries its **own** aura instance, so the refresh check reads the caster-scoped overload. **Do not
  infer the stack count from tick damage**: ticks arrive partially reduced (a parallel series at
  0.8×), so `amount / 12120` rounds a full stack into a lower bucket and invents one-stack drops the
  aura cannot produce — it refreshes whole or falls off whole. Use `amount + resisted`, or read the
  `fl.pyrite` probe, which emits `GetStackAmount`. Tick *count* needs no correction and is the number
  that matters: uptime, one tick per second, measured at 20–75% per demolisher over five wipes.

## Vehicle spells that must be self-cast

Tar, Steam Rush, Speed Boost, Increased Speed and Shield Generator are aimed at the caster or its own
vehicle. They cannot go through `CanCastVehicleSpell` — a self-cast returns `SPELL_FAILED_BAD_TARGETS`,
which that helper does not tolerate — so they are gated on cooldown and power directly. They must
also be passed the vehicle base as their target: `CastVehicleSpell` turns the vehicle to face any
*other* target first, which would aim Steam Rush straight at him and drop the tar pool on the wrong
side of the chopper.

## One action owns all movement

`FlameLeviathanDriveAction` is the only thing that steers a vehicle, with internal precedence:
kite when Pursued → clear a hard-mode hazard → detour to a crate below the pyrite reserve → run the
tar lead → hold station. Hazard clearance used to be a separate action at a higher priority; that is
the two-owner bounce with the priorities swapped, so it was folded in rather than re-ordered. The
action is wired to two trigger nodes, `drive urgent` at `ACTION_RAID + 3` and the routine one at
`+0.5` — the engine caches actions by name, so both nodes drive one instance and one latched
destination.

Positioning: siege engines and non-lead choppers hold his **rear arc**; demolishers hold a 50 yd band
and never close; one chopper (lowest guid, neither Pursued nor frozen) runs *ahead* of him, back
turned, so its tar pool lands in his path — clamped to stop short of the Battering Ram sphere, and
giving the slot up entirely when the chase leaves no room. Each class fans out by guid rank.

`FlameLeviathanVehicleMovementMultiplier` zeroes every other `MovementAction` while a bot is on an FL
vehicle, exempting only the drive action, boarding and `LeaveVehicleAction` — and it stays **inert
until he is engaged**, or the raid could never drive into the arena. The Void Reaver risk applies:
once it is live, a silently failing drive action means vehicles stand still all fight.

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
| Frost | 65077 | 33108 (2 spawn) | Walks to a target, roots itself, fires 5s later where it stopped |
| Life | 64482 | 33367 | Spawns attacking adds — kill, not dodge |

Life tower is skipped: its adds are already covered by the vehicle's kill-nearest-attacker loop.

**Hodir's Fury is a telegraph, not a chase.** `npc_hodirs_fury` *walks* (`SetWalk(true)`) after
`MoveFollow(target, 0, 0)`; on arrival `MovementInform` roots it and starts a **5000 ms fuse**, then
the strike lands where it stopped: it summons `NPC_HODIRS_FURY` overhead and casts **62533**. It is harmless while moving and a static mark once it matters, so
dodge **radially** — breaking sideways is what you do to a chaser and buys nothing here. The strike
carries `62297`: 10 yd, **60s stun**, `Mechanic 0` and no dispel type, so no dispel, trinket or
mechanic-clear touches it. Blast radii are Hodir's Fury 10 yd, Mimiron's Inferno 9 (62910), Thorim's
Hammer 7 (62912) — the 18 yd scan is a warning band, not the circle to leave.

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
## Known gaps

- **Boarding depends on the raid leader.** `FlameLeviathanVehicleNearTrigger` returns false unless
  `master->GetVehicle()` — the Oculus `GroupFlyingTrigger` defect, where one human who has not
  mounted freezes the whole raid.
- **No chopper pyrite ferry**, so crates only reach a demolisher that drives to them itself.
- **No seat-shortfall fallback.** With zero slack, a bot that loses a boarding race is left on foot.
- **`PlayerbotAI::CastVehicleSpell(uint32, float, float, float)` is declared and never defined**
  (`PlayerbotAI.h:558`), so there is no ground-targeting and tar cannot be ignited deliberately.

