# w7-direct-targeting — every ToC encounter targets directly, never through raid icons

Wave 4b, before `refine-beasts`. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Line numbers below are as of `8f0f5975d`.

The user rejects raid-icon targeting as bad practice: Ulduar resolves each bot's target in code and
attacks it directly. A 25N Beasts trace showed the cost. After a human took the mobile worm, only the
holder's own action moved the skull, so it stayed on the now-stationary worm, and 18 of 19 DPS hit
the wrong worm from 3:49 to 4:08. Trace:
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_gormok-the-impaler_1792096133.ndjson`.

## The idiom to copy

- **Docs.** Read these first:
  - `docs/raids/ulduar/yogg-saron.md`, "Targeting is direct, never a raid icon";
  - `docs/raids/ulduar/ignis.md`, "Targeting is direct";
  - the top of `docs/raids/ulduar/kologarn.md`;
  - `docs/engine/pitfalls.md`, "A targeting mark never clears inside a pull" and the `rti`
    persistence note above it.
- **Rule.** An encounter declares `EncounterBuilder::OwnTargeting` (`src/Ai/Raid/RaidEncounter.h:160`,
  rule at `RaidEncounterRules.h:108`). It zeroes the `dps assist` and `tank assist` families per role
  and predicate, classified by `ClassifyAction` (`RaidEncounter.cpp`).
  - Per-boss uses: `UldDefinition_Ignis.cpp:83`, `UldDefinition_Kologarn.cpp:61`,
    `UldDefinition_YoggSaron.cpp:180`.
  - Ulduar wiring: `UldStrategy.cpp:34-60`.
- **Gate.** ToC has no boss state, so `BossStateGate` doesn't work here. Write an `EncounterGate`
  (`RaidEncounter.h:25`, two function pointers taking a `uint32` id) over `ToCEncounterGateOpen` /
  `ToCEncounterIsLive` (`src/Ai/Raid/ToC/Util/ToCEncounterGate.h:52-57`). Precedent: `MalygosGate`
  (`EoEDefinition_Malygos.cpp:46-55`).
- **Settling.** A resolver action returns false once the bot is on its target, and its trigger fires
  only when the current target differs. It commands the pet (`CommandPetAttack` / `StopPet`,
  `src/Util/EncounterHelpers.cpp`) before that early return. Examples:
  `UldActions_IronAssembly.cpp:315-324`, `UldTriggers_Kologarn.cpp:54-64`.
- **Coverage.** The resolver covers every window its rule covers. A rule wider than its resolver
  leaves a bot with no target source at all.
- **Pre-pull.** Resolvers run only while the encounter is live. A direct `Attack()` has no
  out-of-combat guard and reaches 100 yd (`kologarn.md`).
- **Human marks.** A skull on a live unit of the encounter wins, read first by the resolver as
  `UldEncounter_IronAssembly.cpp:440-458` does. `attack rti target` is wired into no generic
  strategy, so leaving it alone gives humans nothing.

## Scope

1. **Plumbing.**
   - The ToC `EncounterGate` adapter.
   - One rules-only `EncounterDefinition` per encounter, wired from `src/Ai/Raid/ToC/ToCStrategy.cpp`.
   - Convert the hand-written picker guards into rules where a rule expresses the same thing:
     `GormokSnoboldTargetGuardMultiplier`, `FactionChampionsTargetGuardMultiplier`,
     `AnubarakTankTargetGuardMultiplier`.
2. **Migration.** `rti` persists to the bot's DB, so:
   - An ungated one-shot node resets a bot's `rti` to `skull` while it holds a tag this raid used to
     set (cross, square, triangle, diamond). Patterns: `YoggSaronTickRtiMark`
     (`UldEncounter_YoggSaron.cpp:1333-1347`), and the ungated `toc restore rti cc` node.
   - Once per pull, clear the group icons ToC placed. Pattern: `UldEncounter_Thorim.cpp:1305-1329`.
3. **Northrend Beasts.**
   - Remove the marks at `ToCActions_Gormok.cpp:49-50`, `ToCActions_Jormungars.cpp:110-115` and
     `ToCActions_Icehowl.cpp:75-80`.
   - DPS resolver, in order: snobold pick → Gormok → mobile worm → Icehowl.
     - The mobile worm is skipped while submerged.
     - Resolve it by its form, never by who holds the mobile duty, so a human holder can't strand
       DPS.
     - This order keeps today's heroic-overlap behaviour.
   - Duty holders `Attack()` their beast. Rules: DPS role whole encounter; `tank assist` for duty
     holders while their beast is engaged.
4. **Lord Jaraxxus.**
   - Remove the skull, square, diamond and cross writes (`ToCActions_Jaraxxus.cpp:177-278`), the
     focus-stale trigger, the reset action and the owner set (`ToCHelpers_Jaraxxus.cpp:62,789-805`).
   - DPS resolver: `GetJaraxxusFocusAdd`, else the boss. The DPS rule holds while the gate is open
     and an add lives or he is in combat, because his adds outlive the kill.
   - Add tanks `Attack()` their `GetJaraxxusAssistTankAdd`. `tank assist` is zeroed for the main
     tank and for add tanks holding an add.
5. **Twin Val'kyr.**
   - Remove the icon and `rti` writes (`ToCActions_TwinValkyr.cpp:145-210`) and `TwinRtiIcon`
     (`ToCHelpers_TwinValkyr.cpp:698-701`).
   - The trigger at `ToCTriggers_TwinValkyr.cpp:99` compares the current target, not `rti target`.
   - DPS resolver: `GetTwinDpsTarget`. Twin tanks are covered by a `tank assist` rule.
6. **Faction Champions.**
   - Remove the kill-target skull and its tracker node (`ToCHelpers_FactionChampions.cpp:643-678`,
     `ToCTriggers_FactionChampions.cpp:10-14,35-36`, `ToCActions_FactionChampions.cpp:8-12`). Focus
     is already direct.
   - The CC icons and `rti cc` stay.
7. **Anub'arak.**
   - Remove the boss skull (`ToCActions_Anubarak.cpp:282-283`) and the burrower cross (`:95-112`,
     `:220-226`).
   - The focus burrower becomes a latched guid in `AnubarakState`, replacing the cross read at
     `ToCHelpers_Anubarak.cpp:1265-1276`. The tie-break at `:1278-1284` stays.
   - Remove the side `rti` strings (`:236-248`) and the triggers keyed on them
     (`ToCTriggers_Anubarak.cpp:58-59,74-75`). Side assignment already lives in `RefreshSideTanks`.
   - DPS resolver: focus burrower → the boss while selectable → scarabs while submerged, so the DPS
     rule can cover the whole fight.
8. **Docs.**
   - Rewrite every passage that describes marking, in `northrend-beasts.md`, `lord-jaraxxus.md`,
     `faction-champions.md`, `twin-valkyr.md` and `anubarak.md`; grep for skull, cross, icon, `rti`.
   - Add a "Targeting is direct, no raid icons" convention to `trial-of-the-crusader/README.md`,
     which this brief hands to the lane.
   - Fix the stale source comments that mention marks.
9. **Observability.** Probe each resolver's pick per encounter under its existing prefix. Extend the
   readers and tests only where a pick is worth reading.

## Decisions already taken

- Faction Champions CC icons stay. `rti cc` is the generic class CC pipeline's only input
  (`RtiTriggers.cpp:25-36`, `CcTargetValue.cpp:29`), and it never drives DPS target choice.
- A human's skull wins.
- Scarabs join the Anub'arak resolver.

## Owns

- every ToC stem under `src/Ai/Raid/ToC/` and its seams;
- new `ToCDefinition_<Stem>` files, in a new `Definition/` directory if that matches Ulduar;
- the five boss docs, `trial-of-the-crusader/README.md`, `tools/botobs/bosses/` ToC readers and
  their tests.

Nothing outside ToC.

## Task list

Investigated against `f7921ea4e` (source identical to `8f0f5975d`). No coordinate is proposed and no
spell id is added, so neither navprobe nor the difficulty CSV is needed. Mechanics used below:
both worms submerge 1.5 s apart, `NOT_SELECTABLE` until the emerge
(`boss_northrend_beasts.cpp:568-621`); swarm scarabs come every 4 s while Anub'arak is down, each
with 20000 threat on a random player (`boss_anubarak_trial.cpp:315-346, 450-466`). They are 600 s
summons (66340) and outlive the emerge; only the ten pre-pull prey scarabs despawn, 2 s after the
engage (`:217-226`).

**The shape every encounter follows.**

- One resolver function per encounter returns the bot's pick, or null outside its window or role. It
  notes the pick through `RaidObs::NoteDerived` (change-only, per bot).
- Its trigger fires while the pick exists and `current target` differs. Its action derives from
  `ToCDirectAttackAction` and calls `AttackDirect(pick)`.
- The encounter's `OwnTargeting` rule predicate is `resolver(botAI) != nullptr`, so rule and resolver
  cover the same window by construction.
- Every branch needs an in-combat unit, which is the pre-pull guard.
- The human skull (`GetToCHumanSkull`) is the first branch.

The `Definition/` files are rules-only: no `Node` rows, no `Tick`. Every node stays in its stem's
context.

### Group A — plumbing and migration

Files:

- `src/Ai/Raid/ToC/Util/ToCEncounterGate.h`, `src/Ai/Raid/ToC/Util/ToCEncounterGate.cpp`
- `src/Ai/Raid/ToC/Definition/ToCDefinitions.h` (new)
- `src/Ai/Raid/ToC/ToCStrategy.h`, `src/Ai/Raid/ToC/ToCStrategy.cpp`
- `src/Ai/Raid/ToC/Util/ToCHelpers_Shared.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_Shared.cpp`
- `src/Ai/Raid/ToC/Action/ToCActions_Shared.h`, `src/Ai/Raid/ToC/Action/ToCActions_Shared.cpp`
- `docs/raids/trial-of-the-crusader/README.md`

API the boss groups code against:

```cpp
// Util/ToCEncounterGate.h, which now includes "RaidEncounter.h". The id is
// static_cast<uint32>(ToCEncounter::X). Open forwards to ToCEncounterGateOpen, live to ToCEncounterIsLive.
bool ToCGateOpen(PlayerbotAI* botAI, uint32 encounterId);
bool ToCGateLive(PlayerbotAI* botAI, uint32 encounterId);
inline constexpr EncounterGate ToCGate{&ToCGateOpen, &ToCGateLive};

// Definition/ToCDefinitions.h, includes "RaidEncounter.h". Each one is defined in its group's
// Definition/ToCDefinition_<Stem>.cpp.
EncounterDefinition const& ToCNorthrendBeastsDefinition();
EncounterDefinition const& ToCJaraxxusDefinition();
EncounterDefinition const& ToCFactionChampionsDefinition();
EncounterDefinition const& ToCTwinValkyrDefinition();
EncounterDefinition const& ToCAnubarakDefinition();

// Util/ToCHelpers_Shared.h, namespace TrialOfTheCrusaderHelpers
bool IsToCDpsBot(Player* bot);  // !IsTank(bot) && !IsTank(bot, true) && !IsHeal(bot), PlayerbotAI statics
Unit* GetToCHumanSkull(PlayerbotAI* botAI, bool (*belongs)(Unit*));
void ToCRestoreLegacyRti(PlayerbotAI* botAI);

// Action/ToCActions_Shared.h
class ToCDirectAttackAction : public AttackAction
{
public:
    ToCDirectAttackAction(PlayerbotAI* botAI, std::string const name) : AttackAction(botAI, name) {}

protected:
    bool AttackDirect(Unit* target);
};
```

A boss definition file looks like `UldDefinition_Ignis.cpp`, with predicates in an anonymous
namespace and:

```cpp
EncounterDefinition const& ToCAnubarakDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::Anubarak), ToCGate,
                                                "anub-arak", &DefineAnubarak);
    return definition;
}
```

1. **Gate adapter** in `ToCEncounterGate.{h,cpp}`, as above.
2. **`Definition/ToCDefinitions.h`** declaring the five accessors.
3. **`ToCStrategy`.**
   - `InitMultipliers` calls `ToC<X>Definition().AddMultipliers(botAI, multipliers)` right after
     each encounter's last stem call:
     - Beasts after `AddToCNorthrendBeastsMultipliers`;
     - Jaraxxus after `AddToCJaraxxusMultipliers`;
     - Anub'arak after `AddToCAnubarakMultipliers`;
     - Faction Champions after `AddToCFactionChampionsMultipliers`, before the Defence call;
     - Twins after `AddToCTwinValkyrMultipliers`.

     `ToCBurstWindowMultiplier` stays last.
   - Add `void OnTick() override` and a member `bool legacyRtiChecked = false`. The first tick calls
     `ToCRestoreLegacyRti(botAI)` and sets the flag. `InitTriggers` is unchanged.
4. **Shared helpers.**
   - `IsToCDpsBot` as commented above.
   - `GetToCHumanSkull`:
     - reads the group skull (`RtiTargetValue::skullIndex`) through `botAI->GetUnit`;
     - returns it when it is alive, `IsInCombat()`, not `UNIT_FLAG_NOT_SELECTABLE`,
       `bot->IsValidAttackTarget(unit)` holds, and `belongs(unit)` holds;
     - returns null without a group.
   - `ToCRestoreLegacyRti`: on map 649 only, a bot whose `rti` is `cross`, `square`, `triangle` or
     `diamond` gets `skull`. `rti cc` is Faction Champions' own and stays.
5. **`ToCDirectAttackAction::AttackDirect`**, in this order:
   - null → false;
   - `CommandPetAttack(botAI, target)`;
   - false if `current target` already is target;
   - else `Attack(target)`.

   The pattern is `UldActions_IronAssembly.cpp:315-324`.
6. **README** (via `compact-docs-writer`).
   - New convention "Targeting is direct, no raid icons":
     - the resolver shape above;
     - rule predicate = resolver non-null;
     - settling and pets;
     - the in-combat pre-pull guard;
     - the human skull's conditions;
     - Faction Champions' CC icons and `rti cc` as the only icons left, and why
       (`RtiTriggers.cpp:25-36`, `CcTargetValue.cpp:29`);
     - the legacy `rti` reset;
     - group icons are memory-only, so nothing clears them at a pull (Decisions for review).
   - Code layout:
     - `Definition/ToCDefinition_<Encounter>.cpp` holds an encounter's rules (gate `ToCGate`, no
       rows);
     - the seam list gains "rule: the encounter's Definition file, wired in
       `ToCStrategy::InitMultipliers`";
     - the shared bases gain `ToCDirectAttackAction`;
     - Shared helpers gain `IsToCDpsBot` and `GetToCHumanSkull`.
   - Node table (line 112): `anubarak burrower should be focused` becomes `anubarak dps target`.

### Group B — Northrend Beasts

Files:

- `src/Ai/Raid/ToC/Definition/ToCDefinition_NorthrendBeasts.cpp` (new)
- `src/Ai/Raid/ToC/Util/ToCHelpers_NorthrendBeasts.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_NorthrendBeasts.cpp`
- `src/Ai/Raid/ToC/Util/ToCHelpers_Gormok.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_Gormok.cpp`
- `src/Ai/Raid/ToC/Util/ToCHelpers_Jormungars.cpp`
- `src/Ai/Raid/ToC/Trigger/ToCTriggers_NorthrendBeasts.h`, `src/Ai/Raid/ToC/Trigger/ToCTriggers_NorthrendBeasts.cpp` (new)
- `src/Ai/Raid/ToC/Action/ToCActions_NorthrendBeasts.h`, `src/Ai/Raid/ToC/Action/ToCActions_NorthrendBeasts.cpp` (new)
- `src/Ai/Raid/ToC/Trigger/ToCTriggers_Gormok.h`, `src/Ai/Raid/ToC/Trigger/ToCTriggers_Gormok.cpp`
- `src/Ai/Raid/ToC/Action/ToCActions_Gormok.h`, `src/Ai/Raid/ToC/Action/ToCActions_Gormok.cpp`
- `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_Gormok.h`, `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_Gormok.cpp`
- `src/Ai/Raid/ToC/Trigger/ToCTriggers_Jormungars.h`, `src/Ai/Raid/ToC/Trigger/ToCTriggers_Jormungars.cpp`
- `src/Ai/Raid/ToC/Action/ToCActions_Jormungars.h`, `src/Ai/Raid/ToC/Action/ToCActions_Jormungars.cpp`
- `src/Ai/Raid/ToC/Trigger/ToCTriggers_Icehowl.h`, `src/Ai/Raid/ToC/Trigger/ToCTriggers_Icehowl.cpp`
- `src/Ai/Raid/ToC/Action/ToCActions_Icehowl.h`, `src/Ai/Raid/ToC/Action/ToCActions_Icehowl.cpp`
- `docs/raids/trial-of-the-crusader/northrend-beasts.md`
- `tools/botobs/bosses/northrend_beasts.py`, `tools/botobs/tests/test_northrend_beasts.py`

7. **Resolver** in `ToCHelpers_NorthrendBeasts`:

   ```cpp
   enum class BeastsDpsBranch : uint8 { None, Skull, Snobold, Gormok, MobileWorm, Icehowl };
   struct BeastsDpsPick { Unit* unit = nullptr; BeastsDpsBranch branch = BeastsDpsBranch::None; };
   BeastsDpsPick GetBeastsDpsTarget(PlayerbotAI* botAI);
   ```

   - Empty, with no note, off map 649 or for a bot that isn't `IsToCDpsBot`.
   - Order:
     1. `GetToCHumanSkull`, taking Gormok, Acidmaw, Dreadscale, Icehowl or a Snobold Vassal (34800);
     2. `GetGormokSnoboldPick`;
     3. `GetEngagedBeast(Gormok)`;
     4. `GetBeastOfDuty(WormMobile)` unless `IsWormSubmerged`, so the worm is resolved by form,
        never by its holder;
     5. `GetEngagedBeast(Icehowl)`.
   - No live test: every branch needs an engaged unit, so a snobold outliving the kill still
     resolves.
   - Notes `nb.dps` as `<branch> <DescribeAssignment(guid)>`, or `none`. Branch words are `skull`,
     `snobold`, `gormok`, `mobile`, `icehowl`.
8. **DPS nodes**, one per stem over that resolver.
   - `NorthrendBeastsDpsTargetTrigger(PlayerbotAI*, std::string const name, <stem>)` goes in
     `ToCTriggers_NorthrendBeasts.{h,cpp}`. It is true when the pick's entry belongs to its stem and
     `current target` differs:
     - Gormok or a snobold belong to the `gormok` stem;
     - Acidmaw and Dreadscale belong to the worms;
     - Icehowl belongs to `icehowl`.
   - `NorthrendBeastsDpsTargetAction : ToCDirectAttackAction` goes in
     `ToCActions_NorthrendBeasts.{h,cpp}`: `AttackDirect(GetBeastsDpsTarget(botAI).unit)`.
   - Each stem registers its own in its context and node list, at `ACTION_RAID + 2`, with trigger
     and action under one name: `gormok dps target`, `northrend worms dps target`,
     `icehowl dps target`.
   - Delete `gormok snobold on raid` → `gormok focus snobold`: classes, creators and node.
9. **Swap hold**: `gormok swap tank duty` → `gormok swap tank hold` at `ACTION_RAID + 1`.
   - Fires on duty `GormokSwap`, Gormok engaged and not the current target.
   - The action is `AttackDirect(gormok)`, with no taunt and no drag. The swap taunt node stays as
     it is.
10. **Remove the marks.**
    - `ToCActions_Gormok.cpp:49-50`, `ToCActions_Jormungars.cpp:106-116` (the block and its comment),
      `ToCActions_Icehowl.cpp:75-80`. Each hold keeps its own `Attack()`.
    - Reword the skull comments at `ToCHelpers_Jormungars.cpp:746,766`.
11. **Definition** `ToCDefinition_NorthrendBeasts.cpp` (`ToCEncounter::NorthrendBeasts`,
    `northrend-beasts`):
    - `OwnTargeting("northrend beasts dps target guard", Role::Any, GetBeastsDpsTarget(botAI).unit != nullptr, Family::DpsAssist)`;
    - `OwnTargeting("northrend beasts tank target guard", Role::Any, duty = GetBeastsTankDuty(botAI), duty != None && GetBeastOfDuty(botAI, duty), Family::TankAssist)`.
    - Delete `GormokSnoboldTargetGuardMultiplier`, both the class and its `AddToCGormokMultipliers`
      line.
12. **Doc** `northrend-beasts.md` (via `compact-docs-writer`).
    - Rewrite every marking passage: Gormok Hold and Snobolds, worms Kill target and Holds, the
      Redirect's "melee hit the skull", Icehowl Hold, and the Known-gaps line on a hunter's
      Misdirection "on the skull".
    - Add the resolver order, the swap hold and both rules.
    - "What a trace answers" gains `nb.dps` and `--targets`.
13. **Reader.**
    - `northrend_beasts.py --targets`: per DPS bot (role melee or ranged, not a human), sampled
      living time on its `nb.dps` pick, on another beast, or on neither, split by branch. Then the
      raid's on-pick share per branch.
    - Test in `test_northrend_beasts.py` with a synthetic trace: one bot on its mobile-worm pick, one
      on the other worm.

### Group C — Lord Jaraxxus and Twin Val'kyr

Files:

- `src/Ai/Raid/ToC/Definition/ToCDefinition_Jaraxxus.cpp` (new)
- `src/Ai/Raid/ToC/Definition/ToCDefinition_TwinValkyr.cpp` (new)
- `src/Ai/Raid/ToC/Util/ToCHelpers_Jaraxxus.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_Jaraxxus.cpp`
- `src/Ai/Raid/ToC/Trigger/ToCTriggers_Jaraxxus.h`, `src/Ai/Raid/ToC/Trigger/ToCTriggers_Jaraxxus.cpp`
- `src/Ai/Raid/ToC/Action/ToCActions_Jaraxxus.h`, `src/Ai/Raid/ToC/Action/ToCActions_Jaraxxus.cpp`
- `src/Ai/Raid/ToC/Util/ToCHelpers_TwinValkyr.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_TwinValkyr.cpp`
- `src/Ai/Raid/ToC/Trigger/ToCTriggers_TwinValkyr.h`, `src/Ai/Raid/ToC/Trigger/ToCTriggers_TwinValkyr.cpp`
- `src/Ai/Raid/ToC/Action/ToCActions_TwinValkyr.h`, `src/Ai/Raid/ToC/Action/ToCActions_TwinValkyr.cpp`
- `docs/raids/trial-of-the-crusader/lord-jaraxxus.md`, `docs/raids/trial-of-the-crusader/twin-valkyr.md`
- `tools/botobs/bosses/val_kyr_twins.py`, `tools/botobs/tests/test_val_kyr_twins.py`

14. **Jaraxxus resolver** `Unit* GetJaraxxusDpsTarget(PlayerbotAI* botAI)` in `ToCHelpers_Jaraxxus`.
    - Null unless `IsToCDpsBot` and `ToCEncounterGateOpen(Jaraxxus)`. Open, not live: his adds
      outlive the kill.
    - Order:
      1. `GetToCHumanSkull`, taking Jaraxxus or `IsJaraxxusAdd`;
      2. `GetJaraxxusFocusAdd`;
      3. `GetJaraxxus` if `IsInCombat()`.
    - Notes `jaraxxus.dps`: `skull`, `add`, `boss`, `none`.
15. **Node** `jaraxxus dps target` → `jaraxxus dps target` at `ACTION_RAID + 1`
    (`ToCDirectAttackAction`). It replaces `jaraxxus add should be focused` → `jaraxxus focus add`.
    Delete:
    - `jaraxxus focus stale` → `jaraxxus reset focus`: classes, creators, node;
    - `JaraxxusClaimRti`, `JaraxxusOwnsRti`, `JaraxxusReleaseRti` and `rtiOwners`
      (`ToCHelpers_Jaraxxus.h:103-106`, `.cpp:62,789-805`).
16. **Remove the marks.**
    - The main tank's skull (`ToCActions_Jaraxxus.cpp:177-178`).
    - `HoldAdd`'s square/diamond and claim (`:209-219`), plus the stale comment at
      `ToCActions_Jaraxxus.h:72`.
17. **Jaraxxus definition** (`ToCEncounter::Jaraxxus`, `lord-jaraxxus`):
    - `OwnTargeting("jaraxxus dps target guard", Role::Any, GetJaraxxusDpsTarget != null, DpsAssist)`;
    - `OwnTargeting("jaraxxus tank target guard", Role::Any, ..., TankAssist)`. The predicate is the
      exact test of the three hold triggers:
      - `IsMainTank && GetJaraxxus && IsInCombat`;
      - or `IsAssistTankOfIndex(bot, 0, true) && GetJaraxxusAssistTankAdd(botAI, 0)`;
      - or the same for index 1.
18. **Twins.**
    - Delete `TwinRtiIcon` (`ToCHelpers_TwinValkyr.h:91-92`, `.cpp:698-701`) and every icon and
      `rti` write in `ToCActions_TwinValkyr.cpp:145-210`: `HoldTwin`, both holds, `FocusTwin`, the
      comment at 205.
    - `TwinValkyrFocusTwinAction` becomes a `ToCDirectAttackAction`:
      `AttackDirect(GetTwinDpsTarget(botAI))`.
    - `TwinValkyrDpsTargetTrigger` compares `current target` only (`:99`) and takes its role from
      `IsToCDpsBot`.
    - `GetTwinDpsTarget` reads `GetToCHumanSkull` first, taking either twin; its `tv.target` rule is
      `skull`.
    - Move `IsAttackableTwin` out of the triggers' anonymous namespace into `ToCHelpers_TwinValkyr`,
      as `bool IsTwinAttackable(Unit* twin)`.
19. **Twins definition** (`ToCEncounter::TwinValkyr`, `val-kyr-twins`):
    - DPS rule, `DpsAssist`: `IsToCDpsBot && ToCEncounterIsLive(TwinValkyr) && GetTwinDpsTarget`;
    - tank rule, `TankAssist`: `(IsTwinAttackable(fjola) && IsTwinTank(bot, fjola)) || (IsTwinAttackable(eydis) && IsTwinTank(bot, eydis))`.
20. **Docs** `lord-jaraxxus.md` and `twin-valkyr.md` (via `compact-docs-writer`).
    - Jaraxxus: the node table rows at 122-123, the icons, the rules, `jaraxxus.dps`.
    - Twins: the per-bot RTI passage at 136-142, the rules, the `skull` rule of `tv.target`.
21. **Twins reader.** `target_rows` counts a sample against the twin `tv.target` names while that
    bot's rule is `skull`. Test in `test_val_kyr_twins.py`.

### Group D — Faction Champions and Anub'arak

Files:

- `src/Ai/Raid/ToC/Definition/ToCDefinition_FactionChampions.cpp` (new)
- `src/Ai/Raid/ToC/Definition/ToCDefinition_Anubarak.cpp` (new)
- `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.cpp`
- `src/Ai/Raid/ToC/Trigger/ToCTriggers_FactionChampions.h`, `src/Ai/Raid/ToC/Trigger/ToCTriggers_FactionChampions.cpp`
- `src/Ai/Raid/ToC/Action/ToCActions_FactionChampions.h`, `src/Ai/Raid/ToC/Action/ToCActions_FactionChampions.cpp`
- `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_FactionChampions.h`, `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_FactionChampions.cpp`
- `src/Ai/Raid/ToC/Util/ToCHelpers_Anubarak.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_Anubarak.cpp`
- `src/Ai/Raid/ToC/Trigger/ToCTriggers_Anubarak.h`, `src/Ai/Raid/ToC/Trigger/ToCTriggers_Anubarak.cpp`
- `src/Ai/Raid/ToC/Action/ToCActions_Anubarak.h`, `src/Ai/Raid/ToC/Action/ToCActions_Anubarak.cpp`
- `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_Anubarak.h`, `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_Anubarak.cpp`
- `docs/raids/trial-of-the-crusader/faction-champions.md`, `docs/raids/trial-of-the-crusader/anubarak.md`
- `tools/botobs/bosses/faction_champions.py`, `tools/botobs/bosses/anub_arak.py`
- `tools/botobs/tests/test_faction_champions.py`, `tools/botobs/tests/test_anub_arak.py`

22. **Faction Champions.**
    - **Skull and its node.** Drop the kill-target skull: `moveSkull` and the skull placement
      (`ToCHelpers_FactionChampions.cpp:452-480,643-678`).
    - **CC-icon hygiene stays.** It clears a CC icon on a champion no assignment gives it to, and
      after the pull the icons this encounter placed. Rename what is left:
      - the node `faction champions mark targets` → `faction champions clear cc icons`, with classes,
        creators and node list;
      - `FactionChampionsMarksPending` / `FactionChampionsApplyMarks` →
        `FactionChampionsCcIconsStale` / `FactionChampionsClearStaleCcIcons`;
      - the header comment at `:81-83`.
    - **Human skull.** In `UpdateKillLatch`, a skull from `GetToCHumanSkull` (taking
      `IsFactionChampion`) on a touchable in-combat champion becomes the latch, reason `skull`,
      unless it already is. `UpdateCcAssignments` leaves the skulled champion out of its candidates.
    - **Focus action.** `FactionChampionsFocusPriorityAction` becomes a `ToCDirectAttackAction` using
      `AttackDirect`.
    - **Rule.** Delete `FactionChampionsTargetGuardMultiplier`. The definition
      (`ToCEncounter::FactionChampions`, `faction-champions`) declares
      `OwnTargeting("faction champions target guard multiplier", Role::Any, FactionChampionsFocusBot && FactionChampionsKillTarget, DpsAssist | TankAssist)`.
      The old name stays, since the reader and its test read it.
23. **Anub'arak.**
    - **Delete:**
      - `ClearStaleBurrowerCross` (`ToCActions_Anubarak.cpp:95-112`);
      - the skull, cross and `rti` writes (`:220-226,236-248,282-283`);
      - `ANUBARAK_BURROWER_RTI`, `ANUBARAK_SIDE_RTI` and `IsAnubarakSideRti`
        (`ToCHelpers_Anubarak.h:65-67,147`, `.cpp:1209`);
      - the triggers' `rti` clauses (`ToCTriggers_Anubarak.cpp:58-59,74-75`);
      - `RtiTargetValue.h` includes left unused.
    - **Focus latch.**
      - `RaidObs::ObsValue<ObjectGuid> focusBurrower{"anub.focus"}` goes in `AnubarakState` and is
        cleared in `ClearPullLatches`.
      - `GetAnubarakFocusBurrower` keeps the latched burrower while it is alive, selectable and
        `UnitOnPermafrost`.
      - Otherwise it re-picks the lowest health with the guid tie-break (`.cpp:1278-1284`) and
        latches that, empty when none.
    - **Resolver** `Unit* GetAnubarakDpsTarget(PlayerbotAI* botAI)`.
      - Null unless `IsToCDpsBot` and `AnubarakEngaged`, and null for a sphere shooter
        (`IsRangedDps && GetAnubarakSphereToShoot`).
      - Order:
        1. `GetToCHumanSkull`, taking Anub'arak, a burrower or a scarab;
        2. `GetAnubarakFocusBurrower`;
        3. the boss while not `NOT_SELECTABLE`;
        4. while the phase is `Submerged`, the nearest scarab within 30 yd
           (`ANUBARAK_SCARAB_PICKUP_RANGE`) that is alive, selectable, not `NON_ATTACKABLE`, and in
           combat.
      - Notes `anub.dps`: `skull`, `burrower`, `boss`, `scarab`, `none`.
    - **Node** `anubarak dps target` → `anubarak dps target` at `ACTION_RAID + 4`
      (`ToCDirectAttackAction`). It replaces `anubarak burrower should be focused` →
      `anubarak focus burrower`.
    - **Side-tank trigger.** `anubarak burrower needs assist tank` fires on
      `GetAnubarakBurrowerPick` alone.
    - **Rules.** Delete `AnubarakTankTargetGuardMultiplier`. The definition
      (`ToCEncounter::Anubarak`, `anub-arak`) declares:
      - `OwnTargeting("anubarak dps target guard", Role::Any, GetAnubarakDpsTarget != null, DpsAssist)`;
      - `OwnTargeting("anubarak tank target guard", ...)` three times, one rule per clause of the old
        multiplier:
        - `AnubarakEngaged && IsAnubarakPickupTank && phase Surface or Swarm`, `DpsAssist | TankAssist`;
        - `AnubarakEngaged && GetAnubarakBurrowerPick`, `DpsAssist | TankAssist`;
        - `AnubarakEngaged && IsRangedDps && GetAnubarakSphereToShoot`, `DpsAssist`.
24. **Docs** `faction-champions.md` and `anubarak.md` (via `compact-docs-writer`).
    - Faction Champions: the Marks and Focus rows at 162-163, the guide line at 76 (the guide marks
      every target, bots don't), the skull note at 183, the `skull` switch reason.
    - Anub'arak:
      - the node table at 143-146;
      - the side-tank and burrower passage at 191-196;
      - Known gaps 274-275;
      - the rules;
      - `anub.focus` and `anub.dps`.
25. **Readers.**
    - `faction_champions.py`: `SWITCH_REASONS` gains `skull`, documented in the docstring.
    - `anub_arak.py --burrowers`: each burrower's time holding `anub.focus` and the DPS bots' share on
      it then, plus `show_counts(trace, "anub.dps")`.
    - Tests in `test_faction_champions.py` and `test_anub_arak.py`.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| No per-pull clear of group icons | Icons live in memory only. `Group::SetTargetIcon` never writes the DB (`Group.cpp:1834-1853`); `Create` saves the empty set once (`:184-191`) and load reads it back (`:238`). A deploy restarts the server, so no icon an older build placed survives. A clear could only remove a player's marks, against "a human's skull wins" | Thorim's once-per-pull clear (`UldEncounter_Thorim.cpp:1305-1329`) of the icons ToC used |
| Legacy `rti` reset once per strategy object, from `ToCStrategy::OnTick` | The `YoggSaronTickRtiMark` pattern. It needs no raid-wide trigger name (which would join `RAID_WIDE_TRIGGERS` in `test_toc_naming.py`), and a player's later `rti` order sticks until the strategy is rebuilt | An ungated node firing whenever `rti` holds one of the four |
| Beasts DPS nodes split per stem over one resolver | Trigger gating keys on the name prefix, and the naming contract has no Beasts-wide prefix. Adding one touches `raidobs/encounter.py`, outside ToC, and the overview | One node under `gormok`; or a new `northrend beasts` prefix (merge stage) |
| Beasts order ends at Icehowl, no stationary-worm fallback | The brief's order. Both worms go under within 1.5 s, so a lone up stationary worm lasts at most 1.5 s; the rule lapses there and `dps assist` covers | Stationary worm as the last branch |
| `GormokSwap` holder gets a hold node attacking Gormok | "Duty holders `Attack()` their beast". With the tank rule on, the swap holder otherwise has no target between swaps; today it reached Gormok through `TankTargetValue` reading the skull | Leave `GormokSwap` out of the tank rule, on `tank assist` |
| Human skull counts only on an alive, in-combat, selectable, attackable unit of the encounter; Anub'arak's frost spheres excluded | Pre-pull `Attack()` reach (`kologarn.md`). Spheres are a ranged-only, limited resource (heroic six; guide: "Do not attack Frost Spheres unless explicitly told to") | Any live unit of the encounter |
| Faction Champions' skull feeds the kill latch (`fc.switch` `skull`), and CC skips the skulled champion | One source for Counterspell, purge, the physical switch and CC. `Group::SetTargetIcon` strips other icons off a target (`Group.cpp:1840-1843`), so a CC icon landing would erase the skull | Skull read only in the focus action, latch untouched |
| The FC marks node stays, renamed `faction champions clear cc icons`, skull part removed | The skull placement used to strip a CC icon off the next kill target. Without it, this node alone clears a CC icon left on a champion after its assignment ends, which a bot whose restored `rti cc` names that icon would CC | Delete the node, as the brief's line reads |
| The converted Faction Champions rule keeps its old multiplier name, `faction champions target guard multiplier` | `faction_champions.py` `FC_MULTIPLIERS` and its test read it | New name plus reader edits |
| Anub'arak's three tank-side rules get one name each: `anubarak pickup tank target guard`, `anubarak side tank target guard`, `anubarak sphere shooter target guard` | A veto is credited to the rule's name, so one shared name hid which clause fired; no reader reads the old one | Keep the shared `anubarak tank target guard` |
| Anub'arak scarab pick: nearest valid scarab within 30 yd per bot | Each scarab puts 20000 threat on a random player, so there's no tank to converge on; 30 yd matches the tank pickup range | Lowest-health scarab raid-wide |
| Sphere shooters out of the Anub'arak resolver, under their own rule | w5's anti-flip: the sphere node (+5) and a resolver (+4) would swap the shooter's target every tick | One resolver with a sphere branch |
| Resolver role `IsToCDpsBot` everywhere (Anub'arak used `IsDps`) | One helper keeps rule and resolver identical. Matches the Jaraxxus, Twins and snobold tests | Per-encounter role tests |
| Beasts and Jaraxxus resolvers use the open gate with an engaged unit per branch; Twins, Faction Champions and Anub'arak need live/engaged | Snobolds and Jaraxxus's adds outlive the kill (`EncounterBeforeStage`). The in-combat test on every branch is the pre-pull guard | Live only, leaving leftovers to `dps assist` |
| Twins: a player's skull wins over the Pact twin too | Brief: "A human's skull wins" | Pact first |
| Definitions wired right after each encounter's stem multipliers | A trace credits the first rule or multiplier that zeroes an action, in declaration order | All at the end |
| `icehowl dps target` inserted ahead of `icehowl breath spread`, both `ACTION_RAID + 2` | `IcehowlMoveToBreathStandAction` returns true while its walk is in flight, holding the tick, and `Attack()` only clears walks below `MOVEMENT_COMBAT`, so behind the stand a DPS switch waits for the walk to land | After the breath stand |
| Each Beasts stem's DPS trigger tests the pick's entry (Gormok or snobold 34800 / Acidmaw or Dreadscale / Icehowl), not its branch | A skull pick then fires the node of the stem the skulled unit belongs to | Route the skull branch to one fixed stem |
| Snobold branch adds no in-combat test beyond `GetGormokSnoboldPick`'s "rides a living player" | Seated snobolds ride Gormok, never a player, so nothing is picked before the pull and `nb.snobold` and `nb.dps` can't disagree | An extra `IsInCombat` test |
| `northrend_beasts.py --targets` counts nothing while `nb.dps` is `none` or before a bot's first `nb.dps` row; snobolds count as "another beast"; steps capped at `MAX_STEP_MS` (2 s); humans excluded by the roster `h` flag | Scores only samples the resolver had a pick for; the cap keeps a trace gap from crediting one target | Count `none` samples as "neither" |
| `GetJaraxxusDpsTarget` reads `GetJaraxxusFocusAdd` even when a skull wins | Keeps the `jaraxxus.focus` latch following the adds; skipped, it would freeze on a possibly dead add while the skull holds | The brief's order literally, no focus read under a skull |
| `GetAnubarakDpsTarget` reads `GetAnubarakFocusBurrower`, and `GetBeastsDpsTarget` reads `GetGormokSnoboldPick`, before the skull branch | The Jaraxxus row: each is the only writer of `anub.focus` / `nb.snobold`, which would go stale under a skull and make `--burrowers` / `--snobold` misread a skull window | Skull first, no read under it |
| Every resolver trigger (and `gormok swap tank duty`) also fires while `ToCPetOffPick`: a living, non-passive pet the bot could send isn't on the pick | The action is the pet's only sender, and it ran only while the owner switched, so a resummoned, revived, drawn-off or never-sent pet stayed off target; `AttackDirect` returns false once the owner is settled, so the extra firings cost no tick | Pet command only on the owner's switch |
| Leftover phase 2 scarabs documented as a gap, not given a surfaced DPS branch | The guide only says "Be mindful of your aggro on Swarm Scarabs in Phase 2"; the old skull-on-boss behaviour was the same | A branch between the focus burrower and the boss: an in-combat scarab within 30 yd whose victim is a healer |
| The rule = resolver non-null convention kept and documented as a deliberate break from `docs/raids/README.md` "Targeting: the whole fight" | No ToC resolver ends in a terminal fallback, so a wider rule strands a bot; the `dps assist` windows are listed in Known gaps | A terminal Anub'arak fallback (nearest attackable scarab at any range) so its rule spans the whole Submerged phase |
| Jaraxxus hold triggers and tank rule share `JaraxxusMainTankHolds` / `JaraxxusAssistTankHolds(index)` | Task 17: the rule is the hold triggers' exact test, by construction | Copy the test into the Definition file |
| `JaraxxusAnyAddAlive` removed | Its only caller was the deleted focus-stale trigger | Keep it unused |
| Twins rule names `twin valkyr dps target guard` and `twin valkyr tank target guard` | The Jaraxxus pattern; the brief names none for Twins | Other names |
| `jaraxxus.dps` not written off map 649 or for a non-DPS bot, `none` while the gate is closed | Matches `nb.dps` | Note `none` for every bot |
| `val_kyr_twins.py --targets` text says the twin the "rule" names, not the "colour" | A skull isn't a colour | Keep "colour" |
| `anubarak burrower needs assist tank` keeps its pickup-tank-while-surfaced exclusion, drops only the `rti` clause | `GetAnubarakBurrowerPick` alone would let side 0's tank, pickup tank after a main-tank death, run the burrower hold (+3) over the boss hold (+1) | The brief's literal "pick alone" |
| A sphere shooter's `GetAnubarakDpsTarget` is null with `anub.dps` `none`; non-DPS bots and calls outside the pull write no note | The note shows why a DPS bot has no pick | No note for shooters |
| FC skull check runs first in `UpdateKillLatch`, before the return to a suspended champion; a skull on the suspended one clears the suspension; a skulled but immune champion stays off the CC candidates while the normal latch runs | "A human's skull wins": `back` must never pull the latch off a skull | Skull checked after the suspension return |
| FC post-pull clear iterates CC icons only, never the skull | Bots place no skull, so any skull is a player's | All eight icons, as before |
| FC and Anub'arak skull `belongs()` require `IsCreature()`; Anub'arak's takes boss, burrower and scarab entries, not spheres | Only the encounter's creatures count, whatever else a skull sits on; spheres per the human-skull row | Entry test only |
| README spell-id table keeps `anubarak dps target` → "Permafrost on the burrower" | The table lists the spell each node keys on; only the burrower branch reads a remapped aura | Describe every branch there |

## Carried over

- **Merge stage, engine lessons** for `docs/engine/pitfalls.md`.
  - Group target icons are memory-only and gone after a restart (row 1 above). Only per-bot `rti` and
    `rti cc` persist (`AiObjectContext::Save`).
  - `mark rti` (`MarkRtiStrategy`, off by default) has a bot place its own `rti` icon on the
    lowest-health unmarked attacker. With it on, a skull is no longer proof of a person.
  - `Group::SetTargetIcon` clears every other icon on the same target.
- **Merge stage, `docs/raids/README.md` "Targeting: the whole fight".** It rules out the gate ToC
  uses (zero assist only while the encounter's lookup is non-null). Amend it to say when an
  exact-test rule is fine: a resolver with no terminal fallback, its `dps assist` windows listed as
  known gaps (ToC; Yogg-Saron's tank rule is a precedent). Outside this lane's Owns.
- **Integrate step.** No registration outside group A: every new node lives in an existing stem
  context. Re-run CMake for the new `.cpp` files and the new `Definition/` directory.
- No `-rest` brief: the scope fits four groups. Re-review left no open issue and no deferred
  finding.

## Known gaps

- A skull from a bot running `mark rti` reads as a player's.
- A player's per-bot `rti` of cross, square, triangle or diamond is reset once each time the ToC
  strategy is rebuilt.
- In the 1.5 s between the two worms' submerges, and in Anub'arak's phase 2 with no scarab within
  30 yd, DPS fall back to `dps assist`.
- Scarabs alive at Anub'arak's emerge are no DPS pick once he surfaces; only side tanks with nothing
  to take peel them off non-tanks.
- Faction Champions' CC icons are still group icons (decided).
- No reader section for `jaraxxus.dps`. `postmortem.py --notes jaraxxus.` shows it.

## Blocked

None.
