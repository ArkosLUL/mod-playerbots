# Strip comments from EoE / Ulduar / ToC / ICC / OS raid code, knowledge into docs

## Context

`src/Ai/Raid/{EoE,Uld,ToC,ICC,OS}` carry ~13,700 comment lines besides license headers, in 472
files (Uld ~7,100, ToC ~3,100, ICC ~2,500, OS ~760, EoE ~230; plus trailing `// …` after code).
They rot with every code change and nothing checks them. The user wants them gone, with anything
durable moved into `docs/raids/…`, and the rule that produced them made stricter.

User decisions (2026-11-22):
- **Floor:** keep only the license header, tool directives (`pblint:`, `NOLINT`, `clang-format`;
  none exist in scope today), and a **one-line hazard** where editing that exact line silently
  breaks something the code can't show. Everything else goes.
- **Upstream PRs** (#2877 EoE, #2986 Ulduar, #3064 ToC): this work changes Custom only; the next
  sync, on the user's go, runs the same strip tool on the PR branches. ICC and OS have no PR.
- **No pblint check**; enforcement is rule + skill text.
- **Tighten** the global `no-nonsense-comments` rule (agent-toolkit) and add a line to the
  `raid-boss-strategy-recipe` skill. The stricter rule stays fork-only (no upstream PR); a project
  with no docs tree sends knowledge to the ticket / PR description and never gets a docs tree
  created for it.
- **What moves:** only what the strategy relies on (a number the code uses, a mechanic bots react
  to, a value measured in a trace, a decision and its reason, a trap, an open gap) and the doc
  lacks. Plain spell / field descriptions the code doesn't depend on are dropped (DBC and core
  scripts are the source).
- **Docs are compacted too:** each lane runs `compact-docs-writer` over every doc it owns, as a
  second commit; nothing moves between files.
- **Reviewers:** one fresh reviewer per lane, on Sonnet.
- **Polish loop keeps running**; `tools/commentstrip/` stays permanently. **ICC runs now**, not
  after `icc-rebuild`'s `r14-closeout`.
- **agent-toolkit sync authorized** (merge upstream, install new skills, push) as the first step.

**agent-toolkit is not up to date:** the fork's `main` is 74 commits (9 weeks) behind
`upstream/main` (eai-org), 2 customization commits ahead, and 1 commit (`4951c80`) not pushed to
`origin`. The daily auto-update pulls `origin` (the fork), so upstream changes never arrive on
their own. Upstream never touched `no-nonsense-comments.md`; a merge conflicts only in
`rules/plans-directory.md`; upstream adds skills `extract-pr-review-rules`, `grill-me`,
`harden-artifact` and doc/skill check scripts (`compact-docs-writer/scripts/check-doc.sh`,
`compact-skill-creator/scripts/check-skill.sh`).

Out of scope: other raids, shared `src/Ai/Raid/Raid*.{h,cpp}`, `src/Util/`, Python tools.

## Phase 0 — agent-toolkit sync, rule and skill (inline)

0. **Authorized by the user (2026-11-22):** merge `upstream/main` into the fork's `main`, re-run
   `install.sh` to link the three new skills, push `main` to `origin`. Conflict resolution:
   keep the fork's `plans-directory.md` and append upstream's new worktree paragraph; confirm the
   other customizations survive (`compact-docs-writer` `model: sonnet`, `git-read-only-by-default`,
   `write-realistic-texts`, `right-size-subagents`). Run the toolkit's `test/auto-update/run.sh`.
1. `G:/DevStuff/GitHub/agent-toolkit/rules/no-nonsense-comments.md`, on the synced tree, through
   `/compact-docs-writer` (read the toolkit's `AGENTS.md` + `docs/authoring-conventions.md` first;
   sync `README.md`/`docs/` if they describe the rule). Intent of the new text:
   - Default is no comment: comments rot, nothing checks them against the code.
   - Durable knowledge (facts, rationale, decisions, traps, open gaps) goes into the project's docs,
     else the tracker / PR description; never into a comment, and no pointer to the doc either.
   - Allowed only: a one-line hazard at the exact line where an edit silently breaks something the
     code can't show (an order that must match an external enum, a call that must come first for a
     reason invisible here); license headers; tool directives.
   - Explicitly banned, however true: field / enum / constant / function descriptions, data
     annotations (what an id does, where a number came from), section banners, design rationale,
     commented-out code — plus the existing list (narration, self-reference, restating code, dead
     context, future work, bug write-ups).
   - Replace the "Good" example (a function description) with a hazard; keep "stand alone" and the
     voice paragraph; update the frontmatter `description`.
   compact-docs-writer presents the diff and waits for approval; **ask before committing** and
   pushing in the toolkit.
2. `C:/Users/boss2/.claude/skills/raid-boss-strategy-recipe/SKILL.md`, through
   `/compact-skill-creator`: one line in Workflow — raid code carries no comment beyond a one-line
   hazard; a boss fact, decision or trap goes into the boss doc.

## Phase 1 — strip tool (inline, commit on Custom)

`tools/commentstrip/commentstrip.py` + `test_commentstrip.py` (fork-only, reused for open polish
lanes and the PR branches later). Python, run with `-I`.

Built: `python -I tools/commentstrip/commentstrip.py [--root DIR] <command>`; tests
`python -I -m unittest discover -s tools/commentstrip`.

- `dump [--rev REV] PATH...`: every strippable comment as `L<first>-<last>`, its text, and the code
  it sits before or on.
- `strip [--keep DIR] [--keep-from REV] [--param-names-strip] [--dry-run] PATH...`: keeps license
  headers, tool directives, `/*name*/` parameter-name comments (unless `--param-names-strip`),
  `--keep` entries (`path<TAB>comment text`; one matching nothing fails the run) and, with
  `--keep-from`, every comment that exists in that revision's copy of the file. CRLF kept; no new
  double blank line, blank line at a brace, trailing space or blank line at end of file.
- `check --rev REV PATH...`: fails on any file whose code tokens (comments ignored, whitespace
  normalised, literals intact) differ from REV's.

Trial on Custom `b2d2df3ea`: 14,435 comments removed in 389 of 472 files; 292 headers and 564
parameter-name comments kept; code tokens identical in every file; codestyle-clean; no new blank
line at a brace.

## Phase 2 — lane briefs

The orchestrator fills `<lane>`, `<worktree>` (absolute), `<base>` (hash), `<files>` (the lane's
paths, repo-relative), `<docs>` from the Phase 3 table, `<Raid>` and `<scratchpad>` (absolute).
Each agent works in `<worktree>` only.

### Lead brief

```text
You work in the git worktree <worktree>, on branch cs/<lane>, branched from Custom at <base>. Its
code comments are about to be stripped from the raid code by a script; your job is to make sure
nothing the strategy relies on is lost, by moving it into the boss docs first. Lane files: <files>.
Docs you own: <docs>.

1. List the comments: python -I tools/commentstrip/commentstrip.py --root <worktree> dump <files>
   > <scratchpad>/cs-<lane>-dump.txt, then read that file in parts. It omits license headers and
   /*name*/ parameter-name comments, which stay.
2. Decide each comment:
   - drop: restates the code or a name, narrates a change, refers to removed code, describes a
     spell or field the code doesn't rely on (the DBC and core scripts are the source), or is
     already in the owned doc (grep the doc for its ids and terms);
   - move: something the strategy relies on and the doc lacks (a number the code uses, a mechanic
     the bots react to, a value measured in a trace, a decision with its reason, a trap, an open
     gap). Merge it into the fitting section of the owned doc, following docs/README.md "Keeping
     it that way": symbol names are fine, never line numbers or pasted code;
   - keep: a hazard, i.e. editing that exact line silently breaks something neither the code nor
     common practice shows (an order that must match a core enum, a call that must come first for
     a reason invisible there). Rewrite it in place to one line that states the fact itself, in a
     colleague's voice (invoke the use-conversational-language skill), and add the line
     <path><TAB><comment text without //> to docs/plans/comment-strip/keep/<lane>.txt. Keep
     hazards rare.
   Never delete other comments from the code: the central strip does that later.
3. Never invent or correct a fact. A comment that contradicts the doc, or that you can't place,
   goes in your report unresolved. A fact that belongs to several encounters (docs/engine/,
   docs/systems/) goes in your report, not into those files. Remove doc text that points at a code
   comment.
4. Verify: python -I tools/commentstrip/commentstrip.py --root <worktree> check --rev <base> <files>
   passes, and strip --dry-run --keep docs/plans/comment-strip/keep <files> reports no unmatched
   keep entry.
5. Commit 1: the doc changes, the hazard rewrites and the keep file.
6. Invoke the compact-docs-writer skill on each owned doc and apply its result without asking
   (standing approval): every fact stays, nothing moves between files. Commit 2: the compacted
   docs.

Commits: stage only your files; message written with the Write tool to
<scratchpad>/cs-<lane>-msg<N>.txt and committed with git commit -F; subject style
docs(Raid/<Raid>): ..., at most 72 characters, wording per the use-conversational-language skill;
no AI attribution or co-author trailer. Never start or stop a Docker container, never touch another
lane's files or any git ref other than committing on cs/<lane> (no push, merge, rebase, amend,
reset).

Report: per owned doc, how many comments you dropped, moved and kept; every contradiction and
unplaced comment with its path and line; every engine/systems candidate; both commit hashes.
```

### Reviewer brief (Sonnet)

```text
You review the git worktree <worktree>, branch cs/<lane>, branched from Custom at <base>. Its code
comments are about to be stripped by a script; the branch's commits must have moved everything the
strategy relies on into the owned docs first. Lane files: <files>. Owned docs: <docs>.

The bar: a comment the strategy relies on (a number the code uses, a mechanic the bots react to, a
value measured in a trace, a decision with its reason, a trap, an open gap) must be in an owned doc,
accurately, unless it is a one-line hazard kept in code and listed in
docs/plans/comment-strip/keep/<lane>.txt. Restated code, change narration and descriptions of
spells or fields the code doesn't rely on may go. Doc text follows docs/README.md "Keeping it that
way".

1. python -I tools/commentstrip/commentstrip.py --root <worktree> dump --rev <base> <files>
   > <scratchpad>/cs-<lane>-review-dump.txt; read it in parts and check every comment against the
   owned docs at the branch head.
2. git diff <base> -- <docs>: every addition matches its source comment; every removal (the
   compaction commit) drops no fact.
3. Every keep entry meets the hazard bar; check --rev <base> <files> passes; strip --dry-run --keep
   docs/plans/comment-strip/keep <files> reports no unmatched entry.
4. Fix what fails in the docs or keep file and commit it on cs/<lane> (message file
   <scratchpad>/cs-<lane>-review-msg.txt, git commit -F, no AI attribution). Never correct a fact
   you can't verify from the comments or the code: report it.

Never start or stop a Docker container, never touch any git ref other than committing on
cs/<lane>. Report: what you fixed, grouped as missing fact, wrong fact, lost in compaction, keep
entry; what you left open and why; your commit hash.
```

## Phase 3 — knowledge lanes (Agent tool, worktrees)

**Stop and ask for the go** on: 11 branches + worktrees `build-toc-wt/cs-<lane>` (`cs/<lane>`) from
current Custom (`<base>`), committing on them, and merging them into Custom. Run 3 lanes at a time;
each lead's fresh reviewer (Sonnet) starts when it finishes. 22 agents in total. ICC lanes run now;
they only must not overlap a running `r14-closeout` (it folds plans into the same boss docs).

| Lane | Files (boss token in the name) | Docs it owns |
|---|---|---|
| `uld-yogg` | YoggSaron | `ulduar/yogg-saron.md` |
| `uld-mimiron` | Mimiron, Kologarn, Razorscale, Ignis | the four boss docs |
| `uld-algalon` | Algalon, Vezax, Auriaya | the three boss docs |
| `uld-thorim` | Thorim, Freya | `thorim.md`, `freya.md` |
| `uld-hodir` | Hodir, IronAssembly, FlameLeviathan | the three boss docs |
| `xt-eoe-os` | XT002, every Uld file with no boss token, all of EoE, all of OS | `xt002.md`, `ulduar/README.md`, `eye-of-eternity.md`, `obsidian-sanctum.md` |
| `toc-anub` | Anubarak (incl. KitePlanner, Pursuit) | `trial-of-the-crusader/anubarak.md` |
| `toc-beasts` | Jormungars, Icehowl, Gormok, NorthrendBeasts, Jaraxxus, every ToC file with no boss token | `northrend-beasts.md`, `lord-jaraxxus.md`, ToC `README.md` |
| `toc-fc` | FactionChampions* (Defence, Ball, GripChain), TwinValkyr | `faction-champions.md`, `twin-valkyr.md` |
| `icc-lower` | Marrowgar, Deathwhisper, Gunship, Saurfang, Festergut, Rotface, Putricide, Dogs, FrostGiant, every ICC file with no boss token | the nine boss docs, ICC `README.md` |
| `icc-upper` | BloodPrinces, BloodQueen, Valithria, Svalna*, Sindragosa*, LichKing* | the six boss docs |

**Lead** per lane: `--dump` its files; per comment decide
- **drop**: restates code or a name, narration, dead context, a spell / field description the
  code doesn't rely on, or already in the doc (grep the doc for the ids and terms);
- **move**: something the strategy relies on and the doc lacks (a number the code uses, a mechanic
  bots react to, a trace-measured value, a decision with its reason, a trap, an open gap) → merge
  into the fitting section of the boss doc per `docs/README.md` "Keeping it that way" (symbols
  fine, no line numbers, no code);
- **keep**: a hazard per the floor → rewrite to one line in place, add to
  `docs/plans/comment-strip/keep/<lane>.txt`.

Never invent or "fix" a fact: a comment that contradicts the doc goes in the report, unresolved.
Cross-encounter facts (`engine/`, `systems/`) go in the report, not edited (shared files). Remove
doc text that points at a code comment. Commit 1 on the lane branch: docs + hazard rewrites + keep
file. Commit 2: `/compact-docs-writer` over every doc the lane owns (standing approval: apply);
every fact kept, nothing moved between files. Unique commit-message file names. Never touch Docker,
other lanes' files, or any other git ref.

**Reviewer** per lane (fresh, Sonnet, never shown the lead's report): `--dump --rev <base>` the
lane's files; check commit 1 adds every relied-on fact neither in the doc nor kept, accurately and
per the README rules; check commit 2's removals against its rendered diff for any dropped fact;
commit fixes on the lane branch.

## Phase 4 — merge, strip, close (inline on Custom)

1. Merge the 11 lanes into Custom one at a time (resolve doc conflicts from concurrent polish-loop
   closeouts by hand).
2. Comments added to the five dirs since `<base>` (`git diff <base> Custom`): migrate inline like a
   lane. Apply the lanes' `engine/` / `systems/` candidates and decide reported contradictions (ask
   the user where the evidence doesn't settle it).
3. Run the strip over `src/Ai/Raid/{EoE,Uld,ToC,ICC,OS}` with `--keep docs/plans/comment-strip/keep`;
   then `--check HEAD`. Commit the strip alone, so syncs can treat it as one step.
4. Record follow-ups in the in-flight plans (one line each):
   - `raid-test-runs.PLAN.md` "Polish loop": an `rt/` lane branched before the strip commit runs
     `commentstrip.py --keep-from <strip>` on its branch before merging; `file:line` refs written
     before it point at `<strip>^`.
   - `toc-upstream-pr/p5-sync.PLAN.md`, `ulduar-upstream-pr/p5-sync.PLAN.md`,
     `ulduar-upstream-pr/p1-eoe-sync.PLAN.md`: port the delta up to `<strip>^`, run the tool on the
     PR branch with `--keep-from <strip>` instead of porting the strip commit, export base moves to
     `<strip>`.
   - `icc-rebuild/r14-closeout.PLAN.md`: `file:line` refs in `icc-rebuild/` point at `<strip>^`;
     the boss docs already carry the facts moved out of comments, so fold without repeating them.
5. Closeout per `CLAUDE.local.md`: delete `docs/plans/comment-strip/`, commit.

## Verification

- `python -I -m unittest discover -s tools/commentstrip` passes.
- `commentstrip.py --check <strip>^` reports every file identical in code tokens.
- Remaining comments = license headers + keep entries (`--dump` count).
- `pb-check` over every changed file (strict compile, pblint, codestyle, native tests); on
  `INCOMPLETE`, rerun with `PB_MAX_FANOUT=all` in the background; commit only when green.
- No behaviour change, so no in-game run is needed.
