# Lessons a boss strategy will otherwise re-derive

Engine-level knowledge that cost a debugging session on one encounter and applies to all of them.
Encounter facts live in [../raids/](../raids/); the silent-failure catalogue is
[pitfalls.md](pitfalls.md); the selection loop is [action-selection.md](action-selection.md). Case
studies here are named in a clause and linked, not retold.

## Vehicles and flight

Nothing about a bot on a vehicle behaves like a bot on the ground. Every rule below has drawn blood.

- **A vehicle has exactly one owner of its position.** Two actions steering the same `MotionMaster`
  produce a visible bounce, not a compromise: each undoes the other every tick. The fix is deleting
  one, never lowering its priority.
- **A vehicle summoned as its rider's minion** (SummonProperties category vehicle) is dismissed by
  `Unit::_ExitVehicle` when the rider leaves, which frees a one-at-a-time source for the next driver
  (Putricide's abomination table).
- **A vehicle rider never counts as flying.** `Player::CanFly` reads only the bot's own
  `MOVEMENTFLAG_CAN_FLY`, which `UpdateMovementState` strips without a flight aura. So `MoveTo` /
  `ReachCombatTo` (via `bot->UpdateAllowedPositionZ`) or `CheckCollisionAndGetValidCoords` run on
  the bot clamp a drake's destination to terrain — on an airborne rider a dive to the floor and a
  climb back, on repeat — and the bot's own `MoveFollow` moves nothing. A strategy that keeps a bot
  on a flying vehicle keeps generic movement off it and steers the vehicle base itself.
- **Anything airborne needs `MovePoint(..., generatePath = false)`.** The mmap is 2d, so a generated
  path drops the destination onto the ground underneath. `false` gives a straight 3d spline. On a map
  with no `.mmtile` files the setting is inert — `CalculatePath` returns a two-point shortcut and
  `PointMovementGenerator` only honours a generated path above two points — but pass it anyway, so
  the behaviour does not change if tiles appear later.
- **A straight spline is a chord.** Sliding `Δ` around a ring of radius `R` passes within
  `R·cos(Δ/2)` of the centre, so a single `MovePoint` "around" a hazard flies through it. Break the
  trip into hops sized against the hazard: on a 45 yd ring against a 30 yd radius, 60° hops clear by
  9 yd, 90° by under 2, and 180° goes straight through.
- **Re-issuing `MovePoint` with the same destination restarts the spline**, so a destination stamped
  every tick crawls and never arrives. Gate on the goal having actually moved, or the mover having
  stopped.
- **A finished `MovePoint` leaves an idle generator**, so
  `GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE` reads as "arrived" — good for chaining
  legs, useless as an "am I busy" test, because it cannot say *whose* move is running.
- **`MotionMaster::MoveForwards` has no answer for a mid-air point.** Its endpoint goes through
  `CanReachPositionAndGetValidCoords` and it returns silently — after `mm->Clear(false)` has already
  wiped what the mover was doing. The rider stops dead, permanently.
- **A moving vehicle can neither cast nor hold a facing.** `CanCastVehicleSpell` rejects on
  `CastingTime && vehicleBase->isMoving()`; `CastVehicleSpell` cancels when
  `seat->CanControl() && isMoving() && GetCastTime()`. A rider left on `MoveFollow` fires nothing all
  phase.
- **`PlayerbotAI::CastVehicleSpell` returns `true` even when `prepare()`'s `CheckCast` failed.** No
  power, no combo points (`SPELL_FAILED_NO_COMBO_POINTS`) and GCD collisions all look like success,
  so anything stamped on that answer records a cast that never happened: Flame Leviathan's
  `CastVehicleSelfSpell` stamped Steam Rush's 15 s cooldown on a refusal from the Ram GCD, and a Pursued siege engine went 15.2 s
  without its escape. **Confirm with a receipt** — energy spent, the vehicle's own power moved (an
  ICC gunship cannon's heat), the aura landed, the channel stopped. `Spell::prepare` calls `cast(true)` inline for instants, so `HasAura` on the same tick is
  real.
- **`CanCastVehicleSpell` reports `BAD_TARGETS` for a self-cast**, so the honest check is unavailable
  exactly where it is needed. Substitute `SpellInfo::CalcPowerCost` against live power, guarding
  `PowerType >= MAX_POWERS` first — `POWER_HEALTH` is `-2` and would index the power array out of
  bounds, and `Powers` has a signed underlying type, so cast `MAX_POWERS`.
- **Creature casters get no server-side cooldown.** `Spell::SendSpellCooldown` returns early for
  non-players with its `AddSpellCooldown` commented out. Stamp one only as the spell's own
  `RecoveryTime`/`CategoryRecoveryTime` on a cast that returned `SPELL_CAST_OK`
  (`CastPetAcidSpitAction`): an invented duration, or one stamped on an unconfirmed cast, silently
  throws casts away for the rest of the fight.
- **On a vehicle a bot drives, `AttackAction` refuses to attack.** Send the swing as a player's
  pet-attack command does, through the vehicle creature's `AI()->AttackStart(target)` (Putricide's
  Mutated Abomination).
- **Vehicle power is creature regen.** `unit_class 4` plus `UNIT_FLAG2_REGENERATE_POWER` gives
  `Creature::Regenerate` a flat +20 per `CREATURE_REGEN_INTERVAL` (2 s) — 10/s against a cap of 100.
  Cost per GCD against that rate decides whether the bar ever climbs at all.
- **A `MotionTransport` keeps DB passengers in `GetStaticPassengers` and summons in
  `GetPassengers`.** Iterate both for an exact unit list with no grid search. A summon moved by an
  effect that only rewrites its transport offset stays its own ship's passenger while it stands on
  the other ship (the ICC gunship boarders).
- **A bot's fly aura sets no flight flags.** `Unit::SetCanFly` only sends a packet to a
  client-controlled unit, which a bot never answers, and `MovementAction::UpdateMovementState` sets
  `CAN_FLY|DISABLE_GRAVITY|FLYING` only while the master flies (or there is none), stripping them on
  the ground under a grounded master. A flight under a grounded human sets the flags itself and calls
  `MovePoint(generatePath = false)` directly, since `MoveTo` strips them first. While they are set,
  `PlayerbotAI::CastSpell` refuses every cast but under the auras `CanCastWhileFlying` lists (Gravity
  Lapse, Dream State).
- **A bot takes no fall damage by itself**: `Player::HandleFall` runs from the client's landing
  packet. A fall the strategy causes uses `MoveFall` plus `HandleFall` with `SetFallInformation`, as
  `CheckMountStateAction::CompleteDismount` does.
- **Do not borrow another instance's vehicle trigger.** `GroupFlyingTrigger` (Oculus) requires the
  *master* to be mounted, so one human who has not taken their vehicle freezes the whole flight.

## Movement that oscillates

Bots jiggling on the spot is one symptom with many causes, plus one that freezes them instead. The
movement lock that decides which of two movers' orders is accepted is in
[pitfalls.md](pitfalls.md#movement-that-silently-no-ops).

- **Two owners** — see above.
- **Two nodes re-deriving their destination every tick.** Every `MoveTo` calls `mm->Clear()`, so a
  high-priority node cancels whatever walk a lower node had in flight, even when its own answer moved
  the bot a yard. Two such nodes alternate at tick rate and neither arrives: tens of yards of travel
  inside a 5 yd strip, which reads in-game as "it refuses to cross" (Freya's Sun Beam dodge and spore
  node, 584 flips in one pull). Latch the destination and, while the walk is in flight and still
  valid, **return true without calling `MoveTo`** — the engine ends the tick at the first action
  returning true, so claiming it is what protects the spline. Give the latch a ceiling, or a bot
  rooted mid-move holds the tick indefinitely.
- **A destination that chases a moving boss.** A boss-relative spot flips to his far side while he is
  still walking. Prefer a spot **latched for the pull**. If it must track him, **clamp, don't
  chase**: keep the bearing the spot already holds, slide only along the segment `[boss, spot]`, and
  keep it continuous — at the clamp distance the clamped point must *equal* the unclamped one. A
  threshold that snaps between two positions is the bouncing mechanism itself.
- **A drag re-planned while the boss walks.** A chasing creature stops on its next 400 ms recheck
  once a still victim is inside its reach, wherever it stands, so a re-plan every tick against a
  tight arrive overshoots and reverses him. Plan one leg on a boss that has stood still, never
  re-plan mid-leg. To meet him face-on the tank walks it backwards (`MoveTo` `backwards = true`), so
  any lead maths uses `MOVE_RUN_BACK`, not `MOVE_RUN`
  ([Anub'arak](../raids/trial-of-the-crusader/anubarak.md)).
- **A spot anchored on a moving unit.** It passes any distance deadband every tick: judge drift only
  once the anchor's own `isMoving()` has been false a short while (500 ms), not by widening the
  deadband. A "victim in melee range" test misses the walk, since a boss chasing a knocked-back tank
  catches it in flight and keeps it mostly in reach.
- **A boss that drifts breaks fixed polar spot sets.** Navprobe an absolute floor grid once offline
  (stored as row bitmasks) and pick a live node per bot under the fight's constraints
  ([Sindragosa](../raids/icecrown-citadel/sindragosa.md)'s heroic Unchained Magic carriers).
- **A stand that yields must stop the bot.** The spline already issued keeps it walking, so the
  stand's mover calls `StopMoving` and clears the `last movement` booking, or the bot walks on into
  the hazard.
- **A stand held round a boss who moves off.** Hold a boss-relative stand only while he is within a
  few yards of where it latched; past that, release it so reach follows him, or he drags stale stands
  along ([Lord Marrowgar](../raids/icecrown-citadel/lord-marrowgar.md)).
- **A hazard sweep that restarts from a fixed base.** "First clear heading from base" re-finds ground
  *behind* the formation the moment a hazard back there expires, and the whole formation reverses.
  Latch the heading and sweep **forward only** from where it already is; an expiring hazard behind
  then cannot be selected. Hazards that outlive their own cadence guarantee this — 20 s fields on a
  12 s timer reversed the EoE flight on roughly every third one.
- **A generic mover fighting a boss action.** The suppression multiplier in
  [../raids/README.md](../raids/README.md) is the right shape, but `AttackAction` **is** a
  `MovementAction` ([action-selection.md](action-selection.md)), so the exemption list must name the
  boss's own attack and vehicle actions or they die with everything else.
- **A dodge that steps further than the hold's arrival tolerance.** The hold's trigger fires the
  moment the dodge finishes, walks the bot back onto the hazard, and the two alternate for the
  hazard's whole life. Either keep the step smaller than the tolerance, or teach the hold to reject a
  destination the hazard covers. Obsidian Sanctum's fissure dodge steps 10 yd against a raid-line
  tolerance of 8 and needed the second fix.
- **A hold that lets go on arrival.** Latching only the walk hands the next tick to the generic
  movers (`reach`, `set behind`, `follow`), which re-plan from where the bot stands and walk it off:
  A-B-A. Latch the spot itself, split by edge (park within 1 yd, walk back only past 2.5), and block
  those movers while it holds. Release a spot that ignores reach (an escape) on arrival instead, or
  the block strands the bot out of range. The
  [Northrend Beasts](../raids/trial-of-the-crusader/northrend-beasts.md) worm stage needed both.
- **One hazard owned by a planner and the forced dodge.** The dodge pulls the bot out of the leg the
  planner walks it back into. Give each hazard one owner per bot state (Rotface's towing kiter: floods
  belong to the kite legs, not its dodge).
- **A walk toward a cooldown-gated action.** A bot walking in to taunt from range arrives, can't act
  while the taunt cools down, and its stand walks it back out, in a loop. The walk waits for the
  cooldown too (the council's Dark Nucleus pickup).
- **A flickering input to a latched layout.** A distance deadband cannot help when the predicate
  itself flips: Icehowl's victim stepped in and out of melee range every ~100 ms and re-dealt the
  breath spread each time. Add time hysteresis, a grace before the change counts.
- **A latch written inside a predicate more than one caller evaluates per tick.** The symptom inverts:
  the bot freezes, because the callers disagree. Thorim's ring hold cleared "arrived" and told the
  trigger to move, then re-armed it on the tighter arrive tolerance and told the action to hold, which
  returned before its `MoveTo` — one bot stood in a Blizzard for 71 s with a clear point 4 yd away.
  Keep the predicate a pure read and split the latch by edge. The action only runs once the trigger
  says move, so it can clear the latch but never set it: set it where the trigger declines, with a
  write that cannot flip a second read that tick (widening the tolerance cannot). With the whole latch
  in the action it was never set: the movement guard it gates went dark, generic movers walked three
  melee into a cone, and a check that the freeze's signature was zero still passed — so also check the
  latch fires. An escape that overrides the deadband belongs outside the tolerance choice too, or the
  bot gets one order then re-latches before retrying — and 40-60% of orders are refused.

Rules that belong with the geometry rather than the action:

- **A spot checked only at its endpoint lets the walk cross the hazard it dodges.** Test the path,
  and a growing hazard (a spreading pool) at its size when the walk ends.
- **Park tolerance belongs in the clearance maths.** If a bot may stop `t` yards off the point, a
  point cleared by exactly the hazard radius still leaves whoever stopped on the hazard side inside
  it. Required clearance is `hazardRadius + tolerance`.
- **A loose tolerance is a feature**, not sloppiness: it is what stops a stacked formation piling
  onto a single coordinate.
- **A lattice spread can leave no dodge.** No point is farther than spacing/√3 from a triangular
  lattice's nearest member, so a dodge that must clear every member's mark by more (6 yd round an 8
  yd lattice) finds none: spread on rings, or give the dodge a fallback ladder
  ([Rotface](../raids/icecrown-citadel/rotface.md)).
- **A lattice spaced at the hazard radius plus two park tolerances**, with the tank spots on it,
  stays a valid spread in every phase and role with no per-pull geometry
  ([Blood Prince Council](../raids/icecrown-citadel/blood-prince-council.md)). Set thresholds from
  the real distances: an 11.69 x step puts 13.5 yd neighbours at 13.499, so `< 13.5` counts them all.
- **A hazard dropper's newest hazards lie a step behind it**, close enough to foul its next waypoint
  and force re-plans: its next-point check skips the hazards it summoned
  (`TempSummon::GetSummonerGUID`; the Blood-Queen's Swarming Shadows).

## Range and reach

Every distance helper measures differently, and a large-model boss inflates all of them.

- Core distance helpers are surface-to-surface on combat reach: `GetObjectSize()` returns
  `UNIT_FIELD_COMBATREACH`, `IsWithinCombatRange` is `dist3d < d + reachSum`, and
  `GetMeleeRange = max(casterReach + targetReach + 4/3, NOMINAL_MELEE_RANGE)` — against a 20-reach
  boss, ~22.8 yd. Hard-coded stand distances that ignore reach break on large models.
- **Combat reach can change mid-fight**: a transform aura's display does it (Putricide's 30993 raises
  his from 7.8 to 9), and so does a `MOD_SCALE` aura (Festergut's Inhaled Blight, +20% a stack, 8 to
  12.8). Read `GetCombatReach()` at runtime, not `creature_model_info`. A chasing creature stops at
  both combat reaches plus `CONTACT_DISTANCE`, not at melee range, so derive a drag or stand-off
  distance from those live.
- **A range literal in a boss script is not the distance it says.** `GetDistance2d(WorldObject
  const*)` subtracts both reaches too: Mimiron's Napalm picks among players at
  `GetDistance2d(mkII) > 15.0f`, which the MK II's CombatReach of 8 makes raw > ~24.5 yd. Convert
  before believing one — it decides who a mechanic may pick, and an empty pool can fall through to
  something quite different (Napalm's fallback is a threat-list pick with no distance floor). Our own
  code needs the same conversion: `reach spell` fires through `IsWithinCombatRange`
  (`OutOfRangeTrigger::IsActive`), so a formation clamped to raw `spellDistance - margin` gives away
  both reaches, 9.5 yd against the MK II, which parked Mimiron's camp on Napalm's floor. An area
  spell's radius goes the other way: `WorldObjectSpellAreaTargetCheck` adds the target's reach only
  when a player-controlled caster hits a target that isn't, so a creature's AoE, or a player's on
  another player (Death Plague's spread), is centre to centre and spacing just over its radius
  isolates. A persistent area aura is the exception: `DynObjAura::FillTargetMap` adds the
  target's size whoever cast it, plus the dynobject's own 0.39 for an ally or `TARGET_DEST_DYNOBJ_NONE`
  effect, so a 9 yd Inferno reaches a siege engine (7.7) at 16.7 yd
  ([Flame Leviathan](../raids/ulduar/flame-leviathan.md#hard-mode)).
- **`Spell::CheckRange` adds `GetMeleeRange` to a spell's *minimum*** for `SPELL_RANGE_RANGED`, so a
  20-reach boss turns a hunter's 5 yd minimum into ~28 yd centre-to-centre and every shot returns
  `SPELL_FAILED_TOO_CLOSE`. **Hunters are the only class with a minimum range** — do not relocate the
  whole ranged group for them.
- That same reach keeps `EnemyTooCloseForSpellTrigger` permanently active for anyone standing near,
  and every class wires it to an escape at **34–50 relevance, above `ACTION_MOVE` (30)**. A
  positioning action in that band loses to the bot's own escapes: suppress `FleeAction`,
  `RunAwayAction`, `CastBlinkBackAction` and `CastDisengageAction`, or it steps out and is dragged
  back forever. The last two are `CastSpellAction`s, so a veto naming only `MovementAction` lets them
  through.
- Pick the helper deliberately: `IsWithinCombatRange` adds **both** combat reaches,
  `WorldObject::IsWithinDist3d(Position const*, float)` adds **neither**, and `GetExactDist2d`
  ignores altitude — the last claims reach that is not there against anything hovering. A point-cast
  (dest) spell's range follows `Spell::CheckRange`'s dest branch: `IsWithinDist3d` with the caster's
  own size only, not `IsWithinDistInMap`, which adds the target's reach.
- **A healer stand must keep every heal target in heal range, or yield.** `PartyMemberToHeal` picks
  targets up to `HealDistance` 38.5 and `reach party member to heal` walks past heal range + 1, so a
  stand that doesn't cover them fights it and the healer stops healing. Test reach from the stand,
  not the bot.
- **`Unit::IsWithinMeleeRange(obj, dist)`: `dist` is extra slack**, not an absolute
  (`maxdist = dist + GetMeleeRange(obj)`). It is 3D with the 5 yd `NOMINAL_MELEE_RANGE` floor, so a
  hovering unit of reach 0 is in melee reach only within 5 yd, its height included (Kinetic Bombs).
- A target that walks needs a **sticky margin** on the reach test, or a bot on the edge of range
  swaps target every other tick. Re-test the sticky pick against the cap anyway: stickiness alone is
  a tow rope, and a mob that retargets drags the bot across the room until something outranks it.
  The cap is per role — Freya's 12 yd melee leash reused for ranged put their focus permanently out
  of reach, so they fell through to the boss.
- **Selection units must match margin units.** Picking by `GetHealth()` while the switch margin is in
  percentage points inverts the pick between adds with unequal max health.
- **Measure a formation from where the boss stands, against his reach.** Algalon's rings hung off the
  tank's slot, which put healers 7 yd and a mage 1.9 yd from a boss whose swing lands at 11.8 yd.
  Harmless while he holds still on his tank, one hit each the moment he doesn't.
- **A chasing add is pure pursuit** (`MoveChase`): it heads at where its target is each moment, so
  its path follows from the kiter's, and a straight lane to the kiter's future spot misses the corner
  it cuts when the kiter runs an arc faster than it. Simulate it from the kiter's planned path
  (Anub'arak's spike: 0.9 yd p90 off the real one 6 s on, the lane 19.7;
  [anubarak.md](../raids/trial-of-the-crusader/anubarak.md#pursuing-spike-34660)). A slow on the kiter
  that it walks out of in a step doesn't decide the race: count its unslowed speed.
- **Commit to an escape by simulating it, never by distance or the chaser's speed now.** A chaser
  that steps up its speed (Anub'arak's spike: 3.5, 6.1, 11.4 yd/s, 7 s apart) is slower at the start,
  so a turn gated on "faster now" comes at top speed with the gap gone. Run on only while turning a
  fixed delay later still makes it; a margin on the escape's own lead won't do, since that lead is
  bounded by the stand's distance over the chaser's speed and reads thin at once. A timer of its own
  (a boost running) must never hold the commit back
  ([anubarak.md](../raids/trial-of-the-crusader/anubarak.md#the-kite-planner)).
- **An immune kiter walks the chaser straight into its trigger.** Immune, it has the chaser on its
  heels, so a stand past the trigger on the far side from the chaser circles round it: walk into the
  trigger's centre and let the chaser follow. Slowed or not, pick the ending by simulating each,
  slow included: Anub'arak's straight walk into a patch beats a top-speed spike only from 40 yd back,
  a pass along its edge from 20.
- **Size a prediction horizon by the chaser's travel, not seconds.** 6 s of a 3.5 yd/s chaser is 21
  yd: the pass it makes 9 s on never shows. Judge up to the predicted catch, capped.
- **Replay a timing rule against the traces before shipping it.** A rule that commits once waiting
  longer would fail spends a one-shot resource as early as the geometry allows: Anub'arak's patch walk
  stays possible only 4.5-7.5 s of a 10-16 s run, so a Hand asked at the turn went 2-6 s in with the
  patch; taken at the catch it covers 10 s and the patch goes about 25 s in
  ([anubarak.md](../raids/trial-of-the-crusader/anubarak.md#the-kite-planner)).
- **Seed a closed-loop replay only with state from before its start.** Seeded with every Forbearance
  span of the real pull, Anub'arak's harness blocked Hands the replayed kite had not yet cast and
  flipped its verdict on a path change.
- **A boss that opens on a random player** (`AttackStart(SelectTargetFromPlayerList(...))` when an
  intro ends) is answered by staging, not by taunt speed: the pickup tank waits inside his reach and
  taunts on the first selectable tick, and everyone else waits outside it until a tank holds him
  ([Algalon](../raids/ulduar/algalon.md#he-is-held-at-the-edge)).

## Crowd control and threat on adds

What lands on a raid add is not what PvP experience predicts.

- **CC breaks on damage two ways.** `Unit::DealDamage` removes auras carrying
  `AURA_INTERRUPT_FLAG_TAKE_DAMAGE` on any hit: Polymorph, Freezing Trap. An aura with proc flags and
  no `PROC_TRIGGER_SPELL` gets a damage cap in `AuraEffect::CalculateAmount` (level-80 warrior base
  health / 4.75 = 2,652), and `HandleBreakableCCAuraProc` removes it past that, DoT ticks included:
  Fear, Frost Nova, Entangling Roots (66 of 67 Fears on Faction Champions broke within 30 ms of a
  hit). Cyclone has neither. Read both, not the reputation.
- **Cyclone makes its target immune to every spell, another Cyclone included**, so a CC chain times
  the recast to land just after it ends, not before. A bot can't: `CanCastSpell` refuses the immune
  target, so its recast starts at the fade and the victim is free for the cast time. A follow-up
  caster can still be elected and positioned during the immunity: test it on its own terms
  (`HasSpell`, cooldown, cost, form, control), keep it in range and sight, and give it an instant
  (Hammer of Justice, Repentance, Blind) so it lands the tick the aura fades.
- **A caster holds one of each single-target CC.** Cyclone, Polymorph, Fear, Banish, Hex and
  Entangling Roots carry `SPELL_ATTR5_LIMIT_N`: the caster's next one of that spell (same rank chain,
  `Aura::IsSingleTargetWith`) on another unit ends the first, so a warlock holds one Fear and one
  Banish. Deal one CC target per caster per spell.
- **`PlayerbotAI::RemoveShapeshift` ends only druid forms and Ghost Wolf.** Tree of Life needs
  `RemoveAura("tree of life")` and Metamorphosis can't be cancelled, so a duty that drops a form
  restricts itself to druids and shamans.
- **A stunned creature takes no heal assist threat**: `ThreatManager::ForwardThreatForAssistingMe`
  skips `UNIT_STATE_CONTROLLED` units.
- **Diminishing returns skip creatures.** `ApplyDiminishingToDuration` diminishes a creature only for a
  `DRTYPE_ALL` group — stun, cyclone, charge — or one carrying `CREATURE_FLAG_EXTRA_ALL_DIMINISH`.
  Root, fear and disorient are `DRTYPE_PLAYER`, so on an add with `flags_extra = 0` they land at full
  duration every time. The 10s PvP duration cap is gated the same way, and also needs a player
  caster. Both tests read the owner first, so a player's pet counts as a player. To predict the next
  duration, `Unit::GetDiminishing` returns the level the next hit lands at (non-const: it resets a
  stale entry).
- **Taunt immunity is a `creature_immunities` row** (`CreatureImmunitiesId`, which replaced
  `mechanic_immune_mask`): aura 11 or effect 114. Check it, and 0x80000 below, before a plan relies on
  a taunt. `Creature::LoadTemplateImmunities` applies it under a placeholder spell id (`uint32` max),
  so a script's `ApplySpellImmune(0, …, false)` never clears it: Lady Deathwhisper's heroic taunt
  immunity holds all fight, not from phase 2 as her script reads.
- **Taunt diminishes only on a creature flagged for it.** `flags_extra` 0x80000
  (`OBEYS_TAUNT_DIMINISHING_RETURNS`) runs taunts at 100/65/42/27% duration, then immune until 15 s
  after the last one fades (`Unit::GetDiminishing`, `Spell.cpp` DR block). A taunt every 8 s plus a
  Righteous Defense in between keeps such a boss immune, so a plan built on repeated taunt-backs
  fails there. Onyxia and the Twin Val'kyr carry it. Swaps 20 s or more apart stay at full duration,
  but generic tank taunts spend the DR in between: block `Family::Taunt` and own the swap
  ([Saurfang](../raids/icecrown-citadel/deathbringer-saurfang.md)).
- **A taunt lends the boss for 3 s, then real threat decides.** `Spell::EffectTaunt` only matches the
  taunter to the highest threat on the table, and the 3 s aura forces the target meanwhile. A boss
  whose table is empty at a phase change (one untouched until then) gives the taunter nothing to
  match, so it goes back to the highest real threat as the aura fades
  ([Thorim](../raids/ulduar/thorim.md#somebody-has-to-hold-the-boss-and-a-taunt-will-not-do-it)).
- **A ranged pull as a taunt's fallback turns the tank.** Shoot or Throw faces its target, so a
  "taunt, else pull" helper retried while the taunt cools down points a tank holding the boss at the
  add. Gate the fallback on the taunt's own cooldown; a cooldown reset hides this (`IccCastClassTaunt`).
- **Death Grip (49576 → 49560) splits its effects.** A creature immune to taunt and attack-me
  (`CreatureImmunitiesId` with aura 11 and effect 114) still takes the dummy pull, with no aura and
  so no DR. Where the taunt aura lands, the grip shares `DIMINISHING_TAUNT` with every class taunt,
  and at taunt-immune DR the whole spell misses (`SPELL_MISS_IMMUNE`), pull included.
- **A rooted creature keeps its threat victim.** `ThreatManager::ReselectVictim` never retargets it
  to whoever stands in melee, so the retail "a rooted add hits the closest player" doesn't hold here.
- **A creature drops a victim it can't hurt.** `ThreatReference::ShouldBeSuppressed` takes out of
  contention a target immune to the attacker's melee school, confused, or under a breakable stun, so
  Hand of Protection moves melee attackers off a focused bot at once, as Divine Shield or Ice Block
  does. School immunity suppresses the holder's reference on apply (`EvaluateSuppressed`), and
  removing it restores nothing: only the holder's next `AddThreat` does, so a cancelled bubble can
  still hand the boss to the next player at his once-a-second threat check.
- **Divine Shield 642 purges as it applies** (`SPELL_ATTR1_IMMUNITY_PURGES_EFFECT`): every
  non-positive aura without `SPELL_ATTR0_NO_IMMUNITIES` goes, so cast and cancel on the next line is a
  real stack reset (Festergut's Gastric Bloat). Kept up on a tank, it hands the boss away.
- **Hunter pet Growl is `SPELL_EFFECT_THREAT` only, not a taunt.** Against a script that seeds player
  threat (Faction Champions' 10M × modifier) it does nothing but burn 15 focus. A multiplier can't
  veto pet autocast: turn it off with `Pet::ToggleAutocast`; the non-combat `toggle pet spell` turns
  it back on.
- **Pulling a pet off a target never sticks.** PetAI re-targets it through `OwnerAttacked` (every
  harmful spell the owner casts), `KilledUnit` → `SelectNextTarget` and `AttackedBy`. All three skip a
  passive pet, which still attacks while `IsCommandAttack` is set (`PetAI::CanAttack`)
  ([Yogg-Saron's Crushers](../raids/ulduar/yogg-saron.md#crush-is-a-5-cone-that-tracks-its-victim)).
- **A kiter's pet must not get the kited unit.** `RaidDirectAttackAction::AttackDirect` sends the pet
  onto the owner's pick, so a ghoul pulls the kited add's threat and takes its swings: give the pet
  its own pick ([Rotface](../raids/icecrown-citadel/rotface.md)).
- **An add that calls `DoResetThreatList` on a timer can be neither tanked, taunted nor redirected.**
  Read its `UpdateAI` before designing any of the three. Freya's Detonating Lasher re-rolls a uniformly
  random player every 10s, which leaves geometry plus a snare as the only handling.
- **The swap tank not holding the boss must not damage him.** After a taunt both tanks build threat at
  the same rate, so the one carrying the debuff stacks takes him straight back, white swings included
  (no action carries those, so a damage-action veto misses them). A swap fired by the tank *not*
  holding him also never fires when that tank is a human: the bot holding him has to call it
  ([Algalon](../raids/ulduar/algalon.md#tanks-the-swap-pair)).
- **A pacing gate with a bypass recreates the stacking it exists to prevent.** Override it only for
  what is worse than the hit it delays (Algalon's star bypass put two raid-wide explosions 3.2 s
  apart, [algalon.md](../raids/ulduar/algalon.md#stars-under-big-bang-else-paced)).
- **Reserve a cooldown against the next event by its own cooldown, not a window.** Hold whenever
  casting now leaves it cooling down at the event; a button whose lockout (Forbearance) blocks another
  counts as spending that one too
  ([Algalon's Big Bang soakers](../raids/ulduar/algalon.md#cooldowns-the-tanks-first)).
- **The Aoe threat type is no splash marker.** `CastHealingSpellAction` declares it, while whirlwind,
  divine storm, consecration, death and decay, starfall, swipe, pestilence, howling blast and explosive
  trap declare something else. Read the spell: `SpellSplashes` (`RaidSplash.h`) tests
  `IsTargetingArea() || IsAffectingArea()` on its id, and names the totem and trap summons, whose area
  lives on the summoned unit. Starfall, Blade Flurry, Sweeping Strikes, Cleave, Bladestorm, HotR,
  Avenger's Shield, Multi-Shot, Chain Lightning, Seed of Corruption and Mind Sear fail even that test.
  A hold on "don't hit the add next to the boss" adds the named list in `RaidSplash.h`, and holds self
  auras outright: they hit whatever comes near for their whole duration.
- **A draining add's lifetime is its health over its measured loss**, splash included, so trace drain
  and splash deaths apart before tuning, and plan each death against the others: killing a substitute
  when the soonest-dying one can't be touched stacks two explosions anyway, and dropping one a few %
  from dead moves nothing, its DoTs finish it inside the window
  ([Algalon's stars](../raids/ulduar/algalon.md#stars-under-big-bang-else-paced)).
- **A slot leash fights `reach spell`.** It walks half the gap to an out-of-range target, the slot node
  walks the bot back, and the target is never reached. Give the mover its own spot in range instead.
- **A taunt moves the boss, not the bot's own target.** A tank that taunts off an add keeps the add targeted, so
  reach walks it to the add, the slot walk back, and a damage guard zeroes every swing: set the target with the
  taunt, and retarget a holder that got the boss by threat.
- **A redirect's fallback target can be what pulls aggro**: Tricks of the Trade falls back to the
  strongest melee DPS ([pitfalls.md](pitfalls.md#detection-that-never-fires)).
- **A cast with `PreventionType` 0 takes no kick.** `EffectInterruptCast` needs
  `SPELL_PREVENTION_TYPE_SILENCE` and no silence aura stops it; only a stun does (`SetStunned` calls
  `CastStop`). Check the prevention type before electing a kicker (Summon Undead 69516, Frozen Orb
  71274).
- **A charmed player is a valid direct-attack target**: `AttackAction`'s player guard checks only the
  PvP-prohibited zone and area lists, and no raid is on them.
- **Mocking Blow is a taunt**, though a `MELEE_ACTION`: `ClassifyAction` counts it in `Family::Taunt`,
  so a taunt guard has to cover every role, not tanks. No strategy wires it; Arms' `piercing howl` falls back
  to `hamstring` instead, since a damage dealer that taunts a raid add dies to it.
- **Read `speed_run` before designing any relocation.** An add faster than a player can be neither
  kited nor ferried: the bot walks the whole distance taking uninterrupted melee, out of healer range,
  dealing nothing, and still arrives with the add on top of it. Freya's lasher is 8.0 yd/s against 7.0,
  and the corral built on ferrying it was the single leading cause of death in the encounter
  ([../raids/ulduar/freya.md](../raids/ulduar/freya.md)). Where the add is faster, **stack the raid and let it
  come**; geometry is the only lever left.

## Coordinating a raid with no shared state

Every bot has its own `AiObjectContext` and cannot see another bot's decision. Five mechanisms make
twenty-five of them agree anyway:

- **Derive, don't communicate.** A roster sorted by guid is identical on every bot. So is "rank by
  energy descending, guid ascending" — and that one **self-rotates**, because acting costs energy and
  drops the actor to the back of its own queue.
- **Latch per instance, not per bot**: a `RaidInstanceState<T>`
  ([../raids/README.md](../raids/README.md#per-instance-state)). Use it for phase, creature lookups,
  layout choices and any held heading. **Never `thread_local`** — `MapUpdater::schedule_update`
  pushes maps onto a shared queue with no thread affinity, so above `MapUpdate.Threads` 1 the pool
  hands the same instance to a different worker and each worker gets its own empty latch. The
  signature is one note per bot **per thread that ever ticked the pull** — identical rows that read as
  a bot re-deciding several times a second. A time-bounded **cache** of live world state is exempt: a
  rebuild on another thread recomputes the same answer. Only a **latch** is corrupted by being
  duplicated.
- **Read the world, not your bookkeeping.** "Has someone already done this?" is answered by the aura
  their action left behind, with its remaining duration as the timestamp. That survives a cast that
  silently failed and it counts real players in the group; a flag set on our own success does
  neither.
- **One helper, two readers.** When a trigger and an action both need "is this bot the one", they
  call the same function. Two derivations of the same predicate will disagree.
- **Anchor the derivation, not the bot.** "Nearest to me" gives every bot a different answer,
  "nearest to the boss" gives one — Hodir's shelter and Freya's parked spore are the same helper.
  Where fresh candidates keep spawning around the anchor, whoever acts on it **latches the guid**,
  since re-deriving each tick can flip the destination mid-walk; the rest still agree, because the
  latched one stays nearest while it closes.

Pitfalls in derived assignment:

- **A fallback that elects a lower-ranked bot needs hysteresis** against the higher-ranked one going
  free for a tick: a main tank is free whenever the boss drops his victim for a moment, and took
  Rotface's kite from a damage dealer for one refresh.
- **A greedy spread's stay-put cost counts the bots not yet placed** at their current positions, or
  the worst spot (the stack) looks cheap to the first bot asked.
- **A bot sharing a held slot must not mark it used** against its holder, or the holder is evicted on
  the next deal.

## What a strategy costs per raid

Cheap per bot, ruinous per raid — and invisible in single-bot testing.

- **A multiplier runs on every popped action that passed `isUseful()`**, dozens per bot per tick, so
  anything it derives is derived that many times: test the action first
  ([action-selection.md](action-selection.md#multiplier-semantics)). Encounter rules do it by
  construction. A multiplier needing the encounter's pick reads the resolver's memoized pick:
  re-deriving it can hide a sight raycast per action. Scale, ToC 25H: a bot pass runs ~127
  interval-1 trigger nodes and ~118 multipliers, 28 of them ungated, so they run on every action
  anywhere in ToC.
- **Ask a definition once per action, not once per rule.** Each rule asking whether it cares cost
  0.12–0.19 µs a row; `CaresTable` (`RaidEncounter.cpp`) fills one bitset per action per definition
  per bot, and the other rows read their bit through a last-action shortcut for ~0.02 µs. That was
  ~90% of the ToC pass's gain ([measured](#measuring-a-cost)). The definition's first row now pays
  the lookup for all of them, so its own pmon row rose.
- **Cache per action only where the check it replaces costs more than the lookup.** A
  `RaidActionCache` is an `unordered_map` on `Action*`. A multiplier sees each action once per pass,
  so the last-action shortcut practically never hits, and after ~120 other multipliers the nodes are
  likely cold: the lookup measured 0.03–0.15 µs dearer than the one to four failing `dynamic_cast`s
  or the short name copy it replaced (cheap ToC guards +8% to +74%). Replacing a `"spell id"` read
  or name-set lookups, it pays (suppress aoe −17%, spike kite −29%).
- **What a per-`Action*` cache may hold**, kept inside one multiplier (a global one breaks when a
  logout frees an address for reuse): facts fixed at construction, i.e. dynamic type, `getName`
  (every override derives from constructor-set members) and `getThreatType` (every override returns
  a constant). Not the spell: `PullAction::InitPullAction` reassigns `CastSpellAction::spell` in
  `isPossible` and `Execute`, so `RaidSpellIdRef` holds the `Value<uint32>*`, fetched at the old
  first-call point, and re-compares `getSpell()` on every read, never caching the id; FC's
  `DispelGuard` caches a `getSpell()` compare, safe only while no cure class reassigns it. Holding a
  `Value*` or keying on `Action*` is safe for a multiplier's life: an `AiObjectContext`'s values and
  actions outlive its engines until the `PlayerbotAI` dies. Tools: `RaidActionCache<Facts>`,
  `RaidSpellIdRef` (`RaidCombatHelpers.h`), the `RaidSplashCache&` overloads of `IsRaidAreaDamage`
  and `SpellSplashes`.
- **A memo call is a side effect.** The first call in a ms fixes what every later caller that ms
  sees, and actions run between multiplier passes; `"spell id"` caches 20 s, so a skipped `Get()`
  shifts its next recompute. A behaviour-neutral speed-up never adds, removes or moves such a call,
  even behind a cheap test in front of it; it only makes the call cheaper.
- **Copy a memo's list before looping it** where the loop can trigger a nested refresh: Anub'arak's
  ReadPhase → Sweep clears `state.burrowers` mid-loop. A by-value `boost::container::small_vector`
  (Boost.Container is already in the core) keeps that snapshot with no heap allocation for short
  creature lists.
- **Role lookups are not free.** `IsMainTank` → `GetMainTankGuid` walks every group member;
  `IsTank`/`IsDps`/`IsRanged`/`IsHeal` cost a `GET_PLAYERBOT_AI` hash lookup plus a bitmask test
  (`Engine::HasStrategyType`). Precompute them for a 25-man loop or a sort comparator, which otherwise
  calls them on both sides of every compare — `Strategy::InitMultipliers` builds one multiplier per
  bot, so there is no sharing to worry about. On a **human** the same predicates once walked the whole
  talent map per call; `specTabCache` (`AiFactory.cpp`) fixed that, and its shape is worth copying:
  entries are **invalidated, never erased** — an erase lets an in-flight read on another thread
  recreate the entry and store a stale answer into a fresh one — results computed across a
  `generation` bump are discarded, and nothing is cached until `IsInWorld()`, because talents are
  still loading mid-login.
- **Do not stack cache windows.** Caching an already-cached value again leaves the second layer
  enforcing the previous answer for up to a full window after the first moved on. Cache the expensive
  leaves; read the cheap composite live.
- **A value built with the default `checkInterval` recomputes on every `Get()`**
  (`CalculatedValue::Get`), so each `GetFirstAliveUnitByEntry` or `"nearest npcs"` read is a fresh
  sight-range sweep, and Ulduar code took 20-60 of them per bot per tick. One **per-bot scan stamped
  with `getMSTime()`** collapses them (`RazorscaleScan`, `IronAssemblyScan`): a bot never ticks twice
  in one ms. A multiplier is per bot and asked once per action, so its boss lookup or sweep caches
  the same way (`IccLadyDeathwhisperMultiplier`, the Dominate Mind splash hold). `FindTargetValue` (`"find target"`) is built with interval 1 too, and every read runs
  `Utf8toWStr` + `wstrToLower` + `Utf8FitTo` on each entry of the bot's threat list
  ([Eye of Eternity](../raids/eye-of-eternity.md)).
- **World state holds still only through the trigger checks.** `ProcessTriggers` checks every trigger
  before any action runs, and no trigger changes the world; an action returning `false` still can —
  drop a target, command a pet, turn the bot, push Yogg-Saron's Brain to 30% and strip Shadow Barrier —
  so a ms stamp can hand a later multiplier or action a stale read. An exact cache keys on
  `EncounterTriggerPassId`: non-zero only inside `EncounterGatedTrigger::Check`, new each pass,
  because `Reset` is not guaranteed to close the gate pass. Yogg-Saron's 200 yd reads use it.
- **Grid sweeps walk every `SIZE_OF_GRID_CELL` (66.67 yd) cell inside the radius**, and twenty-five
  bots re-answer the same instance-wide question every tick — EoE peaked near 250 identical sweeps a
  tick. One instance-keyed creature cache collapses that to one. **Cache guids, not pointers**, so a
  despawn inside the window drops out instead of dangling; and make the fill radius **wide**, since
  the sweep is anchored on whichever bot refreshed it while its answer serves the whole raid.
- **A boss the instance script tracks needs no sweep.** `InstanceScript::GetCreature(type)` is a map
  find for every entry in the script's `creatureData`, written and erased as the creature enters and
  leaves the world. It equals `FindNearestCreature(entry, r, true)` only for an entry with **one**
  creature per instance, after the sweep's own tests (`IsAlive`, `IsWithinDist(c, r)`, `InSamePhase`),
  and only where the sweep provably visits the creature (next bullet); otherwise run the sweep. Ids
  are ported in `UldEncounterGate.h`.
- **A sweep visits container cells, not positions**, so a position bound such as `|dx| + |dy| < r`
  is not exact: a creature's container cell lags its position until `Map::MoveAllCreaturesInMoveList`
  runs, and at r = 200 `Cell::VisitCircle` drops three cells from each corner of the 7×7 block. For a
  bot on `IsValidMapCoord`, r = 200 always visits the 3×3 block around its standing cell (proven over
  every float coordinate); past the map edge the clamped cell breaks that. The exact module-side test
  for a candidate within ±1 cell is a one-cell
  `map->Visit(Cell(coord), TypeContainerVisitor<searcher, GridTypeMapContainer>)` looking for its
  pointer, since `GetCurrentCell()` is protected. So a remembered guid (a "last hit") can spare a
  boolean sweep, accepted only after every test the sweep applies plus that visit, falling back to
  the unchanged sweep on any miss; the cell proof holds for the radius it was run at (ToC's gate hint:
  200 yd only).
- **Several entries take one sweep**: the core's `GetCreatureListWithEntryInGrid(list, entries,
  range)` (upstream AC #22286) visits and tests like the single-entry form, and taking its hits entry
  by entry in config order reproduces one sweep per entry exactly; a later sort makes that grouping
  moot only on a unique key. `Map::Visit` never loads a grid or creates a cell, so sweeps are pure.
- **`"nearest npcs"` casts a line-of-sight ray at every non-player unit** and has no cache. To pick a
  few entries out of it, test the entry inside the searcher's check and ray only the matches — same
  units, same order (`GetYoggSaronGuardiansInSight`, the Yogg-Saron resolver).
- **Target exclusions run for every target value on every boss.** `Strategy::AppendTargetExclusionsFor`
  gets the candidate list; a raid whose exclusions need a sweep returns first unless a candidate has
  the entry it would exclude (`ObjectGuid::GetEntry`). Don't re-read `"attackers"` for it: the 1 s
  refresh can hand back a different list than the caller holds.
- **An open gate is not a live fight, and `z >= 300` is not Yogg's platform.** A raid strategy
  registers every boss at once; the encounter gate (`EncounterGatedTrigger`, `EncounterGatedMultiplier`,
  `RaidEncounterRules::GateOpen`) holds open every encounter not yet killed while none is in progress,
  so triggers run for every bot in the instance between pulls, and a sweep ahead of the live-phase
  test is paid raid-wide all evening. A raid not built on encounter definitions has no gate: every
  boss's triggers run every tick, and its multipliers typically open on `"find target"`.
- **"Am I the first able provider" costs askers × candidates a tick.** Each bot evaluates everyone
  ahead of it, so reject on what stays false for many ticks first — cooldown, a reserve — and only
  then pay range, sight and `CanCastSpell`, which tests the cooldown itself but after the caller's
  raycast. Likewise health before sight ([algalon.md](../raids/ulduar/algalon.md#what-it-costs)).
- **A rule predicate runs for every action it is asked about**, a movement rule for every movement
  action a bot weighs. One that constructs trigger objects pays their construction each time: read
  the helpers those triggers wrap instead, keeping each trigger's full condition. A hazard list or
  resolver it reads needs a per-bot, per-ms memo, or it is rebuilt dozens of times a tick per bot.
- **A progress latch keyed on a point that moves with the bot grows without bound**: each new key is
  a fresh latch that lives for the server run and never reaches its give-up. Evicting or re-keying
  changes which latch matches — a behaviour change, not only a saving.
- **`Trigger` supports throttling and almost nothing uses it.** The ctor is
  `Trigger(botAI, name, checkInterval = 1)`, normalised as
  `checkInterval == 1 ? 1 : (checkInterval < 100 ? checkInterval * 1000 : checkInterval)` — pass
  `200` for 200 ms, and **never pass 2–99**, which silently means seconds. Any trigger starting a
  multi-tick reaction the `MotionMaster` carries on with runs at 200–300 ms invisibly; triggers
  gating positioning or vehicle steering must stay per-tick. A wrapper trigger (`ToCGatedTrigger`)
  must copy the inner one's interval: its own default of 1 promotes a throttled trigger to every tick.
- `Engine::ProcessTriggers` `Check()`s an inactive trigger **once per node** referencing it, so a
  trigger wired to three nodes is evaluated three times while it is quiet.

### Measuring a cost

**Cost symbols**: **S** = one `"possible targets no los"` read (a `Cell::VisitObjects` at
`SightDistance` 100 plus `IsPossibleTarget` per hostile, no LOS ray); **N** = one `"nearest npcs"`
read (the same visit **plus a VMAP and dynamic-tree LOS raycast per non-player unit** — 49-67 units
per Auriaya snapshot, so ~50 raycasts a read); **G100** / **G7** = grid sweeps at those radii.

**Proving a refactor behaviour-neutral.** The equivalence harness is the only validated technique:
take the old function bodies verbatim from `git show HEAD:<file>` and the new ones from the working
tree, compile both together with counting stubs, and enumerate the input space — 18.5 M
combinations, 0 mismatches on XT-002. Include a red-capability check so a pass means something:
dropping `purgingImmunity` produced 1,392 mismatches and dropping the main-tank exemption 11,520.

- Cut both versions out by regex anchor plus brace matching into two namespaces over global counting
  stubs, and compare the result **and** the call log (`Get()`s, memo calls) per call. Write each red
  check as a text mutation that must match exactly once.
- Pick reds that change the verdict: one mapping a branch onto an equivalent branch passes silently,
  and such a pass hints the branches can merge (Feign Death and the held path in ProtectSpikeKite).
- To cover every dynamic type a guard can see, make each cast target inherit `Action` virtually and
  build `Combo<mask>` classes from conditional virtual bases, so `dynamic_cast` cross-casts for any
  subset of targets.
- To list every call resolving to a new overload: in a scratch copy mark it `[[deprecated]]` and
  append `#error` to each TU; pb-syntax-check prints a failed TU's warnings. A sibling overload that
  leaves an anonymous-namespace function uncalled fails CI's `-Wunused-function`: replace it instead.

**A pure move** (a file split, no body changes) needs no harness: one generator reads every source
from the base ref, anchors each segment on a regex searched in order and fails on any miss; an
independent parity check then compares node order, creator maps, multiplier order and a multiset of
code lines. Splitting the move across agents repeats the reading and still leaves the parity work.
ICC's 21k-line split went this way.

**Timing** is `.playerbots pmon` ([../systems/observability.md](../systems/observability.md#timing-a-bot-playerbots-pmon)):
read ratios, never absolute times. A bot pass takes ~1 ms, so a per-instance per-ms memo refreshes
on nearly every pass, paid by the first node in trigger order that reads it: that row times the
refresh, not its own logic, and speeding it up means a cheaper refresh body.

**Comparing two runs** (one pmon dump per fight, `pmon reset` at each pull):

- Take cost per call per fight and re-weight to the before run's call counts,
  after = Σ count_before · µs_after / Σ count_before: fight lengths, phase mixes and overworld load
  differ between runs.
- Deflate by a control: per fight, the time-weighted median after/before ratio over rows the change
  cannot reach (≥ 2000 calls in both runs); net = raw ratio / deflator. The control's p10–p90 range
  is the noise band; a row is slower only past p90 with ≥ 10 ms of baseline time.
- Faction Champions draws a random champion set per run: compare it per check only.
- A behaviour-neutral change keeps call frequencies: each trigger checked once per pass, each
  multiplier's reach (its calls / the first multiplier's) within ±1.5 pp. Per-full-tick and
  per-executed-action ratios drift on their own: `PlayerbotAIBase::FullTick` counts calls the check
  delay skips, and executed actions include overworld bots.

**Measured, the raid perf pass** (ToC 25H, 2026-11-16 vs 2026-11-17 on `ef1dfd517`): ToC's share of
`UpdateAIInternal` 22.7% → 18.0%; trigger plus multiplier cost per call −30.5% net (9.5 of 31.3 s a
clear), multipliers −44%, triggers −11%. `CaresTable` gave −8.5 s; the encounter gate's first-hit and
candidate tests −0.9 s (`gormok tank duty` −66%); the FC Defence refresh −0.4 s. No measurable gain:
Anub'arak's ReadPhase refresh work, and the entry-set sweep in FC and Jaraxxus. The ICC half of the
pass has no baseline, so it is unmeasured.

**Settled — do not re-audit:**

- No `checkInterval` or framework memo on `"possible targets no los"` / `"nearest npcs"`: both are
  global values, so a memo goes stale across ticks for every other reader.
- Do not run `isPossible` before the multipliers — Mimiron's and Algalon's multipliers have side
  effects, and vetoes are logged.
- `GetCreatureListWithEntryInGrid` is **not** a substitute for a LOS sweep: it is a grid container
  only, and `AllCreaturesOfEntryInRange` measures **2D** with both objects' sizes added where
  `AnyUnitInObjectRangeCheck` measures **3D** (`GridNotifiers.h`).
- Leave the shared `GetFirstAliveUnitByEntry` alone: upstream marks it for removal and every raid
  calls it, so its replacement is a Custom-owned copy under another name, not an edit in place.
- Replicating `Cell::CalculateCellArea` + `VisitCircle` to skip the octagon walk is provably
  identical but copies ~40 lines of core internals that go silently wrong the day AC changes cell
  visiting. Rejected.
- The per-action caches that made cheap ToC guards slower (`407bb950e`, `0a35e1f0c`, late interrupt
  in `e7b5c1203`, `ee8d2f669`, `2f0aef500`, `8ff51744f`; +0.40 s a 25H clear) stay as they are: the
  user's call.

**Open:**

- `BossFireResistanceTrigger` / `BossFrostResistanceTrigger::IsActive` (`BossAuraTriggers.cpp`) run
  `FindBossByName` — a sight-range sweep plus a UTF-16 lowercase of every unit name — **before**
  `HasAura`, the spell-known check and the raid-group check, for 7 paladin and 3 hunter triggers in
  every raid. Reordering changes no behaviour.
- Engine: `Engine::LogAction` formats every engine event for any bot with a real player master
  (`LogInGroupOnly` defaults to 1), whether or not debug logging is on; value lookups hash twice;
  the action queue compares by name copies.
- Costliest ToC rows per check after the perf pass: the FC Defence refresh (`personal defensive`
  ~64 µs) and Anub'arak's ReadPhase (`kiter needs immunity` ~44 µs). The FC Ball refresh grew with
  `BreakChasedCasts` (`6632778ad`), a `GET_PLAYERBOT_AI` walk over every member.
- Unexplained: after `ReadOpener`'s one-sweep change the Jaraxxus opener rows rose +91% / +34%,
  though Anub'arak, also one entry, fell 36%; a second Jaraxxus recording would tell a longer live
  window from more adds in range.
- BPC `DirectHit` builds its `"spell id"` lookup per action; `RaidSpellIdRef` would pay.
- `RaidInstanceState` lock cache: crash risk, and Docker Desktop has no TSan to prove it.
- Ulduar and older raids have had no perf pass.
- Behaviour changes, each needing its own go: widened per-ms memos; one refresh per world tick
  (`GameTime::GetGameTimeMS()`); the ToC gate by guid slot alone (the slots in the header make it a
  one-liner); arena-centred ~95 yd sweeps; lazy FC Defence `Update*`; Icehowl's guid slot
  (`DATA_ICEHOWL`, 8) in `RefreshIcehowl`, whose comment wrongly says there is none.
- Ulduar's two directly pushed multipliers (`UldMultipliers_Shared.cpp`): `UldThreatRedirectMultiplier`
  makes seven `GetUldUnitByEntry` lookups per Misdirection/Tricks candidate where one scan would
  serve, and `UlduarBurstWindowMultiplier::EvaluateWindow` ends on an ungated
  `"find target" razorscale` in every other Ulduar fight. Do **not** split `EvaluateWindow` itself:
  its per-boss switch lets the `IsBurstCooldownAction` early-out run once per action, not once per
  boss, and resolves nine boss entries out of **one** `"possible targets no los"` sweep.
- `MimironLethalWindowActive` (five triggers) and `ThorimBalconyWalk` (one) construct triggers per
  call, and `YoggSaronBodyDetourAction` keys `YoggSaronWalkMakingProgress` on its moving arc waypoint
  (both shapes above).

## Combo points, finishers and stacking auras

Vehicle spellbooks are built on these, and so are several boss auras.

- **A `Unit` holds combo points for one target at a time.** `GetComboPoints(who)` returns 0 for
  anyone else and switching target resets the pool, so a rotation that re-picks "most injured" every
  tick can never reach a finisher. Self-casting is what makes banking possible.
- **`SPELL_ATTR1_FINISHING_MOVE_*` spends the whole pool**, whatever its size, so every finisher in a
  spellbook competes for one bank and they have to be tuned against each other rather than apart.
- **Combo points scale duration, not amount**, unless `EffectPointsPerCombo` is non-zero — it usually
  is not. `Unit::CalcSpellDuration` = `min + (max − min)·cp/5`.
- **A stacking aura shares one refreshing duration.** `_TryStackingOrRefreshingExistingAura` →
  `ModStackAmount(1)` → `RefreshTimers`, with the periodic amount multiplied by stack count. Stacks
  therefore grow for as long as the aura lives, **applications per minute is the quantity to
  maximise**, and duration only has to outlive the cycle. `RefreshTimers` recomputes max duration on
  *every* application from the combo points held at that moment — the pool is still there, since
  `_handle_immediate_phase` applies before `_handle_finish_phase` clears — so a long duration cannot
  be bought once and kept.
- **Periodic auras from different casters stack as separate copies** (`Aura::CanStackWith`), so a
  boss that re-targets an afflicted player can give it a second copy ticking on its own.
- **A defensive finisher wants the smallest bank that spans the window, and it wants to be late.**
  Cast at the warning it can expire before the damage lands; cast at full bank it has just spent the
  offensive finisher. Latch when the warning was first seen and cast at `windowEnd − duration(cp)`.

## Reading a boss script

Detection failures, spell ids by difficulty included, are catalogued in
[pitfalls.md](pitfalls.md#detection-that-never-fires); these are the ones specific to scripted raid
encounters.

- **A triggered instant publishes nothing.** `DoCastAOE` leaves `m_targets.GetUnitTargetGUID()`
  empty, and `m_Events.AddEventAtOffset(…, 3s)` has no cast to observe at all. Where the boss AI
  stores victims in its own guid slots, read those — they are filled at the *warning*, which is the
  entire reaction window.
- **Mirror core enum values, do not include them.** Script headers are not on a module's include
  path. Mirror with a comment naming the source enum and the entry's position in it, and re-check
  the mirror whenever a core merge touches the script: core #28117 removed three ToC data types,
  moving Anub'arak's guid slot from 13 to 11, and the stale mirror read an empty guid with no error.
- **A phase heuristic must separate the pull intro from a mid-fight transition.** Both read as "in
  combat, boss non-attackable". Full health separates them whenever the real transitions are
  health-gated. A boss briefly passive after an event looks like an earlier phase's passive window
  too (the Lich King for 1 s after the heroic Frostmourne return), so latch the phases already
  reached.
- **Read a boss spell's full effect list, not its damage.** "Damage" spells carry riders that decide
  the whole reaction: Freya's Ground Tremor is `Effect_1` damage plus `Effect_2 = 68`
  `SPELL_EFFECT_INTERRUPT_CAST`, which silences the raid's school for 10s — the damage number says
  nothing about the mechanic that matters. Read `EffectRadiusIndex` on every effect too; index 28 is
  50000 yd, i.e. raid-wide and undodgeable. No index is radius 0: from a creature that hits only a
  target at 0 yd, since only a player-controlled caster adds the target's reach (ICC gunship
  sergeants' Bladestorm 69653 hits nobody).
- **`SPELL_EFFECT_FORCE_CAST` can make the original caster cast the child.** Its special cases in
  `SpellEffects.cpp` do (Blood Nova 72378/73058: Saurfang casts 72380 on the picked victim), so the
  child's area is a creature's, centre to centre. Check that switch before sizing a forced child's
  radius.
- **A guide's "immunity stops X" must be checked against `SPELL_ATTR0_NO_IMMUNITIES`** (0x20000000,
  `Attributes` in `spell.reference.csv`): `Unit::IsImmunedToDamage` and `IsImmunedToSpell` return
  false for such a spell, so Impaled ignores Hand of Protection.
- **Freedoms clear only snares and roots.** Hand of Freedom, Every Man for Himself and Escape Artist
  remove auras whose spell or effect carries `MECHANIC_SNARE` or `MECHANIC_ROOT`
  (`Unit::RemoveMovementImpairingAuras`, mechanic immunity masks), so a boss slow with mechanic 0
  (Sticky Ooze 69778) survives them. Key a freedom duty on `HasAuraWithMechanic`, not
  `PlayerbotAI::IsMovementImpaired`, which counts every speed cut. Anti-Magic Shell 48707 carries aura
  267 (school immunity, mask 126), so it also keeps magic-school slows and debuffs from landing.
- **A `spell_proc` row can narrow its DBC `ProcTypeMask`**: `SpellTypeMask` 1 procs on damaging hits
  only, so a DoT's application or a debuff never does (Unstable 72059). Check it before trusting the
  DBC.
- **A creature's swing is readable live.** `UNIT_FIELD_MINDAMAGE`/`MAXDAMAGE` already carry
  `DamageModifier`, attack power and `TOTAL_PCT` auras, and `GetAttackTime` reflects haste. With
  `Unit::CalcArmorReducedDamage` and `GetTotalAuraMultiplierByMiscMask(MOD_DAMAGE_PERCENT_TAKEN, …)`
  that is a per-swing model with no hard-coded numbers (Putricide's sharers).
- **A leech's multiplier lives in `EffectMultipleValue`** and differs by difficulty row: Rune of Blood
  heals Saurfang 10x on normal and 20x on heroic. Read every id.
- **Speeds and cast times differ by difficulty too.** `speed_walk` differs between a creature's
  difficulty entries (Kinetic Bomb 0.8 normal, 1.2 heroic), and a heroic id can cast faster (Empowered
  Shock Vortex 73038/73039 4 s, 4.5 s on normal). Derive timings from the live unit or the remapped
  spell, never the normal-mode value.
- **A summon lives as long as its summon spell's duration** (`DurationIndex`). A script's
  `DespawnOrUnsummon(30s)` only queues a delayed unsummon and doesn't extend it, so read the index
  before sizing a chase (Deathwhisper's shade lives 7 s and closes only 1.75 yd on a slowed victim).
- **`SpellInfoCorrections.cpp` can rewrite a DBC value or retarget an effect.** Fire Bomb 66320's
  radius is 5 yd in the DBC and 2 yd in play, and Frozen Orb 71274's aura moves from its caster to
  the target, so read it before sizing a hazard or keying on which unit carries an aura.
- **A hardcoded aura handler can scale a trigger spell's radius every tick**, beyond both the DBC and
  the corrections: `SpellAuraEffects.cpp` casts Slime Pool 66881 and Grobbulus' Poison with a growing
  `SPELLVALUE_RADIUS_MOD`.
- **`AuraEffect::GetPeriodicTimer()` is the ms left to an aura's next tick**: an exact, lag-free
  clock for any summon or hit a periodic trigger aura drives (the heroic Nether Portal's Mistress
  every 6 s).
- **A spell a periodic trigger aura casts carries the aura's caster as original caster**, so its
  damage takes that caster's damage-done modifiers: Burning Inferno, cast by the Incinerate Flesh
  target, takes Jaraxxus's Nether Power.
- **A re-applied non-stacking aura recalculates its amounts** (`Aura::SetStackAmount`), so a ramp
  built in a periodic dummy restarts on every refresh.
- **A spell's Speed delays even a triggered instant**, so an aura sent with a telegraphed cast can
  land before the telegraphed hit (Fire Bomb's 66318 a second before its 66317). Read Speed and the
  corrections before timing a hazard off its aura.
- **A line laid by a stepping head is predictable; its patches come too late.** A Coldflame-style
  head (a `NullCreatureAI` that `NearTeleportTo`s N yd along its facing per tick and leaves a
  dynobject) lands its next patches where its position and orientation say. Dynobjects show only
  what has landed, so a dodge keyed on them alone is always late
  ([Lord Marrowgar](../raids/icecrown-citadel/lord-marrowgar.md)).
- **A charge's landing point is readable before he lands**: `boss->movespline->FinalDestination()`
  while `!movespline->Finalized()`, so whoever he will land on moves first (Onyxia, Marrowgar).
- **A charge on the farthest players right after a telegraphed cast** leaves the cast as the whole
  walk to the stands: whoever is still walking is the farthest, and the hit lands mid-walk
  (Marrowgar's first Bone Storm charge, 3 s in on 25-man). Keep every stand reachable within it.
- **A boss boundary evades him.** `Creature::Update` runs `CheckInRoom` against the AI's
  `SetBoundary` every 2.5 s while engaged, so a charge or tank drag that takes him outside resets the
  fight. Check stands against the instance script's boundary as well as the navmesh.
- **A `TARGET_DEST_TARGET_ENEMY` area spell picks its victims in `Spell::_cast`**, at the cast's end,
  so the cast bar is the whole dodge window and a Speed only delays the damage.
- **A `TEMPSUMMON_TIMED_DESPAWN` marker's age is `duration - TempSummon::GetTimer()`.** With the
  summoner's position and the trigger missile's Speed it gives each impact an exact, lag-free landing
  time (Rotface's explosion marks).
- **A channel cast through `DoCastSelf` names the caster as its unit target**, so read a cone's axis
  from the stalker it faces (Rotface's Slime Spray).
- **`NonTankTargetSelector` excludes only the current threat victim**, so an off-tank can take a
  "non-tank" spell and needs its dodge too (Festergut's Malleable Goo).
- **A random-target selector can be a positioning rule.** Putricide's `MalleableGooSelector` refuses
  anyone within 7 yd plus both reaches, so a raid standing inside that radius leaves the spell no
  target and it is never cast. Read the selector before designing a dodge.
- **A chain spell jumps by `IsWithinDist`**, both combat reaches added (10 yd magic default + 1.5 +
  1.5 = 13 yd centre to centre, 3D, in LOS, nearest first). `EffectChainAmplitude`
  (`SpellEffectInfo::CalcDamageMultiplier`) multiplies each jump in `Spell::DoAllEffectOnLaunchTarget`,
  so a chain can grow per jump, not only shrink.
- **A hazard the script reschedules on a phase edge** (engage, emerge, each cast start) can't fire
  before its earliest timer: arm the dodge only from that timer minus a lead, or facing swings in the
  dead window cause dodges.
- **A creature's totem shares entries with players' totems**; `Minion::InitStats` → `SetMinion`
  sets its `OwnerGUID`, so tell a hostile one apart by owner entry.
- **A script's own periodic damage can skip every mitigation.** `spell_valkyr_touch_aura` hits every
  player on the map (`ExcludeTargetAuraSpell` 0) with damage computed before absorbs.
- **`EventMap::ExecuteEvent` always erases the event**, so one not re-scheduled with `Repeat` runs
  once, whatever its comment says. It pops one event per call, so an `UpdateAI` that returns early
  while `UNIT_STATE_CASTING` banks every event through a long cast or channel, then fires the overdue
  ones one per tick: the seconds after it are a schedule
  ([VX-001](../raids/ulduar/mimiron.md#phase-2-is-a-healing-race-and-heat-wave-is-why)).
- **Some damage pushes back no cast.** `Unit::DealDamage` skips pushback for a DoT and for a spell
  flagged `SPELL_ATTR3_TREAT_AS_PERIODIC` or `SPELL_ATTR7_DONT_CAUSE_SPELL_PUSHBACK`, so a raid under
  such an area hit casts at full speed (Leeching Swarm's 66240).
- **A kick in a channel's first update does nothing.** `Spell::EffectInterruptCast` takes a spell
  that is `CASTING`, or `PREPARING` with a cast time; a channel has none
  ([Yogg-Saron's Drain Life](../raids/ulduar/yogg-saron.md#phase-3-cooldowns-and-casts)).
- **A boss whose script re-seeds threat on a timer never satisfies `TankHasHeldBoss`.** Exempt its
  entries through `BossHasNoStableVictim`
  ([consumables-and-burst.md](../systems/consumables-and-burst.md)).
- **A script may keep no boss state.** Without `SetBossNumber` (`GetEncounterCount()` 0: VoA, ToC
  before core #28117) boss state and its hooks say nothing; read the script's own progress `GetData`
  and `IsEncounterInProgress`. Such an override can write: ToC's old one reset its encounter to
  `NOT_STARTED` when no alive non-GM player was on the map, so never call one with nobody alive.
- **`OnAfterUpdateEncounterState` fires on every `Map::UpdateEncounterState` call**, matched row or
  not, synchronously inside `Unit::Kill` or `Spell::cast`: match credit type and entry in the handler
  and defer heavy work to the map update.
- **Flags beat `GetData`.** `UNIT_FLAG_PACIFIED | UNIT_FLAG_DISABLE_MOVE` on a boss is an exact,
  lag-free statement that he is now a fixed point worth anchoring geometry on.
- **Read which unit a link aura sits on before keying an exclusion on it.** Blood Mirror's 70821 is
  cast by the linked player onto the boss's victim, so "not the Blood Mirror player" excluded the
  victim twice and left the off-tank biteable.
- **A damage-share link copies each hit in that hit's own school.** `SPELL_AURA_SHARE_DAMAGE_PCT`
  (`Unit::DealDamage`) passes the copy through `CalcAbsorbResist` only: armour and percent
  damage-taken auras don't cut it, absorbs and school immunity do. A guide calling Blood Mirror's
  share "shadow damage" is wrong for this core.
- **An aura with duration -1 on a channelled spell lasts until the caster's channel ends**, often at
  its next non-triggered cast. Read the effect it applies (`UNIT_STATE_STUNNED` for `MOD_STUN`), not
  the aura's presence (heroic Harvest Souls 73654).
