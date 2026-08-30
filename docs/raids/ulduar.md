# Ulduar (map 603)

Strategy key `ulduar`, one strategy for all 14 encounters. Cross-raid conventions are in
[README.md](README.md).

Files are split per boss: `Action/UldActions_<Boss>.{h,cpp}`, `Trigger/UldTriggers_<Boss>.{h,cpp}`,
with `Action/UldActions.h` and `Trigger/UldTriggers.h` kept as **thin umbrella headers** that include
the parts — so `UldActionContext.h`, `UldTriggerContext.h` and every registration map needed no edits
when the monolith was split.

## Hard modes are declared by config, not detected

**Ulduar hard modes are a raid choice, not the 10/25 heroic flag**, so the "heroic comes free via a
spell-id predicate" trick from other raids does not apply. Bots follow the **follower model**: they
never *trigger* a hard mode, they react once the raid has. In an all-bot raid nobody triggers one —
accepted.

Detection started as "config **and** a live server signal", and that second half turned out to be the
problem: it depended on this core's scripting details, it **silently disabled a whole boss's
hard-mode handling** when a signal was missing, and it was invisible to the operator who had
explicitly turned the option on. The eight `AiPlayerbot.Ulduar*HardMode` options (all default 0) are
now the **single source of truth** — each `Is*HardModeActive` is a plain config read. The per-boss
triggers keep their own mechanic checks (hazard nearby, debuff on the bot, add alive), and **those
mechanic checks are the real gate**; the detectors were a redundant second one.

**The config check lives in the detector, not scattered across triggers** — detectors are the only
coupling to server internals, and triggers stay thin. A Vezax trigger that bypassed the detector by
calling the raw lookup directly had to be fixed for exactly this reason.

Still dynamic, because they are *phase* or target selection rather than hard-mode detection:
`IsSteelbreakerEmpowered` (must not arm before phase 3), `GetIronAssemblyNextKillTarget`,
`GetFlameLeviathanNearestTowerHazard`. `YoggActiveKeeperMask` and the file-static
`GetBotInstanceScript` were deleted; **`YoggThorimKeeperActive` survives** and still reads
`PERSISTENT_DATA_WATCHERS_MASK` through `InstanceScript`, so that dependency is not fully gone.

Behaviour worth knowing: Flame Leviathan's mask claims all four towers, but hazards are found by NPC
entry, so destroyed towers contribute nothing.

### Why `GetData` is avoided

Three separate hard modes tried it and three found it wrong:

- **Vezax** `GetData(1)` returns `lootMode == 3`, set only in `DoAction(2)` — i.e. **after** the
  Saronite Animus dies. It is a completion flag, not a live signal. The real truth is simply "Animus
  (33524) alive".
- **Hodir** `GetData(3)` is the 3-minute timer flag; deliberately unused, because the buff
  optimisation is harmless past the window.
- **Mimiron** `GetData(1)` is authoritative but lives on Mimiron himself, who sits in his pod and
  **never becomes a bot attack target**, so he never enters the `"possible targets"` lists.
- **Thorim's** `SPELL_SIF_CHANNEL_HOLOGRAM` (64324) is **defined but never cast** in this core — the
  channel Sif actually casts is `SPELL_TOUCH_OF_DOMINION` (62507).

Prefer a boss's **empower aura** where one exists: Flame Leviathan's tower auras tell you *which*
towers are up, where `GetData(DATA_GET_TOWER_COUNT)` gives only a count.

## Per-boss hard modes

### Vezax — the reference implementation

Hard mode = leave Saronite Vapors alive until the **Saronite Animus (33524)** spawns; Vezax gains an
invulnerable Saronite Barrier until it dies. Everyone switches target and kills it (no RTI mark —
each bot `Attack()`s directly). Nobody moves out of its Profound Darkness (63420): radius index 28 is
**50,000 yd**, so the stacking shadow-damage debuff is room-wide and the only answer is killing the
Animus faster. Full encounter facts are under [Vezax](#vezax).

### Assembly of Iron

Killing one member restores the other two to full and hands them Supercharge (61920), so damage
spread across three health bars is thrown away and **focus fire is the encounter**. Each Supercharge
`SpellHit` calls `UpdatePhase()`, so the last one alive reaches `_phase == 3` — and `SetLootMode(0)`
is only undone there, so **all loot comes from whoever dies last**.

Kill order is the only thing `AiPlayerbot.UlduarIronAssemblyHardMode` changes: normally
**Steelbreaker → Molgeim → Brundir**, hard mode **Brundir → Molgeim → Steelbreaker**. Molgeim is
second either way, so Rune of Summoning is unreachable — deliberately, because
`npc_assembly_lightning` no-ops `AttackStart`, `MoveInLineOfSight` and `EnterEvadeMode`: the
Lightning Elementals have no threat table, so taunting and kiting both do nothing and only killing
them works. Guides calling them tauntable are wrong for this core.

`_phase` is private and Steelbreaker has no `GetData` override, so empowerment is inferred:
*Steelbreaker alive AND Molgeim dead AND Brundir dead*. It is **not** gated on the config flag — it
is a phase check, and gating it left a raid that reached the state without the option set with no
tank swap.

His empowered kit is Fusion Punch (61903/63493, a dispellable Magic DoT on the tank), Static
Disruption (61911/63495, a random target beyond 10 yd, 6 yd blast plus a 5 yd +75% nature
vulnerability), Overwhelming Power, and Electrical Charge (61902), +25% per player death. Main tank
and assist 0 trade him on **Overwhelming Power only** — Fusion Punch recurs far too fast, and
swapping on it would ping-pong the boss between them.

**Every ability is difficulty-mapped through `Unit::CastSpell`**, so each id is a 10/25 pair and
callers test both (`UldBossHelper.h`). Two corrections to the written guides: Lightning Tendrils is
**18 yd** (61886/63485), not the 10 of the 61884 dummy; and **Overwhelming Power (64637/61888) is
`DispelType 0`**, not dispellable. Its carrier dies to Meltdown (61889, 29,250 in 15 yd) regardless,
so the node walks them clear of the raid instead — every death it causes is another permanent +25%
Electrical Charge.

`creature_immunities`: **Brundir (`0x24CB375F`) is vulnerable to STUN and INTERRUPT but immune to
SILENCE** — kicks and stuns land, `silencing shot` and `spell lock` never do. Steelbreaker and
Molgeim (`0x26CB3F7F`) add STUN and INTERRUPT, so **Brundir is the only member worth an interrupt
node**. Lightning Whirl reaches 100 yd with no positional answer, so it takes the lowest-ranked
interrupter; Chain Lightning takes the second, and is deliberately allowed through when cooldowns are
thin.

Formation anchors on `(1587.18, 121.02, 427.27)`. navprobe: 8/8 headings clean at 20 and 30 yd, but
the 45° and 135° diagonals settle to Z −27.7 and −438 at 40, and three of eight leave the mesh at 50
— so **nothing sits outside 30 yd** and the slots use cardinals. Brundir parks at 28 yd, 38 from the
ranged stack: more than his Overload needs, so the stack never reacts to it and someone is always
parked and free to kick. **Tanks stand rather than drag**, so a boss follows its tank to the spot
instead of being towed through the raid.

Rune of Power lands on `DoSelectLowestHpFriendly`, i.e. on a **member**, not a player — so both
halves apply at once: the tank walks his boss off it while ranged and healers walk in. The soak is
capped at 25 yd, which admits every rune on Steelbreaker or Molgeim and rejects every one on Brundir
that would otherwise tow the ranged group into Overload.

Deliberate non-behaviours: **tanks hold through Overload** (20,000 nature is survivable in plate and
lethal in cloth, and under the normal order Brundir dies last, so his channel invincibility never
applies); **Electrical Charge is not handled positionally** — a stacking damage buff is a healer
problem, not a movement one; **ranged spread only in hard mode**, since Static Disruption needs
Steelbreaker's phase 2; and **bots never set the skull**, so a human's mark wins.

Core-version assumption: the strategy relies on recent upstream fixes — `#26470` (Rune of Death
restricted to players), `#26449` (Brundir surviving Tendrils), `#26200` (Static Disruption preferring
ranged) and `#25029` (Overload invincibility). An older AzerothCore behaves differently.

### Flame Leviathan

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

#### Gunners can steer, which is why nothing gates on `IsInVehicle(true)`

Seat 0 of **both** the turret (33067) and the mechanic seat (33167) carries `CAN_CONTROL`, so
`MovementAction::MoveTo` passes its `seat->CanControl()` check for a *gunner* and calls `DoMovePoint`
on the turret creature bolted into seat 7. Movement is gated on `vehicleBase->GetEntry()` instead.
No seat a bot can occupy has `CAN_ATTACK` — that flag is on the accessory seats, which hold NPCs — so
`CastSpellAction::isUseful` already blocks class spells and no targeting-suppression multiplier is
needed. Siege engine seats 1 and 2 have **no flags at all**; the chopper rear seat and demolisher
seat 3 are `ENTER_EXIT` only.

`instance_ulduar.cpp` spawns **2 of each vehicle in 10-man, 5 in 25-man** at `x = 119.8`, west of the
arena: exactly 10 / 25 usable slots, **zero slack**.

#### The demolisher runs on pyrite, not energy

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

#### Tar cannot slow him, and Blaze cannot hurt us

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

#### Boss mechanics

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
- **Pursued 62374** picks a random vehicle every 31s and lasts 35s; he then drives at it and uses
  `Battering Ram 62376` inside 15 yd — a **25 yd blast on a point in front of him**, not a hit on one
  target. Everything in his frontal arc is caught: 40% of every Battering Ram landed within 5s of a
  switch, 7–13 vehicles at a time. Only the pursued vehicle belongs in front of him, and the 31s
  cadence makes the switch predictable enough to vacate before it. The aura beats `GetVictim()`.
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

#### Vehicle spells that must be self-cast

Tar, Steam Rush, Speed Boost, Increased Speed and Shield Generator are aimed at the caster or its own
vehicle. They cannot go through `CanCastVehicleSpell` — a self-cast returns `SPELL_FAILED_BAD_TARGETS`,
which that helper does not tolerate — so they are gated on cooldown and power directly. They must
also be passed the vehicle base as their target: `CastVehicleSpell` turns the vehicle to face any
*other* target first, which would aim Steam Rush straight at him and drop the tar pool on the wrong
side of the chopper.

#### One action owns all movement

`FlameLeviathanDriveAction` is the only thing that steers a vehicle, with internal precedence:
kite when Pursued → clear a hard-mode hazard → detour to a crate below the pyrite reserve → run the
tar lead → hold station. Hazard clearance used to be a separate action at a higher priority; that is
the two-owner bounce with the priorities swapped, so it was folded in rather than re-ordered. The
action is wired to two trigger nodes, `drive urgent` at `ACTION_RAID + 3` and the routine one at
`+0.5` — the engine caches actions by name, so both nodes drive one instance and one latched
destination.

Positioning: siege engines and non-lead choppers hold his **rear arc**; demolishers hold a 50 yd band
and never close; one designated chopper (lowest guid, not currently Pursued) runs *ahead* of him,
back turned, so its tar pool lands in his path.

`FlameLeviathanVehicleMovementMultiplier` zeroes every other `MovementAction` while a bot is on an FL
vehicle, exempting only the drive action, boarding and `LeaveVehicleAction` — and it stays **inert
until he is engaged**, or the raid could never drive into the arena. The Void Reaver risk applies:
once it is live, a silently failing drive action means vehicles stand still all fight.

#### The kite is a wall loop, not a corner run

`FlameLeviathanKiteRing()` builds eight nodes from the four `NPC_FREYA_WARD_TARGET` spawn points —
`(159.4, 64.1)`, `(382.9, 74.0)`, `(374.0, -141.0)`, `(157.7, -140.3)` — inset 15 yd off the walls
with each corner chamfered into two nodes 35 yd out. The direction is latched per instance on the
first Pursued and **never reversed**; nodes within 50 yd of him are skipped so the ring never routes
through him; and the vehicle **advances on approach at 30 yd, never on arrival**.

That last rule is the whole point. The old kite drove corner to corner and only switched once within
5 yd, so the vehicle buried itself in the corner while he cut the diagonal — and its advance was also
conditioned on him being within 50 yd, which made progress depend on already being caught.

#### Hard mode

At pull, `ActivateTowers()` adds one empower aura per surviving tower and schedules that tower's
periodic ground event:

| Tower | Boss aura | Ground NPC | Hazard |
|---|---|---|---|
| Storm | 65076 | 33364 (8 spawn) | Static lightning strikes at 8 fixed marks, ~5s telegraph |
| Flame | 65075 | 33369 | Escort-path **moving** fire trail, drops fire every 2s |
| Frost | 65077 | 33108 (2 spawn) | **Chases** a random player, stuns, drops frost AoE at the catch point |
| Life | 64482 | 33367 | Spawns attacking adds — kill, not dodge |

Life tower is skipped: its adds are already covered by the vehicle's kill-nearest-attacker loop.

#### Known gaps

- **Boarding depends on the raid leader.** `FlameLeviathanVehicleNearTrigger` returns false unless
  `master->GetVehicle()` — the Oculus `GroupFlyingTrigger` defect, where one human who has not
  mounted freezes the whole raid.
- **No chopper pyrite ferry**, so crates only reach a demolisher that drives to them itself.
- **No seat-shortfall fallback.** With zero slack, a bot that loses a boarding race is left on foot.
- **`PlayerbotAI::CastVehicleSpell(uint32, float, float, float)` is declared and never defined**
  (`PlayerbotAI.h:558`), so there is no ground-targeting and tar cannot be ignited deliberately.

### Thorim

Two halves that share nothing: a corridor gauntlet past the Runic Colossus and the Ancient Rune Giant,
then a stationary fight on the arena floor once Thorim drops off his balcony. `GetPositionZ() < 429.6`
separates the two everywhere in the strategy.

#### Phase 1 split

The raid fights phase 1 in two halves, and the arena half must never empty out.
`ThorimAI::GetArenaPlayer()` scans for one **living** player inside

```
x 2085..2185   y -305..-214   z < 425
```

**every 5 seconds** from the start of phase 1. The first scan that finds nobody is terminal: `SAY_WIPE`
and a `Lightning Orb` (33138) that kills the raid. There is no grace period and no recovery, so this
is a hard constraint rather than a preference. A separate 5 minute timer
(`EVENT_THORIM_NOT_REACH_IN_TIME`) fires the same orb regardless, which bounds the whole of phase 1.

The split is **latched once per pull** and held until Thorim drops to the floor. Recomputing it per
tick is what let a role predicate flipping mid-fight walk the arena squad into the corridor. Quotas
are 1 tank + 1 healer + 3 DPS (10 man) and 1 tank + 2 healers + 7 DPS (25 man); a roster that cannot
fill them shrinks the gauntlet rather than emptying the arena, and assignment stops entirely once the
arena would drop below 3. The main tank never leaves the arena.

Every gauntlet pick is **bot-only**. A human still occupies their role but nothing here can walk them
anywhere, so spending the single gauntlet tank slot on a human off-tank just leaves the corridor a body
short. **Set `MEMBER_FLAG_MAINTANK` in the raid frame**: without it `GetMainTankGuid` falls back to the
first tank in roster order, so a human tank ahead of the bot main tank silently takes the role — which
sends the bot main tank down the corridor and leaves the arena untanked.

Arena adds all land 19-24 yd from the centre and the nearest box edge is 42 yd out, so the leash is
**30 yd from `ULDUAR_THORIM_NEAR_ARENA_CENTER`**, 15.8 yd short of the corridor mouth at the lever gate.
Melee get a tighter **24 yd**, which is the furthest an add ever lands, so it costs no uptime. Two
guards back it. `ThorimArenaLeashMultiplier` holds the generic movers while a bot is outside its leash
— and, alone among the Ulduar guards, does **not** exempt `AttackAction` or `ReachTargetAction`,
because corridor mobs sit ~92 yd out, inside the 100 yd sight cap, and the chase is exactly what walks
a bot out of the box. `ThorimArenaTargetGuardMultiplier` drops a target outside the box, or the leash
and the chase would take turns at the gate forever.

**A fence is not a formation.** Left with only the leash, the squad diffuses outward until it is parked
against the east edge at x ≈ 2165, which is the gateway. East is the worst direction to drift:
`boss_thorim_arena_npcs::CanAIAttack` is `GetPositionX() < 2180 && GetPositionZ() < 425`, so a bot that
gets there stops being attackable at all and the add re-rolls — and `SelectT()` picks a **random**
arena-side player and gives it 500 threat, so a squad spread over 50 yd puts adds on people no healer
is in range of. Threat is chaotic here by design; being in one place is the only counter.

So the squad is anchored. `GetThorimArenaAnchor` is the single answer trigger and action both read:
the tank holds `ULDUAR_THORIM_NEAR_ARENA_CENTER`, ranged and healers take a ring slot at 10 or 14 yd
(clear of the Champion's Whirlwind on the pile, close enough that anything in it is in range), and
melee get the centre only **out of combat** — in the fight they run free on the 24 yd leash. Slots come
from the latched squad in **roster order**, not from the survivors: bots die in here, and ranking by
who is still standing renumbers everyone behind the corpse and shuffles the formation mid-fight. Ring
points are computed and then validated with `GetMapWaterOrGroundLevel` and
`CheckCollisionAndGetValidCoords`, because raw ring geometry is the shape that lands off the navmesh
and `MoveTo` fails silently there. Slot 0 sits on the bearing from the lever gate to the centre, so the
formation opens away from the corridor. `ThorimArenaAnchorGuardMultiplier` holds the generic movers
once a bot is settled, exempting the chase — an add at 24 yd is up to 38 yd from an outer slot, and a
ranged bot that cannot step into range is silent.

#### Runic Colossus (32872), spawned at (2227.5, -396.179, 412.176)

| Spell | Id | Detail |
|---|---|---|
| Runic Smash, left hand | 62057 | **5s cast**, lights the left-hand bunnies (33141) at x ~2235 / 2246 |
| Runic Smash, right hand | 62058 | **5s cast**, lights the right-hand bunnies (33140) at x ~2210 / 2221 |
| Runic Smash damage | 62465 | 10 yd per bunny; the wave starts 1s after the cast and marches y -385 → -257 at 16 yd / 500ms |
| Runic Barrier | 62338 | -51% damage taken **and a 2000 arcane damage shield per melee swing**; cast at t+10s, 20s duration, recast every 20s |

`EVENT_RC_RUNIC_SMASH` is scheduled in `Reset()` and **cancelled in `JustEngagedWith`**, so the corridor
smash only happens on the approach and stops the moment the Colossus is tanked. The Ancient Rune Giant
has no damage shield — its Runic Fortification (62942) is a friendly buff on its adds.

The two corridor lanes (left x ~2237-2242, right x ~2212-2219) are **index-matched by y**, so a dodge is
a straight index map: same waypoint number, other lane. Each lane sits 2-9 yd from its own hand's bunnies
and 15.5-22.9 yd from the other's. The bot side latches the hand it sees casting and holds the opposite
lane until the other hand goes up — the gauntlet formation follows that preference rather than the
master's own lane, or it would walk everyone straight back into the blast. The Colossus is 131 yd from the
top pair of waypoints, past the 100 yd `AiPlayerbot.SightDistance` cap, so the telegraph is read through a
targeted 150 yd creature lookup rather than the usual target values.

Runic Barrier is effectively permanent, so "stop attacking while it is up" would mean never attacking.
Non-tank melee instead back out to 14 yd below 55% health and return above 80%, keeping their target the
whole time so ranged and instant abilities keep landing.

#### Phase 2

| Spell | Id | Detail |
|---|---|---|
| Chain Lightning | 62131 | `spell_jump_distance` sets the jump radius to **5.0 yd**, not the 10 yd DBC default; 8 bounces, chain source advances to each new victim |
| Lightning Charge | 62466 | `spell_cone` **75 degrees**, 150 yd, 17343 base nature, **instant with no cast bar** |
| Lightning Orb Charged | 62186 | Lands on a Thunder Orb (33378). `SpellInfoCorrections` patches the amplitude to 5000ms, so it ticks once **5s before** the cone — the entire warning |
| Lightning Charge buff | 62279 | Permanent, one stack per cast: +15% damage and melee haste, **+10% nature damage per stack** |

Positioning has to clear 5 yd between stacks. The main tank drags Thorim to `(2134.857, -287.029)` — about
a yard from the hole in the floor south of y = -288, so nothing may be placed past it. Ranged and healers
round-robin three fixed spots. Melee take a **dynamic ring of radius 8 around Thorim's live position**,
three slots at the main tank's bearing +90 / +180 / +270 degrees, which leaves the whole tank side clear
and puts the stacks 11.3 yd apart. The off-tank sits at the tank bearing +20 degrees, inside taunt range
for the Unbalancing Strike swap. Slots are sticky per guid; the bearing is anchored on the tank so the ring
does not rotate as the boss shuffles, and falls back to the static tank spot's bearing when no tank is
alive. Arrival uses a 3 yd / 5 yd deadband, because a tight one against a ring recomputed from a moving
boss leaves the bot sliding in place — and a moving bot casts nothing.

When an orb lights, the **whole ring rotates rigidly** by the smallest angle that clears every occupied
slot out of the 75 degree cone (plus a 15 degree margin). Per-bot shortest paths would swing slots on
opposite edges toward each other and trade a Lightning Charge death for a Chain Lightning one. Both tanks,
ranged and healers hold position and eat it by design: moving a tank drags the boss and re-anchors the ring.

Every melee DPS carries the `behind` strategy from `AiFactory`, so `SetBehindTargetAction` would walk all
three stacks into one arc behind the boss the moment the ring node yields. `ThorimMovementGuardMultiplier`
holds the generic movers, scoped to a **settled** ring holder and exempting `AttackAction`,
`ReachTargetAction` and `AvoidAoeAction` — a permanent movement freeze is the Void Reaver failure.

#### Hard mode

Sif is summoned every pull and normally channels, then despawns after the 150s dominion timer. If the
raid clears the gauntlet fast enough she joins instead and casts Frostbolt Valley (raid-wide,
unavoidable — healed through), Blizzard (62577 → moving `NPC_SIF_BLIZZARD` 32879, respawned every
~15s) and Frost Nova (62605, teleport then point-blank).

**Detector: Sif (33196) alive AND `GetPositionZ() < 429.6`** — she spawns at the throne and only
`NearTeleportTo`s onto the arena floor when she joins. This reuses the same floor threshold the
normal-mode Thorim strategy already uses.

#### Known gaps

Documented, not implemented: the in-combat `SPELL_SMASH` 62339 frontal cone (60 degrees, 3s cast — a
different spell from the corridor Runic Smash), Stormhammer 62042, Rune Detonation 62526, Stomp 62411,
Runic Fortification 62942, arena add kill priority, and arena tank pickup — nothing taunts an add off
whoever it rolled. Nothing recovers the fight once the arena squad is dead either; the 5 second scan
leaves no room to walk anyone back.

### Hodir

Anchored in the **south-west corner**, `(1974.50, -275.50, 432.687)`. The room is x 1965-2041, y -170
to -298, and he evades outside that y band. Cornering him collapses the helper NPCs' 17-30 yd
stand-off arc into one place, which is the only way the buff zones land somewhere predictable. That
corner is **chamfered** — the floor bevels from ~(1966, -274) to ~(1990, -298) — so the tank spot is
the deepest point with 6 yd of floor all round, not the visual corner, which has three yards of
nothing behind it. Off-tank `(1980.00, -277.00)` sits deeper in rather than toward the raid, so a
taunt never walks him at the stack. All spots are navprobe-verified.

**Starlight measures 4 yd, whatever its DBC row says.** `62807` is aura **193
`SPELL_AURA_MELEE_SLOW`**, whose handler `HandleModCombatSpeedPct` applies `ApplyCastTimePercentMod`
as well as all three attack timers, amount 50 — **+50% haste to casting and swinging**, the biggest
throughput lever in the fight. The row says 8, but across two traces bots holding the aura sit a
median **2.0 yd** from the zone (p90 3.3) while bots without it are already at 7.6 by the tenth
percentile. 4 yd holds *one* bot at the 4.5 yd spacing icicles force, so Starlight is a **per-bot
opportunity, never something to build a formation on**: `ULDUAR_HODIR_STARLIGHT_STAND_RADIUS` 2.0 plus
1.0 tolerance, `static_assert`ed to stay inside it, and a 30 yd search radius because the step runs
per bot per tick.

**The formation rides a Toasty Fire instead.** `62821` measures true to its 11 yd (with-aura p90
11.9) and stops Biting Cold, which is otherwise a tenth of the raid's time spent walking. Slots sit on
two concentric rings, `ULDUAR_HODIR_RAID_RING_INNER` 4.5 and `_OUTER` 9.0 with six inner slots — 4.5 yd
minimum separation, so Ice Shards' 4 yd splash catches one bot instead of five, and a bot shedding
Biting Cold can step outward without closing on its neighbours. Outer ring plus its 2 yd arrival
tolerance is exactly 11, `static_assert`ed to stay inside the fire. The mage drops fires 6.8-33 yd
out; `_FIRE_ADOPT_RADIUS` 25 is where roughly four fifths of them are still closer than the shuttle
they save. The centre is gated 15 yd off **Hodir himself**, not the tank spot he leaves — he drifts
10-25 yd, and a fixed-point gate once let the centre land 6.8 yd from him.

**Toasty Fire grants no Flash-Freeze exemption.** It is 11 yd and only blocks Biting Cold. The one
exemption is `SPELL_SAFE_AREA_TRIGGERED (62464)`, off `65705` on **NPC 33174**, radius index 40 → 9 yd.

| Mechanic | Ids | Numbers that drive the code |
|---|---|---|
| Flash Freeze | 61968 | **9s cast**, every 48-49s, 200 yd. Spares only 62464 carriers and pets |
| Shelter chain | 33173 → `62460` → `65370` + `62463` | Drift lands at T+3.8 → Ice Shards 14,000 in **7 yd** → summons **33174**, 12s. Freeze lands T+9: **6.3s of shelter** to cross the room |
| Trapped player | 61969 / 62226 | **300s**, and the *next* Flash Freeze **instakills**. Free by killing NPC **32926** (helpers: 32938) |
| Small icicles | 62227 → 63545 → 33169 → `62457` | **Every 2s** on 1 random player, falls after 2s, **14,000 Frost in 4 yd** + knockback. Off for 12s (25m) / 24s (10m) after each Flash Freeze |
| Biting Cold | 62038 / 62039 | Stacks every 4s on anyone **not moving**, `200 · 2^stacks`. Sheds only on the **second consecutive** moving tick, so a hop cannot clear it |
| Frozen Blows | 62478 / 63512 | 20s, **15s after each Flash Freeze**; +31,061 / +39,999 per swing plus a 3,999 raid tick |
| Freeze | 62469 | Random player in 50 yd every 17-20s, 5,549 + root in 10 yd, **dispellable (Magic)** |
| Storm Cloud → Storm Power | 65123/65133 → 63711/65134 | Carrier holds **4 (10m) / 6 (25m)** stacks, one per second — **4-6 seconds of use**. Storm Power is **3 yd**, +134% crit damage |
| Toasty Fire | 62821 | 11 yd, 60s, at the mage's feet every 10s. **Flash Freeze wipes every fire** (62148) |
| Berserk | 26662 | 8 min, unhandled — no enrage awareness exists anywhere in the module |

**Hard mode is gone as a config.** The "Rare Cache of Winter" 3-minute kill needs no different
behaviour: the helpers *are* the raid's damage, the fire is the Biting Cold answer, and Storm Power
is the biggest buff in the fight, so all three run on every pull. `AiPlayerbot.UlduarHodirHardMode`
and `IsHodirHardModeActive` were deleted rather than left gating nothing.

#### The Flash Freeze window is nine seconds and the shelter exists for six

The drift lands ~3.8s into the 9s cast, so the shelter covers only the last **6.3s** — the whole
budget for crossing the room, against a measured 30 yd median run at ~7 yd/s costing 4.3s of it.

- **The run parks at 6 yd and releases at 8** (`ULDUAR_HODIR_SAFE_AREA_TOLERANCE` / `_RELEASE`, both
  `static_assert`ed inside the 9 yd Safe Area). `MoveInside` → `MoveNear` lands the bot at *exactly*
  the tolerance, so testing one number at both ends stood the trigger down the tick it arrived and
  handed the next tick to the ring anchor — measured walking bots 18-22 yd back out with the freeze
  2s away.
- **Every other mover stands down for the whole cast, every role.** `HodirGuardMultiplier` zeroes
  gap-closers (`CastReachTargetSpellAction` — Charge, Intercept, both Feral Charges) and every
  `MovementAction` bar the shelter run and the icicle dodge. `ReachTargetAction` is in scope;
  `AttackAction` is exempt because it only sets a target. Measured before the gate: 68 of 125
  bot-freeze pairs had their last accepted move come from something else, 36 of them the ring anchor
  and 25 `reach melee` / `reach spell`.
- **Do not instead return `true` from the shelter action.** `MoveTo` answers Duplicate for a
  destination it already issued, so from the second tick of a run `Execute` returns false and the
  engine descends past `ACTION_RAID + 6` — that descent is correct, and holding the tick would
  silence the bot's casting for six seconds, seven times a pull. The movers below are what has to be
  off.
- **The window is gated on the cast, not a timer.** `IsHodirFlashFreezeIncoming` tests
  `UNIT_STATE_CASTING` plus `FindCurrentSpellBySpellId` over **every** cast slot rather than
  `CURRENT_GENERIC_SPELL`: which slot a scripted boss cast lands in is the script's business, and
  guessing wrong opens the window on nothing. `UldTriggers_Mimiron.cpp:31` is the same shape.
- **The shelter run keys off 33174 existing**, not off the boss casting. Starting when the drift
  spawns puts the raid under a 14,000 / 7 yd detonation.
- Everyone converges on the drift nearest the **ring centre**, not `ULDUAR_HODIR_RAID_ANCHOR` — the
  ring rides a fire and sits a median 9.5 yd off that fixed point (p90 17.6), so measuring from a spot
  nobody stands on picked drifts 25 yd away with three closer candidates on the floor. Trigger and
  action share one helper; two derivations would oscillate. It answers "none" before deriving the
  centre, since a shelter exists for ~6s of every 49s cycle and the centre costs a second grid sweep.
- **The anchor is abandoned every 48s and that is correct** — tanks included. He is encased otherwise.
- **Residual, still open:** the dodge issues `MOVEMENT_FORCED` and the shelter run `MOVEMENT_COMBAT`,
  and `IsWaitingForLastMove` only yields to a strictly higher priority, so a dodge firing late in the
  window can hold the slot until the freeze lands. Raising the run trades a ≤300s lockout for one Ice
  Shards hit at ~41% of a health pool — worth doing, but it needs its own before/after trace.

#### Frost Resistance Aura belongs on a tank

**Frozen Blows is 71% of everything the raid takes** — 3.93M of ~5.5M in one 25-man trace, against
929k for Biting Cold, Freeze and Ice Shards combined. `63511` is 39,999 base and lands a median
23,691 after resists into a 34-45k tank pool, so the aura is the margin between a survivable swing
and a killing blow. Every tank killing blow in that trace landed with it **off**, the retribution
paladin carrying it 44-60 yd away, two of the tanks resisting nothing at all.

`GetHodirResistancePaladin` therefore prefers a **paladin tank**, then any non-healer, then whoever
is left. The aura reaches 40 yd (`48945`, radius index 23) and a tank never leaves the corner, so it
covers the two bots that need it 100% of the time against 82% for a DPS paladin running the dodge and
the shelter. The raid loses about ten points of coverage, which is the right trade: a resist point is
~6,000 off a swing that kills a tank and ~700 off a tick the healers already cover.

#### Two icicle pools, and a dodge that leaves on one radius and lands on another

Icicle **33169** leaves Ice Shards `62457` in **4 yd**; the drift **33173** leaves `65370` in **7 yd**.
Both hit for 13-14,000. The dodge leaves on the radius that actually kills and lands on a clear
carrying 2 yd of margin over it (`_ICE_SHARDS_CLEAR` 6, `_BIG_SHARDS_CLEAR` 9) — clearing everything
to 6 stepped bots onto the edge of the big pool and killed four in one pull. `_DODGE_TRIGGER_MARGIN`
is 0.5 for the same reason in reverse: testing the *clear* at both ends had bots stepping out of pools
they were never in, since the small one would trigger over 2.25× the area it kills in. Candidates are
ranked smallest displacement first and leashed to `_DODGE_LEASH` 12 — maximising distance from the
hazard is what walked Auriaya's bots into the corridor.

An icicle summon lives 7,000 ms (`62234`/`62462`, DurationIndex 165) but **detonates at 3,700 ms**:
its AI casts the fall effect at 2,000 ms and that aura's single 1,700 ms tick triggers the blast. The
last `ULDUAR_HODIR_ICICLE_SPENT_MS` = 3,300 ms are inert, so at one icicle every 2s roughly half of
those on the floor have already blown.

**Biting Cold sheds on sustained movement only.** A stack comes off on the second *consecutive*
moving tick and any stationary tick between resets that progress, so the shuttle walks 6 yd legs
(`_SHUTTLE_HALF_LEG` 3.0) on bearing −π/4, parallel to the SW bevel, chaining until the aura is gone.
It arms at 2 stacks: ~33% movement duty for ~600/s, where arming at 1 would cost half the raid's cast
uptime to save 200/s.

#### Traps

- **Three icicle entries, and confusing them breaks the fight.** 33169 is the small one, dodged
  always. 33173 is the drift, dodged **only while falling** — the dodge stands down once a 33174
  exists within 9 yd of it, because 33174 is the shelter everyone is running to. 33174 is never
  dodged.
- The anchor is **not combat-gated**: `MoveInLineOfSight` is a no-op, so bots pre-position in the
  corner and the tank pulls from there instead of dragging him 75 yd.
- **Melee get no anchor, no fire and no Starlight.** Re-examined once Starlight turned out to be +50%
  melee haste too, and confirmed: the only fix is dragging him to the druid, which costs the corner.
- The Storm Cloud carrier **laps the ring**, direction latched for one carry; tanks never run it and
  are never buff targets. Greedy re-targeting is the Auriaya corridor dance.
- Healers are excluded from the targeting node entirely, and **5** non-healers break each ice block —
  raider and helper alike, picked by a GUID window offset per block so several blocks draw disjoint
  sets instead of the same five. Freeing outranks the boss (the trapped raider dies to the next
  freeze) but the block has little health, so only bots within 45 yd leave what they were doing.

#### The anchor and the dodge will thrash unless three invariants hold

Traced on 2026-08-23: two wipes at 67% HP, both tanks dead inside two minutes, healers at 0-10% cast
uptime and ~70% moving. 934 anchor/dodge reversals — 21% of every accepted move, median gap 321 ms,
15,240 yd walked — roughly **43% of the raid's fight time spent walking between two destinations**.
Three separate causes, all of them still easy to reintroduce.

- **The anchor stand-down tests the whole walk back, not just the anchor.** The dodge trigger goes
  false the moment the bot is clear, but the icicle stays lethal until it detonates at 3.7s. Checking
  only the destination lets the anchor walk the bot back under the blast, where the dodge re-arms —
  about 11 round trips per icicle, one icicle every 2 s. `HodirRaidPositionTrigger` projects each
  lethal icicle onto the bot→anchor segment for exactly this reason.
- **Do not "fix" this by widening the arrival tolerance.** A dodge always displaces further than the
  tolerance — by design, not the bug. Widening it stops the *return*, and at one icicle every 2 s the
  formation becomes an unbounded random walk out of the fire inside a minute. What works instead:
  `HodirRaidPositionTrigger` is reactive for ranged, firing only on a broken constraint (inside
  `ULDUAR_HODIR_RANGED_MIN_BOSS_GAP` 15 with a clear slot to reach, no fire, clumped under
  `ULDUAR_HODIR_DECLUMP_RADIUS` 4.5, past `ULDUAR_HODIR_RETURN_LEASH` 20), so it issues one
  destination and goes quiet. `HodirRaidPositionAction` holds no arrival latch on purpose — one would
  swallow those re-anchors, which fire well inside twice the tolerance. Tanks keep the spring: Hodir
  follows whoever holds him.
- **Ring slots are indexed over the whole ranged roster, dead included.** Indexing over the living
  shifts every bot after a corpse, so one death re-seats the entire formation and `total` moves the
  inner/outer split with it. Eleven ranged deaths, nine of them in a 30 s window, re-anchored every
  survivor each time — which is what turned a bad pull into a cascade.
- **Tanks do not run the icicle dodge.** They ate a ~50 yd walk around the room and took Hodir with
  them; Bulwark ended up 70 yd from the boss while alive. Tanks eat the 14,000 instead, and the
  Biting Cold shuttle already gives them the movement they need without leaving the corner.

Two things that look broken in a Hodir trace and are not: `hodir frozen blows swap action` logging
~95% `FAILED` is the stateless trigger retrying every ~110 ms while the taunt is on cooldown — count
the `OK` records instead, one per cooldown per Frozen Blows window is correct. And a
`thorim.squadsassigned` note inside a Hodir pull is `ThorimResetEncounterStateTrigger` clearing stale
state from an earlier attempt, which is cleanup working — Thorim nodes in a Hodir trace issue zero
accepted moves and zero `OK` verdicts, and `NearThorimEncounter` excludes his floor by height
(`z < ULDUAR_THORIM_WING_MAX_Z` 425 against 432.687).

**Still open here:** no tank defensive cooldown is tied to a Frozen Blows window — the tanks spent
four and six in six minutes, unprompted. And the raid was at 42.9% boss health after six minutes,
roughly half the pace hard mode needs; the movement-economy work is aimed at that and wants
re-measuring before anything else is tried.

### Freya

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
ways, falling back to melee when no ranged DPS is alive. The Detonating Lasher rung resolves per role —
see the lasher paragraphs below.

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
the DPS node drags them back to the boss to reach it, and they lose the aura on the way. Ranged and
healers do use their own nearest spore — they need the aura, not melee range, and any spore is inside
casting range of both the boss and the melee stack. Tanks are excluded from the spore node itself: the
add tank arrives inside the aura by dragging the boss there, and the main tank never repositions Freya.

Expect this stack to be broken up regularly. `EVENT_FREYA_NATURE_BOMB` repeats every **18s** for the
whole fight, dropping one bomb per player at their own feet — 7-10 in 25-man, 3-4 in 10-man
(`boss_freya.cpp:645-660`). Damage 64587 is 5850-6150 in **10 yd** with **no difficulty entry**, the
fuse is ~6s (`:1300-1317`), and the marker is GO **194902** summoned in the bomb creature's `Reset()`;
the creature itself is banished and never reaches the npc lists.

The escape rings outward to a spot clear of *every* bomb inside `ULDUAR_FREYA_HAZARD_SEARCH_RADIUS`,
because a volley drops one on each of the stacked melee. The old `FleePosition` dodge moved 5 yd out of
a 10 yd blast, so it killed everyone it fired for
([../engine/pitfalls.md](../engine/pitfalls.md)). Tanks are excluded: stepping out would drag Freya
toward the raid or lift the Conservator off its spore, and ~6k per volley is cheaper than either.
Dodging keeps its `ACTION_RAID + 4` priority — a bomb hit costs more than a few pacified seconds — and
the "go to the parked spore" rule is what makes the raid re-converge afterwards instead of smearing
across three spores.

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

**Detonating Lashers cannot be tanked, and no threat redirect can hold them.** Every 10s each one casts
Flame Lash, then `DoResetThreatList()` and charges a **uniformly random** player within 80 yd
(`boss_freya.cpp:1256-1262`); they spawn the same way after a 5s submerge, and one that finds nobody
inside 80 yd despawns and still counts as cleared. A `GROUP_LASHERS` wave is **10** of them — 717k in
10-man, **2.35M in 25-man** — so the whole raid has to damage them to beat the 60s clock. They run at
**8.0 yd/s** against a player's 7.0: a bot can lead one anywhere and can never shake it.

Detonate (62598) rolls 4162-4837 in 15 yd **on death**, not on a timer, and has **no difficulty entry**,
so it is identical in both sizes — the old 10-man/25-man threshold split was wrong.

**The corral.** `GetFreyaLasherCorral` is 35 yd behind Freya, taken from `GetHomePosition()` — she never
walks but she pivots to face her tank, so the live orientation would swing the spot around the room.
Every bot derives the same point. navprobe puts all 16 headings of that ring on mesh; at 50 yd the
southern ones stop settling, which is what sets the distance. Height only, no collision raycast: several
triggers read it per bot per tick and that floor is open.

Ranged and healers a lasher has picked walk it in, and that trip is the **only** walk they make: they
still never move toward the focused lasher, and outside spell range they shoot whatever is already in
reach. **Melee never ferry** — a melee bot that did would then be standing in the pile the blasts go off
in — and neither does the trap hunter, which has a post of its own. A lasher chases whoever it picked,
so a bot cannot hand one over and walk away; without two brakes the drag ferries in, the step-out pushes
the bot straight back out, and it shuttles the same add all wave. Nothing is ferried into a pile this
bot would itself have to flee, and nothing is ferried to a corral already holding
`ULDUAR_FREYA_LASHER_PACK_MIN_COUNT` (6) — a cap that lifts itself as the pile dies. `freya lasher pack
step out` is the return leg, so there is no walk-back node, and it clears
`ULDUAR_FREYA_LASHER_PACK_CLEAR` (16 yd) of *every* lasher, since ten roam at once.

**A snare and a root hold the pack, not threat.** One hunter — lowest GUID, so every bot agrees — posts
`ULDUAR_FREYA_LASHER_TRAP_OFFSET` (16 yd) short of the corral, where its 10 yd Frost Trap patch covers
the lane back to the raid while it stays outside the blast; a 30s patch on a 30s cooldown is a continuous
**-50%**. A mage with 6 lashers inside Frost Nova's 10 yd roots them for a full **8s** — no DR, no damage
break ([../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md)) — and novas above the
step-out, so it roots first and leaves second. It casts on **self**, not through the class `frost nova`
node, which gates on the *current target* being within 10 yd and so never fires for a ranged mage.

Assist tank 0 parks at the corral once its ladder is empty (`HoldLasherCorral`) and taunts what wanders
off. Every branch there owns the tick: falling through hands the tank back to the ladder, whose last rung
is Freya, and walks it straight off the corral. A taunt is worth under 10s — the next threat wipe re-rolls
regardless — so it holds one add at a time, plus one Challenging Shout / Challenging Roar at 6+.
Righteous Defense is excluded: it taunts attackers of a friendly target, not an area.

Ten Detonates in one pile would be ~45k inside 15 yd. It stays survivable because ranged focus-fire one
lasher at a time (`GetFreyaRangedLasherFocus`: lowest health, GUID breaking ties, which agrees raid-wide
with no shared state and is self-stabilising since the focused add stays lowest), so deaths stagger and
the off-tank at the corral eats them one at a time. Raid AoE bringing several low together is the residual
risk, deliberately untuned.

Melee and tanks still take only what is inside `ULDUAR_FREYA_MELEE_LASHER_RANGE` (12 yd) and drop it the
moment it runs past that, which is the leash: a lasher that retargets cannot tow a bot across the room,
and a tank can damage one on top of it but can never walk one back to the raid. Non-tanks below
`ULDUAR_FREYA_DETONATE_FLEE_HEALTH` step out of the blast, clearing *every* lasher in range rather than
the nearest, except the bot actually killing that lasher, which is inside 15 yd by definition, and except
a dragger inside `ULDUAR_FREYA_LASHER_CORRAL_COMMIT` (12 yd) of the corral, which commits rather than
waste the trip. Tanks never flee; they eat it.

**Threat redirect.** `freya redirect threat` feeds Misdirection / Tricks to assist tank 0 while the
Snaplasher or Conservator is up, otherwise to whoever is holding Freya, falling back to the group main
tank. `NPC_FREYA` is in `UldThreatRedirectMultiplier`'s block list so the class-generic main-tank node
stands down. This can do nothing for lashers — their threat table is wiped every 10s.

Hard mode = Elders left alive at pull (Brightleaf 32915 / Stonebark 32914 / Ironbranch 32913). Per
living Elder, Freya gains an extra ability: Iron Roots (62862), Unstable Sun Beam (62450), or Ground
Tremor (62437, raid-wide knockback — not handled, undodgeable).

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
bot cannot move**, so it must free itself before it can step out of anything. The beam dodge rings
outward to a spot clear of every beam in range, not away from the nearest one.
`ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS = 12.0f` is a DBC guess.

### Mimiron — Firefighter

A player presses the Big Red Button before the pull; `_hardmode` is set at activation and never
cleared. Detection uses `SPELL_EMERGENCY_MODE (64582)`, the empower aura applied to whichever mech is
currently active — those mechs *are* valid attack targets, unlike Mimiron himself.

Hazards: persistent ground fire (`NPC_FLAMES_INITIAL` 34363 → `NPC_FLAMES_SPREAD` 34121, aura 64561)
that creeps toward the nearest player all fight, and the VX-001 **Frost Bomb** (`NPC_FROST_BOMB`
34149). Both flame nodes are non-selectable trigger creatures, so they are found by scanning
`"nearest npcs"` — the same idiom as Freya's beams. Bots avoid the bomb *creature*, so no spell id is
needed and 10/25 are covered identically.

**Correction to the master plan**: Emergency Fire Bots (34147) are **friendly, non-combat fire
extinguishers**. They never enter combat with players, never heal or repair Mimiron, and only run to
flame nodes and cast Water Spray to put fires out. They are **not** kill targets.

### Yogg-Saron — reduced Keepers

The raid frees fewer than 4 Keepers, losing that Keeper's support. Tuned for the hardest single-Keeper
case, **Thorim only**. The bitmask lives in `PERSISTENT_DATA_WATCHERS_MASK` — preferred over Sara's
`GetData(DATA_GET_KEEPERS_COUNT)`, which gives a count only, because reading the mask confirms
**which** Keeper.

What Thorim-only removes: no Freya means **no Sanity Wells, so Sanity (63050, 100 stacks) is a
one-way drain** — nothing restores it; no Hodir means no Protective Gaze absorb; no Mimiron means no
haste clouds and therefore a slower kill and more total drain.

Sanity drains, and whether anything can be done:

| Source | Spell | Loss | Avoidable |
|---|---|---|---|
| Psychosis | 63795 / 65301 | −9 / −12 | **No** — random target every 3.5s in P2 |
| Malady of the Mind | 63830 / 63881 | −3 | Yes |
| Brain Link | 63803 | −2 | Yes, if the pair stays within 20 yd |
| Lunatic Gaze (P2 skull) | 64168 | −2 | Yes — only players *facing* the caster |
| Lunatic Gaze (P3 Yogg) | 64164 | −4 | Yes |
| Induce Madness | 64059 | −100 | Yes — leave the brain room |

**Thorim's Titanic Storm auto-kills anything carrying `SPELL_WEAKENED` (64162)**, and a guardian
drops Empowered at ~<10% HP. So **melee only need to burn a guardian to Weakened; Thorim finishes
it.** With no Thorim, guardians are effectively unkillable without the cheat — which is why the P3
cheat instakill is suppressed **only when Thorim is a Keeper**, leaving other reduced-Keeper combos
winnable.

Crusher Tentacle (33966) is **stationary** and nobody tanks it — with no one in melee range its Crush
cannot land, so ranged nuke it in place. Its Diminish Power (64145) is a raid-wide DPS debuff, so it
must die fast.

Open risks: the removed P3 cheat existed because Lunatic Gaze "freezes" bots, so guardian DPS may
stall between gazes; a guardian spawns up to 48 yd out, so confirm taunt pickup is prompt; and the
sanity-conservation behaviour (stand behind Yogg facing away below 15 stacks) **nearly benches a bot**
— sanity never recovers Thorim-only, so a bot that drops to 15 stays there.

### XT-002

Ulduar is 10/25-man only, so **"hard mode" here is the Heartbreak split, not a difficulty flag**.
Killing the Heart during its 30s exposed window (63849) sets XT to full health, grants **Heartbreak
permanently (65737 on 10-man, 64193 on 25-man)**, and stops rescheduling the phase check — so there
are no further Heart phases.
Heartbreak is a reliable runtime signal that hard mode is live, but there is no signal *before* the
kill, so the config declares intent: on, bots burn the Heart to zero on the first window; off, they
stop at `ULDUAR_XT002_HEART_SAFE_HP_PCT` (15%) so the fight stays in normal mode. In hard mode the
Heart goes to the top of the non-tank target list and tanks join the burn; in normal mode neither
happens, because a stray tank hit is exactly what flips the raid by accident.

**Fork quirk, deliberate:** `npc_xt_toy_pile::SummonDistance = 90.0f` with the check
`if (!xt002 || xt002->IsWithinDist(me, SummonDistance)) return;` — **adds only spawn when XT is more
than 90 yd from a pile**, so tanked in place they essentially never appear on this core. Add handling
is written to work whenever adds do spawn; tank positioning is deliberately untouched. There are also
**no adds after Heartbreak**: toy piles summon only when hit by the Heart's energy orb, and
`RescheduleEvents` omits `EVENT_PHASE_CHECK` once `_hardMode`.

Boombots explode for 15-18k on reaching XT **or at 50% health**, so melee must never touch them.
Scrapbots walk to XT and heal him, so they must die en route.

**Void Zone and Life Spark are both gated on the Heartbreak aura, not on the config.** Both spawn out
of an `AfterEffectRemove` handler that checks `xt002->HasAura(aurEff->GetAmount())` — the Heartbreak
aura — before summoning (`spell_xt002_gravity_bomb_aura`, `spell_xt002_searing_light_spawn_life_spark`
in `boss_xt002.cpp`). So anything reacting to a Void Zone or a Life Spark must key off
`IsXT002HeartbreakActive`, which reads that aura; `IsXT002HardModeActive` is the *config* flag and is
only correct where the code states intent ahead of the Heart dying, such as the 15% Heart floor.

**`aurEff->GetAmount()` is the difficulty-correct id**, so read Heartbreak through
`GetXT002HeartbreakSpellId`, never a constant. Hardcoded to the 10-man 65737 it is false for a whole
25-man pull, taking the parking lot, both Void Zone filters and the burst window with it while
puddles and sparks spawn normally. Silent, and only on 25-man; the trace tell is burst-window vetoes
still firing long after the Heart died.

**Searing Light and Gravity Bomb each repeat on a 16 s (25-man) / 20 s (10-man) timer**, longer than
the debuff lasts, so there is never more than one carrier of either type at a time.

XT spawns at `(886.28, -12.05, 409.6)` facing −x (orientation 3.13) and is the only DB-spawned
creature in the room; every add is script-summoned, so room geometry cannot be checked from the world
DB. Verify a destination with navprobe instead - never with `IsWithinLOS`, see below.

`NPC_XT002` (33293), `NPC_XT_TOY_PILE` (33337), `NPC_XS013_SCRAPBOT` (33343) and
`NPC_HEART_OF_DECONSTRUCTOR` (33329) come from core `ulduar.h` via `UldScripts.h` — **do not
redeclare them.**

Use the `IsBurstCooldownAction` registry rather than hand-rolling a `dynamic_cast` list the way
BT and SWP do.

#### Anchors, and why nothing pre-positions

The tank spot and the ranged anchor `(866.0, -12.5, 409.8)` stay; the pre-pull walk to them does not.
`XT002RaidPositionTrigger` requires `xt002->IsInCombat()`, and the generic-mover stand-down carries the
same gate, so a ranged bot near XT before the pull can still follow its master.

- **The tank anchor yields whenever XT has a victim that is not this bot**, matching Ignis. "No
  victim" deliberately keeps the anchor — that is the Heart window, where standing on the spot is
  right. There is no boss-taunt node because XT is taunt-immune at the core level; recovery is threat
  from damage, which is what unpinning restores.
- **The tank holding XT taunts the Pummeller but never targets it.** Taunt reaches 30 yd, so the add
  walks to the tank instead of the tank walking 80 yd to the add. Taunt ownership and target
  ownership are deliberately different things. The gate is "is there a second alive tank"
  (`GetGroupTankNum(bot) > 1`), not "am I the main tank" — a dead flagged main tank leaves
  `IsMainTank` false for the survivor, who would then chase with the boss in tow.
- **Ranged and healers get a slot each, never the anchor itself.** Sharing one coordinate put 13
  living bots inside 3 yd of it, four dying on the exact point — Searing Light is 8 yd, so that is the
  whole group. `GetXT002RangedSlot` deals slots off an ellipse (half-axes 6×8 inner, 9×12 outer),
  wider north-south because east-west is the line to XT and to the tank spot, so spreading along it
  costs range and across it costs nothing. It is centred **6 yd north of the anchor**: on the anchor,
  the southern slots sit 14 yd from the nearest parking cell, inside Gravity Bomb's 20 yd pull, and an
  expiring puddle would drag those bots into it. Offset, every slot is inside 30 yd of XT and 39 yd of
  the tank and 23 yd clear of the nearest cell. Sorted healers-first then guid, so healers take the
  centre and inner ring and every bot derives the same layout untold. Slots in Consumption are dealt
  *out of the list*, not stepped around, or two bots pick the same one. Sitting at `ACTION_RAID` the
  node outranks `reach party member to heal`, so the healer branch **stands down while a heal target
  is out of spell range** or a healer could never close on a carrier parked in the lot (far cells are
  55.6 yd from the anchor against 40 yd of heal range).
- **The Heart is hidden, not despawned**, when its window shuts (`UNIT_FLAG_NOT_SELECTABLE`,
  `ACTION_DISPOSE_HEART`). `IsAllowedTarget` checking only `IsAlive()` left a bot stuck on it for the
  rest of the fight with every queued spell failing; it now rejects untargetable units and requires
  the Exposed Heart aura, and the sticky rule in `ResolveTarget` no longer holds a target the gates
  just rejected.

#### One mover per bot: the carrier and hazard nodes are each merged

Two nodes that can both move the same bot will tie on relevance and fight over the queue, so
`xt002 debuff carrier` fires on either debuff and resolves one destination per tick, and
`xt002 avoid hazard` clears Boombots and Void Zones in one move.

- **Carriers are held by the multiplier, not by returning `true`.** Both carrier actions return
  `false` on arrival, so the tick continued and `reach melee` at `ACTION_HIGH + 1` walked the carrier
  back mid-debuff. `XT002TargetGuardMultiplier` now zeroes generic movers for anyone carrying either
  debuff, any role, while the encounter's own movers and every cast keep running — a carrier action
  returning `true` would starve everything below `ACTION_EMERGENCY + 1` for 9-10 s and mute a
  debuffed healer. `AttackAction` stays exempt: it never moves the bot, and zeroing it kills the
  encounter's own targeting.
- **Gravity Bomb outranks Searing Light on a double carrier.** The puddle denies raid floor for 180 s;
  the 12 yd splash lasts 9 s and expires over an empty lot. The Life Spark then spawns out there and
  walks in by itself. Reversed, a 180 s puddle lands on the one spot reserved for sparks.
- **The carrier keeps ownership while it holds either debuff.** When the bomb expires with Searing
  Light still ticking, the bot is standing in its own fresh puddle — which makes its old cell test
  occupied, so the picker moves it 6-12 yd to the next free one.
- **The hazard node ignores Void Zones while the bot carries a debuff** and always honours Boombots.
  Cell selection already owns where a carrier stands relative to puddles; letting the generic dodge
  fire would fling it up to 30 yd in whatever direction was emptiest.
- **Hazard moves take the nearest sufficient point, not the furthest** — `MoveClearOf`'s ring search
  stops at the first spot that clears everything, unlike `MoveAwayFromCreatureAction`'s
  maximise-distance sweep. It takes per-unit clearances, so one search serves the pre-Heartbreak ally
  spread at 25 yd and the mixed hazard list of Boombots at 12 and Void Zones at 8.
- **`avoid aoe` is zeroed for everyone during XT's combat.** Nothing is lost: Tympanic Tantrum is
  room-wide so it exceeds `maxAoeAvoidRadius`, and the Life Spark has no damage aura.
- **Healers are in the generic-mover stand-down too.** `follow` lives on the non-combat engine and a
  healer with nothing to heal drops combat constantly — 973 of 2857 actions in one pull, against 1.7%
  for ranged — so it out-issued the anchor three to one and walked them to the master all fight.
  Their disperse goes with it; the anchor's slots are what spreads them now.
- **Healers keep a target, and nothing here may clear it.** A bot with no target never attacks, never
  enters combat, and so runs the non-combat engine — for a priest that is renew, penance and greater
  heal, with no Power Word: Shield, Prayer of Mending, Pain Suppression, Shadowfiend or Hymn of Hope,
  and for a paladin no Beacon of Light. Clearing healer targets left all four targetless for 100% of
  a pull against ~33% on other Ulduar bosses, with not one shield or Beacon even evaluated. Ulduar is
  in `RestrictedHealerDPSMaps` so the target costs no GCD; what it buys is `Attack`'s
  `ChangeEngine(BOT_STATE_COMBAT)`. `dps assist` is exempted for healers for the same reason.

#### Nothing here is reachable just because it is on the mesh

Two silent failures, both measured from one wipe trace: the parking lot was chosen **0 times in 161
carrier moves**, and Searing Light dealt 1.53M damage to bystanders against 298k to carriers.

- **`IsWithinLOS` rejects the entire parking lot.** The building geometry ends at y ≈ −29 (vmap
  surface ~404 under the raid, none south of it) and the lot sits past that edge on bare terrain, so
  a ray from the raid clips the rim while the path to every cell is `PATHFIND_NORMAL`. **No XT-002
  mover has an LOS test any more** — the lot, the ring search and the stop-short point are all
  validated by `MoveTo`'s outcome instead. On the ring search the gate was pruning 820 of 823 ticks
  down to one candidate, which leaves the retry loop nothing to retry.
- **The flakiness has a direction.** From the melee stack every point north of the raid answers
  `NOPATH` and every point south-west answers `NORMAL`. North of the formation is open floor and
  unreachable, which is why the Searing Light spot sits south-west instead.
- **`findSmoothPath` refuses walkable points, and refuses the same ones every tick.** The Searing
  Light spot returns `PATHFIND_SHORTCUT|PATHFIND_NOPATH` from anywhere in the melee stack; on a 34 yd
  ring around XT, headings 0/45/60/135° fail and 30/90/120/150/180° succeed with the mesh 8/8 present.
  `SearchForBestPath` accepts only `PATHFIND_NORMAL|INCOMPLETE`, so `MoveTo` returns false and the bot
  does not move **at all** — for the full 9 s debuff, since re-offering one winner re-offers the same
  refusal.

So every mover here ranks its candidates and offers them in turn, up to
`ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS`, through `MovementAction::TryMoveTo`: `MoveTo`'s bool collapses
`NoPath` into the same false as `Duplicate` and `Waiting`, and reading those as failure turns a
carrier around mid-run. Only `NoPath` earns another candidate; `NotAllowed` means yield the tick.

#### The parking lot is a time budget, not a coordinate

The grid is 5×4 from a raid-facing origin, walking +x / −y so every added cell is further out; all 40
cells are navprobe-verified on mesh. Step is just over the Void Zone diameter, so consecutive drops
cannot overlap.

- **Ranking is reachability first**, inside the aura's remaining duration at the bot's current speed,
  then room, then a clear approach. `GetSpeed(MOVE_RUN)` already carries the tantrum slow, so no
  encounter code has to know the tantrum exists. Only a bot actually holding the bomb is budgeted —
  stepping off its own puddle is a short hop with no deadline.
- **When nothing is reachable the carrier stops short**, keeping the bearing to the best cell and
  walking as far as the budget allows, but only if that point is at least
  `ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS` (20 yd, the real pull radius) from every living raider — so
  the puddle lands on the approach instead of in the raid. Otherwise the ring search takes the tick.
  The point needs no latch: as the bot advances, reach and distance to the cell shrink together.
- **Approach clearance is a preference, not a gate** (`_BOMB_CELL_PREFERRED_CLEARANCE` 8.0, falling
  back to 6.0, approach margin 7.5). As a hard gate one puddle would block five cells of twenty and
  exhaust the lot. There is no puddle dodging on the way in — the carrier sits at
  `ACTION_EMERGENCY + 1` and starves the Void Zone node — and the segment test is an approximation,
  since `MoveTo` follows a navmesh path rather than the line measured.
- **`ParkVoidZone` reports on cells, not on `MoveTo`** — "already walking there" is not "nowhere to
  go", and "no path to there" is a fourth answer meaning try the next cell. Sapphiron's
  `ShelterResult` shape otherwise: latch on arrival, `StopMoving()`, deadband 2.0, re-engage 5.0,
  tighter than Sapphiron's because the puddle lands at the carrier's feet and the destination is
  static. Arrival is measured against the nearest free cell, not the best-ranked one, so a bot on a
  cell has parked whatever the ranking prefers elsewhere. The parked carrier then yields the tick, so
  a healer can heal and a hunter can shoot XT from the lot.
- **The Searing Light spot needs alternates, ranked away from the raid.** A Void Zone parked 2.1 yd
  from it and stayed for the last 133 s of a pull, and carriers kept being sent in. The spot is
  `(846, −22, 409.6)`, south-west: 15.7 yd from the nearest formation slot and 19.2 yd from the
  nearest parking cell, so the 8 yd splash reaches neither. The carrier takes it, or a point on a
  10 yd ring that is puddle-free, pathable, and `ULDUAR_XT002_SEARING_LIGHT_SLOT_CLEARANCE` (12 yd)
  clear of every slot. **Rank by clearance, never by distance from the bot**: nearest-first returns
  the headings pointing back at the raid, and carriers took those 132 times in one pull with 4–6
  raiders inside the splash every time. A fixed destination here is checked against the formation as
  well as against puddles — the two are laid out independently and will drift into each other.

#### Traps

- **A bubble drops the puddle early.** Divine Shield (642) and Ice Block (45438) both apply
  `SPELL_AURA_SCHOOL_IMMUNITY` over every magic school with `SPELL_ATTR1_IMMUNITY_PURGES_EFFECT`, and
  `spell_xt002_gravity_bomb_aura::OnRemove` has no removal-mode check — so a paladin or mage that
  bubbles while carrying summons its Void Zone on the spot. Bots cast both from "critical health" at
  relevance 90, which arrives during a Tympanic Tantrum: exactly when the carrier is still in the raid
  and cannot walk out. `XT002TargetGuardMultiplier` zeroes both while the bot carries either debuff;
  Searing Light is in the same school mask and drops its Life Spark the same way. Hand of Protection
  (1022) is physical-only and strips neither; Anti-Magic Shell blocks the bomb landing rather than
  dropping one.
- **Hand of Freedom cannot help a carrier outrun the tantrum.** 62775 carries `Mechanic = 0` and no
  effect mechanic, while Hand of Freedom is keyed to `MECHANIC_ROOT` and `MECHANIC_SNARE` — same for
  the PvP trinket, Every Man for Himself and shapeshift. Flat speed buffs do work (Sprint gives
  5.25 y/s against 3.5), but only some classes carry one and the carrier is whoever the boss picked.
- **Bots do not dodge Life Sparks and cannot.** Static Charged (64227) is an enemy area aura at radius
  index 30 — **500 yd** — so position changes nothing, and no avoid node can match it. What looked
  like dodging was melee making the 34 yd round trip to the spark.
- **Searing Light stays carrier-only, and bystanders eat it.** It damages allies within 8 yd every
  second for 9 s, nothing steps out of it, and generic `avoid aoe` cannot see it — its dynobj branch
  needs a `DYNOBJ_AURA_TYPE` and its unit branch a `NOT_SELECTABLE` trigger NPC, and a player is
  neither. The carrier needs 4-6 s to clear, and a Pummeller off-tank irradiates the melee stack for
  the whole debuff because tanks never move for it. Accepted: 25 bots scattering costs more than the
  damage.
- **Tanks do not reposition for Searing Light** — dragging XT or abandoning a Pummeller costs more
  than the splash — but they do park for Gravity Bomb, and a Pummeller following an off-tank into the
  lot is fine. The main tank can hold neither debuff anyway.
- `MoveAwayFromPlayerWithDebuffAction` takes a **single** spell id fixed at construction, so it cannot
  cover both the 10 and 25-man ids of Searing Light or Gravity Bomb.
- **A trigger and its action must measure a hazard the same way.** `TooCloseToCreature` asks
  `FindNearestCreature`, whose range test subtracts both object sizes; `MoveClearOf` scores centre to
  centre and caps at zero. In the gap between them the trigger says "too close" while no candidate
  beats standing still, so the action fails every tick — and `XT002RaidPositionTrigger` yields to that
  trigger, so the anchor cannot recover the bot either. One ranged bot sat 64.7 yd from XT for 125 s
  that way, casting nothing, until it died. So the dodge clears to Void Zone radius + 2, leaving
  headroom, and the anchor yields only while the bot is inside the raw radius by the action's own
  measure.
- **Two carriers can pick the same cell** — there is no reservation. Bombs are 16 s apart against a
  9 s duration so two bomb carriers never overlap, and the only bot that can collide is one sitting
  out a Searing Light, which drops nothing.

## Algalon

One AI for 10N and 25N. Only two spells are difficulty-mapped — Big Bang `64443 → 64584` and Black
Hole Explosion `64122 → 65108` — and neither Phase Punch (64412) nor the phase aura (62169) is, so the
strategy carries one extra spell id and is otherwise difficulty-agnostic.

Every timer in the encounter is offset by an intro that runs **26 s on a first pull and 8.5 s
afterwards**, which is why nothing here counts from combat start. The room is a **47 yd disc** around
`(1632.668, -302.7656, 417.32)` with a floor at `z >= 410`; `IsInRoom` calls `ACTION_ASCEND` the moment
Algalon leaves it, so nothing may pull him or a tank past the edge.

| Mechanic | Ids and cadence | Handling |
|---|---|---|
| Quantum Strike | every 3-4.5 s, ~27 k / ~15 k on 25-man | Two tanks or nothing — see below |
| Phase Punch | 64412, every 15.5 s, 45 s aura, 5 stacks | Swap at **4**; the 5th stack phases the tank out for 10 s |
| Collapsing Star | 32955, every 60 s, tops up to 4 alive | Killed one at a time; each death is 16-21 k to the raid |
| Black Hole | 32953, 6 yd field, no target cap | Big Bang shelter and the constellation sink |
| Cosmic Smash | markers 33104/33105, impact 4 s later, 41 437 base | `< 6` yd full, `6-10` `dmg/dist*2`, `>= 10` `dmg/dist` |
| Living Constellation | 33052, 3 activate every 50 s | **Not a kill target** — `HealthModifier = 20` |
| Big Bang | 64443 / 64584, every 90.5 s, 8 s cast | 76 312 / 107 249 at 50 000 yd radius; position is irrelevant |
| Phase 2 | at 20 % HP | Stars, constellations and holes despawn; 4 Worm Holes (34099) spawn on the fixed square |
| Unleashed Dark Matter | 34097, one per Worm Hole per 30 s | `speed_run 1.42857` — faster than players, so it is tanked, never kited |
| Ascend / enrage | 64487 at 6 min | Also fires on the Big Bang evade |

### Big Bang, and the evade that used to end the attempt

`spell_algalon_big_bang::CheckTargets` calls `ACTION_ASCEND` when the spell hits **zero** targets, and
the boss evades about 4 s later. A raid where everyone hides therefore resets him. **Big Bang is also
unavoidable and cannot be immuned** — Divine Shield does not stop it — so the one bot left standing has
to *mitigate* it.

Dispersion's 90 % reduction does that, but its cooldown is **120 000 ms** against a 90.5 s cadence, so
no single bot covers consecutive casts unglyphed. The duty therefore **rotates**: the lowest-guid bot
whose soak is actually off cooldown, Dispersion first, then Guardian Spirit, then a damage dealer who
stays out and probably dies. A corpse still counts as a target, which is strictly better than a reset.
`AlgalonSoakCooldownReserveMultiplier` reserves the chosen bot's cooldown by returning `0.0f` for it
outside the cast, or the priest spends it on the normal `low mana` / `critical health` nodes and has
nothing when it matters. Every other priest behaves normally.

The soaker is latched per cast in `algalonEncounterStates`, because the trigger that exempts a bot from
hiding and the action that spends the cooldown must agree — a per-tick re-derivation strands whoever was
exempted half a second ago.

**Threat survives the hide, and this is not a bug to re-audit.** `CombatManager.cpp:53` gates only
*entering* combat on `InSamePhase`, and `ThreatManager` never purges entries on a phase change: Algalon
simply cannot select a phased target and re-picks the main tank when the phase drops.

### Black Holes are where a star died, not where it spawned

The Collapsing Star spawns on one of four fixed points, then `MoveRandom(25.0f)`; the Black Hole is cast
on **its own position** when Collapse finishes it. So phase 1 holes land anywhere in the room, and any
formation slot can end up buried under one. Only the phase 2 Worm Holes sit on the fixed
`CollapsingStarPos` square. `TryGetAlgalonSlot` therefore steps a bot around a hole on its slot rather
than abandoning the formation, and `algalon leave black hole` outranks the formation node so a bot that
ends up standing in one leaves instead of paying 1531 a tick for nothing.

### Star pacing, which is not optional

Collapse drains **1 % of max health per second**, so an untouched star kills itself after ~100 s, and the
60 s summon only tops up to four alive. Four ignored stars therefore explode within seconds of each other
about 143 s in, for 64-84 k of unavoidable raid damage at once. The kills are deliberately staggered:
focus the **lowest-health** star (health percent *is* the remaining-lifetime clock, so lowest-first
spaces the deaths for free), require the raid's weakest member above 80 % and 8 s since the last
explosion, and override that gate when a star drops under 15 % — a death nobody chose is a death that
lands on top of the next one. `AlgalonCollapsingStarAoeMultiplier` vetoes `DpsAoeAction` while two or
more stars are alive so splash cannot undo the pacing. Phase 2 has no stars, which leaves Dark Matter
cleave untouched.

### Holes are a resource, and the raid can run out

Holes exist only where a star was killed, and every constellation eats one — three constellations per
50 s against four stars per 60 s consumes them faster than they appear. Inside the 30 s before a Big
Bang the kite refuses to spend the last hole, and if there are none at all
`AlgalonTargetGuardMultiplier` takes non-tanks off Algalon entirely until a star dies. A skull mark is
advice; the veto is what makes the star actually die in time.

### Constellations are kited by whoever they already chase

A constellation picks its victim at activation via `AddThreat(target, 100.0f)` and chases it. The
strategy does not appoint a kiter — it uses the bot the constellation already picked, which is the only
bot that can lead it anywhere without a taunt. The single exception is a constellation parked on whoever
is holding Algalon: the other tank pulls that one off, since one bot cannot kite and tank at once.
`AlgalonTargetGuardMultiplier` keeps everyone else off them, because 20× base health inside a six minute
enrage is not a fight anyone wins — the kite through a hole's 65509 (radius 6, one target) is the only
removal there is.

The kiter parks **9 yd past the hole** on the far side from the constellation, outside the 6 yd field.
The spot converges as the constellation closes, so the node yields once parked and gives its tick back
to instants, heals and the class interrupts.

### Formation

The raid enters from +Y — the planetarium console sits ~128 yd that way — so the tank slot is on the
**−Y** edge of the worm hole square at `(1632.7, -321.5)`, 18.7 yd out from Algalon's home position and
10.2 yd clear of the nearest hole spot. There is no drag action: the main tank simply has a slot, and the
boss follows through normal chase.

Ranged and healers ring that slot at 14 / 20.5 / 27 yd, healers innermost, 18 slots in all, filled
centre-out and latched per instance so one death does not renumber everyone behind the corpse. The arcs
are trimmed rather than full half circles because the two −Y hole spots sit level with the tank slot; the
trims keep every slot at least 7.7 yd from all four. Spacing runs 8.9-12.1 yd, which puts a marked bot's
neighbours in Cosmic Smash's cheap `dmg/dist` band. All 19 points are navprobe-verified on map 603, flat
at Z 417.321.

The formation yields while a Cosmic Smash marker is within 12 yd of the slot. This is the Mimiron trap in
different clothes: testing the *bot's* surroundings passes trivially for a bot that already dodged, and
only testing the **slot** stops it walking back under the meteor.

### What is deliberately not supported

**Single-tank raids.** The swap trigger stays inert when the group has no second tank rather than
drafting one, because a damage dealer taking Quantum Strike dies in two swings and a fake off-tank would
hide the failure instead of fixing it.

**Bloodlust stays on the pull.** Phase 1 is 80 % of the health bar and where every mechanic steals damage
time; phase 2 is 18 % and short.

## Ignis

The fight is a construct-disposal loop, not a damage race. Each Iron Construct (33121) Ignis
activates puts a stack of Strength of the Creator (64473) on him, and a construct **cannot be killed
by damage** — it only dies to the chain:

1. it stacks Heat (65667) while standing in a Scorched Ground patch (33123), and turns **Molten**
   (62373) at 10 stacks, which also wipes its threat table;
2. a Molten construct within **18 yd of a water trigger** turns **Brittle** (62382 10-man / 67114
   25-man) on the construct's own once-a-second poll;
3. any single hit of 5000 (10-man) / 3000 (25-man) then shatters it, killing it and removing a
   Strength stack.

Scorch only lights a patch when it lands more than 25 yd from water, so the fire and the pools are
always separate places and the walk between them is the mechanic. The two pools are at
`(526.771, 277.796, 360.802)` and `(646.771, 277.796, 360.802)` — hardcoded as
`ULDUAR_IGNIS_WATER_POOL_WEST` / `_EAST` rather than found by entry, because the water trigger is
22515, the generic Ulduar world trigger.

### The main tank picks where every patch lands

Scorch summons its patch at `boss + 20 yd` along the boss's facing, and the boss faces the main
tank. His spot therefore has to put the patch more than 25 yd from both pools — closer and it never
lights, stalling the loop — and off the raid.

`ULDUAR_IGNIS_BOSS_ANCHOR` (587.5, 277.8) is the room centre and the only spot where either bearing
clears both pools. The tank stands 9.5 yd off it — melee range is ~10.8, Ignis' combat reach being
8.0 — on a bearing of **south**, putting patches south, pools east and west and the raid north:
three axes that cannot collide. Patch to either pool then lands at 44–78 yd; the construct walk is
~45 yd, ~5 s against a 30 s Molten window.

He works **three arc slots 60° apart**, advancing one on the rising edge of Scorch (62546). Ignis is
rooted and rotation-locked for those 3 s and the patch spawns from the orientation frozen at cast
start, so the step is free and leaves him 16 yd from the patch — outside its 13 yd burn — where
standing still would have left him 9 yd inside it. The slot is latched per instance id. A reactive
"step out once it lands" was rejected as the documented oscillation pair.

**Nothing pre-positions**: the tank walks the boss to the anchor in combat, from wherever the pull
happened. Ignis runs at 10 yd/s against a player's 7, so he stays glued to a tank at full speed: a
melee-range leash check is the whole guard, no throttled drag.

### Two construct tanks, pools fixed by index

Assist tank 0 takes the **west** pool, assist tank 1 the **east**, and each works the lit patch
nearest its own pool. Fixed by index rather than distance because the anchor sits almost exactly
between the pools: nearest-pool is a tie that always resolves the same way, stacking both Molten
pulses and both Shatters in one spot.

Kiting is **assist-tank only, on purpose**: a main tank pulled off Ignis drags the boss along behind
the construct, and a DPS holding a Molten one dies. A raid without an assist tank skips the loop and
lets the Strength stacks climb; with one, tank 1 never finds a construct anyway, since
`GetIgnisDrivenConstruct` excludes whatever the other tank holds.

### Targeting is direct — no raid icons

Nothing here sets, clears or reads a raid target icon. `ignis attack brittle construct action` and
`ignis attack boss action` resolve the unit and `Attack()` it, and
`IgnisDisableDefaultTargetingMultiplier` zeroes `DpsAssistAction`, `TankAssistAction` and
`AttackRtiTargetAction` for the whole encounter — not just the Brittle window, or a settled `Attack`
returning false drains the queue to `dps assist`, which re-picks every tick.

The Brittle pick is the **lowest GUID** raid-wide, so every bot converges with nothing shared to
agree through. Ranged and casters take it, with a designated per-class burst spell (Pyroblast,
Chimera Shot, Chaos Bolt, Mind Blast, Starfire, Lava Burst) because rotation filler often will not
reach the 5000/3000 alone. Melee are let in only for the last 7 s of the 15 s window: Shatter deals
18850 in 13 yd, inside melee range of the thing they would be swinging at.

With the generic pickers off, `ignis attack boss trigger` is also what puts the **main tank** on
Ignis — it excludes construct tanks only.

### Everything else

**Flame Jets** (62680) is an observable 2.7 s cast. Bots stop a cast that cannot land before the
knockback and start no new one that would not finish either; instants and short heals keep going.
The 6 s lockout afterwards is unavoidable — this only stops feeding casts into it.

**Slag Pot** (62717 / 63477) is a vehicle ride: healers pour direct heals into the victim, and the
victim's movement actions are suppressed, since orders only fight the ride and leave it facing the
wrong way when it drops. Neither tank can be potted — the core skips the boss's victim and every
construct's victim.

Hazard radii are sized against the spells, not the visuals: the Scorched Ground dodge is 15 yd
(62548 burns in 13), the Molten avoid 15 yd (sized against Shatter's 13, not the ~7 yd pulse).

`IgnisTankMovementMultiplier` takes the generic movers off the main tank and the two construct tanks
and nobody else — the three roles whose spot the encounter owns. Non-tanks keep everything, which is
what spreads the ranged half without an anchor of their own.

Ignis has no hard mode. Heroic is free: the paired spell ids above are both checked, and the only
other 25-man difference is the construct cadence (30s instead of 40s).

Every node is gated on `IsIgnisEngaged` — alive **and** in combat, since the boss is visible from
the whole 200 yd room. Lookups go through `GetIgnis` (a grid search), not `"find target"`: a bot
parked on a construct never has Ignis on its threat list, and a dormant construct carries
`UNIT_FLAG_NOT_SELECTABLE`, which drops it out of `"possible targets"` entirely. The room is wider
than SightDistance too, so the cached `"nearest npcs"` list goes blind at the pools — every Ignis
lookup searches the grid at 200 yd.

## Auriaya

No hard mode. Entries are shared; Sonic Screech is the only difficulty pair the strategy reads.

| Mechanic | Ids | Handling |
|---|---|---|
| Sonic Screech | 64422 / 64688 | **Soaked, never dodged** — see below |
| Terrifying Screech | 64386 | Fear every 35s from the pull, so the whole fight is one anti-fear window |
| Sentinel Blast | 64389 | Raid-wide, **not** a cone: no `spell_cone` row, and its SpellScript strips non-players. Healed through |
| Sanctum Sentry | 34014 | Assist tank 0 taunts each loose one; Strength of the Pack (64369) buffs the boss while they live |
| Feral Defender | 34035 | Random aggro (61906) makes it untankable — focus-killed, never tanked |
| Seeping Feral Essence | 34098 | Non-selectable stalker, one per Defender life; cleared at 10 yd |
| Guardian Swarm | 64396 | Tank DoT, left to the generic dispel |
| Enrage | 47008 | 10 min, unhandled — no enrage awareness exists anywhere in the module |

**Sonic Screech is a damage split, not a dodge.** `spell_custom_attr` carries
`SPELL_ATTR0_CU_SHARE_DAMAGE` on both ids (64422 also `IGNORE_ARMOR`; 64688 does not — upstream
asymmetry), so the 120° cone (`spell_cone`) divides **60,125–69,875** (10N) or **190,000–210,000** (25N) among
everyone it hits. She faces her victim, so the old design — non-tanks sidestepping out while the main
tank arc-stepped her away from the raid centroid — left the tank eating it unsplit, a guaranteed death
at 25N. It also never converged: each tank step swept the cone across the raid, and the bots it clipped
moved, shifting the centroid the tank steered by. Bots soak it now, and there is no
cone node left.

**Anchoring is hybrid**, because she walks to her victim and cannot be pinned to world coordinates
the way XT-002 is:

- **Main tank** → a fixed spot from `ULDUAR_AURIAYA_MAINTANK_SPOTS`. A stationary tank is the entire
  facing control; there is nothing left to steer.
- **Ranged and healers** → boss + 20 yd along the boss→victim bearing, rounded to π/16 so tank drift
  cannot shuffle twenty bots. Reading her live victim rather than a fixed bearing is what keeps the
  split working when a human tanks.
- **Melee and assist tank 0** → unanchored. Melee sit behind her and do not soak; at either raid size
  the remaining soakers already make each share small.

`AuriayaMovementGuardMultiplier` zeroes generic movers for the anchored roles only, or the anchor
oscillates. It spares `AttackAction` and `ReachTargetAction` — both are `MovementAction`s, and a
blanket veto would kill targeting and strand healers out of heal range.

**The pools are permanent**: summon 64457 has `DurationIndex 21` (−1), no SmartAI touches 34098, and
the Defender's 30s respawn never despawns them, so up to 9 accumulate per pull. Hence stations: three
tank spots 10 yd apart along her home facing, which runs away from the corridor at +x. The tank
advances when a pool lands within 12 yd of a station's spots, and only the tank computes the index —
everyone else inherits the move through the bearing, so no two bots can disagree. The dodge itself
takes the **smallest** step that clears, leashed to the bot's anchor; maximising distance from the
nearest pool is what used to walk bots out of the room and up the corridor.

Kill order is **Sentries → Feral Defender → boss**: sentries stay dead and drop the boss's buff, where
each Defender kill costs a pool and buys 35s. Targets are picked in code — bots set no icons, but a
mark a player sets still wins. Melee take the Defender only within 15 yd of the boss, or they chase it
across the room as it re-rolls aggro. It feigns at 1 HP wearing `UNIT_FLAG_NOT_SELECTABLE`, so it
resolves through `GetFirstLiveUnitByEntry`, never `GetFirstAliveUnitByEntry`. Savage Pounce (64666)
fires only at 8–25 yd from the sentry's own victim, so a tank holding it in melee is the whole
counter — the taunt needs no positioning code behind it.

**Every Auriaya trigger used to resolve the boss through `"find target"`**, which walks only the
bot's own threat list: any bot fighting a sentry or the Defender silently lost its dodges and its
anti-fear. All of them go through `GetAuriaya`, by entry, now.

**Crazy Cat Lady requires no sentry killed, so it is incompatible with the kill order.** Bots
optimise for the kill and, per the follower model, never chase achievements.

## Kologarn

**No raid target icons.** Skull means "everyone DPS this" and Moon is the CC channel, so a per-role
split built on them leaks into the generic engine. Targets are picked in code per role, the SWP
Eredar Twins model — which *requires* `KologarnDisableAutomaticTargetingMultiplier`, because
`DpsTargetValue::Calculate` falls back to a smart-target strategy when no icon is set and is
therefore **never null**: `NotDpsTargetActiveTrigger` stays true and `dps assist` retakes the target
on alternating ticks. It zeroes `DpsAssistAction`, `TankAssistAction` and
`CastDebuffSpellOnAttackerAction` — the last is what stops DoTs landing on whatever the bot drifted
onto.

| Role | Target |
|---|---|
| Body tank (`kologarn->GetVictim()`) | the body, held in melee |
| Off-tank (the other of MT / AT0) | rubble while any live; else the right arm, but only inside 30 yd taunt range of the body |
| Melee DPS | right arm while it lives, else the body |
| Ranged DPS (`IsRangedDps`, excludes healers) | rubble while any live, else as melee |

Rubble duty is **derived, never stored** — the off-tank is whoever is not holding the body — so it
follows the taunt swap on its own.

**Nothing engages before the boss does.** Every Kologarn trigger resolves him through
`GetFirstAliveUnitByEntry`, a pure `AiPlayerbot.SightDistance` proximity scan — 100 yd, no line of
sight — that answers "is he nearby", never "is he engaged", and `AttackAction::Attack()` has no
out-of-combat guard, so the raid used to pull itself from up to 100 yd out. `KologarnEncounterActive`
is `kologarn && kologarn->IsInCombat()`, the VoA `EmalonEncounterActive` shape, and gates the triggers
and the targeting multiplier alike. Resolving by entry rather than through `find target` — which walks
only the bot's own threatened-by-me list and therefore cannot fire before engagement — is deliberate:
a tank parked on the body never has the arms on its threat list, so the per-role focus split
collapses without it.

### Facts that contradict the retail guides

| | |
|---|---|
| Arm respawn | **50s**, not 60 |
| Crunch Armor | **63355** (−20%, 4 stacks, 45s). **64002 is never applied here**, so the old cheat checking it was dead code |
| Stone Grip | 62166 / 63981, **1 target**, caster's victim stripped → **the body tank is exempt** |
| Focused Eyebeam | 3 most distant players via `NonTankTargetSelector`, then **exactly one** at random → one runner, never the tank. Eye lives **10s**, chases at **5.5 yd/s** against a player's 7.0; beam is a **3 yd** AoE |
| Rubble | 8.0 yd/s — **faster than players, so held, not kited**. SmartAI: Rumble 63818 (10) / **Stone Nova 63978 (25, 10 yd, ~5550 + knockback)** |
| Shockwave (63783) | **200 yd**, nothing narrows it |
| Petrifying Breath | fires only when the body's victim is out of melee **and** `SelectNearbyTarget` finds nobody close — any body in melee suppresses it |

That tank exemption from both Grip and Eyebeam is what stops "body tanked at all times" and "run from
the eyebeam" ever conflicting.

Boss at `(1797.15, -24.40, 448.74)`, `o≈π`: **entrance is -X**, arms split along **Y**. The walkway
runs from the Shattered Walkway Door (x 1740.84) to the broken span at x 1782, past which
`boss_kologarn_pit_kill_bunny` instakills inside x 1782–1832 / y -56…8 / z 400–439.

### Mechanics

**Tank swap** at 2 Crunch Armor stacks, incoming tank holding **strictly fewer** — not an absolute
cap, which deadlocks both tanks at 2 and stops swapping for good, since 45s duration against a 14s
Smash timer never lets stacks clear. Equal stacks correctly means hold.

**Rubble** are held **laterally**, toward the dead arm's side, ~18 yd off the raid — never backward:
-X is the eyebeam escape lane. Ranged AoE falls out of the role table plus the engine's `"aoe count"`
thresholds, so no AoE action exists.

**Focused Eyebeam** is a real run, not a teleport, re-stepped each tick: -X toward the entrance, then
along the walkway, then back toward the boss. Turning that corner matters — the eye outlives the ~5s
of -X runway. Bystanders `FleePosition` off it. None of it needs the raid cheat.

**Petrifying Breath** gets its own `ACTION_EMERGENCY` guard sending the nearest tank, then nearest
melee, into melee range whenever the body is uncovered. The swap handover is already covered by the
swap action's `Attack`; the case that wipes is the MT dying while the off-tank sits 18 yd out.

**Stone Grip** victims are stunned passengers, so `KologarnMultiplier` zeroes their movement — orders
only fight the ride. Freeing them needs no code: DPS already focus the arm.

Healers need nothing boss-specific: `PartyMemberToHeal` does not filter vehicle passengers, so
gripped victims are already picked up. Do **not** reach for `"focus heal targets"` — it is an
*exclusive* filter and would starve the rest of the raid.

Nature resistance is wanted, since Shockwave and Petrifying Breath are both Nature. Only the first
alive hunter raises it, and Aspect of the Wild 49071 is `APPLY_AREA_AURA_RAID` +
`MOD_RESISTANCE_EXCLUSIVE`, so a second adds nothing. Limit: 30 yd radius.

### Not implemented, deliberately

- **Left arm** — dies to incidental cleave. Focusing it doubles rubble spawns and risks a
  both-arms-down Stone Shout window when the two 50s respawn timers drift into phase.
- **Shockwave** — 200 yd hits the platform wherever anyone stands: a healing check, not a dodge.
- **Raid formation** — no mechanic needs it, and fixed offsets on a narrow walkway over an instakill
  pit is where bots fall in.
- **Fall-from-floor teleport kept** — a pathing workaround, not a mechanic: a bot under the walkway
  is in the kill box and dies within a second, so there is no walk-back to attempt.

## Vezax

Facts below come from the script, the DBC CSVs and `acore_world`. **They override the public guides
where they disagree, and they do disagree.**

| Spell | Id | Effect |
|---|---|---|
| Aura of Despair | 62692 → 64848 | No mana regen, −20% melee attack speed. On aggro |
| Shadow Crash (cast) | 62660 | Every 10s from 13s. Random player **beyond combat reach**, falling back to *any* target — it can land on the tank |
| Shadow Crash (impact) | 62659 | 11,310 + knockback, **10 yd**. Instant on missile landing — unreactable |
| Shadow Crash (field) | 63277 → 65269 | **8 yd, 20s.** +100% magic and +75% shadow damage done, +100% cast speed, −70% mana cost, **−75% healing done**. A `SPELL_EFFECT_PERSISTENT_AREA_AURA`, so it exists as a `DynamicObject` |
| Searing Flames | 62661 | On the tank, **radius 100 yd** = the whole raid. 13,875-16,125 fire, −75% armour 10s, every **8s (25m) / 15s (10m)**. **2000 ms cast, `PreventionType = 1`** — genuinely interruptible |
| Surge of Darkness | 62662 | 63s from pull, repeats 63s. +100% physical damage, −55% move speed, 10s. Delays the Searing Flames group 10s |
| Mark of the Faceless | 63276 → 63278 | 20s from pull, repeats 40s, lasts 10s. Drains 5,000 hp/s from allies **within 15 yd** and heals Vezax. Prefers a player **beyond 15 yd** when ≥9 (25m) / ≥4 (10m) are out there, else someone inside |
| Saronite Vapors (NPC 33488) | summon 63081 | Every 30s. `NullCreatureAI`, no addon auras, `MoveRandom(4.0f)` — **the living cloud is harmless** |
| Saronite Vapors (puddle) | 63323 (30s) → 63322 | Dropped on the **corpse**. 8 yd, reapplied every 4s. Deals `100 · 2^stacks` and returns **half as mana** — stack 5 is 3,200, stack 8 is 25,600 |
| Saronite Animus | NPC 33524 | Hard mode. At vapor #6 with none killed, every vapor charges the anchor and merges |
| Saronite Barrier | 63364 | −99% damage taken on Vezax until the Animus dies |
| Profound Darkness | 63420 | Animus self-cast every 2s. 749 damage plus **+10% shadow damage taken per stack, 180s**. **Radius index 28 = 50,000 yd — room-wide and unavoidable** |
| Berserk | 26662 | 10 min, and instantly if the boss leaves `x ∈ [1720,1940]`, `y ∈ [20,210]` |

Two guide instructions that are wrong on this core: keep interrupting Searing Flames during the
Animus — **you do not**, `boss_general_vezax.cpp:228` skips the cast entirely while the Barrier is up;
and kill a vapor if mana is fine — **any single vapor kill calls `DoAction(1)` and disables hard mode
permanently** for that pull.

**The mana cheat is gone, not kept as a fallback**, following the Mimiron rebuild. The vapor puddle is
the raid's only mana source: healers first, non-mana classes never soak, and the exit is
HP-predictive — leave when the next tick (`100 · 2^(stacks+1)`) would exceed
`ULDUAR_VEZAX_VAPOR_SOAK_MAX_TICK_HP_PCT` (0.35) of current health, which self-tunes across gear and
raid size. If the soak underperforms, tune it; do not reinstate the cheat.

**The formation is a fixed arc, Sunwell-style.** Anchor `(1852.78, 81.3856, 342.461)`,
navprobe-verified, `ULDUAR_VEZAX_ARC_WIDTH` = π at orientation −1.5291. Healers take an inner band of
**8 slots at 15 yd** so `PartyMemberToHeal`'s 30 yd measurement always reaches the tank; ranged take
**6 at 21 yd** and **6 at 28 yd**. Two rings rather than one because a single 25-man ring gives 6.9 yd
of spacing against Shadow Crash's 8 yd field — the split gives 12.6 and 17.6. The arc ends are 39 yd
apart, but every DPS slot has a healer slot within ~13 yd, and aggregate coverage is what the heal
engine needs. Slots persist per instance, pruned when their holder goes invalid; Ulduar's other
bosses re-derive from a GUID rank every tick, which reshuffles the whole formation on a death.

**Returning `false` once parked is load-bearing.** Class interrupts sit at `ACTION_INTERRUPT` (40),
below `ACTION_RAID` (60), so a positioning action that returns `true` while moving starves every
interrupt that tick — and Searing Flames is the one cast in Ulduar that genuinely rewards
interrupting. Duty is GUID-ranked among bots that are both *capable and ready*, recomputed per cast so
cooldowns rotate it naturally.

**Field soak is bounded.** Only casters whose slot is within about one ring spacing of a live field
move to it (`_SHADOW_CRASH_SOAK_MAX_TRAVEL` 15). Unbounded chasing collapses the arc into one 8 yd
circle. Puddles are killed **in place** — slot-safety reassignment handles a covered slot, and a
puddle on the healer band is convenient. Mark of the Faceless walks to whichever of three spots
derived off the arc bearing is nearest, all navprobe-verified; travel time is the whole cost of the
mechanic.

**Hard mode needs four separate guards, and the DoT one is the easy miss.** Targeting is zeroed
(`DpsAssistAction`, `TankAssistAction`), plus `CastDebuffSpellOnAttackerAction` on a vapor target and
every AoE-threat-type cast while a live vapor is in range, plus **explicit pet control** — a hunter
pet off passive will chew a wandering vapor with nobody noticing. A DoT ticking a vapor to death is
the quietest way to lose hard mode. Bloodlust arms only once the Animus is alive and only with hard
mode on. Assist tank 0 taunts the Animus; the main tank keeps Vezax inside the berserk bounds.

**Positioning is gated on the room, not just on presence.** Vezax is visible from outside his hall,
and a presence gate had bots prepositioning through walls before the pull while their generic movers
were already zeroed. `VezaxFormationActive` requires the bot inside a 45 yd bubble around the anchor
plus a 10 yd height band — the room door spawns at `(1854.86, 31.53)`, 49.9 yd out. Resistance and
state reset stay presence-gated; everything else is combat-gated.

Shadow Crash landing in the melee stack, and the strafe that answers it, is covered under
[Core behaviours](#core-behaviours-the-strategies-key-off).

## Core behaviours the strategies key off

Upstream script and DBC facts our code now depends on, with the behaviour each one drives. Every one
of them silently disabled something before it was accounted for, so re-check them after any
parent-repo sync.

### Razorscale — harpoons are GameObject state, not auras

`SPELL_CHAIN_1..4` (49679, 49682, 49683, 49684) **no longer exist anywhere in the core**. Harpoons
fire `SPELL_HARPOON_SHOT_1..4` cast by `NPC_RAZORSCALE_CONTROLLER` out of
`go_razorscale_harpoon::OnGossipHello`; the GameObjects are summoned per ground phase by the
controller (two in 10-man, four in 25-man) and a spent one carries `GO_FLAG_NOT_SELECTABLE` until it
is rebuilt. Readiness is that flag — `IsHarpoonFired()` and `HarpoonData::chainSpellId` are gone,
they had been testing an aura that could never be present.

The tank debuff also moved: **Fuse Armor is 64821**, not 64771 (64774 is still the 5-stack `Fused
Armor`). 64771 is gone from the core, so the tank-swap check never fired. Threshold stays at 2 stacks.

### Razorscale — Devouring Flame is 5 yd, and the skull has one owner

The patch is NPC 34188 carrying 64709, a 2 s periodic trigger of 64704 (64733 in 25-man), **whose
damage radius index is 8 = 5.0 yd**. Bots clear `DEVOURING_FLAME_CLEAR_RADIUS` (7 yd) so a step
actually leaves the patch instead of stopping on its edge, and `DevouringFlameBlocks()` rejects any
destination covered by another one — she drops these every 6–12 s and they stack up.

Clearing is only half of it. `razorscale avoid devouring flames` also **holds the tick without moving**
while the spot a melee bot would walk back to is on fire; `RazorscaleMultiplier` zeroes the generic
movers and `avoid aoe` for the same window. Both release the moment the tank drags her clear, so
nobody stands out the patch's full life. A permanent veto here is the freeze bug.

`razorscale kill target action` is the **only** thing that sets the skull: the boss whenever she is
on the floor, otherwise Sentinel → Watcher → Guardian. `DpsTargetValue` prefers the RTI target, so a
skull left on an add is the whole raid left on an add. Moon stays on the boss while she is airborne —
it is excluded from every DPS target scan — and is cleared on landing.

The generic pet-attack node is commented out engine-wide (`CombatStrategy.cpp`), so pets keep whatever
they last hit unless a script re-orders them: `razorscale pet control action` puts them on the adds
while she flies and on her when she lands, and always reports failure so the tick falls through.

### Vezax — Shadow Crash can land in the melee stack

The event used to only consider players beyond 15 yd; it now picks a random target outside combat
reach and falls back to any target, so the puddle can drop on the tank. Detection was never the
problem (`VezaxShadowCrashTrigger` keys off the puddle's area aura 63277) — the dodge orbited at a
hard-coded 15 yd and dragged every melee out of range and held them there. It strafes at the bot's
own radius now: melee and tanks 4–8 yd, ranged 13–17 yd, constant 5 yd of arc per step so a tight
radius still clears the puddle quickly. Tunables sit next to the other Vezax ones in `UldBossHelper.h`.

### Mimiron — Laser Barrage is a 104° cone, not a beam

The single most load-bearing number on this boss, and it contradicts every public guide.

**63297 and 64042 deal no damage.** They are `SPELL_EFFECT_DUMMY`, placing the two beam *visuals*
via `TARGET_DEST_CASTER_FRONT` (60 yd) plus `TARGET_DEST_DEST_LEFT` 4 yd / `TARGET_DEST_DEST_RIGHT`
6 yd. Reading those 4/6 yd radii as a beam width is what produced the old dodge.

The damage is **63293**: `SPELL_EFFECT_SCHOOL_DAMAGE`, `TARGET_UNIT_CONE_ENEMY_104`, radius index 28
= 50000 yd. `Spell.cpp` maps that target type to a **104° cone** (±52° through `HasInArc`, which
compares `arc/2`), and no `spell_cone_angle` row overrides it. Guides describe retail's 30° visual.

So **distance from VX-001 buys nothing** — the cone outreaches the room. Only bearing matters.

Aim comes from `FaceBarrageArc`: VX-001 is repointed at NPC 33576 every tick of the aura, and 33576
laps the room on a fixed spline every 34016 ms — 10.6°/s, clockwise, ~106° over the 10 s barrage.
Warning is 4 s of Spinning Up (63414); cadence is 60 s. **NPC 33576 is spawned by world DB update
`2026_08_10_00.sql`; without it `FaceBarrageArc` returns early and the cone never moves.**

#### The cone ignites 42.6° clockwise of where the boss is pointing

The single biggest correction to the above. **Spinning Up does not track.** `EVENT_SPELL_SPINNING_UP`
calls `FaceBarrageArc` once and then casts 63414, a 4 s aura whose single 4000 ms tick triggers 63274.
Only 63274 carries the re-aiming script, on `AfterEffectApply` and every 250 ms after. So the boss
aims once, holds that facing for four seconds while 33576 travels another **42.6°**, and the cone then
snaps to the live bearing and starts firing from there.

An arc latched at Spinning Up start is therefore 42.6° stale for the entire cast. Measured against it,
the real danger band is `(−213.7°, +21.2°)` while the old model treated `(−170.9°, +64°)` as unsafe —
so 43° of genuinely lethal floor was reported safe, and bots fleeing clockwise stopped 43° short, in
the exact slice where the trailing edge finishes. At **20000 damage every 250 ms** that is fatal.

There is no latch any more. `GetMimironBarrageWindow` reads the two aura durations off VX-001 and
predicts where 33576 will be at ignition and at the last tick, by rotating its live position clockwise
about the room centre. Everything is recomputed each tick, so a moving apex, a rotating chassis and a
bot joining mid-cast all fall out for free, and every bot still derives the same cone without
coordinating because every input is world state.

#### The orbit, measured rather than assumed

Path 13395 is 19 waypoints at velocity **20.8988**, perimeter **707.0 yd** — a **33.8 s** lap, so
**10.64 °/s** and **106.4°** per barrage, bearings strictly decreasing. A least-squares circle fits it
at centre (2741.98, 2569.36), r = 113.2, which is **2.7 yd** from `ULDUAR_MIMIRON_ROOM_CENTER` — close
enough to orbit about the room centre and stay within a couple of degrees over a whole cast.

**That rate only holds for a VX-001 standing at the room centre.** The bearing rate seen from an
off-centre observer is not constant: at 15 yd it swings 9.2–12.5 °/s, at 30 yd **8.3–14.7 °/s**. Phase
4 rides the chassis that far out, so a fixed 10.6 °/s is off by up to 40° over one barrage. Predicting
33576's actual position removes the error instead of budgeting for it.

#### The danger band cannot be folded to (−π, π]

Sweep plus two clearances is **240°** at the room centre and wider off it. A signed fold puts part of
that band on the far side of π, which is precisely how the opposite side of the room came back "safe"
while the beams crossed it. The dodge works in a clockwise-from-centreline measure in `[0, 2π)`
instead, where the band is one contiguous interval and there is nothing to wrap.

The rest of the dodge is unchanged in shape:

- it is **selective** — bots already outside the swept union never move and keep casting;
- it rotates at **constant radius**, since radius is irrelevant to safety and melee keep their uptime;
- it picks direction on **time spent inside the cone**, not distance travelled.

That last one is the part that is easy to get wrong, and distance is the wrong currency: the short way
round is frequently straight through 104° of beam. Three cases, on which side of the centreline the
bot sits:

| Where the bot is | Direction | Why |
|---|---|---|
| More than 52° clockwise of the centreline | **clockwise** | The beams have not reached it. Running clockwise keeps it that way at zero cost; turning back walks it into a cone it is currently in front of. |
| Inside the cone, leading side | whichever edge is **sooner** | Counter-clockwise rides the sweep out through the trailing edge at `turnRate + rate`; clockwise pushes out through the leading edge against it at `turnRate − rate`. Crossover lands near 30° off the centreline. |
| Counter-clockwise of the centreline | **counter-clockwise** | The sweep is already carrying it clear; it is out in a fraction of a second. |

**During Spinning Up neither direction gets help from the sweep.** The boss aims once and holds for
four seconds, so the band is fixed in world space and both edges close at plain `turnRate`. Getting
that wrong is not cosmetic: it made a bot 25° inside the ignition cone at 24 yd pick the far edge —
92° of travel against the 67° its four seconds actually bought — and it was still in the cone when the
beams lit. With the rate corrected it leaves by the near edge in 1.6 s and is ahead of the sweep for
the rest of the cast.

A simulation over the real waypoint path — every combination of DB Target phase, bot bearing, orbit
radius 14–24 yd and chassis offset out to 30 yd in eight directions — clears **62,208 of 62,208
positions**, with the committed legs and the movement lock modelled rather than assuming the bot can
correct continuously. The same harness scores the previous model at 9.3 % of bots told to stand still
while the cone crossed them, before counting the ones it sent the wrong way.

**The step size is not a safety dial.** Re-running that sweep at 40°, 30°, 20°, 15° and 10° gives zero
hits at every one of them, and *smaller steps are worse* under a moving apex, not better. Every failure
the harness can produce needs the apex to be dragged around underneath the raid — at 2 yd/s of drift
0.53 % of positions are caught, and widening the margin from 15° to 30° only takes that to 0.34 % and
then plateaus. It is not a clearance problem and cannot be tuned away.

What prevents it is the apex holding still, which is why the phase 4 main tank's tighter floor is
load-bearing for the whole raid rather than a tank convenience. `Unit::GetMeleeRange` is
`ownerReach + targetReach + 4/3` — 8 + 1.5 + 1.33 = **10.83 yd** — and `ChaseMovementGenerator`
leaves the chassis alone inside that. The tank orbits at reach + 1.5 = **9.5 yd**, so it never triggers
a chase and the cone apex stays where it is.

The margin is **15°**, up from 12. The cone turns 2.7° per damage tick, so 12° was about one bot
reaction interval with nothing spare. 15° still leaves a 120° safe wedge for a 25-man raid at 22 yd,
and the action drops to a 100 ms recheck while it is actually relocating.

The 24 yd cap on the arc spread is what makes this work: it keeps the worst-case 52° rotation inside
the 4 s window with 1.6× speed margin.

**Without NPC 33576 the cone is frozen, not sweeping.** `FaceBarrageArc` returns early when it cannot
find the target, so the core never re-aims at all. The fallback reports `sweep = 0` and a static
±clearance wedge around VX-001's current facing — running a sweep that is not happening would walk the
raid straight through the beams.

**Rotate in bounded steps, and never below a floor radius.** Creatures are absent from the navmesh,
so `MoveTo` will happily draw a chord straight through VX-001 — and a chord across the apex crosses
every bearing the cone covers, which is a guaranteed hit. The dodge therefore issues one leg of at
most 40° per tick (a 40° chord stays within 6% of the ring) and clamps the radius to
`[combat reach + 6, 24]`. VX-001's combat reach is **8**, so the floor is 14 yd: melee sit inside
that and would otherwise try to orbit through the model. `MOVEMENT_FORCED` sequences the legs for
free — `IsWaitingForLastMove` refuses anything not strictly above the move already in flight.

### A bot mid-cast cannot be moved at all

This is general, not Mimiron-specific, and it silently defeats every dodge in the module.
`PointMovementGenerator<T>::DoInitialize` returns without launching a spline when
`unit->IsMovementPreventedByCasting()`, and `DoUpdate` calls `StopMoving()` and returns early on the
same test. `Unit::IsMovementPreventedByCasting` is true for any `UNIT_STATE_CASTING` except a channel
carrying `IsActionAllowedChannel` — so instants are fine and everything else is not.

Two things make it worse than "the move does nothing". `MovementAction::MoveTo` has its `CastStop` /
`InterruptSpell` block **commented out** (`MovementActions.cpp:222-226`), so it returns `true` and
stamps `LastMovement` with the full travel delay for a leg that never started — which then blocks the
bot's own retries through `IsWaitingForLastMove` for the length of a walk it never took. And the
calling action reads that `true` as success and holds the tick.

A balance druid died to Rocket Strike this way. It is **5,000,000 damage in 3 yd** (63041, radius idx
15), so one occurrence is one death.

The Mimiron dodges that kill outright call `botAI->InterruptSpell()` before moving — Laser Barrage,
Rocket Strike, Shock Blast, and the Firefighter flames and Frost Bomb. The ones that do not kill keep
their cast: Proximity Mine is **9,000** and a Bomb Bot **12,000** (63009), both healable, and clipping
a cast every time a mine lands costs more than the mine does. `PlayerbotAI::InterruptSpell` is free to
call when nothing is casting, and `SpellInterrupted` has no side effect beyond a redundant interrupt,
so the 100 ms recheck during a barrage costs nothing but a queued melee special. Kara, Gruul, Magtheridon
and Naxxramas already did this; Mimiron did not.

### Mimiron — raid nodes must resolve bosses by entry, not by threat

`AI_VALUE2(Unit*, "find target", "<name>")` walks `bot->GetThreatMgr().GetThreatenedByMeList()`
(`TargetValue.cpp:159-184`) — it only ever resolves a boss that already has **this bot** on its threat
list. That is fine for a boss which calls `DoZoneInCombat`, and quietly fatal for one that does not.

VX-001's phase 4 `SetData` calls neither `DoZoneInCombat()` nor `AttackStart()`, and its `AttackStart`
is a no-op override. So a bot that never damaged VX-001 — a healer, a melee locked on the chassis —
got `nullptr` and **the Laser Barrage dodge never ran for it at all**. Every Mimiron node now uses
`GetFirstAliveUnitByEntry`, which reads the grid-swept `"possible targets no los"` and has no threat
dependency. The same defect is still live across a dozen other instance strategies.

### Mimiron — the two adds need opposite answers

**Proximity Mine (34362)** carries `unit_flags = 2` (`UNIT_FLAG_NON_ATTACKABLE`): there is no
legitimate way to remove one. It arms 2.5 s after landing, then polls every 500 ms for a player
inside **1.9 yd**, blasts 3 yd (66351), and self-detonates at 35 s. Ten land 8 s after every Shock
Blast. Avoidance is the whole answer — and because mines are non-selectable they never reach
`"possible targets"`, so pathing is blind to them. `IsMimironSpotMineSafe` gates the destination of
every Mimiron move **except the barrage dodge**, where the cone kills instantly and a mine does not.

**Avoid tightly, and rank it last.** Ten mines inside a 15 yd circle leave no clear ground if the
avoid radius is wide, so the node stays permanently active and the raid paces. Trigger at 3.0 yd
(the 1.9 yd poll plus slop), clear to 3.5 yd, cap the step at 5 yd, and take the *nearest* clear
spot — the generic `MoveAwayFromCreatureAction` maximises min-distance over a fan reaching 30 yd and
picks a different answer every tick, which is what produced both the long runs and the oscillation.
The node sits at `ACTION_RAID - 1`, below every other Mimiron node: Shock Blast is **100000 damage
in a 15 yd circle** (63631, radius idx 18) and a mine is a healable 3 yd, so a mine must never cost
the raid a dodge, a taunt or a core delivery. Some mine hits are the intended price.

**Melee do not dodge mines at all, and nobody dodges them during a barrage.** The ten mines scatter
inside 15 yd of the MK II (65347, radius idx 18) every 30 s, which is precisely where melee have to
stand, so the node fired more or less continuously. Work out what it was buying: 10 mines of 3 yd blast
inside a 707 yd² circle is about 40 % coverage, so a stationary melee expects ~0.4 hits a cycle —
**roughly 120 dps**, against the whole of its uptime and, in phase 4, the barrage deaths that come from
being nudged onto a cone-blind bearing. Ranged keep the sidestep; every healer spec is `IsRanged`
(`PlayerbotAI::IsRanged` reads `STRATEGY_TYPE_RANGED`), so "melee" here means melee DPS plus warrior,
DK, protection paladin and feral tanks. The barrage gate is a suppression rather than a filter,
because a mine is survivable and the cone is not.

**Bomb Bot (33836)** is the opposite: `speed_run` 1.14286 works out at **8.0 yd/s against a player's
7.0**, so it is not merely un-outrunnable, it gains on you, and it detonates on melee contact
(`SMART_EVENT_DAMAGED_TARGET` → 63801, 5 yd). `HealthModifier` is 1.5873, so ranged kill one in a few
globals — it sits at the top of the ranged priority list.

**Range decides who shoots it.** `MimironBombBotTrigger` stands down for a ranged DPS with a Bomb Bot
inside `SpellDistance`, so the DPS list gets it instead of the flee node; healers and melee keep the
sidestep, which is all a 5 yd blast is worth. Out-of-range Bomb Bots are filtered out of
`BuildPriorityList` entirely — targeting one the bot cannot reach abandons the mech for an add
somebody else can hit, and lands the bot in the `reach spell` versus `ACTION_RAID` deadlock below.

**And it is the one Mimiron add that takes a snare.** Its immunity set is **−263**, which leaves
`SNARE`, `ROOT`, `STUN`, `FREEZE`, `GRIP` and `KNOCKOUT` off — every one of which the Assault Bot's
**−285** carries. At 20,000 HP and 8.0 yd/s, a second of extra approach is most of a cast.
`mimiron slow bomb bot` puts the class snare on whichever Bomb Bot the bot is already shooting:
hunter `concussive shot`, shaman `frost shock`, warlock `curse of exhaustion`. Roots are deliberately
absent — Entangling Roots and Frost Nova break on the first hit, and hitting it is the plan.

Two gates keep it from being a waste of a global. It reads `"current target"` rather than scanning, so
it can only fire on a Bomb Bot `mimiron set dps priority` already handed the bot — which also means
healers never snare. And the Bomb Bot has to be more than **15 yd from its own victim**, not from the
caster: measured from the caster, a hunter 25 yd away would happily snare one that is already two
yards from a healer. `ServerFacade::GetChaseTarget` answers that, falling back to the caster's own
distance before the add has picked anyone.

Both used to be handled by a main-tank `unit->Kill()` gated on `BotCheatMask::raid`. That is gone.

### Mimiron — Rapid Burst and Hand Pulse cannot be dodged

Both are also `TARGET_UNIT_CONE_ENEMY_104`. Rapid Burst (63387/64019) is aimed at a random player
every 3.2 s at 100 yd; Hand Pulse (64348/64352) fires every 1.75 s in phase 4. No arrangement avoids
a 104° cone, so the six fixed phase-2 spots that used to stack the raid into three clumps were
solving a problem that does not exist — and three clumps is the worst shape for a cone. They are
replaced by a ring of radius 22 yd, one slot per ranged bot. The *bearing* is index-derived and never
keyed off VX-001's facing, which swings to whoever it last Rapid Burst.

**The ring centres on the mech, not the room.** Bots cast out to `AiPlayerbot.SpellDistance` — 28.5
here, with no `AC_` override — so a ring pinned to the room centre puts the far half of the raid out
of range after about six yards of boss drift. It does not recover on its own either: `reach spell` is
`ACTION_HIGH` (20) and the ring is `ACTION_RAID` (60), so the ring wins every tick and walks the bot
straight back out. That deadlock is why ranged simply stop attacking rather than visibly struggling.

**Phase 1 drift has a cause worth fixing at the source.** The tank is melee, so the Shock Blast
trigger fires for it too: it runs, the MK II follows, and nothing brings either back. Over a five
minute phase that walks the fight around the room. The main tank now gets `ULDUAR_MIMIRON_ROOM_CENTER`
as its phase 1 slot — the same point `boss_mimiron.cpp` charges the MK II to — and drags the boss home
after every cast. Its slot is exempt from `IsMimironSpotSafe` (`IsMimironTankAnchorSlot`), because the
MK II parks on top of the mine field it just laid and a tank that refuses to stand in one never
returns.

**Recovery is a rigid translation, never a per-slot clamp.** When the outermost slot falls outside
casting range the whole anchor slides toward the mech by the excess; clamping slots one at a time
would deform the ring into a lopsided blob leaning at the boss, which is the clump shape the spread
exists to prevent. Every input (anchor, focus, radius, count) is group-global and the config distance
is read directly rather than through `PlayerbotAI::GetRange`, so each bot derives the same anchor
without coordinating. A second clamp keeps the anchor within 40 yd of the room centre — navprobe puts
the walkable floor at 16/16 headings out to 40, flat at Z 364.31.

### Mimiron — phase 3 wants a wedge, not a ring

The add summon pads (GO 194740-194748) sit on **three arms** leaving the room centre at 180°, +59.4°
and -59.4°, each carrying pads at roughly 17, 29 and 40 yd. A ring therefore parks lone ranged bots
directly in an add's path. The raid stages in the **east wedge** instead — the one direction nothing
walks down — centred on the bearing to `ULDUAR_MIMIRON_PHASE3_STAGE` (2762.65, 2569.46, 364.31;
navprobe 16/16 on mesh with a 12 yd fan around it). Ranged and healers fill outward from 18 yd.

**Melee get no slot at all, and neither do the tanks.** They used to take inner rows at 8 and 14 yd,
which was the same deadlock as the phase 1 ranged ring, one relevance band down: every add walks in
from a pad at 17, 29 or 40 yd, `mimiron set dps priority` hands a melee bot one of them, `reach melee`
starts the chase at `ACTION_HIGH` (20), and the formation drags it back at `ACTION_RAID` (60). The
formation wins every tick, so the bot never lands a swing. Any fixed melee slot on this phase is a spot
the target is not in. Melee stand on whatever they are hitting.

That also removes the last reason to add a `disperse distance` back: melee are free-moving now, and a
disperse would be one more node fighting the chase for the same tick. Bomb Bots are the only phase 3
mechanic that punishes stacking, and melee already have a node for those.

The wedge is **built to fit casting range** rather than grown until it stops fitting: rows step by 6 yd
(one more than a Bomb Bot blast) and stop at `SpellDistance` minus the margin, and once the band is
full the remaining rows pack tighter. Spacing is the thing to give up, not range — a Bomb Bot catching
two bots is cheaper than half the raid unable to cast. At ±60° a 17-strong ranged group still holds
4.7 yd of separation. The phase 3 `disperse distance` of 5.0 is gone with it: the slots already do
that job, and `CombatFormationMoveAction` shoving bots off slots the formation pulls them back onto is
pure thrash.

**The wedge is anchored on the room centre and never slides.** It used to slide toward the Aerial
Command Unit until the outermost row was inside casting range, which sounds harmless and was not.
`extent` was measured in every direction while the wedge only occupies 120° of one, so with
`SpellDistance` 28.5 (margin 4 → 24.5) and a two-row wedge of extent 24 the excess came out at
`dist − 0.5`: the anchor landed **half a yard from the boss** every tick, whatever the room centre
said. The excess was never clamped to the distance either, so it could overshoot and place the anchor
on the far side of the unit entirely.

That closed a loop. The unit uses `AttackStartCaster(who, 30.0f)`, so it holds **30 yd** from its
threat target — and its threat target is a ranged bot standing in the wedge the anchor is dragging
after it. Raid and boss then circle the room together, which is what "ranged oscillate instead of
doing damage" looks like from the floor.

Holding still is also the whole Bomb Bot fix. They spawn on the unit (`SPELL_SUMMON_BOMB_BOT` is cast
on self), so once the wedge stops chasing, the unit's own 30 yd standoff is what a Bomb Bot has to
cross: **~3.7 s** of free fire on a 20,000 HP add at 8.0 yd/s, against approximately none while the
raid was closing on it. Pushing `ULDUAR_MIMIRON_PHASE3_MIN_RADIUS` past 18 buys nothing here — the
unit keeps 30 yd from its victim wherever that victim stands.

Nothing is lost by not sliding, because **the phase 3 Aerial Command Unit has no attack**. Its entire
event list is `EVENT_SUMMON_{BOMB,ASSAULT,JUNK}_BOT` plus the Firefighter fire bots; Plasma Ball is
scheduled only in phase 4. Range on it matters for exactly one thing, the Magnetic Core window, and
melee and pets cover that on foot.

### Mimiron — a dodge that returns false hands the tick to Charge

Shock Blast (63631) is a **4 s cast**, `TARGET_SRC_CASTER`, 15 yd, 100000 damage, every 30 s. Four
seconds is a long time to leave the tick open, and `charge` and `intercept` sit at `ACTION_MOVE + 10`
= 40 with nothing above them once the dodge node yields. A gap-closer moves in a straight line and
consults nothing about the ground, so a warrior that finished its flee simply charged back under the
cast and died.

`MimironShockBlastAction` therefore returns **true** for the whole window, not just on the ticks where
it issued a move. It used to return false in two places: when the bot was already clear (so the flee
distance came out non-positive) and while a leg was in flight (the `MOVEMENT_FORCED` lock refuses a
second `MOVEMENT_FORCED` move, every bearing fails, and the `MoveAway` fallback issues at
`MOVEMENT_COMBAT` and is refused too). Once clear, only melee keep holding — they own the gap-closers
and have nothing to do at 18 yd anyway — while ranged and healers go back to work rather than lose
4 s in every 30. `MimironChargeGuardMultiplier` is the backstop: it zeroes every
`CastReachTargetSpellAction` (Charge, Intercept, both Feral Charges — the class has no other
subclasses) inside the five windows a hit actually kills through. Proximity Mines and Bomb Bots are
deliberately **not** among them; the mine node was demoted below the whole ladder because eating one
is healable, and letting it veto a charge would contradict its own ranking.

**`GetDistance2d` versus `GetExactDist2d`, again.** `WorldObject::GetDistance2d(WorldObject*)`
subtracts *both* combat reaches, and the MK II's is 8. `20.0f - GetDistance2d(mk2)` therefore fled to
**29.5 yd centre to centre** against a 15 yd radius, and the trigger's `GetDistance2d(boss) < 15`
fired out to 24.5. Both now measure centre to centre against
`ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST` (18), which also puts the whole 22 yd ranged ring permanently
outside the mechanic. This is the second defect of this exact shape in this encounter — the barrage
radius was the first.

### Mimiron — the Magnetic Core needs a carrier that walks

The core mechanic never once fired in testing, for two independent reasons. The trigger required an
Assault Bot corpse **already within 5 yd** and the action had no travel branch, so unless the carrier
happened to be standing on a corpse it returned false forever. And the carrier came from
`IsMechanicTrackerBot`, which is the first alive bot in group order regardless of role — usually a
ranged bot pinned 22 yd out, which is never within loot range of anything.

`GetMimironCoreCarrier` picks the first alive non-tank melee bot in group order instead (still group
order, so every bot agrees without coordinating), the corpse search widened to 60 yd, and the action
walks to the corpse before looting it. Corpses last 25 s
(`TEMPSUMMON_CORPSE_TIMED_DESPAWN`), which is the entire window.

**What the core buys is the only killable window in the phase, and nothing was using it.** Aura 64436
runs **20 s**, carries `MOD_DAMAGE_PERCENT_TAKEN` at base 49 (**+50 % damage taken**), and its
`OnApply` runs `DO_DISABLE_AERIAL`: `CastStop`, `AttackStop`, `REACT_PASSIVE`, hover cleared,
`MoveFall`, and `_events.DelayEvents(25s)`. The unit's own `UpdateAI` returns early for the whole
aura, so **no adds spawn during it**. It carries neither `NOT_SELECTABLE` nor `NON_ATTACKABLE`, so it
is an ordinary target.

Melee and pets switch to it for the window — `IsAllowedTarget` used to refuse melee the Aerial Command
Unit outside phase 4 unconditionally, and the pet node only ever looked for adds, so both sat it out.
Ranged keep the add order and arrive on their own once the leftovers are dead, since nothing replaces
them. Tanks never reach this: `MimironSetDpsPriorityTrigger` stands down for them, so the Assault Bot
keeps its tank throughout, which is deliberate — it is the one add nobody can ignore, and a tank
contributes little of the burn.

The core grounds the unit **wherever it happens to be**, which is usually across the room, because
64444 places its summon by nearest entry and the carrier has to stand under it. Waiting for it to
drift toward the raid was considered and rejected: the corpse the core is looted from lasts 25 s, the
unit's position is driven by threat and not by anything the raid steers, and losing a core outright
costs far more than melee jogging 30 yd into a 20 s window.

### Mimiron — pets need telling twice, in two different phases

`PetAttackAction`'s trigger node is commented out globally (`CombatStrategy.cpp:53-55`), so a pet keeps
whatever it last latched onto and no encounter redirects it unless it says so. Any fight with an
unreachable boss or a hard target switch has to do it by hand.

In phase 3 the Aerial Command Unit hovers 15 yd up and every ground pet parks underneath it
contributing nothing. `mimiron pet control` sends them to the nearest Assault Bot, then Junk Bot, then
Bomb Bot, and calls `StopPet` when no add is up; the Assault Bot comes first because it is the only
Magnetic Core source. Its action always returns **false** — it redirects the pet without consuming the
bot's own tick. Once a Magnetic Core lands, the unit itself outranks all of them: it is on the floor,
stationary and taking +50 % for 20 s, and nothing new spawns during it.

In phase 4 pets go where the melee go, and hold when the melee hold. A hunter's pet is about a fifth of
that hunter's damage, and leaking that into the rendezvous is exactly what the floor exists to prevent.
Note this is **not** a reachability workaround: the Aerial Command Unit is at ground level in phase 4
(see below), and pets are kept on the chassis parts to hold the ranged/melee split.

**`MOVEMENTFLAG_HOVER` cannot be used to mean "phase 3".** Nothing in the vehicle entry path clears it
and the phase 3 defeat branch does not either, so it survives into phase 4 whenever the ACU happened to
be airborne when it went down — a coin flip. Keying the node off it made it fire in phase 4, find no
adds, and call `StopPet`: every pet in the raid stopped for the whole phase. Both branches are gated on
the phase itself now.

### Mimiron — phase 4 is a rendezvous, and percent is the only correct ordering

`EVENT_FINISH` fires only when **all three parts are casting Self Repair at once**, and Self Repair
(64383) has casting-time index 192 = **15000 ms**. That 15 s is the real mechanic: a part drops out at
`health < 15000` (damage zeroed, `UNIT_FLAG_NON_ATTACKABLE`, Self Repair cast), and if the other two
are not down before the cast completes, `SpellHit` brings it back aggressive and the phase resets.

So the three have to come down **level**, and the old ordering could not do it. It sorted on
`GetHealth()` descending, but `HealthModifier` is 300 / 300 / **200** — the Aerial Command Unit is the
smallest absolute pool in the room and so ranked last on every tick, whatever its actual percentage.
Ordering is on `GetHealthPct()` now.

**All three occupy the same point server-side.** VehicleSeat 3886 (VX-001 on the chassis) and 3806 (the
ACU on VX-001) both carry `AttachmentOffsetX/Y/Z = 0,0,0`, `Vehicle::AddPassenger` relocates by exactly
those offsets, and `RelocatePassengers` rewrites passengers to `vehiclePos + offset` on every chassis
move without re-applying hover height. The visual stack is a client-side model attachment. Two
consequences: melee cleave splashes every part, which is what the **10 % floor** is for; and the Aerial
Command Unit is genuinely reachable by melee in phase 4.

Melee are kept off it anyway, by choice. Ranged DPS own the ACU — `IsRangedDps`, not `IsRanged`, so a
healer is never steered onto it or into the hold — and with ranged at roughly half the raid against the
ACU's third of the health pool, that split lands close to even on its own. **Everyone holds at 10 %**
once nothing they are allowed to touch is above the floor: melee, pets, and both tanks. Tanks holding
is only safe because nothing else is generating threat by then, so threat is static and no mech changes
hands; it is the first thing to revisit if one ever does.

**Every restriction lifts the moment a part starts self-repairing.** `IsMimironPhase4` is keyed on
VX-001 riding the chassis, not on all three being attackable, precisely so it stays true through that
window — the phase is at its most time-critical there, not over. Melee join the ACU, the floor is gone,
and the raid pushes whatever is left.

`IsMimironPhase4` answers from cached lookups only: VX-001 or the ACU riding something covers all of
the phase bar the last seconds, and when only the chassis is left, seat 3 holds the cannon in phase 1
and VX-001 from phase 4 on. It is asked several times per bot per tick, so a grid sweep there would
cost the whole raid every phase.

### Mimiron — a taunt during the cast is always one cast too late

The phase 1 tank swap never happened, for three reasons at once.

**The Leviathan MK II is fully tauntable**, so that was never it: `flags_extra` 524289 is
`OBEYS_TAUNT_DIMINISHING_RETURNS | INSTANCE_BIND` with no `CREATURE_FLAG_EXTRA_NO_TAUNT`, and
`CreatureImmunitiesId` -361 masks 21 mechanics and effects 98/124/144/145 — knockback and pull, not
taunt. DR resets after 15 s against a 22 s cadence, so every taunt lands at full duration.

1. **The trigger fired during the cast.** It required
   `cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST)`, but the cannon casts at
   `me->GetVictim()` resolved **when the cast began**. Nothing that happens during those 3 s moves that
   cast. It now fires in the gaps instead, so the taunt owns the next one ~19 s out.
2. **Three seconds of taunt cannot hold a 22 s rotation.** What makes the swap stick is that taunt sets
   the taunter's threat equal to the current highest — the new tank then holds it by continuing to
   swing, which only works if it took the boss well before the cast.
3. **The raid was actively undoing it.** `CastMisdirectionOnMainTankAction` and
   `TricksOfTheTradeOnMainTankTrigger` both target the *main tank* by name, so every hunter and rogue
   was transferring threat onto exactly the tank being swapped off.
   `MimironThreatRedirectGuardMultiplier` zeroes both in phase 1 only, matching on the action name
   (`BuffOnMainTankAction::getName()` returns `"<spell> on main tank"`) rather than the type, which
   would also catch every blessing aimed at the tank. Redirecting them to the MK II's *current* victim
   would be better still and belongs in its own change.

Plasma Blast's real numbers, for reference: 62997 is a 3 s cast, `TARGET_UNIT_TARGET_ENEMY`, aura 3 at
1000 ms period, base 16999 + 1 → **17000/s for 6 s = 102000**, no stacking, every 22 s. The debuff
expires 16 s before the next one, so this is a heal-check swap, not a stacking-debuff swap — and there
is nothing to test for at swap time, which is why the alternation is unconditional.

### Mimiron — the phase handovers are a minute of wasted time

Measured from the boss script: **47.75 s** from phase 1 to 2 (retreat 5 → elevator 6 → VX-001 summoned
6 → 18 → 4 → 5 → 2 → 1.75), **24 s** to phase 3, **31.8 s** to phase 4. Nearly two minutes a pull.

A defeated mech sets `UNIT_FLAG_NOT_SELECTABLE` and stays in the world — the MK II parks 58 yd off
centre for phases 2 and 3 — and the next mech carries the same flag until its phase starts.
`AttackersValue::IsPossibleTarget` rejects that flag, so `GetFirstAliveUnitByEntry` is blind for the
whole handover and every Mimiron node stands down. With no trigger-driven action succeeding, the engine
falls through to its default action, and with follow enabled that is `follow` at relevance **1.0** — so
the raid spends every handover trailing its master and then walks into the next phase from wherever
that left it.

Two things make the fix nearly free. `Creature::FindNearestCreature` is a grid check on entry, alive
state and range with **no selectability filter**, so bots can see the mechs the target list cannot. And
instance strategies are added to **both** `BOT_STATE_COMBAT` and `BOT_STATE_NON_COMBAT`
(`PlayerbotAI.cpp:1793-1794`), so `ACTION_RAID` nodes already run out of combat — 60 clears `follow` at
1.0 and `drink`/`food` at 3.0–4.2 without any ordering work.

`GetMimironStagingFocus` resolves VX-001-riding-the-chassis → phase 4 shape, else the ACU → phase 3
wedge, else VX-001 → phase 2 ring, and the existing slot generators do the rest. Melee and tanks get a
slot **only while staging**: there is no chase for it to fight yet, and being in range when the boss
goes live is the whole point.

**The staging anchor is the room centre, never the focus.** All three handovers converge there —
VX-001 is summoned at it, `ACUSummonPos` is (2744.650, 2569.460, 380.0), a defeated ACU is walked back
to (2744.65, 2569.46, 381.34), the chassis ends there, and `ULDUAR_MIMIRON_PHASE4_TANK_SPOT` is 1.4 yd
off it. But the focus is mid-script for most of the window: in the phase 3→4 handover the chassis
charges to (2755.77, 2574.95) at 10 s and only reaches the centre at 18.8 s, so a ring pinned to it
walks the melee along the charge waypoints and back. The ring radius is `max(8, focus reach + 1)`,
since a flat 8 yd would stage half the melee inside the chassis model at reach 8.

Three things fall out rather than needing code. The **elevator knockback** 11 s into the first handover
needs no guard, because VX-001 is not summoned until 17 s and it is what the staging keys off. **Eating
and drinking still happen**, because the trigger stops firing inside `ULDUAR_MIMIRON_SPREAD_TOLERANCE`
and the bot yields the tick once it arrives. And there is **nothing to do before a pull or after a
wipe**: the MK II is `NOT_SELECTABLE` until pulled, and evade despawns VX-001 and the ACU outright.

This does deliberately override follow for the handover. A master who wants the raid moved between
phases will find it walking back to formation — the same trade every live phase already makes.

### Mimiron — the ring slot is what kills the Rocket Strike dodge

Rocket Strike target selection **removes every player within 15 yd first** and picks from what is
left, so it lands on the ranged ring by design. The marker (34047) burns a **5 s fuse**, blasts 3 yd
and despawns at 6 s.

That makes fleeing the easy half. The hard half is not walking back: once the bot has stepped clear,
the arc spread sees it off its slot and returns it — to the slot the marker is still sitting on.
`IsMimironSpotSafe` therefore tests the **slot**, not the bot, and holds while any live marker is
within 8 yd of it. Testing the bot's own surroundings is exactly the check that fails, because a bot
that dodged successfully is by definition clear.

Second, quieter trap: the flee and the arc spread both issued at `MOVEMENT_COMBAT`, and
`IsWaitingForLastMove` only lets a move through when its priority is **strictly** above the one in
flight. A dodge starting mid-walk was dropped with no trace. Shock Blast and Rocket Strike now issue
at `MOVEMENT_FORCED`; the low-stakes avoids (mines, bomb bots, flames, Frost Bomb) stay at
`MOVEMENT_COMBAT` so they cannot stomp a real emergency.

**Two `MOVEMENT_FORCED` dodges in one encounter deadlock each other**, and there is no band above
`MOVEMENT_FORCED` to escape into. The barrage dodge returns `false` for a bot that is already clear, so
Rocket Strike and Shock Blast get the tick and flee on a bearing derived purely from the hazard they
are escaping. That leg then holds the lock — about 1.4 s for a 10 yd rocket step, 2.6 s for an 18 yd
Shock Blast flee, against `MaxWaitForMove` of 5000 — and *strictly above* means the barrage dodge's own
`MOVEMENT_FORCED` move is refused for its whole duration. The barrage action ignores `MoveTo`'s return
value and keeps holding the tick, so the bot stands in the beams at 20000 per 250 ms until the lock
expires.

The fix is to make the lower dodge cone-aware rather than to try to outrank it. `MoveAwayClearOfMines`
now rejects any bearing whose destination is inside the swept union, and it asks that question **for
the moment the leg lands**, not for the moment it is issued: the cone turns ~10.6 °/s, so a 2.6 s leg
outruns the 15° margin and a spot that clears by exactly the margin is inside the beams on arrival. The
fan also widened from ±90° to ±112.5° — two stacked filters can empty the first quadrant — with a
guard that the destination must be strictly further from the hazard than the bot already is, because
past roughly 120° off the escape bearing the geometry turns back inward.

`IsMimironSpotSafe` covers Firefighter's ground fire on the same argument, behind the hard-mode check:
without it the flames node at `ACTION_RAID + 4` pushes a bot out of a burning slot and the formation at
`ACTION_RAID` pulls it straight back, and it paces on the edge until it burns down.

### Yogg-Saron — Squeeze breaks on immunity

Removing the Squeeze aura (64125 / 64126) kills the Constrictor Tentacle and drops the passenger, so
`yogg-saron squeeze escape` at `ACTION_RAID + 1` has a grabbed mage cast Ice Block and a paladin cast
Divine Shield. Hunter Feign Death and rogue Vanish are deliberately not used — neither removes a
periodic damage aura.

## Burst and Bloodlust windows

`UlduarBurstWindowMultiplier` is always active inside Ulduar — no config key, matching every other
raid. Two tiers are gated separately: `allowAll` covers every burst cooldown, `allowLust` covers
`bloodlust`/`heroism` only, because a 10-minute raid cooldown wants a later window than personal
cooldowns that come back within a phase.

Burst only ever fires while the bot's current target is boss-flagged — `IsDungeonBoss() ||
isWorldBoss()` in `HoldBurstUntilTankEngagedMultiplier`, under `AiPlayerbot.BurstOnBossOnly` (default
on). So **adds-only phases need no gate of their own**, and conversely **no `allowAll` rule can open
burst on an add**: that veto is final. Freya's wave adds are all `flags_extra = 0`, `rank = 1`. There
is no Sated/Exhaustion check anywhere, and no Drums or Time Warp — lust is shaman-only.

| Boss | `allowAll` | `allowLust` | Why |
|---|---|---|---|
| Razorscale | grounded (`Z <= 440`) | same | Zero damage taken while airborne; every landing, harpoon knockdowns included, is a real burn window |
| Mimiron | always | all three mechs alive | P1-P3 damage counts; all three up is P4, the enrage burn |
| Yogg-Saron | P2 or P3 | P3 | P1 damage lands on Sara and is wasted |
| Assembly of Iron | always | exactly one member alive | They resurrect each other; also covers the hard mode, since Steelbreaker-last means the survivor is empowered |
| Freya | same as `allowLust` | no `SPELL_ATTUNED_TO_NATURE` 62519, **or** HP ≤ 25% | 150 stacks of +8% healing received, so damage lands only in the final phase; the adds that strip it are not boss-flagged |
| Thorim | arena floor (`Z <= 429.6`) | same | Largely redundant, but cheap insurance against a stray lust while he is immune |
| Hodir, Vezax, Algalon, Ignis, Auriaya, Kologarn, Flame Leviathan | — | — | No change; the pull is the right window |

**Freya is the only boss with a fallback release** — `FREYA_LUST_FALLBACK_PCT = 25.0f` keeps lust
from being held forever if the aura read ever misses. Every other window is on the mandatory path to
the kill.

**Never call `RazorscaleBossHelper::UpdateBossAI()` from a multiplier** — it side-effects into
`AssignRolesBasedOnHealth()`, which reassigns the raid's main tank. Read Z straight off the target
sweep instead. Yogg is resolved with `FindNearestCreature`, not `"find target"`, because he is not
reliably on a bot's threat list.

**The `"possible targets no los"` sweep is capped at `AiPlayerbot.SightDistance` (100 yd)**, and
Razorscale's second flight point `RazorFlightPos2` (619.1, -238.1, 475.2) sits past that from most of
the raid. `EvaluateWindow` therefore falls back to `"find target"` for her — `DoZoneInCombat()` on her
first flight point puts the whole raid on her threat list, so that read works at any range. Without
it the function reaches its `return {}`, and `BurstWindow`'s member initialisers are both `true`, so
the gate opens instead of closing.

Open question that cannot be answered from source: whether the Mimiron mechs and the Assembly council
members are `IsDungeonBoss()`-flagged. If they are not, lust never fires on them and those two gates
are inert.

## Threat redirect

Before this work **no WotLK raid had either a redirect action or a veto**, so the generic main-tank
node fired unconditionally — including on the encounters where the strategy deliberately puts a
second tank, an add tank or a swap tank on what the DPS is hitting. This is the veto slice only; no
Ulduar boss has a dedicated redirect action yet.

**Veto — the main tank is actively wrong:**

| Boss | Why | Gated on |
|---|---|---|
| Iron Assembly | Three bosses, one tank each, plus a Fusion Punch swap | boss entries |
| Mimiron | Four phases, each a different creature with a fresh threat table; phase 3 has two tankable units split MT/AT0 | 33432 / 33651 / 33670 |
| Thorim | Raid splits into arena and gauntlet squads with a tank each, plus an Unbalancing Strike swap | boss present |
| Algalon | Phase Punch forces an MT ↔ AT0 swap on a stack timer | boss present |
| Razorscale — **airborne only** | The MT holds nothing while she flies; Dark Rune adds belong to assist tanks. Ground phases are single-tank and the generic node is *correct*, so this is phase-gated, not blanket | boss Z vs 440 |
| Freya | The add tank is the sink whenever the Snaplasher or Conservator is up, and `freya redirect threat` owns the choice | boss present |

**Do not veto** — the boss is main-tank-held all fight, so the generic node is right for the primary
target and the only gain would be redirecting *adds*: Auriaya, Kologarn, Yogg-Saron, Ignis.
**No** — Vezax (one tank, no swap, no reset), Flame Leviathan (vehicle
combat, neither spell castable).

XT-002 was already covered by `XT002TargetGuardMultiplier`, which vetoes for the whole encounter
because the main tank is the wrong sink while a Pummeller is out.

Two load-bearing details: the multiplier `dynamic_cast`s to the two **concrete** redirect actions
only, never the shared `BuffOnMainTankAction` base; and **Razorscale's phase is read straight off the
unit's Z** rather than through `IsFlyingPhase()`, because the helper needs `UpdateBossAI()` first and
that reassigns the main tank. The temporary harpoon knockdowns count as grounded, which is intended —
she is tankable then, and a redirect during the knockdown is what puts her back on the MT.

**No pull-window helper exists outside Naxx, RS and SWP.** If dedicated actions follow, use BT's
stateless `boss->GetHealthPct() > 95.0f` idiom rather than building a combat clock — it needs no new
state and doubles as the fresh-spawn / fresh-phase test. And use `GetFirstAliveUnitByEntry`, not
`"find target"`, for multi-tank detection: a bot parked on boss A never resolves boss B.

`RaidRedirectThreatAction` lives in `Raid/RaidRedirectThreat.{h,cpp}`; Hodir subclasses it as
`HodirRedirectThreatAction`, feeding whichever tank currently holds him. The proc-aura id `35079`
still sits in nine per-raid helper headers including `UldBossHelper.h`; `SPELL_MISDIRECTION_PROC` on
the shared header is the one to converge on.

## Normal-mode gaps still open

From the Sev-1/Sev-2 audit. Sev-1 fails **even with the raid cheat on**:

| Boss | Gap |
|---|---|
| **Razorscale** | Dark Rune Watcher/Guardian adds have no interrupt (focus and the Flame Breath cone are handled) |
| **Freya** | Storm Lasher (Stormbolt, Lightning Lash) and Ancient Water Spirit (Tidal Wave) casts are not interrupted |
| **Auriaya** | `AuriayaEncounterActive` is presence-only (`GetAuriaya(botAI) != nullptr`), so bots pull her on sight from up to 100 yd. Kologarn had the same defect and now gates on `IsInCombat()` |

**Sev-2 CHEAT-ONLY** — works in the default config, breaks silently if `BotCheats` drops `raid`:
Yogg Ominous Clouds, Crusher/Constrictor tentacles, illusion-room adds and P2 movement
(cheat instakill / teleport).

Structural notes: **no boss reuses `RazorscaleBossHelper`'s role-swap machinery for a real tank
swap** — each rolls its own detector instead: Thorim on the Unbalancing Strike debuff, Kologarn on
Crunch Armor stacks, Hodir on Frozen Blows. There is no enrage-timer awareness anywhere, which
blocks every hard-mode kill-timer requirement.
