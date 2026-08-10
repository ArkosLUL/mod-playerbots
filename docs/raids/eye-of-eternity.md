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

## Per-phase behaviour

- **P1 — Power Sparks.** Previously fully stubbed: the trigger was registered but never bound, the DK
  death-grip pull was commented out, the ranged target-switch was commented out, and
  `KillPowerSparkAction` had no body and no registration. Sparks reaching Malygos stack a
  damage + haste buff (56152) into a soft enrage. Now: `power spark` bound, DK `pull power spark`
  at +3, ranged DPS `kill power spark` at +2, and ranged also switch in `malygos target`. Tanks and
  melee never peel, so the boss's cone does not swing. **The DK walks out to grip, then walks back.**
  Death Grip lands the spark *on the caster*, and a killed spark leaves
  `SPELL_POWER_SPARK_GROUND_BUFF` (55852) on its corpse for a minute — so where the DK stands decides
  who gets that buff, and gripping from the melee stack both wastes it and drops the spark inside the
  12 yd at which `npc_power_spark` hands its buff to Malygos instead.
  `POWER_SPARK_GRIP_POSITION` is the midpoint of the melee and ranged stacks (`y = 1300.27`), which is
  the best available spot without knowing 55852's radius — it maximises the smaller of the two
  distances. It sits ~21 yd from where Malygos parks, so a spark dropped there still has 9 yd to walk
  before it could reach him, against ~12k hp and the whole raid.
  The split of duties matters: **`MalygosPositionAction` owns all the walking, `PullPowerSparkAction`
  only ever casts.** Both read `IsOnPowerSparkGripDuty`, so they cannot disagree about where the DK
  should be. Duty needs the grip off cooldown (the cooldown outlasts the gap between spawns, so there
  is no point giving up boss uptime otherwise), a spark within
  `POWER_SPARK_GRIP_ENGAGE_RADIUS` (45 yd) of the spot, and Malygos no closer to the spot than
  `POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE`. Casting the grip ends duty, and the position action walks the
  DK back to the melee stack on the next tick.
- **P1 — the hold spots are taken during the pull intro, before the boss lands.** `JustEngagedWith`
  sends Malygos on an intro circuit and only then drops him at `CenterPos.z`, 35 yd out from centre
  on whatever heading he was circling; `EVENT_START_FIGHT` clears his flags and he chases
  `SelectNearestTarget(250)`. That whole stretch reads as P4, and the P4 centre gather pulls the tank
  *off* his spot — it sits 42 yd out, outside the 30 yd ring. `MalygosPositionAction` runs the P1
  branch through the intro instead, so the tank is parked and facing when the boss touches down.
  During the intro only the raid's assigned main tank counts as the tank: Malygos is pacified, and
  his victim is nothing more than whoever pulled.
- **P1 — three fixed hold spots on one north/south line.** Arcane Breath (56272) is a frontal cone
  on the boss's *current victim*, so the fight is won or lost on where the tank stands. All three
  spots are absolute constants, never recomputed from the boss's live position: a boss-relative spot
  flips to his far side while he is still walking out and sweeps the cone through the raid. The one
  exception is under the stack spot below, and it is a clamp rather than a chase. Whoever
  Malygos is actually hitting behaves as the tank, assigned or not. A bot corrects only past
  `MALYGOS_P1_POSITION_TOLERANCE` (5 yd).
  - `MALYGOS_MAINTANK_POSITION` `y = 1343.27`, 42 yd due north of centre — the Exit Portal at
    43.4 yd proves there is ground there. Malygos' CombatReach of 20 parks him ~21.5 yd short, at
    roughly `y = 1322`.
  - `MALYGOS_STACK_POSITION` `y = 1313.27` — melee and healers. 30 yd from the tank, inside 40 yd
    heal range, and south of where the boss actually stops, so out of the cone. That last part only
    holds when he walks in from the south. He lands 35 yd from centre on whatever bearing his intro
    circuit left him on, then stops wherever his chase first brings him inside melee range of the
    tank, so an approach from the side parks him ~38 yd from the stack with the melee half of the
    raid standing there swinging at nothing. So the stack spot — and only the stack spot — is
    **clamped**: further than `MALYGOS_MELEE_HOLD_DISTANCE` (15 yd) from him and it slides up the
    line towards him until it is that close. Melee range against him is ~22.8 yd (his 20 yd
    CombatReach, the player's own reach, plus the 4/3 the core adds) and a bot may park 5 yd off its
    spot, so 15 swings with margin. The clamp keeps the bearing the stack already holds from him, so
    it can never land in front of him; it moves continuously with him rather than switching between
    two spots, which is what would set the raid bouncing; and it is off during the pull intro, when
    he is circling and untouchable anyway. In a normal pull he ends up 7–12 yd from the stack and the
    clamp never fires.
  - `MALYGOS_RANGED_POSITION` `y = 1287.27` — ranged DPS, ~34.5 yd from the boss. **The stack spot
    is unusable for ranged.** `Spell::CheckRange` adds `GetMeleeRange` to a spell's minimum for
    `SPELL_RANGE_RANGED`, so Malygos' CombatReach of 20 inflates a hunter's 5 yd minimum to ~28 yd
    of centre-to-centre distance and every shot came back `SPELL_FAILED_TOO_CLOSE`. The same reach
    keeps `EnemyTooCloseForSpellTrigger` (threshold ~23.5 yd) permanently active, and every class
    wires that trigger to an escape at 34–50 relevance — above `malygos position` at `ACTION_MOVE`.
    Bots stepped out, were dragged back next tick and never finished a cast. The P1 multiplier now
    also zeroes `FleeAction`, `RunAwayAction`, `CastBlinkBackAction` and `CastDisengageAction` for
    anyone in the encounter.
  Plus `POWER_SPARK_GRIP_POSITION` `y = 1300.27`, held only by a DK on spark duty — see the Power
  Spark bullet above.

  **Nobody but the boss's current victim walks anywhere in P1.** The multiplier zeroes every
  `MovementAction` and `CastReachTargetSpellAction` for everyone else, naming
  `MalygosPositionAction`, `MalygosTargetAction`, `KillPowerSparkAction` and
  `ReachPartyMemberToHealAction` as the exemptions. Two separate symptoms, one cause: ranged were
  chasing Power Sparks back inside Malygos' minimum range (the DK grips sparks to them instead), and
  melee were walking out to get behind him (`set behind`, `ACTION_MOVE + 7`) or to spread
  (`combat formation move`) and being dragged back to the stack next tick — the shuffling that shows
  up in game. The hold spots need no help: they are inside his 20 yd combat reach for melee and
  outside the inflated minimum for ranged. `SetFacingTargetAction` is a plain `Action`, so facing
  still works; `AttackAction` is not, which is why the EoE attack actions have to be named.

  There is no `avoid arcane breath` action: the cone points north at the tank and every other spot
  is behind it.
- **P2 — the Arcane Overload bubbles are shelter, and the old code ran the wrong way.**
  `NPC_ARCANE_OVERLOAD` (30282) grants **56438, −50% damage taken**; the protected radius shrinks
  ~2 % per tick over the bubble's 45 s life, so bots hug the centre within 4 yd and ignore any bubble
  already below `BUBBLE_MIN_USABLE_FACTOR` (35 %) of its original radius. The bubble NPC is
  non-attackable, so it never shows in `"possible targets"` — scan with
  `GetCreatureListWithEntryInGrid`. `malygos seek bubble` sits at `ACTION_EMERGENCY + 2`, above
  `avoid surge of power`, which is now only the fallback for bots that cannot reach one. The bubble
  assignment is **latched by GUID** (the `SapphironFlightPositionAction` idiom) and spread by group
  slot index, or bots hop between bubbles as they shrink. Once the bot holds 56438 the action returns
  false so the rotation runs, and the phase-2 multiplier zeroes reach/chase/follow for non-vehicle
  bots so nobody walks back out; `MalygosTargetAction` only accepts a Nexus Lord / Scion inside
  `spellDistance` by **`IsWithinCombatRange`**, the same 3d combat-reach test `Spell::CheckRange`
  runs — the Scions hover 20–30 yd up, and the old flat 2d distance claimed a reach that was not
  there. Nexus Lords come first for everyone, including ranged DPS: they are on the
  ground, can be tanked and die faster. Scions are what ranged fall back to once no Lord is in reach. Anti-fall recenter in
  `malygos position` still applies, and it bails immediately for anyone in a vehicle. Note the old
  code suppressed `FleeAction` wholesale in P2, removing the one instinct that might have pulled a
  bot back from the edge.
- **P2 hover disks — melee only.** When a Nexus Lord dies the core lands its disk (30248), turns it
  friendly and clears `UNIT_FLAG_NOT_SELECTABLE`, so *landed + selectable + free seat* doubles as
  "its rider is dead". Melee DPS board it (`EnterVehicleAction`, which uses `HandleSpellClick` —
  required, the mount is `npc_spellclick_spells` 61421) and ride it to the Scions of Eternity, which
  hover ~30 yd out and +20 yd up and are otherwise unreachable in melee. Passengers are immune to
  both Arcane Overload and Surge of Power, which is why tanks, ranged and healers stay in bubbles
  instead. `MalygosRideDiskAction` steers the vehicle's own `MotionMaster`, with a
  `POINT_MOTION_TYPE` anti-stutter guard. Its `MovePoint` calls pass `generatePath = false`: the
  mmap is 2d, so a generated path drops the destination onto the platform and the disk dives instead
  of flying. Scion altitude is stable — the core floors a Scion disk's descent at `CenterPos.z + 20`
  (`boss_malygos.cpp`, `MI_POINT_SCION`) — so a fixed target Z is safe.
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
  Skytalon, harmless because it stands down the moment the drake is parked.
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
  `GetDrakeStackPoint` folds the hazard into the park spot instead: if the anchor is inside
  `STATIC_FIELD_CLEARANCE` of any field, it slides **around the boss** to another point on the
  `DRAKE_STACK_RADIUS` ring, and the first heading that clears wins. Boxed in on every heading, it
  takes the roomiest one rather than sit in the field — the fields expire on their own. Staying on
  the ring is the point: an earlier version hopped 32 yd straight off the anchor toward whatever had
  the most clearance, and boss-ward was often that direction, which put the flight inside Arcane
  Pulse and killed it faster than the field would have.
  Two details matter as much as the ring itself. **The slide always goes the same way around it** —
  sweeping both ways picked a nearer spot, but when the next field landed on that spot the answer
  flipped to the far side of the anchor, i.e. back across the field the flight had only just
  dodged. One-way, a second field can only push the flight further along. And **the clearance is
  `STATIC_FIELD_SAFE_RADIUS` (32 yd) plus the stack tolerance**, because the tolerance is exactly how
  far off the point a drake is allowed to park: a spot 32 yd from a field still left whoever stopped
  on the field side of it standing in the pulse, which is why two or three of the flight took damage
  from a field the stack had supposedly cleared.
  Resolving the point is cheap now the creature lookup is cached, so the flight action only holds it
  for `DRAKE_STACK_RECALC_MS` (300 ms) — a field lands *on* the flight, so a stale answer here is
  damage taken.
  It also only re-issues `MovePoint` when the destination has actually moved more than 2 yd or the
  mover has stopped: restamping the same destination every tick restarts the spline and the drake
  crawls without ever arriving.
- **P3 — the healer split is a raid-size call, not a spec one.** Every drake carries the same
  spellbook, so `IsDrakeHealer` caps *and* floors: `DRAKE_HEALERS_25MAN` (5) / `DRAKE_HEALERS_10MAN`
  (2), filled from the bots flagged `IsHeal` in guid order and topped up from the dps if the raid
  brought fewer. The old version only had a floor, so a heal-heavy raid put seven drakes on Revivify
  and ran out of phase. Guid order is identical on every bot, so the flight agrees without talking.
- **P3 — drake healers only ever cast on themselves.** Revivify is a HoT and each cast banks a combo
  point; Life Burst spends five of them as a heal centred on the caster. Both go on the healer's own
  drake: a `Unit` holds combo points for one target at a time, so an earlier version that chased
  whoever was lowest reset the count to one on every switch and Life Burst was unreachable. With the
  flight stacked, a self-cast Life Burst covers the same drakes a targeted one would. Neither goes
  through `CanCastVehicleSpell` — it reports `BAD_TARGETS` on a drake. The healer branch runs before
  the boss lookup in `EoEDrakeAttackAction::Execute`, since a healer needs no boss at all.
- **P3 — Surge of Power cannot be dodged, and the old trigger could never fire.** The boss picks its
  victims, then fires the damage as a **triggered instant 3 s later** (`me->m_Events.AddEventAtOffset`
  → `DoCastAOE`): no beam to walk out of, no cast to outrun. Flame Shield (57108) halves it and that
  is the entire reaction; everything it does not cover is a heal check. The strafe and Blazing Speed
  peel the action used to do were both pointless and are gone, along with the self-imposed rate limit
  they needed.
  The trigger could not fire at all before: both P3 surges are `DoCastAOE` with no unit target, so
  `m_targets.GetUnitTargetGUID()` was always empty and Flame Shield never went up once. The boss AI
  publishes its victims in its own guid slots instead (`DATA_FIRST_SURGE_TARGET_GUID = 14`, three
  slots, mirrored as `EOE_DATA_FIRST_SURGE_TARGET_GUID`), filled by the warning selector 3 s ahead,
  and it does so for the 25-man three-target version as well. The slots are not cleared until the
  next surge is picked, so the trigger stays hot for most of the 7 s between casts — harmless now
  that the action only pops a 30 s cooldown and returns false when it is down.

**`EoEDrakeAttackAction` and `DrakeSurgeShieldAction` are plain `Action`, not `MovementAction`**, so
the P3 movement suppression leaves them free. `EoEFlyDrakeAction` *is* a `MovementAction` and is the
one thing the suppression names as an exemption — it is the only owner of the vehicle's position.

**Nothing but the EoE actions may move a disk rider.** The P2 multiplier zeroes every
`MovementAction` and `CastReachTargetSpellAction` for a bot in a vehicle. A rider's chase actions
steer the *disk*, and `ReachCombatTo` runs its endpoint through `UpdateAllowedPositionZ`, which
clamps Z to the platform floor — the disk dove 25 yd to the ground the moment it parked next to a
Scion, then climbed back up, over and over. **`AttackAction` derives from `MovementAction`**, so the
exemption list has to name `MalygosRideDiskAction` and `MalygosTargetAction` explicitly or the disk
riders board and then sit there doing nothing. `LeaveVehicleAction` is exempt as a manual override.

## Cost

This strategy is cheap per bot and expensive per raid: twenty-five bots on one small platform were
all asking the same questions every tick. Two things dominated — grid searches, which walk every
cell (33 yd a side) inside their radius, and the role lookups behind the multiplier, where
`IsMainTank` walks every group member and each check scans that member's strategy list.

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
