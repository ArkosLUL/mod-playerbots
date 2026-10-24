# Eye of Eternity — Malygos (map 616)

Strategy key `wotlk-eoe`, all of it in `Definition/EoEDefinition_Malygos.cpp`; ids and tuning
constants are in `Util/EoEData.h`. Cross-raid conventions are in [README.md](README.md).

## Settled — do not re-audit

Verified against `boss_malygos.cpp`:

- **No "Deep Breath" spell exists in this core.** `SAY_DEEP_BREATH` is only the P2 Surge windup
  yell, and `56430` is Arcane Overload. A Deep Breath dodge built on it was deleted: the bubbles are
  shelter, not a hazard (see P2).
- **The P2 floor is not gone**: the platform is destroyed only at P3 start
  (`EVENT_DESTROY_PLATFORM_0`). P2 anti-fall is a recenter on drift; the real P2 hazards are Arcane
  Overload and the Surge of Power beam.
- Ids: Static Field `57430` → `NPC_STATIC_FIELD 30592`, a tick is 9,425 in 30 yd; Arcane Storm
  `61693` (random target, unavoidable → heal through); Surge P2 `56505` → `NPC_SURGE_OF_POWER 30334`;
  Surge P3 `57407` (10) / `60936` (25); Arcane Pulse `57432` (self-cast every 3 s, **30 yd** around
  the boss, ~28k arcane: a positioning hazard, not raid-wide damage); Vortex `56105` (server-driven,
  no bot action). Centre anchor `{754.395, 1301.27, 266.1}`.
- **Vortex cannot be broken by immunity.** `VortexHandling()` (`instance_eye_of_eternity.cpp`) makes
  the **player** cast `SPELL_VORTEX_4` (55853) on a Vortex trigger with `TRIGGERED_FULL_MASK`, whose
  `TRIGGERED_IGNORE_CASTER_AURAS` skips `CheckCasterAuras` in `Spell::CheckCast`. And
  `AuraEffect::HandleAuraControlVehicle` puts the aura on the **trigger**, pulling the caster in, so
  there is no player-side aura for Divine Shield to purge. Breaking it needs a core change. (The
  rogue tool would have been Cloak of Shadows, not Shadowstep.)
- **P1 suppresses no heal or buff cast.** The P1 rules block no `CastSpellAction` but the Blink,
  Disengage and Taunt families and, for everyone but his victim, Charge. "Disc priests never shield on
  Malygos" was a generic priest bug ([../classes/priest.md](../classes/priest.md)). P1 leaves healers
  targetless; only `raid healer engine`
  ([README.md](README.md#targeting-the-whole-fight)) keeps them on the combat engine, where the
  shield lives.

## Phase model

P1 = Malygos attackable and no adds. P2 = disc adds (`NPC_NEXUS_LORD` / `NPC_SCION_OF_ETERNITY`)
up. P3 = on a Wyrmrest Skytalon (30161). **P4 = transition** (`MalygosPhase::Transition`): in
combat, Malygos non-attackable, not mounted.

P4 exists because during P2→P3 the boss is untargetable and the bot not yet on its drake: with no
phase every override lifted and **the default raid strategy resumed on a collapsing platform**. An
`Exclusive` rule holds the raid at centre on `malygos position`. P4 also catches the **pull intro**,
which wants the P1 hold spots instead (below); full health separates the intro from the two real
transitions, both at 50%.

`GetMalygos` falls back to `FindNearestCreature` so detection survives the non-attackable flag.

## No navmesh, no vmaps, no terrain

**Map 616 ships no `.mmtile` files** — only the 28-byte `616.mmap` header, which claims
`maxTiles = 25`, the only one of the 98 client maps like this. It ships **no vmaps** (no
`616.vmtree`, no `616*.vmtile`), and its 25 `.map` tiles are 616 bytes of header carrying `MHGT`
flags `0x09` (`MAP_HEIGHT_NO_HEIGHT | MAP_HEIGHT_HAS_FLIGHT_BOUNDS`) with `gridHeight = 0.0`. There
is no collision geometry here, and navprobe cannot answer this map.

What that does — every path is the `0x11` shortcut, `generatePath` is inert, the off-navmesh failure
cannot happen, path types must be masked — is in
[../engine/pitfalls.md](../engine/pitfalls.md#movement-that-silently-no-ops) and
[../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md#vehicles-and-flight).
Specific to 616: `Map::GetHeight` answers 0.0 everywhere while the platform sits near Z 266, so
`UpdateAllowedPositionZ` pins any **non-flying** unit to Z 0, a 266 yd drop; its flying branch only
ever raises Z. A drake rider counts as non-flying, so after the kill `move to loot` and `follow` dove
the flight to Z 0. The gate therefore stays open for Skytalon riders past the kill
(`MalygosGateOpen`).

## P1 — Power Sparks

Sparks reaching Malygos stack a damage + haste buff (56152) into a soft enrage. DK
`pull power spark` at `ACTION_RAID + 3`, DPS `kill power spark` at +2, and the same pick drives the
switch in `malygos target`. A spark spawns at one of the four `FourSidesPos` corners, 94–107 yd from
centre, walks straight at Malygos at 6.0 yd/s (re-issuing `MovePoint(0, *malygos)` every 2 s), and
hands over its buff at 12 yd, centre to centre.

**Reach decides who shoots it.** Nobody walks in P1, so `GetPowerSparkToKill` only returns a spark
the bot can hit standing still — `spellDistance` for ranged, melee range plus
`POWER_SPARK_MELEE_STICKY` for everyone else — and of those the one nearest Malygos, about to hand
over. Without that gate bots locked onto a spark 100 yd away and stood there while the boss went
unhit. Melee DPS take a spark that walks into them; the tank never switches, because dropping
Malygos swings the Arcane Breath cone into whoever is behind him.

**A killed spark stays `IsAlive()` for 60 s.** `npc_power_spark::DamageTaken` zeroes the damage,
sets `UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE`, self-casts
`SPELL_POWER_SPARK_GROUND_BUFF` (55852) and despawns after 60 s. Unfiltered, the corpse kept the
trigger firing, could mask a live spark in `GetPowerSparkToKill` (it returns one, the nearest to
Malygos) and drew Chains of Ice. Every spark consumer goes through `GetLivePowerSparks`, which drops
those flags. **The filter stays spark-specific**: Arcane Overload, Static Field, Surge of Power and
the hover disks are `UNIT_FLAG_NOT_SELECTABLE` from spawn, so the same test in the creature cache
would blind the bubble seek, the surge dodge and the disk boarding.

55852 is a 60 s periodic-trigger aura firing **55849** once a second, which carries the payload:
`EffectRadiusIndex 14` = **8 yd**, aura 79 `MOD_DAMAGE_PERCENT_DONE`, misc 127, **+50% damage done**
to allies in that radius.

**The DK walks out to grip, then back.** Death Grip lands the spark *on the caster*, so where he
stands decides who gets the corpse's buff and whether the spark lands inside the 12 yd handover.
`POWER_SPARK_GRIP_OFFSET` **+4.5 yd** sits 18.7 yd from where Malygos parks: a dropped spark still has
6.7 yd to walk against ~12k hp and the whole raid, and its corpse pool lands 7.5 yd from the melee
stack, inside the 8 yd. The window is 1.5 yd wide and both bounds are hard. Death Grip (49576,
`RangeIndex` 4) reaches 30 yd, ≈ **31.5** once `IsWithinCombatRange` adds both reaches, and a spark on
his bearing is spent at r ≈ 35.2, so below **+3.7** none can be caught. Above **+5.2** the spot is
closer to him than `POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE` (18) and duty never starts. At +4.5 the grip
needs 30.7 of its 31.5. The old spot, midway between stack and hunters (−1 yd), was 13 yd from both,
so the buff landed on nobody, and 37 yd from a spark walking in on his bearing, so one spawn in four
could never be gripped.

**No range gate on the grip.** `CanCastSpell` passes `SPELL_FAILED_OUT_OF_RANGE` as castable
([../engine/action-selection.md](../engine/action-selection.md#casting)), so a DK out of reach still
builds a Death Grip and throws it away. The choice is bounded by `GetNearestPowerSparkTo`, anchored
on the grip spot rather than the bot: asking for the spark nearest the DK ignored one beside his spot
whenever another was marginally closer to him.

`MalygosPositionAction` owns all the walking and `PullPowerSparkAction` only casts; both read
`IsOnPowerSparkGripDuty`, so they cannot disagree. Duty needs the grip off cooldown (the cooldown
outlasts the gap between spawns, so giving up boss uptime otherwise buys nothing), a spark within
`POWER_SPARK_GRIP_ENGAGE_RADIUS` (45 yd) of the spot, and Malygos no closer to the spot than
`POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE`. Casting the grip ends duty, and the position action walks the
DK back to the stack next tick.

**The grip is followed by Chains of Ice**, since the spark walks the pulled distance back off at
6 yd/s. `GetPowerSparkToSnare` takes the nearest unchained spark inside `POWER_SPARK_SNARE_RADIUS`
(15 yd, about where a gripped one lands). Power Spark's immunity mask (`creature_immunities` −335,
mechanics `0x26CB031D`) carries neither root nor snare. The aura check is caster-agnostic: a second
DK re-snaring spends a rune for nothing. The action keeps the grip first whenever it is available
and stays useful on the snare alone, so a DK whose grip is on cooldown chains a spark walking past
the stack.

## P1 — layout

**The hold spots are taken during the pull intro.** `JustEngagedWith` sends Malygos on an intro
circuit and only then drops him at `CenterPos.z`, 35 yd from centre on whatever heading he was
circling; `EVENT_START_FIGHT` clears his flags and he chases `SelectNearestTarget(250)`. That stretch
reads as P4, whose centre gather pulled the tank off his spot (46 yd out, outside the 30 yd ring), so
`MalygosPositionAction` runs the P1 branch through the intro and the tank is parked and facing when
the boss touches down. During the intro only the assigned main tank counts as the tank: Malygos is
pacified, and his victim is just whoever pulled.

**The spots rotate onto the bearing he landed on.** Arcane Breath (56272) is a frontal cone on his
current victim, and he does not always land north. `EVENT_INTRO_MOVE_CENTER` snapshots
`CenterPos.GetAngle(me)` the instant `JustEngagedWith` fires, flies him in along it to 35 yd out, and
`EVENT_INTRO_LAND` drops him straight down. He idles between the four `FourSidesPos` corners, so
there are four answers: **−135.95°, +46.51°, +134.45°, −44.60°** (`MALYGOS_LANDING_ANGLES`), the
bearings of `{686.417, 1235.52}`, `{828.182, 1379.05}`, `{681.278, 1375.796}` and
`{821.182, 1235.42}`. The layout is one set of signed offsets from centre, positive towards him,
rotated onto whichever of those is nearest to where he is:

- `MALYGOS_MAINTANK_OFFSET` **+46 yd**. His CombatReach of 20 parks him ~22.8 yd short of the tank,
  ~23.2 yd out. The floor is **stepped**: **266.10 out to r 29**, **267.25 from 30.5 to 47.5**,
  **268.25 from 48.1 to 55.5**, the last being the real edge — measured off
  `Nexus_Raid_Floating_Platform.wmo.vmo` in the `ac-client-data` volume (`GMOD`/`VERT` chunks, plus
  the GO's 256.25 spawn Z). The GO sits 0.4 yd off `CenterPos`, so those radii hold about centre
  either way, and the `Exit Portal` GO at r 43.41, z 267.23 confirms the middle tier (phased out by
  `DATA_HIDE_IRIS_AND_PORTAL` once the fight starts). Ceilings: the step at **47.5**, and **49.8**,
  past which the boss clears `MALYGOS_MELEE_HOLD_DISTANCE` from the stack and the clamp starts firing.
- `MALYGOS_STACK_OFFSET` **+12 yd**: melee, healers and every ranged DPS but the hunters. 34 yd from
  the tank, inside `HealDistance` (38.5), and behind where the boss stops, so out of the cone. His
  chase stops wherever it first puts him in melee range, so an off-bearing approach can leave melee
  swinging at nothing; the stack spot, and only it, is **clamped**: further than
  `MALYGOS_MELEE_HOLD_DISTANCE` (15 yd) from him, it slides up the line towards him. Melee range
  against him is ~22.8 yd and a bot may park 5 yd off its spot, so 15 swings with margin. The clamp
  keeps the bearing and moves continuously
  ([../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md#movement-that-oscillates));
  it is off during the intro, and with the layout rotated onto him it rarely fires.
- `MALYGOS_HUNTER_OFFSET` **−14 yd**, past centre, ~37 yd from the boss — **hunters only**, because
  his reach inflates their 5 yd minimum to ~28 yd
  ([range and reach](../engine/raid-mechanics-lessons.md#range-and-reach)). Casters hold the stack:
  from out here a spark closing from the far side was past `spellDistance` (37 yd plus 12).
- `POWER_SPARK_GRIP_OFFSET` **+4.5 yd**, a DK on spark duty only.

His reach also keeps `EnemyTooCloseForSpellTrigger` (~23.5 yd) active near him, and its class
escapes outrank `malygos position` at `ACTION_MOVE`, so a P1 rule blocks the Flee, RunAway, Blink and
Disengage families for everyone.

`GetMalygosP1Layout` resolves the set once and **latches it for the pull**, per instance, so the raid
cannot split across two sets while he walks; it clears when the encounter drops out of combat. Spots
are never recomputed from his live position: a chasing spot flipped to his far side while he walked
out, and the tank ping-ponged, sweeping the cone through the raid. Whoever Malygos is hitting
behaves as the tank, assigned or not, and a bot corrects only past `MALYGOS_P1_POSITION_TOLERANCE`
(5 yd).

**Nobody but his current victim walks in P1.** For everyone else an `OwnMovement` rule passes only
`malygos position`, `malygos target`, `malygos kill power spark` and `ReachHeal`, and a `Block`
takes Charge. One cause, two symptoms: ranged chased sparks back inside his minimum range (the DK
grips sparks to them instead), and melee walked out behind him (`set behind`, `ACTION_MOVE + 7`) or
spread (`combat formation move`) and were dragged back next tick. The spots need no help: inside his
20 yd reach for melee, outside the inflated minimum for hunters. `SetFacingTargetAction` is a plain
`Action`, so facing still works; `AttackAction` is a `MovementAction`, so the EoE attack actions are
named. They only *target* — `AttackAction::Attack` stops nothing but a sub-combat-priority walk — so
a melee bot peeling onto a spark stays put.

**Only the assigned tank pulls him back.** `MalygosTargetAction` puts an off-tank on the boss (the
spark peel is gated on `IsDps`, which a tank spec fails) and parks him on the stack, so a taunt from
there swings the cone through the raid. `lose aggro` → `taunt` / `hand of reckoning` /
`dark command` / `growl` was mostly quiet: `HasAggroValue` counts "another tank player holds him" as
aggro for anyone but the flagged main tank. `high aoe` → `challenging shout` / `challenging roar` was
**ungated**, with the off-tank only 12.5 yd out. A rule zeroes the Taunt family for anyone not
`IsMainTank`, **only while a tank player is his victim**: with nobody on him the taunt is the rescue.
A zeroed action still pushes its alternatives at 0.003 relevance, so the warrior's `heroic throw`
fallback survives — damage, not a taunt, beaten by everything at that relevance.

**Threat redirects.** The generic main-tank node is deliberately not vetoed: each phase wants a
different answer. Hunters' `low tank threat` → `misdirection on main tank` (`ACTION_HIGH + 7`)
lands, Misdirection (34477) reaching 100 yd against a 60 yd gap; it is spent for nothing whenever
their target is a Power Spark, on which the tank has 0 threat. Tricks of the Trade (57934) reaches
**20 yd** against 34 yd, so `TricksOfTheTradeTargetValue` (`RogueValues.cpp`) range-gates its
main-tank branch like its fallback; ungated, the cast was built and thrown away every tick.

There is no `avoid arcane breath` action: the cone points at the tank, and every other spot is
behind it.

## P2 — bubbles, adds, disks

**The Arcane Overload bubbles are shelter.** `NPC_ARCANE_OVERLOAD` (30282) grants **56438, −50%
damage taken**; the protected radius shrinks ~2% per tick over its 45 s life, so bots hug the centre
within 4 yd and ignore a bubble below `BUBBLE_MIN_USABLE_FACTOR` (35%) of its original radius. **The
model does not shrink** (the core declares 56435 and never casts it), so the only honest size source
is the tick count of the `creature_template_addon` aura applied at spawn (no aura means brand new).
The NPC is non-attackable, never in `"possible targets"`: read it from the EoE creature cache.

`malygos seek bubble` sits at `ACTION_EMERGENCY + 2`, above `avoid surge of power`, which is only the
fallback for a bot that cannot reach one: it steps off the line from Malygos through the surge focus
(`SURGE_BEAM_CLEAR_DISTANCE` to clear it, `SURGE_BEAM_SIDESTEP` across) and never fires for a disk
rider or a sheltered bot, who are covered already and would be walked out for the beam's whole 10 s.
The bubble is **latched by GUID** (the `SapphironFlightPositionAction` idiom) and spread by group
slot index, or bots hop between bubbles as they shrink — unless that slot's bubble is more than half
`BUBBLE_SEARCH_RADIUS` away, where survival beats spreading and the bot takes the nearest. Once the
bot holds 56438 the action returns false so the rotation runs, and a P2 rule blocks reach and follow
for ranged and healers on foot, heal reach excepted.

**Targets.** `MalygosTargetAction` accepts a Nexus Lord / Scion only inside `spellDistance` by
**`IsWithinCombatRange`**: the Scions hover 20–30 yd up, and a flat 2d distance claimed reach that
was not there. A held add is kept **by GUID** while it is alive, the right kind and in reach:
matching on entry welded bots to an add they could never reach, and re-picking every tick flipped two
equidistant Scions forever. Nexus Lords come first for everyone, ranged included — on the ground,
tankable, quicker to die — and Scions are the ranged fallback once no Lord is in reach.
`MalygosScionTankAssistMultiplier` zeroes `tank assist` onto a Scion. A P2 `Block` on `Flee` keeps
the generic flee from walking bots off the edge; the anti-fall recenter in `malygos position` bails
for anyone in a vehicle.

**Mages steal the Nexus Lords' Haste.** `npc_nexus_lord` self-casts `SPELL_HASTE` (57060) every
20–30 s. `malygos spellsteal` takes it off the nearest Lord inside `spellDistance` by
`IsWithinCombatRange`, so nobody leaves a bubble for it, and never touches the bot's target. It hangs
off the `malygos` trigger and gates itself on class and phase, so it costs nothing outside P2. The
generic `SpellstealTrigger` only looks at the current target, a Scion as often as not.

**Hover disks — melee only.** When a Nexus Lord dies the core lands its disk (30248), turns it
friendly and clears `UNIT_FLAG_NOT_SELECTABLE`, so *landed + selectable + free seat* means its rider
is dead. Melee DPS board it (`MalygosBoardDiskAction`, an `EnterVehicleAction` whose
`HandleSpellClick` is required: the mount is `npc_spellclick_spells` 61421) and ride to the Scions,
~30 yd out and +20 yd up, otherwise unreachable in melee. Passengers are immune to both Arcane
Overload and Surge of Power, which is why tanks, ranged and healers use bubbles instead.
`MalygosRideDiskAction` steers the vehicle's own `MotionMaster` with a `POINT_MOTION_TYPE`
anti-stutter guard and `generatePath = false`. The core floors a Scion disk's descent at
`CenterPos.z + 20` (`MI_POINT_SCION`), so a fixed target Z is safe. A rider dismounts only once the
disk is back at `MALYGOS_PLATFORM_Z`; stepping off at Scion altitude is a 20–30 yd drop. Ending the
ride is manual — Scions can die before the Nexus Lords and the core keeps the disks until both are
gone — and the descent is stamped straight over the approach rather than waiting for
`POINT_MOTION_TYPE` to clear, because the disk is still flying at the dead Scion.

**Nothing but the EoE actions moves a disk rider.** A P2 `Exclusive` passes only
`MalygosRideDiskAction`, `MalygosTargetAction` and `leave vehicle` (a manual override), and a `Block`
takes Charge: a rider's chase actions steer the *disk*, which dove ~25 yd every time it parked next
to a Scion and climbed back. `AttackAction` is a `MovementAction`, so the two EoE actions must be
named or riders board and sit there. **Open gap**: the dive's Z mechanism is unexplained.
`ReachCombatTo` passing its endpoint through `UpdateAllowedPositionZ` does not fit — on 616 that
answers 0.0 for a non-flier, a 266 yd drop, and is inert for a flier — and the lockout fixes it
either way.

**Pets go on Nexus Lords, always.** Scions hover out of any pet's reach, and the generic pet-attack
trigger is off globally, so a pet would chase its owner's airborne target forever.
`MalygosTargetAction` redirects with `CommandPetAttack` / `StopPet` (`EncounterHelpers`) at the top
of its P2 branch, before every early return, so a disk rider's ghoul is covered.

## P3 — the flight

**One stack point, owned by `EoEFlyDrakeAction` alone.** Malygos is pacified and immobile for the
whole phase (`MI_POINT_PH_3_FIGHT_POSITION`, then `UNIT_FLAG_DISABLE_MOVE`), so the park spot is a
fixed offset from him, `DRAKE_STACK_RADIUS` (45 yd) along `DRAKE_STACK_ANGLE`, which every drake
resolves alike. Its height is the constant `MALYGOS_P3_BOSS_Z`, **not** his live Z: he opens the
phase at `CenterPos.z + 70` and takes seconds to sink to `CenterPos.z − 5`, and a stack anchored on
him flew the flight up over the arena and rode him back down. `DRAKE_STACK_TOLERANCE` (10 yd) is
loose on purpose, so the flight does not pile onto one coordinate. Arrival is a straight 3d spline
(`generatePath = false`), then `MoveIdle`, `SetFacingToObject(boss)` and `return false`, handing the
tick to the rotation; everything a drake casts targets the boss or itself, so healers are pinned
too.

The action sits at `ACTION_EMERGENCY`, the only thing a P3 `Exclusive` lets steer a Skytalon, and
stands down once parked. With no boss to anchor on — forming up, or after the kill — the flight fans
out behind the raid leader's drake, `DRAKE_FORMUP_RADIUS` out over three quarters of a circle with a
90° frontal cone clear; a leader with no drake is followed by nobody. `MoveFollow` on the leader and
the rotation's `MoveForwards` range-close each left drakes unable to fire
([vehicles](../engine/raid-mechanics-lessons.md#vehicles-and-flight)); `DpsRotation` no longer moves.
P3 uses its own `malygos drake flight` trigger: the Oculus `group flying` needs the leader mounted.

**Static Field: the stack slides round it.** Malygos casts 57430 on a random raider every 12 s; it
summons a **stationary** `NPC_STATIC_FIELD` that pulses for its full 20 s, so two are often up, and
it lands on someone, so it cannot be pre-dodged. A separate `avoid static field` action steering the
same vehicle was the bounce (fled to 32 yd, pulled back beside the field) and is gone.
`GetDrakeStackPoint` folds the hazard into the park spot: if the held heading is within
`STATIC_FIELD_CLEARANCE` of a field, it slides **around the boss** on the `DRAKE_STACK_RADIUS` ring
to the first of `DRAKE_RING_HEADINGS` (24, 15° apart) that clears, or the roomiest when boxed in.
Staying on the ring matters: hopping 32 yd straight off the anchor towards the most clearance often
went boss-ward, into Arcane Pulse.

The heading is latched per instance (`DrakeInstanceState`) and swept **forward only** from where the
flight already is: backwards finds ground still under the field just dodged, and re-sweeping from a
fixed base reversed the formation around every third field
([oscillation](../engine/raid-mechanics-lessons.md#movement-that-oscillates)). The clearance is
`STATIC_FIELD_SAFE_RADIUS` (32 yd) **plus** the stack tolerance; at 32 alone two or three drakes
parked on the field side still took pulses. The point is re-resolved every `DRAKE_STACK_RECALC_MS`
(300 ms): a field lands *on* the flight.

**The flight walks round to a new point.** A slide past a quarter of the ring is a chord straight
through Malygos and his Arcane Pulse. `GetDrakeApproachPoint` hops `DRAKE_APPROACH_ARC` (60°) along
the ring, whose chord passes no closer than `45 · cos 30° ≈ 39` yd (a 90° hop clears the pulse by
under 2), and forward like the stack slide — the short way round flies back through the field. Two
exceptions: a point less than a hop behind is the drake's own drift, flown straight back to; and a
drake more than `DRAKE_STACK_TOLERANCE` off the ring takes the **shortest** way, since it is not
dodging. That second one makes the phase open cleanly: every drake spawns under its rider near
Malygos, where `atan2` off the boss is noise, and forward-only sent two thirds of the flight up to
300° round in two waves. Each hop is its own `MovePoint`, the next issued once the generator is no
longer `POINT_MOTION_TYPE`. `_issuedX` / `_issuedY` track the *goal*, not the hop, which must move
`DRAKE_DESTINATION_EPSILON` (2 yd) before it is restamped.

## P3 — drakes and Surge of Power

**The healer split is a raid-size call.** Every drake has the same spellbook, so `IsDrakeHealer`
caps *and* floors: `DRAKE_HEALERS_25MAN` (5) / `DRAKE_HEALERS_10MAN` (2), filled from the `IsHeal`
bots in guid order and topped up from the DPS. With only a floor, a heal-heavy raid put seven drakes
on Revivify and ran out of phase.

**The drake spellbook**, out of the client DBC. A 100-energy bar refills at a flat 10/s (creature
regen, see [vehicles](../engine/raid-mechanics-lessons.md#vehicles-and-flight)), and **no button has
a cooldown**:

| spell | energy | GCD | effect |
|---|---|---|---|
| Flame Spike 56091 | 10 | 1 s | 943–1057 damage, +1 combo |
| Engulf in Flames 56092 | 50 | 1 s | 1500 per 3 s, stacks to 999 per caster, finisher |
| Revivify 57090 | 10 | 1 s | ally HoT 500/s for 10 s, 5 stacks per caster, +1 combo |
| Life Burst 57143 | 50 | 1 s | flat 5000 in 60 yd, self +50% healing done, finisher |
| Flame Shield 57108 | 25 | none | −80% damage taken, finisher |

No `EffectPointsPerCombo` is set, so combo points scale only **duration**: Flame Shield 1 s → 6 s,
Engulf 2 s → 22 s, the Life Burst buff 0 s → 25 s. Each finisher spends the **whole** bank
([combo points](../engine/raid-mechanics-lessons.md#combo-points-finishers-and-stacking-auras)), so
the healer rotation and the shield are tuned against one another. None of this is in the world DB
(`spell_dbc` is a partial override with no rows for these); it comes from
`modules/mod-spell-tweaks/data/dbc-reference/spell.reference.csv`, regenerated by mod-spell-tweaks'
`tools/dbc_export.py --export-reference`. **Radius and range are not in that export**:
`SpellRadius.dbc` and `SpellRange.dbc` were parsed from the client files, and `spellradius_dbc` /
`spellrange_dbc` in `acore_world` are empty.

**Drake healers cast only on themselves.** Revivify banks a combo point; Life Burst spends the bank as
a flat heal within 60 yd. Combo points are held for one target, so chasing whoever was lowest reset
the bank every switch and Life Burst was unreachable; with the flight stacked a self-cast covers the
same drakes. `CanCastVehicleSpell` reports `BAD_TARGETS` on a self-cast and `CastVehicleSpell`
reports success on a rejected one, so `DrakeCanAfford` checks `SpellInfo::CalcPowerCost` against live
power. The healer branch runs before the boss lookup in `EoEDrakeAttackAction::Execute`. DPS drakes
target the boss, so `CastDrakeSpell` goes through `CanCastVehicleSpell` and gets the power check
free.

**Healers bank five combo points and then hold them.** The five are for the **+50% healing done**
Life Burst leaves on the caster — 5 s per point, lifting every Revivify tick too
(`SpellPctHealingModsDone`, no creature exclusion) — not for the heal, a flat 5000. At 50 energy per
25 s out of a 10/s regen, letting the buff lapse costs far more than renewing it. Bursting the instant
the bank hit five made the whole corps burst in one second into a full-health flight, then sit empty.
A healer the boss has fixated skips the ladder and runs the surge script below. Otherwise, at the cap,
in order:

1. Worst drake at or below `DRAKE_BURST_EMERGENCY_PCT` (30%) — burst, no gates.
2. Another drake burst within `DRAKE_BURST_STAGGER_MS` (1.5 s) — hold.
3. Own buff gone or under `DRAKE_LIFE_BURST_REFRESH_MS` (5 s) — burst, for upkeep.
4. Worst drake at or below `DRAKE_BURST_HEALTH_PCT` (90%) — burst if this healer's rank falls inside
   `ceil(missing / DRAKE_LIFE_BURST_HEAL)`, so one healer answers a scratch and the whole corps a
   Surge of Power.
5. Otherwise hold, keeping Revivify rolling while energy is at or above `DRAKE_HOLD_ENERGY_FLOOR`
   (75 — a Life Burst plus a Flame Shield). Revivify costs exactly one global's regen, so a capped
   healer that keeps casting is break-even forever and never banks the 50 a burst needs.

**Staggering needs no shared state.** Who burst and when is read off the Life Burst buff on the other
drakes (`DrakeAuraRemainingMs`): true for a real player's cast too, and unfooled by one that silently
failed. Ties go to the rank from `GetDrakeFlightAndHealerRank` — energy descending, guid ascending —
which every bot derives alike and which rotates on its own, because bursting costs 50 energy.

**DPS drakes bank `DRAKE_ENGULF_COMBO` (3), a margin choice.** Engulf's stacks grow while the aura
lives, so applications per minute is what to maximise
([stacking auras](../engine/raid-mechanics-lessons.md#combo-points-finishers-and-stacking-auras)).
N spikes plus an Engulf cost `10N + 50` at 10/s, so the cycle runs `N + 5` s. Two points land ~8.6
applications a minute against three's 7.5 (about 8% more damage at one minute) but leave 3 s of slack
between a 10 s aura and a 7 s cycle; three leaves 6 s of a 14 s aura, enough to survive a Static Field
dodge or a moment out of range. Five gives 6 a minute. Opening at five buys nothing: every
application recomputes the duration from the bank held then, so the next 3-point Engulf resets it to
14 s, and the first stack is only delayed two globals.

**Surge of Power cannot be dodged, so Flame Shield is timed.** The boss picks its victims and fires
the damage as a **triggered instant 3 s later** (`me->m_Events.AddEventAtOffset` → `DoCastAOE`). It
ticks 12,000 every half second for 3 s — **72,000 against a 100,000 HP drake** — so cover t+3 s to
t+6 s and nothing before. Flame Shield (57108) takes 80% off and is the entire reaction; the rest is
a heal check (a strafe and a Blazing Speed peel were pointless and are gone). Both P3 surges are
`DoCastAOE` with no unit target, so the trigger reads the boss AI's own guid slots
(`DATA_FIRST_SURGE_TARGET_GUID = 14`, three slots, mirrored as `EOE_DATA_FIRST_SURGE_TARGET_GUID`),
filled by the warning selector 3 s ahead, the 25-man three-target version included. They are not
cleared until the next pick, so the trigger stays hot for most of the 7 s between casts. The action
and the healer rotation both read them through `IsDrakeSurgeTarget`.

**No cooldown stamp.** Flame Shield's `RecoveryTime`, `CategoryRecoveryTime` and `Category` are all
0, and a creature caster gets no server cooldown anyway; an invented 30 s stamp only threw shields
away. The guard is "is the shield already up".

**The cast waits for the beam.** The shield lasts `1 s + 1 s per combo point` and takes the whole
bank: fired at the fixate, two points or fewer expire before the first tick, and five spends a Life
Burst or Engulf on cover three would have bought. `DrakeSurgeShieldAction` latches when the fixate
was first seen and casts at `SURGE_BEAM_END_MS − (1 + cp)` s, clamped to the beam: at once at five,
a second before the beam at three, at the beam at two or fewer. While waiting it holds off above
`DRAKE_SHIELD_MAX_COMBO` (3), so the rotation can spend the bank and rebuild; once the beam lands it
fires regardless. It yields on an empty bank (`SPELL_FAILED_NO_COMBO_POINTS`, which
`CastVehicleSpell` reports as success).

**The middle state is load-bearing.** Shielded with the beam still landing, both rotations **cast
builders and never finish**, which keeps a point banked for the re-shield at t+5. Without it the
rotation reaches 3 and dumps into Engulf, `DrakeSurgeShieldAction` hits `if (!comboPoints) return
false`, and the re-shield silently drops. It costs one delayed finisher per fixate; the healer
emergency rung (`DRAKE_BURST_EMERGENCY_PCT`) punches through it.

**The rotation reserves the energy.** A DPS drake's cycle keeps it near empty, so a fixated drake that
kept spiking reached the beam without the shield's 25. Both rotations branch on `IsDrakeSurgeTarget`
into one script: spend the bank on Engulf or Life Burst *only* while `DrakeCanAffordWithShield`
covers the finisher and the shield, rebuild to `DRAKE_SHIELD_RESERVE_COMBO`, then stop and let the
bar climb. A drake at zero combo always spikes: that one point is worth going under the reserve.

**The reserve is one point, not two.** Two would cover the whole beam (at one, the last two ticks
land at full price, 24,000 of the 72,000), but testing had fixated drakes reaching the beam with the
points and not the 25 energy. A short shield beats none by 43,200. **Open gap:** that measurement
predates the fixate-clock fix, and a fixated drake now arrives with more energy. At 2 the first
shield alone covers the beam, making the re-shield redundant. Worth re-measuring.

A fixated DPS drake dumps the bank at `DRAKE_ENGULF_SURGE_COMBO` (2) rather than 3, since holding two
for the shield wastes them. Never at one: that refreshes the stack for `2 + 20 · 1/5` = 6 s, shorter
than the cycle that rebuilds it, so the stack falls off.

**A gap rule cannot see a re-pick, so the fixate latch restamps on the cycle.**
`EVENT_SPELL_PH3_SURGE_OF_POWER` clears and refills all three `_surgeTargetGUID` slots in one
`UpdateAI` call, so back-to-back picks leave **no gap** for `DRAKE_FIXATE_GAP_MS` to see. The second
shield went up ~4 s early and expired before the beam on ~15% of 25-man picks and ~12% of 10-man,
the full 72,000 each time. `GetDrakeSurgeElapsedMs` restamps on the gap **or** `SURGE_CYCLE_MS`
(7000): a slot cannot outlive one repeat.

**The 7 s cycle has slack.** Every P3 cast is triggered — Arcane Pulse and Static Field via
`CastSpell(..., true)`, both surges via `DoCastAOE(..., true)`, Arcane Storm via
`CastCustomSpell(..., true)` — so `UpdateAI`'s `HasUnitState(UNIT_STATE_CASTING)` guard never fires.
`events.Repeat(7s)` measures from execution, so the cycle is `7000 + δ`, δ per cycle (not cumulative)
and bounded by roughly three map ticks (~150–300 ms).

**The shield is safe mid-dodge**, which matters because Static Field lands on the flight. A self-cast
skips the branch that turns the vehicle onto a target; the branch that stops it dead is skipped only
because the shield is instant. A Skytalon's seat (2200, `Flags` 0x62110817) carries
`VEHICLE_SEAT_FLAG_CAN_CONTROL`, so any drake spell given a cast time would halt the dodge. The action
returns false either way, so `eoe fly drake` below it still gets the tick.

`EoEDrakeAttackAction` and `DrakeSurgeShieldAction` are plain `Action`s, free under the P3
`Exclusive`; `EoEFlyDrakeAction` is the one `MovementAction` it names.

## Cost

Per-raid cost in general is in
[../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md#what-a-strategy-costs-per-raid);
twenty-five bots on one small platform asking the same questions every tick is its case study.

- **One creature cache for the instance.** `GetEoECreatures` / `GetNearestEoECreature` /
  `AnyEoECreature` answer from a map keyed by instance and entry, refilled at most every
  `EOE_CREATURE_CACHE_MS` (300 ms); P2 had cost eight to ten sweeps per bot per tick, with the bubble
  and free-disk triggers duplicating each other. The fill is anchored on `MALYGOS_CENTER_POSITION`,
  not on the refreshing bot, so the shared answer is position-independent; `EOE_CACHE_SWEEP_RADIUS`
  (120 yd) covers the r 107 spark ring, everything else being inside r 56. That anchor needs a
  file-local check functor: `Acore::AllCreaturesOfEntryInRange::operator()` always runs `IsWithinDist`
  from the object it was built with, so visiting cells around another point does not move its range
  test, and radius 0 filters everything rather than disabling it. Guids are cached, not pointers.
- **`GetMalygosPhase` memoises per instance** for 500 ms (`EOE_PHASE_CACHE_MS`). The Skytalon check,
  the one per-bot part, runs first and uncached (a pointer read). Half a second of lag at a
  transition only affects which overrides are on.
- `GetMalygos` asks the instance script (`GetCreature(DATA_MALYGOS)`, mirrored as `EOE_DATA_MALYGOS`)
  before any search and caches the guid per instance. **Nothing uses `"find target" malygos`**: it
  walks the threat list with `Utf8toWStr` + `wstrToLower` + `Utf8FitTo` per entry, per bot, per
  tick, and answers only for units the bot already threatens
  ([things the bot cannot see](../engine/pitfalls.md#things-the-bot-cannot-see)).
- **Who holds Malygos is read once per bot per tick** (`GetMalygosAggro`, a per-thread cache of live
  state) by the P1 rules and the position, target and kill-spark actions; uncached `IsMainTank` was
  tens of thousands of strategy-list scans a tick across the raid.
- **The P3 healer roster is cached per instance for 2 s** (`GetDrakeHealerGuids` walks and sorts the
  group). `GetDrakeFlightAndHealerRank` stays uncached: it reads live energy.
- **All EoE caches and latches live in `RaidInstanceState`**; latches also carry `EOE_LATCH_STALE_MS`
  (5 min), so a re-pull in a still-loaded instance doesn't inherit the last pull's drake heading.
- `power spark`, `malygos bubble` and `surge of power` check every 200 ms and `malygos free disk`
  every 300 ms: each starts a walk, boarding or peel the `MotionMaster` carries on with. `malygos`,
  `malygos on disk` and `malygos drake flight` stay per-tick: they gate positioning and vehicle
  steering.
