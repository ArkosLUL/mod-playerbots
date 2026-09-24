# Raid encounter definitions

## Context

Two problems share one fix, across Ulduar, Naxx, EoE, OS and ToC (`src/Ai/Raid/`):

- **Wiring spread.** One Ulduar mechanic touches about 8 files: trigger and action classes, both
  contexts (`UldTriggerContext.h`, `UldActionContext.h`), a node in `UldStrategy.cpp`, and every
  multiplier that exempts its action by name. Names are typed 3-5 times; `thorim arena leash action`
  appears in the class, the action context, the node and two multipliers. pblint exists to police
  that sync.
- **Ungated, re-typed vetoes.** `UldGatedTrigger` (`Uld/UldEncounterGate.{h,cpp}`) gates triggers
  only; `UldStrategy::InitMultipliers` registers 68 multipliers bare, so they run on trash, between
  pulls and after kills. Naxx, EoE, OS and ToC gate nothing centrally. 55 multipliers (Uld 39, Naxx
  13, EoE 1, OS 2) re-implement the same "encounter owns movement / targeting" veto
  ([docs/raids/README.md](../../raids/README.md) §"Movement-suppression multiplier",
  §"Targeting-suppression multiplier"), each with hand-typed exemption lists, and 22 of them resolve
  the boss before testing the action. `Multiplier::GetValue` runs on every popped action that passed
  `isUseful()` (`Engine.cpp:242-255`), so that order costs up to ~150 raycasts per candidate.

Goal: one module, `RaidEncounter`, where each boss declares its mechanics once and the module owns
gating, the generic vetoes, check order and housekeeping. This plan also owns items 1 and 2 of
[ulduar-perf-followups.PLAN.md](../ulduar-perf-followups/ulduar-perf-followups.PLAN.md).

Engine facts relied on:

- `AttackAction : MovementAction` (`Ai/Base/Actions/AttackAction.h:16`), with every generic picker
  (`DpsAssistAction`, `TankAssistAction`, …) and `MeleeAction` below it. `CastReachTargetSpellAction`,
  blink and disengage are `CastSpellAction`s. `TankFaceAction` and `SetBehindTargetAction` derive from
  `CombatFormationMoveAction`.
- A zero is final and the multiplier loop stops there; `RaidObs::NoteVeto` credits the first
  multiplier that zeroed (`Engine.cpp:242-255`). Trace readers key on those names
  (`tools/botobs/bosses/general_vezax.py:64-66`).
- Actions are created once per bot context and reused (`NamedObjectContextList::GetContextObject`).
- Shared contexts are one process-wide creator map of `std::function` (`NamedObjectContext.h:46`,
  `BuildShared{Trigger,Action}Contexts.cpp`).
- `Engine::addStrategy`/`removeStrategy` rebuild every node and multiplier (`Engine::Init`), so
  per-boss strategies swapped at runtime are not an option.
- Core boss state: Ulduar (14), Naxx (15, `naxxramas.h`), OS (4, `obsidian_sanctum.h`) and EoE (1)
  use `SetBossNumber`/`GetBossState`. ToC has no boss state, only a stage in
  `GetData(TYPE_INSTANCE_PROGRESS)` ([toc-rework.PLAN.md](../toc-rework/toc-rework.PLAN.md)).

## Design

### Encounter definition

Each boss has one definition file, `<Raid>/Definition/<Raid>Definition_<Boss>.cpp`, listed once per
raid. It declares:

- **Rows**, one per mechanic: trigger class, action class, priority, and `EncounterRow::Mover` if the
  action is one of the boss's **movers** (add a picker flag only when a rule needs one). A row
  registers the trigger creator, the action
  creator and the `TriggerNode`. Names live once, as a `Name` constant on each class used by its
  constructor; a class built under several names (`BossNatureAspectHoldMultiplier`) takes them from
  the row. One trigger may feed several rows (`flame leviathan on vehicle` has two nodes).
- **Rules** (next section).
- **Hand-written multipliers** for anything no rule kind fits, each declaring the action families
  it looks at.
- **A tick** for housekeeping (see "Tick hook").
- Its encounter id and the trace name passed to `RaidObs::NamePull`.

The raid's strategy and contexts build from its definitions. Multipliers not tied to one encounter
(`UldThreatRedirectMultiplier`, `UlduarBurstWindowMultiplier`) stay raid-wide and ungated.

The worked example is `Uld/Definition/UldDefinition_Vezax.cpp`.

### Rules

Each rule is its own `Multiplier`, named after the multiplier it replaces; a multiplier split into
several rules keeps its name on each, in its old order. Checks run cheapest first, and the module
enforces the order:

1. Does the rule care about this action? (action families and names only)
2. Is the encounter's gate open?
3. Does the bot's role match?
4. The boss's predicate, `bool(PlayerbotAI*)`. Predicates are pure reads.

Kinds:

| Kind | Blocks | Passes |
|---|---|---|
| `OwnMovement` | every movement action | the boss's mover rows (a rule may narrow to a named subset) and its `keep` families, default `Attack` + `Reach` |
| `OwnTargeting` | named picker families, default `DpsAssist` + `TankAssist` | everything else |
| `Block` | named families and/or action names (own actions included) | everything else |
| `Exclusive` | every movement action, attacks included | only the actions or families it names, default none |

**Action families**, from one classify function that is the module's only `dynamic_cast` site,
cached per `Action*`:

- movement: `CombatFormationMove` (exact type), `TankFace`, `SetBehind`, `RearFlank`, `Reach`
  (`ReachTargetAction` except heal), `ReachHeal`, `Follow`, `Flee`, `RunAway`, `MoveRandom`,
  `MoveOutOfCollision`, `MoveOutOfEnemyContact`, `AvoidAoe`, `AnyMovement`;
- spell movers: `Charge` (`CastReachTargetSpellAction`), `Blink`, `Disengage`;
- attacks: `Attack` (`AttackAction`), `Melee`;
- pickers: `DpsAssist`, `TankAssist`, `DpsAoe`, `AggressiveTarget`, `AttackAnything`,
  `AttackLeastHp`, `AttackRti`, `DebuffOnAttacker`, `DropTarget`, `PetAttack`;
- `Taunt`: the eight taunt spell actions (taunt, challenging shout, growl, challenging roar, hand of
  reckoning, righteous defense, dark command, death grip), regardless of role;
- `Spell` (`CastSpellAction`), for hand-written multipliers.

Rarer actions (`killing spree`, `sprint`, a lone `death grip`) and own actions go by name.

### Gate

`RaidEncounter` holds the generic gated trigger and the gate for rules and hand-written multipliers.
Each raid supplies an **encounter gate**: `BossStateGate` (Uld, Naxx, OS, EoE) now, `StageGate` (ToC)
in the last phase. Boss-state rule, unchanged from `UldEncounterGateOpen`: open while this encounter
is `IN_PROGRESS`; closed once `DONE`; otherwise closed only while another encounter is
`IN_PROGRESS`; open outside an instance. Live means `IN_PROGRESS`.

Gate on the **fight**, not the unit. A drake Sartharion calls in never starts its own encounter
(core `boss_sartharion.cpp:778`), so OS mechanics sit in Sartharion's definition. A drake pulled alone
does start its own, which closes Sartharion's gate: an OS node that serves a solo drake (not every OS
node checks `SartharionEncounterActive`) goes in that drake's definition or stays raid-wide.

The generic gated trigger keeps everything `UldGatedTrigger` does: it copies the inner trigger's
name and check interval (a default of 1 would promote throttled triggers), keeps the per-pass id
Yogg-Saron's and Mimiron's caches key on (`UldTriggerPassId`, generalised), and calls
`RaidObs::NamePull` when it fires while live. The `thread_local GatePass` is a cache, so it may stay
`thread_local`.

### Tick hook

New `virtual void OnTick() {}` on `Strategy`, called by `Engine::DoNextAction` once per tick for each
strategy, inside the `RaidObs::BotContext` and before `ProcessTriggers`. A raid strategy forwards it
to every definition whose gate is open. Housekeeping moves there with the condition it has today,
and the predicates become plain reads:

| Today | Where |
|---|---|
| `TickMimironObs`, including the wipe reset | `IsMimironEngaged` (`Uld/Util/UldEncounter_Mimiron.cpp:1009`) |
| `TickFlameLeviathan`, including the wipe reset | `FlameLeviathanEngaged` (`UldEncounter_FlameLeviathan.cpp:311`) |
| `AlgalonTickEncounterState` | `AlgalonEncounterActive` (`UldEncounter_Algalon.cpp:160`) |
| `TickYoggSaronObs`; handover latch writes | `YoggSaronPhase` (`UldEncounter_YoggSaron.cpp:218`); `YoggSaronHandoverState` (`:787`) |
| `vezax.formation` note | `VezaxFormationActive` (`UldEncounter_Vezax.cpp:169`) |
| `barrierBailing` latch writes | `ThorimBarrierBailLatched` (`UldEncounter_Thorim.cpp:1589`) |
| tsunami hazard notes | `SartharionEncounterActive` (`OS/Util/OSEncounter.cpp:117`) |
| `neglect threat` = true | Loatheb, Razuvious, Four Horsemen, Gothik multipliers (`Naxx/NaxxMultipliers.cpp:119,283,534,551`) |

A tick tests cheap instance state before any sweep: between pulls every non-`DONE` encounter is open.

### Native test

The gate rule and the rule evaluator live in a std-only header, `src/Ai/Raid/RaidEncounterRules.h`,
working on family bitmasks, so `tools/nativetest/run.sh` builds them without the server. Cases: the
gate truth table; each rule kind's block and pass sets; mover narrowing and `keep`; the role filter;
and, with call counters, that an action no rule cares about never reaches the gate, role or
predicate.

## Allowed behaviour changes

Shared behaviour from the start. A boss's commit may change only what is listed here and must name
each change it makes; any other difference gets a rule switch or stays hand-written.

- **Gating.** A boss's guards stop once it is `DONE` and while another encounter is in progress.
  Naxx, EoE and OS triggers become gated for the first time.
- **Thorim arena leash** passes attacks (`melee`, the `ThorimTakeBossAction` family).
- **Malygos:** in phase 1, bots other than the boss tank keep `melee` (`tank assist` stays blocked by
  its own rule); phase 4 passes `MalygosTargetAction`.
- **Vezax** `vezax mark of the faceless break action` passes the movement guard.
- **Taunt family** replaces five lists: Hodir and Thorim (7 names), EoE (6, no righteous defense),
  Naxx and OS (4 types).
- Dead list entries go: `auriaya fall from floor action` (a plain `Action`) and `iron assembly tank
  assignment action` (an attack, already exempt).

## Per-boss notes

Only what the multiplier code won't make obvious. Paths under `src/Ai/Raid/`.

- **Algalon:** `AlgalonTargetGuardMultiplier` stays hand-written (wrong-target veto with no picker
  escape).
- **Flame Leviathan:** riders get `Exclusive` passing the drive and board rows plus `LeaveVehicleAction`.
- **Hodir:** `HodirGuardMultiplier` has five branches (taunt, pickers, Flash Freeze, icicle path,
  role hold) → one rule each. Flash Freeze is an `OwnMovement` with `keep` = `Attack`, movers
  narrowed to the freeze set (the shed only in a landed shelter), plus a `Block` on `Charge`, `Blink`,
  `Disengage`.
- **Ignis:** Slag Pot is `Exclusive` with no exceptions; the construct tank and main tank also get a
  `Block` on their own `IgnisScorchedGroundAction`; its targeting rule adds `AttackRti`.
- **Kologarn:** Stone Grip is `Exclusive` with no exceptions; its own attack rows stay blocked.
- **Mimiron:** the formation guard blocks `CombatFormationMove` only, so `tank face` survives.
  `MimironChargeGuardMultiplier` builds five triggers per call; read helpers instead.
- **Razorscale:** `MoversBlocked`'s per-ms cache stays in its predicate.
- **Thorim:** the arena target, balcony wrong-target branch and runic barrier guards stay hand-written.
  `ThorimBalconyGuardMultiplier` builds a trigger per call.
- **XT-002:** `XT002TargetGuardMultiplier` stays hand-written; it was proven equivalent as one unit
  (18.5 M-input harness) and a split would need that proof redone.
- **Yogg-Saron:** the displacement guard names `killing spree`.
- **Naxx:** hand-written: Heigan's dance window, the Sapphiron and Kel'Thuzad healer windows, Gothik's
  unattackable boss, Thaddius' ×2.0 pet boost, Gluth's taunt and Zombie Chow rules, Kel'Thuzad's
  tank-assist ×2. The Four Horsemen guard's `find target "sir zeliek"` misses bots parked on Thane;
  keep it. `NaxxBossHelper` copies per node are untouched.
- **EoE:** `MalygosMultiplier` becomes rules keyed on phase; tank assist on a Scion stays hand-written.
- **OS:** `SartharionMultiplier`'s role and dodge parts become rules, its wrong-target taunt, tank
  assist and rear-flank parts stay hand-written; `OsMechanicPriorityMultiplier` becomes one
  `Exclusive` per live mechanic. The file-local `IsGenericMover` family goes.

## Out of scope

Deepening the Mimiron and Thorim modules, Naxx's per-node helper copies, the threat-redirect trio,
fixing the Four Horsemen lookup.

## Commits

**Status:** commits 1-2 landed; commit 3 in progress, Auriaya done, next Kologarn.

Close each per `CLAUDE.local.md`.

1. **Module and Vezax pilot.** Landed. `UldEncounterGate.{h,cpp}` kept only the prefix table,
   `UldEncounterName` and thin forwards (`UldGatedTrigger` subclasses `EncounterGatedTrigger`;
   `UldEncounterGateOpen`, `UldEncounterIsLive` and `UldTriggerPassId` forward to the generic ones).
   Moving a boss: write its definition, add it to `UldEncounterDefinitions()`, call its
   `AddTriggerNodes`/`AddMultipliers` at its old spot in `UldStrategy.cpp` (keeps node and veto order),
   delete its creators from both contexts and its prefix entry. Syntax-check with a raised
   `PB_MAX_FANOUT`: the contexts reach every `BuildShared*` TU.
2. **pblint learns rows.** Landed.
3. **The other 13 Ulduar bosses, one commit each**, least churned first: Auriaya, Kologarn,
   Razorscale, XT-002, Freya, Algalon, Ignis, Iron Assembly, Flame Leviathan, Hodir, Thorim,
   Yogg-Saron, Mimiron. The last one removes the prefix table, `UldGatedTrigger`, the `Uld*`
   forwards and items 1-2 of the perf plan.
4. **EoE**, then **OS**.
5. **Naxx**, one commit per boss group.
6. **ToC**, after the toc-rework `w6-closeout` lane merges: `StageGate` and definitions replace the
   `ToCEncounterGate`, trigger wrapper and multiplier gating that `w0c-foundation` builds. Until then
   do not touch `src/Ai/Raid/ToC/` or `docs/plans/toc-rework/`.

In-game check per boss: a pull compared with a recent trace (`postmortem.py <file> --vetoes`); only
the listed changes may show.

## Docs

Fold per commit, per [docs/README.md](../../README.md):

- `docs/raids/README.md`: "File layout and wiring" describes definitions; the two suppression
  sections become the rule kinds, keeping the Void Reaver and Noth failure modes; the terms
  (encounter definition, encounter gate, the four kinds) are defined there.
- `docs/raids/ulduar/README.md` layout; `docs/engine/action-selection.md` §"Multiplier semantics" and
  `raid-mechanics-lessons.md` §"What a strategy costs per raid": the action-first rule is enforced by
  the module.
- After the last commit, offer to update the user's `raid-boss-strategy-recipe` skill and memory.
