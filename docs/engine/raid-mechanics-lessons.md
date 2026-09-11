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
- **`MoveTo` / `ReachCombatTo` clamp Z to terrain** via `bot->UpdateAllowedPositionZ`. On an airborne
  rider that is a dive to the floor and a climb back, on repeat.
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
- **`PlayerbotAI::CastVehicleSpell` returns `true` even when `prepare()`'s `CheckCast` failed**
  (`src/Bot/PlayerbotAI.cpp:4055`). No power, no combo points (`SPELL_FAILED_NO_COMBO_POINTS`) and
  GCD collisions all look like success. **Verify by aura**: `Spell::prepare` calls `cast(true)`
  inline for instants (`Spell.cpp:3691`), so `HasAura` on the same tick is real.
- **`CanCastVehicleSpell` reports `BAD_TARGETS` for a self-cast**, so the honest check is unavailable
  exactly where it is needed. Substitute `SpellInfo::CalcPowerCost` against live power, guarding
  `PowerType >= MAX_POWERS` first — `POWER_HEALTH` is `-2` and would index the power array out of
  bounds, and `Powers` has a signed underlying type, so cast `MAX_POWERS`.
- **Creature casters get no server-side cooldown.** `Spell::SendSpellCooldown` (`Spell.cpp:4346`)
  returns early for non-players with its `AddSpellCooldown` commented out. Never invent a manual
  cooldown to compensate: an invented one silently throws casts away for the rest of the fight.
- **Vehicle power is creature regen.** `unit_class 4` plus `UNIT_FLAG2_REGENERATE_POWER` gives
  `Creature::Regenerate` a flat +20 per `CREATURE_REGEN_INTERVAL` (2 s) — 10/s against a cap of 100.
  Cost per GCD against that rate decides whether the bar ever climbs at all.
- **Do not borrow another instance's vehicle trigger.** `GroupFlyingTrigger` (Oculus) requires the
  *master* to be mounted, so one human who has not taken their vehicle freezes the whole flight.

## Movement that oscillates

Bots jiggling on the spot is one symptom with six causes, plus a seventh that freezes them instead.

- **Two owners** — see above.
- **A destination that chases a moving boss.** A boss-relative spot flips to his far side while he is
  still walking. Prefer a spot **latched for the pull**. If it must track him, **clamp, don't
  chase**: keep the bearing the spot already holds, slide only along the segment `[boss, spot]`, and
  keep it continuous — at the clamp distance the clamped point must *equal* the unclamped one. A
  threshold that snaps between two positions is the bouncing mechanism itself.
- **A hazard sweep that restarts from a fixed base.** "First clear heading from base" re-finds ground
  *behind* the formation the moment a hazard back there expires, and the whole formation reverses.
  Latch the heading and sweep **forward only** from where it already is; an expiring hazard behind
  then cannot be selected. Hazards that outlive their own cadence guarantee this — 20 s fields on a
  12 s timer reversed the EoE flight on roughly every third one.
- **A path-type test written with `==`.** A path carries flags, not a value:
  `PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH` is `0x11` and equals neither constant, so an equality
  test rejects every path on a map without mmaps (Eye of Eternity is the only such map here) and on
  anything flying or swimming. Always mask.
- **A generic mover fighting a boss action.** The suppression multiplier in
  [../raids/README.md](../raids/README.md) is the right shape, but `AttackAction` **is** a
  `MovementAction` ([action-selection.md](action-selection.md)), so the exemption list must name the
  boss's own attack and vehicle actions or they die with everything else.
- **A dodge that steps further than the hold's arrival tolerance.** The hold's trigger fires the
  moment the dodge finishes, walks the bot back onto the hazard, and the two alternate for the
  hazard's whole life. Either keep the step smaller than the tolerance, or teach the hold to reject a
  destination the hazard covers. Obsidian Sanctum's fissure dodge steps 10 yd against a raid-line
  tolerance of 8 and needed the second fix.
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

Two rules that belong with the geometry rather than the action:

- **Park tolerance belongs in the clearance maths.** If a bot may stop `t` yards off the point, a
  point cleared by exactly the hazard radius still leaves whoever stopped on the hazard side inside
  it. Required clearance is `hazardRadius + tolerance`.
- **A loose tolerance is a feature**, not sloppiness: it is what stops a stacked formation piling
  onto a single coordinate.

## Range and reach

Every distance helper measures differently, and a large-model boss inflates all of them.

- `GetMeleeRange = max(casterReach + targetReach + 4/3, NOMINAL_MELEE_RANGE)` — against a 20-reach
  boss, ~22.8 yd. Hard-coded stand distances break on large models.
- **`Spell::CheckRange` adds `GetMeleeRange` to a spell's *minimum*** for `SPELL_RANGE_RANGED`, so a
  20-reach boss turns a hunter's 5 yd minimum into ~28 yd centre-to-centre and every shot returns
  `SPELL_FAILED_TOO_CLOSE`. **Hunters are the only class with a minimum range** — do not relocate the
  whole ranged group for them.
- That same reach keeps `EnemyTooCloseForSpellTrigger` permanently active for anyone standing near,
  and every class wires it to an escape at **34–50 relevance, above `ACTION_MOVE` (30)**. A
  positioning action in that band loses to the bot's own escapes: suppress `FleeAction`,
  `RunAwayAction`, `CastBlinkBackAction` and `CastDisengageAction`, or it steps out and is dragged
  back forever.
- Pick the helper deliberately: `IsWithinCombatRange` adds **both** combat reaches,
  `WorldObject::IsWithinDist3d(Position const*, float)` adds **neither**, and `GetExactDist2d`
  ignores altitude — the last claims reach that is not there against anything hovering.
- **`Unit::IsWithinMeleeRange(obj, dist)`: `dist` is extra slack**, not an absolute
  (`maxdist = dist + GetMeleeRange(obj)`).
- A target that walks needs a **sticky margin** on the reach test, or a bot on the edge of range
  swaps target every other tick. Re-test the sticky pick against the cap anyway: stickiness alone is
  a tow rope, and a mob that retargets drags the bot across the room until something outranks it.
  The cap is per role — Freya's 12 yd melee leash reused for ranged put their focus permanently out
  of reach, so they fell through to the boss.
- **Selection units must match margin units.** Picking by `GetHealth()` while the switch margin is in
  percentage points inverts the pick between adds with unequal max health.

## Crowd control and threat on adds

What lands on a raid add is not what PvP experience predicts.

- **Roots and CC do not break on damage in 3.3.5.** `Unit::DealDamage` removes exactly one thing: auras
  carrying `AURA_INTERRUPT_FLAG_TAKE_DAMAGE` (`Unit.cpp:1032`). Frost Nova and Entangling Roots do not
  carry it, so they hold their full duration through raid AoE; Freezing Trap does, so it never survives
  one. Read the flag, not the reputation.
- **Diminishing returns skip creatures.** `ApplyDiminishingToDuration` diminishes a creature only for a
  `DRTYPE_ALL` group — stun, taunt, cyclone, charge — or one carrying `CREATURE_FLAG_EXTRA_ALL_DIMINISH`.
  Root, fear and disorient are `DRTYPE_PLAYER`, so on an add with `flags_extra = 0` they land at full
  duration every time. The 10s PvP duration cap is gated the same way.
- **An add that calls `DoResetThreatList` on a timer can be neither tanked, taunted nor redirected.**
  Read its `UpdateAI` before designing any of the three. Freya's Detonating Lasher re-rolls a uniformly
  random player every 10s, which leaves geometry plus a snare as the only handling.
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
- **Latch per instance, not per bot**: `thread_local std::unordered_map<uint32 /*instanceId*/, T>`,
  safe unlocked because a bot is only ever updated from its own map's thread. Use it for phase,
  creature lookups, layout choices and any held heading.
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

- **`Multiplier::GetValue` runs once per queued action per bot per tick** (`Engine.cpp:188`), dozens
  of calls per bot. Anything it derives is derived that many times.
- **Role lookups are not cheap.** `IsMainTank` → `GetMainTankGuid` walks every group member, and each
  `IsTank()` → `ContainsStrategy` scans that member's strategy list; `IsDps`/`IsRanged`/`IsHeal` are
  the same shape. Across a 25-man that is tens of thousands of list scans a tick. Snapshot the roles
  on a window — `Strategy::InitMultipliers` builds one multiplier per bot, so there is no sharing to
  worry about.
- **Do not stack cache windows.** Caching an already-cached value again leaves the second layer
  enforcing the previous answer for up to a full window after the first moved on. Cache the expensive
  leaves; read the cheap composite live.
- **Grid sweeps walk every `SIZE_OF_GRID_CELL` (66.67 yd) cell inside the radius**, and twenty-five
  bots re-answer the same instance-wide question every tick — EoE peaked near 250 identical sweeps a
  tick. One instance-keyed creature cache collapses that to one. **Cache guids, not pointers**, so a
  despawn inside the window drops out instead of dangling; and make the fill radius **wide**, since
  the sweep is anchored on whichever bot refreshed it while its answer serves the whole raid.
- **Split on action family before testing anything.** Where everything a multiplier suppresses is
  either a `MovementAction` or a `CastSpellAction` — disjoint families — one `dynamic_cast` each way
  up front replaces a chain of a dozen for every rotation spell that falls through.
- **`Trigger` supports throttling and almost nothing uses it.** The ctor is
  `Trigger(botAI, name, checkInterval = 1)`, normalised as
  `checkInterval == 1 ? 1 : (checkInterval < 100 ? checkInterval * 1000 : checkInterval)` — pass
  `200` for 200 ms, and **never pass 2–99**, which silently means seconds. Any trigger starting a
  multi-tick reaction the `MotionMaster` carries on with runs at 200–300 ms invisibly; triggers
  gating positioning or vehicle steering must stay per-tick.
- `Engine::ProcessTriggers` `Check()`s an inactive trigger **once per node** referencing it, so a
  trigger wired to three nodes is evaluated three times while it is quiet.

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

Detection failures are catalogued in [pitfalls.md](pitfalls.md); these are the ones specific to
scripted raid encounters.

- **A triggered instant publishes nothing.** `DoCastAOE` leaves `m_targets.GetUnitTargetGUID()`
  empty, and `m_Events.AddEventAtOffset(…, 3s)` has no cast to observe at all. Where the boss AI
  stores victims in its own guid slots, read those — they are filled at the *warning*, which is the
  entire reaction window.
- **Mirror core enum values, do not include them.** Script headers are not on a module's include
  path. Mirror with a comment naming the source enum and the entry's position in it.
- **A phase heuristic must separate the pull intro from a mid-fight transition.** Both read as "in
  combat, boss non-attackable". Full health separates them whenever the real transitions are
  health-gated.
- **Confirm which spell id the difficulty in play actually uses.** A rule built on the wrong twin is
  built on the wrong numbers, and a clean lookup on one id proves nothing about the other. Freya's
  Detonate is 62598 in 10-man and 62937 in 25-man; Ground Tremor 62437 and 62859.
- **Read a boss spell's full effect list, not its damage.** "Damage" spells carry riders that decide
  the whole reaction: Freya's Ground Tremor is `Effect_1` damage plus `Effect_2 = 68`
  `SPELL_EFFECT_INTERRUPT_CAST`, which silences the raid's school for 10s — the damage number says
  nothing about the mechanic that matters. Read `EffectRadiusIndex` on every effect too; index 28 is
  50000 yd, i.e. raid-wide and undodgeable.
- **Flags beat `GetData`.** `UNIT_FLAG_PACIFIED | UNIT_FLAG_DISABLE_MOVE` on a boss is an exact,
  lag-free statement that he is now a fixed point worth anchoring geometry on.
