# Pitfalls

Failure modes this codebase has already hit, grouped by shape. Most produce **no error** — the bot
just quietly does nothing. Check this list before debugging "the trigger doesn't fire".

Vehicle riders, oscillating movement, reach maths, coordination and per-raid cost have their own
doc: [raid-mechanics-lessons.md](raid-mechanics-lessons.md).

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
  **`settledZ`** column, never the trailing "N/N on mesh" line — a point can report a nearest poly
  within 2 yd and still settle to terrain, which is off the floor.

- **`NAV_MAGMA` is in the player path filter** (`PathGenerator::CreateFilter`,
  `PathGenerator.cpp:764-767`), so a destination the navmesh flags as magma is **reachable, not
  rejected**. Do not discard a hand-measured point for sitting on lava — Obsidian Sanctum's pull-drag
  corner is exactly that.

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
- **`IsDuplicateMove` is not the anti-oscillation guard it looks like.** It needs the request within
  **0.01 yd** of `lastMoveShort` (`MovementActions.cpp:939-949`), so any caller passing
  `bot->GetPositionZ()` re-issues a different point as soon as the bot moves on a sloped floor, and
  the pathfinding branch stores the navmesh-resolved Z rather than the requested one. What actually
  throttles a re-issuing action is the arrival tolerance plus the movement lock above.

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
  `spelldifficulty_dbc` per spell and use `NaxxSpellIds::HasAnyAura(unit, {…})`.

## Before the pull, and out of combat

- **`AttackStop()` drops the victim and nothing else.** The `"current target"` AI value survives it —
  and that value is what the class rotation casts at and what `ReachTargetAction` walks to. Clear it
  explicitly: `context->GetValue<Unit*>("current target")->Set(nullptr)` (precedent
  `ICCActions_LK.cpp:690`, `SWPActions_Felmyst.cpp:386`). Blocking re-acquisition in a multiplier
  cannot undo a target the bot has already picked up.
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

`GenericBossHelper<BossAiType>` is **unusable** when the boss AI class is file-local to its `.cpp`
(Anub'rekhan, Gothik, Heigan) or when the script is `TaskScheduler`-driven with no `events` member.
Fall back to the plain-`AiObject` + timer-model pattern (`GluthBossHelper`, `HeiganBossHelper`).

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

## Arc and geometry defaults

`WorldObject::isInFront` / `isInBack` default to `arc = M_PI` (`Object.h:558-559`) — front half plus
back half is the entire circle, so a check like `boss->isInFront(bot) || boss->isInBack(bot)` is
**always true**. On Sapphiron this made a priority-61 repositioning action permanently active and
starved the melee rotation for the whole ground phase.

`IsBotInFrontalCone` forwards to `HasInArc`, which takes the **full** arc, not the half-angle — an
`M_PI / 2` argument leaves everyone between 45° and 60° off-centre inside a cone they believe they
cleared.

Core distance helpers are surface-to-surface on combat reach: `GetObjectSize()` returns
`UNIT_FIELD_COMBATREACH` (`Object.cpp:2888`), `IsWithinCombatRange` is `dist3d < d + reachSum`, and
`GetMeleeRange = reachSum + 4/3`. Hard-coded stand distances that ignore reach break on large models.

## Configuration

- **Raid cheats are on by default**: `AiPlayerbot.BotCheats = "food,taxi,raid"`. A mechanic that is
  only survivable because of the raid cheat is (a) not real play and (b) a silent wipe on any server
  that turns the cheat off. See the CHEAT-ONLY list in [../raids/README.md](../raids/README.md).
- `errorDelay` defaults to 100 ms and is compared as `(now - lastErrorTell) < errorDelay / 1000`
  (`PlayerbotMgr.cpp:1709`) — integer division makes that 0 seconds, so errors flush every tick.
  **Still unfixed.**
