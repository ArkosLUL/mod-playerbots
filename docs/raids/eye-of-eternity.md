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
  `57407` (10) / `60936` (25); Arcane Pulse `57432` (unavoidable); Vortex `56105` (fully
  server-driven, no bot action). Centre anchor `{754.395, 1301.27, 266.1}`.

## Phase model

P1 = Malygos attackable and no adds. P2 = disc adds (`NPC_NEXUS_LORD` / `NPC_SCION_OF_ETERNITY`) up.
P3 = on a Wyrmrest Skytalon (30161). **P4 = transition** — in combat, Malygos non-attackable, not
mounted.

P4 exists because of a real failure: during P2→P3 the boss is untargetable and the bot is not yet on
its drake, so the phase resolved to 0, `MalygosMultiplier` lifted every override, and **the default
raid strategy resumed mid-transition on a collapsing platform**. P4 holds the raid at centre and
keeps the EoE overrides on.

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
- **P1 — three fixed hold spots on one north/south line.** Arcane Breath (56272) is a frontal cone
  on the boss's *current victim*, so the fight is won or lost on where the tank stands. All three
  spots are absolute constants, never recomputed from the boss's live position: a boss-relative spot
  flips to his far side while he is still walking out and sweeps the cone through the raid. Whoever
  Malygos is actually hitting behaves as the tank, assigned or not. A bot corrects only past
  `MALYGOS_P1_POSITION_TOLERANCE` (5 yd).
  - `MALYGOS_MAINTANK_POSITION` `y = 1343.27`, 42 yd due north of centre — the Exit Portal at
    43.4 yd proves there is ground there. Malygos' CombatReach of 20 parks him ~21.5 yd short, at
    roughly `y = 1322`.
  - `MALYGOS_STACK_POSITION` `y = 1313.27` — melee and healers. 30 yd from the tank, inside 40 yd
    heal range, and south of where the boss actually stops, so out of the cone.
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
  of its P2 branch, before every early return, so a disk rider's ghoul is covered too. The grid
  sweep is gated on the bot actually having a pet — otherwise most of the raid paid for it every tick
  to learn it had nothing to redirect.
- **P2 — mages steal the Nexus Lords' Haste.** `npc_nexus_lord` self-casts `SPELL_HASTE` (57060)
  every 20–30 s. `malygos spellsteal` takes it off the nearest Lord already inside `spellDistance`,
  by `IsWithinCombatRange`, so nobody leaves a bubble for it. It never touches the bot's target, and
  it hangs off the `malygos` trigger rather than one of its own — the action gates itself on class
  and phase, so it costs nothing outside P2. The generic `SpellstealTrigger` only looks at the
  current target, which is a Scion as often as not.
- **P3 — Static Field.** Malygos casts 57430 on a random raider every 12 s; it summons a
  **stationary** `NPC_STATIC_FIELD` that pulses for its full 20 s life, so two are often up at once.
  It cannot be pre-dodged — it lands on someone — but it can be walked out of, and three things
  stopped that working:
  - The avoid action bailed out (`return true`, tick consumed) whenever the drake's MotionMaster was
    running *any* `POINT_MOTION_TYPE` move. That guard was meant to let its own escape finish, but it
    could not tell that from `DrakeDpsAction`'s range-close, so a drake mid-approach sat in the field
    for the whole flight. It now latches its own destination and re-routes if a field lands on it.
  - It fled the *nearest* field only, straight into the other one. It now clears **every** field in
    range: sample 16 headings at the shortest workable hop, take the one with the most clearance.
  - Nothing stopped the drake flying back in. `EoEFlyDrakeAction` is now zeroed by the multiplier
    while a field is within `STATIC_FIELD_DANGER_RADIUS`, and `DrakeDpsAction` skips its range-close
    on the same condition. Break-off is at 20 yd, all-clear at 32 yd — a smaller gap re-triggers the
    moment the drake arrives.
- **P3 — drakes hold a ring around Malygos; they do not follow anybody.** `EoEFlyDrakeAction` used
  to `MoveFollow` the raid leader's drake, which left every drake permanently in motion. A moving
  vehicle cannot hold a facing, and `DrakeDpsAction` fought it for the same MotionMaster: its
  range-close called `MoveForwards`, whose endpoint goes through
  `CanReachPositionAndGetValidCoords` — which has no answer for a point in mid-air, so it silently
  did nothing *after* `mm->Clear(false)` had already wiped the follow. The drake stopped dead,
  out of range, and never fired again. Now the flight action owns positioning outright: a ring slot
  by group slot index at `DRAKE_RING_RADIUS` (40 yd) around the boss, a straight 3d spline to get
  there (`generatePath = false`, same reason as the hover disks), then `MoveIdle` plus
  `SetFacingToObject(boss)` and a `return false` handing the tick to the rotation. Its relevance is
  raised above `eoe drake attack` (`ACTION_NORMAL + 6` vs `+ 5`) precisely so it settles first.
  `DrakeDpsAction` no longer moves at all. Healers are not pinned facing the boss — `CastVehicleSpell`
  turns the vehicle onto its target, and a healer's target is another drake.
- **P3 — the healer split is a raid-size call, not a spec one.** Every drake carries the same
  spellbook, so `IsDrakeHealer` caps *and* floors: `DRAKE_HEALERS_25MAN` (5) / `DRAKE_HEALERS_10MAN`
  (2), filled from the bots flagged `IsHeal` in guid order and topped up from the dps if the raid
  brought fewer. The old version only had a floor, so a heal-heavy raid put seven drakes on Revivify
  and ran out of phase. Guid order is identical on every bot, so the flight agrees without talking.
- **P3 — Revivify's combo point lands on the drake it healed, not on the caster.** `Unit` holds combo
  points for one target at a time, so `DrakeHealAction` hopping to the current most-injured drake
  every tick reset the count to one and Life Burst was unreachable. It now latches a heal target
  until that drake is topped off or gone, reads the raw `GetComboPoints()`, and aims Life Burst at
  the *combo* target — fired at itself the finisher would find nothing to spend. With nobody hurt it
  banks points on its own drake, ready for the next Arcane Pulse.
- **P3 — Surge of Power fixate: the old trigger could never fire.** Both P3 surges are `DoCastAOE`
  with no unit target, so `m_targets.GetUnitTargetGUID()` was always empty and `drake dodge surge`
  never ran once — which is why nobody used Flame Shield. The boss AI publishes its victims in its
  own guid slots instead (`DATA_FIRST_SURGE_TARGET_GUID = 14`, three slots, mirrored as
  `EOE_DATA_FIRST_SURGE_TARGET_GUID`), and it fills them **3 s before the beam** via the warning
  selector, so reading them there also buys the whole reaction window. It publishes for the 25-man
  three-target version as well. Note the slots are only cleared when the next surge is picked, so the
  trigger stays hot for most of the 7 s between casts: the action shields and peels once, then
  rate-limits itself with `DRAKE_SURGE_DODGE_COOLDOWN_MS` (5 s) and hands the ticks back, or a victim
  would strafe non-stop and never attack.

**Drake avoid and attack actions are plain `Action`, not `MovementAction`**, so the P3 movement
suppression leaves them free.

**Nothing but the EoE actions may move a disk rider.** The P2 multiplier zeroes every
`MovementAction` and `CastReachTargetSpellAction` for a bot in a vehicle. A rider's chase actions
steer the *disk*, and `ReachCombatTo` runs its endpoint through `UpdateAllowedPositionZ`, which
clamps Z to the platform floor — the disk dove 25 yd to the ground the moment it parked next to a
Scion, then climbed back up, over and over. **`AttackAction` derives from `MovementAction`**, so the
exemption list has to name `MalygosRideDiskAction` and `MalygosTargetAction` explicitly or the disk
riders board and then sit there doing nothing. `LeaveVehicleAction` is exempt as a manual override.

## Cost

`MalygosTrigger::getPhase` is the hot path: eight triggers call it per bot per tick, and
`MalygosMultiplier::GetValue` called it **once per action**, which at a few dozen actions a bot came
to thousands of grid sweeps a tick across a 25-man raid. A grid search walks every cell (33 yd a
side) inside its radius, so a 250 yd sweep visits on the order of 15×15 cells.

- **`getPhase` memoises itself** for 500 ms (`EOE_PHASE_CACHE_MS`), keyed by bot guid in a
  `thread_local` map — a bot is only ever updated from its own map's thread, so no locking. The cache
  used to live in the multiplier, which left the eight triggers paying full price. A phase cannot
  turn over inside a tick, and half a second of lag on a transition only affects which overrides are
  on.
- `getMalygos` asks the instance script (`GetCreature(DATA_MALYGOS)`, an O(1) guid lookup) before
  falling back to a search. `EOE_DATA_MALYGOS` mirrors the core's `Data` enum, which modules cannot
  include.
- Add lookups use `EOE_ADD_SEARCH_RADIUS` (100 yd), not 250. Everything in this fight is on or just
  above a platform under 50 yd across.

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
