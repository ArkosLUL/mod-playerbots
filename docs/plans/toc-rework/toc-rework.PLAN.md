# Trial of the Crusader rework

In flight. Brings the ToC raid strategy (`src/Ai/Raid/ToC/`, map 649, strategy key
`trialofthecrusader`) to Ulduar parity on 10N/25N/10H/25H and wires it into RaidObs. Work runs as
waves of parallel lanes, one brief per lane in this directory. `w6-closeout` deletes this directory
once its durable content lives in `docs/raids/trial-of-the-crusader/`.

## Status

The merge stage alone edits this table.

| Wave | Lane | State | Merge | Decisions for review | Carried over / gaps |
|---|---|---|---|---|---|
| 1 | w0a-restructure | merged | `8b45f7b43` | Per-stem context classes merged into the root via `Absorb` (duplicate key logs, first wins); node lists in Trigger stems, multipliers in Multiplier stems, old insertion order kept; `ToCHelpers_<Stem>` utils, `ToCRaid*.h` umbrellas; drag helper on `ToCMainTankHoldAction`; `ToCBurstWindow` hook unused until w0c; boss docs corrected against the core script | Needs a CMake re-run. Stale `RaidBossHelpers` mentions in other raid docs (w6); champion entries now in `Util/ToCHelpers_FactionChampions.h` (w0b); Sweep 67644/67645 missing, dodge dead in 25N/10H (w0c); future-work comment in `AnubarakFocusBurrowerAction` (w5); every behaviour bug in "Why" still live |
| 2 | w0b-tooling | merged | `275a31a90` | ToC/VoA traces close `kill` on a queued DBC encounter credit (latched while `IsEncounterInProgress` is true), `reset`/`wipe` on its fall after roster combat; never polled with nobody alive; no schema bump; 28 champion aliases, explicit `NOTE_PREFIXES`; pblint `--spell-difficulty` merges DB+CSV by row id, `// pblint: spell-difficulty-ok` marker replaces free-text silencing | Stale docs (w6): `docs/raids/README.md:137`, overview "Spell difficulty", `vault-of-archavon.md:43`, `ulduar/mimiron.md:1474` (155 constants); ToC in observability.md's converted list and NamePull ToC line; `TOC_PREFIXES` vs `ToCEncounterGate.cpp` test; boss-doc Known gaps (twin-valkyr, northrend-beasts, anubarak); sweep findings outside ToC (ICC 5, VoA 72090, Naxx 5, OS/Ony/RS 1). Gaps: no-hit Twins kill files reset; VoA Wintergrasp idle close; heroic Beasts evade after latch files kill; `anub-arak` slug shared with Azjol-Nerub; pblint misses `GetSpellForDifficultyFromSpell` |
| 2 | w0c-foundation | merged | `a9691f782` | `ToCEncounterGate`: triggers gate on "open", multipliers/burst/NamePull on "live", `toc.progress` probe; Beasts/Jaraxxus stay open until the next encounter is live; all multipliers gated; `ToCBurstWindowMultiplier` rows (Anub'arak lust hold ported, others allow); spell constants remapped per call; Anub'arak sphere/Permafrost/Swarm detection rebuilt; 28-champion lookup | w1b: re-key `northrend worms afflicted by burning` on Burning Bile 66869, Bile-to-Toxin cleanse; stale Anub'arak lust gate in `consumables-and-burst.md` (w6); overview "whichever of 24" → 28; CMake re-run (new `ToCEncounterGate.cpp`, `ToCMultipliers_Shared.cpp`); `BuildSharedStrategyContexts.cpp` fails syntax check (`ToCStrategy.h` resolves to dungeon `TOCStrategy.h`, include by path, w6); boss lanes verify the brief's "Nodes made live". After merge `--spell-difficulty` warns on `SPELL_BURNING_BITE`/`SPELL_BURNING_SPRAY` (ToCData.h:63-64): w0b removed the free-text skip their comment relied on (w1b). Gap: the burning node never fires |
| 3 | w1a-beasts | merged | `d67865b40` | One tank-duty deal for all stages (Gormok > Icehowl > WormMobile > GormokSwap > WormStationary, sticky, lowest-priority tank fills); Impale swap at 3 stacks; taunts vetoed off-duty; snobold ranking healer > ranged > melee > tank, no icon; Icehowl charge path hazard 12 yd half-width latched per instance, guard zeroes every mover gaze to crash, forced dodge; held where he stands; Frothing Rage holder defensive; lust at daze or <= 50%; no coordinates (GO floor); full list in brief | CMake re-run (new `ToCHelpers_NorthrendBeasts.cpp`, `ToCMultipliers_Gormok.cpp`). `w1a-beasts-rest.PLAN.md`: Arctic Breath spread, Fire Bomb dodge. w1b: `FindWorm` by form should use `GetBeastOfDuty`, mobile-worm skull every tick. w6: `SPELL_MASSIVE_CRASH_*` unread; `test_toc_naming.py:58,199` use removed `gormok engaged by main tank`. Gaps: burning node dead (w1b), no HoP on snobolled, heroic Frothing Rage undispellable, melee eat Whirl |
| 3 | w2-jaraxxus | merged | `694f15cc6` | Intro closed but MT stand beside him; kill order heroic portal > volcano > Mistress > Infernal; AT0 Mistresses, AT1 Infernals; boss held on `ARENA_CENTER`; one Fel Fireball kicker per cast (lowest guid, ready and affordable, no silences/Spell Lock); Legion Flame carrier on forward-latched heading, `AvoidAoeAction` vetoed while flames up; kissed bots instants only, casts broken via `RequestSpellInterrupt`; Incinerate heals avoid cast-time; lust at pull; separate icons, reset to skull | `w2-jaraxxus-rest.PLAN.md`: Fel Lightning spread. w6: Jaraxxus reader and probes in README trace section and observability.md converted list; outside ToC, `PlayerbotAI::UpdateAI` full-health heal cancel should skip heal-absorb targets. Gaps: no raid cooldown calls, melee take Fel Inferno/portal/volcano, `lord_jaraxxus.py` `carrier_windows` misjudges a repeated tank window |
| 3 | w3a-fc-offence | merged | `b16ee7a42` | Healers-first kill target latched per instance, switched only on death, all-school immunity or suspended target's return; one CC target and icon per CC bot, prior `rti cc` restored (ungated `toc restore rti cc`); mage-only counterspell duty; AoE suppression kept; `BossHasNoStableVictim` in burst gate and `OffensivePotionTrigger`; lust at pull; MD/Tricks to MT vetoed; `DpsAssist`/`TankAssist` vetoed while a kill target exists | Outside ToC: `BurstCooldowns`, `BurstWindowStrategy`, `GenericTriggers`, consumables-and-burst.md. w3b-fc-defence (stem `FactionChampionsDefence`): HoP switch, purge/spellsteal, CC dispels, spread. w6: migrate the FC per-instance map to `RaidInstanceState`; consumables-and-burst.md name table lacks `killing spree`/`power infusion` and the healer exemption. Gaps: guide openers beyond bots, CC into DR immunity, latch and skull never cleared after the kill |
| 3 | w4-twins | merged | `da7ae78f6` | DPS hit the other colour, raid starts Dark, Pact twin first; everyone matches Vortex, Touch outranks Vortex; shielded twin's colour swaps during Pact; lowest-guid interrupt duty, kicks held under shield; orb dodge (other colour, or own near an ally of the other); MT Fjola, AT Eydis, both dragged to centre with taunt guard and redirect; lust waits for the shield; essence-swap multiplier removed; full list in brief | w6: `test_toc_naming.py:63` samples removed `twin valkyr pact interruptible`. Gaps: no soakers or orb collection, Touch during the other Vortex, humans hold no interrupt duty, lone tank can't hold both, per-ms `GetWantedEssence` memo deferred |
| 3 | w5-anubarak | merged | `e222c6590` | Every node waits for the pull; no landing node (no traces); burrowers held on Permafrost every difficulty, DPS only while on it; boss held between the two nearest patches; kiter 8.5 yd past the patch, non-kiters dodge spike and lane; one Shadow Strike interrupter per cast; P3 MT defensive chain, Penetrating Cold heals, lust latched in P3; pre-submerge drag measured on the boss; `AnubarakSubmerged` fixed | `w5-anubarak-rest.PLAN.md`: kiter HoP, P3 raid-health economy, Freezing Slash model, burrower pacing, landing node, scarabs, Spider Frenzy spacing, P2 positioning. w6: raid-wide rti reset after a kill, drop unused `ANUBARAK_PIT_CENTER`, hoist the direct-heal list into Shared (Jaraxxus list still names greater healing wave). Gaps: AoE pops spheres, 10H one side tank stacks burrowers, spike dodge ignores Permafrost, one-group memo, marks outlive the kill |
| 4 | w1b-jormungars | pending | | | |
| 4 | w3b-fc-defence | pending | | | |
| 4 | w1a-beasts-rest | pending | | | |
| refine | w2-jaraxxus-rest | pending | | | Needs 649 traces from a build carrying w2 |
| refine | w5-anubarak-rest | pending | | | Needs 649 traces from a build carrying w5 |
| 5 | w6-closeout | pending | | | |

States: `pending`, `merged`, `blocked` (branch and worktree kept, reason in the lane brief),
`skipped` (merge refused because the main tree had the file dirty; branch kept).

## Why

The ToC code predates `docs/engine/pitfalls.md`, `raid-mechanics-lessons.md` and RaidObs, and
repeats their traps: `FleePosition` dodges, raid-icon marks never cleared and the kill target
re-picked every tick, blanket `MovementAction` vetoes (which also veto `AttackAction`), duplicates of
`EncounterHelpers`, no encounter gating on its nine multipliers, a `dynamic_cast` lust gate, no
probes. Live bugs: single-difficulty spell ids (Twin Val'kyr essence 65686/65684, Touch, Leeching
Swarm 66118, Impale, …) leave nodes dead in 25N/10H/25H; Faction Champions reset threat every 2 s,
so the base "tank held the boss" burst gate never opens.

Verified constraints:

- **No boss state.** `instance_trial_of_the_crusader.cpp` never calls `SetBossState`. It keeps a
  private `EncounterStatus` and exposes `GetData(TYPE_INSTANCE_PROGRESS = 1)`: 0-1 Beasts, 2-3
  Jaraxxus (2 = Fizzlebang intro), 4 Faction Champions, 6 Twins, 8 Lich King transition, 9
  Anub'arak, 10 done. It never moves back on a wipe. `GetGuidData` exposes Gormok (4), Dreadscale
  (6), Acidmaw (7), the twins by NPC entry, Anub'arak (13). `IsEncounterInProgress()` goes true
  before the pull (Beasts walk-in, champions released, twins 3.25 s early, Anub'arak from the Lich
  King scene); only Jaraxxus sets it on engage.
- **Trace naming.** The engage hook names every ToC trace first, from the engaging creature
  (Beasts → `gormok-the-impaler`, champions → whichever of 24 swings first, twins → either name), so
  `NamePull` never acts. No ToC trace ever closes as `kill` or `reset`.
- **Spell difficulty.** ToC remaps live only in the client DBC, mirrored in
  `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-spell-tweaks/data/dbc-reference/spelldifficulty.reference.csv`.
  `acore_world.spelldifficulty_dbc` has none of them; the core merges both sources.
- **Heroic attempts** are private to the script and invisible to the module. Touch of Jaraxxus is
  commented out in this core.

## Decisions

| Topic | Decision |
|---|---|
| Design questions | Never pause. The core script decides mechanics (ids, timers, radii, targeting); the Warcraft Tavern guide decides tactics (kill order, positioning, assignments). On a conflict the script wins and the boss doc says why. What neither settles is decided conservatively. Log every such choice under the brief's "Decisions for review" with its source. |
| Scope | Every mechanic on 10N/25N/10H/25H, heroic-only included. No bot cheats: a cheat-only mechanic is a documented gap. |
| Code outside ToC | Only where a brief says so, and listed in the lane report. `src/Ai/Raid/Uld/` is never touched. |
| Oversized lane | Keep what hurts the raid most; move the rest into a new brief `docs/plans/toc-rework/<lane>-rest.PLAN.md` and report it as carried over. |
| Findings | Confirmed bugs and minors are fixed; cleanups only when small. One re-review round, then the boss doc's "Known gaps". |
| Build and test | The user builds and pulls. Agents never build. |

## Naming contract

C++ emits and the Python readers accept exactly these. `w0b-tooling` makes Python accept them;
`w0c-foundation` makes C++ emit them.

| Encounter | Trigger-name prefixes | Stage | Trace slug | Note-key prefix |
|---|---|---|---|---|
| Northrend Beasts | `gormok`, `northrend worms`, `icehowl` | 0-1 | `northrend-beasts` | `nb.` |
| Lord Jaraxxus | `jaraxxus` | 2-3 | `lord-jaraxxus` | `jaraxxus.` |
| Faction Champions | `faction champions` | 4 | `faction-champions` | `fc.` |
| Twin Val'kyr | `twin valkyr` | 6 | `val-kyr-twins` | `tv.` |
| Anub'arak | `anubarak` | 9 | `anub-arak` | `anub.` |
| raid-wide | none | any | none | `toc.` |

## Lane rules

These bind every agent in every lane; a lane brief adds scope and facts.

**Authority.** The user asked for this effort to run without waiting for them: "with all the docs,
git commit, merge or any other tasks that require approval are not waiting for me", and approved
this plan with per-lane branches, worktrees and `--no-ff` merges. For this effort only, that
replaces `CLAUDE.local.md`'s "no branch". Allowed: `git worktree add/remove`, commits on
`toc/<lane>`, `git merge --no-ff` (merge stage only), `git branch -d`. Never: push, amend, rebase,
`reset --hard`, force, stash, `clean`, a checkout that discards changes.

**Worktree.**

- Main tree: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots` on `Custom`. Lane
  worktree: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/<lane>` on `toc/<lane>`.
- Create it from the main tree: `git worktree add -b toc/<lane> <wt> Custom`. If the branch exists,
  `git worktree add <wt> toc/<lane>`. If the worktree exists, reuse it and continue from its state.
- The path is gitignored by the core's `/build*/`, stays out of `modules/` (the core CMake would
  build it as a second module), and sits two levels under the core root, so
  `tools/botobs/raidobs/paths.py` still finds the traces.
- Every edit of a lane happens inside its worktree. Only the merge stage writes to the main tree.

**Files a lane may touch.** Its own stems under `src/Ai/Raid/ToC/` and its seam files (the code
layout section of `docs/raids/trial-of-the-crusader/README.md` names them once wave 1 lands), its
boss doc, its own brief, `tools/botobs/bosses/<slug>.py`, `tools/botobs/tests/test_<slug>.py`.
Anything else only where the brief says so, listed in the report. Never this file (merge stage)
or `src/Ai/Raid/Uld/`. `docs/raids/trial-of-the-crusader/README.md` belongs to the merge stage,
except in a lane whose brief hands it over.

**Sources.**

- Mechanics: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/`.
  Follow a call to its guards before relying on its effect.
- Spell ids per difficulty: the CSV above plus the world DB
  (`docker exec ac-database mysql -uroot -ppassword -N -B -e "SELECT … FROM acore_world.spelldifficulty_dbc …"`).
  Spell fields: `spell.reference.csv` next to the CSV.
- Coordinates: navprobe, per `docs/engine/pitfalls.md`. Never invent or reject a coordinate from a
  model of the room.
- Tactics: Warcraft Tavern, 25-man only; scale to 10-man by the script's 10-man values. Read it word
  for word (WebFetch only summarises past a 39k-character menu):

  ```
  curl -sL -A "Mozilla/5.0" "<url>" -o guide.html
  python -c "import re,html;s=open('guide.html',encoding='utf-8',errors='ignore').read();s=re.sub(r'(?s)<(script|style|nav|header|footer)[^>]*>.*?</\1>','',s);print(re.sub(r'\s+',' ',html.unescape(re.sub(r'<[^>]+>',' ',s))))"
  ```

  Base `https://www.warcrafttavern.com/wotlk/guides/`: overview `toc-25-raid-guide/`, Beasts
  `beasts-of-northrend-master-strategy-guide-toc-25/`, Jaraxxus
  `lord-jaraxxus-master-strategy-guide-toc-25/`, Champions
  `faction-champions-master-strategy-guide-toc-25/`, Twins `twin-valkyrs-strategy-guide-toc-25/`,
  Anub'arak `anubarak-master-strategy-guide-toc-25/`. Wowhead blocks automated reads; do not work
  around it.

**Required reading before designing.** `docs/engine/pitfalls.md`, `raid-mechanics-lessons.md`,
`action-selection.md`; `docs/raids/README.md`; `docs/systems/observability.md` ("Adding a probe");
`docs/systems/consumables-and-burst.md`. Ulduar references by concern: interrupt duty
`docs/raids/ulduar/vezax.md`; one mover and ranked candidates `xt002.md`; latched stands `hodir.md`;
containers, probes and per-instance state `mimiron.md`; taunts and tank swaps `thorim.md`; spread
rings `iron-assembly.md`; the gate `src/Ai/Raid/Uld/UldEncounterGate.{h,cpp}`.

**Per-instance state.** Use `src/Ai/Raid/RaidInstanceState.h` (`RaidInstanceState<State>`, from a
parallel effort, plan `docs/plans/raid-instance-state/`) when it exists on the lane's branch. Until
then use a mutex-guarded map keyed by instance id, never `thread_local`, and list each one in the
report for `w6-closeout` to migrate. `src/Script/Playerbots.cpp` has uncommitted edits from that
effort in the main tree, so a lane touching it will fail to merge: stay out of it.

**Checks**, from the worktree root:

- `PB_REPO=<wt> PB_MAX_FANOUT=60 ~/.claude/scripts/pb-syntax-check.sh <changed .cpp and .h>` (a
  `.h` expands to the `.cpp` files including it by name).
- `python tools/pblint/pblint.py src/Ai/Raid/ToC src/Bot/PlayerbotAI.cpp src/Ai/Raid/RaidStrategyContext.h`,
  plus `--spell-difficulty` once `w0b-tooling` has merged. Stay scoped: the whole tree carries known
  errors.
- `python -m unittest discover -s tools/botobs/tests`.
- In a lane with several implementers only the integrate agent runs checks.

**Commit.** One commit per lane on `toc/<lane>`, staged by explicit path, never `-A`. Subject ≤72
characters; body at most three lines, only a why the diff can't show; voice per
`use-conversational-language`; no AI attribution or `Co-Authored-By`. Code comments follow the
no-nonsense-comments rule: no process narration, no references to lanes, waves or plans.

**Docs.** Durable facts go to `docs/raids/trial-of-the-crusader/<boss>.md` per `docs/README.md`
(ids, timers, decisions with rationale, traps, known gaps; no change lists). A generic engine lesson
goes into the lane report for the merge stage. Fold docs with `compact-docs-writer` (standing
approval). Each boss doc ends with "What a trace answers": probe keys and reader usage, as in
`docs/raids/ulduar/mimiron.md`.

## Waves

| Wave | Lanes, in parallel |
|---|---|
| 1 | `w0a-restructure` (runs chained into wave 2) |
| 2 | `w0b-tooling`, `w0c-foundation` |
| 3 | `w1a-beasts`, `w2-jaraxxus`, `w3a-fc-offence`, `w4-twins`, `w5-anubarak` |
| 4 | `w1b-jormungars`, `w3b-fc-defence`, plus `refine-<boss>` for any boss with fresh traces |
| 5 | `w6-closeout` |

A lane runs: investigate → 1-3 parallel implementers on disjoint files (up to 5 in `w0a`) →
integrate and check → three reviewers (mechanics, conformance, observability and conventions) →
skeptic verify → fix → one re-review → close and commit. A merge stage then lands every finished
lane on `Custom`, runs the checks on the merged tree, updates the status table and the README boss
table, and removes merged worktrees and branches.

A `refine-<boss>` lane reads the 649 traces, first checking `hdr.bin` against HEAD so it only reads
pulls from a build that has the change, then runs `postmortem.py`, `batch.py` and the boss reader
and fixes what the traces show.
