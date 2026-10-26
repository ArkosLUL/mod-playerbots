# Raid strategies — shared conventions

Per-raid encounter knowledge lives in the files below; this one holds what applies across all of
them. Engine mechanics are in [../engine/action-selection.md](../engine/action-selection.md); the trap
catalogue every boss strategy keeps rediscovering is [../engine/pitfalls.md](../engine/pitfalls.md);
vehicles, movement stability, reach and per-raid cost are in
[../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md).

## Raid docs

| Doc | Raid |
|---|---|
| [eye-of-eternity.md](eye-of-eternity.md) | Eye of Eternity — Malygos |
| [naxxramas.md](naxxramas.md) | Naxxramas |
| [obsidian-sanctum.md](obsidian-sanctum.md) | Obsidian Sanctum — Sartharion |
| [onyxias-lair.md](onyxias-lair.md) | Onyxia's Lair |
| [sunwell.md](sunwell.md) | Sunwell Plateau |
| [tempest-keep.md](tempest-keep.md) | Tempest Keep — The Eye |
| [vault-of-archavon.md](vault-of-archavon.md) | Vault of Archavon |
| [ulduar/](ulduar/README.md) | Ulduar, a file per boss |
| [trial-of-the-crusader/](trial-of-the-crusader/README.md) | Trial of the Crusader, a file per boss |

## File layout and wiring

A raid is a folder under `src/Ai/Raid/<Raid>/` mirroring `src/Ai/Raid/Gruul/`, the reference
implementation: `<X>Strategy`, `<X>Triggers` + `<X>TriggerContext.h`, `<X>Actions` +
`<X>ActionContext.h`, `<X>Multipliers`, `<X>Helpers`, and `<X>ValueContext.h` for cached lookups.
Large raids split per boss under `Action/` and `Trigger/` and keep a thin umbrella header
(`UldActions.h`) that includes the parts, so contexts and registration maps need no edits. There is
no `CMakeLists.txt`: the core globs `src/**` and puts every source subdirectory on the include path,
so a new directory needs no build file and includes stay flat (`#include "EoEData.h"`).

Four registration sites: `RaidStrategyContext.h` (key → strategy), `BuildSharedTriggerContexts.cpp`,
`BuildSharedActionContexts.cpp`, and `PlayerbotAI::ApplyInstanceStrategies` — both the
`GetInstanceStrategies()` list *and* the `case <mapId>:` arm. A key the arm applies but the list
lacks is never removed on a zone change (`rs` once was). Its nodes run out of combat too
([registration](../engine/action-selection.md#registration)). A value context adds a fifth,
`BuildSharedValueContexts.cpp`; pblint does not check value names.

Naming: strategy keys are bare lowercase (`"blacktemple"`); triggers and actions are lowercase,
space-separated and boss-prefixed; multipliers are `{BossName}{Purpose}Multiplier`.

**Encounter definitions** (`Raid/RaidEncounter.{h,cpp}`) replace that wiring for Ulduar, EoE, OS,
Onyxia and Naxx. One file per boss, `<Raid>/Definition/<Raid>Definition_<Boss>.cpp`, declares:

- **rows**: trigger class, action class, priority, `EncounterRow::Mover`. Each name is typed once, as
  the class's `Name` constant; a class built under several names (`BossNatureAspectHoldMultiplier`)
  takes them from the row. Back-to-back rows on one trigger share its node, and a row on an action a
  shared context registers (`Node<T>("rear flank", …)`) registers only its trigger;
- its [encounter rules](#encounter-rules) and hand-written multipliers;
- a **tick**, run by `Strategy::OnTick` once per engine tick, inside the `RaidObs::BotContext` and
  before the triggers, while the gate is open. Housekeeping (trace notes, latch writes, wipe resets)
  goes there, never into a predicate a cheaper check may skip, and tests cheap instance state before
  any sweep: between pulls every encounter not yet killed is open. Not `neglect threat`:
  `ThreatMultiplier` clears it on every read, once per action, so a multiplier seeing every action
  (`AnyAction`) sets it;
- its encounter id and the trace name `RaidObs::NamePull` files the pull under.

The raid lists its definitions once (`<Raid>Definitions.h`); its contexts and strategy build from
the list. All of it sits behind the boss's **encounter gate**: `BossStateGate` opens while the boss
is `IN_PROGRESS`, closes once `DONE`, otherwise closes only while another encounter is
`IN_PROGRESS`, and is open outside an instance — so between pulls everything is open and predicates
still check the boss. "Live" means `IN_PROGRESS`. The gated trigger (`EncounterGatedTrigger`) copies
the inner trigger's name and check interval, hands out `EncounterTriggerPassId`, and calls
`RaidObs::NamePull` when it fires while live.

Gate on the fight, not the unit: a drake Sartharion calls in never starts its own encounter, so OS
mechanics sit in his definition, while a drake pulled alone starts one and closes his gate. A node
that also serves another encounter stays in the strategy, outside every gate, after the
definition's nodes (OS's realm kill order serves a solo drake too); check equal-priority order.
Multipliers not tied to one encounter (`UldThreatRedirectMultiplier`, `UlduarBurstWindowMultiplier`,
Naxx's redirect and burst multipliers) stay raid-wide and ungated.

`Engine::addStrategy` / `removeStrategy` rebuild every node and multiplier, so swapping per-boss
strategies at runtime is not an option; that is why one raid strategy registers every boss behind
gates.

**ToC** has `Definition/` files too, rules and hand-written multipliers only: its nodes stay in the
strategy, gated by name prefix through `ToCGatedTrigger`, and its gate (`ToCGate`) reads the
`GetData(TOC_DATA_INSTANCE_PROGRESS)` stage. **Open gap:** moving ToC onto rows and a stage gate
waits on the ToC rework's closeout; core #28117 since gave ToC boss states, so `BossStateGate` may
make the stage gate moot.

Nothing enforces that the registration sites agree, so run `tools/pblint/pblint.py` over what a
commit touches: it cross-checks all four, and every trigger and action name besides, definition rows
included. It only proves the name resolves — `postmortem.py --coverage` says whether the node then
did anything.

### Moving a boss onto a definition

Give its classes `Name` constants, write its definition, list it, and call its `AddTriggerNodes` /
`AddMultipliers` at its old spot in the strategy, which keeps node and veto order; then delete its
creators and the hand gating the definition replaces. Syntax-check with a raised `PB_MAX_FANOUT`: the
contexts reach every `BuildShared*` TU.

- A multiplier becomes rules only where they are exactly equivalent; otherwise it stays hand-written,
  keeps its name and declares its families. `Multiplier<M>(families, args...)` passes constructor
  arguments (the nature aspect hold). A guard whose boss helper keeps state across ticks, or that
  sets `neglect threat`, is never equivalent (Naxx: all but Grobbulus).
- A branchy multiplier splits into several rules under its one name, in its old order; check that no
  early `return 1` in the original shields an action a later rule would zero (Hodir's Flash Freeze).
- A role no `Role` bit expresses goes in the predicate. `Role::NonTank` and `Role::Dps` (`IsDps`, the
  bot's dps strategy) exist for Auriaya and Freya.
- Housekeeping found in a read moves to the tick behind the same per-instance throttle, with any
  lookup after the throttle check (Flame Leviathan, Mimiron, Algalon, Yogg-Saron, Thorim).
- An old multiplier's cache becomes a per-bot, per-tick `thread_local` cache in the predicate
  (Razorscale, `GetMalygosAggro`), or in the boss's `Util` when hand-written guards share it
  (`SartharionSnapshotFor`).
- An old name list matches `getName()`, not the registered name: `taunt on snare target` is named
  `taunt`, so every list caught it and the Taunt family missed it until it was added.
- A class named otherwise than its registration takes the registered name, unless its base reads the
  name (`HasAuraTrigger`'s aura, `MoveInsideAction`): then `Name` only registers.

Not migrated, by choice: deeper Mimiron and Thorim modules, Naxx's per-node helper copies, the threat
redirect trio, and the Four Horsemen lookup ([naxxramas.md](naxxramas.md#four-horsemen)).

## Priority conventions

| Band | Use |
|---|---|
| `ACTION_RAID` (60) | role assignments, target selection, positioning |
| `ACTION_RAID + N` | stagger within a boss — flat priorities leave ordering to vector insertion order |
| `ACTION_EMERGENCY + 6` (96) with `MOVEMENT_FORCED` | emergency hazard dodge (ZA precedent) |

Class dispel nodes sit **below** `ACTION_RAID` (mage 40, druid 57), so any raid positioning node
outranks them. A boss whose mechanic is a dispel race needs its own dispel action above 60 — Heigan's
sits at `ACTION_RAID + 5`.

## Threat redirect — the rubric

Quoted verbatim because three separate encounters re-derived it:

> The generic node already redirects to the main tank at the pull. A dedicated action only earns its
> keep when **the target is not the main tank**, or **the moment that matters is not the pull**.

And its corollary, which matters more:

> A veto only earns its keep where the main tank is *reliably* wrong. Vetoing an encounter that is
> merely *imperfect* for the MT turns hunters and rogues into dead weight.

Three timing flavours cover everything seen so far, all from the Black Temple pattern: **pull** (boss
above 95% health), **phase transition** (timer- or state-based), and **multi-tank assignment** (index
the living hunters/rogues and map each to a tank).

`RaidRedirectThreatAction` (`Raid/RaidRedirectThreat.{h,cpp}`) is the **only redirect base covering
hunters *and* rogues** — every BT/SSC/TK/SWP/Hyjal/ZA action is hunter-only. A rogue casts Tricks of
the Trade and stops there (it redirects everything for 6s); a hunter casts Misdirection then dumps
`steady shot` to spend the three charges. Subclasses implement only `GetRedirectTank()` and
`GetThreatDumpTarget()`; Naxx, OS, VoA and Hodir do.

Vetoes must `dynamic_cast` to the **concrete** actions (`CastMisdirectionOnMainTankAction`,
`CastTricksOfTheTradeOnMainTankAction`), never the shared `BuffOnMainTankAction` base, which also
carries Beacon, Earth Shield, Thorns and Lifebloom
([../engine/action-selection.md](../engine/action-selection.md#multiplier-semantics)). Any new
boss-scoped redirect action needs its matching veto entry removed or narrowed, or the two fight each
other.

Both presume threat sticks: **a mob that wipes its own threat table has no redirect and no
taunt answer** ([../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)).
Look for `DoResetThreatList` / `SelectTargetFromPlayerList` in the script first — Freya's Detonating
Lashers re-roll every 10s, so ten of them face one 30s cooldown and the answer is geometry (a range
leash), not threat. The same test bounds a taunt: allowlist the adds the encounter **owns**, never
the whole target ladder — taunting a rung borrowed for damage off another tank means owning its
positioning too.

## Encounter rules

Eight encounters independently arrived at **movement suppression** (zero the generic movers while a
positioning action is active, so they cannot drag the bot back), and bosses that own target selection
at **targeting suppression**. A definition declares both as rules: each is a `Multiplier` (traces
credit a veto by its name), and its checks run action, gate, role, then the boss's predicate, a pure
`bool(PlayerbotAI*)`.

| Kind | Blocks | Passes |
|---|---|---|
| `OwnMovement` | every `MovementAction` | the boss's `Mover` rows (or a named subset) and its `keep` families, default `Attack` + `Reach` |
| `OwnTargeting` | named picker families, default `DpsAssist` + `TankAssist` | everything else |
| `Block` | named families and/or action names | everything else |
| `Exclusive` | every `MovementAction`, attacks included | only what it names |

Families come from `ClassifyAction`, the only `dynamic_cast` site, cached per `Action*`;
`RaidEncounterRules.h` lists them, std-only so `tools/nativetest/raid_encounter_rules_test.cpp` covers
the kinds without the server. `Taunt` holds every class taunt action plus `taunt on snare target`,
whatever the role; rarer actions (`killing spree`, `sprint`, a lone `death grip`) and the boss's own
go by name. Two gaps in `OwnMovement`'s defaults: `ReachHeal` is blocked, so add it to `keep` where a
healer must walk to its target (Vezax), and the spell movers (`Charge`, `Blink`, `Disengage`) are
not `MovementAction`s, so they need a `Block`. A hand-written multiplier declares the families it
looks at and sees only those, while the gate is open; `AnyAction` covers one that can zero an item, a
plain `Action` or a threat type (XT-002's, burst lists, AoE holds). A predicate keeps no state, so a
guard caching its boss across ticks (Naxx's helpers) stays hand-written.

### Movement: the only mover left

**The failure mode this creates**: once `CombatFormationMoveAction` is disabled, the boss-specific
action is the *only* thing that can position those bots. If it silently fails — an off-navmesh
`MoveTo`, a `MoveInside` that never returns false — they stand still for the whole fight. Void Reaver
is the case study.

### Targeting: the whole fight

A boss that owns target selection must zero `DpsAssist` and `TankAssist` **for the whole fight**, not
only inside the phase that motivated it.

**The failure mode**: a `choose target` action conventionally returns `false` once the bot already
holds its pick, which `DoNextAction` treats as FAILED (see
[../engine/action-selection.md](../engine/action-selection.md)) — so every settled tick keeps
draining the queue down to `dps assist` at relevance 50. `GeneralFindTargetSmartStrategy`
(`Value/DpsTargetValue.cpp`) ranks by attack range then remaining lifetime with **no current-target
preference**, so a fresh low-health add always outranks a boss. The bot takes the add, the boss
action yanks it back next tick, and it flips forever without landing a cast — in-game it reads as
bots jiggling on the spot. Noth is the case study.

**Whole fight** also rules out the subtler gate: zeroing only while the encounter's own target lookup
returns non-null. That reopens the hole exactly where the encounter node has nothing to say — on
Freya's lasher waves generic assist took over and the off-tank towed ten detonating adds into the
raid stack. Suppressing unconditionally is safe only if the ladder ends in a **terminal fallback**
(the boss), so no role is left nodeless — Freya's main tank had none and held her only because it
pulled her. Without one, an unconditional rule strands the bot with no target source, so gate the
rule on exactly `resolver != nullptr` and list the windows `dps assist` takes as known gaps (ToC,
[trial-of-the-crusader/README.md](trial-of-the-crusader/README.md#targeting-is-direct-no-raid-icons)).

**Healers are the exception: a ladder may give them nothing.** A targetless healer drops to the
non-combat engine (no mana cooldowns, potions, shields or Beacon) and only `dps assist` brings it
back. `raid healer engine` (`RaidHealerEngine.h`, its own strategy context without siblings, or the
raid's strategy would remove it, [pitfalls.md](../engine/pitfalls.md#names-fail-silently-at-runtime)),
added on every raid map by `ApplyInstanceStrategies`, keeps a bot healer on the combat engine in combat while the instance has
an encounter in progress, outside vehicles, and blocks `drop target` on a null target. Strategies that
leave healers targetless, so removing it strands them: Malygos P1, Lady Vashj P2-P3, the Lich King's
Remorseless Winters, Valithria after each portal, Sindragosa's P3 tomb hides, Yogg-Saron P3. Trash
and dungeons are not covered.

## Boss helpers

`GenericBossHelper<BossAiType>` works only when the boss AI class is visible and exposes `events`.
It is **unusable** for Anub'rekhan, Gothik and Heigan (AI class file-local to its `.cpp`) and for any
`TaskScheduler`-driven script. Fall back to the plain-`AiObject` + timer-model shape used by
`GluthBossHelper` and `HeiganBossHelper`.

For a clock every bot in the instance must agree on, keep it in a
[`RaidInstanceState`](#per-instance-state), re-anchored on phase edges with a staleness guard as
`HeiganBossHelper::PhaseStateFor` does. Trigger, action and multiplier each hold **separate** helper
instances ([../engine/pitfalls.md](../engine/pitfalls.md#state-does-not-cross-objects)).

Resolve bosses with `AI_VALUE2(Unit*, "find target", "<lowercase name>")` **only** when the bot is
reliably on that creature's threat list; otherwise use `GetFirstAliveUnitByEntry`
(`src/Util/EncounterHelpers.h`), deprecated upstream
([../engine/pitfalls.md](../engine/pitfalls.md#things-the-bot-cannot-see)). `SetInCombatWithZone()`
in a script's `JustEngagedWith` is what makes `"find target"` work from the pull on some bosses.

## Heroic and 10/25

There is no separate heroic strategy anywhere — difficulty is an instance property and one code path
serves both. What actually differs per mode:

- **Spell ids**, and the mapping is not uniform
  ([../engine/pitfalls.md](../engine/pitfalls.md#detection-that-never-fires)). On Kel'Thuzad only
  Frost Bolt has variants. Use `NaxxSpellIds::HasAnyAura(unit, {…})` rather than a raw id.
- **Add counts and target counts** — ask the map, not the player (same section).
- **Some mechanics exist only in 25-man** (Noth's Blink/Cripple).

Ulduar hard modes are the exception to all of this: they are a **raid choice, not the 10/25 flag**,
so the "heroic comes free" reasoning does not apply. See [ulduar/README.md](ulduar/README.md).

## Anti-fear (`RaidAntiFear.h`)

One shared component in `src/Ai/Raid/`, so no encounter hand-copies its own. Upstream's Hyjal and
Tempest Keep rewrites don't use it: Archimonde keeps upstream's own Tremor Totem code, and Solarian
and Kael'thas get no anti-fear.
`RaidAntiFearTrigger` / `RaidAntiFearAction` / `RaidAntiFearTotemGuardMultiplier` are abstract;
concrete subclasses supply only `FearWindowActive()`.

Behaviour: a **priest** casts Fear Ward (6346) on the group main tank, or the main healer if the tank
already has it; a **shaman** drops Tremor Totem, gated on it not already being out. Consumers are
Onyxia from her phase 3 descent on (Bellowing Roar 18431), Auriaya (Terrifying Screech 64386, fixed
35s cycle so the totem stays up all fight), and Yogg-Saron P2/P3 (Malady of the Mind 63830/63881,
Deafening Roar 64189).

**Two defects this design exists to avoid:**

1. **`ChangeStrategy("+tremor")` does not work.** `NoEarthTotemTrigger` picks the earth totem by
   first match in `{strength of earth, stoneskin, tremor, earthbind}`, and every shaman spec ships
   with one of the first two — so Tremor never wins. Both strategies also register a node on the same
   `"no earth totem"` trigger at priority 55, so the slot thrashes, and the strategy is never removed
   afterwards, leaking past the instance.
2. **The earth totem slot is exclusive.** Anything that drops Tremor must also suppress the competing
   earth-totem casts for as long as the fear window is open, or the shaman's own strategy re-drops
   Stoneskin on the next GCD. Hence the guard multiplier — the same shape as
   `IllidanStormrageUseEarthbindTotemMultiplier`.

Cast directly and suppress the competition; no `ChangeStrategy`, no persistent state, self-reverting
when the window closes.

**Hunter Aspect of the Wild follows the same shape**: per-boss `BossNatureResistanceTrigger` /
`BossNatureResistanceAction` plus `BossNatureAspectHoldMultiplier` nodes (AQ40's Viscidus and
Princess Huhuran, Kologarn, Freya, Thorim, Emalon, Archavon). The gate is `FindBossByName` + alive +
hostile, so hunters take the aspect when the boss comes into range, not at engage. **Never set
`rnature`:** it is a sibling of `bdps`, so it evicts the combat Dragonhawk node, and
`HunterAspectOfTheViperTrigger` bails out whenever it is set, so the hunter cannot use Aspect of the
Viper and runs dry in every later raid — 59.7% of a 514 s fight and 70.3% of a 363 s one under 5%
mana. Why a stray `ChangeStrategy` outlives the session, and how to migrate the stored rows, is in
[../engine/action-selection.md](../engine/action-selection.md#strategy-assignment). Not yet seen on a
live AQ40 pull.

AQ40's old resistance action also had a priest `+rshadow` branch that never fired: every priest
already gets `rshadow` in the non-combat engine (`AiFactory::AddDefaultNonCombatStrategies`) and the
branch guarded on not having it. Had it fired, it would have loaded a non-combat strategy into the
combat engine for good.

**The paladin resistance auras still have the old defect:** `BossFireResistanceAction`,
`BossFrostResistanceAction` and `BossShadowResistanceAction` add `+rfire`/`+rfrost`/`+rshadow` and never
remove them, evicting the sibling blessing strategy for good. Live at Razorscale, Ignis, Freya, Thorim,
Mimiron, Sara, Yogg-Saron, Koralon and Toravon; the fix is a per-boss hold multiplier for the paladin
aura slot.

**Explicitly not counterable — do not target:** Insane (63120/64464, AoE charm), Psychosis, Lunatic
Gaze and Induce Madness (sanity drains), and Chains of Kel'Thuzad (28410, mind control — no *fear*
counter applies, but a druid does Cyclone the charmed raider, see [naxxramas.md](naxxramas.md)).
Phase 1 WotLK content — Naxxramas, Obsidian Sanctum, Eye of Eternity, Vault of Archavon — has
**zero** fear mechanics; all 15 Naxx boss spell enums were checked, and Gluth's vanilla Terrifying
Roar (29685) does not exist in this build.

`SPELL_FEAR_WARD = 6346` is defined here, but still duplicated in `KaraHelpers.h` and SSC's
`Util/SSCHelpers.h` — deliberately left alone to keep the change contained.

## Calls to human tanks

A human tank has no trigger to fire, so where a boss times a bot tank's defensive the encounter tells a
human instead: `CallHumanTankDefensive` (`RaidHumanTankCall.h`), on Thorim phase 2, Yogg-Saron phases 1
and 3 and Algalon. Calls key on who the boss or Guardians are hitting, never on role: `IsTank` reads a
human Death Knight as a tank only in Blood. The button comes from `ReadyTankDefensiveSpell`, the same
table bots use. No call while one of his buttons runs, or within 6 s of the last.

Each call is two whispers from a group bot, to him only:

- an addon message `PBCALL` `DEF<tab>spellId<tab>hitInMs<tab>reason`, shown as a DBM special warning (plus
  a bar when `hitInMs` ≥ 1 s) by the `tools/addons/PlayerbotCalls` add-on, copied into the client's
  `Interface\AddOns`. Stock DBM can't do this: its warnings fire only from its own boss mods, and the only
  remote input, a shared timer bar, needs raid assist and reaches the whole raid;
- a `CHAT_MSG_RAID_BOSS_WHISPER` fallback (center screen, warning sound), which the add-on hides.

Traps: `Player::Whisper` ignores its boss-whisper flag, so the packet is built by hand. The client runs
the fallback through `format()`, so a bare `%` breaks it. DBM's "Hide raid boss emote frame during boss
fights" hides the fallback.

## Per-instance state

Cross-bot state an encounter latches per instance (slots, clocks, held choices) lives in a
`static RaidInstanceState<State>` in the boss `.cpp` (`src/Ai/Raid/RaidInstanceState.h`), never in a
hand-rolled mutex + `unordered_map`. `For(id)` creates the entry, `Find(id)` never does, `Reset(id)`
erases it. A has-state check, such as a reset trigger's gate, must use `Find`: `For` re-creates the
entry a reset just erased.

- **Locking:** only the lookup is locked, the reference is used unlocked. Safe because one worker
  updates an instance map at a time (`MapInstanced::Update` schedules each instance once per tick and
  `MapMgr::Update` waits for the pool) and bot code runs inside that update or on the world thread,
  never alongside it. Never keep the reference past the tick.
- **Eviction:** `RaidInstanceStateMapScript` drops the instance from every store on `OnDestroyMap`.
  It is the only lifetime cleanup: Ulduar's reset nodes are gated off once the boss is DONE, and a
  saved lockout entered again after its map unloaded comes back with the same instance id. Mid-fight
  resets (wipe, boss out of combat) stay in the boss code.
- **Testing:** `tools/nativetest/run.sh` builds `tools/nativetest/*_test.cpp` in the build image under
  ThreadSanitizer, without the core, so the header must stay std-only.

Not migrated, on purpose: Naxx (upstream still edits `NaxxBossHelper.h`) and ICC (clears on its own
`OnDestroyMap`). **Known gap:** BT, SSC, SWP, Hyjal, Mag, Kara, TK, ZA and RS (`RSActions_SAV.cpp`,
`RSActions_BAL.cpp`) share unlocked `extern` maps across instances, a data race once two raids run at
once. Move them here when touched.

## Raid cheats are on by default

Stock config is `Playerbots.BotCheats = "food,taxi,raid"`. A number of mechanics are "handled" only
because the raid cheat papers over them — which makes them (a) not real play and (b) a silent wipe on
any server that turns the cheat off. Treat every cheat-dependent mechanic as an open gap, not as
working behaviour.

## Recording new work

Durable encounter knowledge goes into the matching `<raid>.md` here, or into `<raid>/<boss>.md`
where the raid has outgrown one file — Ulduar has, and its per-boss files mirror
`src/Ai/Raid/Uld/Action/UldActions_<Boss>.cpp`. An in-flight plan lives in
[../plans/](../plans/) only while the work is unfinished; when it ships, fold its durable content
into the raid file and delete the plan.
