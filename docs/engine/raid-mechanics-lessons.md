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
  without its escape. **Confirm with a receipt** — energy spent, the aura landed, the channel
  stopped. `Spell::prepare` calls `cast(true)` inline for instants, so `HasAura` on the same tick is
  real.
- **`CanCastVehicleSpell` reports `BAD_TARGETS` for a self-cast**, so the honest check is unavailable
  exactly where it is needed. Substitute `SpellInfo::CalcPowerCost` against live power, guarding
  `PowerType >= MAX_POWERS` first — `POWER_HEALTH` is `-2` and would index the power array out of
  bounds, and `Powers` has a signed underlying type, so cast `MAX_POWERS`.
- **Creature casters get no server-side cooldown.** `Spell::SendSpellCooldown` returns early for
  non-players with its `AddSpellCooldown` commented out. Never invent a manual cooldown to
  compensate: an invented one silently throws casts away for the rest of the fight.
- **Vehicle power is creature regen.** `unit_class 4` plus `UNIT_FLAG2_REGENERATE_POWER` gives
  `Creature::Regenerate` a flat +20 per `CREATURE_REGEN_INTERVAL` (2 s) — 10/s against a cap of 100.
  Cost per GCD against that rate decides whether the bar ever climbs at all.
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

Three rules that belong with the geometry rather than the action:

- **A spot checked only at its endpoint lets the walk cross the hazard it dodges.** Test the path,
  and a growing hazard (a spreading pool) at its size when the walk ends.
- **Park tolerance belongs in the clearance maths.** If a bot may stop `t` yards off the point, a
  point cleared by exactly the hazard radius still leaves whoever stopped on the hazard side inside
  it. Required clearance is `hazardRadius + tolerance`.
- **A loose tolerance is a feature**, not sloppiness: it is what stops a stacked formation piling
  onto a single coordinate.

## Range and reach

Every distance helper measures differently, and a large-model boss inflates all of them.

- Core distance helpers are surface-to-surface on combat reach: `GetObjectSize()` returns
  `UNIT_FIELD_COMBATREACH`, `IsWithinCombatRange` is `dist3d < d + reachSum`, and
  `GetMeleeRange = max(casterReach + targetReach + 4/3, NOMINAL_MELEE_RANGE)` — against a 20-reach
  boss, ~22.8 yd. Hard-coded stand distances that ignore reach break on large models.
- **A range literal in a boss script is not the distance it says.** `GetDistance2d(WorldObject
  const*)` subtracts both reaches too: Mimiron's Napalm picks among players at
  `GetDistance2d(mkII) > 15.0f`, which the MK II's CombatReach of 8 makes raw > ~24.5 yd. Convert
  before believing one — it decides who a mechanic may pick, and an empty pool can fall through to
  something quite different (Napalm's fallback is a threat-list pick with no distance floor). Our own
  code needs the same conversion: `reach spell` fires through `IsWithinCombatRange`
  (`OutOfRangeTrigger::IsActive`), so a formation clamped to raw `spellDistance - margin` gives away
  both reaches, 9.5 yd against the MK II, which parked Mimiron's camp on Napalm's floor. An area
  spell's radius goes the other way: `WorldObjectSpellAreaTargetCheck` adds the target's reach only
  for a player-controlled caster, so a creature's AoE is centre to centre and spacing just over its
  radius isolates.
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
  (`maxdist = dist + GetMeleeRange(obj)`).
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
- **A boss that opens on a random player** (`AttackStart(SelectTargetFromPlayerList(...))` when an
  intro ends) is answered by staging, not by taunt speed: the pickup tank waits inside his reach and
  taunts on the first selectable tick, and everyone else waits outside it until a tank holds him.

## Crowd control and threat on adds

What lands on a raid add is not what PvP experience predicts.

- **CC breaks on damage two ways.** `Unit::DealDamage` removes auras carrying
  `AURA_INTERRUPT_FLAG_TAKE_DAMAGE` on any hit: Polymorph, Freezing Trap. An aura with proc flags and
  no `PROC_TRIGGER_SPELL` gets a damage cap in `AuraEffect::CalculateAmount` (level-80 warrior base
  health / 4.75 = 2,652), and `HandleBreakableCCAuraProc` removes it past that, DoT ticks included:
  Fear, Frost Nova, Entangling Roots (66 of 67 Fears on Faction Champions broke within 30 ms of a
  hit). Cyclone has neither. Read both, not the reputation.
- **A stunned creature takes no heal assist threat**: `ThreatManager::ForwardThreatForAssistingMe`
  skips `UNIT_STATE_CONTROLLED` units.
- **Diminishing returns skip creatures.** `ApplyDiminishingToDuration` diminishes a creature only for a
  `DRTYPE_ALL` group — stun, cyclone, charge — or one carrying `CREATURE_FLAG_EXTRA_ALL_DIMINISH`.
  Root, fear and disorient are `DRTYPE_PLAYER`, so on an add with `flags_extra = 0` they land at full
  duration every time. The 10s PvP duration cap is gated the same way, and also needs a player
  caster. Both tests read the owner first, so a player's pet counts as a player. To predict the next
  duration, `Unit::GetDiminishing` returns the level the next hit lands at (non-const: it resets a
  stale entry).
- **Taunt diminishes only on a creature flagged for it.** `flags_extra` 0x80000
  (`OBEYS_TAUNT_DIMINISHING_RETURNS`) runs taunts at 100/65/42/27% duration, then immune until 15 s
  after the last one fades (`Unit::GetDiminishing`, `Spell.cpp` DR block). A taunt every 8 s plus a
  Righteous Defense in between keeps such a boss immune, so a plan built on repeated taunt-backs
  fails there. Onyxia and the Twin Val'kyr carry it.
- **Death Grip (49576 → 49560) splits its effects.** A creature immune to taunt and attack-me
  (`CreatureImmunitiesId` with aura 11 and effect 114) still takes the dummy pull, with no aura and
  so no DR. Where the taunt aura lands, the grip shares `DIMINISHING_TAUNT` with every class taunt,
  and at taunt-immune DR the whole spell misses (`SPELL_MISS_IMMUNE`), pull included.
- **A creature drops a victim it can't hurt.** `ThreatReference::ShouldBeSuppressed` takes out of
  contention a target immune to the attacker's melee school, confused, or under a breakable stun, so
  Hand of Protection moves melee attackers off a focused bot at once, as Divine Shield or Ice Block
  does.
- **Hunter pet Growl is `SPELL_EFFECT_THREAT` only, not a taunt.** Against a script that seeds player
  threat (Faction Champions' 10M × modifier) it does nothing but burn 15 focus. A multiplier can't
  veto pet autocast: turn it off with `Pet::ToggleAutocast`; the non-combat `toggle pet spell` turns
  it back on.
- **An add that calls `DoResetThreatList` on a timer can be neither tanked, taunted nor redirected.**
  Read its `UpdateAI` before designing any of the three. Freya's Detonating Lasher re-rolls a uniformly
  random player every 10s, which leaves geometry plus a snare as the only handling.
- **The swap tank not holding the boss must not damage him.** After a taunt both tanks build threat at
  the same rate, so the one carrying the debuff stacks takes him straight back, white swings included
  (no action carries those, so a damage-action veto misses them). A swap fired by the tank *not*
  holding him also never fires when that tank is a human: the bot holding him has to call it.
- **A pacing gate with a bypass recreates the stacking it exists to prevent.** Override it only for
  what is worse than the hit it delays (Algalon's star bypass put two raid-wide explosions 3.2 s
  apart, [algalon.md](../raids/ulduar/algalon.md)).
- **Reserve a cooldown against the next event by its own cooldown, not a window.** Hold whenever
  casting now leaves it cooling down at the event; a button whose lockout (Forbearance) blocks another
  counts as spending that one too (Algalon's Big Bang soakers).
- **The Aoe threat type misses most splash.** Starfall, Blade Flurry, Sweeping Strikes, Cleave, Bladestorm,
  HotR, Avenger's Shield, Multi-Shot, Chain Lightning, Seed of Corruption and Mind Sear fail even the spell
  area test. A hold on "don't hit the add next to the boss" needs `SpellSplashes` plus the named list in
  `RaidSplash.h`, and self auras held outright: they hit whatever comes near for their whole duration.
- **A draining add's lifetime is its health over its measured loss**, splash included, so trace drain
  and splash deaths apart before tuning, and plan each death against the others: killing a substitute
  when the soonest-dying one can't be touched stacks two explosions anyway, and dropping one a few %
  from dead moves nothing, its DoTs finish it inside the window (Algalon's stars).
- **A slot leash fights `reach spell`.** It walks half the gap to an out-of-range target, the slot node
  walks the bot back, and the target is never reached. Give the mover its own spot in range instead.
- **A taunt moves the boss, not the bot's own target.** A tank that taunts off an add keeps the add targeted, so
  reach walks it to the add, the slot walk back, and a damage guard zeroes every swing: set the target with the
  taunt, and retarget a holder that got the boss by threat.
- **A redirect's fallback target can be what pulls aggro**: Tricks of the Trade falls back to the
  strongest melee DPS ([pitfalls.md](pitfalls.md#detection-that-never-fires)).
- **Mocking Blow is a taunt.** It is a `MELEE_ACTION`, and an Arms warrior's snare chain falls back to it;
  `ClassifyAction` counts it in `Family::Taunt`, so a taunt guard has to cover every role, not tanks.
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

## What a strategy costs per raid

Cheap per bot, ruinous per raid — and invisible in single-bot testing.

- **A multiplier runs on every popped action that passed `isUseful()`**, dozens per bot per tick, so
  anything it derives is derived that many times: test the action first
  ([action-selection.md](action-selection.md#multiplier-semantics)). Encounter rules do it by
  construction.
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
  in one ms. `FindTargetValue` (`"find target"`) is built with interval 1 too.
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
  and only inside `|dx| + |dy| < r` at r = 200: `Cell::VisitCircle` drops three cells from each corner
  of the 7×7 block, so a creature 150-200 yd off on a diagonal can be missed. Past that bound run the
  sweep. Ids are ported in `UldEncounterGate.h`.
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
- **A rule predicate that constructs trigger objects pays their construction on every action it is
  asked about.** Read the helpers those triggers wrap instead, keeping each trigger's full condition.
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

**Timing** is `.playerbots pmon` ([../systems/observability.md](../systems/observability.md#timing-a-bot-playerbots-pmon)):
read ratios, never absolute times.

**Settled — do not re-audit:**

- No `checkInterval` or framework memo on `"possible targets no los"` / `"nearest npcs"`: both are
  global values, so a memo goes stale across ticks for every other reader.
- Do not run `isPossible` before the multipliers — Mimiron's and Algalon's multipliers have side
  effects, and vetoes are logged.
- `GetCreatureListWithEntryInGrid` is **not** a substitute for a LOS sweep: it is a grid container
  only, and `AllCreaturesOfEntryInRange` measures **2D** where `AnyUnitInObjectRangeCheck` measures
  **3D** (`GridNotifiers.h`).
- Leave the shared `GetFirstAliveUnitByEntry` alone: upstream marks it for removal and every raid
  calls it, so its replacement is a Custom-owned copy under another name, not an edit in place.
- Replicating `Cell::CalculateCellArea` + `VisitCircle` to skip the octagon walk is provably
  identical but copies ~40 lines of core internals that go silently wrong the day AC changes cell
  visiting. Rejected.

**Open:**

- `BossFireResistanceTrigger` / `BossFrostResistanceTrigger::IsActive` (`BossAuraTriggers.cpp`) run
  `FindBossByName` — a sight-range sweep plus a UTF-16 lowercase of every unit name — **before**
  `HasAura`, the spell-known check and the raid-group check, for 7 paladin and 3 hunter triggers in
  every raid. Reordering changes no behaviour.
- `Engine::LogAction` formats every engine event for any bot with a real player master
  (`LogInGroupOnly` defaults to 1), whether or not debug logging is on.
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
  health-gated.
- **Read a boss spell's full effect list, not its damage.** "Damage" spells carry riders that decide
  the whole reaction: Freya's Ground Tremor is `Effect_1` damage plus `Effect_2 = 68`
  `SPELL_EFFECT_INTERRUPT_CAST`, which silences the raid's school for 10s — the damage number says
  nothing about the mechanic that matters. Read `EffectRadiusIndex` on every effect too; index 28 is
  50000 yd, i.e. raid-wide and undodgeable.
- **`SpellInfoCorrections.cpp` can rewrite a DBC value.** Fire Bomb 66320's radius is 5 yd in the
  DBC and 2 yd in play, so read it before sizing a hazard.
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
- **A `TARGET_DEST_TARGET_ENEMY` area spell picks its victims in `Spell::_cast`**, at the cast's end,
  so the cast bar is the whole dodge window and a Speed only delays the damage.
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
  once, whatever its comment says.
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
