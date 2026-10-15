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

_Filled by the investigator: numbered tasks in up to 4 groups with disjoint file ownership. Group A
(plumbing and migration) goes first in the list. The investigator spells out the API that group A
delivers (names and signatures), so the boss groups code against it in parallel._

## Decisions for review

## Carried over

## Known gaps

## Blocked
