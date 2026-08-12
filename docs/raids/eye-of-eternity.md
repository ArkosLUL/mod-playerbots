# Eye of Eternity — Malygos (map 616)

Strategy key `wotlk-eoe`. Cross-raid conventions are in [README.md](README.md).

## Three plan assumptions were wrong — corrected in code

Verified against `boss_malygos.cpp`:

- **No "Deep Breath" spell exists in this core.** `SAY_DEEP_BREATH` is only the P2 Surge windup yell,
  and **`56430` is Arcane Overload**, the P2 ground void zone. `deep breath` / `deep breath dodge`
  were kept for a while as an Arcane Overload *dodge* and are now deleted — the bubbles are shelter,
  not a hazard (see P2 below).
- **The P2 floor is not gone** — the platform is destroyed only at P3 start
  (`EVENT_DESTROY_PLATFORM_0`). P2 anti-fall is still worth doing as recenter-on-drift, but the real
  P2 hazards are Arcane Overload and the Surge of Power beam.
- Confirmed ids: Static Field `57430` → `NPC_STATIC_FIELD 30592`; Arcane Storm `61693` (random
  target, unavoidable → heal through); Surge P2 `56505` → `NPC_SURGE_OF_POWER 30334`; Surge P3
  `57407` (10) / `60936` (25); Arcane Pulse `57432` (self-cast every 3 s, **30 yd** around the boss,
  ~28k arcane — a positioning hazard, not raid-wide damage); Vortex `56105` (fully server-driven, no
  bot action). Centre anchor `{754.395, 1301.27, 266.1}`.

## Phase model

P1 = Malygos attackable and no adds. P2 = disc adds (`NPC_NEXUS_LORD` / `NPC_SCION_OF_ETERNITY`) up.
P3 = on a Wyrmrest Skytalon (30161). **P4 = transition** — in combat, Malygos non-attackable, not
mounted.

P4 exists because of a real failure: during P2→P3 the boss is untargetable and the bot is not yet on
its drake, so the phase resolved to 0, `MalygosMultiplier` lifted every override, and **the default
raid strategy resumed mid-transition on a collapsing platform**. P4 holds the raid at centre and
keeps the EoE overrides on.

P4 also catches the **pull intro**, which wants the opposite of a centre gather — see the P1 bullet
below. Malygos is untouchable for the whole intro, so full health separates it from the two real
transitions, which are both at 50%.

`getMalygos` uses `FindNearestCreature` so detection survives the non-attackable flag.

## No navmesh, no vmaps, no terrain

**Map 616 ships no `.mmtile` files at all** — only the 28-byte `616.mmap` header, which claims
`maxTiles = 25`. It is the only one of the 98 maps in the client data like this. It also ships **no
vmaps** (no `616.vmtree`, no `616*.vmtile`), and all 25 of its `.map` tiles are 616 bytes of header
carrying `MHGT` flags `0x09` (`MAP_HEIGHT_NO_HEIGHT | MAP_HEIGHT_HAS_FLIGHT_BOUNDS`) with
`gridHeight = 0.0`. There is no collision geometry of any kind here.

`PathGenerator::CalculatePath` bails at its `!HaveTile(start) || !HaveTile(dest)` guard, calls
`BuildShortcut()` and reports `PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH` (0x11) — whose own enum
comment is "used when we are either flying/swiming or **on map w/o mmaps**". Four consequences:

- **`generatePath` is inert here.** A shortcut path has exactly two points, and
  `PointMovementGenerator` only uses a generated path when `GetPath().size() > 2`, so both settings
  emit the same straight spline. Passing `false` is still right — it is explicit, and it survives
  someone generating tiles for 616 later — but it is not what fixes anything today.
- **Ground height is 0.0 everywhere.** `getHeightFromFlat` returns `gridHeight`, and with no vmaps
  nothing can override it, so `Map::GetHeight` answers 0.0 across the whole map while the platform
  sits near Z 266. `UpdateAllowedPositionZ` then pins any **non-flying** unit to Z 0 — a 266 yd
  drop. Its flying branch only ever raises Z, so it is inert up here.
- **The off-navmesh failure mode cannot happen on this map.** Raw ring geometry that would be
  rejected elsewhere ([../engine/pitfalls.md](../engine/pitfalls.md)) always paths here, and the
  *height* half of `SearchForBestPath`'s check has nothing but the flat 0.0 surface to test against.
- **Path-type tests must be bitmask, not equality.** `0x11` equals neither `PATHFIND_NORMAL` nor
  `PATHFIND_INCOMPLETE`, so a `type != PATHFIND_NORMAL && type != PATHFIND_INCOMPLETE` test rejects
  every path in this instance. `ReachCombatTo` and `SearchForBestPath` both mask correctly;
  `MovementAction::MoveToLOS` does not, and would refuse to move at all here. It has no callers
  today, so this is latent.

## Per-phase behaviour

- **P1 — Power Sparks.** Previously fully stubbed: the trigger was registered but never bound, the DK
  death-grip pull was commented out, the ranged target-switch was commented out, and
  `KillPowerSparkAction` had no body and no registration. Sparks reaching Malygos stack a
  damage + haste buff (56152) into a soft enrage. Now: `power spark` bound, DK `pull power spark`
  at +3, DPS `kill power spark` at +2, and the same pick drives the switch in `malygos target`.
  A spark spawns at one of the four `FourSidesPos` corners, 94–107 yd from centre, and walks straight
  at Malygos at 6.0 yd/s, re-issuing `MovePoint(0, *malygos)` every 2 s. It hands over its buff at
  12 yd, centre to centre.
  **Reach is what decides who shoots it.** Nobody walks in P1, so `GetPowerSparkToKill` only ever
  returns a spark the bot can hit standing still — `spellDistance` for ranged, melee range plus
  `POWER_SPARK_MELEE_STICKY` for everyone else — and of those, the one nearest Malygos, i.e. the one
  about to hand over its buff. Before that gate, bots locked onto whichever spark came out of the
  target list first, usually one 100 yd away, and stood there doing nothing while the boss went unhit.
  Melee dps take a spark that walks into them on its way past; the tank never switches, because
  dropping Malygos swings the Arcane Breath cone into whoever is behind him.
  **The DK walks out to grip, then walks back.** Death Grip lands the spark *on the caster*, so
  where the DK stands decides both who gets the corpse's buff and whether the spark ends up inside
  the 12 yd at which `npc_power_spark` hands *its* buff to Malygos. `POWER_SPARK_GRIP_OFFSET` sits
  ~21 yd from where Malygos parks, so a spark dropped there still has 9 yd to walk against ~12k hp
  and the whole raid.

  **Open gap — the grip spot reaches nobody.** A dying spark self-casts
  `SPELL_POWER_SPARK_GROUND_BUFF` (55852) and despawns after 60 s (`boss_malygos.cpp:842`). 55852 is
  a 60 s periodic-trigger aura firing **55849** once a second, and 55849 is what carries the payload:
  `EffectRadiusIndex 14` = **8 yd**, aura 79 `MOD_DAMAGE_PERCENT_DONE`, misc 127, **+50% damage
  done** to allies in that radius. The offset is `(MALYGOS_STACK_OFFSET + MALYGOS_HUNTER_OFFSET) / 2`
  = −1 yd from centre, i.e. **13 yd from both** the stack and the hunters, so the buff currently
  lands on nobody. It was chosen to maximise the smaller of the two distances back when the radius
  was unknown; that reasoning is dead.

  Retuning is not a one-line change, because the two constraints collide: the corpse wants to be
  within 8 yd of the stack, while Malygos parks only ~8.5 yd from it (~+20.5 from centre against the
  stack's +12) and takes the buff at 12. An offset around **+5 to +6** puts the corpse ~6 yd from the
  stack and ~15 yd from him. Unverified in game.
  The split of duties matters: **`MalygosPositionAction` owns all the walking, `PullPowerSparkAction`
  only ever casts.** Both read `IsOnPowerSparkGripDuty`, so they cannot disagree about where the DK
  should be. Duty needs the grip off cooldown (the cooldown outlasts the gap between spawns, so there
  is no point giving up boss uptime otherwise), a spark within
  `POWER_SPARK_GRIP_ENGAGE_RADIUS` (45 yd) of the spot, and Malygos no closer to the spot than
  `POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE`. Casting the grip ends duty, and the position action walks the
  DK back to the melee stack on the next tick.
  **The grip is followed by Chains of Ice.** The pull buys the distance back once and the spark walks
  it off again at 6 yd/s, so the same action snares whatever it just landed: `GetPowerSparkToSnare`
  takes the nearest spark inside `POWER_SPARK_SNARE_RADIUS` (15 yd, about where a gripped one lands)
  that nobody has chained yet. Power Spark's immunity mask (`creature_immunities` −335, mechanics
  `0x26CB031D`) carries neither root nor snare, so it lands; the aura check is caster-agnostic
  because a second DK re-snaring spends a rune for nothing. The action keeps the grip first whenever
  the grip is available, and stays useful on the snare alone, so a DK whose grip is still on cooldown
  can chain a spark that walks past the melee stack.
- **P1 — the hold spots are taken during the pull intro, before the boss lands.** `JustEngagedWith`
  sends Malygos on an intro circuit and only then drops him at `CenterPos.z`, 35 yd out from centre
  on whatever heading he was circling; `EVENT_START_FIGHT` clears his flags and he chases
  `SelectNearestTarget(250)`. That whole stretch reads as P4, and the P4 centre gather pulls the tank
  *off* his spot — it sits 42 yd out, outside the 30 yd ring. `MalygosPositionAction` runs the P1
  branch through the intro instead, so the tank is parked and facing when the boss touches down.
  During the intro only the raid's assigned main tank counts as the tank: Malygos is pacified, and
  his victim is nothing more than whoever pulled.
- **P1 — the hold spots rotate onto the bearing Malygos landed on.** Arcane Breath (56272) is a
  frontal cone on the boss's *current victim*, so the fight is won or lost on where the tank stands —
  and he does not always land north. `EVENT_INTRO_MOVE_CENTER` snapshots `CenterPos.GetAngle(me)` the
  instant `JustEngagedWith` fires, flies him in along that bearing to 35 yd out, and
  `EVENT_INTRO_LAND` drops him straight down. He idles between the four `FourSidesPos` corners, so
  there are only four answers: **−135.95°, +46.51°, +134.45°, −44.60°**
  (`MALYGOS_LANDING_ANGLES`), the bearings of `{686.417, 1235.52}`, `{828.182, 1379.05}`,
  `{681.278, 1375.796}` and `{821.182, 1235.42}`. The layout is one set of signed offsets from
  centre — positive towards him — rotated onto whichever of those four is nearest to where he
  actually is:
  - `MALYGOS_MAINTANK_OFFSET` **+42 yd** — the Exit Portal sits 43.4 yd out on bearing 133.2°, and
    the platform GO is centred on `CenterPos`, so there is ground that far on any bearing. The portal
    is phased out by `DATA_HIDE_IRIS_AND_PORTAL` once the fight starts. Malygos' CombatReach of 20
    parks him ~21.5 yd short of the tank.
  - `MALYGOS_STACK_OFFSET` **+12 yd** — melee, healers and every ranged DPS but the hunters. 30 yd
    from the tank, inside 40 yd heal range, and behind where the boss stops, so out of the cone.
    That last part assumes his chase actually brings him to ~21.5 yd short of the tank spot; it stops
    wherever it first puts him in melee range, so coming in off-bearing can leave the melee half of
    the raid swinging at nothing. So the stack spot — and only the stack spot — is **clamped**:
    further than `MALYGOS_MELEE_HOLD_DISTANCE` (15 yd) from him and it slides up the line towards him
    until it is that close. Melee range against him is ~22.8 yd (his 20 yd CombatReach, the player's
    own reach, plus the 4/3 the core adds) and a bot may park 5 yd off its spot, so 15 swings with
    margin. The clamp keeps the bearing the stack already holds from him, so it can never land in
    front of him; it moves continuously with him rather than switching between two spots, which is
    what would set the raid bouncing; and it is off during the pull intro, when he is circling and
    untouchable anyway. With the layout rotated onto him it should rarely fire at all.
  - `MALYGOS_HUNTER_OFFSET` **−14 yd**, i.e. past centre, ~33 yd from the boss — **hunters only**.
    `Spell::CheckRange` adds `GetMeleeRange` to a spell's minimum for `SPELL_RANGE_RANGED`, so
    Malygos' CombatReach of 20 inflates a hunter's 5 yd minimum to ~28 yd of centre-to-centre distance
    and every shot came back `SPELL_FAILED_TOO_CLOSE`. Nothing else has a minimum range, and standing
    out here is exactly what left the raid unable to reach a Power Spark closing on the boss from the
    far side — 33 yd to the boss plus 12 more to the spark is well past `spellDistance`. So casters
    hold the stack instead. The same reach keeps `EnemyTooCloseForSpellTrigger` (threshold ~23.5 yd)
    permanently active for anyone standing close, and every class wires that trigger to an escape at
    34–50 relevance — above `malygos position` at `ACTION_MOVE`. Bots stepped out, were dragged back
    next tick and never finished a cast, so the P1 multiplier zeroes `FleeAction`, `RunAwayAction`,
    `CastBlinkBackAction` and `CastDisengageAction` for anyone in the encounter.
  - `POWER_SPARK_GRIP_OFFSET` **−1 yd**, held only by a DK on spark duty — see the Power Spark
    bullet above.

  `GetMalygosP1Layout` resolves the set once and **latches it for the pull**, keyed on the instance
  and shared across the raid, so bots cannot end up half on one set and half on another while he
  walks. The latch clears when the encounter drops out of combat. Within a pull the spots are never
  recomputed from his live position: a spot that chases him flips to his far side while he is still
  walking out, and the tank then ping-pongs between the edge and the middle, sweeping the cone through
  the raid. Whoever Malygos is actually hitting behaves as the tank, assigned or not, and a bot
  corrects only past `MALYGOS_P1_POSITION_TOLERANCE` (5 yd).

  **Nobody but the boss's current victim walks anywhere in P1.** The multiplier zeroes every
  `MovementAction` and `CastReachTargetSpellAction` for everyone else, naming
  `MalygosPositionAction`, `MalygosTargetAction`, `KillPowerSparkAction` and
  `ReachPartyMemberToHealAction` as the exemptions. Two separate symptoms, one cause: ranged were
  chasing Power Sparks back inside Malygos' minimum range (the DK grips sparks to them instead), and
  melee were walking out to get behind him (`set behind`, `ACTION_MOVE + 7`) or to spread
  (`combat formation move`) and being dragged back to the stack next tick — the shuffling that shows
  up in game. The hold spots need no help: they are inside his 20 yd combat reach for melee and
  outside the inflated minimum for hunters. `SetFacingTargetAction` is a plain `Action`, so facing
  still works; `AttackAction` is not, which is why the EoE attack actions have to be named. Note the
  named exemptions only ever *target* — `AttackAction::Attack` sets the target and stops nothing but a
  sub-combat-priority walk, so a melee bot peeling onto a spark still stays put.

  There is no `avoid arcane breath` action: the cone points away from the raid, at the tank, and every
  other spot is behind it.
- **P2 — the Arcane Overload bubbles are shelter, and the old code ran the wrong way.**
  `NPC_ARCANE_OVERLOAD` (30282) grants **56438, −50% damage taken**; the protected radius shrinks
  ~2 % per tick over the bubble's 45 s life, so bots hug the centre within 4 yd and ignore any bubble
  already below `BUBBLE_MIN_USABLE_FACTOR` (35 %) of its original radius. **The model does not
  shrink with it** — the core declares 56435 and never casts it — so apparent size says nothing and
  the only honest source is the aura's tick count, read off the `creature_template_addon` aura
  applied at spawn (no aura means brand new). The bubble NPC is
  non-attackable, so it never shows in `"possible targets"` — scan with
  `GetCreatureListWithEntryInGrid`. `malygos seek bubble` sits at `ACTION_EMERGENCY + 2`, above
  `avoid surge of power`, which is now only the fallback for bots that cannot reach one: it steps
  off the line running from Malygos through the surge focus (`SURGE_BEAM_CLEAR_DISTANCE` to clear
  it, `SURGE_BEAM_SIDESTEP` across), and it never fires for a disk rider or a sheltered bot, both of
  whom are already covered and would be walked out of cover for the beam's whole 10 s. The bubble
  assignment is **latched by GUID** (the `SapphironFlightPositionAction` idiom) and spread by group
  slot index, or bots hop between bubbles as they shrink — unless that slot's bubble is more than
  half `BUBBLE_SEARCH_RADIUS` away, where survival beats spreading and the bot takes the nearest.
  Once the bot holds 56438 the action returns false so the rotation runs, and the phase-2
  multiplier zeroes reach/chase/follow for non-vehicle
  bots so nobody walks back out; `MalygosTargetAction` only accepts a Nexus Lord / Scion inside
  `spellDistance` by **`IsWithinCombatRange`**, the same 3d combat-reach test `Spell::CheckRange`
  runs — the Scions hover 20–30 yd up, and the old flat 2d distance claimed a reach that was not
  there. A held add is kept **by GUID** while it is alive, still of the right kind and still in
  reach; matching on entry alone welded bots to an add they could never get to, and re-picking every
  tick made two equidistant Scions flip forever. Nexus Lords come first for everyone, ranged DPS
  included: they are on the ground, can be tanked and die faster, and Scions are what ranged fall
  back to once no Lord is in reach. Anti-fall recenter in `malygos position` still applies, and it
  bails immediately for anyone in a vehicle. Note the old code suppressed `FleeAction` wholesale in
  P2, removing the one instinct that might have pulled a bot back from the edge.
- **P2 hover disks — melee only.** When a Nexus Lord dies the core lands its disk (30248), turns it
  friendly and clears `UNIT_FLAG_NOT_SELECTABLE`, so *landed + selectable + free seat* doubles as
  "its rider is dead". Melee DPS board it (`EnterVehicleAction`, which uses `HandleSpellClick` —
  required, the mount is `npc_spellclick_spells` 61421) and ride it to the Scions of Eternity, which
  hover ~30 yd out and +20 yd up and are otherwise unreachable in melee. Passengers are immune to
  both Arcane Overload and Surge of Power, which is why tanks, ranged and healers stay in bubbles
  instead. `MalygosRideDiskAction` steers the vehicle's own `MotionMaster`, with a
  `POINT_MOTION_TYPE` anti-stutter guard. Its `MovePoint` calls pass `generatePath = false`, which
  on this map is belt-and-braces rather than the fix — see **No navmesh, no vmaps, no terrain**
  below. The dive came from a rider's own chase actions steering the disk, and the multiplier
  lockout is what stops it; the Z mechanism itself is still open (see P2 movement ownership).
  Scion altitude is stable — the
  core floors a Scion disk's descent at `CenterPos.z + 20` (`boss_malygos.cpp`, `MI_POINT_SCION`) —
  so a fixed target Z is safe. A rider only dismounts once
  the disk is back down at `MALYGOS_PLATFORM_Z`; stepping off at Scion altitude is a 20–30 yd drop.
  Ending the ride is manual — Scions can die before the Nexus Lords do and the core keeps the disks
  until both are gone — and the descent is stamped straight over the approach in progress rather
  than waiting for `POINT_MOTION_TYPE` to clear, because the disk is still flying at the Scion that
  just died.
- **P2 pets go on Nexus Lords, always.** Scions hover out of reach of any pet, and the generic
  pet-attack trigger is disabled globally, so a pet would chase its owner's airborne target forever.
  `MalygosTargetAction` redirects with `CommandPetAttack` / `StopPet` (`RaidBossHelpers`) at the top
  of its P2 branch, before every early return, so a disk rider's ghoul is covered too.
- **P2 — mages steal the Nexus Lords' Haste.** `npc_nexus_lord` self-casts `SPELL_HASTE` (57060)
  every 20–30 s. `malygos spellsteal` takes it off the nearest Lord already inside `spellDistance`,
  by `IsWithinCombatRange`, so nobody leaves a bubble for it. It never touches the bot's target, and
  it hangs off the `malygos` trigger rather than one of its own — the action gates itself on class
  and phase, so it costs nothing outside P2. The generic `SpellstealTrigger` only looks at the
  current target, which is a Scion as often as not.
- **P3 — one stack point, not a ring, and `EoEFlyDrakeAction` owns it alone.** Malygos is pacified
  and immobile for the whole of phase 3 (`MI_POINT_PH_3_FIGHT_POSITION`, then
  `UNIT_FLAG_DISABLE_MOVE`), so the park spot is a fixed offset from him: `DRAKE_STACK_RADIUS`
  (45 yd) along `DRAKE_STACK_ANGLE`, resolved identically by every drake without anyone
  coordinating. Its height is `MALYGOS_P3_BOSS_Z`, a constant, **not** the boss' live Z — he opens
  the phase at `CenterPos.z + 70` and takes several seconds to sink to his fight position at
  `CenterPos.z - 5`, and a stack anchored on him flew the whole flight up over the arena and then
  rode him back down. `DRAKE_STACK_TOLERANCE` (10 yd) is deliberately loose — drakes park anywhere
  inside it, which is what stops the flight piling onto one coordinate. Arriving is a straight 3d spline
  (`generatePath = false`, same reason as the hover disks), then `MoveIdle` plus
  `SetFacingToObject(boss)` and a `return false` handing the tick to the rotation. Everything a
  drake casts is aimed at the boss or at itself, so the facing pin applies to healers too.
  The action sits at `ACTION_EMERGENCY` — high because it is the *only* thing allowed to steer a
  Skytalon, harmless because it stands down the moment the drake is parked. Before the boss is in
  reach there is nothing to anchor on, so the flight fans out behind the raid leader instead:
  `DRAKE_FORMUP_RADIUS` out, spread over three quarters of a circle with a 90° frontal cone left
  clear.
  Two earlier bugs died here: `MoveFollow` on the raid leader left every drake permanently in
  motion (a moving vehicle can neither finish a cast nor hold a facing — the flight circled the boss
  without ever firing), and `DrakeDpsAction`'s range-close called `MoveForwards`, whose endpoint
  goes through `CanReachPositionAndGetValidCoords` — no answer for a point in mid-air, so it
  silently did nothing *after* `mm->Clear(false)` had wiped the follow, and the drake stopped dead
  out of range for good. `DrakeDpsAction` no longer moves at all.
- **P3 — Static Field: the stack drifts off it and re-parks.** Malygos casts 57430 on a random
  raider every 12 s; it summons a **stationary** `NPC_STATIC_FIELD` that pulses for its full 20 s
  life, so two are often up at once. It cannot be pre-dodged — it lands on someone.
  There used to be a separate `avoid static field` action at emergency relevance, and *that* was the
  bounce: it and the flight action both steered the same vehicle, so the drake fled to 32 yd, the
  flight action pulled it back to a formation slot next to the field, and round it went. It is gone.
  `GetDrakeStackPoint` folds the hazard into the park spot instead: if the held heading is inside
  `STATIC_FIELD_CLEARANCE` of any field, it slides **around the boss** to another point on the
  `DRAKE_STACK_RADIUS` ring, and the first of `DRAKE_RING_HEADINGS` (24, so 15° apart) that clears
  wins. Boxed in on every heading, it
  takes the roomiest one rather than sit in the field — the fields expire on their own. Staying on
  the ring is the point: an earlier version hopped 32 yd straight off the anchor toward whatever had
  the most clearance, and boss-ward was often that direction, which put the flight inside Arcane
  Pulse and killed it faster than the field would have.
  Two details matter as much as the ring itself. **The whole phase is one slow lap, always forward**
  — the heading is latched per instance (`stackAngleCache`, `thread_local`, keyed on the instance
  like the phase and creature caches) and the sweep starts from wherever the flight already is,
  never from `DRAKE_STACK_ANGLE`. Both halves of that are load-bearing. Sweeping backwards picks the
  ground the flight has just crossed, where the field it dodged is still live. And re-sweeping from
  a fixed base is the subtler one: an expiring field frees a heading *behind* the flight, the sweep
  finds it before any heading ahead, and the whole formation reverses — with fields lasting 20 s and
  landing every 12 s, that happened around the third field, which is exactly what it looked like in
  game. Forward-only, a field can only ever push the flight further along. And **the clearance is
  `STATIC_FIELD_SAFE_RADIUS` (32 yd) plus the stack tolerance**, because the tolerance is exactly how
  far off the point a drake is allowed to park: a spot 32 yd from a field still left whoever stopped
  on the field side of it standing in the pulse, which is why two or three of the flight took damage
  from a field the stack had supposedly cleared.
  Resolving the point is cheap now the creature lookup is cached, so the flight action only holds it
  for `DRAKE_STACK_RECALC_MS` (300 ms) — a field lands *on* the flight, so a stale answer here is
  damage taken.
  **The flight walks round to the new point rather than flying at it.** `MovePoint` runs a straight
  spline, so a slide of more than a quarter of the ring is a chord across the middle — straight
  through Malygos and his 30 yd Arcane Pulse, which costs more than the field being dodged.
  `GetDrakeApproachPoint` breaks the trip into hops of `DRAKE_APPROACH_ARC` (60°) along the
  `DRAKE_STACK_RADIUS` ring, and a drake **already on the ring** hops **forward**, the same way the
  stack point slides — one that took the short way round would fly back through the field. Two
  exceptions. A point less than a hop behind the drake is its own drift off the stack rather than a
  dodge, so it just flies straight back to it. And a drake that is not on the ring yet — more than
  `DRAKE_STACK_TOLERANCE` off `DRAKE_STACK_RADIUS` — takes the **shortest** way, because it is not
  dodging anything. That second exception is what makes the phase open cleanly: P3 summons every
  drake under its own rider, all of them within a few yards of Malygos at the centre, where `atan2`
  off the boss is noise, so forward-only would send about two thirds of the flight up to 300° round
  the ring in legs a tick apart and the stack would fill in two separate waves.
  The chord of one hop passes no closer than
  `45 · cos 30° ≈ 39` yd to him, where 90° would clear the pulse by under 2 yd. Each hop goes out as
  its own `MovePoint`, and the drake takes the next one when the spline ends — a finished point
  generator is replaced by an idle one, so "not `POINT_MOTION_TYPE`" is the arrival signal.
  `issuedX`/`issuedY` track the *goal*, not the hop, and exist only to notice the goal itself moving:
  re-issuing `MovePoint` restarts the spline, so a drake handed the same destination every tick
  crawls and never arrives; the goal has to move `DRAKE_DESTINATION_EPSILON` (2 yd) before it is
  restamped.
- **P3 — the healer split is a raid-size call, not a spec one.** Every drake carries the same
  spellbook, so `IsDrakeHealer` caps *and* floors: `DRAKE_HEALERS_25MAN` (5) / `DRAKE_HEALERS_10MAN`
  (2), filled from the bots flagged `IsHeal` in guid order and topped up from the dps if the raid
  brought fewer. The old version only had a floor, so a heal-heavy raid put seven drakes on Revivify
  and ran out of phase. Guid order is identical on every bot, so the flight agrees without talking.
- **P3 — the drake spellbook, read out of the client DBC.** Every button spends energy from a
  100-point bar that refills at a flat 10/s (`unit_class` 4 plus `UNIT_FLAG2_REGENERATE_POWER`, so
  `Creature::Regenerate` hands out 20 every 2 s), and **not one of them has a cooldown**:

  | spell | energy | GCD | effect |
  |---|---|---|---|
  | Flame Spike 56091 | 10 | 1 s | 943–1057 damage, +1 combo |
  | Engulf in Flames 56092 | 50 | 1 s | 1500 per 3 s, stacks to 999 per caster, finisher |
  | Revivify 57090 | 10 | 1 s | ally HoT 500/s for 10 s, 5 stacks per caster, +1 combo |
  | Life Burst 57143 | 50 | 1 s | flat 5000 in 60 yd, self +50% healing done, finisher |
  | Flame Shield 57108 | 25 | none | −80% damage taken, finisher |

  Combo points never scale an *amount* on any of the five — no `EffectPointsPerCombo` is set. They
  scale **duration**, through `Unit::CalcSpellDuration`'s `min + (max − min) · cp / 5`: Flame Shield
  runs 1 s → 6 s, Engulf 2 s → 22 s, the Life Burst buff 0 s → 25 s. And all three finishers carry a
  `SPELL_ATTR1_FINISHING_MOVE_*` bit, so each spends the **whole** bank whatever size it is — which
  is why the healer rotation and the shield have to be tuned against one another rather than apart.
  None of this is in the world DB; `spell_dbc` is a partial override table with no rows for these
  spells. It comes from `modules/mod-spell-tweaks/data/dbc-reference/spell.reference.csv`
  (regenerate with `python tools/dbc_export.py --export-reference`).
- **P3 — drake healers only ever cast on themselves.** Revivify is a HoT and each cast banks a combo
  point; Life Burst spends the bank as a flat heal on everyone within 60 yd of the caster. Both go on
  the healer's own drake: a `Unit` holds combo points for one target at a time, so an earlier version
  that chased whoever was lowest reset the count to one on every switch and Life Burst was
  unreachable. With the flight stacked, a self-cast Life Burst covers the same drakes a targeted one
  would. Neither goes through `CanCastVehicleSpell` — it reports `BAD_TARGETS` on a drake — so
  `DrakeCanAfford` stands in for the power half of that check, reading `SpellInfo::CalcPowerCost`
  against the drake's current power rather than hardcoding a cost. Without it an unaffordable spell
  would look exactly like one that went out, since `CastVehicleSpell` returns true even when the cast
  it prepared was rejected. The healer branch runs before the boss lookup in
  `EoEDrakeAttackAction::Execute`, since a healer needs no boss at all. The dps side needs none of
  this — its targets are the boss, so `CastDrakeSpellAction` goes through `CanCastVehicleSpell` and
  gets the power check for free.
- **P3 — healers bank five combo points and then hold them.** The five are not for the heal: Life
  Burst restores a flat 5000 whatever the drake holds. They are for the **+50% healing done** it
  leaves on the caster, which runs 5 s per point and lifts every Revivify tick as well
  (`SpellPctHealingModsDone`, no creature exclusion). At 50 energy per 25 s out of a 10/s regen,
  letting that buff lapse costs far more than renewing it does. An earlier version burst the instant
  it reached five — and since every healer starts the phase together and casts once per global, the
  whole corps burst in the same second into a full-health flight, then sat at zero combo and zero
  energy waiting for the next one. A healer the boss has fixated skips the ladder below entirely and
  runs the surge script in the Flame Shield bullet instead. Otherwise, at the cap it decides in this
  order:
  1. Worst drake at or below `DRAKE_BURST_EMERGENCY_PCT` (30%) — burst, no gates.
  2. Another drake burst within `DRAKE_BURST_STAGGER_MS` (1.5 s) — hold.
  3. Own buff gone or under `DRAKE_LIFE_BURST_REFRESH_MS` (5 s) — burst, for upkeep.
  4. Worst drake at or below `DRAKE_BURST_HEALTH_PCT` (90%) — burst if this healer's rank falls inside
     `ceil(missing / DRAKE_LIFE_BURST_HEAL)`, so one healer answers a scratch and the whole corps
     answers a Surge of Power.
  5. Otherwise hold, keeping Revivify rolling while energy is at or above `DRAKE_HOLD_ENERGY_FLOOR`
     (75 — a Life Burst plus a Flame Shield). Revivify costs exactly what a Skytalon regenerates in
     one global, so a capped healer that keeps casting is break-even forever and never banks the 50 a
     burst needs.

  **Staggering needs no shared state.** Who burst and how recently is read off the Life Burst buff
  sitting on the other drakes (`DrakeAuraRemainingMs`) — true even when the caster was a real player,
  and impossible to fool with a cast that quietly failed. Ties go to `GetDrakeHealerRank`, which
  orders healer drakes by energy descending and guid ascending: every bot derives the same order from
  the same visible state, and it rotates on its own, because bursting costs 50 energy and drops the
  caster to the back of the queue.
- **P3 — dps drakes bank `DRAKE_ENGULF_COMBO` (3), and three is a margin choice rather than a damage
  one.** Engulf is a single aura per caster with a 999 stack cap, and every application runs
  `ModStackAmount(1)` → `RefreshTimers`, so the whole stack shares one duration that resets on each
  cast while the periodic amount is multiplied by the stack. Stacks therefore keep growing for as
  long as the aura stays alive, which makes *applications per minute* the thing to maximise —
  duration only has to outlive the cycle. That cycle is energy-bound: N spikes plus an Engulf costs
  `10N + 50` at 10/s, so it runs `N + 5` seconds. Two points would land ~8.6 applications a minute
  against three's 7.5 (about 8% more damage by the one-minute mark) but leaves only 3 s of slack
  between a 10 s aura and a 7 s cycle, where three leaves 6 s against 14 s — enough to survive a
  Static Field dodge or a moment out of range without dropping the stack and starting again at one.
  Five would be clearly worse: 6 applications a minute.

  There is no way to buy that margin once and keep it. `ModStackAmount` → `RefreshTimers` calls
  `CalcMaxDuration(GetCaster())` on **every** application, which runs `Unit::CalcSpellDuration`
  against the combo points held at that moment — and those are still on the drake, because
  `_handle_immediate_phase` applies the aura before `_handle_finish_phase` clears the pool. So an
  opening Engulf at five points gives the stack 22 s exactly until the next one at three resets it to
  14 s. Opening at five only delays the first stack by two globals.
- **P3 — Surge of Power cannot be dodged, so Flame Shield is timed rather than reflexive.** The boss
  picks its victims, then fires the damage as a **triggered instant 3 s later**
  (`me->m_Events.AddEventAtOffset` → `DoCastAOE`): no beam to walk out of, no cast to outrun. It then
  ticks 12,000 every half second for 3 s — **72,000 against a 100,000 HP drake** — so the window that
  has to be covered is t+3 s to t+6 s and nothing before it. Flame Shield (57108) takes 80% off, and
  that is the entire reaction; everything it does not cover is a heal check. The strafe and Blazing
  Speed peel the action used to do were both pointless and are gone, along with the self-imposed rate
  limit they needed.
  The trigger could not fire at all before: both P3 surges are `DoCastAOE` with no unit target, so
  `m_targets.GetUnitTargetGUID()` was always empty and Flame Shield never went up once. The boss AI
  publishes its victims in its own guid slots instead (`DATA_FIRST_SURGE_TARGET_GUID = 14`, three
  slots, mirrored as `EOE_DATA_FIRST_SURGE_TARGET_GUID`), filled by the warning selector 3 s ahead,
  and it does so for the 25-man three-target version as well. The slots are not cleared until the
  next surge is picked, so the trigger stays hot for most of the 7 s between casts. Both the action
  and the healer rotation read them through one helper, `IsDrakeSurgeTarget`, so they cannot disagree
  about who is about to be hit.
  **The 30 s cooldown it used to stamp was invented.** Flame Shield's real `RecoveryTime`,
  `CategoryRecoveryTime` and `Category` are all 0, and nothing would enforce one even if they were
  not: `Spell::SendSpellCooldown` returns early for a non-player caster with its `AddSpellCooldown`
  call commented out. The stamp only ever threw shields away. The guard is now just "is the shield
  already up".
  **The cast waits for the beam.** The shield lasts `1 s + 1 s per combo point` and takes the whole
  bank whatever size it is, so firing at the fixate is the worst of both worlds: at two points or
  fewer it has expired before the first tick, and at five it has just spent a Life Burst or an Engulf
  on cover that three would have bought. `DrakeSurgeShieldAction` latches when the fixate was first
  seen and casts at `SURGE_BEAM_END_MS − (1 + cp)` seconds, clamped to the beam itself — immediately
  at five points, a second before the beam at three, at the beam at two or fewer. While it waits it
  holds off entirely above `DRAKE_SHIELD_MAX_COMBO` (3), so the rotation below can spend the bank on
  something worth more and rebuild a point or two; once the beam is landing it fires regardless, since
  even a short shield eats whole ticks. It also yields on an empty bank: a finisher with no combo
  points is `SPELL_FAILED_NO_COMBO_POINTS`, and `CastVehicleSpell` would have reported that as a
  success.
  **The rotation reserves the energy for it.** Shields still went missing in testing, and the reason
  was the bar rather than the timing: a dps drake's cycle costs `10N + 50` against a 10/s regen, so
  it lives near empty, and a fixated drake that kept spiking or bursting through the three seconds
  reached the beam without the shield's 25. Both rotations now branch on `IsDrakeSurgeTarget` and run
  the same three-step script — spend the bank on Engulf or Life Burst *only* while
  `DrakeCanAffordWithShield` says the bar covers the finisher and the shield both, rebuild to
  `DRAKE_SHIELD_RESERVE_COMBO`, then stop casting and let the bar climb. The one point the shield
  cannot do without is worth going under the energy reserve for, so a drake at zero combo always
  spikes.
  **The reserve is one point, not two.** Two would cover the whole beam rather than its first two
  seconds — at one point the last two ticks land at full price, 24,000 of the 72,000 — but the bar
  will not pay for it. Banking the second point is another 10 energy off a drake already spending
  faster than it regenerates, and testing had fixated drakes reaching the beam with the points but
  not the shield's 25. A short shield beats no shield by 43,200.
  A fixated dps drake dumps the bank at `DRAKE_ENGULF_SURGE_COMBO` (2) rather than the rotation's
  usual 3, because holding a two-point bank for the shield wastes it. It will not dump at one: that
  refreshes the stack for `2 + 20 · 1/5` = 6 s, shorter than the cycle that rebuilds it, so the
  stack falls off and everything the drake has already put into it is gone.
  The latch spots a new fixate as a `DRAKE_FIXATE_GAP_MS` (2 s) gap
  in the trigger, so a drake the boss picks twice running reads as one long fixate and shields once
  for both — rare enough at one victim per 7 s cycle to be worth the simplicity.
  **It is safe to fire mid-dodge**, which matters because Static Field lands *on* the flight. Being
  a self-cast skips the branch that turns the vehicle onto a target; the branch that stops the
  vehicle dead is skipped only because the shield is instant. A Skytalon's seat (2200, `Flags`
  0x62110817) does carry `VEHICLE_SEAT_FLAG_CAN_CONTROL`, so any drake spell given a cast time
  would halt the dodge. The action also returns false either way,
  so `eoe fly drake` — directly below it at `ACTION_EMERGENCY` — still gets the tick and the dodge
  spline is not left half-flown.

**`EoEDrakeAttackAction` and `DrakeSurgeShieldAction` are plain `Action`, not `MovementAction`**, so
the P3 movement suppression leaves them free. `EoEFlyDrakeAction` *is* a `MovementAction` and is the
one thing the suppression names as an exemption — it is the only owner of the vehicle's position.

**Nothing but the EoE actions may move a disk rider.** The P2 multiplier zeroes every
`MovementAction` and `CastReachTargetSpellAction` for a bot in a vehicle. A rider's chase actions
steer the *disk* — which dove ~25 yd the moment it parked next to a Scion, then climbed back up,
over and over. `ReachCombatTo` passing its endpoint through `UpdateAllowedPositionZ` was blamed for
this, but that does not hold: on map 616 that call answers 0.0 for a non-flying unit (a 266 yd drop,
not 25) and is inert for a flying one. **Open gap**, though the lockout fixes it either way.
**`AttackAction` derives from `MovementAction`**, so the
exemption list has to name `MalygosRideDiskAction` and `MalygosTargetAction` explicitly or the disk
riders board and then sit there doing nothing. `LeaveVehicleAction` is exempt as a manual override.

## Cost

This strategy is cheap per bot and expensive per raid: twenty-five bots on one small platform were
all asking the same questions every tick. Two things dominated — grid searches, which walk every
cell (`SIZE_OF_GRID_CELL`, 66.67 yd a side) inside their radius, and the role lookups behind the
multiplier, where `IsMainTank` walks every group member and each check scans that member's strategy
list.

- **One creature cache for the whole instance.** `GetEoECreatures` / `GetNearestEoECreature` /
  `AnyEoECreature` (`EoETriggers.cpp`) answer from a `thread_local` map keyed by instance id and
  creature entry, refilled at most every `EOE_CREATURE_CACHE_MS` (300 ms). Before this, phase 2 cost
  eight to ten sweeps per bot per tick — the bubble trigger and the free-disk trigger duplicated each
  other outright — for about 250 sweeps a tick that all returned the same answer. The fill sweep uses
  `EOE_CACHE_SWEEP_RADIUS` (200 yd) rather than something tight: it is anchored on whichever bot
  happened to refresh it but its answer goes to the whole raid, so it has to reach every creature
  from anywhere a bot can be. **Guids are cached, not pointers** — a creature that despawns inside the
  window drops out of the answer instead of coming back as a dangling read. Bots are only ever
  updated from their own map's thread, so no locking.
- **`getPhase` memoises per instance**, not per bot, for 500 ms (`EOE_PHASE_CACHE_MS`). Riding a
  Skytalon is the one part of the answer that differs between bots, so that check runs first and
  uncached; it is a pointer read. A phase cannot turn over inside a tick, and half a second of lag on
  a transition only affects which overrides are on.
- `getMalygos` asks the instance script (`GetCreature(DATA_MALYGOS)`, an O(1) guid lookup) before
  falling back to a search. `EOE_DATA_MALYGOS` mirrors the core's `Data` enum, which modules cannot
  include.
- **`MalygosMultiplier` snapshots the bot's roles** for 500 ms instead of re-deriving them per
  action. It runs once per queued action per bot per tick, so `IsMainTank` alone was tens of thousands
  of strategy-list scans a tick across a raid. The phase deliberately stays out of the snapshot —
  `getPhase` has its own window, and stacking a second one on top would leave the multiplier applying
  the previous phase's rules for up to a second after the actions had moved on.
- **The multiplier splits on action family before testing anything.** Everything it suppresses is
  either a `MovementAction` or a `CastSpellAction`, and the two are disjoint, so one `dynamic_cast`
  each way up front means a plain rotation cast pays two instead of walking a list of thirteen.
- **Triggers that gate a multi-tick reaction do not check every tick.** `power spark`,
  `malygos bubble` and `surge of power` run at 200 ms, `malygos free disk` at 300 ms (`Trigger`'s
  `checkInterval`). Each of them starts a walk, a boarding or a peel that the MotionMaster carries on
  with, so a fraction of a second of latency is invisible. The `malygos`, `malygos on disk` and
  `malygos drake flight` triggers stay per-tick: they gate positioning and vehicle steering, which
  have to re-issue.

## Fragile by design

`drake combat` is borrowed from the **Oculus dungeon**, not EoE-defined. It does resolve —
`WotlkDungeonOccTriggerContext` is registered in `BuildSharedTriggerContexts.cpp:75`, so the missing
entry in `RaidEoETriggerContext` is not a bug, do not re-audit it.

`group flying` used to be borrowed the same way and is not any more. `GroupFlyingTrigger` checks
`master->GetVehicleBase() && bot->GetVehicleBase()`, so a human raid leader who had not taken a drake
yet left the whole flight unable to fly-follow. That mattered more once `EoEFlyDrakeAction` took over
parking and facing as well, so P3 now uses its own `malygos drake flight` — the bot is on a Wyrmrest
Skytalon, full stop. `EnterVehicleAction` is IOC-scoped and does not help with the mount itself.

## Threat redirect

Three phases, three different right answers, which is why the generic main-tank node is not simply
vetoed here. See the per-raid verdicts in [ulduar.md](ulduar.md), which shares the same
investigation.
