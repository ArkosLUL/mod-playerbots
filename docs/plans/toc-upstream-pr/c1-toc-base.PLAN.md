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

## Decisions for review

## Known gaps

## Blocked
