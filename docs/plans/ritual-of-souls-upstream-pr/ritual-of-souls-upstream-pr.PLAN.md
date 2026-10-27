# Ritual of Souls upstream PR

## Context

The fork (`G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots`, branch `Custom`) has a
Ritual of Souls feature that upstream `mod-playerbots/mod-playerbots` lacks: warlock bots cast the
ritual, other bots click the Soul Portal, bots without a healthstone take one from the soulwell.
Commits `1eef9b490` (feature) and `323d59ad8` (on by default, Improved Healthstone caster pick).
Durable notes: `docs/systems/consumables-and-burst.md` § Ritual of Souls.

Goal: a ready-for-review PR to upstream `test-staging` that passes upstream review. Two earlier
community ritual PRs (#1664, #1726) were closed over repeated code, long comments and braced
one-line bodies. Upstream rules that hit this feature (`AGENTS.md`, `.agents/docs/cpp-playerbots.md`,
`self-review-rules.md`, `pull-requests.md` on `upstream/test-staging`; read them with
`MSYS2_ARG_CONV_EXCL='*' git show "upstream/test-staging:<path>"`):

- behaviour-changing features are off by default;
- triggers are O(1): no group-member, inventory or nearby scans without a cached value behind them;
- `Execute()` never runs a full search just to return `false`;
- never hold a raw `GameObject*`/`Player*` across ticks, cache the `ObjectGuid`;
- comments: one line preferred, two at most; no braces around one-line bodies.

User decisions (2026-10-27):
- **Off by default, no new config option.** The PR registers the strategy but no bot gets it by
  default. Enable per bot with `nc +ritualofsouls`, or server-wide with
  `Playerbots.NonCombatStrategies` / `Playerbots.RandomBotNonCombatStrategies = "+ritualofsouls"`.
  Custom keeps `"ritualofsouls"` in its default list in `AiFactory.cpp`, so the fork's own behaviour
  stays the same.
- **Rework on Custom first** (cached values + upstream file layout), then port. Custom is the source
  of truth.
- **Open ready for review** after the user has tested the reworked Custom build in game.
- **"Multibot requires update" label**: upstream's `.coderabbit.yaml` makes CodeRabbit apply it
  when a PR adds a player-facing chat command or a player-selectable strategy. This PR adds both
  (`ritualofsouls` strategy, `ritual of souls` command). The user's account has only `pull`
  permission, so it can't set labels; CodeRabbit does that (as on #2874, #2836, #2735, #2982).

## Current shape (Custom) and the cost problems

- `src/Ai/Base/Actions/RitualOfSoulsActions.{h,cpp}`: GO/spell ids, `FindGroupRitualPortal`
  (loops same-map group warlocks' `GetGameObject(29893|58887)`, stamps a group "portal seen" time),
  `FindGroupSoulwell` (grid search for 181621/193169, only within 190 s of a portal sighting),
  helper-slot reservation, `JoinRitualOfSoulsAction`, `UseSoulwellAction`, and both base triggers.
- `src/Ai/Base/Strategy/RitualOfSoulsStrategy.{h,cpp}`: base strategy (join 27, soulwell 26).
- Warlock: `CastRitualOfSoulsAction` + group cast reservation (`WarlockActions.cpp`),
  `RitualOfSoulsTrigger` "group needs healthstones" with `ReadyRitualRank`,
  `IsPreferredRitualCaster`, `HasAnyHealthstone` (`WarlockTriggers.cpp`),
  `WarlockRitualOfSoulsStrategy` (`GenericWarlockNonCombatStrategy.{h,cpp}`), registrations in
  `WarlockAiObjectContext.cpp`.
- Wiring: `ActionContext.h`, `TriggerContext.h`, `StrategyContext.h`,
  `ChatCommandHandlerStrategy.cpp` (`supported.push_back("ritual of souls")`), `AiFactory.cpp`.

Problems: the portal trigger loops the group every 2 s on every bot; the soulwell trigger reads
`"inventory items"` (uncached, interval 1) every 2 s before any group check; the warlock trigger
loops the group and scans every nearby member's bags every 5 s; both actions repeat the search
in `Execute()` and share an identical move-or-click block.

## Design (identical on Custom and the PR, except `AiFactory.cpp`)

**Values** — new `src/Ai/Base/Value/RitualOfSoulsValues.{h,cpp}`, registered in `ValueContext.h`.
The GO and spell id constants move here.
- `"ritual of souls portal"`: `CalculatedValue<ObjectGuid>`, 2 s. Another group member's portal
  within 40 yd. Same-map warlocks only (other maps run on other threads). Any group portal found,
  the bot's own included, stamps the portal-seen time.
- `"ritual of souls soulwell"`: `CalculatedValue<ObjectGuid>`, 2 s. Empty unless the bot is
  grouped, a portal was seen within 190 s and the bot has no healthstone; only then the grid search.

**Warlock value**: new `src/Ai/Class/Warlock/WarlockValues.{h,cpp}`. `"should cast ritual of
souls"` is a bool with a 10 s interval: no own portal (`bot->GetGameObject(spell)`), no group
portal or soulwell (the two values), this bot is the preferred caster, and at least 3 nearby
members, 3 of them without a healthstone. Registered through a new `WarlockValueContextInternal`
added in `WarlockAiObjectContext::BuildSharedValueContexts`, using the pattern of
`PaladinAiObjectContext.cpp` (`PaladinValueContextInternal`). `IsPreferredRitualCaster` and
`HasAnyHealthstone` move here from `WarlockTriggers.cpp`.

**Triggers** — new `src/Ai/Base/Trigger/RitualOfSoulsTriggers.{h,cpp}` (moved out of the actions
file). `"ritual of souls portal nearby"` / `"soulwell nearby"`: out of combat and the value is
set. `RitualOfSoulsTrigger` keeps its cheap checks on the bot itself first (grouped, out of
combat, ritual known and off cooldown, then the shard count), then reads `"should cast ritual of
souls"`. Same trigger names, so the strategies don't change.

**Actions**: resolve the cached GUID with `botAI->GetGameObject(guid)`, return on null. One
shared helper in the `.cpp` does "move into interact range, else send `CMSG_GAMEOBJ_USE`".
`UseSoulwellAction` drops its own healthstone check (the value covers it). The helper-slot and
group cast reservations stay as they are.

**Comments**: at most 2 lines each, nothing that restates the code. Invoke
`use-conversational-language` before writing any comment.

## Steps

### 1. Custom cycle (this session, no stop)
1. Save this plan to `docs/plans/ritual-of-souls-upstream-pr/ritual-of-souls-upstream-pr.PLAN.md`.
2. Rework per the design. Leave `AiFactory.cpp` alone.
3. Close per `CLAUDE.local.md`. Run `PB_MAX_FANOUT=100000 ~/.claude/scripts/pb-syntax-check.sh`
   on every changed `.cpp` plus the includers of changed headers (`ActionContext.h`,
   `TriggerContext.h`, `ValueContext.h` are widely included, so check their includers too). Then
   `tools/pblint/pblint.py <paths>` and `python apps/codestyle/codestyle-cpp.py`. Run
   `/compact-docs-writer` before updating `docs/systems/consumables-and-burst.md` § Ritual of
   Souls: new values and intervals, file locations, and that upstream ships it off by default.
   Commit only the files this cycle touched, plan included.

### 2. In-game test (user)
The user rebuilds the worldserver, or tells me to run the recipe in the memory note
`worldserver-rebuild`; that touches the live server, so only on their word. In a group with a
warlock bot (the caster needs a Soul Shard), take healthstones away from 3+ members. Expected:
the warlock casts, 2 helpers click the portal, a soulwell appears, and every bot without a stone
takes one. Fixes go to Custom first.

### 3. PR branch (stop and ask for the go first: it creates a branch and a worktree)
1. `git worktree add G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-uldpr-wt/ritual-of-souls -b
   ritual-of-souls upstream/test-staging` (after `git fetch upstream`).
2. Port: copy the new files from Custom as they are. In the shared files (which differ a lot
   between Custom and upstream: `WarlockActions.*`, `WarlockTriggers.*`,
   `WarlockAiObjectContext.cpp`, `ChatCommandHandlerStrategy.cpp`, the base contexts), apply only
   the ritual hunks, one by one, never whole-file copies. No `AiFactory.cpp` change, no fork docs.
3. Checks:
   - `PB_REPO=<worktree>` syntax check on the same TU set as step 1.
   - CI-equivalent warnings pass: CI's `CXXFLAGS` with `-Werror`, g++ and clang++, per the memory
     note `build-verification-feasibility`.
   - `python apps/codestyle/codestyle-cpp.py` in the worktree.
   - Upstream cppcheck: `cppcheck --force --inline-suppr --suppressions-list=./.suppress.cppcheck
     src/` in a throwaway `ubuntu:24.04` `docker run --rm` with `--memory`/`--cpus` caps. The
     report must be empty.
   - Grep every new action, trigger, strategy and value name against its creator table.
4. Commit on the branch: one commit, title from the `pr-title` skill (`type(Scope/Subscope):
   Summary` ≤72), why-body via `use-conversational-language`, no AI co-author trailer.

### 4. Publish (show the wording first, then wait for the go)
1. Title via `pr-title`; body via `handover` + `use-conversational-language`. Fill
   `PULL_REQUEST_TEMPLATE.md` completely, in the format of #2987
   (`docs/plans/ulduar-upstream-pr/ulduar-upstream-pr.shaman-lust.HANDOVER.md`).
   - Description: the failure today is that warlock bots never cast the ritual and no bot clicks
     a portal or soulwell, so a real warlock can't finish one with a bot group. Then what changes.
   - Feature Evaluation: the cached values and intervals; no cost when the strategy is off.
   - How to Test: how to turn it on (`nc +ritualofsouls` on each bot, or the two conf lines) and
     the step 2 scenario. Check the ritual's learn levels in the DBC before writing them.
   - Impact Assessment: per-tick "No" while off, cost when on explained; default behaviour "No";
     new decision branches "Yes". Ask the user whether they add `playerbot pmon` figures.
   - AI Assistance: Yes, Claude Code. Provenance: original.
   - Notes for Reviewers: adds the `ritualofsouls` strategy and the `ritual of souls` command, so
     MultiBot and the wiki need updating.
2. On the go: `git push -u origin ritual-of-souls`, then `gh pr create -R
   mod-playerbots/mod-playerbots --base test-staging --head ArkosLUL:ritual-of-souls --title …
   --body-file <scratchpad file>`, ready (not draft), no `--label`.
3. A few minutes later, `gh pr view <n> --json labels`. If CodeRabbit hasn't added "Multibot
   requires update", draft a one-line comment asking a maintainer to add it. Post it only on the
   go.
4. Afterwards: delete the plan dir on Custom (commit), and save a project memory with the PR number.

## Verification
- Step 1 and step 3 checks green before each commit.
- The user's in-game ritual (step 2) passes before anything is pushed.
- Upstream CI on the ready PR (build matrix, codestyle, cppcheck) is green; fixes go to Custom first,
  then to the branch as new commits (upstream changes come in by merge, never rebase).
