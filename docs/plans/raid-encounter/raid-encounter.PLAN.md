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
  reckoning, righteous defense, dark command, death grip) and `taunt on snare target`, regardless of
  role;
- `Spell` (`CastSpellAction`), for hand-written multipliers.

Rarer actions (`killing spree`, `sprint`, a lone `death grip`) and own actions go by name.

### Gate

`RaidEncounter` holds the generic gated trigger and the gate for rules and hand-written multipliers.
Each raid supplies an **encounter gate**: `BossStateGate` (Uld, Naxx, OS, EoE) now, `StageGate` (ToC)
in the last phase. Boss-state rule: open while this encounter
is `IN_PROGRESS`; closed once `DONE`; otherwise closed only while another encounter is
`IN_PROGRESS`; open outside an instance. Live means `IN_PROGRESS`.

Gate on the **fight**, not the unit. A drake Sartharion calls in never starts its own encounter
(core `boss_sartharion.cpp:778`), so OS mechanics sit in Sartharion's definition. A drake pulled alone
does start its own, which closes Sartharion's gate: an OS node that serves a solo drake (not every OS
node checks `SartharionEncounterActive`) goes in that drake's definition or stays raid-wide.

The generic gated trigger copies the inner trigger's name and check interval (a default of 1 would
promote throttled triggers), hands out the per-pass id Yogg-Saron's and Mimiron's caches key on
(`EncounterTriggerPassId`), and calls `RaidObs::NamePull` when it fires while live. The
`thread_local GatePass` is a cache, so it may stay `thread_local`.

### Tick hook

New `virtual void OnTick() {}` on `Strategy`, called by `Engine::DoNextAction` once per tick for each
strategy, inside the `RaidObs::BotContext` and before `ProcessTriggers`. A raid strategy forwards it
to every definition whose gate is open. Housekeeping moves there with the condition it has today,
and the predicates become plain reads.

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
- **Vezax** `vezax mark of the faceless break action` passes the movement guard.
- **Taunt family** replaces five lists: Hodir and Thorim (7 names), EoE (6, no righteous defense),
  Naxx and OS (4 types).
- Dead list entries go: `auriaya fall from floor action` (a plain `Action`) and `iron assembly tank
  assignment action` (an attack, already exempt).

## Per-boss notes

Only what the multiplier code won't make obvious. Paths under `src/Ai/Raid/`.

- **Naxx:** only Grobbulus' guard becomes rules. The rest stay hand-written with the families they
  can zero: each reads a `NaxxBossHelper` that keeps its boss (some also clocks) across ticks, which
  a predicate can't, or sets `neglect threat` (`AnyAction`). The Taunt family replaces Razuvious'
  and Gluth's four types. The Four Horsemen guard's `find target "sir zeliek"` misses bots parked on
  Thane; keep it. `NaxxBossHelper` copies per node are untouched. A class whose own name differs
  from its registered one is renamed to it, except the Mutating Injection triggers (`HasAuraTrigger`
  reads the aura by that name) and `grobbulus move center` (`MoveInsideAction` names itself), whose
  `Name` only registers.

## Out of scope

Deepening the Mimiron and Thorim modules, Naxx's per-node helper copies, the threat-redirect trio,
fixing the Four Horsemen lookup.

## Commits

**Status:** commits 1-4 and Naxx's Arachnid, Plague and Military wings landed; continue with the
Construct wing.

Close each per `CLAUDE.local.md`.

Moving a boss: give its classes `Name` constants, write its definition, list it in the raid's
definitions, call its `AddTriggerNodes`/`AddMultipliers` at its old spot in the strategy (keeps node
and veto order), and delete its creators and whatever hand gating the definition replaces.
Syntax-check with a raised `PB_MAX_FANOUT`: the contexts reach every `BuildShared*` TU. What
Ulduar, EoE and OS taught:

- A multiplier becomes rules only where they are exactly equivalent; otherwise it stays hand-written,
  keeps its name and declares its families. `Family::AnyAction` is for one whose zero can land on
  an item, a plain `Action` or a threat type (burst lists, AoE holds, wrong-target guards).
  `Multiplier<M>(families, args...)` passes constructor arguments (the nature aspect hold).
- A role no `Role` bit expresses goes in the predicate. `Role::NonTank` and `Role::Dps`
  (`IsDps`, the bot's dps strategy) were added for Auriaya and Freya.
- A branchy multiplier splits into several rules under one name; check that no early `return 1`
  in the original shields an action a later rule would zero (Hodir's Flash Freeze).
- Housekeeping found in a read moves to the tick behind the same per-instance throttle, with any
  lookup after the throttle check (Flame Leviathan, Mimiron, Algalon, Yogg-Saron, Thorim).
- An old multiplier's cache becomes a per-bot, per-tick `thread_local` cache in the predicate
  (Razorscale, Malygos' `Holder`), or in the boss's `Util` when hand-written guards share it
  (`SartharionSnapshotFor`).
- An old name list matches `getName()`, not the registered name: `taunt on snare target` is named
  `taunt`, so every list caught it and the Taunt family missed it until EoE.
- A node with several actions becomes back-to-back rows on its trigger, which share one node. A
  node on a generic action is `Node<T>("rear flank", …)`: Naxx has nine.
- A node that also serves another encounter stays in the strategy, after the definition's nodes
  (OS's `sartharion dps` and `twilight portal exit` serve solo drakes). Check equal-priority order.

1. **Module and Vezax pilot.** Landed.
2. **pblint learns rows.** Landed.
3. **The other 13 Ulduar bosses.** Landed; the prefix table, `UldGatedTrigger` and the `Uld*`
   forwards are gone. The Taunt family replaced Hodir's and Thorim's lists, adding Death Grip.
4. **EoE** and **OS**. Landed.
5. **Naxx**, one commit per wing: Arachnid, Plague and Military landed; then Construct, Frostwyrm.
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
