# Pitfalls

Failure modes this codebase has already hit, grouped by shape. Most produce **no error** — the bot
just quietly does nothing. Check this list before debugging "the trigger doesn't fire".

Vehicle riders, oscillating movement, reach maths, coordination and per-raid cost have their own
doc: [raid-mechanics-lessons.md](raid-mechanics-lessons.md).

## A trace only proves what the build behind it did

A pull recorded before a fix landed looks exactly like a pull where the fix did nothing. Three Freya
pulls on 2026-09-05 were read as evidence against code the running worldserver had never been built
with; the binary predated the commit by two hours.

Establish the build from inside the trace before reading anything else out of it: a node the change
added has to appear at least once, and an action a new multiplier zeroes has to show that multiplier's
label in a `veto` record instead of running at its own relevance. `AzerothCore rev.` in `Server.log` is
the *core* hash and says nothing about a module. The worldserver binary's mtime does
(`docker exec ac-worldserver ls -l --time-style=+%F_%R env/dist/bin/worldserver`) — it is UTC, so
convert before comparing it against a local commit time.

## A solved table outlives the traces it was solved from

Coordinates fitted offline stay fitted to the pulls available that day. Thorim's twelve Lightning Charge
shelters were solved against three pulls' boss positions; two later pulls widened that set enough to
push four rows out of their own cone margin, one to 38.1° against a 37.5° arc — a bot sheltered into the
cone it was dodging.

Re-verify every row against every trace you have before shipping, and treat a measured limit as
pass/fail: three of those shelters sat 7.6-8.2 yd off Sif's Blizzard track against a measured 9.8 yd
reach, because clearance was a solver preference it was free to trade away. Parse the shipped table back
out of the source to check it — the solver's own output only proves the solver agrees with itself.

## Names fail silently at runtime

Everything is wired by string. Nothing here is a compile error.

- A `TriggerNode` whose name has no `creators[...]` entry is skipped by `Engine::ProcessTriggers`
  with `continue` and **no warning**.
- An unregistered action logs `A:<name> - UNKNOWN`.
- Spell names resolve by string through the `"spell id"` value.
- `TwoTriggers`' creator key must exactly equal `getName()` = `"<name1> and <name2>"`.

Casualties found so far: `blade fury` (should be `blade flurry`), `"boost"` and `"high threat"`
(never registered as triggers), `conflagrate`, `chaos bolt`, `freezing trap on cc`,
`cure party member` (a base class — subclasses register under spell names). All fixed.

The two most recent were both in Ulduar and both had been dead since they were written, which is the
point: nothing anywhere reports them. `UldTriggerContext.h` registered
`"yogg-saron shadow resistance trigger**r**"`, so the node asking for
`"yogg-saron shadow resistance trigger"` never resolved; and `"thorim fall from floor action"` had a
class and a trigger but no `creators[...]` entry at all, so the node resolved its trigger and then
found nothing to run.

Sweep for these by checking every `NextAction`/`TriggerNode` name against `creators[...]`. Multiplier
`dynamic_cast` mistakes, by contrast, do fail at compile time.

Registration is not activation, and that fails just as quietly. `DpsAoeStrategy` is defined
(`src/Ai/Base/Strategy/DpsAssistStrategy.h:23`) and has a creator (`StrategyContext.h:245`), but no
`addStrategies*` call anywhere adds `"dps aoe"` — only the unrelated `"aoe"` — so it can never run.
Check for both the `creators[...]` entry **and** an `addStrategies*` mention.

## A moving bot cannot cast

`PlayerbotAI::CanCastSpell` / `CastSpell` refuse **any** spell with a non-zero cast time while
`bot->isMoving()` (`src/Bot/PlayerbotAI.cpp:3369-3379`, `:3746-3753`). The `StopMoving()` calls that
would let a bot stop and cast are commented out, so movement versus casting is resolved purely by
action priority.

This is the root of a whole class of "healers stop healing" bugs. The Sapphiron air phase was the
worst case: a position action at `ACTION_RAID + 1` recomputed an exact shelter point every tick
against a moving boss and a moving player, accepted arrival only within `0.35f`, and returned `true`
right after issuing a `MOVEMENT_FORCED` move — so healers slid in place for the whole phase and never
landed a cast.

The fix shape is **reach-then-hold with hysteresis**: accept a generous arrival deadband, latch
"arrived", `StopMoving()`, and issue no further move until the bot genuinely drifts out. Return
`true` while actually moving (the action owns the tick; heals could not fire anyway) and yield only
once stopped. Do **not** lower the action's priority or widen the multiplier whitelist — the mechanic
must still outrank heals while the bot is unsheltered.

## A bot mid-cast cannot be moved at all

The mirror of the rule above, and it silently defeats every dodge in the module.
`PointMovementGenerator<T>::DoInitialize` returns without launching a spline when
`unit->IsMovementPreventedByCasting()`, and `DoUpdate` calls `StopMoving()` and returns early on the
same test. `Unit::IsMovementPreventedByCasting` is true for any `UNIT_STATE_CASTING` except a channel
carrying `IsActionAllowedChannel` — so instants are fine and everything else is not.

Two things make it worse than "the move does nothing". `MovementAction::MoveTo` has its `CastStop` /
`InterruptSpell` block **commented out** (`MovementActions.cpp:318`, `:345`), so it returns `true` and
stamps `LastMovement` with the full travel delay for a leg that never started — which then blocks the
bot's own retries through `IsWaitingForLastMove` for the length of a walk it never took. And the
calling action reads that `true` as success and holds the tick. Nothing upstream can see this: the
node's verdict, the accepted-move record and the registered POINT generator all say it worked. Judge a
positioning node by measured displacement instead — Freya's Sun Beam dodge logged 158 accepted moves in
one pull while bots channelling Volley or Mind Sear went nowhere on 57-67% of them and died standing in
the beam.

A dodge that must not be missed interrupts **before** it moves, and the two calls are not
interchangeable. `bot->InterruptNonMeleeSpells(true)` is immediate: the cast ends inside the call, so
the `MovePoint` on the same tick launches its spline. `botAI->RequestSpellInterrupt()` only sets a flag
for a later `UpdateAIInternal`, so the move issued this tick is still pinned — it is for scripts
reacting to a boss event, not for an action that has to travel now. Gate the interrupt on
`IsMovementPreventedByCasting()` so it only fires when the bot is genuinely stuck, and decide it per
mechanic: clipping a cast every time a *survivable* hazard lands costs more than the hazard does.
Kara, Gruul, Magtheridon and Naxxramas already do this.

## Movement that silently no-ops

- **`MoveTo` does not validate the destination against the navmesh.** With `exact_waypoint = false,
  generatePath = true` it goes through `SearchForBestPath` (`MovementActions.cpp:240`, height check
  at `:1733`); an off-mesh point or a Z outside the height band returns `INVALID_HEIGHT` and `MoveTo`
  returns `false`. Raw ring geometry (`center + r·cos θ`) is exactly the shape that produces off-mesh
  points. Void Reaver's ranged never spread for this reason, and because
  `VoidReaverMaintainPositionsMultiplier` disables `CombatFormationMoveAction`, the spread action was
  the *only* thing that could position them — so they froze permanently.

  Two qualifiers. The navmesh half cannot fire on a map with no `.mmtile` files — `CalculatePath`
  short-circuits to a shortcut and every destination "paths". The height band survives only where
  there is height data: `GetMapHeight` reads vmaps *and* the raw `.map` surface, and a map shipping
  neither (Eye of Eternity) answers a flat 0.0 everywhere, pinning any non-flying Z to 0. And the
  resulting `PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH` (`0x11`) equals neither `PATHFIND_NORMAL` nor
  `PATHFIND_INCOMPLETE`, so any path-type test written with `==` rather than a mask rejects
  everything there; `MoveToLOS` is written that way and has no callers.

  `navprobe` answers both halves offline, per map and per point, without a pull. It lives in the
  **core fork** at `src/tools/navprobe`, not in this module, so it never shows up in module history.
  It is **prebuilt** in `acore/ac-wotlk-build:master`, and the tiles are in the `ac-client-data`
  Docker volume — **not** in the host's `env/dist/data`:
  `MSYS_NO_PATHCONV=1 docker run --rm -v azerothcore-wotlk-pb_ac-client-data:/azerothcore/env/dist/data:ro --entrypoint /azerothcore/env/dist/bin/navprobe acore/ac-wotlk-build:master --map <id> coverage`
  (without `MSYS_NO_PATHCONV=1`, Git Bash mangles the entrypoint into a Windows path). Read the
  **settled Z**, never the trailing "N/N on mesh" line — a point can report a nearest poly within
  2 yd and still settle to terrain, which is off the floor. The same value carries two labels: for a
  single point it prints as **`UpdateAllowedPositionZ`** (plus a `** Z would be rewritten by N yards
  **` line once the drop exceeds 1 yd); in the ring sweep and the JSON it is the **`settledZ`**
  column.

- **`NAV_MAGMA` is in the player path filter** (`PathGenerator::CreateFilter`), so a destination the
  navmesh flags as magma is **reachable, not rejected**. Do not discard a hand-measured point for
  sitting on lava — Obsidian Sanctum's pull-drag corner is exactly that.

- **Bots do not path like players.** `CreateFilter` has a `MOD_PLAYERBOTS` branch giving a bot
  `NAV_GROUND | NAV_WATER` **minus `NAV_GROUND_STEEP`**, plus `setAreaCost(NAV_WATER, 20.0f)`; humans
  and creatures keep the wider filter at no cost. Poly flags hold a single `NavTerrain` value and
  Recast merges overlapping spans by **max** area, so a shallow stream over a sloped bed collapses to
  `NAV_GROUND_STEEP` (0x10, the highest value) and walls off bots alone. `navprobe` ports the **human**
  branch and models no liquid at all, so every "navprobe-verified" coordinate in `docs/raids/**` was
  checked against the wrong filter. `--nav 0x09` approximates the bot one (a `NAV_GROUND_STEEP` poly
  then fails the include test); the 20x cost cannot be reproduced. To settle it live, `.mmap path`
  with **the bot selected** builds the path on that unit's own filter.

- **This fork names mmap files with a 3-digit map id**, not upstream's `%04i`:
  `"{}/mmaps/{:03}.mmap"` and `"{}/mmaps/{:03}{:02}{:02}.mmtile"` (`MMapMgr.h:48-49`). Any external
  script or tool written against upstream naming silently finds nothing at all.

  Latent, and worth knowing before trusting a tile name: the generator writes it as
  `(mapID, tileY, tileX)` (`MapBuilder.cpp:866`) while the loader parses it as `(mapId, x, y)`
  (`MMapMgr.cpp:71`).

  `FleePosition` (`MovementActions.cpp:2214`) picks a navmesh-validated destination via
  `BestPositionForRangedToFlee`, which is why it never fails this way. Prefer it where 5 yd of travel
  is enough (next bullet), or pass the platform/boss floor Z rather than `bot->GetPositionZ()` and
  shrink the radius until the path succeeds.
- **`FleePosition` caps travel at `AiPlayerbot.FleeDistance`** — `min(radius + 1, fleeDistance)`,
  default **5.0** (`MovementActions.cpp:2152`, `:2215`) — so `radius` is a request, not a distance,
  and a bot centred on a 10 yd blast steps 5 yd and eats it while the action returns `true`. It reads
  one hazard, and for non-tank melee the candidate angles are perpendicular to the current target or
  straight at it; away-from-the-hazard is a candidate only while `isTanking` (`:2119-2135`).
  `CheckLastFlee` then blacklists any angle within 45° of a recent flee's reverse for 5s, so the
  second hazard of a volley usually yields `Position()` → `false` → the bot resumes attacking inside
  the blast. Anything wider than ~5 yd, or arriving in numbers, wants
  `FindNearestPositionClearOfHazards` (`RaidBossHelpers.cpp`), which rings outward to the nearest spot
  clear of *every* hazard. Freya's Nature Bombs, Detonating Lashers and Unstable Sun Beams were all
  silently undodgeable until they moved onto it.
- **A movement lock refuses anything not *strictly* above it.** `IsWaitingForLastMove`
  (`MovementActions.cpp:951-963`) compares priorities with `>`, so a move already in flight refuses
  the next move of the *same* priority until its lock expires — `distance / speed`, capped at
  `AiPlayerbot.MaxWaitForMove` (5000 ms). `ACTION_*` relevance decides which action runs and has no
  bearing on whether its `MoveTo` is accepted, so an emergency dodge issued at the same priority as
  the routine hold that just fired is silently dropped while the bot walks on to the old destination.
  Obsidian Sanctum lost most of a Flame Tsunami's 3.6 s budget to a 2.9 s hold lock exactly this way.
  Emergency dodges want `MOVEMENT_FORCED` — and two of those in one encounter then deadlock each
  other, with no band above to escape into, so precedence between them has to be settled at the
  multiplier layer instead.

  It also hides from the caller's own log. `MoveTo` collapses every `RaidObs::MoveOutcome` into one
  `bool`, so code that records *why* it gave up — a dodge fan noting which hazard vetoed each bearing
  — reports a locked bot as "every bearing was hazardous". Count what `TryMoveTo` hands back too.
  Mimiron's fire dodge read as a saturated arena for two days on **1,249 locked moves against 387
  issued**.

  And frequency, not danger, decides who wins the tie. A hazard firing thousands of times a pull and
  one firing five times are not symmetric at equal `MOVEMENT_FORCED`: the frequent one holds the lock
  most of the time, and it re-issues *over* an escape already in flight, so the bot turns round
  mid-run. Promoting Mimiron's fire dodge to `MOVEMENT_FORCED` cancelled seven Shock Blast escapes
  1.5 s after they issued, and all seven died inside a blast they had already left. Reordering
  relevance does not fix it on its own — stand the frequent one down, in its trigger or a
  multiplier, for as long as any lethal one is live.
- **A multiplier that zeroes relevance is reported as `IMPOSSIBLE`.** `Engine::DoNextAction` takes
  the `else` of `if (action->isPossible() && relevance > 0)`, so a vetoed action is indistinguishable
  in the act stream from one whose spell is unknown, out of range or on cooldown. The `veto` rows
  name the multiplier that did it; read those before concluding an ability is broken.
- **A registered POINT generator does not mean the unit is moving.**
  `PointMovementGenerator::DoInitialize` returns **without launching a spline** while the unit has
  `UNIT_STATE_NOT_MOVE` (`ROOT|STUNNED|DIED|DISTRACTED`), and `MoveTo` reports success as soon as it
  calls `MovePoint`. A stunned unit therefore accepts move orders forever and never travels — in a
  trace, a bot holding one coordinate while its action keeps issuing accepted moves.
  `postmortem.py --stalls` is that query; it found a Flame Leviathan siege engine frozen for exactly
  60s with goals 122 yd away.
- **`IsDuplicateMove` is not the anti-oscillation guard it looks like.** It needs the request within
  **0.01 yd** of `lastMoveShort` (`MovementActions.cpp:939-949`), so any caller passing
  `bot->GetPositionZ()` re-issues a different point as soon as the bot moves on a sloped floor, and
  the pathfinding branch stores the navmesh-resolved Z rather than the requested one. What actually
  throttles a re-issuing action is the arrival tolerance plus the movement lock above.

- **Every `MoveTo` calls `mm->Clear()`.** So a high-priority node that re-derives its destination each
  tick cancels whatever walk a lower node had in flight, even when its own answer moved the bot a yard.
  Two such nodes alternate at tick rate and neither ever arrives: the bot logs tens of yards of travel
  inside a 5 yd strip, which reads in-game as "it refuses to cross". Freya's Sun Beam dodge and spore
  node did exactly this, 584 flips in one pull. Latch the destination and, while the walk is still in
  flight and still valid, **return true without calling `MoveTo`** — the engine ends the tick at the
  first action returning true, so claiming it is what protects the spline. Give the latch a ceiling, or
  a bot rooted mid-move holds the tick indefinitely.

- **A dodge must clear more than its own trigger radius.** `FindNearestPositionClearOfHazards` rings
  outward and returns the *first* clear spot, so a bot on the rim of a hazard it clears by one yard
  gets a ~2 yd step: it leaves by a hair, the next spawn puts it back inside the trigger, and the node
  re-fires forever at `MOVEMENT_FORCED`, starving everything under it. Size the clearance a few yards
  past the trigger radius, and fall back to the tight value only where overlap leaves nothing wider.

- **A gather node is a damage amplifier whenever the AoE it faces is wider than the camp — and the
  spread that fixes it usually costs more.** Freya's ranged camp pulls the raid into a 10 yd ball for
  the Detonating Lasher wave and Detonate reaches 15.5 yd: each of the ten deaths in a wave hits ~10
  bots, and six pulls lost 120 of 161 that way. Compare the two radii before writing a gather at all.
  Victims per blast is a **step function** of spacing, not a slope, so a spread is sized past the AoE
  radius or it buys nothing — which at Freya meant 16 yd slots, and the raid then stopped killing the
  wave: melee DPS halved, two of ten adds died, and the wipe came earlier than any camp pull. Weigh
  the AoE against everything else that kills, not against zero: Detonate was only 4.6-16.3% of the
  damage taken.

- **`ReachTargetAction` is the only generic way a bot closes on a hostile target, so a multiplier that
  zeroes it strands everyone out of range.** It is the base class of both `reach melee` and
  `reach spell`; every other generic mover retreats (`flee`, `runaway`), backs out of a hitbox, or
  follows the leader, and `CastSpellAction` out of range passes `isUseful`/`isPossible` and then fails
  silently inside `Spell::prepare` without asking for movement. The tempting reason to zero it: a
  positioning node returning false inside its tolerance — the right anti-churn shape — hands the tick
  to the generic chase, which walks the bot back to its target and re-fires the node next tick.
  Zeroing the chase does hold the formation, but a formation spaced wider than the bots' own reach
  cannot fight from its slots. Exempt the heal and resurrect reaches at minimum, or anyone standing
  outside the formation — a tank on the boss — stops being reachable.

- **A conjunctive gate can be dead code for a dozen pulls.** Freya's step-out, frost nova and trap all
  hung off "3+ lashers within 8 yd *and* none above 20% health", which held **once in six pulls**: the
  adds scatter by design and die one at a time. Conditions that each look reasonable can still
  describe a state the encounter never reaches, and only firing counts reveal it — `postmortem.py`
  verdict counts against the number of chances the node had.

- **`MoveInside(..., distance = 0)` effectively never returns false** — `MovementActions.cpp:1692`
  returns false only when `GetDistance2d <= distance`. The action then succeeds every tick,
  out-prioritises combat, and pins every melee on one exact point at zero DPS. `MoveNear` offsets by
  `cos(angle) * distance`, so at 0 the whole raid stacks on a single coordinate.
- **`ReachCombatTo` paths to the boss's origin** (`MovementActions.cpp:802-857`) and returns false on
  any path type outside `NORMAL|INCOMPLETE|SHORTCUT`. On a large-model boss the origin sits 10+ yd
  inside the model, possibly off-mesh. `ReachMeleeAction` is the *only* NextAction behind
  `enemy out of melee`, so when this fails the bot simply stands there — manually summoning it into
  melee range is the tell.
- **`ReachCombatTo`'s prediction offset is measured against a different point than the trigger.** It
  advances the target up to 3 yd along the boss's facing, then early-outs against that predicted
  point, while the trigger and `isUseful()` measure the actual position. While a boss moves and the
  bot is behind it, the trigger can fire forever and the action decline forever.
- Its stand point is `reachSum + 0.75` against a core swing limit of `reachSum + 1.33` — a 0.58 yd
  margin, and `PathGenerator::ShortenPathUntilDist` "settles for a guesstimate", so the real stop
  distance is never below the intended one.

## Things the bot cannot see

- **`"find target"` walks only the bot's own threat list** (`FindTargetValue`, via
  `GetThreatenedByMeList()`) and requires an **exact-length, case-insensitive match** against
  `creature_template.name`. A melee bot parked on Thane never resolves Zeliek, and every Auriaya
  trigger lost the boss the same way to a Sanctum Sentry. A pacified or CC'd bot has attacked
  nothing, so it fails for exactly the bot a rescue node exists to serve. For multi-boss encounters,
  non-attacking objects, or anything not yet on threat, use `GetFirstAliveUnitByEntry`.
- **`AvoidAoeAction` only sees three things**: a dynobject aura, a damaging trap GameObject, or a
  `UNIT_FLAG_NOT_SELECTABLE` trigger NPC. Mechanics outside those — Anub'rekhan's Impale and Locust
  Swarm, the Four Horsemen's Void Zone NPC 16697 (SmartAI, casts on update) — are invisible to it.
  **The answer is pre-emptive spread, not reactive avoidance.**
- **Non-selectable stalkers never enter attack-target lists at all** (Freya's beam stalkers
  33170/33050, Mimiron's flame nodes), so they cannot be found through `"possible targets"`. Scan the
  `"nearest npcs"` GuidVector instead. Watch the inverse too: Freya's root creatures 33088/33168
  *are* selectable, so a bot will happily kill its own root.
- `AttackersValue::IsPossibleTarget` drops `UNIT_FLAG_NOT_SELECTABLE` units
  (`Value/AttackersValue.cpp:156`), so a phased-out boss leaves `"attackers"` entirely.
- **`DisperseDistanceValue` defaults to `-1.0f`**, and `CombatFormationMoveAction::Execute` bails on
  `dis <= 0` — the generic de-clumper is inert unless a strategy sets it.

## Detection that never fires

The recurring fatal shape is **a mechanic wired to a cast the bots cannot observe**. Two encounters
shipped broken this way:

- **Heigan**: the dance advanced by watching the boss cast Eruption (29371). The boss never casts it —
  `instance_naxxramas.cpp:254-267` erupts via floor GameObjects. `NextSafe()` was never called, so
  every bot parked on one waypoint for the whole 90 s phase.
- **Noth**: the threat-reset protection keyed on catching `UNIT_STATE_CASTING` with Blink. The core
  casts it as `CastSpell(me, SPELL_BLINK, true)` — **a triggered instant never sets
  `UNIT_STATE_CASTING`** and is gone before the next bot tick.

Both fixes are the same: find the adjacent *non-triggered* cast (Noth's Cripple 29212) or the
resulting aura, or build a timer model anchored on a phase edge.

Related traps:

- **Positional phase checks false-positive at the pull.** Heigan's DB spawn is on the platform, 0.8 yd
  from the platform check point, so bots believed they were in the fast dance from the moment of the
  pull. Prefer a state read — `boss->ToCreature()->GetReactState() == REACT_PASSIVE` is exact and has
  no lag. Unit flags work the same way (`UNIT_FLAG_NOT_SELECTABLE` for Noth's balcony,
  `UNIT_FLAG_DISABLE_MOVE` for Gothik's).
- **`GetData` is a completion flag as often as a live signal.** Vezax's `GetData(1)` returns
  `lootMode == 3`, set only *after* the Animus dies. Mimiron's `GetData(1)` is authoritative but sits
  on a unit that never becomes a bot target. Thorim's `SPELL_SIF_CHANNEL_HOLOGRAM` (64324) is defined
  but **never cast** in this core. Prefer a boss's empower aura where one exists — it also tells you
  *which* thing is active, not just how many.
- **Scheduled events outlive their cause.** Freya's empower events are scheduled once at pull and
  repeat unconditionally, even after the Elder dies — so a hard-mode reaction must key off the hazard
  world object, never off an Elder still being alive.
- **Spell ids differ by difficulty and the mapping is not uniform.** Heigan's 25-man Decrepit Fever is
  **55011**, not 29998, so a raw `HasAura(29998)` dispelled nothing in 25-man; Eruption, Spell
  Disruption and Plague Cloud have no difficulty rows at all. Check
  `spelldifficulty_dbc` per spell and use `NaxxSpellIds::HasAnyAura(unit, {…})`. The table is in the
  **world DB** and the client `SpellDifficulty.dbc` can be empty for the same spell (Mimiron's Plasma
  Blast, 62997 → 64529), so a DBC-only check reads as "no remap". It binds boss casts, not just player
  auras: `FindCurrentSpellBySpellId` or `m_spellInfo->Id ==` on the 10-man id silently never matches.
  Mimiron's Plasma Blast defensive was dead for two pulls that way, tank dying to it twice in each.
  Sweep a raid's constants against the table in one pass — 28 of Ulduar's 139 remap, three of the
  checks were reading only the 10-man id.

## Before the pull, and out of combat

- **`AttackStop()` drops the victim and nothing else.** The `"current target"` AI value survives it —
  and that value is what the class rotation casts at and what `ReachTargetAction` walks to. Clear it
  explicitly: `context->GetValue<Unit*>("current target")->Set(nullptr)` (precedent
  `ICCActions_LK.cpp:690`, `SWPActions_Felmyst.cpp:386`). Blocking re-acquisition in a multiplier
  cannot undo a target the bot has already picked up.
- **Eating and drinking blind the bot.** `DrinkAction`/`EatAction` (`NonCombatActions.cpp:65`, `:125`)
  set `SetNextCheckDelay` to 12-18 s scaled by what is missing, so nothing runs, dodges included,
  until it expires. Wherever a hazard outlives combat, such as a phase handover, veto `drink`/`food`
  near it (`MimironDrinkGuardMultiplier`).
- **An encounter gate that requires `boss->IsInCombat()` leaves the strategy inert through the whole
  approach and the instant of the pull.** Generic tank and DPS behaviour therefore picks targets
  first, and the boss-specific rules inherit whatever state that left behind.
- **A boss resolved by a wide grid search is resolved long before the pull** — a 200 yd search reaches
  the instance entrance in Obsidian Sanctum. So any per-instance state stamped on first sight starts
  its clock at zone-in, and a `lastSeenMs` staleness guard can never fire. Re-anchor on the boss's
  **combat edge** instead.
- **A druid tank loses bear form out of combat.** Every druid non-combat node carries
  `/*P*/ { NextAction("caster form") }`, and `CastCasterFormAction::Execute` is a bare
  `RemoveShapeshift()`. `CheckMountStateAction` also reaches `Mount()` with no current target, and
  `Mount()` calls `RemoveShapeshift()` **before** the cast — so the form dies even where the mount
  cannot succeed. `bear form` lives in the combat-only `BearDruidStrategy`, so nothing shifts him
  back. This bites any strategy whose holds run out of combat. Suppress `CastCasterFormAction` for
  **tanks only** — a cat-spec druid still needs caster form for Rebirth.

## Upstream merges

- **PR #2592 renamed the master/player predicates, and one of them changed meaning while keeping its
  spelling.** `HasRealPlayerMaster()` ≡ `HasGameClientMaster()`; `HasActivePlayerMaster()` ≡ the free
  `IsRealPlayer(master)`; the old **member** `IsRealPlayer()` ≡ the free `IsSelfBot(player)`. So
  `IsRealPlayer` used to mean "is a selfbot" and now means "is a plain player with no bot AI" — same
  name, inverted truth value, no compile error.
- **The four raid registration sites are pure include/name lists** (`RaidStrategyContext.h`,
  `BuildSharedActionContexts.cpp`, `BuildSharedTriggerContexts.cpp`, `GetInstanceStrategies()`), so a
  merge that drops either side **silently unregisters raid strategies** and nothing fails to compile.

## State does not cross objects

Every bot has its own `AiObjectContext`, and within it the trigger, the action and the multiplier each
hold **their own helper instance**. Anything they must agree on has to be shared explicitly — a
file-static `std::unordered_map<ObjectGuid, State>` defined in exactly one `.cpp`, declared in the
header.

For a per-instance clock (shared across bots), use the `PhaseStateFor` pattern: static mutex plus
`unordered_map<instanceId, State>` with a staleness re-anchor.

**A handout standing in for looting has to consume its source.** `StoreNewItem` off a corpse that
nothing marks spent hands out a fresh item every time the last is used: Mimiron's core carrier minted
32 Magnetic Cores off two corpses in 24 s. Take the item off the corpse's own `loot` (`is_looted`,
`unlootedCount`, `NotifyItemRemoved`) and claim the corpse per instance for when loot was never filled
(`TakeMimironCore`).

`GenericBossHelper<BossAiType>` is **unusable** when the boss AI class is file-local to its `.cpp`
(Anub'rekhan, Gothik, Heigan) or when the script is `TaskScheduler`-driven with no `events` member.
Fall back to the plain-`AiObject` + timer-model pattern (`GluthBossHelper`, `HeiganBossHelper`).

## A targeting mark never clears inside a pull

`prioritized targets` is written by `AttackMyTargetAction` (`src/Ai/Base/Actions/AttackAction.cpp:47`)
and `AttackRtiTargetAction` (`src/Ai/Base/Actions/ChooseTargetActions.cpp:168`), and cleared **only**
by `PlayerbotAI::Reset` — that is, on leaving combat. Inside one pull the mark never lets go, and a
wrong mark is unrecoverable: `FindTargetStrategy::IsHighPriority`
(`src/Ai/Base/Value/TargetValue.cpp:124`) returns true for the skull icon *and* for anything in
`prioritized targets`, which trips `foundHighPriority` and short-circuits every smart strategy.
`DpsTargetValue` (`src/Ai/Base/Value/DpsTargetValue.cpp:281`) compounds it by calling
`RtiTargetValue::Calculate()` first and returning it unconditionally on a hit — alive, LOS and sight
distance are the only checks.

This is a base-engine trap, not a boss-specific one. **Any encounter with an untargetable phase hits
it**: the bot is held on a dead or wrong target for the rest of the fight. An encounter that retargets
mid-pull must clear the mark itself — Thorim sets both the group icon and its own `prioritized
targets` back to empty once per pull (`UldEncounter_Thorim.cpp:740`).

## Role and index traps

- `IsAssist{Tank,Heal,RangedDps}OfIndex(..., ignoreDeadPlayers)` **defaults to `false`**, which
  silently deletes a role when its holder dies. Passing `true` shifts indices past dead members and
  gives free auto-promotion (`PlayerbotAI.h:437-439`).
- `botAI->GetMeleeIndex()` counts tanks too.
- **`IsRanged()` includes healers.** With the default `bySpec = false` it is just
  `ContainsStrategy(STRATEGY_TYPE_RANGED)` (`PlayerbotAI.cpp:1905`), so a holy priest answers it. Any
  fixed ranged position gated on `IsRanged` therefore drags healers off heal range; use `IsRangedDps`
  when the intent is "the ones that only need to be in cast range of the boss".
- `FindTankTargetSmartStrategy::IsBetter` (`Value/TankTargetValue.cpp:76-84`) short-circuits: an
  explicit main tank in a group with more than one tank **sticks to `current target`** and rejects
  every alternative. A main tank that picked up an add during a phase where the boss was untargetable
  never goes back to the boss on its own.
- `PartyMemberToProtect` keys on `attacker->GetVictim()`, so anyone dying to raid-wide AoE, a DoT or a
  ground effect is nobody's current victim and **can never be selected**. Pain Suppression, Blessing
  of Protection and Intervene cannot answer that damage pattern.
- `PartyMemberToDispel::Calculate` returns **one** target per tick, so N decursers all chase the same
  first target unless duty is split by index. `HasAuraToDispel` also skips auras with less than
  `dispelAuraDuration` (default 700 ms) remaining.
- Class dispel nodes sit **below** `ACTION_RAID`: mage `remove curse on party` at 40, druid at 57
  against `ACTION_RAID` 60. Any raid positioning node outranks them.
- **Boarding changes what a rider is, in two ways that break tests written for a bot on foot.**
  `Vehicle::AddPassenger` roots every passenger, driver included — `SetControlled(true,
  UNIT_STATE_ROOT)` (`Vehicle.cpp:437`), cleared only on exit — so `UNIT_STATE_NOT_MOVE` on a rider
  means "seated", not "disabled", and a role gated on it elects nobody. That switched off Flame
  Leviathan's tar lead, vent interrupt and corner posting at once, silently, for six pulls. Test
  `UNIT_STATE_STUNNED` on the rider and `UNIT_STATE_NOT_MOVE` on the hull. A seat carrying
  `VEHICLE_SEAT_FLAG_PASSENGER_NOT_SELECTABLE` also sets `UNIT_FLAG_NOT_SELECTABLE`
  (`Vehicle.cpp:395`), which every AoE searcher skips (`GridNotifiers.h:1154`): a crewed bot is
  invisible to boss AoE and a dismounted one absorbs all of it, so on a vehicle fight hull loss is
  the death, not the damage that follows it.

## Arc and geometry defaults

`WorldObject::isInFront` / `isInBack` default to `arc = M_PI` (`Object.h:558-559`) — front half plus
back half is the entire circle, so a check like `boss->isInFront(bot) || boss->isInBack(bot)` is
**always true**. On Sapphiron this made a priority-61 repositioning action permanently active and
starved the melee rotation for the whole ground phase.

`IsBotInFrontalCone` forwards to `HasInArc`, which takes the **full** arc, not the half-angle — an
`M_PI / 2` argument leaves everyone between 45° and 60° off-centre inside a cone they believe they
cleared.

**An aura's removal conditions are not all in the DBC.** `spell_linked_spell` can strip one on hit: a
negative `spell_effect` at `type 1` (`SPELL_LINK_HIT`) becomes `RemoveAurasDueToSpell` in
`Spell::DoAllEffectOnTarget`. Flame Leviathan's Hodir's Fury stun reads as permanent from `Mechanic`,
`DispelType` and `AuraInterruptFlags` — all zero — and is in fact removed by "Flames" (65044/65045),
which a demolisher throws every time it uses Hurl Boulder. Searching for what *casts* the remover
also finds nothing: it is an `EffectTriggerSpell` two levels below the spellbook entry
(`62306 → 62307 → 65045`). Follow the trigger chain and check the link table before calling anything
undispellable.

**A spell's shape is its implicit target type, not what the boss appears to be doing.**
`TARGET_UNIT_CONE_ENEMY_*` is a cone off the caster; `TARGET_DEST_TARGET_ENEMY` (53) centres the blast
on the **victim**, wherever that is, and the caster's facing is irrelevant. Flame Leviathan's Battering
Ram is the second and was modelled as the first: the resulting "stay out of his front" test saw a
third of the real hits while over half of what it did fire on dragged a vehicle off station for
nothing.

Core distance helpers are surface-to-surface on combat reach: `GetObjectSize()` returns
`UNIT_FIELD_COMBATREACH` (`Object.cpp:2888`), `IsWithinCombatRange` is `dist3d < d + reachSum`, and
`GetMeleeRange = reachSum + 4/3`. Hard-coded stand distances that ignore reach break on large models.
`GetDistance2d(WorldObject const*)` subtracts both reaches too, so a **boss script's** own filter is
never the number written in it: Mimiron's Napalm picks among players at `GetDistance2d(mkII) > 15.0f`,
which the MK II's CombatReach of 8 makes raw > ~24.5 yd. Convert before believing a range literal —
it decides who a mechanic may pick, and an empty pool can fall through to something quite different
(Napalm's fallback is a threat-list pick with no distance floor at all). Our own code needs the same
conversion: `reach spell` fires through `IsWithinCombatRange` (`RangeTriggers.cpp:155`), so a
formation clamped to raw `spellDistance - margin` gives away both reaches, 9.5 yd against the MK II,
which parked Mimiron's camp on Napalm's floor. An area spell's radius goes the other way:
`WorldObjectSpellAreaTargetCheck` (`Spell.cpp:9203`) adds the target's reach only for a
player-controlled caster, so a creature's AoE is centre to centre and spacing just over its radius
isolates.

## Configuration

- **Raid cheats are on by default**: `AiPlayerbot.BotCheats = "food,taxi,raid"`. A mechanic that is
  only survivable because of the raid cheat is (a) not real play and (b) a silent wipe on any server
  that turns the cheat off. See the CHEAT-ONLY list in [../raids/README.md](../raids/README.md).
- `errorDelay` defaults to 100 ms and is compared as `(now - lastErrorTell) < errorDelay / 1000`
  (`PlayerbotMgr.cpp:1709`) — integer division makes that 0 seconds, so errors flush every tick.
  **Still unfixed.**
