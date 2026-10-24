# Pitfalls

Failure modes this codebase has already hit, grouped by shape. Most produce **no error** — the bot
just quietly does nothing. Check this list before debugging "the trigger doesn't fire".

Vehicle riders, oscillating movement, reach maths, coordination and per-raid cost have their own
doc: [raid-mechanics-lessons.md](raid-mechanics-lessons.md). Traps in reading a trace — a stale
build, a probe that never reaches the file, what a trace cannot see — are in
[../systems/observability.md](../systems/observability.md).

## A solved table outlives the traces it was solved from

Coordinates fitted offline stay fitted to the pulls available that day. Thorim's twelve Lightning
Charge shelters were solved against three pulls' boss positions; two later pulls pushed four rows
out of their own cone margin, one to 38.1° against a 37.5° arc. Re-verify every row against every
trace you have before shipping, and treat a measured limit as pass/fail: three of those shelters sat
7.6-8.2 yd off Sif's Blizzard track against a measured 9.8 yd reach, because clearance was a solver
preference it was free to trade away. Parse the shipped table back out of the source to check it —
the solver's own output only proves the solver agrees with itself.

**A tuned constant carries its trace's layout as an unwritten premise.** Hodir's `ULDUAR_HODIR_BUFF_WALK` 15 was
fitted to pulls where a usable buff stand sat inside 15 yd on 53.7% of ranged samples. Parking the
boss on the Toasty Fire moved every legal stand out to the helper arc, and the same 15 then reached
**6.6%**: the rule quietly became "never walk". When a change moves what a fitted constant measures
against, re-derive it in the same cycle, and print the availability it is fitted to beside the share
it lets through — the share alone cannot tell a tight budget from a dead one.

**A navprobe-clean point can still be a wrong point.** Navprobe answers "is this on the mesh", one
constraint of several: a Thorim camp solve that ignored Sif's Blizzard lane put two points 5.6 and
6.8 yd from the bunny's path, both probing clean. The hazard check is its own step, and a failed
point means **re-solving the whole set against every constraint** — nudging one moves the violation
to a neighbour.

## Names fail silently at runtime

Everything is wired by string. Nothing here is a compile error.

- A `TriggerNode` whose name has no `creators[...]` entry is skipped by `Engine::ProcessTriggers`
  with `continue` and **no warning**.
- An unregistered action logs `A:<name> - UNKNOWN`.
- Spell names resolve through the `"spell id"` value (`SpellIdValue`), a case-insensitive match of
  the whole 3.3.5 DBC name, hyphens included. A wrong name resolves to 0 and the action never casts,
  unreported — and so does every `PlayerbotAI::CanCastSpell(name)` / `CastSpell(name)` test, so a
  guard asking by name is dead too. DK `anti magic shell` and `anti magic zone` ("Anti-Magic …") and
  paladin `blessing of protection on party` (spell `blessing of protection`; 3.3.5 has Hand of
  Protection) are dead this way: no bot cast
  Anti-Magic Shell in twelve Yogg-Saron pulls until the tank-defensive ladder tested its id
  (`RaidTankDefensive.cpp`). Resolve a hyphenated or renamed spell by id.
- `TwoTriggers`' creator key must exactly equal `getName()` = `"<name1> and <name2>"`.

**`tools/pblint/pblint.py` is this sweep.** Point it at the paths a commit touches; it exits non-zero
on a name no context can build. It checks both directions, and needs to: a creator nothing
references is the only trace of a typo hidden in a string array no node scan sees. It reads
[encounter definition](../raids/README.md#file-layout-and-wiring) rows through their `Name`
constants. Also errors: a rule listing an unregistered action, and a `creators[]` entry duplicating a
row, where registration order decides whether the bot gets the gated trigger. The constructor checks
(`ctor-name`, `trigger-interval`) only match a parameter spelled `AI` or `botAI`, missing the `ai`
that about half of Ulduar's and Naxx's constructors take.

**Still open**, all base and class, none reported anywhere at runtime:

| Node | Registered as |
|---|---|
| DK `rune strike`, paladin `blessing of might`, `master loot roll`, DK `mind freeze on enemy healer` | on the other side — an action referenced as a trigger, or the reverse |
| `grounding totem`, `reset`, `tank aoe`, `location stuck`, `stay line`, chat `naxx`/`bwl`, `team flagcarrier near`, `freezing trap on cc` | nothing at all: either delete the reference or write the creator |

**The runtime half is `postmortem.py --coverage`**, and neither tool subsumes the other. pblint
proves a name has a creator somewhere in the tree, statically. Coverage proves the node carried
current on the pull you ran, and catches a name registered in a context this bot's own stack does
not include. A creator nothing references has no runtime counterpart, and a correctly wired node
whose condition is never true is invisible to pblint. The shapes, both dead from the day they were
written: `UldTriggerContext.h` registered `"yogg-saron shadow resistance trigger**r**"`, so the node
never resolved; `"thorim fall from floor action"` had a class and a trigger but no `creators[...]`
entry, so the node fired and found nothing to run.

Registration is not activation, and that fails just as quietly. `DpsAoeStrategy`
(`DpsAssistStrategy.h`) has a creator (`StrategyContext.h`), but no `addStrategies*` call adds
`"dps aoe"` — only the unrelated `"aoe"` — so it never runs. Check for both the `creators[...]`
entry **and** an `addStrategies*` mention.

## A moving bot cannot cast

`PlayerbotAI::CanCastSpell` / `CastSpell` refuse **any** spell with a non-zero cast time while
`bot->isMoving()`. The `StopMoving()` calls that would let a bot stop and cast are commented out, so
movement versus casting is resolved purely by action priority.

This is the root of a whole class of "healers stop healing" bugs: Sapphiron's air-phase shelter
action at `ACTION_RAID + 1` recomputed an exact point every tick against a moving boss and player,
accepted arrival only within `0.35f`, and returned `true` right after a `MOVEMENT_FORCED` move, so
healers slid in place all phase and never landed a cast.

The fix shape is **reach-then-hold with hysteresis**: accept a generous arrival deadband, latch
"arrived", `StopMoving()`, and issue no further move until the bot genuinely drifts out. Return
`true` while actually moving (the action owns the tick; heals could not fire anyway) and yield only
once stopped. Do **not** lower the action's priority or widen the multiplier whitelist — the mechanic
must still outrank heals while the bot is unsheltered.

## A seated bot's first cast hands the tick to the alternative

`PlayerbotAI::CastSpell` stands a seated bot up and returns false, so the engine runs the node's
alternative in the same tick, now standing. Gate a fallback alternative on its own precondition:
eating after Soul Link, Demonology cast `demon armor` instead of `fel armor` on every pull until
`CastDemonArmorAction::isUseful` refused while Fel Armor is known.

## A bot mid-cast cannot be moved at all

The mirror of the rule above, and it silently defeats every dodge in the module.
`PointMovementGenerator<T>::DoInitialize` returns without launching a spline when
`unit->IsMovementPreventedByCasting()`, and `DoUpdate` calls `StopMoving()` and returns early on the
same test. `Unit::IsMovementPreventedByCasting` is true for any `UNIT_STATE_CASTING` except a channel
carrying `IsActionAllowedChannel` — so instants are fine and everything else is not.

It is worse than "the move does nothing". `MovementAction::MoveTo` has its `CastStop` /
`InterruptSpell` block **commented out**, so it returns `true` and stamps `LastMovement` with the full
travel delay for a leg that never started, which then blocks the bot's own retries through
`IsWaitingForLastMove`; and the calling action reads that `true` as success and holds the tick. The
node's verdict, the accepted-move record and the registered POINT generator all say it worked. Judge
a positioning node by measured displacement instead — Freya's Sun Beam dodge logged 158 accepted
moves in one pull while bots channelling Volley or Mind Sear went nowhere on 57-67% of them.

A dodge that must not be missed interrupts **before** it moves, and the two calls are not
interchangeable. `bot->InterruptNonMeleeSpells(true)` ends the cast inside the call, so a `MovePoint`
on the same tick launches. `botAI->RequestSpellInterrupt()` only sets a flag for a later
`UpdateAIInternal`, so the move issued this tick is still pinned — it is for scripts reacting to a
boss event. Gate the interrupt on `IsMovementPreventedByCasting()`, and decide it per mechanic:
clipping a cast every time a *survivable* hazard lands costs more than the hazard does. Kara, Gruul,
Magtheridon and Naxxramas already do this.

A bot mid-cast also runs **no triggers**: `UpdateAIInternal` yields while its spell prepares. So a
mechanic that punishes an ongoing cast (Mistress' Kiss) can only be answered from another bot's
tick, by `RequestSpellInterrupt` on the caster. A channel runs triggers but fails `CanCastSpell`, so a
caster mid-cast or mid-channel never kicks either (Yogg-Saron's Drain Life nudge).

## An ability lock makes a portal or spell click a silent no-op

`GameObject::Use` on a goober and `Unit::HandleSpellClick` make the **user** cast the spell,
untriggered. Bladestorm and Killing Spree carry `SPELL_AURA_ALLOW_ONLY_ABILITY`, which sets
`PLAYER_ALLOW_ONLY_ABILITY`, and `Spell::CheckCast` then fails every untriggered cast with
`SPELL_FAILED_SPELL_IN_PROGRESS`: the call returns, nothing happens. Cancel the aura first, under the
client's cancel rules (`YoggSaronCancelAbilityLock`); a warrior went insane at Yogg-Saron's exit
portal this way.

## Movement that silently no-ops

- **`MoveTo` does not validate the destination against the navmesh.** With `exact_waypoint = false,
  generatePath = true` it goes through `MovementAction::SearchForBestPath`; an off-mesh point or a Z
  outside the height band returns `INVALID_HEIGHT` and `MoveTo` returns `false`. Raw ring geometry
  (`center + r·cos θ`) is exactly the shape that produces off-mesh points: Void Reaver's ring spread
  froze its ranged this way, since `VoidReaverMaintainPositionsMultiplier` disables
  `CombatFormationMoveAction` and the failed spread was the *only* thing positioning them.
  `FleePosition` picks reachable ground and never fails this way: prefer it where 5 yd of travel is
  enough (below), or pass the platform or boss floor Z rather than `bot->GetPositionZ()` and shrink
  the radius until the path succeeds.

  **`exact_waypoint = true` skips `SearchForBestPath` entirely, so such a node structurally cannot log
  `nopath`** — its absence is not evidence the destination was reachable. Instead `MoveTo` returns
  `ok` and the core lays a **straight-line spline**: one bot travelled **91.3 yd vertically over
  12.5 yd horizontally in 8.5 s**, `ok=1` throughout. `ok` says a command was issued, never that a
  route exists; **closing distance is the only evidence of a real path**, so a forced walk that matters
  wants a progress latch, not a return value (`YoggSaronWalkMakingProgress`).

  The navmesh half cannot fire on a map with no `.mmtile` files — `CalculatePath` short-circuits to a
  shortcut and every destination "paths". The height band survives only where there is height data:
  `GetMapHeight` reads vmaps *and* the raw `.map` surface, and a map shipping neither (Eye of
  Eternity) answers a flat 0.0 everywhere, pinning any non-flying Z to 0. And the resulting
  `PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH` (`0x11`) equals neither `PATHFIND_NORMAL` nor
  `PATHFIND_INCOMPLETE`, so a path-type test written with `==` rather than a mask rejects every path
  there, and on anything flying or swimming. Always mask; `MoveToLOS` is written with `==` and has no
  callers.

  `navprobe` answers both halves offline, per map and per point, without a pull. It lives in the
  **core fork** at `src/tools/navprobe`, not in this module, so it never shows up in module history;
  it is prebuilt in the `acore/ac-wotlk-build:master` image, and the tiles are in the
  `ac-client-data` Docker volume — **not** in the host's `env/dist/data`. Run it from Git Bash with
  `MSYS_NO_PATHCONV=1`, or the entrypoint is mangled into a Windows path. Read the **settled Z**,
  never the trailing "N/N on mesh" line — a point can report a nearest poly within 2 yd and still
  settle to terrain, off the floor. The value carries two labels: **`UpdateAllowedPositionZ`** for a
  single point (plus a `** Z would be rewritten by N yards **` line past a 1 yd drop), and the
  **`settledZ`** column in the ring sweep and the JSON.

  **Floor is not sight, and navprobe answers both.** `los X1 Y1 Z1 X2 Y2 Z2` casts the static half
  of `Map::isInLineOfSight`; `ring ... --los-from X Y Z` adds a sight column to a slot sweep — the
  real question is "which of these slots can see the boss". With no coordinates `los` reads 6-tuples
  from stdin, one verdict a line: loading the vmap tree dominates, so batch the rays in one
  invocation. The eye is `--collision` above both endpoints, matching `IsWithinLOSInMap`.
  **GameObject collision needs a live server**, and a map with no vmap tree reads every ray as clear
  (`coverage` warns).

  **A gameobject floor is invisible to navprobe.** The static mesh and vmaps have nothing under it,
  so every point reads off mesh with no height: there the offline check is impossible, not failed.
  Live, a bot's `MoveTo` onto it takes the straight-line `0x11` shortcut, clipped only by the static
  LOS ray, and its Z comes from the object's dynamic collision. ToC's arena (GO 195527) is the case.

- **A floor check clips a derived point back to a gameobject's edge.**
  `EncounterHelpers::ValidateFloorPoint` runs `Map::CheckCollisionAndGetValidCoords` **from the bot**,
  which raycasts the **dynamic** tree as well as the navmesh, so a point behind a solid gameobject
  comes back pulled to its near edge, with no verdict to read. Hodir's Toasty Fire (`62819` summons
  GO **194300**, display 7748 `Bonfirenorthrend_01.m2`, bounds ±2.4 yd at scale 0.5, in `vmaps/GameObjectModels.dtree`) put
  four tank anchors meant 4.5 yd past it **1.2-1.7 yd** from its centre. Nothing on the server
  collides a walking bot with a gameobject, so for a spot on or past a prop use
  `ValidateStaticFloorPoint`: the same navmesh raycast and path-type filter without the dynamic pass.

- **`NAV_MAGMA` is in the player path filter** (`PathGenerator::CreateFilter`), so a destination the
  navmesh flags as magma is **reachable, not rejected**. Do not discard a hand-measured point for
  sitting on lava — Obsidian Sanctum's pull-drag corner is exactly that.

- **Bots do not path like players.** `CreateFilter` has a `MOD_PLAYERBOTS` branch giving a bot
  `NAV_GROUND | NAV_WATER` **minus `NAV_GROUND_STEEP`**, plus `setAreaCost(NAV_WATER, 20.0f)`; humans
  and creatures keep the wider filter at no cost. Poly flags hold a single `NavTerrain` value and
  Recast merges overlapping spans by **max** area, so a shallow stream over a sloped bed collapses to
  `NAV_GROUND_STEEP` (0x10, the highest value) and walls off bots alone. `navprobe` ports the **human**
  branch and models no liquid, so every "navprobe-verified" coordinate in `docs/raids/**` was checked
  against the wrong filter. `--nav 0x09` approximates the bot one (a `NAV_GROUND_STEEP` poly then
  fails the include test); the 20x cost cannot be reproduced. To settle it live, `.mmap path` with
  **the bot selected** builds the path on that unit's own filter.

- **This fork names mmap files with a 3-digit map id**, not upstream's `%04i`:
  `"{}/mmaps/{:03}.mmap"` and `"{}/mmaps/{:03}{:02}{:02}.mmtile"` (`MMapMgr.h`, `MAP_FILE_NAME_FORMAT` /
  `TILE_FILE_NAME_FORMAT`). A tool written against upstream naming silently finds nothing. Latent,
  and worth knowing before trusting a tile name: the generator (`MapBuilder.cpp`) writes it as
  `(mapID, tileY, tileX)` while `MMapMgr.cpp` parses it as `(mapId, x, y)`.

- **`FleePosition` caps travel at `Playerbots.FleeDistance`** — `min(radius + 1, FleeDistance)`,
  default **5.0**, in `BestPositionForMeleeToFlee` / `BestPositionForRangedToFlee` — so `radius` is a
  request, not a distance, and a bot centred on a 10 yd blast steps 5 yd and eats it while the action
  returns `true`. It reads one hazard; for a non-tank melee with a target the candidate angles are
  perpendicular to that target or straight at it, and away-from-the-hazard is a candidate only while
  `isTanking`. `CheckLastFlee` then blacklists any angle within 45° of a recent flee's reverse for 5 s,
  so the second hazard of a volley usually yields `Position()` → `false` and the bot resumes attacking
  inside the blast. It also refuses outright while the shared `"recently flee info"` list holds a flee
  younger than `minInterval` (default 1000 ms), whichever action made it. Anything wider than ~5 yd, or
  arriving in numbers, wants `EncounterHelpers::FindNearestPositionClearOfHazards`
  (`src/Util/EncounterHelpers`), which rings outward to the nearest spot clear of *every* hazard.
  Freya's Nature Bombs, Detonating Lashers and Unstable Sun Beams were silently undodgeable until they
  moved onto it.

- **A movement lock refuses anything not *strictly* above it.** `MovementAction::IsWaitingForLastMove`
  compares priorities with `>`, so a move in flight refuses the next move of the *same* priority until
  its lock expires — `distance / speed`, capped at `Playerbots.MaxWaitForMove` (5000 ms). `ACTION_*`
  relevance decides which action runs and has no bearing on whether its `MoveTo` is accepted, so an
  emergency dodge at the same priority as the routine hold that just fired is silently dropped while
  the bot walks on to the old destination: Obsidian Sanctum lost most of a Flame Tsunami's 3.6 s
  budget to a 2.9 s hold lock. Emergency dodges want `MOVEMENT_FORCED` — and two of those in one
  encounter deadlock each other, with no band above to escape into, so precedence between them is
  settled at the multiplier layer. The same rule refuses a walk re-aiming its own `MOVEMENT_FORCED`
  walk: lower the booking first.

  **The booking outlives the action that made it**: a forced kite walk kept a tank walking into a
  hole 1.7 s after it took the boss, and refused its exit too. Drop the booking on a role change, and
  let a walk that must land within a second drop one it didn't make. An encounter rule's `Exclusive`
  holds movement only; holding a cast or an item use needs a multiplier.

  It also hides from the caller's own log. `MoveTo` collapses every `MoveOutcome` into one `bool`, so
  code recording *why* it gave up — a dodge fan noting which hazard vetoed each bearing — reports a
  locked bot as "every bearing was hazardous". Count what `TryMoveTo` hands back too: Mimiron's fire
  dodge read as a saturated arena on **1,249 locked moves against 387 issued**.

  Frequency, not danger, decides who wins the tie. A hazard firing thousands of times a pull holds
  the lock most of the time against one firing five times at equal `MOVEMENT_FORCED`, and re-issues
  *over* an escape in flight, so the bot turns round mid-run: promoting Mimiron's fire dodge to
  `MOVEMENT_FORCED` cancelled seven Shock Blast escapes 1.5 s after they issued, and all seven died in
  a blast they had already left. Reordering relevance does not fix it alone — stand the frequent one
  down, in its trigger or a multiplier, while any lethal one is live.

  A single mover is refused its own re-target. An action re-issuing every leg toward a goal that
  moves waits out its own stale leg at the priority already in flight: Flame Leviathan's drive had
  57–59% of COMBAT re-targets refused, its hulls driving up to 5 s toward where the boss had been.
  Such an owner lowers `last movement`'s priority one step before re-issuing, and restores it when
  nothing issued. That also removes the hold a re-planned dodge got for free: its legs now replace
  each other, and Flame Leviathan's siege engines inside several Inferno patches flipped sides of the
  trail every 200-400 ms and burned in place. Latch such a goal and re-validate it
  ([raid-mechanics-lessons.md](raid-mechanics-lessons.md#movement-that-oscillates)).

- **An open encounter gate is not a scope, and a hand-pushed multiplier has none.** The encounter gate
  ([raid-mechanics-lessons.md](raid-mechanics-lessons.md#what-a-strategy-costs-per-raid)) wraps
  triggers (`EncounterGatedTrigger`) and definition multipliers and rules (`EncounterGatedMultiplier`,
  `EncounterRuleMultiplier`), and `RaidEncounterRules::GateOpen` holds open every encounter not yet
  killed while no other is in progress. A multiplier pushed straight into `InitMultipliers` has no gate
  at all, and runs against every action of every bot on the map. Neither of these is the gate it reads
  as:
  - a **config read**: `IsMimironHardModeActive` returns `sPlayerbotAIConfig.UlduarMimironHardMode`
    and nothing else, so Mimiron's `avoid aoe` veto landed 15 times in Thorim's gauntlet and its
    formation guard swept 200 yd twice a tick there.
  - a **live-boss test over a guid lookup**: `InstanceScript::GetCreature` answers from anywhere on
    the map, so `VezaxEncounterActive`'s "he is alive" went true the moment his grid loaded and zeroed
    `dps assist`, `tank assist` and caster damage instance-wide — all 116 `dps assist` ticks of a
    Razorscale pull vetoed.

  Gate on the boss being **in combat**, and pair a guid lookup with a room screen
  (`IsNearMimironRoom`) or a combat test.
- **The gate's boss ids are a copy of AC's.** `UldEncounterGate.h` ports `ulduar.h`, which AC
  renumbers: #28079 moved Freya from 7 to 9. A stale id keys the gate to another boss's state, so
  that boss's whole strategy stays off (every node NEVER, banner `strategy-off`) while whichever
  strategy holds the live id in the stale copy opens instead. Re-check ported ids after each core merge.
- **A registered POINT generator does not mean the unit is moving.**
  `PointMovementGenerator::DoInitialize` returns **without launching a spline** while the unit has
  `UNIT_STATE_NOT_MOVE` (`ROOT|STUNNED|DIED|DISTRACTED`), and `MoveTo` reports success as soon as it
  calls `MovePoint`. A stunned unit accepts move orders forever and never travels: in a trace, a bot
  holding one coordinate while its action keeps issuing accepted moves. `postmortem.py --stalls` is
  that query; it found a Flame Leviathan siege engine frozen for exactly 60 s with goals 122 yd away.
- **`IsDuplicateMove` is not the anti-oscillation guard it looks like.** It needs the request within
  **0.01 yd** of `lastMoveShort`, so any caller passing `bot->GetPositionZ()` re-issues a different
  point as soon as the bot moves on a sloped floor, and the pathfinding branch stores the
  navmesh-resolved Z rather than the requested one. What actually throttles a re-issuing action is the
  arrival tolerance plus the movement lock above.
- **A walk stopped short is never re-issued.** The same check refuses an *exact* repeat for
  `MaxWaitForMove` (5000 ms) whether or not the bot is still moving, so an action that holds its
  destination gets `Duplicate` while the bot stands wherever something stopped it:
  `UseItemAction::UseItem` stops a moving bot for anything but an instant potion (a mana gem runs at
  relevance 90), and Disengage throws a hunter back. Hodir lost a mage 6.5 yd short of its dodge and a
  hunter out of its shelter that way. Clear `last movement` once the bot has stood still a beat past
  the issue (`ReleaseStalledWalk`, 500 ms). `MOVEMENT_FORCED` does not lift this: forced does not
  outrank forced. The flip side: `Duplicate` on a bot still moving means it is already walking
  there, not that the move was refused.

- **A dodge must clear more than its own trigger radius.** `FindNearestPositionClearOfHazards` rings
  outward and returns the *first* clear spot, so a bot on the rim of a hazard it clears by one yard
  gets a ~2 yd step: the next spawn puts it back inside the trigger, and the node re-fires forever at
  `MOVEMENT_FORCED`, starving everything under it. Size the clearance a few yards past the trigger
  radius, and fall back to the tight value only where overlap leaves nothing wider.

- **A sweep that moves for the sake of moving needs a travel floor.** `FindNearestPositionClearOfHazards`
  re-checks each spot against the hazards after the collision check pulls it back, which protects a
  dodge, whose own spot is inside what it flees. A bot that only has to keep moving already stands
  clear, so at a wall the pulled-back probe *is* its own spot, it wins the ring, and `MoveTo` refuses
  it as `there`/`dup`: Hodir's Biting Cold shed stood two casters still for ~12 s until their stacks
  killed them. Pass an `accept` that rejects spots under the travel the mechanic needs. With an
  **empty hazard list** it returns `Position()`, so a caller that must always move passes at least
  one circle (its own feet).

- **A warning band is a permanent dodge once the field carries enough marks.** Padding a hazard's
  reach buys reaction time per mark and costs coverage per volley: Thorim's Hammer drops 8 marks at
  once across a 200x200 box for 24 s, so at `ULDUAR_FL_TOWER_HAZARD_MARGIN` (8 yd) Flame Leviathan's
  vent reserve dodged through a whole 10 s channel while inside the real 7 yd circle for one second
  of it, and lost the channel by drifting out of Electroshock range. A hull that owes the raid a
  firing position dodges on the strict circle and steps somewhere it can still fire from;
  nearest-clear is the wrong ranking for it. This holds against a short-lived mark only: the same
  rules against Mimiron's Inferno, a patch that burns 30 s and costs a siege engine 10% a tick, let
  three reserves wait until inside one and then drive across the trail toward him.

- **A gather node is a damage amplifier whenever the AoE it faces is wider than the camp — and the
  spread that fixes it usually costs more.** Freya's ranged camp pulls the raid into a 10 yd ball for
  the Detonating Lasher wave and Detonate reaches 15.5 yd: each of the ten deaths in a wave hits ~10
  bots, and six pulls lost 120 of 161 that way. Compare the two radii before writing a gather at all.
  Victims per blast is a **step function** of spacing, so a spread is sized past the AoE radius or it
  buys nothing — 16 yd slots at Freya, and the raid then stopped killing the wave: melee DPS halved,
  two of ten adds died, and the wipe came earlier than any camp pull. Weigh the AoE against everything
  else that kills, not against zero: Detonate was only 4.6-16.3% of the damage taken.

- **`ReachTargetAction` is the only generic way a bot closes on a hostile target, so a multiplier that
  zeroes it strands everyone out of range.** It is the base of both `reach melee` and `reach spell`;
  every other generic mover retreats (`flee`, `runaway`), backs out of a hitbox, or follows the leader,
  and `CastSpellAction` out of range passes `isUseful`/`isPossible` and then fails silently inside
  `Spell::prepare` without asking for movement. The tempting reason to zero it: a positioning node
  returning false inside its tolerance — the right anti-churn shape — hands the tick to the generic
  chase, which walks the bot back to its target and re-fires the node. Zeroing the chase holds the
  formation, but one spaced wider than the bots' own reach cannot fight from its slots. Exempt the
  heal and resurrect reaches at minimum, or anyone outside the formation — a tank on the boss — stops
  being reachable; the Reach family holds those too, so block `reach melee` and `reach spell` by name.

- **A claim that says "this one is mine to kill" needs a liveness test, or it shields the target.**
  `FlameLeviathanHeldByAnotherPost` keeps the fleet off an add a posted siege engine has aggro on. But
  the post's guns leave a dead band — Fire Cannon will not fire under 10 yd, Ram is a cone — and an add
  sitting in it was safe from everyone: 71.7% of held frames, dying at 1.8 %/s against 12.1 where a gun
  bore. Gate the claim on the holder still having a shot, not on it holding.

- **A conjunctive gate can be dead code for a dozen pulls.** Freya's step-out, frost nova and trap all
  hung off "3+ lashers within 8 yd *and* none above 20% health", which held **once in six pulls**: the
  adds scatter by design and die one at a time. Only firing counts reveal it — `postmortem.py` verdict
  counts against the number of chances the node had.

- **`MoveInside(..., distance = 0)` effectively never returns false** — it returns false only when
  `GetDistance2d <= distance`. The action then succeeds every tick, out-prioritises combat, and pins
  every melee on one exact point at zero DPS. `MoveNear` offsets by `cos(angle) * distance`, so at 0
  the whole raid stacks on a single coordinate.
- **`ReachCombatTo` paths to the boss's origin** and returns false on any path type outside
  `NORMAL|INCOMPLETE|SHORTCUT`. On a large-model boss the origin sits 10+ yd inside the model,
  possibly off-mesh. `ReachMeleeAction` is the *only* NextAction behind `enemy out of melee`, so when
  this fails the bot simply stands there — manually summoning it into melee range is the tell.
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
  non-attacking objects, or anything not yet on threat, use `GetFirstAliveUnitByEntry` — **which
  upstream has marked `// DO NOT USE, WILL BE REMOVED`** (`src/Util/EncounterHelpers.h`). It is still
  the right answer; Ulduar already calls its own copy, `GetUldUnitByEntry`
  (`Raid/Uld/Util/UldTargets.h`), and other raids take the same route when it goes. Every
  recommendation of it in `raids/` inherits this caveat. It also stops at `SightDistance` (100 yd): a
  far bot gets null, and shared encounter state reset on "no boss" is wiped for the whole raid — Flame
  Leviathan's posted engines flapped `fl.pursued` 26 times in one pull. A boss the instance script
  tracks resolves at any range through `InstanceScript::GetCreature`.
- **`AvoidAoeAction` only sees three things**: a dynobject aura, a damaging trap GameObject, or a
  `UNIT_FLAG_NOT_SELECTABLE` trigger NPC whose periodic-trigger aura casts school damage. Mechanics
  outside those — Anub'rekhan's Impale and Locust Swarm, the Four Horsemen's Void Zone NPC 16697
  (SmartAI, casts on update) — are invisible to it. **The answer is pre-emptive spread, not reactive
  avoidance.** On a trigger unit it fires once `GetDistance` (both combat reaches subtracted) is
  inside the damage spell's radius, and moves by `FleePosition`, so an encounter with its own mover
  for that hazard vetoes it, or the two fight. Two defects of its own, left alone because it runs in
  every encounter in the game:
  - `AvoidUnitWithDamageAura` **ignores the flee's result and returns false**, so at
    `ACTION_EMERGENCY` (90) it takes the movement and still hands the tick on, every tick a trigger is
    in range: on Mimiron's flame nodes, 642 evaluations, zero wins, 619 accepted moves.
  - Its rate limiter is dead: `isUseful` reads `lastMoveTimer`, which is set only inside the
    `TellWhenAvoidAoe` branch.

  Its flees also fill the shared `"recently flee info"` list, so every other `FleePosition` caller on
  that bot is refused for the next second (above): 71% of Mimiron's old flame dodge's FAILEDs had a
  flee by the same bot in the previous second.
- **Non-selectable stalkers never enter attack-target lists at all** (Freya's beam stalkers
  33170/33050, Mimiron's flame nodes), so they cannot be found through `"possible targets"`. Scan the
  `"nearest npcs"` GuidVector instead. Watch the inverse too: Freya's root creatures 33088/33168
  *are* selectable, so a bot will happily kill its own root.
- `AttackersValue::IsPossibleTarget` drops `UNIT_FLAG_NOT_SELECTABLE` units, so a phased-out boss
  leaves `"attackers"` entirely, and `GetFirstAliveUnitByEntry` and `GetUldUnitByEntry`, reading
  `"possible targets"`, lose a submerged boss or a spike the same way. Resolve those through the
  instance guid slot or a grid search.
- **A `NullCreatureAI` unit stays out of combat, so out of `"attackers"`, until something hits it.**
  `dps assist` swaps a bot off it every tick unless the encounter vetoes `DpsAssist` for that bot. An
  out-of-combat critter also fails `IsPossibleTarget`, so `invalid target` → `drop target` clears an
  `Attack()` on it 0.2 s later and drops the bot to the non-combat engine: shooting one needs
  `drop target` blocked while it is the bot's duty target (Anub'arak's spheres).
- **No mage counterspells what it is attacking.** Mages have no current-target counterspell node,
  and every `... on enemy healer` value (`EnemyHealerTargetValue`) skips the bot's own current target.
- **No mage Polymorphs the `rti cc` mark in a raid.** `PolymorphTrigger` is a `HasCcTargetTrigger`,
  which reads `rti cc target` only outside raids, and `CastPolymorphAction` aims at `cc target`, whose
  scan lets a later attacker overwrite the marked one.
- **Out of line of sight is an invalid target.** `AttackersValue::IsValidTarget` is `IsPossibleTarget`
  plus `IsWithinLOSInMap`, so a slot behind a wall fires `invalid target` → `drop target`, and
  `DropTargetAction` calls `ChangeEngine(BOT_STATE_NON_COMBAT)`: the bot stops fighting and `follow`
  walks it to its master against the formation. Mimiron's alcove walls did it to two camp slots for
  20-35 s a pull. Test a fixed slot's sight of its focus, not only its floor.
- **A targetless gap drops the bot to the non-combat engine too**
  ([action-selection.md](action-selection.md#multiplier-semantics)). A submerge leaves nothing to
  attack, and `follow` then walks bots off any encounter spread: zero it for the whole engagement,
  not only while a target is up (the Jormungars' 1.5 s worm gap).
- **`DisperseDistanceValue` defaults to `-1.0f`**, and `CombatFormationMoveAction::Execute` bails on
  `dis <= 0` — the generic de-clumper is inert unless a strategy sets it.
- **`AttackersValue::Calculate` is threat-based, not range-based.** It builds from `AddAttackersOf`,
  so an untouchable boss 138 yd away stays in every bot's attacker list and `dps assist` takes him at
  `rel=50.0`. **Distance never removes a target** — only threat does.
- **`CommandPetAttack` reaches at most one guardian.** It drives `bot->GetGuardianPet()`, which reads
  the single `SUMMON_SLOT_PET` slot, so a shaman's two Feral Spirits are invisible to it — the exact
  pets most likely to run off — and it dereferences `GetCharmInfo()` unchecked. Redirecting every
  guardian means walking `m_Controlled`.

## Detection that never fires

The recurring fatal shape is **a mechanic wired to a cast the bots cannot observe**:

- **Heigan**: the dance once advanced on the boss casting Eruption (29371). He never casts it — the
  instance script erupts floor GameObjects (`HeiganEruptSections`), so every bot parked on one
  waypoint for the whole 90 s phase.
- **Noth**: the threat-reset protection keyed on catching `UNIT_STATE_CASTING` with Blink. The core
  casts it as `CastSpell(me, SPELL_BLINK, true)` — **a triggered instant never sets
  `UNIT_STATE_CASTING`** and is gone before the next bot tick.

Find the adjacent *non-triggered* cast (Noth's Cripple 29212) or the resulting aura, or build a timer
model anchored on a phase edge.

Related traps:

- **Positional phase checks false-positive at the pull.** Heigan's DB spawn is on the platform, 0.8 yd
  from the platform check point, so bots believed they were in the fast dance from the pull. Prefer a
  state read — `boss->ToCreature()->GetReactState() == REACT_PASSIVE` is exact and has no lag. Unit
  flags work the same way (`UNIT_FLAG_NOT_SELECTABLE` for Noth's balcony, `UNIT_FLAG_DISABLE_MOVE` for
  Gothik's).
- **`GetData` is a completion flag as often as a live signal.** Vezax's `GetData(1)` returns
  `lootMode == 3`, set only *after* the Animus dies. Mimiron's `GetData(1)` is authoritative but sits
  on a unit that never becomes a bot target. Thorim's `SPELL_SIF_CHANNEL_HOLOGRAM` (64324) is defined
  but **never cast** in this core. Prefer a boss's empower aura where one exists — it also tells you
  *which* thing is active, not just how many.
- **A call site is not proof of its effect.** `InitFight` calls `me->SetInCombatWithZone()` on Sara,
  which reads as the pull, but `CombatManager::CanBeginCombat` refuses a combat reference while either
  side is friendly and neither hostile — she is `FACTION_FRIENDLY` for all of phase 1, so the call puts
  her summons in combat and never touches her. A phase gate on `sara->IsInCombat()` opened **24 s late
  on every pull**. Finding the call says it runs, never that it reached the unit you are reading:
  follow it down to the guard that can turn it into a no-op.
- **An area aura never lands on its source.** An enemy area aura (effect 129,
  `SPELL_EFFECT_APPLY_AREA_AURA_ENEMY`) skips its owner (`UnitAura::FillTargetMap`), a persistent one
  its caster (`DynObjAura::FillTargetMap`; `Unit::_IsValidAttackTarget` refuses self), so `HasAura`
  there never fires: read it on a target. No Frost Sphere ever carries its own Permafrost.
- **A paladin aura can be on with no effects.** This core keeps a second paladin's copy of the same
  aura applied but strips its effects while a stronger or equal one is up (a local patch in
  `Aura::UpdateTargetMap`, `IsPaladinAuraDominant`), and `PlayerbotAI::HasAura(name, …)` walks
  effects, so it misses that copy. Read it by the application, `unit->HasAura(spellId, casterGuid)`:
  Yogg-Saron's roar paladin recast Concentration Aura five times and never pressed Aura Mastery.
- **A debuff can sit below the cast.** It may be an `EffectTriggerSpell` or a `spell_linked_spell`
  row, so keying on the cast id's aura leaves the node dead: ToC's Burning Bite/Spray carry no aura,
  the debuff is Burning Bile 66869.
- **`Unit::IsImmunedToDamage(mask)` wants one aura covering the whole mask.** Ice Block is a
  physical aura plus a magic one, so a mask test reads it as not immune: test each school.
- **Scheduled events outlive their cause.** Freya's empower events are scheduled once at pull and
  repeat unconditionally, even after the Elder dies — so a hard-mode reaction must key off the hazard
  world object, never off an Elder still being alive.
- **Spell ids differ by difficulty and the mapping is not uniform.** Heigan's 25-man Decrepit Fever is
  **55011**, not 29998, so a raw `HasAura(29998)` dispelled nothing in 25-man; Eruption, Spell
  Disruption and Plague Cloud have no difficulty rows at all; Freya's Detonate is 62598 / 62937 and
  Ground Tremor 62437 / 62859, so a clean lookup on one twin proves nothing about the other. Check each
  spell in both sources below; match every id (`NaxxSpellIds::HasAnyAura(unit, {…})`) or remap one
  through `sSpellMgr->GetSpellIdForDifficulty`. It binds boss casts, not just player auras:
  `FindCurrentSpellBySpellId` or `m_spellInfo->Id ==` on the 10-man id silently never matches —
  Mimiron's Plasma Blast defensive was dead for two pulls that way, tank dying to it twice in each.
  - **Two sources sharing no row.** The core loads the client `SpellDifficulty.dbc` (mirrored in
    mod-spell-tweaks' `spelldifficulty.reference.csv`) and overlays the world DB's
    `spelldifficulty_dbc` on it by row id, so either alone reads a remap as "none": Plasma Blast
    62997 → 64529 and Valithria's Emerald Vigor 70873 → 71941 are DB-only, every ToC combat remap
    DBC-only.
  - **The remap reads the map; the player's own difficulty does not.** `Player::GetRaidDifficulty` is
    the group's setting, and a raid with no heroic mode (Ulduar, Naxxramas, Eye of Eternity)
    downscales on entry, so a group on 25 Heroic runs a 25 Normal map while every member keeps
    reporting 25 Heroic (`MapInstanced::CreateInstance` via `GetDownscaledMapDifficultyData`; the core
    keeps the map's value apart in `m_raidMapDifficulty`). A test against
    `RAID_DIFFICULTY_25MAN_NORMAL` then reads as 10-man for the whole pull, one against
    `RAID_DIFFICULTY_10MAN_NORMAL` as 25-man under a 10 Heroic group, and it binds raid-size counts
    and role splits as much as spell ids. Ask the map: `IsRaid25Man` / `GetRaidMapDifficulty` in
    `src/Util/RaidDifficulty.h`. pblint's `raid-difficulty` errors on the player read under
    `src/Ai/Raid` and `src/Ai/Dungeon`, exempt with `// pblint: raid-difficulty-ok`. Two 25-man
    XT-002 pulls wiped on it, the carrier trigger never once true across 13,437 and 22,424 asks.
  - **Any member id resolves its row**, not only the 10-man base: Touch of Light 67297 is a 10H id,
    which `GetSpellIdForDifficulty` still remaps and a base-keyed lookup misses.
  - **`pblint.py --spell-difficulty --warnings`** reads both sources (exit 2 if one is missing). Per
    raid directory it warns once per row, at the first referenced member, when some id of the row
    appears nowhere and no referenced member is handled: in the first argument of a
    `GetSpellIdForDifficulty` call, declared and never used, named as a non-spell (`npc`, `entry`, …;
    a brace list's name counts), or marked `// pblint: spell-difficulty-ok` on its line or the one
    above.
- **`TricksOfTheTradeTargetValue::TankNeedsRedirect` never takes its opener branch, and the fallback
  is wrong for the class that casts it.** The opener branch keys on `"combat start time"`, which
  `PlayerbotAI::ChangeEngineOnCombat` sets **only** under the `wait for attack` strategy — so it is 0
  in every raid. The tank then gets Tricks only while it is within 20 yd and holds no threat yet or
  the rogue's threat exceeds half of it; otherwise the value takes the highest-attack-power melee DPS
  within 20 yd. Measured on Mimiron: the buff landed on the hardest-hitting melee at 13.5-15.5 s and
  that bot pulled the boss 2-3 s later, in all three pulls. **Still open** — encounters work around
  it by suppressing the smart-target node.
- **No bot has a threat brake.** `ThreatStrategy` stops damage at 80% of the top tank's threat, but
  `AiFactory` only ever removes `threat` and no config adds it, so dps hit at full rate however
  close they get. An encounter that needs one writes its own multiplier (Vezax's Animus) and reads
  `GetThreatMgr().GetThreat()` directly: `ThreatValue` is a `uint8` percentage that overflows past
  255%, so the bot furthest past the tank can read low.

## Before the pull, and out of combat

- **`AttackStop()` drops the victim and nothing else.** The `"current target"` AI value survives it —
  and that value is what the class rotation casts at and what `ReachTargetAction` walks to. Clear it
  explicitly: `context->GetValue<Unit*>("current target")->Set(nullptr)` (precedents
  `IccLichKingWinterAction::ClearInvalidTarget`, `FelmystMeleeClearTargetAction`). Blocking
  re-acquisition in a multiplier cannot undo a target the bot has already picked up.
- **Eating and drinking blind the bot.** `DrinkAction`/`EatAction::Execute` set `SetNextCheckDelay`
  to 12-18 s scaled by what is missing, so nothing runs, dodges included, until it expires. Wherever a
  hazard outlives combat, such as a phase handover, veto `drink`/`food` near it (Mimiron's
  `mimiron drink guard` rule).
- **A same-map `Player::TeleportTo` calls `CombatStop()`** unless given `TELE_TO_NOT_LEAVE_COMBAT`, so a
  floor-fall rescue built on it drops the bot out of the fight. Use `NearTeleportTo`, which passes it
  (and returns `void`, so return `true` after).
- **A direct `Attack()` has no out-of-combat guard** and reaches `SightDistance` (100 yd), so every
  branch of a resolver feeding it needs an in-combat unit, or the raid pulls itself
  ([kologarn.md](../raids/ulduar/kologarn.md)).
- **An encounter gate that requires `boss->IsInCombat()` leaves the strategy inert through the whole
  approach and the instant of the pull.** Generic tank and DPS behaviour therefore picks targets
  first, and the boss-specific rules inherit whatever state that left behind.
- **A rider's own combat state is not the encounter's.** Where threat sits on a vehicle creature, a
  passenger that drives away from the fight drops combat while the pull is still on. Gate on that and
  the encounter's own mover and the multiplier shutting the generic movers out die together, handing
  the wheel to `follow` — on Flame Leviathan every accepted `follow` move on a crewed bot sat 105+ yd
  out, which cost the corner drive, the pyrite band, and a ~30 s oscillation. Read the boss's combat
  first and fall back to the rider's, so the approach above still counts.
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
- **`rerere` must not be trusted** on a merge that spans a refactor — it will replay a resolution
  written against code that has since moved.
- **Fork-only `ai_playerbot_texts` rows take ids from 30001 up** (`id` is a signed smallint). Upstream
  numbers its texts straight up from its last id, so a fork text inside that range fails the next
  upstream insert on a duplicate key, and a failed playerbots update stops the worldserver. Texts
  load by name, so moving an id is safe.
- **A core merge's SQL half does not apply itself.** The worldserver image sets
  `AC_UPDATES_ENABLE_DATABASES=0` by design (`apps/docker/Dockerfile`), so new
  `data/sql/updates/db_world` files run only when `ac-db-import` is rebuilt from the merged checkout
  and run, then the worldserver restarted; `acore_world.updates` must then hold the newest file. Until
  then a script moved to a new `ScriptName` has no binding and the creature silently keeps its old AI.
- **PR #2854 renamed every config key from `AiPlayerbot.X` to `Playerbots.X` with no fallback.** An
  old `playerbots.conf` key or `AC_AI_PLAYERBOT_*` env var is ignored without an error and the option
  falls to its default; overrides are now `AC_PLAYERBOTS_*`. The part after the prefix kept its
  spelling (`Playerbots.botActiveAloneSmartScale`), so a key string must not be recased with its
  member. Members went UpperCamelCase too, and fork-only code breaks with no textual conflict.
- **PR #2836 renamed class strategies** (warrior and paladin `tank` -> `prot`, paladin `dps` -> `ret`
  and `heal` -> `holy`, priest `heal` -> `disc` and `dps` -> `shadow`). `+`/`-`/`~` still accept the
  old names, but `HasStrategy` compares `getName()`, so a check on an old name is silently false.
- **Both sides appending a parameter to one signature compiles wrong, not broken.** Upstream gave
  `StatsCollector` a level, the fork a spell-school mask; a caller still passing the mask third
  reads it as a level. Check every caller, not just the conflicted ones.
- **`IsSummonedBy` cannot set a summon's despawn type.** `Map::SummonCreature` runs `InitSummon` →
  `IsSummonedBy` before it returns, then `WorldObject::SummonCreature` and
  `Spell::EffectSummonType`'s `default:` branch re-apply the duration's type. Upstream `f4763cc9e` set
  `TEMPSUMMON_MANUAL_DESPAWN` there for Flame Leviathan's ward adds, and `4d4ae4f95` defers it a tick
  through `m_Events` (`UpdateAI` is skipped while evading). A tree with the first and not the second
  loses those adds at 3 s and 10 s with the DB fully applied.

### Local divergences a merge must re-apply

None of these fail to compile when reverted, so a merge that takes upstream's side is silent.

- **`GenericRogueStrategy` is re-parented to `MeleeCombatStrategy`** (upstream parents it to
  `CombatStrategy`). `MeleeCombatStrategy` is the **only** source of `"enemy out of melee"` →
  `reach melee`, and `AiFactory.cpp` never gives rogues the `"close"` strategy, so without the
  re-parent **every rogue stands still whenever Sprint is on cooldown**, in every raid. Upstream bug,
  never reported. See [../classes/rogue.md](../classes/rogue.md).
- **`tricks of the trade` is registered at `ACTION_HIGH + 6.5f` (26.5)**, not upstream's 26.0, to break
  an exact tie with `use instant poison on main hand` — equal relevance resolves in unspecified order.
  The ladder it sits in: tricks 26.5 / MH poison 26.0 / OH poison 25.5 / slice and dice 25.0 /
  rupture 24.0.
- **`fear ward on main tank` is registered at `ACTION_HIGH + 0.7f` (20.7)**, not upstream's 23.0, which
  puts it below the whole Discipline ladder deliberately.
- **CI builds with `-Werror`** (the module's `.github/workflows` CXXFLAGS), and Custom-only code
  carries warnings — mostly `-Wunused-parameter` on `Execute(Event event)` overrides and on free
  helpers taking an unused `PlayerbotAI* botAI`. CI jobs go red on the first upstream PR until they
  are cleared.
- **`GatherStrategyTargetExclusions` keeps the fork's `candidates` argument**, and every caller
  passes the list it will filter. Upstream calls it without one; an empty list makes Ulduar's
  Yogg-Saron sweep return early and its Guardian exclusion drops.
- **`GetFirstAliveUnitByEntry` is kept** in `EncounterHelpers` for the Naxx callers; upstream deleted
  it. Ulduar calls its own copy, `GetUldUnitByEntry`.
- `AttackersValue::IsPossibleTarget` gained an `IsInCombatWith(bot)` gate upstream, so
  `GetFirstAliveUnitByEntry` and `GetUldUnitByEntry` resolve mobs slightly earlier than they used to —
  relevant anywhere a `nullptr` return was being read as "not engaged yet".

## State does not cross objects

Every bot has its own `AiObjectContext`, and within it the trigger, the action and the multiplier each
hold **their own helper instance**. Anything they must agree on has to be shared explicitly — a
file-static `std::unordered_map<ObjectGuid, State>` defined in exactly one `.cpp`, declared in the
header.

For a per-instance clock (shared across bots), use the `PhaseStateFor` pattern: static mutex plus
`unordered_map<instanceId, State>` with a staleness re-anchor. A timestamp each bot back-dates off a
cast bar jitters by a few ms between bots, so compare it signed: an earlier one wraps `getMSTimeDiff`
to ~4.29e9 and reads as a new event (Yogg-Saron's Roar double press).

**A handout standing in for looting has to consume its source.** `StoreNewItem` off a corpse that
nothing marks spent hands out a fresh item every time the last is used: Mimiron's core carrier minted
32 Magnetic Cores off two corpses in 24 s. Take the item off the corpse's own `loot` (`is_looted`,
`unlootedCount`, `NotifyItemRemoved`) and claim the corpse per instance for when loot was never filled
(`TakeMimironCore`).

`GenericBossHelper<BossAiType>` is **unusable** when the boss AI class is file-local to its `.cpp`
(Anub'rekhan, Gothik, Heigan) or when the script is `TaskScheduler`-driven with no `events` member.
Fall back to the plain-`AiObject` + timer-model pattern (`GluthBossHelper`, `HeiganBossHelper`).

**An AI value set by one encounter is still set in the next one.** `disperse distance` is written by
Mimiron (`MimironPhase1PositioningAction`), the only encounter that writes it, and was once never
cleared — so three Mimiron pulls before a Thorim pull left `combat formation move` armed on every
bot: ranged travel **277 yd/min against 27** in a clean pull, and arena add damage up from 3,077/s to
5,105-6,310/s. That is why `MimironResetEncounterStateTrigger/Action` exists. **The proof technique
generalises**: the first `combat formation move` fired 39-686 ms into the pull, and a value armed
before the pull began cannot have been set by that pull. Any encounter that writes a shared value
owes the next one a reset.

`rti` and `rti cc` outlive the session too: `AiObjectContext::Save` persists them to the bot's DB
store. Group target icons don't: `Group::SetTargetIcon` keeps them in memory only, so a restart
clears them and a migration resets `rti`, never icons. It also strips every other icon from that
target, so a CC icon placed on a skulled unit erases the skull. An encounter that writes one restores
it, through an ungated node where the encounter gate can close before cleanup.

**A shared staleness flag cleared by the first actor starves every other actor.** Thorim's
`ThorimEncounterStateIsStale` returned one instance-wide `engagedSeen` and the reset set it false, so
**the first bot to reset closed the gate for the other 24**, which carried their latched slots, ring
state and waypoint steps into the next pull, accumulating over attempts. Split it: a per-bot
`resetDone` guid set for the work, a raid-wide flag for the detection. How a traced container shows
carried-in state is in [../systems/observability.md](../systems/observability.md#adding-a-probe).

## A targeting mark never clears inside a pull

`prioritized targets` is written by `AttackMyTargetAction` (`src/Ai/Base/Actions/AttackAction.cpp`)
and `AttackRtiTargetAction` (`ChooseTargetActions.cpp`). It is cleared by `PlayerbotAI::Reset` (far
teleport, the `reset` command, chat shortcuts, group changes), `follow`, a summon that revives, and
death — **never by leaving combat**, which `DropTargetAction` handles without touching it. A living
bot never lets go of the mark inside one pull, and a wrong mark is unrecoverable:
`FindTargetStrategy::IsHighPriority` (`TargetValue.cpp`) returns true for the skull icon *and* for
anything in `prioritized targets`, which trips `foundHighPriority` and short-circuits every smart
strategy. `DpsTargetValue::Calculate` compounds it by calling `RtiTargetValue::Calculate()` first and
returning it unconditionally on a hit — alive, LOS and sight distance are the only checks, and `rti`
defaults to `"skull"`.

This is a base-engine trap. **Any encounter with an untargetable phase hits it**: the bot is held on
a dead or wrong target for the rest of the fight. An encounter that retargets mid-pull must clear the
mark itself — Thorim sets both the group icon and its own `prioritized targets` back to empty once per
pull (`ThorimClearStaleMarks`).

**A skull is not proof of a player, and alone it does nothing.** `mark rti` (`MarkRtiStrategy`, off by
default) makes a bot put its own `rti` icon on the lowest-health unmarked attacker. `attack rti
target` is wired into no generic strategy, so a player's skull moves a bot only where an encounter
reads it.

**It outlives the pull as well.** `AttackersValue::Calculate` adds `prioritized targets` and the
skull with only a line-of-sight check, and protection paladins, bears and protection warriors run
`tank assist` out of combat too, which fires on any `attacker count`. A tank still carrying the
order walks onto the idle boss and pulls it: a paladin back up by Soulstone after a `wipe` pulled
Vezax although summoned back twice. A Soulstone skips the reviving summon, which is why the just-died
block in `PlayerbotAI::DoNextAction` clears it.

## Role and index traps

- `IsAssist{Tank,Heal,RangedDps}OfIndex(..., ignoreDeadPlayers)` **defaults to `false`**, which
  silently deletes a role when its holder dies. Passing `true` shifts indices past dead members and
  gives free auto-promotion.
- `botAI->GetMeleeIndex()` counts tanks too.
- **`IsMechanicTrackerBot` elects the first alive bot of any role**, so a marker gated behind a role
  predicate goes silent whenever a healer is first in the group.
- **`IsRanged()` includes healers.** With the default `bySpec = false` it is just
  `ContainsStrategy(STRATEGY_TYPE_RANGED)`, so a holy priest answers it. Any fixed ranged position
  gated on `IsRanged` therefore drags healers off heal range; use `IsRangedDps` when the intent is
  "the ones that only need to be in cast range of the boss".
- `FindTankTargetSmartStrategy::IsBetter` (`TankTargetValue.cpp`) short-circuits: an explicit main
  tank in a group with more than one tank **sticks to `current target`** and rejects every
  alternative. A main tank that picked up an add during a phase where the boss was untargetable never
  goes back to the boss on its own.
- `PartyMemberToProtect` keys on `attacker->GetVictim()`, so anyone dying to raid-wide AoE, a DoT or a
  ground effect is nobody's current victim and **can never be selected**. Pain Suppression, Blessing
  of Protection and Intervene cannot answer that damage pattern.
- `PartyMemberToDispel::Calculate` returns **one** target per tick, the first member holding any
  aura of the type, DoTs included, so N decursers all chase the same member and nothing prefers CC
  unless duty is split by index. `HasAuraToDispel` also skips auras with less than
  `DispelAuraDuration` (default 700 ms) remaining.
- **A dispel takes a random aura.** `Spell::EffectDispel` draws weighted by stacks, so a dispel aimed
  at CC may take a DoT. Class dispel nodes ignore dispel backlash (Unstable Affliction's silence and
  damage).
- **A school lockout locks that school's dispels.** `Player::ProhibitSpellSchool` (Counterspell,
  Spell Lock, Earth Shock) covers them, so a dispel or purge election must test the chosen rank's
  cooldown, or a kicked bot holds the duty through the lockout.
- **Hex-style auras set `UNIT_FLAG_SILENCED` without `UNIT_STATE_LOST_CONTROL`**: a "can act"
  readiness test needs both.
- Class dispel nodes sit **below** `ACTION_RAID`: mage `remove curse on party` at 40, druid at 57
  against `ACTION_RAID` 60. Any raid positioning node outranks them.
- **A raid positioning node outranks `reach melee` too** (`ACTION_HIGH + 1`, ~21), and `Engine` stops
  at the first action that returns true, so a reactive rule keyed on "too close to the boss" must
  exclude melee: their correct position breaks it every tick, the node walks them out, `reach melee`
  walks them back, and the two trade the bot all fight with nothing logged as an error. On Hodir that
  cost **45% of the raid's damage** in one pull — melee median gap to him 6 → 14.4 yd. Put the gate on
  the trigger beside the role predicate the node's anchor already uses, so the two cannot drift
  apart, and split accepted moves by role in the reader; nothing else names a gate that stopped
  gating.
- **`IsTank` flickers, and a role test that reads false once has lasting consequences.** It is
  `ContainsStrategy(STRATEGY_TYPE_TANK)`, so a bot the roster records as a tank can answer false — on
  one Thorim pull that dropped him past `IsMainTank` and `IsAssistTankOfIndex` into the melee ring,
  where he took a DPS slot, lost his role-keyed hazard exemption, and got **156 ring points and zero
  anchor points while holding the boss**. A ring point is 8 yd off the boss, so a tank standing on one
  never pulls him anywhere. Belt and braces is
  `PlayerbotAI::IsTank(bot) || PlayerbotAI::IsTank(bot, /*bySpec*/ true)` in **both** the role
  resolver and whatever assigns slots.
- **Boarding changes what a rider is, in two ways that break tests written for a bot on foot.**
  `Vehicle::AddPassenger` roots every passenger, driver included — `SetControlled(true,
  UNIT_STATE_ROOT)`, cleared only on exit — so `UNIT_STATE_NOT_MOVE` on a rider means "seated", not
  "disabled", and a role gated on it elects nobody: that switched off Flame Leviathan's tar lead, vent
  interrupt and corner posting at once, silently, for six pulls. Test `UNIT_STATE_STUNNED` on the
  rider and `UNIT_STATE_NOT_MOVE` on the hull. A seat carrying
  `VEHICLE_SEAT_FLAG_PASSENGER_NOT_SELECTABLE` also sets `UNIT_FLAG_NOT_SELECTABLE` (same function),
  which every AoE searcher skips (`AnyAttackableUnitExceptForOriginalCasterInObjectRangeCheck` in
  `GridNotifiers.h`): a crewed bot is invisible to boss AoE and a dismounted one absorbs all of it, so
  on a vehicle fight hull loss is the death, not the damage that follows it.

## Arc and geometry defaults

`WorldObject::isInFront` / `isInBack` default to `arc = M_PI` (`Object.h`) — front half plus back half
is the entire circle, so a check like `boss->isInFront(bot) || boss->isInBack(bot)` is **always
true**. On Sapphiron this made a priority-61 repositioning action permanently active and starved the
melee rotation for the whole ground phase.

`IsBotInFrontalCone` forwards to `HasInArc`, which takes the **full** arc, not the half-angle — an
`M_PI / 2` argument leaves everyone between 45° and 60° off-centre inside a cone they believe they
cleared.

**A cone's width comes from `spell_cone` only for the ids that table names.** A remapped difficulty
id without a row falls back to its implicit target type (`TARGET_UNIT_CONE_ENEMY_24`), so call
`GetSpellCone` on the remapped id. A targeted cone's bisector runs through its target, so a
neighbour is hit only within half the width of the target's bearing at any distance: space bearings
by half-width plus stand error, not full width or radius.

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

**And a cone's facing is a runtime fact, not a call-chain conclusion.** A static walk of Yogg-Saron's
Crusher Tentacle read its orientation as frozen at spawn — `SetCombatMovement(false)` routes through
`MoveIdle`, and the proc casts carry `TRIGGERED_IGNORE_SET_FACING` — so a dodge built on it would have
aimed at a fixed compass direction. The trace showed ten distinct orientations in 28 s, every Crush
victim within 0.7° of the recorded facing and every survivor inside the radius at 5.1° or more.
Orientation is `snap.u` column 4 ([../systems/observability.md](../systems/observability.md)); measure
it there.

Distance helpers and reach — what each one adds, and how to read a range literal in a boss script —
are in [raid-mechanics-lessons.md](raid-mechanics-lessons.md#range-and-reach).

## Configuration

- **Raid cheats are on by default**: `Playerbots.BotCheats = "food,taxi,raid"`. A mechanic that is
  only survivable because of the raid cheat is (a) not real play and (b) a silent wipe on any server
  that turns the cheat off. See the CHEAT-ONLY list in [../raids/README.md](../raids/README.md).
- **The raid cheat strips auras, which silently breaks any node keyed on one.** Thorim's tank-swap
  trigger fires only while the active tank still carries Unbalancing Strike; the cheat stripped it
  **2-230 ms** after it landed, so only 2-3 of every 5-8 strikes produced a swap, while the strikes
  that kept their aura the full 15 s **all** swapped within 37-199 ms. Before keying a node on a
  debuff, check the cheat is not removing it. Raid cheat is live for Yogg-Saron, ICC, SSC, BT, BWL
  and RS.
- `ErrorDelay` defaults to 100 ms and is compared as `(now - lastErrorTell) < ErrorDelay / 1000`
  (`PlayerbotMgr::CheckTellErrors`) — integer division makes that 0 seconds, so errors flush every
  tick. **Still unfixed.**
- **Boss health is not `creature_template`.** `mod-dungeon-scale` is loaded and
  `AC_DUNGEON_SCALE_STAT_MODIFIER_RAID_BOSS_HEALTH=1.2` scales every raid boss, with the result moving
  with raid size: Sara measured **240,000 at 25 raiders and 237,500 at 24** against the 199,999 her
  `HealthModifier` gives, and the Guardians scaled with her — the difference between an eight- and a
  ten-kill phase. Anything that turns a boss's health into a count — adds to kill, casts to survive, a
  burn window — takes it from the trace's `unit` row (`mhp`), never from the DB, and never from a
  figure another doc quotes without saying which it came from.
- **It rescales mid-fight.** Any map-data change — the player count, or the rounded average creature
  level as summons spawn — re-runs mod-dungeon-scale's `ModifyCreatureAttributes` on every creature in
  the map: it sets the max to the aura-free base, scales current health against the old aura-inflated
  max, then `UpdateAllStats` puts the auras back on the max alone. Upstream, a creature holding
  `MOD_INCREASE_HEALTH_PERCENT` loses that whole multiplier in one frame — Flame Leviathan 92.6% ->
  24.0% with four +40% towers up, in 4 of 10 traces. The local module clone restores the percentage;
  the signature in a trace is a one-frame drop with no damage row whose ratio equals the creature's
  aura multiplier.

## A missing table or column kills the worldserver, not just the query

`MySQLConnection` aborts the whole process on a query naming a table or column that doesn't exist —
not an empty result, not a catchable error. Any code reading a table it doesn't own itself (another
module's, or one an import pipeline creates later) must check `information_schema.COLUMNS` for every
name first and skip the query whenever one is missing. `BisDatasetMgr` (`src/Mgr/Item/BisDatasetMgr.cpp`)
does this before every load and version poll of `bistooltip_dataset`/`_subject`/`_block` — tables
mod-bis-tooltip owns and playerbots only reads, absent on any server that hasn't loaded that module or
run its import yet.
