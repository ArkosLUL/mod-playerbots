# Merge upstream test-staging into Custom (PR ArkosLUL/mod-playerbots#24)

## Context

PR #24 (head `ArkosLUL:test-staging` @ `619a06fc7`, base `Custom`) is `CONFLICTING`. It brings 7 upstream
commits: chat null guard (#2713), whispered-item crash fix, Vigilance rewrite (#2771), RPG quest pickup,
rogue strategy rename (#2766), Gruul rewrite (#2751), Zul'Aman rewrite (#2762).

- `619a06fc7` == local `upstream/test-staging`; merge base with Custom is `075242fd1`. No fetch needed.
  (Local `test-staging` / `origin/test-staging` refs are stale at `075242fd1` — ignore them.)
- Local `Custom` is `64802c8ec`, 1 commit ahead of `origin/Custom` — fine, merge on top of it.
- Dry run (`git merge-tree --write-tree Custom 619a06fc7`) → exactly 4 content conflicts, below.

Pushing resolves the PR: once `Custom` containing `619a06fc7` is pushed, GitHub marks #24 merged.
Push is **not** part of this plan — left to the user.

## Steps

0. Save this plan to `docs/plans/merge-test-staging-pr24/merge-test-staging-pr24.PLAN.md` (untracked;
   deleted at the end, never committed).
1. Preflight: `git status` must be clean (other sessions share the tree — if dirty, stop and ask);
   `gh pr view 24 --repo ArkosLUL/mod-playerbots --json headRefOid` must still be `619a06fc7…` and equal
   `git rev-parse upstream/test-staging`. If the head moved, re-run the dry-run merge-tree before going on.
2. On `Custom`: `git merge upstream/test-staging` (stops on conflicts). Use the
   `resolving-merge-conflicts` skill.
3. Resolve:

| File | Resolution |
|---|---|
| `src/Ai/Class/Warrior/Strategy/GenericWarriorNonCombatStrategy.cpp` | **Keep both** lines: Custom's `"battle shout"` → `battle shout` @`ACTION_NORMAL`, then upstream's `"vigilance"` → `vigilance` @10.0f. |
| `src/Ai/Class/Warrior/WarriorTriggers.h` | **Keep both**: Custom's comment + `SunderArmorStackTrigger` class, then upstream's `class VigilanceTrigger : public BuffTrigger` (the constructor already auto-merged to `BuffTrigger(botAI, "vigilance", 5 * IN_MILLISECONDS)`; the Custom side's `BuffOnPartyTrigger` base is just base text). |
| `src/Ai/Raid/ZA/ZAStrategy.cpp` | **Take upstream** for all 3 hunks (`<boss> should be tanked` → `<boss> tanks position boss` @`ACTION_RAID`, for akil'zon / jan'alai / zul'jin). Custom's only ZA change (4fe60957e) renamed those nodes to names that existed; upstream's rewrite renamed them again and renumbered priorities consistently. Verified: all 29 triggers and every action in upstream's strategy are registered in upstream's `ZATriggerContext.h` / `ZAActionContext.h`, and no other ZA file differs from upstream. |
| `src/Bot/Engine/BuildSharedValueContexts.cpp` | **Union**. Includes: `AiObjectContext.h`, `GDValueContext.h`, `GruulValueContext.h`, `MechValueContext.h`, `MgTValueContext.h`, `UBValueContext.h`, `UldValueContext.h`, `ZAValueContext.h`, `ValueContext.h`. Body = upstream's list (ValueContext, Gruul, ZulAman, Mech, MgT, Underbog, GD) + `valueContexts.Add(new RaidUlduarValueContext());`. Drop the duplicate `TbcDungeonMgTValueContext` line the Custom hunk shows. |

   Then `git grep -nE '^(<<<<<<<|=======|>>>>>>>)' -- src` → empty; `git add` the 4 files.

4. Auto-merged files already checked and need no edits: rename detection carried Custom's
   `DpsRogueStrategy.cpp` changes into `CombatRogueStrategy.cpp` (no `DpsRogueStrategy` left in `src/`);
   Custom's `CastSlamAction`/`CastInterveneAction`/`SunderArmorStackTrigger` coexist with upstream's
   Vigilance rewrite; new `EncounterHelpers::IsEncounterInProgress` / `CanTakeStepTowards` don't collide.
   The compile check in step 5 is what covers upstream's Gruul/ZA code calling APIs Custom changed.

5. Gate before committing (per `CLAUDE.local.md`), on the merged working tree:
   - `PB_MAX_FANOUT=400 ~/.claude/scripts/pb-syntax-check.sh $(git diff --name-only HEAD -- src)`
     (51 code paths; default fanout 15 would silently truncate — run in background). Any failure: fix, or
     report and stop — never commit over it.
   - `python tools/pblint/pblint.py $(git diff --name-only HEAD -- src)` — same rule.
   - No `tools/` change → Python suite not needed.
6. Commit the merge: message via file + `git commit -F` (invoke `use-conversational-language` for any
   body). Subject `Merge upstream/test-staging into Custom`; body ≤2 lines only if a resolution needs a
   why (e.g. ZA took upstream's node names). No AI trailer, no amend, no push, no branch.
7. Docs that the merge makes stale — edit through `/compact-docs-writer`, commit separately so the
   merge commit stays pure:
   - `docs/classes/rogue.md:61` — `DpsRogueStrategy` → `CombatRogueStrategy`; note strategy keys are
     now `combat` / `assassin` (were `dps` / `melee`), Subtlety strategy exists but is not registered.
   - `docs/engine/pitfalls.md:68-73` — the Zul'Aman paragraph describes code upstream replaced (all
     five bosses are now `<boss> should be tanked` / `<boss> tanks position boss`). Keep the lesson
     (a name mismatch is a silent dead node), drop the now-false per-boss specifics.
   - `docs/raids/README.md:11-17` — the Gruul reference layout now includes `<X>ValueContext.h`,
     registered in `BuildSharedValueContexts.cpp`; add it to the layout and registration sites.
8. Delete the plan from step 0.

## Verification

- `git log --oneline -3` shows the merge commit (parents `64802c8ec` + `619a06fc7`) and the docs commit;
  `git merge-base --is-ancestor 619a06fc7 Custom` succeeds.
- `git diff 619a06fc7 Custom -- src/Ai/Raid/ZA` is empty (ZA identical to upstream).
- `git diff 64802c8ec Custom -- src/Ai/Class/Warrior/Strategy/GenericWarriorNonCombatStrategy.cpp`
  shows only the added `vigilance` node.
- Syntax check and pblint from step 5 pass.
- After the user pushes: PR #24 shows as merged.
