# c1-toc-base

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the
**Wave 1** section bind this lane.

Cleanup on `Custom`, branch `tocpr/c1-toc-base`. Apply the overview's "Every cleanup lane" list and
its "`c1` also" list to the `c1-toc-base` files of the Wave 1 table. Measurements cut from comments
go to the matching doc in `docs/raids/trial-of-the-crusader/`. RaidObs calls stay. A change that
alters behaviour beyond the listed ones is out of scope: log it under Decisions for review.

Lane facts:
- The shared raid files (`src/Ai/Raid/Raid{DirectTargeting,CombatHelpers,BurstWindow,Opener,Defensive,Splash,ResistAura}.{h,cpp}`)
  are also called by ICC and Ulduar on Custom: keep every public name and signature, so other
  raids need no edit. Restyle private members, comments and literals only.
- Rename with `git mv`: `ToCStrategy.{h,cpp}` → `ToCRaidStrategy.{h,cpp}`, `ToCActionContext.h` →
  `ToCRaidActionContext.h`, `ToCTriggerContext.h` → `ToCRaidTriggerContext.h`. Include sites:
  `src/Ai/Raid/RaidStrategyContext.h:29` (unqualified today), `src/Bot/Engine/BuildSharedActionContexts.cpp:9`,
  `src/Bot/Engine/BuildSharedTriggerContexts.cpp:7`, `ToCStrategy.cpp:1`. Also rename the docs that
  name the old files (`git grep` under `docs/` minus `docs/plans/toc-rework/`, which `c4` deletes).
- Self-res guard: `src/Ai/Base/Trigger/GenericTriggers.cpp:27-28` include ToC headers and
  `:302-306` return early on `TrialOfTheCrusaderHelpers::FactionChampionsCorpseCrowded` and
  `CorpseInSlimePool`. A dead bot runs `engines[BOT_STATE_DEAD]` (`src/Bot/PlayerbotAI.cpp:175`,
  switched at `:1615-1644`): before moving the check into a ToC rule or trigger, prove the ToC
  strategy is in that engine (follow `AiFactory::createDeadEngine` and the instance-strategy wiring,
  per `docs/engine/pitfalls.md`). If it isn't, leave `GenericTriggers.cpp` as it is and log it.
- Jaraxxus: `JaraxxusPinnedCastTrigger` calls `GetJaraxxusPinnedCasters`
  (`Util/ToCHelpers_Jaraxxus.cpp:986`) every tick; cache or gate it. `Trigger/ToCTriggers_Jaraxxus.cpp:14`
  and `:57` keep running every tick only to keep the `jaraxxus.interrupter` / `jaraxxus.netherpower`
  trace values current: give them the shape the logic needs alone and log what the trace loses.
- Staging: `Trigger/ToCTriggers_Staging.cpp:16` writes `SetToCStagingParked` inside `IsActive`;
  move the write to the action or a tick.
- Multiplier audit covers `Multiplier/ToCMultipliers_Shared.*` and the lane's encounter multipliers.

## Task list

Not split: the lane's files already met most cleanup items (no `ai`, no bare doubles, 44 comment blocks
over 2 lines, ~20 unprefixed private members), so the rest was a few hundred mechanical lines plus the
lane-only items.

1. [x] Renames with `git mv`: `ToCRaidStrategy.{h,cpp}`, `ToCRaidActionContext.h`, `ToCRaidTriggerContext.h`,
   new include guards; `RaidStrategyContext.h` and `BuildShared{Action,Trigger}Contexts.cpp` include them
   unqualified (sorted), the collision that forced the path is gone.
2. [x] Self-res guard: no ToC hook reaches a dead bot, so `GenericTriggers.cpp` is unchanged (Decisions).
3. [x] `JaraxxusPinnedCastTrigger`: `GetJaraxxusPinnedCasters` keeps its group walk in a per-group,
   per-ms memo of caster guids in `JaraxxusState`, shared by every bot's trigger and the break action.
4. [x] Staging: the park write moved from `ToCStagingStandTrigger::IsActive` to the strategy's
   `OnTick` (`UpdateToCStagingPark`, replacing `SetToCStagingParked`); the trigger is `held && walk`.
5. [x] Trace shapes: Jaraxxus triggers `:14`/`:57` kept their code, comments now state the logic;
   `GetJaraxxusDpsTarget` reads the focus add under a skull only inside `RaidObs::Active()`;
   `GetJaraxxusAssistTankAdd`'s trace prune and Twins' `NoteTankRole` run only while tracing.
6. [x] Cleanup list: comments to 2 lines or less, history and trace evidence out (measurements already in
   the boss docs, the rest added); private members `_lowerCamelCase` (Twin actions, Jaraxxus and Twin
   multipliers, strategy, gated trigger, `ToCEntrySet`, `RaidBurstWindowMultiplier`); ToC header
   constants `inline constexpr` (`ToCData.h`, `ToCHelpers_{Jaraxxus,TwinValkyr,Staging}.h`,
   `ToCActions_Shared.h`); braces off single-statement bodies; literals named; stray `{};`.
7. [x] w6 audits over the lane's multipliers, triggers and state: nothing to fix (Decisions).
8. [x] Docs: `trial-of-the-crusader/README.md` code layout, staging park and drag constants;
   `lord-jaraxxus.md` pinned memo; `twin-valkyr.md` hunter row reason; `raids/README.md` takes the
   `GetRaidCover` deal and `GetRaidPersonalDefensive` ladder cut from `RaidDefensive.h`.

## Decisions for review

- **Self-res guard stays in `GenericTriggers.cpp`; the PR drops it.** A dead bot runs only
  `engines[BOT_STATE_DEAD]` (`PlayerbotAI::DoNextAction` switches `currentEngine`), built by
  `AiFactory::createDeadEngine` from `dead, stay, chat, default, follow`. `ApplyInstanceStrategies` adds
  `trialofthecrusader` to the combat and non-combat engines only, and `Engine::DoNextAction` runs
  `OnTick`, triggers and multipliers of the current engine alone, so no ToC rule, trigger or tick ever
  runs for a dead bot.
- **Pinned cast memo** (listed behaviour change): the walk runs once per group per ms; a bot ticking later
  in the same ms reads the guid list the first one built, then resolves each guid. A cast that starts
  or ends between two bots' ticks inside one ms is seen one tick later. Groupless bots skip the memo.
- **Staging park in `OnTick`** (listed behaviour change): `OnTick` reads the stand and writes the latch
  before triggers, so the trigger's read the same tick gives the same walk. Difference: on minimal ticks
  (trigger nodes under relevance 100 skipped) the latch now still moves; it costs one extra
  `ReadStand` per tick per ToC bot, cheap outside a staging window.
- **Jaraxxus `:14`/`:57` unchanged in code.** `JaraxxusIsFelFireballInterrupter` tests the cast first, so
  the trigger is already logic-only once p3 strips its note; `:57` keeps the stack read first because
  it spares a healing priest or shaman the remover's group walk when he has no Nether Power. Both
  probes keep working, nothing lost.
- **Focus under a skull**: the logic now skips `GetJaraxxusFocusAdd` while a human skull holds; a trace
  still calls it, so `jaraxxus.focus` keeps following the adds.
- **Shared raid headers keep `constexpr`** (brief: restyle private members, comments and literals only).
  Their public names and signatures are unchanged; `RaidDefensive.h`'s deal and ladder detail moved to
  `docs/raids/README.md`.
- **Named literals**: `TOC_DRAG_STEP`/`TOC_DRAG_DONE` in `ToCActions_Shared.h` (Jaraxxus's private copies
  dropped), Jaraxxus `JARAXXUS_NETHER_POWER_PER_STACK`, `SWEEP_RING_STEP`/`SWEEP_ANGLE_STEP`,
  `PORTAL_EXIT_SLACK`, `SAME_DESTINATION`, Twins `ORB_CAST_CLIP_MARGIN`, staging `STAGING_3D_GAP`,
  `CHAMPION_GRIP_CARRY`. Values unchanged. Left as literals: trigger relevance offsets, `90°` turns,
  float epsilons, `4/3` in `PlayerMeleeRange` (the core's formula), `AuraCost` ranks.
- **Braces kept** where an `else` sibling needs them (`ScanArena` call, `GetTwinDpsTarget` picks) and
  around multi-line `if` bodies in loops.
- **Group-walk audit** (c1 files; per call, uncached): Jaraxxus `IncinerateFleshTargetFor` (healers'
  `incinerate flesh on raid` trigger every tick), `JaraxxusIsNetherPowerRemover` (healing priest or
  shaman while Nether Power is up), `JaraxxusIsFelFireballInterrupter` (during the cast, bots with an
  interrupt ready), `AnyLivingAssistTankOne` (assist tank 0 with no Mistress up); Twins
  `OtherColourAllyNear` (orb trigger while an own-colour orb is up), `IsTwinPactInterrupter` (during an
  unshielded Pact), `LustReady` (burst row after the lust window opens); staging `HumanInsideRelease`
  (trace label, runs untraced by design since labels are replayed when the trace opens). Cached already:
  Jaraxxus `InfernoFor`, Twins `ResolveTwinTanks` and door deal, staging deal; action-only: Jaraxxus
  carrier spot, Twins `CriticalAllyNear`.
- **w6 audits**: every lane multiplier tests its action family (or the class, Twins' redirect guard)
  before its encounter gate; definition multipliers are family- and gate-wrapped. No
  `GetFirstAliveUnitByEntry`. The only AI value write is `RaidRestoreLegacyRti` (map 649 only). Per-instance
  state resets per pull (Twins `TwinPull`, staging holds) or self-releases (Jaraxxus threat cap). Lust:
  Twins latch their window per pull, everything else uses the default row.

## Known gaps

- The lane's `src/Ai/Raid/ToC/` files carry no GPLv2 header; the PR branch adds them.

## Blocked

None.
